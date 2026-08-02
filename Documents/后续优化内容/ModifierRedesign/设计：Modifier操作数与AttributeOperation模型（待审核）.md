# 设计：Modifier 操作数与 AttributeOperation 模型（待审核）

> 状态：待审核，尚未创建 OpenSpec change，尚未实现。

## 1. 统一模型

Modifier 将“计算操作数”和“施加运算”分离：

```text
OperandPayload -- OperandEvaluator --> EvaluatedOperand -- Operator --> NewValue
```

| 概念 | 职责 |
|---|---|
| OperandPayload | Evaluator 的创作输入，例如常量、随机范围、结构化公式、条件。 |
| OperandEvaluator | 根据 Payload 和只读上下文计算 typed Operand。 |
| Operator | 将 EvaluatedOperand 施加到当前目标值。 |
| FTcsAttributeOperationDefinition | 编辑器中配置的一条 Attribute 修改模板。 |
| FTcsEvaluatedAttributeOperation | 本轮计算完成、尚未写入 Attribute 的运行时操作。 |

`FInstancedStruct` 只承载 OperandPayload。它不属于最终 Operator 配置，也不应在 Skill StateParam 的每次 effective 读取时被解析。

## 2. Operator 与 Operand

Numeric Operator：

```text
Custom, Add, MultiplyAdditive, MultiplyCompound, Override
```

```text
Add                Current + Operand
MultiplyAdditive   Current * (1 + Operand)
MultiplyCompound   Current * Operand
Override           Operand
```

Bool Operator（SkillModifier / Buff 流程专用，不属于 AttributeModifier）：

```text
Custom, Override
```

Vector Operator（SkillModifier / Buff 流程专用，不属于 AttributeModifier）：

```text
Custom, Override, Add, MultiplyAdditive, MultiplyCompound
```

Vector `MultiplyAdditive` 和 `MultiplyCompound` 支持 Definition 固定的 typed 分量掩码：`X`、`Y`、`Z`、`XY`、`XZ`、`YZ`、`XYZ`。选中分量参与乘算；未选中分量使用乘法单位元 `1`，保持原值。掩码与 Operator 一样不可由 Application 覆写；Custom Vector Operator 的分量语义由其 CDO 定义。

AttributeModifier 的 Evaluator 与 Operand 固定为 Numeric / `float`。Bool / Vector Evaluator 与 Operator 仅供 Buff、Skill 等系统执行数值属性修改之外的游戏流程，不进入 AttributeModifier、AttributeInstance、Clamp、快照、事件或 DataTable 模型。Payload 必须继承共同 Payload 基类，Evaluator 必须校验具体 Payload 类型。首版使用结构化 Formula Payload 或专用 Evaluator CDO，不引入运行时自由文本公式解析器。

每轮动态构建的只读 `FTcsAttributeOperandEvaluatorContext` 必须包含 TargetAttributeComponent / Target、从 SourceHandle 派生的 Instigator、SourceHandle、可空的 SourceStateInstance、可空的 SourceSkillEntry，以及本轮构建的 `const AttributeEvaluationSnapshot&`；Snapshot 不得持久化到 ModifierInstance。StateTree Task 不向编辑器暴露来源对象，而是从 `TcsSTSchema_Buff` / `TcsSTSchema_Skill` 的根运行时 Context 读取 SourceStateInstance；SkillInstance 再通过 `GetSkillEntry()` 派生 SourceSkillEntry。伤害模块直接调用时两个来源对象可为空；Evaluator 实际需要但未取得来源对象、StateParam 或正确类型时，整次 Application 原子失败，不得通过 SourceHandle 隐式反查或回退默认值。

删除旧 `TArray<FTcsStateParamBinding>`。StateParam Operand 使用 `FTcsStateParamOperandPayload` 与专用 Numeric Evaluator：Payload 指定 StateParamTag 和来源类型，Evaluator 从 Context 读取对应 Numeric StateParam 的无参 `GetModifiedValue()`。

Definition 默认提供 Evaluator 与 Payload；Application 只允许按目标 Modifier 或 Operation 覆写：

- `OperandEvaluatorClass`
- `OperandPayload`

不得覆写 Operator、目标、Priority、Merger / MergePolicy 或选择器。覆写后重新校验 Evaluator、Payload 和目标类型；不匹配时 apply 原子失败。

## 3. SkillModifier

SkillModifier 保持“一个 Def 修改一个 StateParam”：

```text
SkillModifierDefinition
  SkillModifierDefId
  EntrySelectorClass / EntrySelectorConfig
  TargetParamTag / TargetParamType
  Typed Operator / CustomOperator / VectorMask
  Typed OperandEvaluator / OperandPayload
  Priority / MergePolicy
```

`EntrySelector` 继续决定命中的 `0..N` 个 SkillEntry；本设计只重构命中后如何计算 Operand 并修改目标参数。

SkillModifier 在 apply 时解析一次 Operand，保存到 RuntimeEntry 与 ParamModifierInstance。后续 `GetModifiedValue()` 仅执行 Operator；随机不重掷，公式不重算。来源数据变化时使用 remove + reapply。

## 4. AttributeModifier

`UTcsAttributeModifierDefinition` 使用稳定 Id 的 Operation Map：

```text
AttributeModifierDefId                // 定义唯一 Id。
MergerType                            // 多个同 Def 实例的合并策略。
Operations: TMap<FName, FTcsAttributeOperationDefinition>
  Key: OperationId                    // Definition 内唯一，亦是 Override 的索引键。
  TargetAttributeId                   // 最终修改的 Attribute。
  Operator / CustomOperatorClass      // 用 EvaluatedOperand 修改目标值的方式。
  OperandEvaluatorClass               // 根据 Payload / 快照计算 Operand。
  OperandPayload                      // Evaluator 的默认创作输入。
```

`FTcsAttributeOperationDefinition` 不暴露 `ModifierMode`、`AMM_BaseValue`、`AMM_CurrentValue` 或等价的写入层配置。Operation Definition 只描述“计算什么 Operand、对哪个 Attribute 施加什么 Operator”。调用方通过唯一的 `ApplyAttributeModifier` 入口和 ApplicationMode 决定结算路径：

```text
ApplyAttributeModifier(ApplicationMode = Instant)  一次性结算，原子写入目标 Attribute 的 BaseValue。
ApplyAttributeModifier(ApplicationMode = Ongoing)  创建或更新可撤销的持续实例，参与目标 Attribute 的 CurrentValue 聚合。
```

```cpp
enum class ETcsAttributeModifierApplicationMode : uint8
{
    None,       // 无效；调用方必须显式选择。
    Instant,    // 一次性结算。
    Ongoing,    // 来源存续期间持续生效。
};

struct FTcsAttributeModifierApplicationRequest
{
    FName ModifierDefId;
    ETcsAttributeModifierApplicationMode ApplicationMode;
    FTcsSourceHandle SourceHandle;
    TMap<FName, FTcsOperandOverride> OperationOverrides;
};

bool ApplyAttributeModifier(
    const FTcsAttributeModifierApplicationRequest& Request,
    FTcsAttributeModifierApplicationResult& OutResult);
```

所有有效的 AttributeModifierInstance 都必须持有有效 `SourceHandle`，包括 `Instant`。缺少或无效 Handle 的请求必须在计算前失败，不产生任何数值、事件或统计结果。`Instant` 使用 Handle 进行有效性验证、事件归因、战斗记录与统计溯源，但不将实例保存为可撤销持续贡献；`Ongoing` 额外使用同一 Handle 作为持续实例的生命周期锚点。

`FTcsSourceHandleFactory` 是 SourceHandle 的唯一创建入口。它是无状态静态工厂，负责分配进程内唯一 Id 与构造因果链；`UTcsGenericLibrary` 只提供 Blueprint 转发，不拥有工厂状态。工厂提供根来源和子来源两类创建 API；子来源必须由工厂继承父链并追加直接因果来源的 Definition Id，调用方不得手工拼接 CausalityChain。SourceHandle 不建立全局运行时对象解析或注册表，State、Skill、Buff、伤害等具体来源对象的保存和查询由所属领域模块承担。

`SourceHandle.IsValid()` 等价于 `Id > -1`，首个生成的 Id `0` 有效；所有调用点必须使用该谓词，禁止手写 `Id > 0`。`Instigator`、`SourceTags` 与因果链内容保持可选。

同一次 `ApplyAttributeModifier` 调用中的全部 Operation 必须使用同一个 `ApplicationMode`；不允许一个多 Operation Modifier 同时混合 BaseValue 写入和 CurrentValue 聚合。Buff 的 `DurationType` 或 `Period` 不直接替 Operation 决定模式：调用它的技能、StateTree Task 或 Buff 周期触发在 Request 中显式指定 `Instant` 或 `Ongoing`。

Ongoing 严格禁止直接跨 Actor 施加。跨 Actor 持续效果必须先在目标 Actor 上创建目标本地 StateInstance，首版以 BuffInstance 作为生命周期宿主，再由该 BuffInstance 在目标 AttributeComponent 上施加和结束 Ongoing。Instant 允许直接作用于 TargetAttributeComponent，供后续伤害模块等一次性跨 Actor 结算使用；它仍必须持有有效 SourceHandle。现有 `TcsSTTask_ApplyAttributeModifierToTarget` 删除，不保留为 Instant 限制入口或 Ongoing 兼容入口。

Application 以 `OperationId` 覆写指定 Operation 的 Evaluator 或 Payload：

```text
AttributeModifierApplication
  ModifierDefId
  OperationOverrides: TMap<FName, OperandOverride>
    Key: OperationId
    Evaluator override
    Payload override
```

`TMap` 是权威存储与按 Id 查询结构；它不保证遍历顺序。任何影响结算、调试或网络确定性的遍历，都必须先按 `OperationId` 稳定排序。

`FTcsAttributeModifierDefRow` 必须与新的 AttributeModifier Definition 非标识 UPROPERTY 完全 1:1 对齐，并通过双向直接赋值同步；RowName 承载 AttributeModifierDefId。Operation Map 与嵌套 OperandPayload 使用现有 `FInstancedStruct` DataTable 编辑和深拷贝能力，不为旧单 Operation Row、旧 DataTable 行或旧 DefinitionAsset 保留兼容、自动迁移或转换工具。当前处于开发阶段，相关资产直接重新创建。

运行时解析结构：

```text
FTcsEvaluatedAttributeOperation
  OperationId
  TargetAttributeId
  Operator / CustomOperator
  EvaluatedOperand
  SourceHandle
  ParentModifierInstanceId
```

`FTcsEvaluatedAttributeOperation` 不携带写入层；它由本次结算入口提供上下文。`UTcsAttributeModifierExecution`、`ModifierType`、`ExecutionClass` 不再作为新模型的一部分。AttributeComponent 是唯一计算、合并、排序、写入和移除 Operation 结果的宿主。

## 5. Attribute 结算契约

每次 AttributeModifier 结算的所有 Operation 都必须从**同一份不可变 `AttributeEvaluationSnapshot`**解析。任一 Operation 解析或应用失败，则本次结算不产生任何已应用结果，不允许部分写入。

`ApplyAttributeModifier` 的 ApplicationMode 职责边界固定如下：

| ApplicationMode | 适用场景 | 数值结果 | 生命周期 |
|---|---|---|---|
| `Instant` | 瞬时效果、命中结算、伤害、治疗、周期 DoT / HoT | 对目标 Attribute 的 `BaseValue` 执行一次原子修改 | 必须带有效 `SourceHandle`；不保存到持续 Modifier 索引；历史结果不因来源移除而回滚。 |
| `Ongoing` | 无周期的持续 Buff、装备、光环、天赋 | 作为可撤销贡献参与目标 Attribute 的 `CurrentValue` 聚合 | 必须带有效 `SourceHandle`；按 Handle 保存，来源通过显式入口移除、失效或更新时重算。 |

`Instant` 与 `Ongoing` 描述效果生命周期，不是 Definition 中可配置的 `ExecutionClass`。`Ongoing` 不代表直接覆写 AttributeInstance 的 `CurrentValue` 字段。单个 BuffInstance 在其生命周期内可施加多个不同 Definition 的 Ongoing AttributeModifier，但同一 `ModifierDefId` 最多一次；重复请求必须硬拒绝、零修改并在 Development / Editor 输出 Warning。SkillInstance 不得直接施加 Ongoing AttributeModifier，只能施加 Instant，或通过 BuffInstance 间接产生 Ongoing。装备、天赋、场景光环等非 Buff 来源可以直接施加 Ongoing：它们必须自行创建唯一 SourceHandle，并在自身生命周期结束时显式调用 `RemoveOngoingModifiersBySourceHandle(SourceHandle)`；TCS 不解析、注册或观察业务来源对象。非 Buff 来源对同一 `ModifierDefId` 的重复 Ongoing Apply、更新或并存语义仍需在创建 proposal 前确认。

Instant 不产生 AttributeModifier Added / Updated / Removed 事件；成功结算仍产生 Attribute BaseValue Change 事件。`FTcsAttributeModifierApplicationResult` 必须承载逐 Operation 的成功状态、TargetAttributeId、OldValue、NewValue、SourceHandle、OperationId 与失败原因，供调用方处理伤害统计、战斗记录与诊断。Ongoing 的 Attribute Change Event 只报告 Clamp 和范围传播完成后的最终稳定态，不广播直接结算或中间传播状态；精确 Operation 归因由 Ongoing 已应用 Operation 记录提供，不继续依赖旧的 `TMap<SourceHandle, float>` ChangeSourceRecord。

```text
共同阶段
1. 构造本轮不可变 AttributeEvaluationSnapshot。
2. 按 OperationId 稳定顺序计算每个 Operand；全部成功才得到完整 EvaluatedOperation 集。

ApplicationMode = Instant
3. 在候选 BaseValues 中按稳定顺序应用 EvaluatedOperation 集。
4. 全部应用成功才提交候选 BaseValue；随后 Clamp、范围传播、事件广播。

ApplicationMode = Ongoing
3. 保存或更新完整 ModifierInstance，并由 Merger 处理完整、已解析的持续实例。
4. 在候选 CurrentValue 聚合结果中按稳定顺序应用 EvaluatedOperation 集。
5. 全部应用成功才提交候选 CurrentValue；随后 Clamp、范围传播、事件广播。
```

`Instant` 在事件发生时计算一次 Operand 并立即提交。`Ongoing` 的动态 Operand 必须在每轮聚合、Merger 前计算；`UseMaximum`、`UseMinimum`、`UseAdditiveSum` 等 Merger 只能读取本轮 `EvaluatedOperand`，不能读取 Definition 默认值或上一轮缓存值。`UseMaximum` / `UseMinimum` 对应设计讨论中的 `UseHighest` / `UseLowest`。

只要存在动态 Operand，就不能复用带数值的 `CachedMergedModifiers`；第一版每轮重建“解析 -> Merge -> 排序”的临时结果。后续只能缓存静态元数据、分组或排序计划。

编辑器验证静态错误：空 / 重复 OperationId、无效 TargetAttributeId、缺失或 abstract Evaluator、Payload 类型不匹配、缺失 Custom Operator、非法 Override Key。运行时仍须安全拒绝目标 Attribute 不存在、来源上下文失效、缺失动态输入、NaN / Inf 等错误，并在 Development / Editor 输出包含 DefId、ModifierInstanceId、OperationId 的诊断；Shipping 不崩溃且不留下部分结果。

## 6. Ongoing Merger 边界

Merger 只适用于 `ApplicationMode = Ongoing` 的持续实例；`Instant` 没有待保存、待移除或待合并的实例。内建 Merger 一律在 Operator 之前处理 EvaluatedOperand，不依据最终贡献；最终贡献会受目标当前值、执行顺序、Clamp 和 Custom Operator 影响，不能作为稳定的合并输入。TCS 不猜测多 Operation 持续 Modifier 的合并语义。

GAS 的持续 Attribute Aggregator 不提供独立的 `UseHighest` / `UseLowest` / `UseAdditiveSum` Merger；它按 Attribute、EvaluationChannel、ModOp 分桶，再对同 Op 使用固定归约：Additive 求和，MultiplyAdditive / DivideAdditive 围绕单位元 `1` 做偏置求和，MultiplyCompound 连乘，Override 取当前数组中第一个合格项。这个设计说明不同 Operator 的 Operand 并不天然处于同一比较空间，最高 / 最低 / 求和这类 Merger 必须先限定可解释的 Operator 组合。

单 Operation Ongoing 的推荐兼容表如下。当前代码已有 `UseMaximum` / `UseMinimum`，它们对应设计讨论中的 `UseHighest` / `UseLowest`：

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

```text
Operations.Num() == 1
  内建 Merger 可正常使用。

Operations.Num() > 1
  NoMerge 始终合法。
  内建选择 / 聚合 Merger 不自动逐 Operation 重组，也不猜测整组选择。
  若使用该类 Merger，Data Validation 报错或强警告；开发者应改用 NoMerge，
  或提供具有明确整体语义的 Custom Merger。
```

TCS 负责完整输入、原子过程、稳定排序和失败安全；使用 TCS 的开发者负责为多 Operation Modifier 选择正确的 Merger 语义。

`TcsDeveloperSettings` 需要提供 AttributeModifier Operator 与 Merger 的兼容性配置，用于驱动 DefinitionAsset 编辑器过滤、Data Validation 与运行时防御。配置需要同时支持内建 Operator 枚举和 Custom Operator 策略类：若最终 Operator 使用枚举，匹配键使用枚举值；若最终 Operator 使用策略类，匹配键使用 Operator Class；若两者并存，配置结构必须能区分内建 Operator 与 Custom Operator。

DefinitionAsset 编辑器需要提供双重验证：先设置 Operator 再设置 Merger 时，Merger 下拉菜单只显示或只允许选择与当前 Operator 兼容的 Merger；若默认 Details 面板无法仅靠元数据完成动态过滤，则通过自定义 Detail Customization / Class Viewer Filter 实现。先设置 Merger 再设置 Operator 时，如果当前 Merger 与新 Operator 不兼容，不自动清空用户已有选择；通过 `PostEditChangeProperty` 输出编辑器告警，并在 `IsDataValid` 中使用现有 DefAsset 验证方式添加 Warning 或 Error。对无法安全执行或语义必然错误的组合使用 Error；对可运行但语义需要设计者显式确认的组合使用 Warning。运行时 Apply / Recalculate 仍必须做兼容性检查，编辑器过滤不能作为唯一防线。

推荐的设置语义：

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

兼容表不硬编码在每个 DefinitionAsset 中。DefinitionAsset 只保存自己的 Operator / Evaluator / OperandPayload / Merger 等定义数据；兼容性规则的权威来源是 `TcsDeveloperSettings`。Operator / Merger 类型可以提供默认兼容信息或自检能力，但 DefinitionAsset 编辑器应以 `TcsDeveloperSettings` 查询结果执行过滤和验证。

## 7. 血色契约业务层示例

血色契约不是 Attribute Core 的内置能力。下例仅说明业务模块在显式维护 `BonusHealth` 业务 Attribute 或提供等价 Custom Evaluator 后，可如何创建无周期持续 `ApplyAttributeModifier(ApplicationMode = Ongoing)`；Buff 只管理施加和移除：

```text
BloodPact.Operations

HealthToAbilityPower
  Target: AbilityPower
  Operator: Add
  Evaluator: Formula
  Payload: BonusHealth / 30

AbilityPowerToHealth
  Target: MaxHealth
  Operator: Add
  Evaluator: Formula
  Payload: AbilityPower * 1.6
```

两条 Operation 读取同一份排除该父 Ongoing ModifierInstance 旧结果的通用 Snapshot：

```text
H0 = 业务层维护的 BonusHealth
A0 = Snapshot 中排除 BloodPact 后的 AbilityPower

AbilityPowerBonus = H0 / 30
MaxHealthBonus    = A0 * 1.6
```

全部成功才原子应用，因此不形成 `AP -> Health -> AP` 反馈，也不依赖 Operation Map 的遍历顺序。该 Modifier 应使用 `NoMerge`。TCS 不从 BaseValue、CurrentValue 或 Ongoing 记录推导 BonusHealth；没有业务 Attribute 或 Custom Evaluator 时，该示例不能由 Attribute Core 单独实现。

## 8. AttributeInstance 初始化与重置边界

`FTcsAttributeInstance` 不保留 `InitValue`、`InitialValue`、`InitialBaseValue` 或其他创建时数值基线。它的数值状态只包含：

```text
BaseValue     // 基础值层的已提交结果；业务层数据、等级、存档与 Instant 结算写入此层。
CurrentValue  // 由 BaseValue 与全部 Ongoing 贡献聚合得到的当前有效结果。
```

`AddAttribute(AttributeDefId)` 和 `AddAttributeByTag(AttributeTag)` 只负责创建、注册 AttributeInstance 与建立范围依赖，不接收数值参数。创建时的零值只是未配置完成前的内部占位值，不是角色出生值、等级 1 值或任何可恢复的业务基线。

业务层负责从自身的数据表、等级、存档或角色出生配置确定数值：

```text
1. AddAttribute(AttributeDefId)
2. 业务层查询数据表并计算当前应有的 BaseValue
3. SetAttributeBaseValue(AttributeDefId, CalculatedBaseValue)
```

例如，创建角色、读档或等级变化时，业务层分别为 `MaxHealth` 与 `CurrentHealth` 设置各自应有的 BaseValue；若没有 Ongoing 直接作用于 `CurrentHealth`，则其 `CurrentValue` 自然等于 BaseValue。`SetAttributeCurrentValue` 不属于本方案中 Attribute 初始化、伤害、治疗或周期结算的正常入口；是否保留它作为受限的底层校正 API，留待后续独立决策。

资源属性始终按当前有效最大值 Clamp。临时 Max 类 Ongoing 贡献移除并降低有效上限时，AttributeRange 必须将资源的 BaseValue 与 CurrentValue 一并截断至新上限；超出部分永久丢失，不保存隐藏溢出，也不在容量恢复时返还。

BaseValue 与 CurrentValue 使用同一个 AttributeRange 和同一 ClampStrategy。所有动态 Min / Max 均在同一 AttributeComponent 内读取依赖 Attribute 的 CurrentValue；本次不向 ClampContext 引入 ValueLayer，也不支持按数值层配置不同的范围策略。

删除 `ResetAttribute`。TCS 无法定义“重置”应回到等级 1、角色出生配置、最近存档、重生配置还是某张业务数据表的值。尤其在多 Operation Modifier 下，按目标 Attribute 删除整个 Modifier 还会错误移除该 Modifier 对其他 Attribute 的 Operation。重生、回档、等级回退和满资源恢复等行为必须由业务层选择数据来源、Modifier 生命周期与 BaseValue / CurrentValue 写入顺序。

只要任意已保存 Ongoing Operation 将某 Attribute 作为 TargetAttributeId，`RemoveAttribute` 必须硬拒绝且不修改 Attribute、Ongoing 实例记录、索引或事件。调用方必须先结束相关来源并完成 Ongoing 清理，再移除该 Attribute；不得部分删除单条 Operation，也不得删除整个父 ModifierInstance 作为隐式补救。

## 9. 通用 AttributeEvaluationSnapshot

每次 Application 动态构建只读 `AttributeEvaluationSnapshot`，Evaluator 可以查询 TargetAttributeComponent 内任意 Attribute 的 BaseValue / CurrentValue；Snapshot 不支持隐式读取 Instigator、其他 Actor 或其他 Component 的 Attribute。

Ongoing Evaluator 的 Snapshot 自动排除当前父 Ongoing ModifierInstance 的全部旧结果：初次 Apply 时该父实例尚未进入已应用记录；后续重算时 TCS 虚拟移除该父实例，按其余原始 Ongoing 实例、Merger 与稳定排序重建 Snapshot，且该父实例的全部 Operation 共用该 Snapshot。此行为不是编辑器配置，Evaluator 不得指定排除其他父实例，也不得通过 `CurrentValue - 自身贡献` 反推排除结果。

Attribute Core 不提供 `IntrinsicValue`、`BonusHealth`、ContributionCategory、分类贡献小计或其他业务归因语义。项目需要这类语义时，必须显式维护对应业务 Attribute 或由业务模块 / Custom Evaluator 计算；TCS 不从 BaseValue、CurrentValue 或 Ongoing 记录推导。

`Instant` 必须携带有效 SourceHandle 产生审计事件、统计归因与战斗记录，但不形成可撤销贡献记录，也不因来源移除而回滚。

## 10. 后续 TCS 伤害模块边界

`Damage` 不是 Attribute 核心内置的 Meta Attribute。TCS Core 不预设 `Damage`、`Healing`、`CurrentHealth`、`MaxHealth` 等 AttributeId，也不在 AttributeComponent 内硬编码伤害转生命、护盾吸收、暴击或减伤规则。

后续可在 TCS 插件下建立独立伤害模块，由它提供面向业务的 `ApplyDamageToTarget`：

```text
ApplyDamageToTarget
  -> DamageAmountCalculation
     - 读取 Instigator 与 Target 的 Attribute 只读值。
     - 收集 DamageModifier。
     - 计算最终伤害值。
  -> 创建目标为 CurrentHealth 的 AttributeModifierApplicationRequest。
  -> ApplicationMode = Instant，且必须提供伤害来源的有效 SourceHandle。
  -> ApplyAttributeModifier 原子修改 CurrentHealth.BaseValue。
```

这里的 `ApplicationMode = Instant` 表示一次性结算，目标 Attribute 是 `CurrentHealth`，并不表示写入 `CurrentValue`。对没有 Ongoing 直接作用的资源 Attribute，`CurrentHealth.CurrentValue == CurrentHealth.BaseValue`；伤害模块按自身规则处理符号、免伤、护盾、治疗、死亡和范围约束，AttributeComponent 仅负责通用 Operator、原子提交和 Clamp。

实施时新建独立 OpenSpec change；不修改现有 SkillModifier runtime change。
