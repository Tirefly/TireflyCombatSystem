## MODIFIED Requirements

### Requirement: Attribute Manager Subsystem 的最终职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。其残余的 tag 解析与全局 Attribute / Modifier ID 工厂职责 MUST 下沉到对应组件或 `UTcsDefinitionManagerSubsystem`；SourceHandle 创建职责 MUST 从 Manager / Component / DefinitionManager 中剥离，并统一收敛到共享静态 `FTcsSourceHandleFactory`；Definition cache/load 与 `AttributeDef` / `AttributeModifierDef` 查询 SHALL 统一收敛到运行时 Definition 管理层。Attribute / Ongoing Modifier 的进程级 ID 工厂 MUST 只覆盖 `AttributeInstId` 与 `ModifierInstId`，MUST NOT 再分配 `ModifierChangeBatchId`。

#### Scenario: 枚举出的 public API 是穷尽集合
- **WHEN** 在归档后检查 `UTcsAttributeManagerSubsystem` 的 public 接口面时
- **THEN** 该子系统 MUST 已从 runtime 模块清零
- **AND** 任何残留的 runtime-ready 诊断、tag/name 解析、全局 ID 工厂、SourceHandle 工厂或 Definition 查询 public API 都属于违约

#### Scenario: Attribute Manager 不再解析 Definition
- **WHEN** `UTcsAttributeComponent` 或其他调用方需要解析 `AttributeDef` / `AttributeModifierDef`
- **THEN** 该查询 MUST 通过统一的运行时 Definition 加载归口完成
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `GetAttributeDefinition` / `GetModifierDefinition` 这类 Definition 查询入口

#### Scenario: Tag 解析迁移到 DefinitionManager
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 Attribute 语义标识时
- **THEN** MUST 使用 `UTcsDefinitionManagerSubsystem` 提供的 Attribute tag 查询入口
- **AND** 系统 MUST NOT 继续通过 `UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag` / `TryGetAttributeTagByName` 提供同类 public API

#### Scenario: SourceHandle 工厂统一到共享静态工厂
- **WHEN** 调用方构造一个新的有效 `FTcsSourceHandle`
- **THEN** 该入口 MUST 是共享静态 `FTcsSourceHandleFactory`
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `CreateSourceHandle` 入口
- **AND** `UTcsDefinitionManagerSubsystem`、`UTcsAttributeComponent` 与 `UTcsStateComponent` MUST NOT 成为 SourceHandle 分配器

#### Scenario: ID 计数器下沉到 Component 静态工厂
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId` 或 `ModifierInstId`
- **THEN** 它 MUST 使用自身的静态分配入口完成分配
- **AND** 分配结果 MUST 在当前进程内保持全局唯一
- **AND** 已删除的 `UTcsAttributeManagerSubsystem` MUST NOT 再持有 `GlobalAttributeInstanceIdMgr` / `GlobalAttributeModifierInstanceIdMgr` / `GlobalAttributeModifierChangeBatchIdMgr` 或等价计数器
- **AND** 系统 MUST NOT 再分配 `ModifierChangeBatchId`
