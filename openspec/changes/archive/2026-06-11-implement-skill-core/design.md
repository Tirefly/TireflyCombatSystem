# Design: Skill 核心实现

## 类型变更

### `UTcsSkillEntry` — 技能持有态

SkillEntry 作为 learned-skill 的权威运行时持有者，管理 Level、Cooldown、完整的 StateParamInstances（与 StateInstance 同构）。

```
新增：
  int32 Level = 1;
  float RemainingCooldown = 0.0f;  // 运行时递减
  TWeakObjectPtr<UTcsSkillInstance> ActiveInstance;
  TMap<FGameplayTag, FTcsStateParamInstance> StateParamInstances;  // 与 StateInstance 同构

新增方法：
  void InitializeFromDef(UTcsSkillDefinition* Def)
      → Level = 1
      → RemainingCooldown = 0.0f
      → for each Def->Parameters: 创建 FTcsStateParamInstance → 加入 StateParamInstances
      // 与 UTcsStateInstance::PopulateStateParamInstances 同一逻辑

  void SetLevel(int32 InLevel) → Level = InLevel

  void StartCooldown(SkillInstance)
      → CooldownInstance.Evaluate(...)
      → RemainingCooldown = CooldownInstance.GetNumeric()
      // GetNumeric() = (BaseValue + ModifierOffset) * ModifierScale

  void TickCooldown(float DeltaTime)
      → RemainingCooldown = FMath::Max(0.0f, RemainingCooldown - DeltaTime)

  bool IsOnCooldown() const → RemainingCooldown > 0.0f

  float GetRemainingCooldownRatio() const
      → (CooldownInstance.GetNumeric() > 0)
        ? RemainingCooldown / CooldownInstance.GetNumeric()
        : 0.0f
```

### `UTcsSkillDefinition` — 新增 CooldownParam

```
新增：
  UPROPERTY(EditAnywhere, Category = "Cooldown")
  FTcsStateParameter CooldownParam;  // Numeric 类型，支持 LevelArray 等求值器
```

### `FTcsStateParamInstance` — 扩展修正因子

`NumericValue` 拆分为 `BaseValue`（求值器产出）和修正因子（SkillModifier 写入）：

```
新增：
  float BaseValue = 0.0f;       // 求值器产出（LevelArray → 10s）
  float ModifierScale = 1.0f;   // SkillModifier 乘算修正（冷却戒指 → 0.5）
  float ModifierOffset = 0.0f;  // SkillModifier 加算修正

修改：
  float GetNumeric() const
      → (BaseValue + ModifierOffset) * ModifierScale  // 旧: return NumericValue

  Evaluate(...)
      → Evaluator->Evaluate(..., BaseValue)  // 旧: 写入 NumericValue
      → 求值失败时保留 BaseValue 旧值
```

### `UTcsStateInstance` — 新增 virtual 访问器

```
新增 protected virtual:
  FTcsStateParamInstance* GetStateParamInstance(FGameplayTag Tag)
      → return StateParamInstances.Find(Tag);

新增 public virtual:
  void PopulateStateParamInstances(const UTcsStateDefinition* Def, AActor* Instigator, AActor* Target)
      → for each Def->Parameters: Initialize → Evaluate → StateParamInstances.Add()
      // 从 CreateStateInstance 中的 inline 代码块移入

修改：
  int32 GetLevel() → virtual int32 GetLevel()  // 允许子类覆写
```

### `UTcsSkillInstance` — 覆写两个 virtual

```cpp
virtual FTcsStateParamInstance* GetStateParamInstance(FGameplayTag Tag) override
{
    return SkillEntry.IsValid()
        ? SkillEntry->StateParamInstances.Find(Tag)
        : Super::GetStateParamInstance(Tag);
}

virtual void PopulateStateParamInstances(...) override
{
    // 空实现 — SkillEntry 已持有 StateParamInstances
}

virtual int32 GetLevel() const override
{
    return SkillEntry.IsValid() ? SkillEntry->GetLevel() : Super::GetLevel();
}
```

### `UTcsSkillComponent` — 填充核心逻辑

```
新增：
  TMap<FName, UTcsSkillEntry*> LearnedSkills;

新增方法：
  void LearnSkill(UTcsSkillDefinition* Def)
      → 创建 Entry (Def->SkillEntryClass)
      → Entry->InitializeFromDef(Def)  // 内部 populate StateParamInstances
      → LearnedSkills.Add(Def->GetFName(), Entry)

  void ForgetSkill(FName SkillDefId)
  bool HasSkill(FName SkillDefId) const
  UTcsSkillEntry* GetSkillEntry(FName SkillDefId) const

  ETcsSkillActivateResult ActivateSkill(FName SkillDefId, AActor* Instigator);

  // Tick 驱动 CD 递减
  TickComponent → for each Entry : TickCooldown(DeltaTime)
```

### `ETcsSkillActivateResult` — 新枚举

```
enum class ETcsSkillActivateResult : uint8
{
    Success, NotLearned, OnCooldown, InvalidDefinition, ApplyFailed
};
```

## 激活流程

```
ActivateSkill(SkillDefId, Instigator):
  1. Entry = LearnedSkills.Find(SkillDefId) → if !Entry: return NotLearned

  2. if Entry->IsOnCooldown(): return OnCooldown

  3. // 单实例：取消上一个
     if (Entry->ActiveInstance.IsValid())
        StateCmp->RequestStateRemoval(...TcsStateRemovalReasons::Cancelled)

  4. Def = Entry->GetSkillDefinition()
     → if !Def || !Def->SkillInstanceClass: return InvalidDefinition

  5. SkillInst = NewObject<Def->SkillInstanceClass>(...)
     → SkillInst->SetSkillEntry(Entry)
     // PopulateStateParamInstances 在此为 no-op（Entry 已持有）
     // GetLevel() → Entry->GetLevel() 虚函数分发生效

  6. StateCmp->TryApplyStateInstance(SkillInst) → if !success: return ApplyFailed

  7. Entry->StartCooldown(SkillInst)
       → CooldownInstance.Evaluate(Instigator, Target, SkillInst)
       → RemainingCooldown = CooldownInstance.GetNumeric()  // (BaseValue + Off) * Scale

  8. Entry->ActiveInstance = SkillInst
  9. return Success
```

## CreateStateInstance 清理

`CreateStateInstance` 中的重复逻辑合并：

```
// 删除
行 148: EvaluateAndApplyStateParameters(...)     → 逐类型手动求值 + Set*ParamByTag

// 删除
行 190-205: inline for each: Initialize → Evaluate → StateParamInstances.Add()

// 替换为
StateInstance->PopulateStateParamInstances(StateDef, Instigator, OwnerActor)
  → 基类实现：遍历 Def->Parameters，创建 Instance，Evaluate，Add
  → SkillInstance 实现：空（Entry 已持有）
```

`EvaluateAndApplyStateParameters` 方法从 `UTcsStateComponent` 删除，调用方改为 `PopulateStateParamInstances`。

## SkillInstance 移除时清理

`UTcsSkillComponent` 通过 State 移除事件的广播 → `Entry->ActiveInstance.Reset()`。

### `FTcsAttributeModifierInstance` — 新增引用字段

```cpp
新增：
  TWeakObjectPtr<UTcsStateInstance> SourceStateInstance;  // 直接引用，最快路径
  TWeakObjectPtr<UTcsSkillEntry>    SourceSkillEntry;     // AOE/投射物场景
```

### `ResolveStateParamInstances` — 替换 `ResolveStateInstanceFromModifier`

当前 `ResolveStateInstanceFromModifier` 返回 `UTcsStateInstance*`，改为返回 `TMap<FGameplayTag, FTcsStateParamInstance>*`，三层回退：

```cpp
static TMap<FGameplayTag, FTcsStateParamInstance>* ResolveStateParamInstances(
    const FTcsAttributeModifierInstance& Modifier,
    UTcsSkillComponent* TargetSkillComp)
{
    // 1. SourceStateInstance 直接引用（最快）
    if (Modifier.SourceStateInstance.IsValid())
        return &Modifier.SourceStateInstance->GetStateParamInstances();

    // 2. SourceSkillEntry（AOE/投射物：技能已结束但 Entry 还在）
    if (Modifier.SourceSkillEntry.IsValid())
        return &Modifier.SourceSkillEntry->StateParamInstances;

    // 3. SourceHandle 回溯（兜底）
    if (UTcsStateInstance* SI = ResolveStateInstanceFromSourceHandle(Modifier))
        return &SI->GetStateParamInstances();

    return nullptr;
}
```

### `RecalculateAttributeCurrentValues` 中刷新 Operand 的适配

```cpp
// 旧
UTcsStateInstance* SI = ResolveStateInstanceFromModifier(Modifier);
if (SI)
    P = SI->StateParamInstances.Find(Tag);

// 新
auto* ParamMap = ResolveStateParamInstances(Modifier, TargetSkillComp);
if (ParamMap)
    P = ParamMap->Find(Tag);
```

## SkillModifier 路径（后续提案，仅预留）

```
SkillModifier 挂载 → SkillComponent 接收 → 注入 Entry.StateParamInstances[TAG]
  → ModifierScale / ModifierOffset 写入
  → 存活 SkillInstance 通过 GetStateParamInstance() 自然读到
  → AOE/投射物通过 SourceSkillEntry → Entry.StateParamInstances 读到
  → 下次 StartCooldown → GetNumeric() = 修正后值
```

## SkillStateTree 运行时

SkillStateTree 由 `UTcsSkillInstance::SetContextRequirements` + `CollectExternalData` 驱动，直接对接已有 `UTcsSTSchema_Skill`。

双上下文可用：
- `SkillInstance` — 当前激活执行态
- `SkillEntry`   — learned-skill 数据对象（GetSkillDefinition, GetLevel, IsOnCooldown, GetRemainingCooldownRatio, GetStateParamInstance...）
