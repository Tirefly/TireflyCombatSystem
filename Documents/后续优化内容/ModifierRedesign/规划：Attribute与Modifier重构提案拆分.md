# 规划：Attribute 与 Modifier 重构提案拆分

> 状态：Change 1–4 均已归档。Change 4 `add-ongoing-attribute-dependency-recalculation` 已实现、编译通过，并归档为 `openspec/changes/archive/2026-08-08-add-ongoing-attribute-dependency-recalculation/`；delta 已写入当前 `openspec/specs/`。Change 5 仍为后续。本文不替代主设计文档中的行为契约。
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
4. Ongoing 依赖链自动重算（已归档）
5. TCS 伤害模块（后续）
```

第一项与第二项在概念上可以并行，但都应先于第三项完成。第四项已在第三项之后完成并归档；第五项不得提前并入第三项。

## 2. Change 1：SourceHandle 统一工厂

建议 change id：

```text
centralize-source-handle-creation
```

### 2.0 当前实施状态

- 已实现并归档为 `openspec/changes/archive/2026-08-03-centralize-source-handle-creation/`。
- 已实现无状态静态 `FTcsSourceHandleFactory`、Root / Child API、`Id == 0` 有效性、`UTcsGenericLibrary` Blueprint authority 转发，以及无对象反查注册表边界。
- 已删除 `UTcsStateComponent::CreateSourceHandle` 与 `NextSourceHandleId`，并迁移现有 State / Skill 来源创建路径。
- 已通过编译、Glue / 用户脚本编译和 4 个 `TireflyCombatSystem.SourceHandle` 自动化测试；delta 已写入当前 `openspec/specs/`（含新 capability `source-handle-runtime`）。

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
- 删除 `SetAttributeCurrentValue`，不保留受限底层校正 API。
- 更新 `attribute-management` 规格和现有 public API 扩展点描述。

### 3.2 非目标

- 不引入多 Operation AttributeModifier。
- 不实现 OperandEvaluator、AttributeEvaluationSnapshot 或 Merger 兼容规则。
- 不实现 `RemoveAttribute` 对多 Operation Ongoing 引用的检查；该规则依赖新的 Ongoing Operation 记录，归入 Change 3。
- 不实现伤害、治疗、复活或存档业务规则。

### 3.3 独立原因

Attribute 数值状态、CRUD 与 Clamp 生命周期不依赖新的 Modifier Operation 模型。现有 `attribute-management` 规格仍包含 `AddAttribute(Name, InitValue)`、`ResetAttribute` 与相关 virtual API，适合先独立收敛并形成清晰的 Attribute 底盘。

### 3.4 当前实施状态

- 已实现并归档为 `openspec/changes/archive/2026-08-03-simplify-attribute-value-lifecycle/`。
- 已删除 `InitialValue`、`ResetAttribute`、`SetAttributeCurrentValue`，以及 `AddAttribute` / `AddAttributeByTag` 的 InitValue 参数。
- 已迁移 C# 测试 Director 与 Core fixture；delta 已写入当前 `openspec/specs/attribute-management/`。

## 4. Change 3：AttributeModifier Operation 重构

建议 change id：

```text
refactor-attribute-modifier-operations
```

### 4.0 当前实施状态

- 已实现并归档为 `openspec/changes/archive/2026-08-06-refactor-attribute-modifier-operations/`。
- delta 已写入当前 `openspec/specs/`：`attribute-modifier-runtime`、`attribute-management`、`def-editor-authoring`、`skill-runtime`、`combat-manager-subsystems`。
- 已落地 Operation 模型、唯一 `ApplyAttributeModifier` 入口、Snapshot 自排除、StateInstance Ongoing 宿主、EvaluatedOperand Merger、`TcsDeveloperSettings` Operator/Merger 兼容规则、Def Data Validation、保留 `ModifierInstId` / 删除 ChangeBatchId，以及旧 API / Target StateTree Task 删除。
- Operation 类型位于 `Source/TireflyCombatSystem/Public|Private/Attribute/AttrModOperation/`。
- `UTcsAttributeModifierDefinition` / Row 以 `Operations` Map 创作；`MergerType` 默认 `NoMerge`。
- AttributeComponent 实现按职责拆分：`AttrModApplication` / `AttrModEvaluation` / `AttrModAggregation` / `Query` / `Events` / `Clamp` / `RangeConstraints` / `OngoingCalculation` 等。
- 构建验证：`TireflyGameplayUtilsEditor Win64 Development`、相关 Glue / Managed 编译通过；按用户规范不设自动化测试任务。

这是本轮改造的核心纵向 change，现已完成并归档。

### 4.1 Operation 与 Definition

- 建立 `OperandPayload -> OperandEvaluator -> EvaluatedOperand -> Operator` 模型。
- AttributeModifier 的 Evaluator 与 Operand 固定为 Numeric / `float`。
- `UTcsAttributeModifierDefinition` 使用 `TMap<FName, FTcsAttributeOperationSpec>`，Key 为稳定 `OperationId`。
- Operation 保存 TargetAttributeId、Operator / CustomOperator、Evaluator 与 OperandPayload。
- `FInstancedStruct` 只承载 OperandPayload。
- Operation 的公共类型按职责拆分到 `AttrModOperation`：Payload / Numeric Evaluator、Constant Payload / Evaluator、Custom Operator 与 Operation Spec / 内建 Operator 分发分别独立维护。
- 新建 Operation 默认 Constant Evaluator 与 Constant Payload；不默认 Operator 或 Custom Operator，Custom Operator 仅在 `AMO_Custom` 时可编辑。
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
- 先设置 Merger 后设置 Operator 时，不自动清空已有值；通过 `PostEditChangeProperty` 与 `IsDataValid` 报告 Error。
- Operator / Merger 兼容规则采用二元判定：`Allowed` 或 `Forbidden`；不引入 `AllowedWithWarning` 中间态。对语义必然错误或不可安全执行的组合使用 Forbidden（Data Validation 报 Error）；对可安全执行的组合使用 Allowed。`Override + UseMaximum / UseMinimum / UseAdditiveSum` 归为 Forbidden。
- 多 Operation + 内建选择 / 聚合 Merger（`UseMaximum`、`UseMinimum`、`UseAdditiveSum`、`UseNewest`、`UseOldest`）的 Data Validation 默认 Error；`TcsDeveloperSettings` 可降级为强 Warning。`NoMerge` 始终合法，不触发该验证。
- Custom Merger 类自带默认兼容 Operator 列表；`TcsDeveloperSettings` 只能收紧或调整 Severity，不能放宽到类未声明的 Operator。
- Apply / Recalculate 保留运行时兼容性检查，编辑器过滤不是唯一防线。

### 4.6 DefinitionAsset 与 DataTable

- `FTcsAttributeModifierDefRow` 与新 DefinitionAsset 的非标识 UPROPERTY 保持 1:1。
- RowName 承载 AttributeModifierDefId。
- 双向同步使用直接赋值，并验证 Operation Map 与嵌套 `FInstancedStruct` 的编辑和深拷贝。
- 不兼容、不迁移旧单 Operation Row、旧 DataTable 行或旧 DefinitionAsset。
- 当前处于开发阶段，相关资产直接重建。

### 4.7 生命周期、事件与调用方迁移

- 单个 StateInstance 可以施加多个不同 Ongoing AttributeModifier，但同一 ModifierDefId 最多一次。
- 所有 Ongoing AttributeModifier 必须经由 StateInstance 持有与施加，不允许直接作用于 TargetAttributeComponent；装备、天赋、场景光环等持续效果必须在 owning Actor 上创建 StateInstance 来持有其 Ongoing。
- SkillInstance 不得直接施加 Ongoing，只能施加 Instant 或通过目标本地 StateInstance 间接产生 Ongoing。
- Instant 允许直接作用于 TargetAttributeComponent（含跨 Actor）。
- 非 Buff 来源的 Ongoing 生命周期由其 StateInstance 管理；StateInstance 结束时显式调用 `RemoveOngoingModifiersBySourceHandle(SourceHandle)` 清理。
- `RemoveAttribute` 被任意 Ongoing Operation 作为目标引用时硬拒绝且零修改。
- Instant 不产生 AttributeModifier Added / Updated / Removed 事件；成功结算产生 BaseValue Change 事件。
- Ongoing Attribute Change Event 只报告 Clamp 与范围传播后的最终稳定态。
- 删除旧 AttributeModifier 创建、绑定和 Apply API，并迁移 StateInstance、Skill、StateTree、C++ 与 Blueprint 调用方。
- 删除 `TcsSTTask_ApplyAttributeModifierToTarget`，不保留兼容入口。
- 保留 `ModifierInstId` 作为 Ongoing 实例稳定标识；删除 `ModifierChangeBatchId` 及其静态计数器。

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

### 5.0 当前实施状态

- 已实现并归档为 `openspec/changes/archive/2026-08-08-add-ongoing-attribute-dependency-recalculation/`。
- delta 已写入当前 `openspec/specs/`：`attribute-modifier-runtime`、`attribute-management`、`state-parameter-management`。
- 已落地 DependencyKey / Revision / 反向索引 / Dirty 集合、Context 自动收集、策略 C Flush（受控路径安全点同步 + `TG_PostUpdateWork` 帧末）、父实例级原子重算、最小 SCC 与临时跳过、单一注册表空 `AppliedOperations` 无贡献语义、Source Step 5 先删来源再重建、显式 `RequestOngoingModifierRecalculation(SourceHandle)`。
- 首版自动观察范围：目标 Attribute CurrentValue + 本地 Buff Numeric StateParam effective；SkillEntry / 跨 Actor 等走显式 Request。
- 验证：`openspec validate --strict`、`TireflyGameplayUtilsEditor Win64 Development` 编译通过；按用户规范不设自动化测试任务。

### 5.1 范围（已完成）

该 change 在 Change 3 完成后创建并完成。范围包括：

- DependencyKey 与 Revision。
- 读取时自动收集依赖。
- 反向依赖索引与 Dirty 集合。
- 延迟 Flush 和同帧合并。
- 父 Ongoing ModifierInstance 级原子重算。
- 依赖闭包、稳定排序、拓扑排序与循环检测（精确最小 SCC）。
- StateParam 失效通知。
- 未自动观察依赖的显式重算入口。
- 已注册失败父实例临时跳过（清空贡献、保留注册，后续 Attribute 事务重试）。
- Initial Apply 失败零提交、不保存空贡献注册项。

不引入每帧全量 Tick、跨 Actor 全局轮询、固定次数迭代兜底，也不引入 Quarantined / Disabled / Retry 第二套容器。

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
完成并归档重叠的活动 change（已完成）
  |
  +-- centralize-source-handle-creation（已归档 2026-08-03）
  |
  +-- simplify-attribute-value-lifecycle（已归档 2026-08-03）
  |
  +-- refactor-attribute-modifier-operations（已归档 2026-08-06）
  |
  +-- add-ongoing-attribute-dependency-recalculation（已归档 2026-08-08）
  |
  +-- add-tcs-damage-runtime（后续）
```

创建 Change 3 前的活动 change 前置已全部完成并归档：

- `add-def-strategy-defaults-and-validation` → `2026-08-03-add-def-strategy-defaults-and-validation`
- `add-skill-modifier-runtime-management` → `2026-08-03-add-skill-modifier-runtime-management`
- `add-component-runtime-bootstrap` → `2026-08-03-add-component-runtime-bootstrap`

Change 3 / Change 4 已归档：

- `refactor-attribute-modifier-operations` → `2026-08-06-refactor-attribute-modifier-operations`
- `add-ongoing-attribute-dependency-recalculation` → `2026-08-08-add-ongoing-attribute-dependency-recalculation`

## 8. 创建提案前阻塞项

### 8.1 Change 2 阻塞项

- 无。`SetAttributeCurrentValue` 已确认删除；业务初始化、读档、等级变化和资源恢复使用明确的 BaseValue 写入，伤害、治疗和周期结算等待后续 Instant AttributeModifier 入口。

### 8.2 Change 3 阻塞项

- ~~非 Buff 来源对同一 ModifierDefId 的重复 Ongoing Apply、更新或并存语义。~~ 已确认：所有 Ongoing 必须经由 StateInstance 持有与施加；同一 StateInstance 同 DefId 最多一次，不同 StateInstance 同 DefId 允许并存，由 Merger 处理叠加（见提案前待确认问题 2.4）。
- ~~Operator / Merger 配置结构如何无歧义地表达 `Allowed`、`AllowedWithWarning`、`Forbidden`。~~ 已确认：采用二元 `Allowed` / `Forbidden`，不引入 `AllowedWithWarning`（见提案前待确认问题 2.13.1）。
- ~~Custom Merger 如何声明默认兼容规则，以及项目设置如何覆盖或收紧该规则。~~ 已确认：Custom Merger 类自带默认兼容 Operator 列表，`TcsDeveloperSettings` 只能收紧，不能放宽（见提案前待确认问题 2.13.1）。
- ~~多 Operation 使用内建选择 / 聚合 Merger 时，Data Validation 固定为 Error，还是允许项目配置为强 Warning。~~ 已确认：默认 Error，`TcsDeveloperSettings` 可降级为强 Warning；`NoMerge` 始终合法（见提案前待确认问题 2.16）。
- `ModifierInstId` / `ModifierChangeBatchId` 在新父 Ongoing 实例模型中的去留已确认：保留 `ModifierInstId`，删除 `ModifierChangeBatchId`（见提案前待确认问题 2.17）。

Change 3 / Change 4 提案前阻塞项已全部确认，且均已归档（见第 7 节）。后续应创建 Change 5 伤害模块提案，而不是继续修改已归档的 Change 3 / Change 4 快照。

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
| SourceHandle 统一工厂 | 3 个 Requirement / 3 个明确变化 Scenario；完整 delta 需携带 11 个 Scenario | 至少 3 个新 Requirement / 约 10 个 Scenario | 0 个明确冲突 delta | `centralize-source-handle-creation` 已实现且 tasks 完成，待单独归档；重点是静态工厂、Id `0` 合法、Root / Child、无对象注册表。 |
| Attribute 数值生命周期收敛 | 3 个 Requirement / 3 个明确变化 Scenario；完整 delta 需携带 5 个 Scenario | 3 个新 Requirement / 9 个 Scenario | 0 个明确冲突 delta | `SetAttributeCurrentValue` 已确认删除；`simplify-attribute-value-lifecycle` 已创建，需覆盖无初始基线、统一 Clamp、容量降低永久损失和测试 Director 迁移。 |
| AttributeModifier Operation 重构 | 约 7 个 Requirement / 10 个 Scenario 明确过期或冲突；约 4 个 Requirement / 6 个 Scenario 取决于新 ID 与 EvaluatorContext 决策 | def-editor-authoring 需 1 个机械性 MODIFIED Requirement 追加 2 个 Scenario，并新增 3 个 Requirement / 8 个 Scenario；调用方规格主要为 ADDED | 至少 4 个活动 delta Requirement / 6 个 Scenario 归档后需由 Change 3 修改或移除 | 当前影响最大，必须分散到 `attribute-modifier-runtime`、`attribute-management`、`def-editor-authoring`、必要的 `skill-runtime` / `buff-runtime`。 |
| Ongoing 依赖链自动重算 | 已归档到当前 specs：`attribute-modifier-runtime` +7、`attribute-management` +1/~1、`state-parameter-management` +2 | 依赖键、Revision、Dirty、Flush、拓扑/SCC、临时跳过、显式 Request 已写入 | 0 | 已归档 `2026-08-08-add-ongoing-attribute-dependency-recalculation`；后续以当前 specs 为准。 |
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
- 删除 `ResetAttribute` 与 `SetAttributeCurrentValue` 作为 public virtual 扩展点的正向契约。
- 现有两个 Scenario 本身不直接验证 `ResetAttribute`，但完整 MODIFIED 区块必须原样或等价携带。

- `Requirement: Attribute Component 拥有 Actor 本地 Attribute 业务逻辑`：MODIFIED。
- `Scenario: 子类扩展夹值策略` 必须删除 `SetAttributeCurrentValue`，并改为验证 AddAttribute 占位值、SetAttributeBaseValue 与 modifier / range 传播路径。

- `Requirement: Attribute 夹值绑定到单一 Component 作用域`：保持现有语义，另用 ADDED Requirements 补充 Base / Current 共用 Range、动态 Min / Max 读取 CurrentValue、容量降低永久损失等规则。

### 11.4 Change 3 需要更新的当前 specs

#### `attribute-management`

- `Requirement: Modifier 管线保持行为不变量`：MODIFIED。
- `Scenario: ApplyModifier 按正确顺序写入时间戳` 旧 Apply API 过期，需要改成 `ApplyAttributeModifier(Request, OutResult)` 或新父 Ongoing 实例语义。
- `Scenario: SourceHandle 索引在移除后保持一致` 需要按新父 Ongoing 实例 / Operation 粒度重写，最终取决于 SourceHandle 索引粒度和 ID 模型。
- `Scenario: 范围约束是最后一步` 与新事件边界冲突，应改为 Clamp / 范围传播完成后才广播最终稳定态事件。

- `Requirement: Modifier 创建通过 Manager 获取全局 ID`：RENAMED + MODIFIED。
- 需要修正标题，并将旧 `CreateAttributeModifier` 场景重写为新 Ongoing 父实例或 Application 分配模型。
- 保留 `ModifierInstId`；删除 `ModifierChangeBatchId` 及其静态计数器（见提案前待确认问题 2.17）。

- `Requirement: 通过 Virtual 明确 Public Component API 的扩展点`：MODIFIED。
- 旧 `CreateAttributeModifier`、`CreateAttributeModifierWithBindings`、`ApplyModifier`、`ApplyModifierWithSourceHandle` 等 API 会删除或迁移。
- `Scenario: 包装器不可覆写` 不能再验证 `ApplyModifierWithSourceHandle`，需要替换为新入口扩展策略。

- `Requirement: Attribute Manager Subsystem 只保留全局职责`：可能 MODIFIED。
- Change 3 删除 `ModifierChangeBatchId` 后，`Scenario: Attribute 与 Modifier 的全局 ID 工厂下沉到 Component` 必须同步修改，移除 `ModifierChangeBatchId` 分配职责，保留 `AttributeInstId` 与 `ModifierInstId`。

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

- 当前 `buff-runtime` 没有直接过期的 AttributeModifier 契约；Change 3 应新增 StateInstance 持有 Ongoing AttributeModifier、同一 StateInstance 同一 ModifierDefId 最多一次、生命周期结束清理等契约。所有 Ongoing 必须经由 StateInstance 持有与施加，不允许直接作用于 TargetAttributeComponent。
- 当前 `skill-runtime` 没有明确允许 SkillInstance 直接施加 Ongoing 的旧契约；Change 3 应新增 SkillInstance 不能直接 Ongoing、只能 Instant 或通过目标本地 StateInstance 间接 Ongoing 的契约。
- 当前 `state-management` 已清零跨 Actor State facade；Change 3 删除 `TcsSTTask_ApplyAttributeModifierToTarget` 主要是代码/API 任务，当前 specs 中没有同名 Requirement 可 REMOVED。
- 当前没有伤害 capability；不要把 Damage 业务写入 AttributeModifier 通用规格。

### 11.5 Change 4 需要更新的 specs

- 已完成：`2026-08-08-add-ongoing-attribute-dependency-recalculation` 已归档，delta 已写入当前 specs。
- `attribute-modifier-runtime`：新增依赖键自动收集、Revision 标脏、策略 C Flush、循环/SCC、显式 Request、移除清理、临时跳过等 Requirements。
- `attribute-management`：更新 Modifier 管线不变量（路径安全点 Flush + 事务内 candidate clamp），并新增 AttributeComponent 拥有 Dirty Flush 调度。
- `state-parameter-management`：新增 effective 变化可通知依赖失效且禁止同步 Attribute 重算；跨模块消费仍读 effective。
- 后续以当前 `openspec/specs/` 为准；归档目录仅作历史背景。

### 11.6 Change 5 需要更新的 specs

- 当前 specs 中没有 Damage、DamageAmountCalculation、DamageModifier、CurrentHealth 伤害结算或 Damage Meta Attribute 的既有 Requirement。
- Change 5 应新增独立伤害 capability，主要使用 ADDED Requirements。
- 只有当伤害模块改变 Change 3 已归档的 Instant AttributeModifier 通用契约时，才允许 MODIFIED AttributeModifier 规格；按当前规划不应发生。

## 12. 已归档 change 对 Change 3 的规格影响

以下 change 已于 2026-08-03 归档，其 delta 已写入当前 `openspec/specs/`。Change 3 必须负责修改或删除这些归档后的事实中与新 AttributeModifier 架构冲突的部分。

### 12.1 `2026-08-03-add-def-strategy-defaults-and-validation`

- 已写入 `def-editor-authoring`：`DefAsset 策略字段默认值与镜像同步` 和 `DefAsset 策略字段有效性勘误`。
- `Scenario: AttributeModifierDef、BuffDef 与 StateSlotDef 使用 concrete 默认策略` 中的 `MergerType -> UTcsAttrModMerger_NoMerge` 仍可与新模型兼容。
- `Scenario: 非默认项不被强行补值` 提到 `UTcsAttributeModifierDefinition::ModifierType`；Change 3 删除 `ModifierType` 后必须 MODIFIED 该场景，改成新模型中的非默认项。

### 12.2 `2026-08-03-add-skill-modifier-runtime-management`

- 已写入 `attribute-modifier-runtime`：`Requirement: RecalculateAttributeCurrentValues 中刷新 Operand` 继续强化 `OperandBindings -> StateParam effective -> Operands` 旧模型。
- 已写入 `Requirement: AttributeModifier 已解析 Operand 的统一访问口径`，将 `FTcsAttributeModifierInstance::Operands` 规定为已解析 Operand 的权威读取位置。
- 这两项都会被 Change 3 的 OperandPayload / OperandEvaluator / EvaluatedOperand 模型替代；Change 3 必须 MODIFIED / REMOVED 它们。
- `skill-runtime` 的 `Requirement: 公开参数读取默认返回 effective value` 正文提到“跨系统消费（含 Attribute OperandBinding）”；Change 3 删除 OperandBinding 后，应将该表述改成新 StateParam OperandEvaluator 或等价消费路径。

### 12.3 `2026-08-03-add-component-runtime-bootstrap`

- 已新增 `component-runtime-bootstrap` capability。
- Change 3 实现时必须遵守其未 ready 命令型 API 拒绝策略。

## 13. 提案内备注要求

创建后续 OpenSpec change 时，每个 proposal 或 design 至少要包含以下备注，避免规格过期项散落无主：

- Change 1 备注：列出会 MODIFIED 的 `attribute-management`、`combat-manager-subsystems`、`state-management` Requirement，并说明 `Id == 0` 合法性修正。
- Change 2 备注：列出 `AddAttribute(Name, InitValue)`、`ResetAttribute`、`SetAttributeCurrentValue` 的处理方式，并说明 Clamp / 容量损失是 ADDED 契约。
- Change 3 备注：列出 `attribute-modifier-runtime` 中将 REMOVED / RENAMED / MODIFIED 的旧 OperandBindings 和 WithBindings Requirements，并列出 `def-editor-authoring` 的新增创作与兼容验证要求。
- Change 4 备注：明确它修改的是 Change 3 归档后的 Ongoing 惰性重算契约，而不是当前旧 OperandBinding 契约。
- Change 5 备注：明确新增伤害 capability，不引入 Damage Meta Attribute，不改 AttributeModifier 通用 Instant 语义。
