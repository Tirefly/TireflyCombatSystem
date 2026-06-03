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
- **AND** 创建出的资产应使用 `UTcsSTSchema_StateComponent`
- **AND** 整个流程不需要 schema picker

#### Scenario: 为 Buff 创建 StateTree
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 StateTree
- **THEN** 插件应暴露一个面向 `UTcsBuffInstance` 的 Buff StateTree 入口
- **AND** 创建出的资产应使用 `UTcsSTSchema_Buff`
- **AND** 整个流程不需要 schema picker

#### Scenario: 不再暴露 generic StateInstance StateTree
- **WHEN** `UTcsStateInstance` 被确认为抽象共享执行态基类，且 generic `StateInstance` schema 已删除
- **THEN** 插件不应继续把 generic `StateInstance StateTree` 当作 gameplay runtime authoring 入口
- **AND** 当前运行时树入口应收敛到 concrete runtime owner
- **AND** 受支持的 concrete runtime 入口仍不应要求 schema picker

#### Scenario: 创建 learned-skill data Blueprint
- **WHEN** 开发者从 `Tirefly Combat System -> Gameplay Runtime` 创建一个 Blueprint
- **THEN** 插件应暴露一个 learned-skill data Blueprint 入口
- **AND** 创建出的资产应为 `UTcsSkillEntry` 的 Blueprint 子类
- **AND** 整个流程不需要 parent-class picker

### Requirement: 为未来 TCS Authoring 保留稳定扩展路径
TCS 编辑器 authoring capability SHALL 为后续的 StateComponent schema 和 Skill authoring 扩展保留稳定升级点，而不是把它们分裂成互不相关的菜单路径。

#### Scenario: State Component StateTree 入口沿用专用 schema 升级
- **WHEN** 当前阶段已经提供专用的 `UTcsSTSchema_StateComponent`
- **THEN** `Tirefly Combat System -> Gameplay Runtime -> State Component StateTree` 入口应创建基于该 schema 的资产
- **AND** 如果未来继续扩展组件树 authoring，这个同一入口仍应是预期的升级目标

#### Scenario: 过渡性的 generic StateTree 入口需要迁移
- **WHEN** `refactor-state-runtime-access-contract` 删除 generic `StateInstance` schema
- **THEN** 现有 `Gameplay Runtime` 中的过渡性 generic StateTree 入口应被移除或改造成 concrete runtime owner 入口
- **AND** editor authoring 面不应继续把抽象共享运行时类型暴露成稳定创建目标

#### Scenario: Skill authoring 沿用同一 capability 扩展
- **WHEN** `SkillDef` 将来变为可直接资产化的类型时
- **THEN** TCS 编辑器创作面应沿用同一个插件自有能力与菜单结构继续扩展
- **AND** Skill 创作流程不应被拆成另一条无关的编辑器创作路径

### Requirement: 编辑器阶段 AssetManagerSettings 覆盖勘误
TCS 编辑器 authoring 集成 SHALL 在编辑器阶段检测 `AssetManagerSettings` 对 TCS DefinitionAsset 的覆盖完整性，并在漏配时通过错误日志与编辑器通知提供明确勘误提示。

#### Scenario: 检测到 Definition 类型或扫描路径漏配
- **WHEN** `PrimaryAssetTypesToScan` 未正确覆盖 `UTcsAttributeDefinition`、`UTcsAttributeModifierDefinition`、`UTcsStateDefinition`、`UTcsStateSlotDefinition` 对应的类型或扫描目录
- **THEN** 编辑器应输出可读勘误信息，明确缺失的 PrimaryAssetType 与扫描路径
- **AND** 该勘误信息至少应同时出现在错误日志与编辑器通知中
- **AND** 勘误信息应可用于直接指导开发者修正工程配置

#### Scenario: 类型与路径漏配必须分别可见
- **WHEN** 某个 TCS DefAsset 类型已存在于 `PrimaryAssetTypesToScan`，但其扫描目录配置错误或缺失
- **THEN** 编辑器仍应报错，且该错误不得被“类型已存在”判定掩盖
- **AND** 报错信息应明确这是路径覆盖问题

#### Scenario: DevSettings 忽略列表可以抑制指定类型报错
- **WHEN** 某个 TCS DefAsset 类型被加入 `UTcsDeveloperSettings` 的勘误忽略列表
- **THEN** 该类型的类型漏配或路径漏配不应再产生勘误报错
- **AND** 其他未被忽略类型的漏配检测继续生效

#### Scenario: 勘误校验不改写工程配置
- **WHEN** 编辑器执行 `AssetManagerSettings` 覆盖勘误检查
- **THEN** 该检查应只报告配置问题，不自动改写项目 `AssetManagerSettings`

#### Scenario: 修复漏配后勘误消失
- **WHEN** 开发者按提示补齐 `AssetManagerSettings` 的缺失类型与扫描目录
- **THEN** 后续勘误检查不应再报告同一漏配项
- **AND** 既有 Definition 资产同步与加载行为保持有效

#### Scenario: 未修复漏配在常用 Save 入口重复提示
- **WHEN** 编辑器中仍存在未忽略且未修复的 DefAsset 漏配项
- **AND** 开发者执行普通单资产/单包保存、主窗口/快捷键 `Save All` 或 Content Browser 顶部工具栏 `Save All`
- **THEN** 编辑器应再次输出对应勘误提示
- **AND** 该重复提示应继续同时使用错误日志与编辑器通知两条通道
- **AND** 该重复提示行为持续到漏配被修复或该类型被加入忽略列表
