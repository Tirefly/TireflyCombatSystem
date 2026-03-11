# TCS 核心修复提案草案（中文）

本文档用于辅助在 OpenSpec 中创建新的 proposal 和 tasks，覆盖 TCS 的 Attribute 与 State 两个模块中若干“正确性 / 可维护性”改动。

本文档**不会修改任何代码**；仅描述范围、设计决策与任务拆分建议。

---

## 0. 目标 / 非目标

### 目标

- 通过移除对“不稳定数组下标”的依赖，修复 Attribute 中 SourceHandle 按句柄查询/移除的正确性问题。
- 让 Attribute 的“动态范围 Clamp”（Dynamic Range Clamp）在被引用属性变化时行为正确（例如 MaxHP Buff 结束后，HP 被 MaxHP 重新 Clamp）。
- 强化 Attribute API（AddAttribute 不再静默覆盖；创建 Modifier 时严格校验输入）。
- 移除 State 槽位更新的再入风险（合并移除 + 激活更新导致的递归/嵌套更新）。
- 在 PriorityOnly 槽位中，将“同优先级 tie-break”做成可配置的策略对象（UObject CDO），而不是硬编码规则。
- 简化/收敛 Slot Gate 关闭逻辑到单一权威路径，降低重复逻辑与维护成本。

### 非目标

- State 免疫、对象池集成、State 实例的网络同步（根据前序讨论，明确不在本轮范围内）。
- PendingRemoval 超时强制 StopStateTree（保持由项目开发者自行负责）。

---

## 1. Proposal 拆分建议

这些改动彼此有关联，但会同时触达两个子系统，并且风险画像不同。

### 推荐：拆分为 2 个 proposals（可审查性最佳）

1) **Attribute 提案**（改动较大，但单领域）：
   - SourceHandle 索引重构（稳定 ID）
   - 创建 Modifier 的严格校验
   - AddAttribute 防覆盖 + AddAttributeByTag 语义调整
   - 动态范围 Clamp 的传播 / 范围约束（Range Enforcement）

2) **State 提案**（单领域，但行为更敏感）：
   - 合并移除统一走 RequestStateRemoval
   - 槽位激活更新：通过“延迟请求/排队”消除再入
   - 同优先级 tie-break：策略对象（UObject policy）
   - Gate close 逻辑重构（单一权威函数）

为什么这么拆：
- Attribute 与 State 可独立评审/测试，降低评审与回归压力。
- Attribute 的索引重构属于结构性改动，适合集中 review。
- State 的 tie-break 策略属于玩法敏感点，隔离有利于降低风险。

### 备选：1 个 proposal（仅当你希望一次性合并发布）

优点：一个分支/PR、发布步骤更少。  
缺点：review 更难、关注点混杂、回归漏测概率更高。

### 备选：3 个 proposals（当你希望把每一步做得更小）

1) Attribute 索引 + 校验
2) Attribute 动态范围约束（Range Enforcement）
3) State 激活/tie-break/gate 重构

---

## 2. Proposal A（Attribute）：稳定 SourceHandle 索引 + 范围约束 + API 强化

### 2.1. 问题：SourceHandle 查询/移除依赖不稳定数组下标

当前情况：
- `UTcsAttributeComponent::SourceHandleIdToModifierIndices`：SourceId -> `AttributeModifiers` 数组下标列表。
- `AttributeModifiers` 删除元素会导致下标整体左移；当前代码通过扫描所有桶来“修正下标”，实现脆弱、容易漂移。
- `RemoveModifiersBySourceHandle()` 一类 API 在缓存漂移时可能出现“假失败”（明明存在但返回 false / 不移除）。

### 2.2. 设计：用“稳定 ID 缓存”替换“数组下标缓存”

核心思路：
- 使用 `FTcsAttributeModifierInstance::ModifierInstId` 作为稳定身份。
- 保持 `AttributeModifiers` 为数组便于遍历，但 SourceHandle 缓存里永远不存数组下标。

#### 数据结构调整（UTcsAttributeComponent）

- 替换：
  - `TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;`
- 为：
  - `TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds;`  （SourceId -> ModifierInstId 列表）
  - `TMap<int32, int32> ModifierInstIdToIndex;`                    （ModifierInstId -> 当前数组下标）

备注：
- 这是运行时缓存；不应做成 UPROPERTY（与当前索引缓存的设计动机一致）。
- `SourceHandleIdToModifierInstIds` 可能包含“陈旧 id”；API 调用应具备自愈能力：若 id 在 `ModifierInstIdToIndex` 中不存在，就从桶里剔除。

#### 变更规则（Mutation rules）

1) **插入（Insert）**
- 在 `AttributeModifiers.Add(...)` 之后：
  - `ModifierInstIdToIndex[ModifierInstId] = NewIndex`
  - `SourceHandleIdToModifierInstIds[SourceId].AddUnique(ModifierInstId)`（仅当 SourceHandle 有效）

2) **更新已有（Update / refresh）**
- `ModifierInstId` 必须保持稳定不变。
- 若 SourceId 发生变化：
  - 从旧 SourceId 桶移除该 id，并加入新 SourceId 桶。
- 始终确保 `ModifierInstIdToIndex` 正确。

3) **删除（Remove）**
- 用 `ModifierInstIdToIndex` 定位元素下标。
- 用 `RemoveAtSwap(Index)` 删除，避免整个数组左移。
- 只需要修正 1 个被 swap 过来的元素的 index 映射：
  - `ModifierInstIdToIndex[SwappedId] = Index`
- 从以下位置移除被删 id：
  - `ModifierInstIdToIndex`
  - `SourceHandleIdToModifierInstIds[SourceId]` 桶（若桶为空则移除该桶）

取舍：
- `RemoveAtSwap` 会改变 `AttributeModifiers` 的遍历顺序。由于数值计算通常依赖 Priority/时间戳来决定最终效果，不应依赖数组物理顺序来承载语义，因此这一点是可接受的。

#### 查询/移除 API 的预期行为

- `GetModifiersBySourceHandle`：
  - 从 `SourceHandleIdToModifierInstIds[SourceId]` 读取 id 列表
  - 对每个 id，通过 `ModifierInstIdToIndex` 解析数组下标
  - 若解析失败，说明该 id 已陈旧：从桶中剔除（自愈）

- `RemoveModifiersBySourceHandle`：
  - 先拷贝桶内 id 列表（避免遍历时修改桶）
  - 再按 id 删除（或先收集实例，再调用既有 RemoveModifier 管线）

### 2.3. 严格校验：创建 Modifier 必须拒绝非法输入

落点函数：
- `UTcsAttributeManagerSubsystem::CreateAttributeModifier(...)`
- `UTcsAttributeManagerSubsystem::CreateAttributeModifierWithOperands(...)`

规则：
- `SourceName == NAME_None`：失败 + error log
- `!IsValid(Instigator) || !IsValid(Target)`：失败 + error log（当前已有部分校验）
- `Instigator` 与 `Target` 必须实现 `UTcsEntityInterface`：失败 + error log
- 若未来接入 RowHandle 的 SourceDefinition：
  - RowHandle 非空但找不到行：失败 + error log（内容正确性优先，建议直接 fail）

### 2.4. AddAttribute 防覆盖

建议调整语义：
- `AddAttribute`：若已存在同名属性，则不覆盖；log warning；return。
- `AddAttributes`：逐个属性同样处理。
- `AddAttributeByTag`：返回值应表达“是否真的添加成功”，而不是仅表达“Tag 是否解析成功”（避免调用方误判已初始化）。

### 2.5. 动态范围 Clamp 传播（MaxHP -> HP 场景）

#### 已观察到的问题

`ClampAttributeValueInRange` 解析动态 min/max 时会调用 `UTcsAttributeComponent::GetAttributeValue`，该函数只读取**已提交（committed）**的 `CurrentValue`。

这意味着在一次重算过程中：
- 当 MaxHP 在“计算中途”发生变化时，HP 的 Clamp 仍可能读到旧的 MaxHP（因为 MaxHP 尚未提交）。
- 当 MaxHP 下降（Buff 移除）时，HP 的 BaseValue 可能仍保持 > MaxHP：因为 BaseValue 的重算通常不会被“CurrentValue modifier 的移除”触发。

这会破坏期望的不变量（Invariant）：
- 如果 HP 的动态上限绑定到 MaxHP，则 **HP（base/current）** 必须在任意时刻 <= **MaxHP（current）**。

#### 建议设计：把属性范围约束提升为“值变更后的强制不变量”

新增一个显式的“范围约束（Range Enforcement）”步骤，在属性值发生变化时执行：

1) 先按当前逻辑重算（existing behavior）：
   - `RecalculateAttributeBaseValues(...)`（base modifiers 作用时）
   - `RecalculateAttributeCurrentValues(...)`（persistent modifiers 增/删/更时）

2) 在 (1) 之后，执行 **Range Enforcement Pass**：
   - 遍历所有属性并 Clamp：
     - `BaseValue` 约束到计算出的范围
     - `CurrentValue` 约束到计算出的范围
   - 因为范围可能依赖其他属性（多跳依赖），需要迭代直到稳定：
     - 当没有任何值发生变化则停止；或命中 `MaxIterations`（例如 4-8 次）
     - 若命中最大迭代次数：log warning，提示可能存在循环依赖

动态范围解析必须使用“最新值”：
- 在 enforcement 迭代中，动态 min/max 的解析应优先读取“当前工作集（working values）”，而不是读取旧的 committed 值。

实现路线（在 proposal 中二选一）：

Option A（推荐）：“in-flight resolver” 支持
- 扩展 clamp 逻辑，使其可接受一个“值解析器”回调：
  - `ResolveAttributeValue(AttributeName) -> float`（优先从 working map 读，fallback 到 component）
- 同时用于：
  - 重算过程中的 in-flight clamp（HP 在同一轮可读到最新的 MaxHP）
  - 重算后的 post-pass enforcement（依赖关系在迭代中稳定收敛）

Option B：先提交（commit-first），再执行 enforcement
- 先把重算结果提交到 component（committed）
- 再用 `GetAttributeValue` 执行 enforcement（此时 committed 值已更新）
- 仍需迭代以处理多跳依赖

Option A 更精确，避免“短暂提交越界值”的过渡状态；Option B 实现更简单，但可能在修正前短暂提交 out-of-range 值。

#### 事件语义（需要明确决策）

当 enforcement 导致 Clamp 时：
- `OnAttributeValueChanged`：当 `CurrentValue` 发生变化时应触发（即使本批次没有 modifier 直接触碰该属性）。
- `OnAttributeBaseValueChanged`：可选两种语义：
  - 仅当 base modifiers 直接导致 base 变化时触发（偏归因事件）
  - 或者当 enforcement 改变 BaseValue 时也触发（更符合“真值变化事件”）

以 HP 这类常见“以 Base 存储”的属性为例：若 enforcement 会改变 BaseValue，触发 base 事件通常更安全；但这属于产品语义决策。若你坚持 base 事件仅用于归因，则必须明确写入文档：不变量约束可能在不广播 base 事件的情况下改变 BaseValue。

---

## 3. Proposal B（State）：移除统一 + 去再入 + tie-break 策略 + Gate 重构

### 3.1. 合并移除应统一走 RequestStateRemoval

当前风险：
- 合并逻辑直接 Finalize removal，而 Finalize 会触发槽位激活更新，导致嵌套 `UpdateStateSlotActivation` 调用（再入风险）。

设计：
- 对 “merged out” 的 state，不直接 finalize。
- 改为调用 `RequestStateRemoval`：
  - `Reason = Custom`
  - `CustomReason = "MergedOut"`
- 保障：
  - 若有 StateTree 退场逻辑，有机会执行
  - 移除流程单路径（single-path）

### 3.2. 通过“延迟请求”消除 UpdateStateSlotActivation 再入

设计：
- 在 `UTcsStateManagerSubsystem` 内部增加“延迟激活更新”机制：
  - `bIsUpdatingSlotActivation`
  - `PendingSlotActivationUpdates`（TSet<FGameplayTag>）
  - `RequestUpdateStateSlotActivation(SlotTag)`

规则：
- 若当前正在更新，则仅把 SlotTag 入队。
- 若当前不在更新，则执行一次更新，并在结束后 drain 队列。
- 目标：让槽位激活在有限步内收敛，避免递归/嵌套调用。

### 3.3. 同优先级 tie-break：策略对象（CDO），不是 enum

需求：
- 在 `SSAM_PriorityOnly` 中，多个 state 同优先级时，排序必须确定性（deterministic）。
- 但 tie-break 规则高度领域化（buff vs skill），需要支持项目扩展。

设计：
- 在 `FTcsStateSlotDefinition` 中增加策略字段，例如：
  - `TSubclassOf<UTcsStateSamePriorityPolicy> SamePriorityPolicy;`
- 该策略是 UObject CDO，可做：
  - 决定“同优先级组”的排序
  - 可选：拒绝新 state（队列上限）或决定溢出处理

最小接口建议：
- `int64 GetOrderKey(const UTcsStateInstance* State) const;`
  - key 越大 => 排序越靠前（或明确采用反向）
- 可选扩展（队列上限）：
  - `bool ShouldAcceptNewState(const UTcsStateInstance* NewState, const TArray<UTcsStateInstance*>& ExistingSamePriority, FName& OutFailReason) const;`

TCS 内置默认策略建议：
- `UTcsStateSamePriorityPolicy_UseNewest`（buff 友好）
  - key = ApplyTimestamp（或 InstanceId 兜底）
- `UTcsStateSamePriorityPolicy_UseOldest`（技能队列友好）
  - key = -ApplyTimestamp（或采用反向 compare）

与现有流程的集成方式：
- 仍以“按 Priority 排序”为主键。
- 对每个“同优先级组”，应用策略排序。
- PriorityOnly 激活阶段选择排序后的第一个作为 Active；其它保持 Inactive（除非已 Active，且抢占策略允许）。

### 3.4. Gate close 重构：单一权威函数

现状：多个函数同时对 gate close 行为与不变量做约束，存在重复逻辑。

设计：
- 创建一个权威内部函数：
  - `HandleGateClosed(StateComponent, SlotTag, SlotDef, Slot)`
- 它必须：
  - 一致地应用 `GateCloseBehavior`（HangUp/Pause/Cancel）
  - 保证不变量：Gate 关闭时该 Slot 内不允许存在 Active state
- 移除重复路径；只保留 `UpdateStateSlotActivation` 的一个调用点。

---

## 4. 建议的 Change ID（新 proposals）

若采用 2 个 proposals：

1) `refactor-attribute-runtime-integrity`
2) `refactor-state-slot-activation-integrity`

若采用 3 个 proposals：
- `refactor-attribute-sourcehandle-indexing`
- `fix-attribute-dynamic-range-enforcement`
- `refactor-state-slot-activation-integrity`

---

## 5. 测试清单（自动化级别，不含网络）

Attribute：
- SourceHandle remove/get 在以下情况下依然正确：
  - 大量插入
  - 随机删除（含中间位置删除）
  - 存在陈旧 id 时，缓存可自愈（prune stale ids）
- AddAttribute 不会覆盖已存在属性（应输出 warning）。
- 创建 modifier 的严格校验：拒绝非法 SourceName / 非战斗实体的 instigator/target。
- 动态范围约束（MaxHP -> HP）：
  - MaxHP Buff 移除后，HP 能按设计被 clamp 回落（BaseValue 和/或 CurrentValue，取决于你选择的语义）。

State：
- 合并移除使用 RequestStateRemoval，且不会导致递归激活更新。
- PriorityOnly + 同优先级排序：策略下行为确定性。
- Gate close 使用单一权威函数，且 Gate 关闭时 Slot 内无 Active state 残留。

