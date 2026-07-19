## MODIFIED Requirements
### Requirement: Attribute 定义通过 Manager 解析

attribute 与 modifier 的定义查询 SHALL 统一经过 `UTcsDefinitionManagerSubsystem` 的类型化 Definition 查询入口。Components MUST NOT 在本地缓存或复制 definition registry，也 MUST NOT 继续通过 `UTcsAttributeManagerSubsystem` 解析 `AttributeDef` / `AttributeModifierDef`。

#### Scenario: AddAttribute 从 DefinitionManager 解析定义
- **WHEN** `UTcsAttributeComponent::AddAttribute(Name, InitValue)` executes
- **THEN** `UTcsAttributeDefinition` MUST 通过统一的运行时 Definition 加载归口获取
- **AND** 它 MUST NOT 通过 component 本地 map 或 `UTcsAttributeManagerSubsystem::GetAttributeDefinition(Name)` 获取

### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。attribute 与 modifier 的 tag 解析、Definition 查询与相关运行时索引 SHALL 收敛到 `UTcsDefinitionManagerSubsystem`；Attribute / Modifier 的运行时实例 ID 工厂 SHALL 下沉到 `UTcsAttributeComponent` 内部静态工厂。

#### Scenario: Tag 到 AttributeDefId 的解析集中在 DefinitionManager
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsDefinitionManagerSubsystem` 提供的 Attribute tag 查询或解析接口
- **AND** `UTcsAttributeComponent` MUST NOT 在本地缓存或复制全局 tag 映射

#### Scenario: Attribute 与 Modifier 的全局 ID 工厂下沉到 Component
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId`、`ModifierInstId` 或 `ModifierChangeBatchId`
- **THEN** 它 MUST 通过自身持有的静态工厂完成分配
- **AND** 分配出的 ID MUST 在当前进程内保持全局唯一

#### Scenario: AttributeManager 不再提供 Definition 查询
- **WHEN** 调用方需要解析 `AttributeDef` 或 `AttributeModifierDef`
- **THEN** MUST 使用统一的运行时 Definition 加载归口
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 继续暴露 `GetAttributeDefinition` / `GetModifierDefinition`

#### Scenario: SourceHandle 工厂不再挂在 AttributeManager 上
- **WHEN** State 运行时创建新的 `FTcsSourceHandle`
- **THEN** 该工厂 MUST 位于更贴近 State 生命周期的实现侧
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `CreateSourceHandle` 入口

### Requirement: Modifier 创建通过 Manager 获取全局 ID

`UTcsAttributeComponent` 中凡是需要进程级唯一 ID 的方法 SHALL 通过自身静态工厂分配 `AttributeInstId`、`ModifierInstId` 与 `ModifierChangeBatchId`。已删除的 `UTcsAttributeManagerSubsystem` MUST NOT 继续承担 ID 分配职责，Component 实例之间也 MUST NOT 各自维护独立计数器。

#### Scenario: CreateAttributeModifier 从 Component 静态工厂分配 ID
- **WHEN** `UTcsAttributeComponent::CreateAttributeModifier` 创建新的 `FTcsAttributeModifierInstance`
- **THEN** 其实例 ID MUST 来自 `UTcsAttributeComponent` 的静态分配入口
- **AND** 该入口 MUST 在当前进程内保持全局唯一

#### Scenario: 已删除 AttributeManager 不再分配 ID
- **WHEN** 在 TCS runtime 源码中搜索 `ResolveAttributeManager`、`UTcsAttributeManagerSubsystem` 或旧 Manager ID 计数器
- **THEN** 搜索结果 MUST 不包含 Attribute / Modifier ID 分配实现

## REMOVED Requirements
### Requirement: Deprecated 兼容层必须严格作为临时过渡
**Reason**: `UTcsAttributeManagerSubsystem` 已被删除，不再存在需要保留或移除的 Manager 兼容包装器。
**Migration**: Actor 本地 Attribute 业务直接使用 `UTcsAttributeComponent`；Definition 与 tag 解析使用 `UTcsDefinitionManagerSubsystem`。
