# combat-manager-subsystems Specification

## Purpose
记录 TCS 全局 manager / subsystem 职责边界的收敛结果：`UTcsStateManagerSubsystem` 与 `UTcsAttributeManagerSubsystem` 已从 runtime 删除；统一 Definition cache/load 与类型化查询由 `UTcsDefinitionManagerSubsystem` 承担；State / Attribute 的进程级 ID 工厂下沉到对应 Component 静态工厂。
## Requirements
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

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。其残余的 tag 解析与全局 Attribute / Modifier ID 工厂职责 MUST 下沉到对应组件或 `UTcsDefinitionManagerSubsystem`；SourceHandle 创建职责 MUST 从 Manager / Component / DefinitionManager 中剥离，并统一收敛到共享静态 `FTcsSourceHandleFactory`；Definition cache/load 与 `AttributeDef` / `AttributeModifierDef` 查询 SHALL 统一收敛到运行时 Definition 管理层。

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
- **WHEN** `UTcsAttributeComponent` 需要分配新的 `AttributeInstId`、`ModifierInstId` 或 `ModifierChangeBatchId`
- **THEN** 它 MUST 使用自身的静态分配入口完成分配
- **AND** 分配结果 MUST 在当前进程内保持全局唯一
- **AND** 已删除的 `UTcsAttributeManagerSubsystem` MUST NOT 再持有 `GlobalAttributeInstanceIdMgr` / `GlobalAttributeModifierInstanceIdMgr` / `GlobalAttributeModifierChangeBatchIdMgr` 或等价计数器

### Requirement: Subsystem 到 Component 不再保留 Friend 访问

迁移完成后，Component 类 SHALL NOT 为已删除的 `UTcsStateManagerSubsystem` 或 `UTcsAttributeManagerSubsystem` 保留 friendship、public bridge 或等价后门。组件内部实现 MAY 为实际拥有生命周期的运行时对象保留最小必要的内部访问，但不得用于复活已删除 Manager 的职责。

#### Scenario: 已删除 Manager 的 friend 声明已清零
- **WHEN** 在 TCS runtime 源码中搜索 `friend class UTcsStateManagerSubsystem` 或 `friend class UTcsAttributeManagerSubsystem`
- **THEN** 搜索结果 MUST 为空

#### Scenario: 不引入已删除 Manager 的等价后门
- **WHEN** 审查 runtime Component 的访问边界时
- **THEN** 系统 MUST NOT 为已删除 Manager 新增 friend 声明、只服务于该 Manager 的 public bridge，或通过子类中转的 protected 后门

