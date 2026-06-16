# state-runtime-access Specification

## Purpose
TBD - created by archiving change refactor-state-runtime-access-contract. Update Purpose after archive.
## Requirements
### Requirement: StateInstance 应作为抽象共享执行态基类存在
TCS SHALL 将 `UTcsStateInstance` 固化为抽象共享执行态基类，而不是继续把它当成可直接承载具体业务 StateTree 的 concrete 运行时类型。

#### Scenario: 抽象执行态基类继续承载共享底盘职责
- **WHEN** TCS 需要表达所有执行态共享的生命周期、参数、宿主上下文或 StateTree 驱动能力
- **THEN** 这些共享职责应继续保留在 `UTcsStateInstance` 上
- **AND** 但 concrete 业务树不应再直接以它作为长期业务 owner

#### Scenario: generic StateInstance concrete schema 被移除
- **WHEN** TCS 评估 concrete editor-facing schema 列表
- **THEN** 系统不应继续保留 `UTcsSTSchema_StateInstance`
- **AND** 也不应再引入任何新的 concrete `StateInstance` schema 作为长期兜底入口

### Requirement: BuffInstance StateTree 应使用独立的 Buff schema
TCS SHALL 为 `UTcsBuffInstance` 提供专用的 `UTcsSTSchema_Buff`，并将 BuffStateTree 的根上下文收敛到单一 `BuffInstance`。

#### Scenario: BuffInstance schema 只暴露 BuffInstance 根上下文
- **WHEN** 一个 BuffStateTree 运行在 `UTcsBuffInstance` 上
- **THEN** 该树的根上下文应只暴露 `BuffInstance`
- **AND** Buff 相关运行时引用应通过 `UTcsBuffInstance` 访问

#### Scenario: Buff schema 不依赖已删除的 generic StateInstance schema
- **WHEN** TCS 为 `UTcsBuffInstance` 设计专用 schema
- **THEN** 该 schema 不应再依赖任何 generic `StateInstance` schema 入口
- **AND** BuffStateTree 不应同时暴露抽象 `StateInstance` 与 concrete `BuffInstance` 这组双根上下文

### Requirement: StateComponent StateTree 应使用专用的组件 schema
TCS SHALL 为 `UTcsStateComponent` 提供专用的 `UTcsSTSchema_StateComponent`，并把组件树的根上下文收敛到单一 `UTcsStateComponent`。

#### Scenario: StateComponent schema 只暴露 TcsStateComponent 根上下文
- **WHEN** 一个面向 `UTcsStateComponent` 的 StateTree 被创建或执行
- **THEN** 该树的根上下文应只暴露 `UTcsStateComponent`
- **AND** 不应继续依赖临时的多上下文平铺约定

#### Scenario: StateComponent schema 需要支持 LinkSubTree
- **WHEN** 开发者在 `UTcsStateComponent` 对应的树中使用 `LinkSubTree`
- **THEN** 专用的 `UTcsSTSchema_StateComponent` 不应破坏该能力
- **AND** 组件 schema 的设计必须以保留 `LinkSubTree` 兼容性为前提

### Requirement: Skill 数据对象与 Skill 执行态应显式分离
TCS SHALL 将当前 learned skill 数据对象命名为 `UTcsSkillEntry`，并新增 `UTcsSkillInstance : UTcsStateInstance` 作为技能激活执行态。

#### Scenario: SkillEntry 不再表示执行态
- **WHEN** TCS 表达 SkillComponent 持有的 learned skill 数据对象
- **THEN** 该对象应使用 `UTcsSkillEntry` 命名
- **AND** 它不应继续承担一次技能激活执行态的语义

#### Scenario: SkillInstance 表示技能激活执行态
- **WHEN** TCS 表达一次技能激活进入 State 主链后的运行时对象
- **THEN** 该对象应使用 `UTcsSkillInstance : UTcsStateInstance`
- **AND** 它应作为 concrete 业务执行态类型进入 SkillStateTree

### Requirement: SkillStateTree 应使用独立的 Skill schema
TCS SHALL 为技能相关运行时树提供专用的 `UTcsSTSchema_Skill`，并以 `SkillInstance + SkillEntry` 两个根上下文表达执行态与 learned skill 数据对象的分工。

#### Scenario: Skill schema 使用 SkillInstance 与 SkillEntry 作为根上下文名
- **WHEN** 一个技能相关运行时树需要同时读取执行态与 learned skill 数据对象
- **THEN** 该 schema 应至少暴露 `SkillInstance` 与 `SkillEntry` 两个根上下文
- **AND** 其中 `SkillInstance` 表示当前技能激活执行态
- **AND** `SkillEntry` 明确只表示 SkillComponent 托管的 learned skill 数据对象

### Requirement: TCS StateTree 的 LinkedSubTree 支持应被视为统一方向
TCS SHALL 将“尽量让所有 TCS StateTree 支持 `LinkedSubTree`”视为统一方向；若当前 change 无法完整覆盖，则必须明确记录后续独立 proposal 的必要性。

#### Scenario: 当前 change 至少不应让新增 schema 主动阻断 LinkedSubTree 方向
- **WHEN** TCS 在本 change 中新增或重构专用 schema
- **THEN** 这些 schema 不应在设计层面主动排斥 `LinkedSubTree` 支持
- **AND** 至少已经确认的 `StateComponent` schema 兼容性要求必须被保留

#### Scenario: 全量 LinkedSubTree 支持范围过大时拆出后续 proposal
- **WHEN** TCS 评估后发现 `LinkedSubTree` 的全量支持需要跨多个 schema、节点契约或 capability 进行大范围实现
- **THEN** 本 change 应明确记录这项工作尚未完结
- **AND** 后续应新开独立 proposal 推进该目标

### Requirement: UTcsStateInstance 暴露 virtual PopulateStateParamInstances

`UTcsStateInstance` SHALL 新增 `virtual bool PopulateStateParamInstances(StateDef, Instigator, Target, OutFailedParams)`，从 Def 遍历 Parameters 创建并求值参数实例。`CreateStateInstance` 调用此方法替代旧的 inline 代码块。`UTcsSkillInstance` SHALL 覆写为空实现。

#### Scenario: 基类填充本地实例
- **WHEN** 普通 State/Buff 类型调用 PopulateStateParamInstances
- **THEN** Def->Parameters 逐个 Initialize → 内联求值 → 按类型分桶写入对应容器

#### Scenario: SkillInstance 跳过
- **WHEN** SkillInstance 调用 PopulateStateParamInstances
- **THEN** MUST 返回 true 且不写入任何本地容器

### Requirement: UTcsStateInstance 暴露 virtual GetStateParamInstance

`UTcsStateInstance` SHALL 新增按类型拆分的 virtual 方法：`GetNumericParamInstance(Tag)` / `GetBoolParamInstance(Tag)` / `GetVectorParamInstance(Tag)`，返回本地对应容器中的指针。`UTcsSkillInstance` SHALL 覆写指向 `Entry` 的对应容器。

#### Scenario: SkillInstance 指向 Entry
- **WHEN** 调用 SkillInstance 上的 GetNumericParamInstance(Tag)
- **THEN** MUST 返回 SkillEntry->NumericParamInstances.Find(Tag)

### Requirement: UTcsStateInstance 暴露 virtual GetXxxParamInstances

`UTcsStateInstance` SHALL 新增完整表访问器：`GetNumericParamInstances()` / `GetBoolParamInstances()` / `GetVectorParamInstances()`，返回对应 TMap 引用。`UTcsSkillInstance` SHALL 覆写指向 `Entry` 的对应容器。

#### Scenario: SkillInstance 返回 Entry 容器
- **WHEN** 调用 SkillInstance 上的 GetNumericParamInstances()
- **THEN** MUST 返回 SkillEntry->NumericParamInstances 引用

### Requirement: GetLevel 加 virtual

`UTcsStateInstance::GetLevel()` SHALL 加 `virtual`。`UTcsSkillInstance` SHALL 覆写返回 `Entry->GetLevel()`。

#### Scenario: SkillInstance 读取 Entry 的 Level
- **WHEN** LevelArray 求值器调 StateInstance->GetLevel()，实际类型为 UTcsSkillInstance
- **THEN** 虚函数分发 MUST 返回 SkillEntry->GetLevel()

### Requirement: 定义层应负责选择 concrete 运行时与数据类型
TCS SHALL 让定义层负责选择 concrete 运行时类型与 learned 数据类型，而不是把类型选择权散落到宿主组件或执行现场。

#### Scenario: BuffDefinition 负责声明 Buff concrete runtime 类型
- **WHEN** TCS 为 Buff 定义运行时实例类型
- **THEN** `UTcsBuffDefinition` 应提供 `BuffInstanceClass`
- **AND** 运行时实例解析应回到定义层统一完成

#### Scenario: SkillDefinition 负责声明 Skill concrete runtime 与 learned data 类型
- **WHEN** TCS 为 Skill 定义执行态类型与 learned skill 数据类型
- **THEN** `UTcsSkillDefinition` 应提供 `SkillInstanceClass` 与 `SkillEntryClass`
- **AND** 类型选择不应继续散落在 SkillComponent 或 State 执行态侧

