# 规划：Attribute 与 Modifier 重构提案拆分

> 状态：规划中。本文只定义后续 OpenSpec change 的拆分边界、依赖关系与创建顺序；当前不创建 change，也不替代主设计文档中的行为契约。
>
> 关联主方案：[设计：Modifier操作数与AttributeOperation模型（待审核）.md](设计：Modifier操作数与AttributeOperation模型（待审核）.md)。
>
> 提案前决策清单：[提案前待确认问题（AttributeModifier重构）.md](提案前待确认问题（AttributeModifier重构）.md)。

## 1. 拆分结论

主设计和提案前决策清单描述的是同一套总体架构，不应按文档数量拆成两个 OpenSpec change。实际 change 应按“能否形成可编译、可使用、可独立验收的中间态”拆分。

建议建立三个直接重构 change，并保留两个后续能力 change：

```text
1. SourceHandle 统一工厂
2. Attribute 数值生命周期收敛
3. AttributeModifier Operation 运行时与创作链重构
4. Ongoing 依赖链自动重算（后续）
5. TCS 伤害模块（后续）
```

第一项与第二项在概念上可以并行，但都应先于第三项完成。第四项与第五项不得提前并入第三项。

## 2. Change 1：SourceHandle 统一工厂

建议 change id：

```text
centralize-source-handle-creation
```

### 2.1 范围

- 新增无状态静态 `FTcsSourceHandleFactory`，作为 SourceHandle 的唯一创建入口。
- 固化 `SourceHandle.IsValid()` 等价于 `Id > -1`，并确保 Id `0` 合法。
- 提供 Root / Child 创建 API，统一生成进程内唯一 Id 与 CausalityChain。
- Child API 继承父链并追加直接因果来源的 Definition Id，禁止调用方手工拼接因果链。
- `UTcsGenericLibrary` 只提供 Blueprint 转发，不拥有工厂状态。
- 迁移 State、Buff、Skill、Attribute 及其他现有来源创建入口。
- 更新相关 SourceHandle 规格、诊断与回归场景。

### 2.2 非目标

- 不新建 SourceHandle Subsystem。
- 不把 SourceHandle 工厂放入 `UTcsDefinitionManagerSubsystem`。
- 不建立 SourceHandle 到运行时 UObject 的全局注册表或反查机制。
- 不在本 change 中重构 AttributeModifier Operation 模型。

### 2.3 独立原因

SourceHandle 是 State、Buff、Skill、Attribute 与后续伤害模块共享的跨系统基础设施，不属于 AttributeModifier 私有能力。现有规格只要求工厂不再挂在旧 AttributeManager 上，尚未完整定义最终权威入口；该能力可以独立编译、迁移和验收。

## 3. Change 2：Attribute 数值生命周期收敛

建议 change id：

```text
simplify-attribute-value-lifecycle
```

### 3.1 范围

- 从 `FTcsAttributeInstance` 删除 `InitValue`、`InitialValue`、`InitialBaseValue` 及等价创建时基线字段。
- `AddAttribute` / `AddAttributeByTag` 只负责创建、注册与建立范围依赖，不再接收初始数值。
- 业务层在创建后通过明确入口设置 BaseValue。
- 删除 `ResetAttribute`；重生、回档、等级回退与资源恢复由业务层决定。
- BaseValue 与 CurrentValue 使用同一个 AttributeRange 和同一 ClampStrategy。
- 动态 Min / Max 只读取同一 AttributeComponent 中依赖 Attribute 的 CurrentValue。
- 临时容量降低时，将资源的 BaseValue 与 CurrentValue 一并 Clamp；超限部分永久丢失。
- 明确并实现 `SetAttributeCurrentValue` 的最终边界。
- 更新 `attribute-management` 规格和现有 public API 扩展点描述。

### 3.2 非目标

- 不引入多 Operation AttributeModifier。
- 不实现 OperandEvaluator、AttributeEvaluationSnapshot 或 Merger 兼容规则。
- 不实现 `RemoveAttribute` 对多 Operation Ongoing 引用的检查；该规则依赖新的 Ongoing Operation 记录，归入 Change 3。
- 不实现伤害、治疗、复活或存档业务规则。

### 3.3 独立原因

Attribute 数值状态、CRUD 与 Clamp 生命周期不依赖新的 Modifier Operation 模型。现有 `attribute-management` 规格仍包含 `AddAttribute(Name, InitValue)`、`ResetAttribute` 与相关 virtual API，适合先独立收敛并形成清晰的 Attribute 底盘。

## 4. Change 3：AttributeModifier Operation 重构

建议 change id：

```text
refactor-attribute-modifier-operations
```

这是本轮改造的核心纵向 change。虽然范围较大，但运行时、Definition、DataTable、Merger 和验证不能继续横向拆开，否则会产生不可使用的中间态或被迫增加已经明确不需要的兼容层。

### 4.1 Operation 与 Definition

- 建立 `OperandPayload -> OperandEvaluator -> EvaluatedOperand -> Operator` 模型。
- AttributeModifier 的 Evaluator 与 Operand 固定为 Numeric / `float`。
- `UTcsAttributeModifierDefinition` 使用 `TMap<FName, FTcsAttributeOperationDefinition>`，Key 为稳定 `OperationId`。
- Operation 保存 TargetAttributeId、Operator / CustomOperator、Evaluator 与 OperandPayload。
- `FInstancedStruct` 只承载 OperandPayload。
- Definition 不暴露 ModifierMode 或数值写入层。
- Application 只允许覆写 Evaluator 与 Payload，不得覆写 Operator、目标、Priority 或 Merger。
- 删除旧 `UTcsAttributeModifierExecution`、`ModifierType`、单 Operation Operands 与 OperandBindings 模型。

### 4.2 Application 与原子结算

- 建立唯一入口 `ApplyAttributeModifier(Request, OutResult)`。
- `ApplicationMode = Instant` 原子写入目标 Attribute 的 BaseValue。
- `ApplicationMode = Ongoing` 创建或更新可撤销持续实例并参与 CurrentValue 聚合。
- 同一次调用的所有 Operation 使用同一个 ApplicationMode。
- 所有有效请求必须携带有效 SourceHandle，包括 Instant。
- 所有 Operation 从同一份不可变 AttributeEvaluationSnapshot 计算。
- 任一 Evaluator、Operator、目标或上下文失败时整次 Application 零提交。
- 所有影响结果、调试和确定性的 Operation 遍历按 OperationId 稳定排序。
- `FTcsAttributeModifierApplicationResult` 提供逐 Operation 成功状态、目标、OldValue、NewValue、SourceHandle、OperationId 与失败原因。

### 4.3 Snapshot 与 Ongoing

- Snapshot 只提供 TargetAttributeComponent 内任意 Attribute 的 BaseValue / CurrentValue。
- Snapshot 不隐式读取 Instigator、其他 Actor 或其他 Component 的 Attribute。
- Ongoing 初次 Apply 时当前父实例天然不在已应用记录中。
- Ongoing 重算时，TCS 虚拟排除当前父实例的全部旧结果并重建 Snapshot。
- 当前父实例的全部 Operation 共用同一份排除自身后的 Snapshot。
- 保持当前受控入口中的拉取式惰性重算，不在本 change 实现自动标脏或依赖图。

### 4.4 Merger 与 Operator

- Merger 只用于 Ongoing；Instant 不经过 Merger。
- 内建 Merger 在 Operator 前处理本轮 EvaluatedOperand，不读取最终贡献或上一轮缓存值。
- 单 Operation 按已确认的 Merger / Operator 兼容表执行。
- 多 Operation 使用 `NoMerge` 始终合法。
- 内建选择 / 聚合 Merger 不猜测多 Operation 的整组或逐 Operation 语义。
- Custom Merger 必须声明输入空间、排序、失败策略和可用 Operator。
- `MultiplyAdditive + UseAdditiveSum` 遵守 TCS delta Operand 语义。

### 4.5 配置与编辑器验证

- `TcsDeveloperSettings` 作为 Operator / Merger 兼容规则的权威来源。
- 规则同时支持内建 Operator 枚举与 Custom Operator Class。
- DefinitionAsset 先设置 Operator 时，Merger 下拉菜单过滤不兼容候选。
- 必要时使用 Detail Customization / Class Viewer Filter 完成动态过滤。
- 先设置 Merger 后设置 Operator 时，不自动清空已有值；通过 `PostEditChangeProperty` 与 `IsDataValid` 报告问题。
- 对无法安全执行的组合使用 Error；对可运行但需要显式确认的组合使用 Warning。
- Apply / Recalculate 保留运行时兼容性检查，编辑器过滤不是唯一防线。
- 兼容规则应能清晰表达 `Allowed`、`AllowedWithWarning`、`Forbidden` 三种状态，或提供等价且无歧义的结构。

### 4.6 DefinitionAsset 与 DataTable

- `FTcsAttributeModifierDefRow` 与新 DefinitionAsset 的非标识 UPROPERTY 保持 1:1。
- RowName 承载 AttributeModifierDefId。
- 双向同步使用直接赋值，并验证 Operation Map 与嵌套 `FInstancedStruct` 的编辑和深拷贝。
- 不兼容、不迁移旧单 Operation Row、旧 DataTable 行或旧 DefinitionAsset。
- 当前处于开发阶段，相关资产直接重建。

### 4.7 生命周期、事件与调用方迁移

- 单个 BuffInstance 可以施加多个不同 Ongoing AttributeModifier，但同一 ModifierDefId 最多一次。
- SkillInstance 不得直接施加 Ongoing，只能施加 Instant 或通过目标本地 Buff 间接产生 Ongoing。
- Ongoing 禁止直接跨 Actor；Instant 允许直接作用于 TargetAttributeComponent。
- 非 Buff Ongoing 来源自行创建 SourceHandle，并在生命周期结束时显式移除。
- `RemoveAttribute` 被任意 Ongoing Operation 作为目标引用时硬拒绝且零修改。
- Instant 不产生 AttributeModifier Added / Updated / Removed 事件；成功结算产生 BaseValue Change 事件。
- Ongoing Attribute Change Event 只报告 Clamp 与范围传播后的最终稳定态。
- 删除旧 AttributeModifier 创建、绑定和 Apply API，并迁移 Buff、Skill、StateTree、C++ 与 Blueprint 调用方。
- 删除 `TcsSTTask_ApplyAttributeModifierToTarget`，不保留兼容入口。

### 4.8 不应继续拆分的部分

- 运行时与 Definition 不能拆：新运行时依赖 Operation Map。
- Definition 与 DataTable 不能拆：Row / Asset 必须 1:1，分开会直接破坏同步或迫使引入临时兼容层。
- Instant 与 Ongoing 不能拆：二者共享 Application、Request / Result、Operation、Snapshot、Operator、SourceHandle 与原子提交。
- Merger 与 Ongoing 不能拆：Ongoing 聚合结果依赖 Merger 输入和兼容规则。
- 兼容设置与 Data Validation 不宜拆：运行时、Data Validation 和编辑器必须读取同一权威规则。

动态 Merger 下拉过滤若实现量明显超出预期，可以在 Change 3 中先完成规则结构、Data Validation 和运行时防御，再将纯编辑器交互增强提取为后置 change；不能因此拆出第二套兼容规则。

## 5. Change 4：Ongoing 依赖链自动重算

建议 change id：

```text
add-ongoing-attribute-dependency-recalculation
```

该 change 必须在 Change 3 完成后创建。范围以现有后续设计文档为准：

- DependencyKey 与 Revision。
- 读取时自动收集依赖。
- 反向依赖索引与 Dirty 集合。
- 延迟 Flush 和同帧合并。
- 父 Ongoing ModifierInstance 级原子重算。
- 依赖闭包、稳定排序、拓扑排序与循环检测。
- StateParam 失效通知。
- 未自动观察依赖的显式重算入口。

不引入每帧 Tick、跨 Actor 全局轮询或固定次数迭代兜底。

关联文档：[设计：OngoingAttrMod依赖链惰性重算（后续提案）.md](设计：OngoingAttrMod依赖链惰性重算（后续提案）.md)。

## 6. Change 5：TCS 伤害模块

建议 change id：

```text
add-tcs-damage-runtime
```

该 change 必须在 Change 3 提供稳定的 Instant AttributeModifier 入口后创建。

范围：

- 提供面向业务的 `ApplyDamageToTarget`。
- 独立执行 DamageAmountCalculation。
- 显式读取 Instigator 与 Target 的只读 Attribute 值。
- 收集和应用 DamageModifier。
- 将最终伤害转换为目标 CurrentHealth Attribute 的 Instant AttributeModifier 请求。
- 由伤害模块处理符号、免伤、护盾、暴击、治疗、死亡与战斗记录语义。

非目标：

- 不引入 GAS 风格 Damage Meta Attribute。
- 不在 AttributeComponent 内硬编码 CurrentHealth、MaxHealth、Damage 或 Healing。
- 不让 AttributeModifier Snapshot 隐式承担跨 Actor 公式求值。

## 7. 依赖顺序

推荐顺序：

```text
完成并归档重叠的活动 change
  |
  +-- centralize-source-handle-creation
  |
  +-- simplify-attribute-value-lifecycle
  |
  +-- refactor-attribute-modifier-operations
  |
  +-- add-ongoing-attribute-dependency-recalculation
  |
  +-- add-tcs-damage-runtime
```

创建 Change 3 前至少应完成并归档：

- `add-def-strategy-defaults-and-validation`：直接修改 AttributeModifier Definition 与 DataTable 同步面。
- `add-skill-modifier-runtime-management`：直接修改当前 Attribute OperandBinding 的 StateParam effective 消费契约。

`add-component-runtime-bootstrap` 不是绝对规格前置，但若尚未归档，会与 Component 生命周期文件形成较高的并行冲突风险；优先完成后再启动 Change 3。

## 8. 创建提案前阻塞项

### 8.1 Change 2 阻塞项

- `SetAttributeCurrentValue` 是删除，还是保留为受限底层校正 API。

### 8.2 Change 3 阻塞项

- 非 Buff 来源对同一 ModifierDefId 的重复 Ongoing Apply、更新或并存语义。
- Operator / Merger 配置结构如何无歧义地表达 `Allowed`、`AllowedWithWarning`、`Forbidden`。
- Custom Merger 如何声明默认兼容规则，以及项目设置如何覆盖或收紧该规则。
- 多 Operation 使用内建选择 / 聚合 Merger 时，Data Validation 固定为 Error，还是允许项目配置为强 Warning。

## 9. OpenSpec Capability 映射

一个 change 可以同时修改多个 capability；不能因为涉及多个 spec 文件就机械拆分 change。建议映射如下：

| Change | 主要受影响 capability |
| --- | --- |
| SourceHandle 统一工厂 | `attribute-management`、`combat-manager-subsystems`、`state-runtime-core`，必要时补充独立 SourceHandle capability |
| Attribute 数值生命周期收敛 | `attribute-management` |
| AttributeModifier Operation 重构 | `attribute-modifier-runtime`、`attribute-management`、`def-editor-authoring`，并按调用方迁移情况修改 `buff-runtime` / `skill-runtime` |
| Ongoing 依赖链自动重算 | `attribute-modifier-runtime`、`attribute-management`、`state-parameter-management` |
| TCS 伤害模块 | 新增伤害 capability，并修改与 Attribute Instant 入口直接相关的 capability |

## 10. 总体非目标

- 不按两个设计文档机械创建两个 change。
- 不把全部改造塞进一个同时跨 SourceHandle、Attribute CRUD、Modifier、编辑器和伤害的超大 change。
- 不为获得可编译中间态而添加没有产品需求的旧 API、旧资产或旧 Row 兼容层。
- 不在 AttributeModifier 核心 change 中重做 SkillModifier 的完整 Operand / Operator 架构。
- 不在当前重构中实现跨 Actor Ongoing、跨 Actor 全局依赖图或业务贡献分类账本。

## 11. 规格影响审计总览

本节记录在真正创建 OpenSpec change 前，对当前权威 `openspec/specs/` 与活动 `openspec/changes/` 的过期风险审计结果。后续创建每个 change 时，必须把本节对应条目分散写入该 change 的 spec delta、proposal 或 tasks，不允许等全部实现结束后再集中补规格。

OpenSpec 约束提醒：只要一个现有 Requirement 需要增加、删除或修改 Scenario，就必须在 `## MODIFIED Requirements` 中复制完整 Requirement 区块，而不是只写变化的 Scenario。

### 11.1 汇总统计

| 规划 change | 当前 specs 明确需要 MODIFIED / REMOVED / RENAMED | 当前 specs 需 ADDED 或机械性补充 | 活动 changes 归档后会强化的旧语义 | 备注 |
| --- | ---: | ---: | ---: | --- |
| SourceHandle 统一工厂 | 3 个 Requirement / 3 个明确变化 Scenario；完整 delta 需携带 11 个 Scenario | 至少 3 个新 Requirement / 约 10 个 Scenario | 0 个明确冲突 delta | 重点是静态工厂、Id `0` 合法、Root / Child、无对象注册表。 |
| Attribute 数值生命周期收敛 | 2 个 Requirement / 1 个明确变化 Scenario；另有 1 个 Requirement / 1 个 Scenario 取决于 `SetAttributeCurrentValue` 决策 | Clamp / 容量损失 / 无初始基线等主要为 ADDED | 0 个明确冲突 delta | 创建 Change 2 前必须先定 `SetAttributeCurrentValue`。 |
| AttributeModifier Operation 重构 | 约 7 个 Requirement / 10 个 Scenario 明确过期或冲突；约 4 个 Requirement / 6 个 Scenario 取决于新 ID 与 EvaluatorContext 决策 | def-editor-authoring 需 1 个机械性 MODIFIED Requirement 追加 2 个 Scenario，并新增 3 个 Requirement / 8 个 Scenario；调用方规格主要为 ADDED | 至少 4 个活动 delta Requirement / 6 个 Scenario 归档后需由 Change 3 修改或移除 | 当前影响最大，必须分散到 `attribute-modifier-runtime`、`attribute-management`、`def-editor-authoring`、必要的 `skill-runtime` / `buff-runtime`。 |
| Ongoing 依赖链自动重算 | 当前基线直接计数 0；语义血缘上会替代旧 `RecalculateAttributeCurrentValues` 的 1 个 Requirement / 2 个 Scenario | 依赖键、Revision、Dirty、Flush、拓扑与循环检测主要为 ADDED | 取决于 Change 3 归档后的新 Ongoing 惰性重算 Requirement | Change 4 必须修改 Change 3 之后的规格，不能直接针对旧 OperandBindings 写 delta。 |
| TCS 伤害模块 | 0 | 新伤害 capability 主要为 ADDED | 0 | 当前没有 Damage 规格；不要修改 AttributeModifier 通用 Instant 语义来表达伤害业务。 |

统计口径说明：表中“当前 specs”只统计已归档到 `openspec/specs/` 的权威 Requirement / Scenario；“活动 changes”统计当前非 archive change 在归档后可能写入的旧语义。活动 change 的历史 `proposal.md` / `design.md` 归档后是历史背景，不需要回改，但其 spec delta 一旦归档会成为当前事实，必须由后续 change 负责修改。

### 11.2 Change 1 需要更新的当前 specs

#### `attribute-management`

- `Requirement: Attribute Manager Subsystem 只保留全局职责`：MODIFIED。
- 需要改写 `Scenario: SourceHandle 工厂不再挂在 AttributeManager 上`。
- 旧语义说 SourceHandle 工厂位于更贴近 State 生命周期的实现侧；新语义应为 C++ 权威入口统一委托 `FTcsSourceHandleFactory`，`UTcsAttributeManagerSubsystem` 不再暴露创建入口。
- 完整 MODIFIED 区块必须携带该 Requirement 下的全部 4 个 Scenario。

#### `combat-manager-subsystems`

- `Requirement: Attribute Manager Subsystem 的最终职责`：MODIFIED。
- 需要改写 `Scenario: SourceHandle 工厂下沉到更贴近使用点的实现`。
- 旧语义允许 SourceHandle 工厂下沉到组件或 `UTcsDefinitionManagerSubsystem`；新语义应禁止两者成为权威创建入口。
- 完整 MODIFIED 区块必须携带该 Requirement 下的全部 5 个 Scenario。

#### `state-management`

- `Requirement: FinalizeStateRemoval 保持八步顺序`：MODIFIED。
- 需要改写 `Scenario: Modifier 清理绕过 Subsystem`。
- 旧语义使用 `SourceHandle.Id > 0`，会错误排除合法 Id `0`；新语义必须使用 `SourceHandle.IsValid()` 或 `Id >= 0`。
- 完整 MODIFIED 区块必须携带该 Requirement 下的全部 2 个 Scenario。

#### 需要新增或补充的 SourceHandle 契约

- 新增 `SourceHandle 创建由无状态静态工厂统一`。
- 新增 `SourceHandle 使用非负 ID 判定有效性`。
- 新增或补充 Root / Child 因果链值语义。
- 补充 `UTcsGenericLibrary` 只做 Blueprint 转发，不持有分配状态。
- 补充不在 factory 或全局 subsystem 中建立 `HandleId -> UObject` 对象注册表。
- 补充 Blueprint 转发不能绕过 authority 限制；最终 authority SourceHandle 仍不得由预测客户端自行生成。

### 11.3 Change 2 需要更新的当前 specs

#### `attribute-management`

- `Requirement: Attribute 定义通过 Manager 解析`：MODIFIED。
- 需要改写 `Scenario: AddAttribute 从 DefinitionManager 解析定义`。
- 旧场景使用 `AddAttribute(Name, InitValue)`；新场景应验证无初始数值参数的 AddAttribute 仍通过 DefinitionManager 解析定义。

- `Requirement: 通过 Virtual 明确 Public Component API 的扩展点`：MODIFIED。
- 至少删除 `ResetAttribute` 作为 public virtual 扩展点的正向契约。
- `SetAttributeCurrentValue` 是否继续列入该 Requirement，取决于 Change 2 创建前的最终决策。
- 现有两个 Scenario 本身不直接验证 `ResetAttribute`，但完整 MODIFIED 区块必须原样或等价携带。

- `Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑`：待决。
- 如果删除 `SetAttributeCurrentValue`，或将其改成不再走统一 ClampStrategy 的受限底层 API，则 `Scenario: 子类扩展夹值策略` 必须同步修改。

- `Requirement: Attribute 夹值绑定到单一 Component 作用域`：保持现有语义，另用 ADDED Requirements 补充 Base / Current 共用 Range、动态 Min / Max 读取 CurrentValue、容量降低永久损失等规则。

### 11.4 Change 3 需要更新的当前 specs

#### `attribute-management`

- `Requirement: Modifier 管线保持行为不变量`：MODIFIED。
- `Scenario: ApplyModifier 按正确顺序写入时间戳` 旧 Apply API 过期，需要改成 `ApplyAttributeModifier(Request, OutResult)` 或新父 Ongoing 实例语义。
- `Scenario: SourceHandle 索引在移除后保持一致` 需要按新父 Ongoing 实例 / Operation 粒度重写，最终取决于 SourceHandle 索引粒度和 ID 模型。
- `Scenario: 范围约束是最后一步` 与新事件边界冲突，应改为 Clamp / 范围传播完成后才广播最终稳定态事件。

- `Requirement: Modifier 创建通过 Manager 获取全局 ID`：RENAMED + MODIFIED。
- 需要修正标题，并将旧 `CreateAttributeModifier` 场景重写为新 Ongoing 父实例或 Application 分配模型。
- 是否继续保留 `ModifierInstId` / `ModifierChangeBatchId` 取决于 Change 3 创建前的 ID 决策。

- `Requirement: 通过 Virtual 明确 Public Component API 的扩展点`：MODIFIED。
- 旧 `CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`ApplyModifierWithSourceHandle` 等 API 会删除或迁移。
- `Scenario: 包装器不可覆写` 不能再验证 `ApplyModifierWithSourceHandle`，需要替换为新入口扩展策略。

- `Requirement: Attribute Manager Subsystem 只保留全局职责`：可能 MODIFIED。
- 若 Change 3 改变 `ModifierInstId` / `ModifierChangeBatchId` 的存在和分配粒度，则 `Scenario: Attribute 与 Modifier 的全局 ID 工厂下沉到 Component` 必须同步修改。

#### `attribute-modifier-runtime`

- `Purpose` 整段必须更新；旧 Purpose 以 OperandBindings 和 `CreateAttributeModifierWithBindings` 为 capability 主语义。
- `Requirement: Operand 动态绑定`：REMOVED。
- `Requirement: RecalculateAttributeCurrentValues 中刷新 Operand`：RENAMED + MODIFIED，建议替换为 Ongoing Modifier 受控惰性重算 / Snapshot 计算语义。
- `Requirement: ResolveStateParamInstances`：建议 REMOVED；若新 Evaluator 仍复用类似 helper，应改成新 EvaluatorContext 的 ADDED 或 MODIFIED 契约。
- `Requirement: CreateAttributeModifierWithBindings`：REMOVED。
- `Requirement: 删除 CreateAttributeModifierWithOperands`：RENAMED + MODIFIED，扩展为删除旧 AttributeModifier 创建、绑定和 Apply API，并更新迁移目标。
- `Requirement: AttributeModifierInstance 新增引用字段`：建议 REMOVED；若新 EvaluatorContext 仍保留 `SourceStateInstance` / `SourceSkillEntry`，应重新定义字段所属对象、生命周期和依赖收集边界。

#### `def-editor-authoring`

- `Requirement: Row Struct 直接赋值映射`：机械性 MODIFIED，用于追加两个 AttributeModifier 专项 Scenario，现有通用语义不冲突。
- 建议新增 `Scenario: AttributeModifier Row 与新 Definition 非标识字段保持 1:1`。
- 建议新增 `Scenario: Operation Map 中嵌套 OperandPayload 可编辑并完整深拷贝`。
- 新增 `Requirement: AttributeModifier Operation Map 编辑器创作`。
- 新增 `Requirement: Operator / Merger 兼容规则驱动创作与验证`。
- 新增 `Requirement: 旧 AttributeModifier 创作数据不迁移`。
- `DataTable ↔ DefAsset 双向自动同步` 本身不应误报过期；新旧 Schema 不迁移应作为独立 Requirement 明确。

#### 调用方相关 capability

- 当前 `buff-runtime` 没有直接过期的 AttributeModifier 契约；Change 3 应新增 BuffInstance 持有 Ongoing AttributeModifier、同一 BuffInstance 同一 ModifierDefId 最多一次、生命周期结束清理等契约。
- 当前 `skill-runtime` 没有明确允许 SkillInstance 直接施加 Ongoing 的旧契约；Change 3 应新增 SkillInstance 不能直接 Ongoing、只能 Instant 或通过目标本地 Buff 间接 Ongoing 的契约。
- 当前 `state-management` 已清零跨 Actor State facade；Change 3 删除 `TcsSTTask_ApplyAttributeModifierToTarget` 主要是代码/API 任务，当前 specs 中没有同名 Requirement 可 REMOVED。
- 当前没有伤害 capability；不要把 Damage 业务写入 AttributeModifier 通用规格。

### 11.5 Change 4 需要更新的 specs

- Change 4 不应直接修改当前旧 `Requirement: RecalculateAttributeCurrentValues 中刷新 Operand`。
- 正确顺序是：Change 3 先把该 Requirement 替换为 Ongoing Modifier 受控惰性重算或等价新契约；Change 4 再 MODIFIED 这个新契约，加入 DependencyKey、Revision、Dirty、Flush、拓扑排序和循环检测。
- `Attribute Component 拥有 Actor 本地 Attribute 业务逻辑` 与 `Attribute 夹值绑定到单一 Component 作用域` 可保持现有语义，通过 ADDED Requirements 补依赖机制。

### 11.6 Change 5 需要更新的 specs

- 当前 specs 中没有 Damage、DamageAmountCalculation、DamageModifier、CurrentHealth 伤害结算或 Damage Meta Attribute 的既有 Requirement。
- Change 5 应新增独立伤害 capability，主要使用 ADDED Requirements。
- 只有当伤害模块改变 Change 3 已归档的 Instant AttributeModifier 通用契约时，才允许 MODIFIED AttributeModifier 规格；按当前规划不应发生。

## 12. 活动 change 归档风险

以下活动 change 当前可以继续按原范围完成，但创建 Change 3 前必须认识到它们归档后会把部分旧 AttributeModifier 语义写回当前 specs。后续 Change 3 应负责修改或删除这些归档后的事实，不要在这些已接近完成的活动 change 中强行预改 AttributeModifier 新架构。

### 12.1 `add-def-strategy-defaults-and-validation`

- 该 change 当前 `12/13`，直接修改 Definition / Row 默认策略与 DataTable 同步面。
- 归档后，`def-editor-authoring` 会新增 `DefAsset 策略字段默认值与镜像同步` 和 `DefAsset 策略字段有效性勘误`。
- `Scenario: AttributeModifierDef、BuffDef 与 StateSlotDef 使用 concrete 默认策略` 中的 `MergerType -> UTcsAttrModMerger_NoMerge` 仍可与新模型兼容，因为新 AttributeModifier 仍保留 MergerType。
- `Scenario: 非默认项不被强行补值` 提到 `UTcsAttributeModifierDefinition::ModifierType`；Change 3 删除 `ModifierType` 后必须 MODIFIED 该场景，改成新模型中的非默认项，例如缺失 Custom Operator、缺失 Payload 或其他仍不应自动补值的字段。
- 处理建议：先完成并归档当前 change；Change 3 再更新该归档后的具体场景。

### 12.2 `add-skill-modifier-runtime-management`

- 该 change 当前 `30/31`，直接修改 `attribute-modifier-runtime` 的 OperandBinding effective 消费契约。
- 归档后，`Requirement: RecalculateAttributeCurrentValues 中刷新 Operand` 会继续强化 `OperandBindings -> StateParam effective -> Operands` 旧模型。
- 归档后，`Requirement: AttributeModifier 已解析 Operand 的统一访问口径` 会将 `FTcsAttributeModifierInstance::Operands` 规定为已解析 Operand 的权威读取位置。
- 这两项都会被 Change 3 的 OperandPayload / OperandEvaluator / EvaluatedOperand 模型替代；Change 3 必须 MODIFIED / REMOVED 它们。
- 归档后，`skill-runtime` 的 `Requirement: 公开参数读取默认返回 effective value` 正文会提到“跨系统消费（含 Attribute OperandBinding）”；Change 3 删除 OperandBinding 后，应将该表述改成新 StateParam OperandEvaluator 或等价消费路径。
- 处理建议：先完成并归档当前 change；Change 3 再整体替换 AttributeModifier 侧旧解析模型。

### 12.3 `add-component-runtime-bootstrap`

- 该 change 当前 `42/47`，新增 `component-runtime-bootstrap` capability。
- 其 spec 主要规定 runtime-ready 生命周期、未 ready API 保护、组件依赖图和 StateTree 启动屏障。
- 未发现会直接写入旧 AttributeModifier Schema、OperandBindings、SourceHandle 工厂或 Attribute CRUD 初始值语义的 Requirement。
- Change 2 / Change 3 实现时必须遵守其未 ready 命令型 API 拒绝策略，但当前不需要预先修改该活动 change。

## 13. 提案内备注要求

创建后续 OpenSpec change 时，每个 proposal 或 design 至少要包含以下备注，避免规格过期项散落无主：

- Change 1 备注：列出会 MODIFIED 的 `attribute-management`、`combat-manager-subsystems`、`state-management` Requirement，并说明 `Id == 0` 合法性修正。
- Change 2 备注：列出 `AddAttribute(Name, InitValue)`、`ResetAttribute`、`SetAttributeCurrentValue` 的处理方式，并说明 Clamp / 容量损失是 ADDED 契约。
- Change 3 备注：列出 `attribute-modifier-runtime` 中将 REMOVED / RENAMED / MODIFIED 的旧 OperandBindings 和 WithBindings Requirements，并列出 `def-editor-authoring` 的新增创作与兼容验证要求。
- Change 4 备注：明确它修改的是 Change 3 归档后的 Ongoing 惰性重算契约，而不是当前旧 OperandBinding 契约。
- Change 5 备注：明确新增伤害 capability，不引入 Damage Meta Attribute，不改 AttributeModifier 通用 Instant 语义。
