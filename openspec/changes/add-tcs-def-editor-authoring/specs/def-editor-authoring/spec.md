## ADDED Requirements

### Requirement: 插件自有的 DefinitionAsset Authoring 入口
TCS 编辑器集成 SHALL 为每一个规范、且应直接 authoring 的 TCS DefinitionAsset 类型暴露插件自有的 Content Browser 创建入口。

#### Scenario: 从 TCS 分类中创建 AttributeDef
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsAttributeDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 AttributeModifierDef
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsAttributeModifierDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 BuffDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsBuffDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

#### Scenario: 从 TCS 分类中创建 StateSlotDefinition
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 开发者可以从 TCS 自有分类中创建 `UTcsStateSlotDefinition`
- **AND** 整个流程不需要先选择通用 `Data Asset`，再额外选择父类

### Requirement: 不暴露损坏的抽象 DefinitionAsset 入口
TCS 编辑器集成 SHALL 不把抽象 DefinitionAsset 类暴露成损坏的直接创建目标。

#### Scenario: 抽象 StateDefinition 与编辑器 authoring 保持一致
- **WHEN** `UTcsStateDefinition` is abstract in runtime code
- **THEN** 插件不应暴露一个最终解析到 `UTcsStateDefinition` 的直接创建入口
- **AND** 那些本就应直接 authoring 的具体 state 侧 DefinitionAsset 类型仍应在 TCS 分类下可见

### Requirement: 保持运行时 Def 契约不变
TCS 编辑器 authoring 集成 SHALL 保持现有基于 `UPrimaryDataAsset` 的运行时 Def 资产契约不变。

#### Scenario: 通过编辑器创建的 Def 继续兼容 AssetManager
- **WHEN** 开发者通过插件自有的编辑器入口创建一个 Def 资产
- **THEN** 创建出的资产仍然是基于 `UPrimaryDataAsset` 的 TCS Def 资产
- **AND** 既有的 `PrimaryAssetId` 与 `AssetManager` 加载行为继续有效

### Requirement: 以组合优先作为 Authoring 方向
TCS 编辑器 authoring 集成 SHALL 不把 Def subclassing 作为主要扩展模型。

#### Scenario: 基础 Def 类型是默认 authoring 路径
- **WHEN** 插件注册其内建的 Def 资产创建入口时
- **THEN** 这些入口应创建 TCS 期望开发者直接 authoring 的规范内建 DefinitionAsset 类型
- **AND** 下游团队在正常 TCS authoring 中不应被迫使用 Def subclassing

### Requirement: 结构化的 TCS Authoring 菜单
TCS 编辑器 authoring 集成 SHALL 将插件自有创建入口组织到 `Tirefly Combat System` 下的稳定子菜单中。

#### Scenario: Definition 资产归入 Definition Asset 子菜单
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 每个规范 TCS DefinitionAsset 创建入口都出现在 `Tirefly Combat System -> Definition Asset` 下

#### Scenario: Gameplay runtime 资产归入 Gameplay Runtime 子菜单
- **WHEN** 开发者打开 Content Browser 的添加资产菜单
- **THEN** 受支持的 runtime authoring 入口都出现在 `Tirefly Combat System -> Gameplay Runtime` 下

### Requirement: TCS Gameplay Runtime Authoring 入口
TCS 编辑器集成 SHALL 为 TCS 开发者需要直接 authoring 的那一小部分 gameplay runtime 资产暴露插件自有创建入口。

#### Scenario: 为 TcsStateComponent 创建 StateTree
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 StateTree
- **THEN** 插件应暴露一个面向组件的 StateTree 入口
- **AND** 创建出的资产应使用 `UStateTreeComponentSchema`
- **AND** 整个流程不需要 schema picker

#### Scenario: 为 TcsStateInstance 创建 StateTree
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 StateTree
- **THEN** 插件应暴露一个面向 StateInstance 的 StateTree 入口
- **AND** 创建出的资产应使用 `UTcsStateTreeSchema_StateInstance`
- **AND** 整个流程不需要 schema picker

#### Scenario: 创建 SkillInstance Blueprint
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 Blueprint
- **THEN** 插件应暴露一个 SkillInstance Blueprint 入口
- **AND** 创建出的资产应是 `UTcsSkillInstance` 的 Blueprint 子类
- **AND** 整个流程不需要 parent-class picker

### Requirement: 为未来 TCS Authoring 保留稳定扩展路径
TCS 编辑器 authoring capability SHALL 为后续的 StateComponent schema 和 Skill authoring 扩展保留稳定升级点，而不是把它们分裂成互不相关的菜单路径。

#### Scenario: State Component StateTree 入口是未来的升级点
- **WHEN** 当前阶段尚未提供专用的 `UTcsStateTreeSchema_StateComponent`
- **THEN** `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 入口可以临时创建基于 `UStateTreeComponentSchema` 的资产
- **AND** 如果未来新增专用 TCS 组件 schema，这个同一入口仍应是预期的迁移目标

#### Scenario: Skill authoring 沿用同一 capability 扩展
- **WHEN** `SkillDef` 将来变为可直接资产化的类型时
- **THEN** TCS 编辑器创作面应沿用同一个插件自有能力与菜单结构继续扩展
- **AND** Skill 创作流程不应被拆成另一条无关的编辑器创作路径
