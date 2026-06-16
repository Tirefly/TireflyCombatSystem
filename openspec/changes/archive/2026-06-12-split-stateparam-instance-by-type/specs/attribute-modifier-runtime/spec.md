# attribute-modifier-runtime Spec Delta

## MODIFIED Requirements

### Requirement: ResolveStateParamInstances

`ResolveStateParamInstances` SHALL 更名为 `ResolveNumericParamInstances`，返回类型从 `TMap<FGameplayTag, FTcsStateParamInstance>*` 改为 `TMap<FGameplayTag, FTcsNumericStateParamInstance>*`。

#### Scenario: 仅返回 Numeric 容器
- **WHEN** 调用 ResolveNumericParamInstances
- **THEN** 返回的 Map 中仅包含 Numeric 类型的 StateParam 实例

#### Scenario: Operand 刷新无需类型判空
- **WHEN** RecalculateAttributeCurrentValues 通过 ResolveNumericParamInstances 获取 ParamInstance
- **THEN** MUST NOT 再检查 CachedEvaluator 是否为 null
