# Design: StateParamInstance 类型拆分

## 类型拆分

```cpp
// 旧：一个结构体装三种值
USTRUCT()
struct FTcsStateParamInstance { ... float NumericValue; bool BoolValue; FVector VectorValue; ... };

// 新：三个独立结构体
USTRUCT()
struct FTcsNumericStateParamInstance
{
    FGameplayTag ParamTag;
    bool bIsSnapshot = true;
    FInstancedStruct ParamData;
    TSubclassOf<UTcsStateNumericParamEvaluator> NumericEvaluatorClass;
    TObjectPtr<UTcsStateNumericParamEvaluator> CachedEvaluator;
    float NumericValue = 0.0f;
    bool bHasEvaluated = false;

    bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
    float GetValue() const { return NumericValue; }
};

USTRUCT()
struct FTcsBoolStateParamInstance
{
    FGameplayTag ParamTag;
    bool bIsSnapshot = true;
    FInstancedStruct ParamData;
    TSubclassOf<UTcsStateBoolParamEvaluator> BoolEvaluatorClass;
    TObjectPtr<UTcsStateBoolParamEvaluator> CachedEvaluator;
    bool BoolValue = false;
    bool bHasEvaluated = false;

    bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
    bool GetValue() const { return BoolValue; }
};

USTRUCT()
struct FTcsVectorStateParamInstance
{
    FGameplayTag ParamTag;
    bool bIsSnapshot = true;
    FInstancedStruct ParamData;
    TSubclassOf<UTcsStateVectorParamEvaluator> VectorEvaluatorClass;
    TObjectPtr<UTcsStateVectorParamEvaluator> CachedEvaluator;
    FVector VectorValue = FVector::ZeroVector;
    bool bHasEvaluated = false;

    bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
    FVector GetValue() const { return VectorValue; }
};
```

## 容器拆分

```cpp
// UTcsStateInstance — 旧
TMap<FGameplayTag, FTcsStateParamInstance> StateParamInstances;

// 新
TMap<FGameplayTag, FTcsNumericStateParamInstance> NumericParamInstances;
TMap<FGameplayTag, FTcsBoolStateParamInstance>    BoolParamInstances;
TMap<FGameplayTag, FTcsVectorStateParamInstance>  VectorParamInstances;
```

## 访问器拆分

```cpp
// 旧
virtual FTcsStateParamInstance* GetStateParamInstance(FGameplayTag Tag);
virtual TMap<FGameplayTag, FTcsStateParamInstance>& GetStateParamInstances();

// 新
virtual FTcsNumericStateParamInstance* GetNumericParamInstance(FGameplayTag Tag)
    { return NumericParamInstances.Find(Tag); }
virtual FTcsBoolStateParamInstance*    GetBoolParamInstance(FGameplayTag Tag)
    { return BoolParamInstances.Find(Tag); }
virtual FTcsVectorStateParamInstance*  GetVectorParamInstance(FGameplayTag Tag)
    { return VectorParamInstances.Find(Tag); }

virtual TMap<FGameplayTag, FTcsNumericStateParamInstance>& GetNumericParamInstances()
    { return NumericParamInstances; }
```

## ResolveStateParamInstances 收窄

```cpp
// 旧：返回混合 Map*
static TMap<FGameplayTag, FTcsStateParamInstance>* ResolveStateParamInstances(Modifier, Target);

// 新：返回 Numeric Map*，函数名收窄
static TMap<FGameplayTag, FTcsNumericStateParamInstance>* ResolveNumericParamInstances(Modifier, Target);
```

## PopulateStateParamInstances 适配

```cpp
bool UTcsStateInstance::PopulateStateParamInstances(Def, Instigator, Target, FailedParams)
{
    for (auto& Pair : Def->Parameters)
    {
        switch (Pair.Value.ParameterType)
        {
        case SPT_Numeric:
            FTcsNumericStateParamInstance NInst;
            if (!NInst.Initialize(...)) { FailedParams.Add(...); continue; }
            // 内联求值
            NumericParamInstances.Add(Pair.Key, NInst);
            break;
        case SPT_Bool:
            FTcsBoolStateParamInstance BInst;
            if (!BInst.Initialize(...)) continue;
            // 内联求值 → BInst.BoolValue
            BoolParamInstances.Add(...);  break;
        case SPT_Vector:
            FTcsVectorStateParamInstance VInst;
            if (!VInst.Initialize(...)) continue;
            // 内联求值 → VInst.VectorValue
            VectorParamInstances.Add(...);  break;
        }
    }
}
```

## SkillEntry 同构

`UTcsSkillEntry::StateParamInstances` 同上述拆分为三个 TMap。`StartCooldown` / `GetLevel` 读取 Numeric 容器。`GetRemainingCooldownRatio` 等同步适配。

## 调用方适配

| 调用方 | 改动 |
|--------|------|
| `SetNumericParamByTag` | `NumericParamInstances.Find(Tag)->NumericValue = Value` |
| `GetNumericParamByTag` | `NumericParamInstances.Find(Tag)->GetValue()` |
| `RecalculateAttributeCurrentValues` 刷新 Operand | `ResolveNumericParamInstances → NumericParamInstances.Find`，不再需要 `CachedEvaluator` 空判 |
| `TcsSkillEntry::StartCooldown` | `NumericParamInstances.Find(CooldownTag)` |
| `TcsSkillInstance` 覆写 | 三个方法分别覆写指向 Entry 的对应容器 |
