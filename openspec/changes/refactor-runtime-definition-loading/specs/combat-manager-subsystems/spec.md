## MODIFIED Requirements
### Requirement: State Manager Subsystem 的最终职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。原本残留在其上的全局 `StateInstanceId` 分配能力与 facade 语义 MUST 下沉或清零，Definition cache/load 与 Definition 查询 SHALL 全部收敛到统一的运行时 Definition 加载归口；编辑器期 registry 同步 MUST 保持 editor-only 职责，不得转移给 runtime `UTcsDefinitionManagerSubsystem`。

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

### Requirement: ID 计数器只存在于 Subsystem 中

迁移完成后，Attribute、Modifier 与 ModifierChangeBatch 的进程级唯一 ID SHALL 由 `UTcsAttributeComponent` 内部静态工厂分配。`UTcsAttributeManagerSubsystem` 不再存在，其他组件或 runtime subsystem MUST NOT 持有这些 ID 工厂的并行副本。

#### Scenario: Component 静态工厂分配 Attribute 相关 ID
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId`、`ModifierInstId` 或 `ModifierChangeBatchId`
- **THEN** 它 MUST 使用自身的静态分配入口完成分配
- **AND** 分配结果 MUST 在当前进程内保持全局唯一

#### Scenario: 已删除 Manager 不再持有计数器
- **WHEN** 在 TCS runtime 源码中搜索 `UTcsAttributeManagerSubsystem`、`GlobalAttributeInstanceIdMgr`、`GlobalAttributeModifierInstanceIdMgr` 或 `GlobalAttributeModifierChangeBatchIdMgr`
- **THEN** 搜索结果 MUST 不包含已删除 Manager 的声明、实现或等价计数器

### Requirement: Subsystem 到 Component 不再保留 Friend 访问

迁移完成后，Component 类 SHALL NOT 为已删除的 `UTcsStateManagerSubsystem` 或 `UTcsAttributeManagerSubsystem` 保留 friendship、public bridge 或等价后门。组件内部实现 MAY 为实际拥有生命周期的运行时对象保留最小必要的内部访问，但不得用于复活已删除 Manager 的职责。

#### Scenario: 已删除 Manager 的 friend 声明已清零
- **WHEN** 在 TCS runtime 源码中搜索 `friend class UTcsStateManagerSubsystem` 或 `friend class UTcsAttributeManagerSubsystem`
- **THEN** 搜索结果 MUST 为空

#### Scenario: 不引入已删除 Manager 的等价后门
- **WHEN** 审查 runtime Component 的访问边界时
- **THEN** 系统 MUST NOT 为已删除 Manager 新增 friend 声明、只服务于该 Manager 的 public bridge，或通过子类中转的 protected 后门

## REMOVED Requirements
### Requirement: Subsystem 保持 GameInstanceSubsystem 作用域
**Reason**: `UTcsStateManagerSubsystem` 与 `UTcsAttributeManagerSubsystem` 已从 runtime 模块删除，继续约束其 subsystem scope 没有运行时事实基础。
**Migration**: 统一 Definition cache/load 由 `UGameInstanceSubsystem` `UTcsDefinitionManagerSubsystem` 承担；State 与 Attribute 运行时实例 ID 由对应 Component 的静态工厂承担。
