# AttributeModifier 重构：提案前待确认问题

> 状态：讨论中。本文记录创建 OpenSpec change 前必须冻结的架构契约，以及可留待 proposal 设计阶段细化的事项；未标记“已确认”的建议不是冻结决策。
>
> 关联主方案：[设计：Modifier操作数与AttributeOperation模型（待审核）.md](设计：Modifier操作数与AttributeOperation模型（待审核）.md)。

## 1. 已确认前提

以下结论已讨论确认，应作为后续逐项决策的约束，而不是重新讨论的候选方案：

- `SkillModifier` 保持“一个 Definition 修改一个 StateParam”；`AttributeModifier` 使用以 `OperationId` 为 Key 的多 Operation Map。
- `OperandPayload -> OperandEvaluator -> EvaluatedOperand -> Operator` 分离操作数计算与数值施加；`FInstancedStruct` 只承载 OperandPayload。
- Definition 不暴露 `ModifierMode`；调用方只通过 `ApplyAttributeModifier(Request, OutResult)` 与 `ApplicationMode = Instant | Ongoing` 决定结算路径。
- `Instant` 一次性原子修改目标 Attribute 的 `BaseValue`；`Ongoing` 作为可撤销贡献参与 `CurrentValue` 聚合。
- 每次调用中的所有 Operation 必须使用同一个 ApplicationMode；不得混合 BaseValue 写入与 CurrentValue 聚合。
- 所有有效 AttributeModifierInstance 都必须具有有效 `SourceHandle`，包括 `Instant`。
- `FTcsAttributeInstance` 删除 `InitValue` / `InitialValue` 等创建时数值基线；数值状态只保留 `BaseValue` 和 `CurrentValue`。
- `AddAttribute` 不接收初始数值；业务层根据数据表、等级、存档或角色配置写入 BaseValue。
- 删除 `ResetAttribute`；重生、回档、等级回退与资源恢复属于业务层语义。
- `Damage` 不是 Attribute Core 内置 Meta Attribute；后续独立伤害模块负责 `ApplyDamageToTarget -> DamageAmountCalculation -> ApplyAttributeModifier(Instant)`。
- 所有影响结算、调试或未来网络确定性的 Operation 遍历必须按 `OperationId` 稳定排序。
- 多 Operation Ongoing Modifier 使用 `NoMerge` 始终合法；内建选择 / 聚合 Merger 不得猜测整组或逐 Operation 语义。

## 2. 创建提案前必须确认

### 2.1 AttributeOperation 的值类型范围（已确认）

首版及后续 AttributeOperation 都只支持 `float` Attribute 与 Numeric Operand。Bool / Vector Evaluator 与 Operator 不属于 AttributeModifier；它们供 Buff、Skill 等系统执行数值属性修改之外的游戏流程使用。

`FTcsAttributeInstance`、Clamp、快照、事件、查询与 DataTable 将继续以 `float` 为唯一 Attribute 数值类型，不为 Bool / Vector 增加类型化分支。

### 2.2 SourceHandle 的工厂归属与有效性（已确认）

SourceHandle 的唯一创建入口是无状态静态 `FTcsSourceHandleFactory`，不归属 State、Attribute 或 Definition 模块。`UTcsGenericLibrary` 只提供其 Blueprint 转发入口；不新建 SourceHandle Subsystem，也不将职责加入 `UTcsDefinitionManagerSubsystem`。

工厂只负责分配进程内唯一 Id 与构造因果链，提供根来源与子来源两类 API。子来源由工厂继承父链并追加直接因果来源的 Definition Id，调用方不得手工拼接 CausalityChain。

`Id > -1` 是唯一有效性标准，首个生成的 Id `0` 有效；所有调用点和规范必须使用 `SourceHandle.IsValid()`，不得手写 `Id > 0`。`Instigator`、`SourceTags` 与因果链内容保持可选。

SourceHandle 不建立全局运行时对象解析或注册表。Handle 已承载 Id、Instigator、SourceTags 与 CausalityChain；State、Skill、Buff、伤害等具体来源对象的保存和查询由所属领域模块承担。

### 2.3 Buff / Skill 的 Ongoing 施加边界（已确认）

单个 BuffInstance 在其生命周期内可以施加多个不同的 Ongoing AttributeModifier，但同一个 `ModifierDefId` 最多施加一次。再次施加相同 Definition 必须硬拒绝、零修改，并在 Development / Editor 输出包含 BuffInstance、SourceHandle 与 ModifierDefId 的 Warning。

SkillInstance 不得直接施加 Ongoing AttributeModifier；它可以直接施加 Instant AttributeModifier，或通过施加 Buff 使 BuffInstance 持有 Ongoing AttributeModifier。违反该规则必须硬拒绝、零修改，并在 Development / Editor 输出 Warning。

### 2.4 非 Buff Ongoing 来源的生命周期契约（已确认）

装备、天赋、场景光环等持续效果允许直接持有并施加 Ongoing AttributeModifier。来源自行创建唯一 SourceHandle，Apply 后在自身生命周期结束时显式调用 `RemoveOngoingModifiersBySourceHandle(SourceHandle)` 清理；TCS 不解析、注册或观察业务来源对象。

非 Buff 来源对同一 `ModifierDefId` 的重复 Ongoing Apply、更新或并存语义尚未确认，保留到后续讨论。

### 2.5 跨 Actor 的 Instant / Ongoing 施加路径（已确认）

Ongoing AttributeModifierInstance 严格禁止直接跨 Actor 施加。跨 Actor 持续效果必须先在目标 Actor 上创建 StateInstance，首版以 BuffInstance 作为该目标本地生命周期宿主；由该 BuffInstance 在目标 AttributeComponent 上施加和结束 Ongoing。

Instant AttributeModifierInstance 允许直接作用于 TargetAttributeComponent，供后续独立伤害模块等一次性跨 Actor 结算使用；它仍必须携带有效 SourceHandle，但不依赖目标 State 或 BuffInstance。

删除现有 `TcsSTTask_ApplyAttributeModifierToTarget`，不得将其限制为 Instant 或保留为跨 Actor Ongoing 的兼容入口。

### 2.6 资源属性与临时容量变化（已确认）

资源属性始终按当前有效最大值 Clamp。临时 Max 类 Ongoing 贡献移除并降低有效上限时，AttributeRange 必须将资源的 BaseValue 与 CurrentValue 一并截断至新上限；超出部分永久丢失，不保存隐藏溢出，也不在容量恢复时返还。

### 2.7 Clamp 的 Base / Current 层契约（已确认）

BaseValue 与 CurrentValue 使用同一个 AttributeRange 和同一 ClampStrategy。所有动态 Min / Max 均在同一 AttributeComponent 内读取依赖 Attribute 的 CurrentValue；本次不向 ClampContext 引入 ValueLayer，也不支持按数值层配置不同的范围策略。

### 2.8 Ongoing 动态 Operand 的依赖链惰性重算（延期）

依赖链自动标脏、延迟 Flush、循环检测与 StateParam 失效通知不进入当前 AttributeModifier 重构，将以独立 proposal 处理，详见：[设计：OngoingAttrMod依赖链惰性重算（后续提案）.md](设计：OngoingAttrMod依赖链惰性重算（后续提案）.md)。

当前重构维持既有拉取式惰性语义：Ongoing 仅在 AttributeComponent 已有的受控重算入口执行时读取最新动态 Operand；不承诺 StateParam、Attribute 或其他依赖变化会自动触发重算，也不引入全帧 Tick 或跨 Actor 轮询作为过渡方案。

### 2.9 EvaluatorContext 与旧 OperandBinding 迁移（已确认）

AttributeModifier Operand 固定为 `float`。每轮动态构建的只读 `FTcsAttributeOperandEvaluatorContext` 必须包含 TargetAttributeComponent / Target、从 SourceHandle 派生的 Instigator、SourceHandle、可空的 SourceStateInstance、可空的 SourceSkillEntry，以及本轮构建的 `const AttributeEvaluationSnapshot&`；Snapshot 不得持久化到 ModifierInstance。

StateTree Task 不向编辑器暴露来源对象。Task 从 `TcsSTSchema_Buff` / `TcsSTSchema_Skill` 的根运行时 Context 读取 SourceStateInstance；若它是 SkillInstance，则通过 `GetSkillEntry()` 派生 SourceSkillEntry。伤害模块直接调用时 SourceStateInstance / SourceSkillEntry 可以为空；任何实际需要缺失来源对象的 Evaluator 必须使本次 Application 原子失败，不得通过 SourceHandle 隐式反查或回退默认值。

删除旧 `TArray<FTcsStateParamBinding>`。StateParam Operand 改用 `FTcsStateParamOperandPayload` 与专用 Numeric Evaluator：Payload 指定 StateParamTag 和来源类型，Evaluator 从 Context 读取对应的 Numeric StateParam 无参 `GetModifiedValue()`。缺来源、缺 Param 或类型不匹配时，Application 原子失败。

### 2.10 通用 AttributeEvaluationSnapshot 与业务贡献语义（已确认）

Attribute Core 提供仅限 TargetAttributeComponent 的通用只读 Snapshot，Evaluator 可以查询其中任意 Attribute 的 BaseValue / CurrentValue；不支持经 Snapshot 隐式读取 Instigator、其他 Actor 或其他 Component 的 Attribute。

Ongoing Evaluator 的 Snapshot 由 TCS 自动排除当前父 Ongoing ModifierInstance 的全部旧结果：初次 Apply 时该父实例尚未进入已应用记录；后续重算时 TCS 虚拟移除该父实例，按其余原始 Ongoing 实例、Merger 与稳定排序重建 Snapshot，且该父实例的全部 Operation 共用这份 Snapshot。此行为不是编辑器配置，Evaluator 不得指定排除其他父实例，也不得通过 `CurrentValue - 自身贡献` 反推排除结果。

Attribute Core 不提供 `IntrinsicValue`、`BonusHealth`、ContributionCategory、分类贡献小计或其他业务归因语义。项目需要这类语义时，必须显式维护对应业务 Attribute 或由业务模块 / Custom Evaluator 计算；TCS 不从 BaseValue、CurrentValue 或 Ongoing 记录推导。

### 2.11 RemoveAttribute 与多 Operation Ongoing 的关系（已确认）

只要任意已保存 Ongoing Operation 将某 Attribute 作为 TargetAttributeId，`RemoveAttribute` 必须硬拒绝且不修改 Attribute、Ongoing 实例记录、索引或事件。调用方必须先结束相关来源并完成 Ongoing 清理，再移除该 Attribute；不得部分删除单条 Operation，也不得删除整个父 ModifierInstance 作为隐式补救。

### 2.12 Instant / Ongoing 的事件与审计（已确认）

Instant 不产生 AttributeModifier Added / Updated / Removed 事件；成功结算仍产生 Attribute BaseValue Change 事件。`FTcsAttributeModifierApplicationResult` 必须承载逐 Operation 的成功状态、TargetAttributeId、OldValue、NewValue、SourceHandle、OperationId 与失败原因，供调用方处理伤害统计、战斗记录与诊断。

Ongoing 的 Attribute Change Event 只报告 Clamp 和范围传播完成后的最终稳定态，不广播直接结算或中间传播状态。精确 Operation 归因由 Ongoing 已应用 Operation 记录提供，不能继续依赖旧的 `TMap<SourceHandle, float>` ChangeSourceRecord。

### 2.13 单 Operation Ongoing 的内建 Merger 输入语义（已确认）

**已确认**：内建 Merger 一律在 Operator 之前处理 EvaluatedOperand，不依据最终贡献。最终贡献会受目标当前值、执行顺序、Clamp 和 Custom Operator 影响，不能作为稳定的合并输入。

GAS 的持续 Attribute Aggregator 不提供独立的 `UseHighest` / `UseLowest` / `UseAdditiveSum` Merger；它按 Attribute、EvaluationChannel、ModOp 分桶，再对同 Op 使用固定归约：Additive 求和，MultiplyAdditive / DivideAdditive 围绕单位元 `1` 做偏置求和，MultiplyCompound 连乘，Override 取当前数组中第一个合格项。这个设计说明不同 Operator 的 Operand 并不天然处于同一比较空间，最高 / 最低 / 求和这类 Merger 必须先限定可解释的 Operator 组合。

推荐兼容表如下。命名上，当前代码已有 `UseMaximum` / `UseMinimum`，它们对应设计讨论中的 `UseHighest` / `UseLowest`：

| Merger | 推荐 Operator | 说明与限制 | GAS 启发 |
| --- | --- | --- | --- |
| `NoMerge` | 全部内建 Operator；Custom Operator | 默认行为，保留所有实例并按 Attribute 聚合顺序逐个参与计算。 | GAS 对持续 Modifier 的常规路径也是保留多个合格 Modifier，再按 Op 分桶聚合。 |
| `UseNewest` | 全部内建 Operator；Custom Operator | 按稳定的应用顺序选择最新实例，只保留一个 EvaluatedOperand。 | 比 GAS Override 的数组顺序更显式，适合“后施加覆盖先施加”的业务语义。 |
| `UseOldest` | 全部内建 Operator；Custom Operator | 按稳定的应用顺序选择最早实例，只保留一个 EvaluatedOperand。 | 与 `UseNewest` 对称，适合“首次施加锁定”的业务语义。 |
| `UseMaximum` | `Add`、`MultiplyAdditive`、`MultiplyCompound` | 比较 EvaluatedOperand 本身，选择最大 Operand；不比较最终 Attribute 贡献。 | GAS 不做跨 Op 最高值选择；仅在同一可解释数值空间内比较才稳定。 |
| `UseMinimum` | `Add`、`MultiplyAdditive`、`MultiplyCompound` | 比较 EvaluatedOperand 本身，选择最小 Operand；不比较最终 Attribute 贡献。 | 与 `UseMaximum` 对称，限定同一可解释数值空间。 |
| `UseAdditiveSum` | `Add`；项目可通过设置显式允许 `MultiplyAdditive` delta 加和 | 将多个 EvaluatedOperand 加和成一个 Operand；默认不用于 `Override` 或 `MultiplyCompound`。 | GAS 的 Additive 求和稳定；MultiplyAdditive 是围绕 `1` 的偏置求和，不是直接乘法。 |
| Custom Merger | 由 `TcsDeveloperSettings` 显式声明兼容 Operator | Custom Merger 必须明确自己的输入空间、排序、失败策略和可用 Operator。 | GAS 的自定义扩展更接近 qualifier / execution，不提供通用跨 Op Merger。 |

`Override` 不应与 `UseMaximum` / `UseMinimum` / `UseAdditiveSum` 默认兼容。Override 表达“唯一值覆盖”；多个 Override 候选如果需要最新或最旧，应使用 `UseNewest` / `UseOldest`。若项目确实需要“最高 Override 值”或“最低 Override 值”，应通过 `TcsDeveloperSettings` 显式放开 `Override + UseMaximum / UseMinimum` 并至少给出 Warning 级说明，不能把 Override 的最终结果与其他 Operator 的贡献混合比较。

`MultiplyAdditive` 与 `UseAdditiveSum` 的组合必须遵守 TCS 的 delta Operand 语义，而不是直接相乘。例如多个 Operand `0.2` 与 `0.3` 聚合为 `0.5`，最终由 Operator 计算为 `Current * (1 + 0.5)`；不是把 `1.2` 与 `1.3` 直接相加，也不是把它们复合为 `1.56`。如果实现阶段不想引入该组合，首版应只允许 `UseAdditiveSum + Add`。

#### 2.13.1 Operator / Merger 编辑器验证设计（已确认方向）

在 `TcsDeveloperSettings` 中增加 AttributeModifier Operator 与 Merger 的兼容性配置，用于驱动 DefinitionAsset 编辑器过滤、Data Validation 与运行时防御。配置需要同时支持内建 Operator 枚举和 Custom Operator 策略类：若最终 Operator 使用枚举，匹配键使用枚举值；若最终 Operator 使用策略类，匹配键使用 Operator Class；若两者并存，配置结构必须能区分内建 Operator 与 Custom Operator。

DefinitionAsset 编辑器需要提供双重验证：

- 先设置 Operator 再设置 Merger 时，Merger 下拉菜单只显示或只允许选择与当前 Operator 兼容的 Merger；若默认 Details 面板无法仅靠元数据完成动态过滤，则通过自定义 Detail Customization / Class Viewer Filter 实现。
- 先设置 Merger 再设置 Operator 时，如果当前 Merger 与新 Operator 不兼容，不自动清空用户已有选择；通过 `PostEditChangeProperty` 输出编辑器告警，并在 `IsDataValid` 中使用现有 DefAsset 验证方式添加 Warning 或 Error。
- 对无法安全执行或语义必然错误的组合使用 Error；对可运行但语义需要设计者显式确认的组合使用 Warning。
- 运行时 Apply / Recalculate 仍必须做兼容性检查；编辑器过滤不能作为唯一防线。

推荐的 `TcsDeveloperSettings` 配置语义：

```text
AttributeModifierOperatorMergerRules
  OperatorKey
    - BuiltInOperator 或 CustomOperatorClass
  AllowedMergerTypes
    - 可选择的 AttributeModifierMerger Class 列表
  ValidationSeverity
    - 不在 AllowedMergerTypes 内时使用 Error 或 Warning
  Description
    - 面向编辑器诊断的说明文本
```

实现时不应把兼容表硬编码在每个 DefinitionAsset 中。DefinitionAsset 只保存自己的 Operator / Evaluator / OperandPayload / Merger 等定义数据；兼容性规则的权威来源是 `TcsDeveloperSettings`。Operator / Merger 类型可以提供默认兼容信息或自检能力，但 DefinitionAsset 编辑器应以 `TcsDeveloperSettings` 查询结果执行过滤和验证。

### 2.14 DataTable / DefinitionAsset 迁移与活动 change 协调（已确认）

AttributeModifier DataTableRow 必须与新的 DefinitionAsset 非标识 UPROPERTY 完全 1:1 对齐，并通过双向直接赋值同步。Row 以 RowName 承载 AttributeModifierDefId，镜像 Priority、Merger、Operation Map 及每条 Operation 的 TargetAttributeId、Operator、Evaluator、OperandPayload、Custom Operator 等新模型字段。

旧 `AttributeId + ModifierMode + Operands + ModifierType + MergerType` Row 结构、旧 DataTable 行和旧 DefinitionAsset 不保留兼容或自动迁移；当前处于开发阶段，相关测试资产可直接重新创建。`FInstancedStruct` 已是现有 DataTable ↔ DefAsset 同步支持的直接映射类型，Operation Map 的编辑与同步只需在实现时补充一次实际编辑器验证。

`add-def-strategy-defaults-and-validation` 与本重构修改同一 AttributeModifier Definition / Row 同步面。本重构 proposal 必须在该 change 完成并归档后创建，避免旧默认化逻辑或同步映射回写已删除字段。

## 3. 可在 proposal 内细化

这些事项已有明确方向，但需要在 proposal 的 `design.md` 和 delta spec 中写成精确算法、接口和验收场景：

- OperationId 的命名限制、字典序排序比较器、调试输出排序和同 Priority Modifier 的 tie-breaker。
- OperandPayload 基类、Numeric Evaluator 返回值、Custom Operator CDO 接口与结构化错误结果。
- Editor Error / Warning 的具体分界、Development 与 Shipping 的诊断策略。
- ApplicationResult 的错误码、逐 Operation 成功记录与 Blueprint 表达。
- Ongoing 内建 Merger 的单 Operation 比较规则和 Custom Merger 输入结构。
- Operation Map、FInstancedStruct 与 DataTable 的精确字段映射，以及旧结构不迁移时的拒绝 / 重建说明。
- 自动化测试布局和具体断言。

## 4. 实施时必须覆盖的回归场景

以下不是当前待定项，但 proposal 的 tasks 与 specs 应明确覆盖：

- Instant 原子提交、来源结束后不回滚、无效 SourceHandle 零修改。
- Ongoing 添加、Buff 同 Definition 重复施加硬拒绝、来源结束回滚、无效 SourceHandle 拒绝；非 Buff 同 Definition 重复语义需在确认后补充覆盖。
- 多 Operation 任一 Evaluator / Operator 失败时零提交。
- 多 Operation 按 OperationId 稳定顺序，且不依赖 TMap 遍历。
- Ongoing 自引用公式排除自身旧贡献，避免反馈循环。
- 动态 Operand 的拉取时机、来源 State / Skill 上下文失效行为。
- Base / Current Clamp、MaxHealth 临时变化对 CurrentHealth 的影响。
- RemoveAttribute 被 Ongoing Operation 引用时的既定行为。
- Ongoing 跨 Actor 禁止或目标本地 State 路径；Instant 跨 Actor 的既定行为。
- DataTable 旧行拒绝 / 重建说明、新结构双向同步与编辑器 Data Validation。
- Buff Period 首跳、长帧、到期同帧与 StateTree reselect 的 Instant 执行次数。

## 5. 关联活动变更

当前活动 OpenSpec change：

- `add-def-strategy-defaults-and-validation`：修改 AttributeModifier Definition 的策略默认值与 DataTable 同步面。与本重构直接重叠，应先协调其完成、归档或范围调整。
- `add-skill-modifier-runtime-management`：已将 effective StateParam 消费、Attribute OperandBinding 读取纳入范围。新 Attribute Evaluator / OperandContext 必须与其无参 `GetModifiedValue()` 契约一致，但不应把 Attribute 重构塞入该 change。
- `add-component-runtime-bootstrap`：与 AttributeModifier 数据模型不直接重叠，但实施期间应避免破坏 Component runtime ready / bootstrap 契约。

## 6. 当前结论

在第 2 节的事项全部确认前，不创建 OpenSpec change。确认后，新 change 应独立覆盖 AttributeModifier 运行时、Attribute 管理 API、Definition / DataTable authoring、StateTree Task 迁移与相关规格；伤害系统本身仍作为后续独立模块和独立 change。
