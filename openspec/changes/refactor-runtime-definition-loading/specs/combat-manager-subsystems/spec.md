## MODIFIED Requirements
### Requirement: State Manager Subsystem 的最终职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。原本残留在其上的全局 `StateInstanceId` 分配能力与 facade 语义 MUST 下沉或清零，Definition cache/load、Definition 查询与 registry 同步 SHALL 全部收敛到统一的运行时 Definition 加载归口。

#### Scenario: 枚举出的 public API 是穷尽集合
- **WHEN** 在归档后检查 `UTcsStateManagerSubsystem` 的 public 接口面时
- **THEN** 该子系统 MUST 已从 runtime 模块清零
- **AND** 任何残留的 Definition cache/load、Definition 查询、全局 ID 工厂或 facade public API 都属于违约

#### Scenario: 跨 Actor Buff facade 使用 BuffDefId 语义
- **WHEN** 检查迁移后的 public API 面时
- **THEN** 系统 MUST NOT 继续保留 `TryApplyStateToTarget(..., StateDefId, ...)` 这类跨 Actor facade
- **AND** Buff apply 主路径 MUST 通过更贴近 Buff 语义的组件或 gameplay API 承担

#### Scenario: 跨 Actor facade 只做委托
- **WHEN** 评估是否还需要单独的 StateManager facade 时
- **THEN** 若该 facade 无外部调用方，则 SHOULD 直接删除而不是继续保留空壳委托层
- **AND** 被删除后不得再以兼容性包装器的形式残留在 public API 面中

### Requirement: Attribute Manager Subsystem 的最终职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。其残余的 tag 解析、全局 ID 工厂与 SourceHandle 工厂职责 MUST 下沉到更贴近使用点的组件或 `UTcsDefinitionManagerSubsystem`，Definition cache/load 与 `AttributeDef` / `AttributeModifierDef` 查询 SHALL 统一收敛到运行时 Definition 管理层。

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

#### Scenario: SourceHandle 工厂下沉到更贴近使用点的实现
- **WHEN** 调用方构造一个新的 `FTcsSourceHandle`
- **THEN** 该入口 MUST 位于实际拥有状态生命周期的组件或其紧邻实现中
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `CreateSourceHandle` 入口
