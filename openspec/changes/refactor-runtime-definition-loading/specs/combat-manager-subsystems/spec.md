## MODIFIED Requirements
### Requirement: State Manager Subsystem 的最终职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 严格只暴露以下职责，且不得多于这些：全局 state instance ID 工厂（`AllocateStateInstanceId`）以及必要的跨 Actor 门面。Definition cache/load、Definition 查询与 registry 同步 SHALL 从 `UTcsStateManagerSubsystem` 中移出，并收敛到统一的运行时 Definition 加载归口。

#### Scenario: 枚举出的 public API 是穷尽集合
- **WHEN** 在归档后检查 `UTcsStateManagerSubsystem` 的 public 接口面时
- **THEN** 每一个声明出的 public 方法 MUST 属于“全局 ID 工厂”或“跨 Actor 门面”二者之一；任何 Definition cache/load、Definition 查询或 registry 同步相关 public API 都属于违约

#### Scenario: 跨 Actor Buff facade 使用 BuffDefId 语义
- **WHEN** 保留一个跨 Actor Buff apply facade 时
- **THEN** 它的 public surface SHOULD 采用类似 `TryApplyBuffToTarget(TargetActor, BuffDefId, Instigator, StateLevel, ParentSourceHandle)` 的命名与参数语义
- **AND** 它 MUST NOT 继续以 `TryApplyStateToTarget(..., StateDefId, ...)` 的形式对外泄露旧的抽象 `StateDef` 语义

#### Scenario: 跨 Actor facade 只做委托
- **WHEN** 跨 Actor Buff apply facade 被调用
- **THEN** 它 MUST 只做四件事：(1) 校验 `TargetActor`，(2) 通过 `ITcsEntityInterface` 解析 `UTcsStateComponent`，(3) 调用对应的 `StateComp` apply 入口，(4) 返回结果
- **AND** 该 facade 中 MUST NOT 出现任何 Definition 解析、状态实例构造、参数求值或槽位逻辑

### Requirement: Attribute Manager Subsystem 的最终职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 严格只暴露以下职责，且不得多于这些：runtime-ready 诊断、attribute tag/name 解析、全局 ID 工厂，以及 SourceHandle 工厂。Definition cache/load 与 `AttributeDef` / `AttributeModifierDef` 查询 SHALL 从 `UTcsAttributeManagerSubsystem` 中移出，并收敛到统一的运行时 Definition 加载归口。

#### Scenario: 枚举出的 public API 是穷尽集合
- **WHEN** 在归档后检查 `UTcsAttributeManagerSubsystem` 的 public 接口面时
- **THEN** 每一个声明出的 public 方法 MUST 属于“runtime-ready 诊断”“attribute tag/name 解析”“全局 ID 工厂”或“SourceHandle 工厂”四者之一
- **AND** 任何 Definition cache/load 或 `AttributeDef` / `AttributeModifierDef` 查询 public API 都属于违约

#### Scenario: Attribute Manager 不再解析 Definition
- **WHEN** `UTcsAttributeComponent` 或其他调用方需要解析 `AttributeDef` / `AttributeModifierDef`
- **THEN** 该查询 MUST 通过统一的运行时 Definition 加载归口完成
- **AND** `UTcsAttributeManagerSubsystem` MUST NOT 再暴露 `GetAttributeDefinition` / `GetModifierDefinition` 这类 Definition 查询入口

#### Scenario: Tag 到 Name 的解析仍然集中化
- **WHEN** 任意调用方需要从 `FGameplayTag` 解析 attribute 名称时
- **THEN** MUST 使用 `UTcsAttributeManagerSubsystem::TryResolveAttributeNameByTag` / `TryGetAttributeTagByName`
- **AND** 不提供 per-component 的 tag 解析接口

#### Scenario: SourceHandle 工厂保持全局入口
- **WHEN** 调用方构造一个新的 `FTcsSourceHandle`
- **THEN** `UTcsAttributeManagerSubsystem::CreateSourceHandle(CausalityChain, Instigator, SourceTags)` MUST 作为唯一入口
- **AND** `UTcsAttributeComponent` MUST NOT 暴露等价接口
