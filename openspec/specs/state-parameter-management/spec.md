# state-parameter-management Specification

## Purpose
TBD - created by archiving change add-stateparam-instance-operand-binding. Update Purpose after archive.
## Requirements
### Requirement: StateParamInstance 运行时实例化

`FTcsNumericStateParamInstance` SHALL 新增 `ModifierInstances` (TArray<FStateParamModifierInstance>) 和 `DeriveModifiedValue()` 方法。

#### Scenario: NumericValue 不被 Modifier 改写
- **WHEN** DeriveModifiedValue 被调用
- **THEN** NumericValue MUST 保持求值器产出的原始值不变

#### Scenario: DeriveModifiedValue 沿链求值
- **WHEN** 有多个 bActive==true 的 ModifierInstance
- **THEN** 按 Priority 降序依次调用 Evaluator->Evaluate()，返回最终值

### Requirement: Snapshot 求值策略

`Evaluate()` SHALL 根据 bIsSnapshot 决定求值行为：
- bIsSnapshot == true：首次求值后 bHasEvaluated 为 true，后续调用直接返回缓存
- bIsSnapshot == false：每次调用都重新求值

#### Scenario: Snapshot 参数只求值一次
- **WHEN** bIsSnapshot=true，首次调用 Evaluate
- **THEN** 求值器执行，Value 更新，bHasEvaluated=true
- **WHEN** 再次调用 Evaluate
- **THEN** 直接返回，求值器不再执行

#### Scenario: 非 Snapshot 参数每次求值
- **WHEN** bIsSnapshot=false，每次调用 Evaluate
- **THEN** 每次都重新调求值器，Value 更新

### Requirement: GameplayTag 统一标识

`TcsStateDefinition::Parameters` SHALL 使用 FGameplayTag 作为 Key。原有的 `TagParameters` 字段 SHALL 删除。

#### Scenario: 参数 Key 为 GameplayTag
- **WHEN** 配置 `StateParam.Attack.Power` 参数
- **THEN** Parameters Map 中以该 GameplayTag 为 Key 存储

### Requirement: 六容器替换

`UTcsStateInstance` 上统一的 `StateParamInstances` TMap SHALL 拆为 `NumericParamInstances` / `BoolParamInstances` / `VectorParamInstances` 三个独立容器。`UTcsSkillEntry` SHALL 同构。

#### Scenario: Populate 时分桶
- **WHEN** `PopulateStateParamInstances` 遍历 Def->Parameters
- **THEN** Numeric/Bool/Vector 分别写入对应容器

#### Scenario: SkillInstance 指向 Entry 容器
- **WHEN** `UTcsSkillInstance` 调用 `GetNumericParamInstances()`
- **THEN** MUST 返回 `SkillEntry->NumericParamInstances`

### Requirement: virtual PopulateStateParamInstances

`UTcsStateInstance` SHALL 提供 virtual 方法 PopulateStateParamInstances，替代旧的 UTcsStateComponent::EvaluateAndApplyStateParameters。

#### Scenario: 基类 populate
- **WHEN** 普通 State/Buff 类型调用
- **THEN** 从 StateDef.Parameters 遍历创建并求值 StateParamInstance，填充到 this->StateParamInstances

#### Scenario: SkillInstance 跳过
- **WHEN** UTcsSkillInstance 调用
- **THEN** 空实现（实例由 Entry 持有）

### Requirement: virtual GetStateParamInstance

`GetStateParamInstance` SHALL 拆为 `GetNumericParamInstance` / `GetBoolParamInstance` / `GetVectorParamInstance` 三个 virtual 方法。`GetStateParamInstances` SHALL 同步拆分为对应完整表访问器。

#### Scenario: 基类返回本地容器
- **WHEN** 普通 StateInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `this->BoolParamInstances.Find(Tag)`

#### Scenario: SkillInstance 覆写指向 Entry
- **WHEN** SkillInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `SkillEntry->BoolParamInstances.Find(Tag)`

