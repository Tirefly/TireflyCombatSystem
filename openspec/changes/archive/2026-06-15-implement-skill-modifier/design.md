# Design: 实现 SkillModifier 机制

## 1. 新增类型

### ETcsSkillModifierMergePolicy

```cpp
UENUM(BlueprintType)
enum class ETcsSkillModifierMergePolicy : uint8
{
    Stack       UMETA(DisplayName = "Stack", ToolTip = "同 ModifierId 可叠加"),
    Exclusive   UMETA(DisplayName = "Exclusive", ToolTip = "同 ModifierId 只保留最高 Priority"),
};
```

### FStateParamModifierInstance

```cpp
USTRUCT(BlueprintType)
struct FStateParamModifierInstance
{
    GENERATED_BODY()

    // 用于互斥判定
    UPROPERTY()
    FName ModifierId;

    // 策略 CDO
    UPROPERTY()
    TObjectPtr<UTcsStateParamNumericModifierExecution> Evaluator;

    // 策略入参
    UPROPERTY()
    FInstancedStruct Config;

    // 执行优先级（从高到低）
    UPROPERTY()
    int32 Priority = 0;

    // 生命周期追因
    UPROPERTY()
    FTcsSourceHandle SourceHandle;

    // 被同 Id 更高优先级顶替时为 false
    UPROPERTY()
    bool bActive = true;
};
```

### UTcsStateParamNumericModifierExecution

三个参数类型的返回值无法统一（`float` / `bool` / `FVector`），因此无共同父类。各自独立的抽象基类。`Evaluate` 接收完整 `FStateParamModifierInstance` 而非仅 `Config`——自定义策略可能需要读取 `Priority`/`ModifierId`/`SourceHandle`。

```cpp
// Numeric — 本提案实现
UCLASS(Abstract, Blueprintable)
class UTcsStateParamNumericModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    /** @param ModifierInst 完整的 ModifierInstance（含 Config/Priority/ModifierId/SourceHandle） */
    virtual float Evaluate(float CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};

// Bool — 本提案提供基础 Set 子类
UCLASS(Abstract, Blueprintable)
class UTcsStateParamBoolModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    virtual bool Evaluate(bool CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};

// Vector — 本提案提供基础 Set 子类
UCLASS(Abstract, Blueprintable)
class UTcsStateParamVectorModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    virtual FVector Evaluate(FVector CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};
```

内建子类：

| 类型 | 子类 | 公式 |
|------|------|------|
| Numeric | `UTcsSkillModExec_Addition` | `Value + Config.Operand` |
| Numeric | `UTcsSkillModExec_MultiplyAdditive` | `Value * (1 + Config.Operand)` |
| Numeric | `UTcsSkillModExec_MultiplyContinued` | `Value * Config.Operand` |
| Numeric | `UTcsSkillModExec_Override` | `Config.Operand` |
| Bool | `UTcsSkillModExec_SetBool` | `Config.Value` |
| Vector | `UTcsSkillModExec_SetVector` | `Config.Value` |

### UTcsSkillEntrySelector

```cpp
UCLASS(Abstract, Blueprintable)
class UTcsSkillEntrySelector : public UObject
{
    GENERATED_BODY()
public:
    virtual TArray<UTcsSkillEntry*> ResolveTargets(
        const FInstancedStruct& Config,
        UTcsSkillComponent* SkillComp) const PURE_VIRTUAL(, return {};);
};
```

内建策略：

| 子类 | Config 内容 | 行为 |
|------|-----------|------|
| `UTcsSkillEntrySelector_ById` | `FName SkillDefId` | 精确匹配 |
| `UTcsSkillEntrySelector_ByGameplayTag` | `FGameplayTagContainer` | 按标签筛选 |
| `UTcsSkillEntrySelector_All` | 无 | 全部已学技能 |

### UTcsSkillModifierDefinition (DefAsset)

```cpp
UCLASS(BlueprintType)
class UTcsSkillModifierDefinition : public UPrimaryDataAsset
{
    UPROPERTY(EditAnywhere) FName ModifierId;

    // Target
    UPROPERTY(EditAnywhere, Category = "Target")
    TSubclassOf<UTcsSkillEntrySelector> EntrySelectorClass;
    UPROPERTY(EditAnywhere, Category = "Target",
        Meta = (EditCondition = "EntrySelectorClass != nullptr"))
    FInstancedStruct EntrySelectorConfig;

    // Value Correction
    UPROPERTY(EditAnywhere, Category = "Value Correction", Meta = (Categories = "StateParam"))
    FGameplayTag TargetParamTag;
    UPROPERTY(EditAnywhere, Category = "Value Correction")
    ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;
    UPROPERTY(EditAnywhere, Category = "Value Correction",
        Meta = (EditCondition = "TargetParamTag.IsValid()"))
    TSubclassOf<UTcsStateParamNumericModifierExecution> EvaluatorClass;
    UPROPERTY(EditAnywhere, Category = "Value Correction",
        Meta = (EditCondition = "EvaluatorClass != nullptr"))
    FInstancedStruct EvaluatorConfig;

    // Merge
    UPROPERTY(EditAnywhere, Category = "Merge") int32 Priority = 0;
    UPROPERTY(EditAnywhere, Category = "Merge")
    ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;
};
```

## 2. FTcsNumericStateParamInstance 扩展

```cpp
struct FTcsNumericStateParamInstance
{
    // ... 现有字段 ...

    // 新增：SkillModifier 实例列表
    UPROPERTY()
    TArray<FStateParamModifierInstance> ModifierInstances;

    // 新增：Assign/Remove
    void AssignModifier(const FStateParamModifierInstance& Instance);
    void RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);

    // 新增：带修正的求值
    float DeriveModifiedValue(UTcsSkillEntry* SkillEntry, AActor* Instigator) const;
};
```

### AssignModifier 逻辑

```
void AssignModifier(Instance):
    if (Instance.MergePolicy == Exclusive):
        for (Existing : ModifierInstances):
            if (Existing.ModifierId == Instance.ModifierId):
                if (Existing.Priority < Instance.Priority)
                    Existing.bActive = false   // 被顶替
                else
                    Instance.bActive = false    // 新的不如老的
    ModifierInstances.Add(Instance)
    ModifierInstances.Sort(按 Priority 降序)
```

### RemoveModifiersBySourceHandle 逻辑

```
void RemoveModifiersBySourceHandle(SourceHandle):
    TArray<FName> RemovedIds;
    for (Inst : ModifierInstances):
        if (Inst.SourceHandle == SourceHandle):
            RemovedIds.Add(Inst.ModifierId)
            ModifierInstances.Remove(Inst)
    // 恢复被顶替的同 Id 实例
    for (Id : RemovedIds):
        FStateParamModifierInstance* Highest = nullptr
        for (Inst : ModifierInstances):
            if (Inst.ModifierId == Id && !Inst.bActive):
                if (!Highest || Inst.Priority > Highest.Priority)
                    Highest = &Inst
        if (Highest) Highest.bActive = true
```

## 3. Level 迁移到 NumericParamInstances

Level 是所有 State 的通用概念（Buff/Skill/State），因此 `LevelParamTag` 放在 `UTcsStateDefinition` 基类。

### UTcsStateDefinition 新增

```cpp
// LevelParamTag — 编辑器可视，构造函数从 DeveloperSettings 读取默认值
UPROPERTY(EditAnywhere, Category = "Level", Meta = (Categories = "StateParam"))
FGameplayTag LevelParamTag;

// 不暴露 LevelParam 编辑器配置——Level 是运行时常量，无需 Evaluator
```

### UTcsDeveloperSettings 新增

```cpp
UPROPERTY(EditAnywhere, Config, Category = "State", Meta = (Categories = "StateParam"))
FGameplayTag DefaultLevelParamTag; // RENAMED to DefaultStateInstanceLevelParamTag
```

### 运行时注入

LearnSkill / ApplyState 时，直接创建一个用 `StateParam_ConstantNumeric` Evaluator 的 `FTcsNumericStateParamInstance`，写入 `NumericValue = Level`，插入 `NumericParamInstances[LevelParamTag]`。无需 DefAsset 上配置 `LevelParam`。

### SkillEntry 变更

```cpp
// TcsSkillEntry.h — 删除 int32 Level 成员字段

public:
    /** @return 当前技能等级（含 SkillModifier 修正）。 */
    int32 GetLevel() const
    {
        const UTcsStateDefinition* Def = GetStateDefinition();
        if (!Def) return 1;
        auto* Inst = NumericParamInstances.Find(Def->LevelParamTag);
        if (!Inst) return 1;
        return FMath::RoundToInt(Inst->DeriveModifiedValue(...));
    }

    /** 设置技能基础等级（写入 NumericValue，不经过 Modifier）。 */
    void SetLevel(int32 InLevel)
    {
        const UTcsStateDefinition* Def = GetStateDefinition();
        if (!Def) return;
        auto* Inst = NumericParamInstances.Find(Def->LevelParamTag);
        if (Inst) Inst->NumericValue = static_cast<float>(InLevel);
    }
```

### SkillEntry::InitializeFromDef 变更

Level 不再从 Def->Parameters 遍历创建——由外部调用方（LearnSkill）在创建 Entry 后显式注入。

## 4. StartCooldown 适配

```cpp
bool UTcsSkillEntry::StartCooldown(UTcsSkillInstance* SkillInstance)
{
    if (!SkillInstance) return false;

    auto* Def = GetSkillDefinition();
    auto* CI = NumericParamInstances.Find(Def->CooldownParamTag);
    if (!CI || !CI->CachedEvaluator) return true;

    float Duration = 0.0f;
    if (!CI->CachedEvaluator->Evaluate(...))
        return false;
    CI->NumericValue = Duration;
    CI->bHasEvaluated = CI->bIsSnapshot;

    // 使用 DeriveModifiedValue 获取含 SkillModifier 修正的 CD
    RemainingCooldown = CI->DeriveModifiedValue(this, SkillInstance->GetInstigator());
    return true;
}
```

## 5. 文件布局

```
Public/Skill/
  ├── TcsSkillModifierDefinition.h       (DefAsset)
  ├── TcsSkillModifierInstance.h          (枚举 + Config + Instance 结构体)
  ├── SkillEntrySelector/
  │     ├── TcsSkillEntrySelector.h       (基类)
  │     ├── TcsSkillEntrySelector_ById.h
  │     ├── TcsSkillEntrySelector_ByGameplayTag.h
  │     └── TcsSkillEntrySelector_All.h
  └── SkillModExecution/
        ├── TcsSkillModifierExecution.h   (三个类型基类 + Config 结构体)
        └── TcsSkillModExec_*.h           (6 个内建策略)
```
