# skill-runtime Spec Delta

## ADDED Requirements

### Requirement: StateParamInstance 求值-修正分离

`FTcsStateParamInstance` SHALL 将 `NumericValue` 拆分为 `BaseValue`（求值器产出）和 `ModifierScale`/`ModifierOffset`（外部修正）。`GetNumeric()` SHALL 返回 `(BaseValue + ModifierOffset) * ModifierScale`。`Evaluate()` SHALL 只写入 `BaseValue`。

#### Scenario: 求值不覆盖修正
- **WHEN** Evaluate 返回新 BaseValue=10，ModifierScale 已被 SkillModifier 写入 0.5
- **THEN** GetNumeric() MUST 返回 5，ModifierScale MUST 保持 0.5

### Requirement: StateInstance 提供 virtual StateParamInstance 访问器

`UTcsStateInstance` SHALL 新增 `virtual GetStateParamInstance(FGameplayTag)`，返回本地 `StateParamInstances` 中的指针。所有对 `StateParamInstances` 的内部读写 MUST 通过此方法。`UTcsSkillInstance` SHALL 覆写指向 `Entry->StateParamInstances`。

#### Scenario: SkillInstance 读 Entry 上的实例
- **WHEN** `GetStateParamInstance(Tag)` 在 SkillInstance 上被调用
- **THEN** MUST 返回 `SkillEntry->StateParamInstances.Find(Tag)`

### Requirement: StateInstance 提供 virtual PopulateStateParamInstances

`UTcsStateInstance` SHALL 新增 `virtual PopulateStateParamInstances(Def, Instigator, Target)`，从 Def 遍历 Parameters 创建并求值参数实例。`CreateStateInstance` SHALL 调用此方法替代现有 inline 代码块。`UTcsSkillInstance` SHALL 覆写为空实现。

#### Scenario: 基类实现 populate 本地 StateParamInstances
- **WHEN** Buff/State 类型调用 PopulateStateParamInstances
- **THEN** Def->Parameters 逐个 Initialize → Evaluate → Add 到 this->StateParamInstances

#### Scenario: SkillInstance 跳过 populate
- **WHEN** SkillInstance 的 PopulateStateParamInstances 被调用
- **THEN** MUST 为空实现（Entry 已持有实例）

### Requirement: 删除 EvaluateAndApplyStateParameters

`UTcsStateComponent::EvaluateAndApplyStateParameters` SHALL 删除。调用方 SHALL 改用 `StateInstance->PopulateStateParamInstances()`。

#### Scenario: 不再被调用
- **WHEN** 编译引用了 `EvaluateAndApplyStateParameters` 的代码
- **THEN** 编译失败，需迁移到 `PopulateStateParamInstances`

### Requirement: SkillEntry 承载完整 StateParamInstances

`UTcsSkillEntry` SHALL 持有 `TMap<FGameplayTag, FTcsStateParamInstance> StateParamInstances`，在 `InitializeFromDef` 时从 Def 的 `Parameters` 和 `CooldownParam` 构建。

#### Scenario: Entry 的实例是 SkillInstance 的权威源
- **WHEN** SkillModifier 写入 `Entry->StateParamInstances[Cooldown].ModifierScale = 0.5`
- **THEN** 存活 SkillInstance 通过 `GetStateParamInstance()` 立即读到修正后值

### Requirement: SkillEntry 承载技能等级

`UTcsSkillEntry` SHALL 通过 `Level` 字段承载技能等级。`UTcsStateInstance::GetLevel()` SHALL 加 `virtual`，`UTcsSkillInstance` SHALL 覆写返回 `Entry->GetLevel()`。

#### Scenario: LevelArray 通过虚函数读取 Skill Level
- **WHEN** LevelArray 求值器调 `StateInstance->GetLevel()`，实际类型为 `UTcsSkillInstance`
- **THEN** 虚函数分发 MUST 返回 `SkillEntry->Level`

### Requirement: SkillDefinition 通过 FTcsStateParameter 声明冷却

`UTcsSkillDefinition` SHALL 通过 `CooldownParam` (FTcsStateParameter, Numeric 类型) 声明冷却时长，支持 LevelArray 等求值器。值为 0 的求值结果表示无冷却。

#### Scenario: LevelArray 冷却
- **WHEN** CooldownParam 配置 LevelArray {Lv1:10, Lv2:8, Lv3:6}，Entry->Level=3
- **THEN** StartCooldown 时 Evaluate → BaseValue=6 → GetNumeric() = (6+0)*1.0 = 6s

### Requirement: SkillEntry 冷却在 StartCooldown 时求值

`StartCooldown` SHALL 调用 `CooldownInstance.Evaluate()` 并设置 `RemainingCooldown = CooldownInstance.GetNumeric()`。

#### Scenario: 升级后下次激活使用新 CD
- **WHEN** Entry->SetLevel(3)，下次 ActivateSkill → StartCooldown
- **THEN** Evaluate 返回新等级的 BaseValue，RemainingCooldown 使用新值

#### Scenario: SkillModifier 修正 CD 在下次激活生效
- **WHEN** SkillModifier 写入 ModifierScale=0.5，下次 ActivateSkill → StartCooldown
- **THEN** GetNumeric() = BaseValue * 0.5
