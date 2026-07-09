## MODIFIED Requirements
### Requirement: Attribute 定义通过 Manager 解析

attribute 与 modifier 的定义查询 SHALL 统一经过 `UTcsDefinitionManagerSubsystem` 的类型化 Definition 查询入口。Components MUST NOT 在本地缓存或复制 definition registry，也 MUST NOT 继续通过 `UTcsAttributeManagerSubsystem` 解析 `AttributeDef` / `AttributeModifierDef`。

#### Scenario: AddAttribute 从 DefinitionManager 解析定义
- **WHEN** `UTcsAttributeComponent::AddAttribute(Name, InitValue)` executes
- **THEN** `UTcsAttributeDefinition` MUST 通过统一的运行时 Definition 加载归口获取
- **AND** 它 MUST NOT 通过 component 本地 map 或 `UTcsAttributeManagerSubsystem::GetAttributeDefinition(Name)` 获取

### Requirement: Attribute Manager Subsystem 只保留全局职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 只暴露 runtime-ready 诊断、attribute tag/name 解析、全局 ID 工厂，以及全局 `CreateSourceHandle` 工厂。Definition cache/load 与 `AttributeDef` / `AttributeModifierDef` 查询 SHALL 由统一的运行时 Definition 加载归口承担。

#### Scenario: Tag 到 Name 的解析仍然集中化
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag` / `TryGetAttributeTagByName`
- **AND** 不提供 per-component 的 tag 解析接口

#### Scenario: SourceHandle 工厂保持全局入口
- **WHEN** 调用方构造一个新的 `FTcsSourceHandle`
- **THEN** `UTcsAttributeManagerSubsystem::CreateSourceHandle(CausalityChain, Instigator, SourceTags)` MUST 作为唯一入口
- **AND** `UTcsAttributeComponent` MUST NOT 暴露等价接口

#### Scenario: AttributeManager 不再提供 Definition 查询
- **WHEN** 调用方需要解析 `AttributeDef` 或 `AttributeModifierDef`
- **THEN** MUST 使用统一的运行时 Definition 加载归口
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 继续暴露 `GetAttributeDefinition` / `GetModifierDefinition`
