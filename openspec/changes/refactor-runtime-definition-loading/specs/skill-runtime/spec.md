## MODIFIED Requirements
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
