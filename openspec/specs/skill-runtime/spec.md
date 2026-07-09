# skill-runtime Specification

## Purpose
TBD - created by archiving change refactor-tcs-state-buff-skill-split. Update Purpose after archive.
## Requirements
### Requirement: Skill 自有 Learned 与 Cooldown 语义
TCS SHALL 在 Skill 自有的定义、实例和组件中建模 learned skill、cooldown、激活门槛以及 Skill 侧 snapshot 配置。

#### Scenario: 已学会技能在没有活动运行时状态时仍然持久存在
- **WHEN** 一个实体学会了技能但当前并未执行它
- **THEN** learned/cooldown 状态 SHALL 仍然存在于 Skill 侧自有的 learned-skill 数据对象与 `UTcsSkillComponent` 中
- **AND** 仅为了记住该技能存在，不应要求额外依赖通用 `State Core` 对象

#### Scenario: learned-skill 数据对象保持独立持有态命名
- **WHEN** TCS 为 learned-skill 持有态定义 Skill 自有数据对象
- **THEN** 该对象 SHALL 使用与技能执行态分离的独立持有态类型，例如当前契约中的 `UTcsSkillEntry`
- **AND** 这种命名收敛 SHALL NOT 把 learned skill 持有态重新塞回共享 `State Core` 基类

### Requirement: Skill 激活桥接到 State Runtime
TCS SHALL 让 Skill 自有激活逻辑在通过 Skill 侧校验后，再向 `UTcsStateComponent` 请求运行时状态。

#### Scenario: Skill 激活通过 State Core 创建运行时效果
- **WHEN** 一个技能通过 learned/cooldown/cost/activation 检查后
- **THEN** 该技能激活路径 MAY 向 `UTcsStateComponent` 请求一个或多个运行时状态
- **AND** 这些运行时状态 SHALL 通过共享 State Core 生命周期编排执行

#### Scenario: Skill 校验先于运行时状态请求发生
- **WHEN** 一个技能处于冷却中或因其他原因无法激活
- **THEN** `UTcsSkillComponent` SHALL 在向 `UTcsStateComponent` 请求任何运行时状态应用之前，就先拒绝这次激活

### Requirement: Skill 专属语义不污染共享 State 基类
TCS SHALL 让 Skill-only 的归属引用和激活元数据停留在共享 State 基类之外，除非它们确实被每一种运行时状态类型共享。

#### Scenario: 共享状态实例排除 Skill-only 归属元数据
- **WHEN** 定义共享运行时状态实例数据时
- **THEN** skill-only 的 owner 引用、learned-state 元数据和 cooldown 元数据 SHALL NOT 仅为 Skill 方便而存放到通用 state 基类上
- **AND** 激活时所需的任何桥接数据，都 SHALL 通过显式的 Skill 自有接口或 Skill 自有运行时类型传递

#### Scenario: 共享参数时机策略不被误写成 Skill-only
- **WHEN** Buff 与 Skill 都可能复用参数快照或实时重算这类求值时机策略
- **THEN** 这些共享参数策略 SHALL 保留在共享参数系统中
- **AND** Skill 侧文档不应把它们重新写成 Skill-only 配置

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

### Requirement: SkillModifierDefinition 定义资产

`UTcsSkillModifierDefinition` SHALL 是一个 DefAsset，声明修改目标、求值策略、优先级和互斥策略。一个 Def 只修改一个 StateParam。

#### Scenario: 配置冷却戒指修改器
- **WHEN** 创建 `Mod_RingOfCooldown` DefAsset，TargetParamTag = Cooldown Tag，EvaluatorClass = Multiply(0.5)
- **THEN** 编译通过，可在 Buff/装备中引用

#### Scenario: 修改多个参数需多个 Def
- **WHEN** 天赋需要同时 +Level 和 +DamageFactor
- **THEN** 必须创建两个独立的 SkillModifierDef

### Requirement: EntrySelector CDO 策略

`UTcsSkillEntrySelector` SHALL 通过 `ResolveTargets(SkillComp)` 返回匹配的 `TArray<UTcsSkillEntry*>`。TCS 内建提供 ById / ByGameplayTag / All。

#### Scenario: ById 精确匹配
- **WHEN** EntrySelectorClass=ById，Config={SkillDefId="SKILL_A"}
- **THEN** ResolveTargets 返回 Id 为 "SKILL_A" 的 SkillEntry

#### Scenario: All 返回全部
- **WHEN** EntrySelectorClass=All，SkillComp 有 3 个 LearnedSkill
- **THEN** ResolveTargets 返回全部 3 个 SkillEntry

### Requirement: SkillModifierInstance 的 Assign 与互斥

`FTcsNumericStateParamInstance::AssignModifier()` SHALL 创建 `FStateParamModifierInstance` 并加入 `ModifierInstances` 列表。`MergePolicy == Exclusive` 时，同 `ModifierId` 只保留最高 `Priority` 的实例为 Active，其余置为 Inactive。

#### Scenario: 同 Id 互斥—高优先级顶替低优先级
- **WHEN** 先 Assign Mod_LevelUp(Priority=0)，再 Assign Mod_LevelUp(Priority=100)
- **THEN** 第一个 bActive=false；第二个 bActive=true；DeriveModifiedValue 只计第二个

#### Scenario: 移除高优先级后低优先级恢复
- **WHEN** 上述状态下移除 Priority=100 的实例
- **THEN** Priority=0 的实例 bActive 恢复为 true

#### Scenario: Stack 策略不互斥
- **WHEN** MergePolicy=Stack，Assign 两个同 ModifierId 的实例
- **THEN** 两者 bActive 均为 true，DeriveModifiedValue 按 Priority 依次求值

### Requirement: DeriveModifiedValue 链式求值

`FTcsNumericStateParamInstance::DeriveModifiedValue()` SHALL 以 `NumericValue` 为起点，按 Priority 从高到低依次调用 `bActive == true` 的 Modifier 的 Evaluator，返回最终值。

#### Scenario: 多层修正链
- **WHEN** NumericValue=10，ModifierInstances=[Multiply(0.5, Priority=0), Add(3, Priority=100)]
- **THEN** DeriveModifiedValue = (10 + 3) * 0.5 = 6.5

#### Scenario: Inactive 实例被跳过
- **WHEN** 一个 Modifier 的 bActive=false
- **THEN** DeriveModifiedValue MUST NOT 调用其 Evaluator

### Requirement: Level 迁移到 NumericParamInstances

`UTcsStateDefinition` 基类 SHALL 新增 `LevelParamTag`（构造函数从 `UTcsDeveloperSettings::DefaultLevelParamTag` <!-- RENAMED to DefaultStateInstanceLevelParamTag; 待通过 change proposal 正式更新本 spec --> 读取默认值）。`UTcsSkillEntry::Level` (int32) SHALL 删除。Level 值 SHALL 在 LearnSkill / ApplyState 时通过 `StateParam_ConstantNumeric` Evaluator 注入到 `NumericParamInstances[LevelParamTag]`。

#### Scenario: LearnSkill 时注入 Level
- **WHEN** LearnSkill(Def="SKILL_A", Level=3)
- **THEN** NumericParamInstances[LevelParamTag] 的 NumericValue = 3.0f

#### Scenario: GetLevel 含 SkillModifier 修正
- **WHEN** NumericParamInstances[Level.Tag].NumericValue=3，有一个 Modifier Add(+1)
- **THEN** GetLevel() 返回 4

#### Scenario: SetLevel 写入基础值
- **WHEN** 调用 SetLevel(5)
- **THEN** NumericParamInstances[Level.Tag].NumericValue = 5.0f，ModifierInstances 不受影响

### Requirement: StartCooldown 使用修正值

`UTcsSkillEntry::StartCooldown()` SHALL 内联求值后，通过 `DeriveModifiedValue()` 获取含 SkillModifier 修正的冷却时长。

#### Scenario: 冷却戒指生效
- **WHEN** Cooldown 求值结果 = 8s，有一个 Modifier Multiply(0.5)
- **THEN** RemainingCooldown = 4s

