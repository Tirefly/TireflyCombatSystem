# skill-runtime Specification

## Purpose
定义 Skill 运行时边界：`UTcsSkillEntry` 以 `SkillDefId` 表达 learned-skill 拥有态，并缓存已校验 `UTcsSkillDefinition*`；`LearnSkill` / `ActivateSkill` / SkillModifier apply 以 DefId 为主路径，Definition 解析失败必须显式失败。等级参数使用 `LevelParamTag`，默认值来自 `UTcsDeveloperSettings::DefaultStateInstanceLevelParamTag`。
## Requirements
### Requirement: Skill 自有 Learned 与 Cooldown 语义

TCS SHALL 在 Skill 自有的定义、实例和组件中建模 learned skill、cooldown、激活门槛以及 Skill 侧 snapshot 配置。`UTcsSkillEntry` SHALL 以 `SkillDefId` 承担对外身份流转，同时在实例内部持有一个由合法 `SkillDefId` 解析并校验后的权威 `UTcsSkillDefinition*` 运行时缓存。

#### Scenario: learned-skill 拥有态以 SkillDefId 作为权威身份
- **WHEN** 一个实体已经学会某个技能，且当前没有活动中的 Skill runtime state
- **THEN** `UTcsSkillEntry` MUST 仍能仅凭 `SkillDefId` 表达其拥有态身份
- **AND** `UTcsSkillDefinition*` 若存在，SHALL 被视为该运行时实例内部的权威 SkillDef 缓存，而不是脱离 `SkillDefId` 约束的外部身份真相

#### Scenario: SkillEntry 内部缓存减少重复读取负载
- **WHEN** `UTcsSkillEntry` 已经完成合法 Definition 解析并进入运行时使用阶段
- **THEN** 它 MUST 持有对应的已校验 `UTcsSkillDefinition*` 缓存
- **AND** 调用链 SHOULD 优先复用该缓存，而不是在每次运行时访问时都重新向 DefinitionManager 取回同一个 SkillDef

### Requirement: Skill 激活桥接到 State Runtime

TCS SHALL 让 Skill 自有激活逻辑在通过 Skill 侧校验后，再向 `UTcsStateComponent` 请求运行时状态。`LearnSkill` 与 `ActivateSkill` 的主执行路径 SHALL 支持按 `SkillDefId` 执行。

#### Scenario: LearnSkill 按 SkillDefId 执行
- **WHEN** 调用方只有 `SkillDefId`，且对应 `UTcsSkillDefinition` 当前未加载
- **THEN** 系统 MUST 允许通过统一的 Definition 加载归口解析该 Def
- **AND** 解析成功后 MUST 完整创建对应的 `UTcsSkillEntry`

#### Scenario: ActivateSkill 按 SkillDefId 执行
- **WHEN** `ActivateSkill(SkillDefId, Instigator)` is invoked
- **THEN** Skill 激活主路径 MUST 以 `SkillDefId` 为输入驱动 Definition 解析、Entry 查询与后续激活校验
- **AND** 不得要求调用方先显式持有已加载 `UTcsSkillDefinition*`

#### Scenario: 指针型 Skill 入口不得继续作为 public API 保留
- **WHEN** 检查 `LearnSkill`、`ActivateSkill` 的最终 public API 面
- **THEN** `LearnSkill(UTcsSkillDefinition*)` 与其他对象型 Skill public 入口 MUST 已清零
- **AND** 若迁移阶段存在残余逻辑，它们也只能作为内部辅助转发实现存在

#### Scenario: Definition 解析失败时拒绝学习或激活
- **WHEN** `SkillDefId` 无法解析到合法的 `UTcsSkillDefinition`
- **THEN** `LearnSkill` 或 `ActivateSkill` MUST 明确失败
- **AND** MUST NOT 创建残缺的占位 `UTcsSkillEntry`

#### Scenario: Skill 对象型残余逻辑不得改写失败语义
- **WHEN** 迁移阶段内部残余的对象型 Skill 转发逻辑收到空对象、非法对象或无法提取合法 `SkillDefId` 的输入
- **THEN** 它 MUST 遵循统一 Definition 查询失败语义并透传失败
- **AND** 它 MUST NOT 因兼容旧接口而静默吞掉失败或伪造 learned/activated 成功

#### Scenario: ApplySkillModifier 按 DefId 执行
- **WHEN** 调用方要施加一个 SkillModifier，且手里只有 `SkillModifierDefId`
- **THEN** 主执行路径 MUST 允许直接按 `SkillModifierDefId` 解析对应定义并继续执行 apply
- **AND** 不得要求调用方先显式持有已加载的 `UTcsSkillModifierDefinition*`

#### Scenario: SkillModifier Definition 解析失败时不得部分 apply
- **WHEN** SkillModifier apply 主路径按 `SkillModifierDefId` 解析 Definition 失败
- **THEN** 该次 apply MUST 明确失败
- **AND** 系统 MUST NOT 写入部分 runtime modifier entry、部分参数实例链或其他半完成副作用

#### Scenario: SkillModifier 对象型入口不得继续作为 public API 保留
- **WHEN** 检查 `ApplySkillModifier` 的最终 public API 面
- **THEN** 对象型 SkillModifier public 入口 MUST 已清零
- **AND** 若迁移阶段存在残余逻辑，它们也只能作为内部辅助转发实现存在

#### Scenario: SkillModifier 对象型残余逻辑不得改写失败语义
- **WHEN** 迁移阶段内部残余的对象型 SkillModifier 转发逻辑收到空对象、非法对象或无法提取合法 `SkillModifierDefId` 的输入
- **THEN** 它 MUST 遵循统一 Definition 查询失败语义并透传失败
- **AND** 它 MUST NOT 因兼容旧接口而吞掉错误或伪造 apply 成功

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

`UTcsStateDefinition` 基类 SHALL 使用 `LevelParamTag` 表达实例等级参数，其构造默认值 MUST 从 `UTcsDeveloperSettings::DefaultStateInstanceLevelParamTag` 读取。`UTcsSkillEntry` MUST NOT 保留独立 `int32 Level` 字段；技能等级 SHALL 保存在 `NumericParamInstances[LevelParamTag]` 中，并通过 `GetLevel()` / `SetLevel()` 访问。

#### Scenario: LearnSkill 时建立默认等级参数
- **WHEN** `LearnSkill(SkillDefId)` 成功创建 `UTcsSkillEntry`
- **THEN** 若对应 Definition 配置了有效 `LevelParamTag`，Entry MUST 初始化该 NumericParamInstance 的基础值为 `1.0f`

#### Scenario: SetLevel 更新基础等级
- **WHEN** 调用 `UTcsSkillEntry::SetLevel(3)`
- **THEN** `NumericParamInstances[LevelParamTag]` 的基础值 MUST 更新为 `3.0f`
- **AND** 已挂接的 SkillModifier 参数链 MUST 保持不变

#### Scenario: GetLevel 含 SkillModifier 修正
- **WHEN** `NumericParamInstances[LevelParamTag]` 的基础值为 `3`，且存在 Add(+1) SkillModifier
- **THEN** `GetLevel()` MUST 返回 `4`

### Requirement: StartCooldown 使用修正值

`UTcsSkillEntry::StartCooldown()` SHALL 内联求值后，通过 `DeriveModifiedValue()` 获取含 SkillModifier 修正的冷却时长。

#### Scenario: 冷却戒指生效
- **WHEN** Cooldown 求值结果 = 8s，有一个 Modifier Multiply(0.5)
- **THEN** RemainingCooldown = 4s
