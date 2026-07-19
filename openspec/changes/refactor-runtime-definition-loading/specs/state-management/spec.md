## MODIFIED Requirements
### Requirement: State Manager Subsystem 只保留全局职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 不再作为独立 runtime 子系统存在。State 运行时所需的全局 `StateInstanceId` 分配能力 SHALL 下沉到 `UTcsStateComponent` 内部静态工厂；Definition cache/load、Definition 查询与运行时 Definition source cache 归口 SHALL 全部由 `UTcsDefinitionManagerSubsystem` 承担。

#### Scenario: Definition 查询不再由 StateManager 提供
- **WHEN** 任意调用方需要通过 `FName` 或 `FGameplayTag` 解析具体 State-like Definition 或 `UTcsStateSlotDefinition` 时
- **THEN** 该查询 MUST 通过统一的运行时 Definition 加载归口完成
- **AND** `UTcsStateManagerSubsystem` MUST NOT 再暴露 `GetStateDefinition` / `GetStateDefinitionByTag` / `GetStateSlotDefinition` / `GetStateSlotDefinitionByTag` / `GetAllStateDefNames`

#### Scenario: StateDefId 查询语义只允许留在 State 模块内部
- **WHEN** 系统仍保留 `StateDefId` 相关查询或标识语义
- **THEN** 它 MUST 只服务于 `State` 模块内部单个 state definition 在 `StateComponent` 上的运行生命周期
- **AND** Buff 相关 public API MUST NOT 再对外暴露 `StateDefId`

#### Scenario: 全局 StateInstanceId 工厂下沉到 StateComponent
- **WHEN** `UTcsStateComponent` 需要一个新的全局唯一 `StateInstanceId`
- **THEN** 它 MUST 通过 `UTcsStateComponent` 自身持有的静态工厂分配该 ID
- **AND** 分配出的 `StateInstanceId` MUST 在当前进程内保持全局唯一

#### Scenario: 跨 Actor facade 已清零
- **WHEN** 检查最终 public API 面时
- **THEN** `UTcsStateManagerSubsystem` MUST NOT 再保留任何跨 Actor apply facade
- **AND** 系统 MUST NOT 继续以 `TryApplyStateToTarget(..., StateDefId, ...)` 之类的接口对外暴露抽象 StateDef 语义

#### Scenario: Buff apply 主路径按 DefId 驱动
- **WHEN** 调用方要施加一个 Buff，且手里只有 `BuffDefId`
- **THEN** 主执行路径 MUST 允许直接按 DefId 解析并继续 apply
- **AND** 不得要求调用方先显式持有已加载的 `UTcsBuffDefinition*`

#### Scenario: Buff Definition 解析失败时不得部分 apply
- **WHEN** Buff apply 主路径按 `BuffDefId` 解析 Definition 失败
- **THEN** 该次 apply MUST 明确失败
- **AND** 系统 MUST NOT 创建部分初始化的 runtime state、占位 buff 或半完成的 apply 副作用

#### Scenario: Buff 对象型逻辑若残留只能作为内部辅助实现
- **WHEN** 迁移阶段内部仍存在对象型 Buff apply 转发逻辑
- **THEN** 这些逻辑 MUST NOT 继续作为 public API 暴露
- **AND** 它们 MUST 只服务于内部参数转换与 DefId 主路径收口

### Requirement: Manager 缓存在 BeginPlay 解析并带有诊断安全网

`UTcsStateComponent` SHALL NOT 缓存、解析或重新获取已删除的 `UTcsStateManagerSubsystem`。当 State runtime prepare、StateSlot 构建或按 `StateDefId` 解析确实需要 Definition 时，组件 MUST 通过当前 `UGameInstance` 获取 `UTcsDefinitionManagerSubsystem`，并使用其 State 模块内部聚合查询或具体类型化查询完成解析。

#### Scenario: State runtime 只依赖 DefinitionManager 实例
- **WHEN** `UTcsStateComponent` 准备 State runtime 或解析必要 Definition
- **THEN** 它 MUST 只要求 `UTcsDefinitionManagerSubsystem` 实例可用
- **AND** MUST NOT 缓存或调用 `ResolveStateManager()`
- **AND** MUST NOT 将 DefinitionManager 的全局 `IsRuntimeReady()` 作为自身单域 ready 前置条件

#### Scenario: 已删除 StateManager 缓存路径已清零
- **WHEN** 在 TCS runtime 源码中搜索 `StateMgr`、`ResolveStateManager` 或 `UTcsStateManagerSubsystem`
- **THEN** 搜索结果 MUST 不包含 StateComponent 的缓存、诊断或 fallback 路径

### Requirement: TryApplyStateInstance 在入口强制校验归属

`UTcsStateComponent::TryApplyStateInstance(UTcsStateInstance*)` SHALL 作为 `virtual` 的 `UFUNCTION(BlueprintCallable)` 入口暴露出来，让调用方可以对已构造好的 `UTcsStateInstance` 执行 apply 后半段流程。入口 MUST 校验 `StateInstance->GetOwner() == GetOwner()`；不匹配时 MUST 通知 `InvalidInput` 失败并返回 `false`，从而防止属于 Actor A 的状态实例被注入到 Actor B 的 Component。

#### Scenario: 归属匹配时继续执行后半段
- **WHEN** `TryApplyStateInstance(Instance)` 在所属 Actor 与 Component owner 匹配时被调用
- **THEN** 该方法 MUST 执行条件检查、槽位分配、索引登记与成功通知

#### Scenario: 归属不匹配时被拒绝
- **WHEN** `TryApplyStateInstance(Instance)` 在 Component A 上被调用，而 `Instance->GetOwner()` 是 Actor B
- **THEN** 该方法 MUST NOT 修改任何槽位或索引状态
- **AND** MUST 通知 `InvalidInput` 失败并返回 `false`

#### Scenario: 已删除 StateManager 不再保留 apply 包装器
- **WHEN** 检查最终 runtime public API 面
- **THEN** `UTcsStateManagerSubsystem::TryApplyStateInstance` 或任何等价的 Manager apply 包装器 MUST 不存在
