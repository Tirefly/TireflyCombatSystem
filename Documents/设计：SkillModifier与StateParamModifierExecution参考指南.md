# 设计：SkillModifier 与 StateParamModifierExecution 参考指南

> **状态**：设计参考，不作为当前实现计划。SkillModifier 独立提案的输入文档。

## 1. 设计目标

SkillModifier 是一种独立于 AttributeModifier 的修改机制，用于"一个技能/天赋修改另一个技能（或自身）的 StateParam 运行时值"。典型场景：

- 装备"冷却戒指"：技能 A 的冷却时间 × 0.5
- 天赋"强化"：技能 B 的伤害系数 + 0.20
- Buff 期间：技能 C 的检测半径 + 25%

区别于 AttributeModifier（修改 Entity 的 Attribute），SkillModifier 的操作对象是 **SkillEntry 上的 NumericParamInstances**。

## 2. 核心设计：Level/Cooldown/Cost 统一为 StateParam

Level、Cooldown、Cost 本质上都是 Numeric 类型的运行时值。将它们全部存入 `SkillEntry.NumericParamInstances`，SkillModifier 只认 `TAG`，不区分语义。

```
UTcsSkillEntry
  └── NumericParamInstances
        ├── StateParam.Attack.Multiplier  → NumericValue = 1.55
        ├── StateParam.Skill.Level        → NumericValue = 3
        ├── StateParam.Skill.Cooldown     → NumericValue = 8s
        └── StateParam.Skill.ManaCost     → NumericValue = 50  (将来)

GetLevel()     → NumericParamInstances[Level.Tag].DeriveModifiedValue() → RoundToInt
StartCooldown() → NumericParamInstances[Cooldown.Tag] → 内联求值 → DeriveModifiedValue → RemainingCooldown
CheckCost()    → NumericParamInstances[Cost.Tag].DeriveModifiedValue()  (将来)
```

## 3. 运行时流程

```
SkillModifierDef (DefAsset, 编辑器配置)
  ├── EntrySelectorClass      → 技能 Entry 选取策略 CDO
  ├── TargetParamTag          → 修改哪个 StateParam
  ├── EvaluatorClass          → UTcsStateParamModifierExecution CDO
  ├── EvaluatorConfig         → FInstancedStruct（策略入参）
  ├── Priority / MergePolicy  → 排序与合并

═══════════ 应用时 ═══════════
SkillModifierDef 被引用（Buff EnterState / 装备穿戴 / 天赋激活）
  → EntrySelector->ResolveTargets(SkillComp) → TArray<UTcsSkillEntry*>
  → for each Entry:
        ParamInst = Entry->NumericParamInstances.Find(TargetParamTag)
        → if !ParamInst: continue
        → 创建 FStateParamModifierInstance
             ├── Evaluator = Def->EvaluatorClass CDO
             ├── Config    = Def->EvaluatorConfig
             ├── Priority / MergePolicy
             └── SourceHandle
        → Assign 到 ParamInst.ModifierInstances
        → Sort by Priority

═══════════ 求值时 ═══════════
FTcsNumericStateParamInstance::DeriveModifiedValue()
  1. float Value = NumericValue  // 初始化时求值器产出的基础值（永不改写）
  2. for (ModifierInstance : ModifierInstances)  // 按 Priority 从高到低，仅 bActive == true
        Value = ModifierInstance.Evaluator->Evaluate(Value, Config, SkillEntry, Instigator)
  3. return Value  // 每次调用时实时计算，不缓存

═══════════ 移除时 ═══════════
SourceHandle → 从所有受影响的 NumericParamInstance.ModifierInstances 中移除
```

### 关键设计

- **NumericValue 永不改写**：初始化时求值器产出的基础值缓存。修改时以此为起点沿链计算
- **不缓存最终值**：每次调用实时计算，由调用方自行决定缓存策略
- **ModifierInstance 直接挂在 NumericParamInstance 上**：无需额外查表
- **仅适用于 Numeric 类型**：Bool/Vector 暂不参与此机制

## 4. ModifierInstance 结构

```cpp
// 合并策略
enum class ETcsSkillModifierMergePolicy : uint8
{
    Stack,       // 默认：同 ModifierId 也可叠加
    Exclusive,   // 同 ModifierId 只保留 Priority 最高的一个
};

// 运行时实例（USTRUCT，直接挂在 FTcsNumericStateParamInstance 上）
USTRUCT()
struct FStateParamModifierInstance
{
    FName ModifierId;              // 用于互斥判定
    TObjectPtr<UTcsStateParamNumericModifierExecution> Evaluator;  // 策略 CDO
    FInstancedStruct Config;       // 策略入参
    int32 Priority = 0;           // 执行优先级
    FTcsSourceHandle SourceHandle; // 生命周期追因
    bool bActive = true;          // 被同 Id 更高优先级顶替时为 false
};
```

求值由 `FTcsNumericStateParamInstance::DeriveModifiedValue()` 沿链调用：

```cpp
float Value = NumericValue;
for (auto& Inst : ModifierInstances)
{
    if (!Inst.bActive) continue;
    Value = Inst.Evaluator->Evaluate(Value, Inst.Config, SkillEntry, Instigator);
}
return Value;
```

### Evaluator 类

命名：`UTcsStateParamNumericModifierExecution`。对标 `UTcsAttributeModifierExecution` 的 CDO 策略模式。

三个参数类型的返回值无法统一（`float` / `bool` / `FVector`），因此无共同父类，各自独立。`Evaluate` 接收完整 `FStateParamModifierInstance`——自定义策略可能需要读取 `Priority`/`ModifierId`/`SourceHandle`。

```cpp
// Numeric — 当前提案实现
UCLASS(Abstract, Blueprintable)
class UTcsStateParamNumericModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    virtual float Evaluate(float CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};

// Bool
UCLASS(Abstract, Blueprintable)
class UTcsStateParamBoolModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    virtual bool Evaluate(bool CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};

// Vector
UCLASS(Abstract, Blueprintable)
class UTcsStateParamVectorModifierExecution : public UObject
{
    GENERATED_BODY()
public:
    virtual FVector Evaluate(FVector CurrentValue, const FStateParamModifierInstance& ModifierInst,
        UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};
```

内建子类（对标 `UTcsAttributeModifierExecution` 子类命名）：

| 类型 | 子类 | 公式 |
|------|------|------|
| Numeric | `UTcsSkillModExec_Addition` | `Value + Config.Operand` |
| Numeric | `UTcsSkillModExec_MultiplyAdditive` | `Value * (1 + Config.Operand)` |
| Numeric | `UTcsSkillModExec_MultiplyContinued` | `Value * Config.Operand` |
| Numeric | `UTcsSkillModExec_Override` | `Config.Operand` |
| Bool | `UTcsSkillModExec_SetBool` | `Config.Value` |
| Vector | `UTcsSkillModExec_SetVector` | `Config.Value` |
```

### SkillModifierDefAsset

```cpp
// UTcsSkillModifierDefinition (DefAsset)
UPROPERTY(EditAnywhere)
FName ModifierId;

#pragma region Target

// 目标选取策略（CDO 模式）
UPROPERTY(EditAnywhere, Category = "Target")
TSubclassOf<UTcsSkillEntrySelector> EntrySelectorClass;

UPROPERTY(EditAnywhere, Category = "Target",
    Meta = (EditCondition = "EntrySelectorClass != nullptr"))
FInstancedStruct EntrySelectorConfig;

#pragma endregion


#pragma region Value Correction

// 修改的 StateParam 标识
UPROPERTY(EditAnywhere, Category = "Value Correction", Meta = (Categories = "StateParam"))
FGameplayTag TargetParamTag;

// 参数类型（用于筛选 EvaluatorClass 的基类）
UPROPERTY(EditAnywhere, Category = "Value Correction")
ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;

// 求值策略（当前仅 Numeric；按 TargetParamType 筛选对应类型基类）
UPROPERTY(EditAnywhere, Category = "Value Correction",
    Meta = (EditCondition = "TargetParamTag.IsValid()"))
TSubclassOf<UTcsStateParamNumericModifierExecution> EvaluatorClass;

UPROPERTY(EditAnywhere, Category = "Value Correction",
    Meta = (EditCondition = "EvaluatorClass != nullptr"))
FInstancedStruct EvaluatorConfig;

#pragma endregion


#pragma region Merge

UPROPERTY(EditAnywhere, Category = "Merge")
int32 Priority = 0;

UPROPERTY(EditAnywhere, Category = "Merge")
ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;

#pragma endregion
```

**一个 SkillModifierDef 只修改一个 StateParam**。如需修改多个参数，创建多个 DefAsset。

### EntrySelector 策略

```cpp
UCLASS(Abstract, Blueprintable)
class UTcsSkillEntrySelector : public UObject
{
    GENERATED_BODY()
public:
    /** @return 当前 Entity 上匹配的所有 SkillEntry。 */
    virtual TArray<UTcsSkillEntry*> ResolveTargets(
        const FInstancedStruct& Config,
        UTcsSkillComponent* SkillComp) const PURE_VIRTUAL(, return {};);
};
```

TCS 内建提供：

| 策略 | 行为 |
|------|------|
| `ById` | Config 中指定 `FName SkillDefId`，精确匹配一个技能 |
| `ByGameplayTag` | Config 中指定 `FGameplayTagContainer`，按标签筛选 |
| `All` | 当前 Entity 上所有已学会的技能 |

允许开发者自定义子类。

## 5. 互斥与 Merge

同一 ModifierId 的多个实例被同时引用时（如两件装备都提供同一个 `Mod_LevelUp`），按 MergePolicy 处理。互斥键为 `ModifierId`。

```
Assign(Instance, TargetParamInstance):
  if (Instance->MergePolicy == Exclusive):
    for (Existing : TargetParamInstance->ModifierInstances):
      if (Existing->ModifierId == Instance->ModifierId):
        if (Existing->Priority < Instance->Priority)
          Existing->bActive = false    // 被顶替
        else
          Instance->bActive = false    // 新来的更低，不生效
  Add & Sort

Remove(SourceHandle):
  → 删除匹配的 Instance
  → 同 ModifierId 的剩余 Inactive 实例中，最高 Priority 的 → bActive = true
```

`DeriveModifiedValue()` 只迭代 `bActive == true` 的 Modifier。移除高优先级时，低优先级自动恢复。



## 6. 优先级与执行顺序

`bActive == true` 的实例按 Priority 从高到低排序执行：

| Priority | 用途 | 示例 |
|----------|------|------|
| -100 | 全局约束 | 冷却上限 60s |
| 0 | 装备/物品 | 冷却戒指 ×0.5 |
| 100 | 天赋/被动 | 强化 +20% |
| 200 | 临时 Buff | 加速 -3s |
| 999 | 强制覆盖 | 冷却 = 0（技能刷新球） |

同 Priority 按添加顺序执行。

## 7. 生命周期

```
激活（Buff EnterState / 装备穿戴）
  → EntrySelector→ResolveTargets → for each matched Entry:
        → 创建 FStateParamModifierInstance → Assign → MergePolicy 判定 → Sort
  → 下次 DeriveModifiedValue() → 修正值生效

移除（Buff ExitState / 装备脱下）
  → SourceHandle → 遍历所有受影响的 NumericParamInstance
  → 删除匹配的 Instance
  → 同 ModifierId 被顶替的 Instance → bActive = true（恢复）
  → 下次 DeriveModifiedValue() → 修正已不再生效
```

## 8. 当前实现状态

`FTcsNumericStateParamInstance` 当前仅持有 `NumericValue`、`CachedEvaluator`，无 `ModifierInstances` 列表。`GetValue()` 直接返回 `NumericValue`。

已就位的架构准备：
- `SkillEntry.NumericParamInstances` 可承载 ModifierInstance 列表
- `GetNumericParamInstance()` virtual 访问器对存活 SkillInstance 即时可见
- Level/Cooldown 已接入 NumericParamInstances 体系

## 9. 不包含在当前实现范围

- `UTcsSkillModifierDefinition` DefAsset 设计
- `FStateParamModifierInstance` + `ETcsSkillModifierMergePolicy` 枚举
- `UTcsSkillEntrySelector` Entry 选取策略基类 + 内建子类
- `UTcsStateParamModifierExecution` Evaluator 基类 + 各类型基类 + 内建子类
- Level/Cost 正式迁移到 `NumericParamInstances`（当前 Cooldown 已迁移）
