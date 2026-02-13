# Spec: 状态参数评估严格检查

## ADDED Requirements

### Requirement: 参数评估失败阻止实例创建

状态实例创建时，如果任何参数评估失败， MUST 阻止实例创建并返回失败。

#### Scenario: 单个参数评估失败

**Given**:
- 状态定义包含参数 `Damage`
- `Damage` 参数评估器返回失败

**When**: 尝试创建状态实例

**Then**:
- 状态实例创建失败
- 返回 `ETcsStateApplyFailReason::CreateInstanceFailed`
- 记录Error级别日志
- 不创建状态实例对象

#### Scenario: 多个参数评估失败

**Given**:
- 状态定义包含参数 `Damage` 和 `Duration`
- 两个参数评估器都返回失败

**When**: 尝试创建状态实例

**Then**:
- 状态实例创建失败
- 日志包含所有失败的参数名称
- 返回失败原因
- 不创建状态实例对象

#### Scenario: 所有参数评估成功

**Given**:
- 状态定义包含多个参数
- 所有参数评估器都返回成功

**When**: 创建状态实例

**Then**:
- 状态实例创建成功
- 参数值被正确设置
- 继续后续创建流程

### Requirement: 参数评估验证阶段

状态实例创建 MUST 包含独立的参数评估验证阶段。

#### Scenario: 验证阶段在实例创建前

**Given**: 调用CreateStateInstance

**When**: 执行创建流程

**Then**:
1. 首先执行参数评估验证阶段
2. 收集所有参数评估结果
3. 如果有失败，立即返回，不创建实例
4. 如果全部成功，继续创建实例

#### Scenario: 验证所有参数类型

**Given**: 状态定义包含不同类型的参数（Numeric、Bool、Vector）

**When**: 执行参数评估验证

**Then**:
- 验证所有Numeric参数
- 验证所有Bool参数
- 验证所有Vector参数
- 任何类型失败都阻止创建

### Requirement: 详细的错误日志

参数评估失败 MUST 记录详细的错误信息。

#### Scenario: 错误日志包含失败参数

**Given**: 参数 `Damage` 和 `Duration` 评估失败

**When**: 记录错误日志

**Then**:
- 日志级别为Error
- 日志包含状态名称
- 日志包含所有失败的参数名称（逗号分隔）
- 日志包含参数类型（Numeric/Bool/Vector）

#### Scenario: 错误日志包含评估器信息

**Given**: 参数评估失败

**When**: 记录错误日志

**Then**:
- 日志包含评估器类名
- 日志包含失败原因（如果评估器提供）
- 便于定位配置问题

### Requirement: 失败原因枚举

 MUST 提供明确的失败原因枚举值。

#### Scenario: 使用CreateInstanceFailed

**Given**: 参数评估失败

**When**: 返回失败原因

**Then**:
- 使用现有的 `ETcsStateApplyFailReason::CreateInstanceFailed`
- 或新增 `ParameterEvaluationFailed`（如果需要更细粒度）

#### Scenario: 失败事件通知

**Given**: 参数评估失败导致创建失败

**When**: 返回失败

**Then**:
- 广播 `OnStateApplyFailed` 事件
- 事件包含失败原因
- 事件包含状态定义ID

### Requirement: 评估器返回值规范

参数评估器 MUST 明确返回成功或失败。

#### Scenario: 评估器返回bool

**Given**: 参数评估器执行评估

**When**: 评估完成

**Then**:
- 返回true表示成功
- 返回false表示失败
- 不使用异常或其他机制

#### Scenario: 评估器设置输出参数

**Given**: 参数评估器执行评估

**When**: 评估成功

**Then**:
- 通过输出参数返回评估结果
- 输出参数值有效
- 返回true

#### Scenario: 评估器失败不设置输出

**Given**: 参数评估器执行评估失败

**When**: 评估失败

**Then**:
- 输出参数值未定义（不应使用）
- 返回false
- 可选：记录失败原因到日志

## ADDED Requirements

### Requirement: 状态实例创建流程

状态实例创建流程 MUST 包含参数评估验证。

#### Scenario: 创建流程顺序

**Given**: 调用CreateStateInstance

**When**: 执行创建流程

**Then**:
1. 验证输入参数（Actor、StateDefId等）
2. **新增：执行参数评估验证阶段**
3. 创建状态实例对象
4. 初始化状态实例
5. 设置参数值
6. 返回成功

#### Scenario: 参数评估失败提前返回

**Given**: 参数评估验证失败

**When**: 创建流程执行到验证阶段

**Then**:
- 立即返回失败
- 不执行后续步骤
- 不创建任何对象
- 不消耗资源

## Implementation Notes

### 参数评估验证实现

```cpp
bool UTcsStateManagerSubsystem::ValidateStateParameters(
    const FTcsStateDefinition& StateDef,
    AActor* Owner,
    AActor* Instigator,
    TArray<FString>& OutFailedParams)
{
    bool bAllParamsValid = true;

    // 验证Numeric参数
    for (const auto& ParamPair : StateDef.NumericParameters)
    {
        if (!ParamPair.Value.NumericParamEvaluator)
        {
            continue;
        }

        float OutValue = 0.0f;
        auto Evaluator = ParamPair.Value.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
        bool bSuccess = Evaluator->Evaluate(Owner, Instigator, ParamPair.Value.ParamValueContainer, OutValue);

        if (!bSuccess)
        {
            bAllParamsValid = false;
            OutFailedParams.Add(FString::Printf(TEXT("%s (Numeric)"), *ParamPair.Key.ToString()));

            UE_LOG(LogTcsState, Error, TEXT("Numeric parameter '%s' evaluation failed for state '%s'"),
                *ParamPair.Key.ToString(),
                *StateDef.GetRowName().ToString());
        }
    }

    // 验证Bool参数
    for (const auto& ParamPair : StateDef.BoolParameters)
    {
        if (!ParamPair.Value.BoolParamEvaluator)
        {
            continue;
        }

        bool OutValue = false;
        auto Evaluator = ParamPair.Value.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
        bool bSuccess = Evaluator->Evaluate(Owner, Instigator, ParamPair.Value.ParamValueContainer, OutValue);

        if (!bSuccess)
        {
            bAllParamsValid = false;
            OutFailedParams.Add(FString::Printf(TEXT("%s (Bool)"), *ParamPair.Key.ToString()));

            UE_LOG(LogTcsState, Error, TEXT("Bool parameter '%s' evaluation failed for state '%s'"),
                *ParamPair.Key.ToString(),
                *StateDef.GetRowName().ToString());
        }
    }

    // 验证Vector参数
    for (const auto& ParamPair : StateDef.VectorParameters)
    {
        if (!ParamPair.Value.VectorParamEvaluator)
        {
            continue;
        }

        FVector OutValue = FVector::ZeroVector;
        auto Evaluator = ParamPair.Value.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
        bool bSuccess = Evaluator->Evaluate(Owner, Instigator, ParamPair.Value.ParamValueContainer, OutValue);

        if (!bSuccess)
        {
            bAllParamsValid = false;
            OutFailedParams.Add(FString::Printf(TEXT("%s (Vector)"), *ParamPair.Key.ToString()));

            UE_LOG(LogTcsState, Error, TEXT("Vector parameter '%s' evaluation failed for state '%s'"),
                *ParamPair.Key.ToString(),
                *StateDef.GetRowName().ToString());
        }
    }

    return bAllParamsValid;
}
```

### 创建流程集成

```cpp
ETcsStateApplyFailReason UTcsStateManagerSubsystem::CreateStateInstance(...)
{
    // ... 现有验证逻辑 ...

    // 新增：参数评估验证
    TArray<FString> FailedParams;
    if (!ValidateStateParameters(StateDef, Owner, Instigator, FailedParams))
    {
        UE_LOG(LogTcsState, Error, TEXT("State '%s' parameter evaluation failed: %s"),
            *StateDefId.ToString(),
            *FString::Join(FailedParams, TEXT(", ")));

        // 广播失败事件
        StateComponent->NotifyStateApplyFailed(
            Owner,
            StateDefId,
            ETcsStateApplyFailReason::CreateInstanceFailed);

        return ETcsStateApplyFailReason::CreateInstanceFailed;
    }

    // 继续创建实例...
}
```

### 评估器接口规范

```cpp
// UTcsStateNumericParamEvaluator
/**
 * 评估数值参数
 * @param Owner 状态所有者
 * @param Instigator 状态发起者
 * @param ParamData 参数数据
 * @param OutValue 输出值（仅在返回true时有效）
 * @return 是否评估成功
 */
UFUNCTION(BlueprintNativeEvent, Category = "State")
bool Evaluate(
    AActor* Owner,
    AActor* Instigator,
    const FInstancedStruct& ParamData,
    float& OutValue);
```

### 向后兼容性

- 现有评估器需要更新以返回bool
- 如果评估器未实现返回值，默认返回true（容错）
- 逐步迁移现有评估器

### 性能考虑

- 参数评估在创建阶段执行，不影响运行时性能
- 提前发现配置错误，避免运行时问题
- 验证开销远小于创建带缺参状态的风险

## Related Specs

- `state-level-parameter-clarity`: 状态等级参数清晰化
- `state-safety-fixes`: 状态安全修复
