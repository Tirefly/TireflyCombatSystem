# state-management Specification

## Purpose
定义 `UTcsStateComponent` 的 Actor 本地 State 业务边界：创建、应用、参数求值、条件检查、槽位、生命周期与查询。Definition cache/load 与 State-like 内部聚合查询归口到 `UTcsDefinitionManagerSubsystem`；`StateDefId` 仅保留在 State 模块内部；Buff apply 对外使用 `BuffDefId`。
## Requirements
### Requirement: State Component 拥有 Actor 本地 State 业务逻辑

`UTcsStateComponent` SHALL 成为 Actor 本地 state 业务逻辑的唯一归属，包括状态创建、应用、参数求值、条件检查、槽位分配、槽位激活、生命周期转换、移除与查询。该 change 归档后，`UTcsStateManagerSubsystem` MUST NOT 再保留任何 Actor 本地业务实现。

#### Scenario: 开发者通过子类扩展生命周期逻辑

- **WHEN** 开发者从 `UTcsStateComponent` 派生 `UMyCustomStateComponent` 并覆写 `FinalizeStateRemoval`
- **THEN** 该覆写 MUST 在每一种移除路径上都被调用，包括 direct remove、stack-depleted、duration-expired、merged-out 与 cancel，并且不允许被 Subsystem 绕过

#### Scenario: Subsystem 不再接受 Actor 本地调用

- **WHEN** 迁移完成后（Phase G 已归档）
- **THEN** `UTcsStateManagerSubsystem` MUST NOT 声明或实现 `TryApplyStateInstance` / `CreateStateInstance` / `EvaluateAndApplyStateParameters` / `CheckStateApplyConditions` / `InitStateSlotMappings` / `TryAssignStateToStateSlot` / `RequestUpdateStateSlotActivation` / `DrainPendingSlotActivationUpdates` / `UpdateStateSlotActivation` / `EnforceSlotGateConsistency` / `RefreshSlotsForStateChange` / 任何生命周期方法 / 任何查询方法 / 任何移除方法

#### Scenario: 移除 friend 声明

- **WHEN** 迁移完成时
- **THEN** `UTcsStateComponent` MUST NOT 再声明 `friend class UTcsStateManagerSubsystem`

### Requirement: State 移除保持单阶段且幂等

State 移除 SHALL 保持单阶段立即收敛流程（`RequestStateRemoval` → `FinalizeStateRemoval`），并以 `SS_Expired` 的短路语义保证幂等。迁移过程中 MUST NOT 重新引入 `PendingRemoval`、`FTcsStateRemovalRequest`、`FinalizePendingRemovalRequest`、`RemovalFlowPolicy` 或 `HardTimeout`。

#### Scenario: 重复 RequestStateRemoval 保持幂等

- **WHEN** `RequestStateRemoval(Instance, Reason)` is called twice on the same state instance within one frame
- **THEN** 第二次调用 MUST 在检测到 `SS_Expired` 后直接短路，不再执行额外工作，并且事件广播 MUST 只触发一次

#### Scenario: 不重新出现两阶段相关类型

- **WHEN** 该 change 已归档时
- **THEN** TCS 代码库中 MUST NOT 再包含 `FTcsStateRemovalRequest` / `FinalizePendingRemovalRequest` / `RemovalFlowPolicy` / `HardTimeout` 的声明

### Requirement: FinalizeStateRemoval 保持八步顺序

`UTcsStateComponent::FinalizeStateRemoval` SHALL 严格按以下顺序执行步骤，不允许重排，也不允许跳过：

1. Validate `StateInstance` and `StateDef`
2. If StateTree is running, call `StopStateTree()`
3. Attempt stage transition to `SS_Expired`; return early if it fails (idempotency)
4. Remove from local runtime caches: `StateTreeTickScheduler`, `DurationTracker`, `StateInstanceIndex`
5. If `SourceHandle` is valid, clear modifiers created by this state via `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(SourceHandle)`
6. Broadcast `NotifyStateStageChanged` and `NotifyStateRemoved`
7. Remove from slot containers and request slot activation refresh
8. `MarkPendingGC()`

#### Scenario: Modifier 清理绕过 Subsystem

- **WHEN** `FinalizeStateRemoval` executes Step 5 on a state whose `SourceHandle.Id > 0`
- **THEN** 调用路径 MUST 直接是 `StateInstance->GetOwnerAttributeComponent()->RemoveModifiersBySourceHandle(...)`，而 NOT 是 `AttrMgr->RemoveModifiersBySourceHandle(OwnerActor, ...)`

#### Scenario: 子类覆写仍保持顺序

- **WHEN** 某个子类覆写 `FinalizeStateRemoval` 并调用 `Super::FinalizeStateRemoval(...)`
- **THEN** 上述八个步骤 MUST 仍按原顺序执行

### Requirement: 内部调用点使用 Component-First 路径

TCS 内部的调用点在处理 Actor 本地操作时，MUST 调用 Component API，而不是再回绕到 `UTcsStateManagerSubsystem`。

#### Scenario: SetStackCount 耗尽时调用本地 RequestStateRemoval

- **WHEN** `UTcsStateInstance::SetStackCount(0)` triggers depletion removal
- **THEN** 代码路径 MUST 是 `OwnerStateCmp->RequestStateRemoval(this, TcsStateRemovalReasons::StackDepleted)`，而 NOT 是 `StateMgr->RequestStateRemoval(this, ...)`

#### Scenario: Duration 到期使用本地 ExpireState

- **WHEN** `UTcsStateComponent::UpdateActiveStateDurations` identifies expired instances
- **THEN** 每个到期实例 MUST 通过 `this->ExpireState(ExpiredState)` 处理，而 NOT 是 `StateMgr->ExpireState(ExpiredState)`

#### Scenario: BeginPlay 在本地初始化槽位

- **WHEN** `UTcsStateComponent::BeginPlay` executes
- **THEN** 槽位初始化 MUST 调用本地 `InitStateSlotMappings()` 方法，而 NOT 委托给 Subsystem

#### Scenario: 打开 Slot Gate 时触发本地激活

- **WHEN** `SetSlotGateOpen` toggles a slot to open
- **THEN** 激活刷新 MUST 调用本地 `RequestUpdateStateSlotActivation(SlotTag)`，而 NOT 调用 Subsystem 上的等价方法

#### Scenario: StateTree 变化通过本地刷新处理

- **WHEN** `OnStateTreeStateChanged` fires
- **THEN** 槽位刷新 MUST 调用本地 `RefreshSlotsForStateChange(...)`，而 NOT 调用 Subsystem 上的等价方法

### Requirement: Slot 激活防重入的作用域限定为 Component 实例

Slot 激活的防重入保护 SHALL 成为 `UTcsStateComponent` 的实例级关注点，每个 component 使用各自的 `protected bool bIsUpdatingSlotActivation` 与 `protected TSet<FGameplayTag> PendingSlotActivationUpdates`。Subsystem MUST NOT 再保留任何全局防重入标记。

#### Scenario: Actor A 的槽位更新不会阻塞 Actor B

- **WHEN** Actor A 正处于 `UpdateStateSlotActivation` 中，而 Actor B 在同一帧对自己的 component 并发调用 `TryApplyState`
- **THEN** Actor B 的槽位激活 MUST 独立推进，不得被排入 Actor A 的 pending set

#### Scenario: 防重入标记总能恢复

- **WHEN** `UpdateStateSlotActivation` executes any path (normal return, early return, or exceptional exit)
- **THEN** `bIsUpdatingSlotActivation` MUST 通过 `TGuardValue<bool>` 语义恢复到进入前的值

#### Scenario: 同帧多次 Apply 会被合并处理

- **WHEN** 同一个 component 在单帧内收到多次 `TryApplyState` 调用（Slot A、随后 Slot B、再随后 Slot A）
- **THEN** 第一次调用中的 `UpdateStateSlotActivation` MUST 通过 `DrainPendingSlotActivationUpdates` 处理所有受影响槽位；退出时 `PendingSlotActivationUpdates` MUST 为空，且不得重复触发激活/取消激活事件

### Requirement: State 查询使用 StateInstanceIndex 且保持 Non-Virtual

State 查询 API `GetStatesInSlot`、`GetStatesByDefId`、`GetAllActiveStates`、`HasStateWithDefId`、`HasActiveStateInSlot` SHALL 定义在 `UTcsStateComponent` 上，并优先通过 `StateInstanceIndex` 解析结果，同时 MUST NOT 声明为 `virtual`。查询路径 MUST NOT 调用诸如惰性 `RefreshInstances()` 之类带副作用的操作。

#### Scenario: 应用后查询返回基于索引的结果

- **WHEN** 一个 state 通过 `TryApplyState` 被成功应用到 Slot X
- **THEN** `GetStatesInSlot(X)` MUST 通过 `StateInstanceIndex.GetInstancesBySlot(X)` 返回该状态，而不是扫描 `StateSlotsX`

#### Scenario: 移除后的查询不再返回已过期状态

- **WHEN** `FinalizeStateRemoval` completes for a state
- **THEN** 后续对 `GetAllActiveStates`、`GetStatesByDefId` 与 `HasStateWithDefId` 的调用 MUST NOT 再返回该已移除状态

#### Scenario: 子类不能覆写查询方法

- **WHEN** 某个子类试图为 `GetStatesInSlot` / `GetStatesByDefId` / `GetAllActiveStates` / `HasStateWithDefId` / `HasActiveStateInSlot` 声明 override
- **THEN** 编译器 MUST 因基类方法为 non-virtual 而拒绝该覆写

### Requirement: Owner 在到期分发期间自毁仍然安全

`UTcsStateComponent::UpdateActiveStateDurations` SHALL 能检测并容忍由 `ExpireState` 分发期间的回调触发的 owner / component 自毁，从而避免 use-after-free。

#### Scenario: Attribute 回调在循环中途销毁 owner

- **WHEN** `UpdateActiveStateDurations` dispatches three expired states, and the first state's `ExpireState` triggers an attribute-change callback that calls `Destroy(GetOwner())`
- **THEN** 在 `ExpireState` 返回后，循环 MUST 检查 `IsBeingDestroyed() || !IsValid(GetOwner())` 并立即退出；第二个和第三个已过期状态 MUST NOT 再被分发；整个调用 MUST NOT 崩溃

### Requirement: RemoveAllStates 对 StateTree 重入给出诊断

`UTcsStateComponent::RemoveAllStates` 在从 StateTree 更新上下文内部调用时（例如 task 的 `Tick` / `EnterState` / `ExitState` 回调），SHALL 发出 `ensureMsgf` 诊断。该诊断 MUST NOT 改变控制流；真正的安全性由引擎层的 deferred-stop（`FStateTreeExecutionContext::RequestedStop`）负责。

#### Scenario: 帧边界调用不产生诊断

- **WHEN** `RemoveAllStates` is called outside any StateTree callback (e.g., from object pool `OnReturnedToPool` scheduled at frame edge)
- **THEN** 不应触发任何 `ensureMsgf`

#### Scenario: 在 Tick 中调用会发出诊断但不崩溃

- **WHEN** 一个自定义 StateTree task 在其 `Tick` 中调用 `RemoveAllStates`
- **THEN** MUST 触发带有 deferred-stop 语义说明的 `ensureMsgf`；引擎的 `RequestedStop` 路径 MUST 将真正的 `Stop` 延后到当前 phase 结束；`FinalizeStateRemoval` 的第 2-8 步 MUST 仍然同步执行；task 的 `ExitState` 中 MUST NOT 出现野指针崩溃

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

### Requirement: Removal Reason 使用具名常量

所有 state-removal reason 标识符都 SHALL 在 `Public/State/TcsStateInstance.h` 中的专用 `TcsStateRemovalReasons` namespace 内声明为 `static const FName`。使用点 MUST 引用这些常量，而不是字符串字面量。

#### Scenario: 已声明常量

- **WHEN** 审查 `Public/State/TcsStateInstance.h` 时
- **THEN** `namespace TcsStateRemovalReasons` MUST 将 `Removed`、`Cancelled`、`Expired`、`MergedOut`、`StackDepleted` 声明为 `static const FName`

#### Scenario: 不再漂移回字符串字面量

- **WHEN** 在 TCS 代码库中以 removal reason 的用途搜索 `"Removed"`、`"Cancelled"`、`"Expired"`、`"MergedOut"`、`"StackDepleted"`
- **THEN** 所有业务调用点中 MUST 有零处继续使用这些字符串字面量；全部 MUST 改为引用 namespace 常量

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

