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
