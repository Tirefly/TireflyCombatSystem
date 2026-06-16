# 规范增量 —— state-management

> **权威执行依据**：本 spec delta 用于定义契约与不变量。具体迁移步骤、文件行号、签名替换规则以及 static→成员方法的转换，以 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/03_PhaseD_StateRemoval与生命周期迁移.md` 与 `04_PhaseE_状态应用槽位链路与查询.md` 为准。

## ADDED Requirements

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

迁移完成后，`UTcsStateManagerSubsystem` SHALL 只暴露 definition cache/load、definition 查询、全局 state instance ID 工厂，以及跨 Actor 门面。

#### Scenario: Definition 查询仍保持集中化

- **WHEN** 任意调用方需要通过 `FName` 或 `FGameplayTag` 解析 `UTcsStateDefinition` 时
- **THEN** MUST 使用 `UTcsStateManagerSubsystem::GetStateDefinition` / `GetStateDefinitionByTag` / `GetStateSlotDefinition` / `GetStateSlotDefinitionByTag` / `GetAllStateDefNames`

#### Scenario: 全局 ID 工厂仍留在 Subsystem 上

- **WHEN** `UTcsStateComponent` 需要一个新的全局唯一 `StateInstanceId`
- **THEN** 它 MUST 调用 `ResolveStateManager()->AllocateStateInstanceId()`；ID 计数器 MUST NOT 被迁移到 Component 作用域

#### Scenario: 跨 Actor 门面保持轻薄

- **WHEN** `UTcsStateManagerSubsystem::TryApplyStateToTarget(TargetActor, ...)` is invoked
- **THEN** 它 MUST 只执行：Target 校验、`StateComponent` 解析，以及委托到 `StateComp->TryApplyState(...)`；不允许包含任何 Actor 本地逻辑



### Requirement: Removal Reason 使用具名常量

所有 state-removal reason 标识符都 SHALL 在 `Public/State/TcsStateInstance.h` 中的专用 `TcsStateRemovalReasons` namespace 内声明为 `static const FName`。使用点 MUST 引用这些常量，而不是字符串字面量。

#### Scenario: 已声明常量

- **WHEN** 审查 `Public/State/TcsStateInstance.h` 时
- **THEN** `namespace TcsStateRemovalReasons` MUST 将 `Removed`、`Cancelled`、`Expired`、`MergedOut`、`StackDepleted` 声明为 `static const FName`

#### Scenario: 不再漂移回字符串字面量

- **WHEN** 在 TCS 代码库中以 removal reason 的用途搜索 `"Removed"`、`"Cancelled"`、`"Expired"`、`"MergedOut"`、`"StackDepleted"`
- **THEN** 所有业务调用点中 MUST 有零处继续使用这些字符串字面量；全部 MUST 改为引用 namespace 常量



### Requirement: Manager 缓存在 BeginPlay 解析并带有诊断安全网

`UTcsStateComponent` SHALL 在 `BeginPlay` 期间缓存 `UTcsStateManagerSubsystem` 指针，暴露一个返回该缓存的 `protected ResolveStateManager()` helper（缓存缺失时带 `ensureMsgf` 诊断并尝试重新获取一次），并在非 shipping 构建下于 `BeginPlay` 末尾加入 `checkf` 自检。

#### Scenario: 正常 BeginPlay 会解析缓存

- **WHEN** `UTcsStateComponent::BeginPlay` runs in any target configuration
- **THEN** `BeginPlay` 返回后 `StateMgr` MUST 非空；后续调用 `ResolveStateManager()` 时 MUST 直接返回缓存指针，不引入额外运行时开销

#### Scenario: 预热失败时触发 checkf（非 shipping）

- **WHEN** 一个异常环境导致 `GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>()` 在 `BeginPlay` 期间返回 null（Debug / Development / Test 构建）
- **THEN** `#if !UE_BUILD_SHIPPING` 下的 `checkf(StateMgr, ...)` MUST 立即触发，并先于任何 gameplay 代码执行

#### Scenario: Shipping 运行时缓存缺失会发出 ensureMsgf

- **WHEN** 某个运行时方法在 Shipping 中调用 `ResolveStateManager()` 且缓存为空（理论场景）
- **THEN** 该 helper MUST 带诊断上下文发出 `ensureMsgf`，并尝试重新获取一次；Shipping 下的 `checkf` MUST 被裁掉



### Requirement: TryApplyStateInstance 在入口强制校验归属

`UTcsStateComponent::TryApplyStateInstance(UTcsStateInstance*)` SHALL 作为 `virtual` 的 `UFUNCTION(BlueprintCallable)` 入口暴露出来，让调用方可以对已构造好的 `UTcsStateInstance` 执行 apply 后半段流程（条件检查 → 槽位分配 → 索引登记 → 成功通知）。在入口处，该方法 MUST 校验 `StateInstance->GetOwner() == GetOwner()`；若校验失败，它 MUST 调用 `NotifyStateApplyFailed(ETcsStateApplyFailReason::InvalidInput, ...)` 并返回 `false`，从而防止属于 Actor A 的状态实例被误注入到 Actor B 的 component 中。

#### Scenario: 归属匹配时继续执行后半段

- **WHEN** `TryApplyStateInstance(Instance)` is invoked on the component whose `GetOwner()` matches `Instance->GetOwner()`
- **THEN** 该方法 MUST 依次执行条件检查 → 槽位分配 → `StateInstanceIndex.AddInstance`（仅在槽位分配成功后）→ `NotifyStateApplySuccess`

#### Scenario: 归属不匹配时被拒绝

- **WHEN** `TryApplyStateInstance(Instance)` is invoked on Component A while `Instance->GetOwner()` is Actor B (B != A's owner)
- **THEN** 该方法 MUST NOT 修改任何槽位或索引状态，MUST 调用 `NotifyStateApplyFailed(ETcsStateApplyFailReason::InvalidInput, ...)`，并且 MUST 返回 `false`

#### Scenario: Deprecated 的 Subsystem 包装器委托给 owner component

- **WHEN** 在 Phase F 期间调用过渡态包装器 `UTcsStateManagerSubsystem::TryApplyStateInstance(Instance)`
- **THEN** 它 MUST 委托到 `Instance->GetOwnerStateComponent()->TryApplyStateInstance(Instance)`；Subsystem 自身 MUST NOT 承担任何 Actor 本地逻辑
