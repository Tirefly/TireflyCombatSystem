## ADDED Requirements

### Requirement: 编辑器期实时 Definition 感知
TCS SHALL 在编辑器会话期间维护一份权威的实时 Def 快照，使 Def 变化无需重启编辑器即可被观察到。

#### Scenario: 新建 AttributeDef 在同一编辑器会话内可见
- **WHEN** 开发者在编辑器中新建一个 AttributeDef 资产
- **THEN** 权威 TCS Def 快照会在同一编辑器会话内刷新
- **AND** 新的 AttributeDef 会在无需重启编辑器的前提下，对编辑器期 TCS 加载路径可见

#### Scenario: 具体 state-side Definition 的更新在同一编辑器会话内可见
- **WHEN** 开发者在编辑器中修改并保存一个具体的 state-side DefinitionAsset（例如 `UTcsBuffDefinition`，或任意派生自 `UTcsStateDefinition` 的资产）
- **THEN** 权威 TCS Def 快照会在同一编辑器会话内刷新
- **AND** 更新后的 state-side DefinitionAsset 会在无需重启编辑器的前提下，对编辑器期 TCS 加载路径可见

### Requirement: Subsystem 实时同步
TCS 管理子系统在编辑器期 Def registry 刷新时 SHALL 从最新的权威 Def 快照中重建。

#### Scenario: Attribute 管理子系统响应 Def 刷新
- **WHEN** 权威 TCS Def 快照在编辑器中刷新时
- **THEN** `UTcsAttributeManagerSubsystem` 会从刷新后的快照中重建其 definition 与 tag 查询状态

#### Scenario: State 管理子系统响应 Def 刷新
- **WHEN** 权威 TCS Def 快照在编辑器中刷新时
- **THEN** `UTcsStateManagerSubsystem` 会从刷新后的快照中重建其 slot / state-side definition 查询状态
- **AND** 该重建会保留已配置的 State 加载策略语义

### Requirement: DeveloperSettings 兼容视图
即使实时缓存的归属被迁移到专用 registry 中，TCS 仍 SHALL 通过 `UTcsDeveloperSettings` 保留一份面向兼容的缓存视图。

#### Scenario: DeveloperSettings 视图反映最新快照
- **WHEN** 权威 TCS Def 快照在编辑器中刷新时
- **THEN** `UTcsDeveloperSettings` 会暴露一份反映最新刷新快照的缓存视图
- **AND** 仍然读取 settings 缓存的调用方无需重启编辑器即可看到 Def 变化

### Requirement: 派生 Def 兼容性
TCS SHALL 按基类契约发现 Def 资产，而不是按精确类相等进行匹配。

#### Scenario: 派生自 `UTcsStateDefinition` 的具体定义资产能建立索引
- **WHEN** 某个项目定义了一个派生自 `UTcsStateDefinition` 的具体资产类（例如项目自定义 BuffDefinition）
- **AND** 该派生类的某个资产存在于被扫描的 Def 根路径下
- **THEN** 实时 TCS Def registry 会将其索引为有效的 state-side DefinitionAsset
- **AND** State 管理子系统能通过基类 Def 契约从中完成重建

#### Scenario: 派生 AttributeDef 能建立索引
- **WHEN** 某个项目定义了一个派生自 `UTcsAttributeDefinition` 的资产类
- **AND** 该派生类的某个资产存在于被扫描的 Def 根路径下
- **THEN** 实时 TCS Def registry 会将其索引为有效 AttributeDef 资产
- **AND** Attribute 管理子系统能通过基类 Def 契约从中完成重建
