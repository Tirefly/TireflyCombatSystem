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

`UTcsSkillModifierDefinition` SHALL 是一个 DefAsset，声明修改目标、与 `TargetParamType` 对应的 typed evaluator 类、优先级和互斥策略。一个 Def 只修改一个 StateParam。`TargetParamType` SHALL 决定 Numeric / Bool / Vector 三个 evaluator 字段中哪一个是当前有效的 authoring 输入；当当前类型对应字段为空时，系统 SHALL 归一化为对应的 concrete 默认执行器。

#### Scenario: 配置 Numeric SkillModifierDef
- **WHEN** 创建一个 Numeric 类型的 `UTcsSkillModifierDefinition`
- **AND** `TargetParamType = Numeric`
- **THEN** authoring 面 SHALL 使用 Numeric evaluator 字段
- **AND** 若该字段为空，系统 SHALL 将其归一化为 `UTcsSkillModExec_Addition`

#### Scenario: 配置 Bool 或 Vector SkillModifierDef
- **WHEN** 创建一个 Bool 或 Vector 类型的 `UTcsSkillModifierDefinition`
- **THEN** authoring 面 SHALL 分别使用 Bool 或 Vector evaluator 字段
- **AND** 若对应字段为空，系统 SHALL 分别归一化为 `UTcsSkillModExec_SetBool` 或 `UTcsSkillModExec_SetVector`

#### Scenario: 只有匹配类型的 evaluator 字段可编辑
- **WHEN** 开发者切换 `TargetParamType`
- **THEN** 只有与当前类型匹配的 evaluator 字段应保持可编辑
- **AND** 其他类型字段不应继续作为当前 Def 的有效 authoring 输入

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

### Requirement: SkillComponent 承载 SkillModifier 权威账本

TCS SHALL 让 `UTcsSkillComponent` 成为 SkillModifier 的唯一权威账本宿主，并通过统一组件入口承担登记、查询、移除、恢复与生命周期清理职责。

#### Scenario: 通过组件入口应用 SkillModifier
- **WHEN** 任意 C++、Blueprint 或 StateTree 调用面要对技能应用一个 SkillModifier
- **THEN** 它 MUST 通过 `UTcsSkillComponent` 的统一组件入口完成
- **AND** 它 MUST NOT 直接手写 `UTcsSkillEntry` 的参数实例容器

#### Scenario: 通过来源批量清理 SkillModifier
- **WHEN** 一个 `SourceHandle` 对应的来源结束
- **THEN** `UTcsSkillComponent` MUST 能按该 `SourceHandle` 找到并移除全部相关 SkillModifier
- **AND** 该移除流程 MUST 不依赖对全部 learned skill 做无界全表扫描

#### Scenario: 单次 apply 写入失败时回滚本次新增项
- **WHEN** 一次 SkillModifier apply 调用已经向部分目标 `SkillEntry` 成功写入 runtime entry
- **AND** 后续目标写入失败
- **THEN** TCS MUST 回滚该次调用已经成功写入的 runtime entry 与参数实例链
- **AND** 系统 MUST NOT 留下半成功的脏状态

### Requirement: SkillModifier 直接写入 SkillEntry 参数实例链

TCS SHALL 让 SkillModifier 的唯一生效容器落在 `UTcsSkillEntry` 的 typed `StateParamInstances` 上。`UTcsSkillInstance` SHALL 继续通过既有透传访问器读取 `SkillEntry` 的参数实例，不新增独立的 SkillModifier 目标作用域。

#### Scenario: SkillEntry 立即看到新写入的 SkillModifier
- **WHEN** `UTcsSkillComponent` 成功将一个 SkillModifier 应用到某个 `UTcsSkillEntry`
- **THEN** 目标参数对应的 typed `StateParamInstance.ModifierInstances` MUST 立即包含该 modifier
- **AND** 后续通过 `SkillEntry` 读取该参数时 MUST 立即看到修正后的值

#### Scenario: 多目标 selector 会展开为多条 runtime records
- **WHEN** 一个 SkillModifier 的 `EntrySelector` 在一次 apply 中命中多个 `UTcsSkillEntry`
- **THEN** TCS MUST 为每个命中的 `UTcsSkillEntry` 分别创建独立的 runtime record
- **AND** 系统 MUST NOT 把多个目标折叠进同一条 runtime record

#### Scenario: 单条 runtime record 只对应一个目标落点
- **WHEN** TCS 创建一条 SkillModifier runtime record
- **THEN** 该记录 MUST 只对应一个具体 `TargetSkillEntry`
- **AND** 它 MUST 只对应一个具体目标参数落点，而不是“多个目标的聚合请求对象”

#### Scenario: SkillInstance 继续透传读取同一参数链
- **WHEN** 某个活跃 `UTcsSkillInstance` 读取它对应 `SkillEntry` 上的目标参数
- **THEN** 它 MUST 读到与 `SkillEntry` 相同的 SkillModifier 链结果
- **AND** TCS MUST NOT 因 SkillModifier 引入第二套仅供 `SkillInstance` 使用的目标容器

#### Scenario: 来源存活期间写入的临时 SkillModifier 对全部读取者可见
- **WHEN** 一个来源在其存活期间向目标 `SkillEntry` 写入临时 SkillModifier
- **THEN** 任何在该期间读取该 `SkillEntry` 参数的调用者 MUST 看到这份临时修正
- **AND** 该共享可见性 SHALL 被视为本提案接受的运行时语义

### Requirement: Snapshot 只冻结 Evaluator 重求值

TCS SHALL 将 `Snapshot` 语义限定为“禁止 Evaluator 重新求值”，而不是“冻结 SkillModifier 链”。即使目标参数已经 snapshot，SkillModifier 仍然可以增删改对应的 modifier 实例链。

#### Scenario: Snapshot 参数在首次求值后仍可写入 SkillModifier
- **WHEN** 一个 `bIsSnapshot == true` 的参数已经完成首次求值
- **AND** 后续有 SkillModifier 被应用到该参数
- **THEN** TCS MUST 允许该 SkillModifier 写入参数实例链
- **AND** 读取结果 MUST 体现该 SkillModifier 的修正

#### Scenario: Snapshot 只阻止再次调用 Evaluator
- **WHEN** 一个已 snapshot 的参数被重复读取
- **THEN** 它 MUST 不因为再次读取而重新调用 Evaluator
- **AND** 但它 MAY 因 SkillModifier 链变化而返回不同的最终值

### Requirement: SkillModifier 生命周期必须可按来源与目标清理

TCS SHALL 为 SkillModifier 提供可预测的生命周期清理语义，至少覆盖 `SourceHandle` 结束、`ForgetSkill` 和 Exclusive 恢复三个方向。

#### Scenario: 来源结束时按 SourceHandle 移除全部 SkillModifier
- **WHEN** 一个来源的 `SourceHandle` 生命周期结束
- **THEN** `UTcsSkillComponent` MUST 移除该来源写入的全部 SkillModifier
- **AND** 被移除后对应 `SkillEntry` 的参数实例链 MUST 不再保留这些 modifier

#### Scenario: ForgetSkill 时清理目标 SkillEntry 的 SkillModifier 账本记录
- **WHEN** `UTcsSkillComponent::ForgetSkill` 移除某个 learned skill
- **THEN** 与该 `SkillEntry` 关联的 SkillModifier 账本记录和索引 MUST 一并清理
- **AND** 后续查询 MUST 不再返回指向已遗忘 `SkillEntry` 的 SkillModifier

#### Scenario: 技能实例结束时清理该实例来源的 SkillModifier
- **WHEN** 一个 `UTcsSkillInstance` 生命周期结束，且它对应的 `SourceHandle` 曾写入 SkillModifier
- **THEN** TCS MUST 通过该 `SourceHandle` 清理这些 SkillModifier
- **AND** 该清理路径 SHOULD 复用统一的按来源移除逻辑，而不是复制第二套实例结束专用算法

#### Scenario: Exclusive SkillModifier 在高优先级移除后恢复次高候选
- **WHEN** 同组 `ModifierId` 的 Exclusive SkillModifier 中，当前最高优先级实例被移除
- **THEN** TCS MUST 恢复该组内剩余最高优先级候选的 `bActive`
- **AND** 后续参数读取 MUST 反映恢复后的有效实例

### Requirement: SkillModifier 外部入口必须统一路由到组件核心

TCS SHALL 为 SkillModifier 提供统一的组件级外部入口，并让 StateTree、Blueprint 与 C++ 共用同一条核心逻辑路径。

#### Scenario: StateTree 任务应用 SkillModifier 时附带来源句柄
- **WHEN** 一个 StateTree 任务在运行时对目标 `SkillComponent` 应用 SkillModifier
- **THEN** 它 MUST 将来源 `SourceHandle` 一并传入 `UTcsSkillComponent` 的组件入口
- **AND** 后续清理路径 MUST 能依赖该 `SourceHandle` 自动移除对应 SkillModifier

#### Scenario: 组件入口返回展开后的平铺结果
- **WHEN** 组件入口对一个或多个 `ModifierId` 执行 apply
- **THEN** 返回给调用方的 runtime records MUST 是展开后的平铺结果集合
- **AND** TCS MUST NOT 为此强制引入额外的 request、batch 或 group 级运行时对象

#### Scenario: Blueprint 与 C++ 不直接操作 SkillEntry 容器
- **WHEN** Blueprint 或 C++ 调用面要新增、查询或移除 SkillModifier
- **THEN** 它们 MUST 调用 `UTcsSkillComponent` 暴露的统一运行时入口
- **AND** 它们 MUST NOT 绕过组件直接改写 `SkillEntry.StateParamInstances`

### Requirement: 公开参数读取默认返回 effective value

TCS SHALL 让业务可见的 Skill / State 参数读取默认返回 effective value（base + 激活中的 SkillModifier 链结果）。无参 `GetModifiedValue()` 与任何默认参数读取 API SHALL 使用 ParamInstance 自身绑定的上下文；`GetBaseValue()` 与任何显式 Base API SHALL 仅表示未经 SkillModifier 链改写的 base value。跨系统消费（含 AttributeModifier 的 StateParam OperandEvaluator、参数条件、冷却进度）MUST 复用同一 effective 读取口径，而不是各自读取 base 字段。

#### Scenario: Get*ParamByTag 默认返回 SkillModifier 修正后的值
- **WHEN** 目标 `SkillEntry` 参数上已存在激活中的 SkillModifier
- **AND** 调用方通过 `UTcsStateInstance` / `UTcsSkillInstance` 的 `Get*ParamByTag` 读取该参数
- **THEN** 返回值 MUST 等于无参 `GetModifiedValue()` 的结果
- **AND** 返回值 MUST NOT 仅等于 base `GetBaseValue()`

#### Scenario: SkillInstance 读取真实宿主参数而不是本地空容器
- **WHEN** `UTcsSkillInstance` 调用 `Get*ParamByTag`
- **THEN** 它 MUST 通过 virtual `Get*ParamInstance` 定位到 `SkillEntry` 上的参数实例
- **AND** MUST NOT 因读取本地空容器而返回缺失或 base 默认值

#### Scenario: 需要 base 时必须走显式 Base API
- **WHEN** 调用方明确只需要未经 SkillModifier 修正的基础参数值
- **THEN** 它 MUST 调用显式 Base 读取入口（如 `Get*BaseParamByTag`）
- **AND** 默认 `Get*ParamByTag` MUST 继续返回 effective

#### Scenario: 冷却进度分母使用 effective 冷却时长
- **WHEN** 技能冷却已启动，且冷却参数上存在激活中的 SkillModifier
- **THEN** `GetRemainingCooldownRatio` 使用的冷却分母 MUST 为 effective 冷却时长
- **AND** MUST NOT 继续使用 base `GetBaseValue()` 作为分母

#### Scenario: 新增公开参数读取 API 不得绕过 effective 契约
- **WHEN** 后续新增任何公开的、面向业务的 StateParam 解析值读取 API
- **THEN** 该 API MUST 默认返回 effective value
- **AND** 若接口要暴露 base，MUST 在 API 名称中明确声明 Base
- **AND** MUST NOT 直接把 `NumericValue` / `GetBaseValue()` 作为业务默认返回

### Requirement: 技能激活固定使用所属实体

TCS SHALL 将 `UTcsSkillComponent::ActivateSkill` 的公开输入收敛为 `SkillDefId`。SkillComponent Owner MUST 同时作为创建出的 SkillInstance 的 Owner、Instigator、SourceHandle Instigator，以及其 SkillEntry 参数的固定求值 Instigator。

#### Scenario: ActivateSkill 不接受外部 Instigator
- **WHEN** 检查 `UTcsSkillComponent::ActivateSkill` 的 public API
- **THEN** 它 MUST 仅接收 `SkillDefId`
- **AND** MUST NOT 允许调用方传入与 SkillComponent Owner 不同的 Instigator

#### Scenario: SkillEntry 参数上下文绑定所属实体
- **WHEN** SkillComponent 为所属实体创建一个 SkillEntry
- **THEN** 该 Entry 的全部 StateParamInstance MUST 绑定该 Entry 与所属实体作为内部求值上下文
- **AND** 后续激活该技能 MUST NOT 重写这些参数的 Instigator 上下文

### Requirement: SkillInstance 不得直接施加 Ongoing AttributeModifier

SkillInstance SHALL 只能直接施加 Instant AttributeModifier，或通过目标本地 StateInstance（首版以 BuffInstance 为宿主）间接产生 Ongoing AttributeModifier。直接 Ongoing 请求 MUST 硬拒绝、零修改，并在 Development / Editor 输出 Warning。

#### Scenario: Skill 直接 Ongoing 被拒绝
- **WHEN** SkillInstance 调用 `ApplyAttributeModifier` 且 `ApplicationMode = Ongoing`
- **THEN** 系统 MUST 拒绝该请求并保持零修改
- **AND** Development / Editor MUST 输出 Warning

#### Scenario: Skill 可通过目标本地 StateInstance 间接 Ongoing
- **WHEN** SkillInstance 先在目标 Actor 上创建 / 施加 StateInstance
- **AND** 该 StateInstance 再 Apply Ongoing AttributeModifier
- **THEN** 该路径 MUST 被允许
- **AND** Ongoing 生命周期 MUST 由该目标本地 StateInstance 管理

