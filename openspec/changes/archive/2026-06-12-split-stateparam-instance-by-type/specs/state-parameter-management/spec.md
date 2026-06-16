# state-parameter-management Spec Delta

## MODIFIED Requirements

### Requirement: StateParamInstance 运行时实例化

系统 SHALL 将 `FTcsStateParamInstance` 拆分为 `FTcsNumericStateParamInstance` / `FTcsBoolStateParamInstance` / `FTcsVectorStateParamInstance` 三个独立 USTRUCT，分别只含对应类型的值字段和 Evaluator CDO。

#### Scenario: Numeric Instance 持有 Evaluator
- **WHEN** 参数类型为 Numeric
- **THEN** 创建 `FTcsNumericStateParamInstance`，包含 CachedEvaluator / NumericValue / bIsSnapshot

#### Scenario: Bool Instance 持有 Evaluator
- **WHEN** 参数类型为 Bool
- **THEN** 创建 `FTcsBoolStateParamInstance`，包含 CachedEvaluator / BoolValue / bIsSnapshot

#### Scenario: Vector Instance 持有 Evaluator
- **WHEN** 参数类型为 Vector
- **THEN** 创建 `FTcsVectorStateParamInstance`，包含 CachedEvaluator / VectorValue / bIsSnapshot

### Requirement: 六容器替换

`UTcsStateInstance` 上统一的 `StateParamInstances` TMap SHALL 拆为 `NumericParamInstances` / `BoolParamInstances` / `VectorParamInstances` 三个独立容器。`UTcsSkillEntry` SHALL 同构。

#### Scenario: Populate 时分桶
- **WHEN** `PopulateStateParamInstances` 遍历 Def->Parameters
- **THEN** Numeric/Bool/Vector 分别写入对应容器

#### Scenario: SkillInstance 指向 Entry 容器
- **WHEN** `UTcsSkillInstance` 调用 `GetNumericParamInstances()`
- **THEN** MUST 返回 `SkillEntry->NumericParamInstances`

### Requirement: virtual GetStateParamInstance

`GetStateParamInstance` SHALL 拆为 `GetNumericParamInstance` / `GetBoolParamInstance` / `GetVectorParamInstance` 三个 virtual 方法。`GetStateParamInstances` SHALL 同步拆分为对应完整表访问器。

#### Scenario: 基类返回本地容器
- **WHEN** 普通 StateInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `this->BoolParamInstances.Find(Tag)`

#### Scenario: SkillInstance 覆写指向 Entry
- **WHEN** SkillInstance 上调用 `GetBoolParamInstance(Tag)`
- **THEN** MUST 返回 `SkillEntry->BoolParamInstances.Find(Tag)`
