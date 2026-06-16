# 设计：StateParamInstance + AttributeModifier Operand 动态绑定

## 概述

为三种类型的 StateParam 建立统一的运行时实例（`FTcsStateParamInstance`），在 AttributeModifier 的 Operand 上支持动态绑定到 StateParam，非 Snapshot 参数在每次属性重算时自动拉取最新值。

核心原则：**StateParam 值变化只更新自身，永远不主动触发下游逻辑。** `RecalculateAttributeCurrentValues` 是唯一的 Operand 刷新时机——StateParam 变，下次重算自然读到新值。

设计约束：

| 决策 | 结论 |
|------|------|
| 统一 Key 为 GameplayTag | Parameters、Instances、Bindings 全部用 GameplayTag |
| 三种类型共用一个 Instance | Numeric/Bool/Vector 三种值字段，根据 ParamType 使用对应字段 |
| Instance 存 StateInstance 上 | Merge 会拷贝/合并 Modifier，存生产者侧避免丢失 |
| CDO 缓存 + 初始化校验 | `Initialize` 获取 CDO 并缓存；失败则拒绝创建 |
| Operand 刷新时机 | 唯一入口：`RecalculateAttributeCurrentValues` 执行前 |

---

## 新增类型

### `FTcsStateParamInstance` — 统一运行时参数实例

```cpp
// 文件: TcsStateParamInstance.h

USTRUCT(BlueprintType)
struct FTcsStateParamInstance
{
    GENERATED_BODY()

    UPROPERTY() FGameplayTag ParamTag;
    UPROPERTY() ETcsStateParameterType ParamType;
    UPROPERTY() bool bIsSnapshot = true;
    UPROPERTY() FInstancedStruct ParamData;
    UPROPERTY() TSubclassOf<UTcsStateNumericParamEvaluator> NumericEvaluatorClass;
    UPROPERTY() TObjectPtr<UTcsStateNumericParamEvaluator> CachedEvaluator;
    UPROPERTY() float NumericValue = 0.0f;
    UPROPERTY() bool  BoolValue = false;
    UPROPERTY() FVector VectorValue = FVector::ZeroVector;
    bool bHasEvaluated = false;

    bool Initialize(const FGameplayTag& InTag, const FTcsStateParameter& ParamDef, FString& OutError);
    void Evaluate(AActor* Instigator, AActor* Target, UTcsStateInstance* StateInstance);
    float   GetNumeric() const { return NumericValue; }
    bool    GetBool()    const { return BoolValue; }
    FVector GetVector()  const { return VectorValue; }
};
```

### `FTcsStateParamBinding` — Operand 绑定描述

```cpp
// 文件: TcsAttributeModifier.h

USTRUCT(BlueprintType)
struct FTcsStateParamBinding
{
    GENERATED_BODY()

    UPROPERTY() FName OperandName;
    UPROPERTY() FGameplayTag StateParamTag;
};
```

---

## 修改现有类型

### `TcsStateDefinition::Parameters` — Key 改为 GameplayTag

```cpp
// TagParameters 字段已删除
UPROPERTY()
TMap<FGameplayTag, FTcsStateParameter> Parameters;
```

### `FTcsAttributeModifierInstance` — 新增 `OperandBindings`

```cpp
UPROPERTY()
TArray<FTcsStateParamBinding> OperandBindings;
```

### `UTcsStateInstance` — 六容器替换为 `StateParamInstances`

```
原有六容器已全部删除，统一为：
  StateParamInstances: TMap<FGameplayTag, FTcsStateParamInstance>
```

### `CreateAttributeModifierWithBindings`

```cpp
bool CreateAttributeModifierWithBindings(
    FName ModifierId,
    AActor* Instigator,
    const TArray<FTcsStateParamBinding>& Bindings,
    FTcsAttributeModifierInstance& OutModifierInst);
```

从 DefAsset 复制默认 Operands + 设置 OperandBindings，首次 `RecalculateAttributeCurrentValues` 时拉取初值。

### `RecalculateAttributeCurrentValues` — 执行前刷新 Operand

```cpp
if (Modifier.OperandBindings.Num() > 0)
{
    if (UTcsStateInstance* SI = ResolveStateInstanceFromModifier(Modifier))
    {
        for (const auto& B : Modifier.OperandBindings)
        {
            if (auto* P = SI->StateParamInstances.Find(B.StateParamTag))
            {
                P->Evaluate(Modifier.Instigator.Get(), GetOwner(), SI);
                Modifier.Operands.FindOrAdd(B.OperandName) = P->GetNumeric();
            }
        }
    }
}
```

### StateTree Task 节点

| Task | 目标 | 文件 |
|------|------|------|
| `ApplyAttributeModifierToOwner` | StateInstance.Owner 的 AttributeComponent | `TcsSTTask_ApplyAttributeModifierToOwner.h/.cpp` |
| `ApplyAttributeModifierToTarget` | 配置的 TargetActor 的 AttributeComponent | `TcsSTTask_ApplyAttributeModifierToTarget.h/.cpp` |

EnterState: `CreateAttributeModifierWithBindings` → `ApplyModifier`。ExitState 由 `SourceHandle → RemoveModifiersBySourceHandle` 自动清理。

### BuffPeriod Evaluator

`BuffPeriodDriver` Task 已删除，替换为 `TcsSTEvaluator_BuffPeriod` Global Evaluator，通过 `bIsPeriodBoundary` 输出 + Transition 驱动周期动作。

---

## 完整流程

```
State 激活 → 初始化 StateParamInstances（CDO 校验 → 首次求值）

StateParam 值变化 → 仅更新 Instance 内部 → 不触发下游

ApplyAttributeModifier → RecalculateAttributeCurrentValues
  → 刷新 Operands ← StateParamInstance.Evaluate() + GetNumeric()
  → Execute(Modifier)

Periodic Buff: BuffPeriod Evaluator → bIsPeriodBoundary → Transition → Task → ApplyModifier
```

---

## 实现清单

| # | 任务 | 状态 |
|---|------|------|
| 1 | 新增 `FTcsStateParamInstance` | ✅ |
| 2 | 新增 `FTcsStateParamBinding` | ✅ |
| 3 | `Parameters` Key → FGameplayTag | ✅ |
| 4 | `OperandBindings` on ModifierInstance | ✅ |
| 5 | `StateParamInstances` 替代六容器 | ✅ |
| 6 | `Initialize` + `Evaluate` | ✅ |
| 7 | `CreateStateInstance` 初始化 | ✅ |
| 8 | `Recalculate` 刷新 Operand | ✅ |
| 9 | `ResolveStateInstanceFromModifier` | ✅ |
| 10 | `CreateAttributeModifierWithBindings` | ✅ |
| 11 | StateTree Task (ToOwner + ToTarget) | ✅ |
| 12 | BuffPeriod Evaluator | ✅ |
