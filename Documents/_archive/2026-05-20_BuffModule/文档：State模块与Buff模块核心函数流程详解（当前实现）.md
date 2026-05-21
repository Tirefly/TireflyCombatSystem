# State 模块与 Buff 模块核心函数流程详解（当前实现）

## 文档目的

本文直接基于当前代码实现，展开说明 TCS 中 State 模块的核心函数调用流程，以及 Buff 模块依附于 State 主流程后的关键运行链路。

本文关注的是“当前代码实际如何运行”，不是历史设计稿，也不是抽象理想模型。

当前结论先写在最前面：

- `UTcsStateComponent` 是 Actor 本地的共享状态宿主，负责状态实例的创建、参数求值、应用条件检查、槽位归属、激活切换、StateTree Tick、移除收敛和事件广播。
- `UTcsBuffComponent` 不是平行的第二套状态宿主，而是一个挂在 Actor 上的 Buff 语义扩展层，负责 Buff 专属的持续时间、叠层/合并、调试叠层信息，以及把这些行为挂接到 State 主流程中。
- Buff 的“申请 / 生效 / 淘汰 / 过期”最终都还是通过 State 主流程落地；Buff 只是在若干关键节点插入自己的语义。

## 代码锚点

本文主要依据以下文件：

- `Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
- `Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
- `Source/TireflyCombatSystem/Private/State/TcsStateInstance.cpp`
- `Source/TireflyCombatSystem/Public/State/TcsStateDefinition.h`
- `Source/TireflyCombatSystem/Public/State/TcsStateSlotDefinition.h`
- `Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp`
- `Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h`
- `Source/TireflyCombatSystem/Private/Buff/TcsBuffInstance.cpp`
- `Source/TireflyCombatSystem/Public/Buff/TcsBuffDefinition.h`
- `Source/TireflyCombatSystem/Public/Buff/BuffMerger/TcsBuffMerger.h`

## 一、总体协作关系

### 1.1 State 模块负责什么

State 模块的职责可以概括成六件事：

1. 解析和创建状态实例。
2. 对状态定义中的参数进行运行时求值并写入实例。
3. 检查状态是否允许应用。
4. 把状态放进对应槽位，并根据槽位配置决定谁激活、谁挂起、谁暂停、谁被取消。
5. 驱动每个状态实例内部的 StateTree 生命周期。
6. 把所有移除原因统一收敛到同一条移除链路。

### 1.2 Buff 模块负责什么

Buff 模块的职责明显更窄，也更语义化：

1. 解析或创建 Actor 上的 Buff 宿主组件，并在 Owner 上已经存在 `UTcsStateComponent` 时完成绑定。
2. 只允许 `UTcsBuffDefinition` 类型走 Buff 申请入口。
3. 为 Buff 实例维护剩余持续时间。
4. 在槽位激活刷新前执行 Buff 合并。
5. 维护 Buff 的叠层变化、时长刷新和调试叠加层展示。
6. 当 Buff 需要过期、叠层耗尽或合并淘汰时，把移除请求重新送回 State 模块统一处理。

### 1.3 当前的核心架构结论

当前架构不是“State 管 State，Buff 也自己再管一套 Buff 状态”，而是：

- `State` 管共享宿主与统一时序。
- `Buff` 管 Buff 专属语义。
- 两者通过事件和扩展点解耦协作。

最关键的两个扩展点在 `UTcsStateComponent` 里：

- `OnPrepareStateSlotActivation()`：槽位刷新前置扩展点，Buff 在这里做合并。
- `OnBuildStateDebugOverlay()`：调试展示扩展点，Buff 在这里补叠层和时长文本。

## 二、State 模块核心调用流程

## 2.1 组件启动与运行时槽位初始化

入口函数链：

```text
UTcsStateComponent::BeginPlay
  -> ResolveStateManager / ResolveAttributeManager
  -> InitStateSlotMappings
     -> RebuildStateSlotRuntimeData
     -> RebuildStateTreeSlotBindings
  -> Super::BeginPlay
```

### 2.1.1 `BeginPlay`

`UTcsStateComponent::BeginPlay()` 做三件关键事情：

1. 解析 `UTcsStateManagerSubsystem` 和 `UTcsAttributeManagerSubsystem`。
2. 初始化运行时槽位容器和 StateTree 状态名绑定表。
3. 在这些初始化完成后，才调用 `UStateTreeComponent::BeginPlay()` 启动外层 StateTree。

这里的顺序很重要：槽位和映射必须先准备好，否则后面外层 StateTree 的状态变化通知一旦回调到组件，组件还没有可以接收 Gate 联动的运行时容器。

不过这里还要补一个实现前提：当前外层 StateTree 的 Gate 联动并不是 `UStateTreeComponent` 自动内建的行为，而是依赖树里显式放入状态变化通知任务后，才会调用 `UTcsStateComponent::OnStateTreeStateChanged()`。

### 2.1.2 `InitStateSlotMappings`

`InitStateSlotMappings()` 内部分成两步：

1. `RebuildStateSlotRuntimeData()`
2. `RebuildStateTreeSlotBindings()`

#### `RebuildStateSlotRuntimeData()`

这个函数从 StateManager 中取出所有 `UTcsStateSlotDefinition`，为每个 `SlotTag` 建立一份 `RuntimeStateSlots` 运行时槽位数据。

关键点：

- 旧容器会先被搬走，再按最新定义表重建。
- 同名 `SlotTag` 的旧运行时数据会被迁移回来，因此已有状态列表和 Gate 状态会尽可能保留。
- 槽位定义 `StateSlotDef` 会在这一步直接缓存进 `FTcsStateSlot`，后续应用和激活主链不需要再反复按 `SlotTag` 回 `StateManager` 查定义。
- 一个 `SlotTag` 最终只保留一份运行时槽位，避免重复定义造成运行时污染。

#### `RebuildStateTreeSlotBindings()`

这个函数扫描当前外层 `StateTree` 中真实存在的状态名，然后直接读取 `FTcsStateSlot` 中缓存的 `StateSlotDef`，把 `SlotTag` 和其中声明的 `StateTreeStateName` 记录进 `Mapping_StateSlotToStateTreeStateName`。这里不是把两个同类型标识“绑定成一个值”，而是建立一条 `FGameplayTag -> FName` 的桥接映射。

作用是建立这样一张桥接表：

```text
Mapping_StateSlotToStateTreeStateName: SlotTag -> StateTreeStateName
外层 StateTree 中的激活状态名 -> 反推出要打开/关闭哪个 StateSlot 的 Gate
```

如果某个槽位没配置 `StateTreeStateName`，那它仍然存在于 `RuntimeStateSlots`，只是不会被外层 StateTree 自动驱动 Gate。进一步说，即使配置了 `StateTreeStateName`，如果外层 StateTree 里没有显式放入状态变化通知任务，Gate 联动链仍然不会自动发生。

## 2.2 状态申请主链

这是 State 模块最核心的一条链：

```text
UTcsStateComponent::TryApplyState
  -> GetStateDefinition
  -> CreateStateInstance
     -> UTcsStateInstance::Initialize
     -> EvaluateAndApplyStateParameters
     -> SetApplyTimestamp
     -> CreateSourceHandle
  -> TryApplyStateInstance
     -> CheckStateApplyConditions
     -> TryAssignStateToStateSlot
```

### 2.2.1 `TryApplyState`

`TryApplyState()` 是最外层申请入口。

它的工作顺序是：

1. 校验 `Owner`、`Instigator`、`StateDefId` 是否有效。
2. 通过 `StateMgr->GetStateDefinition(StateDefId)` 取定义。
3. 调用 `CreateStateInstance()` 创建并初始化实例。
4. 把实例交给 `TryApplyStateInstance()` 进入共享应用主流程。

这个函数同时维护了一个统一的失败上报出口 `ReportApplyFailure`：

- 记录日志。
- 广播 `NotifyStateApplyFailed()`。

所以从这里开始，State 模块已经把“申请失败”也做成了可观察事件，而不是简单返回 `false`。

### 2.2.2 `CreateStateInstance`

`CreateStateInstance()` 是“创建实例 + 注入初始运行时信息”的关键点。

它的核心动作是：

1. 校验 `Owner` / `Instigator` 是否实现了 `ITcsEntityInterface`。
2. 通过 `UTcsStateDefinition::ResolveStateInstanceClass()` 确认要创建的运行时实例类。
3. `NewObject<UTcsStateInstance>(OwnerActor, StateInstanceClass)` 创建实例。
4. 调用 `UTcsStateInstance::Initialize()` 完成实例初始化。
5. 调用 `EvaluateAndApplyStateParameters()` 对定义中的参数求值并写回实例。
6. 写入 ApplyTimestamp。
7. 根据父级 `SourceHandle` 构建新的因果链，并通过 `UTcsAttributeManagerSubsystem::CreateSourceHandle()` 生成新的 `SourceHandle`。

#### `UTcsStateInstance::Initialize()` 在这里做了什么

`UTcsStateInstance::Initialize()` 负责把实例真正接入运行时上下文：

- 缓存 `Owner` / `Instigator`。
- 通过 `ITcsEntityInterface` 解析双方的 `StateComponent`、`AttributeComponent`、`SkillComponent`。
- 清理所有参数缓存。
- 调用 `InitializeRuntimeParameters()` 给派生类留运行时参数初始化扩展点。

也就是说，StateInstance 在进入主流程前，已经是一个“绑定到 Owner / Instigator 上下文”的实体，而不是裸对象。

### 2.2.3 `EvaluateAndApplyStateParameters`

这个函数把 `UTcsStateDefinition` 中的：

- `Parameters`
- `TagParameters`

统一求值并写回到 `UTcsStateInstance`。

支持的参数类型有：

- Numeric
- Bool
- Vector

它不会延后求值，而是在实例真正进入槽位前完成；这意味着：

- 后续的应用条件检查可以直接读取实例参数。
- Buff 等派生类型也能在自己的运行时逻辑里直接读到已经写好的参数。

## 2.3 状态进入共享应用流程

### 2.3.1 `TryApplyStateInstance`

`TryApplyStateInstance()` 做的是“创建后、入槽前”的最后一轮总校验。

顺序是：

1. 校验实例是否有效。
2. 校验实例 Owner 是否就是当前组件的 Owner。
3. 校验实例是否已经完成 `Initialize()`。
4. 调用 `CheckStateApplyConditions()` 检查定义上的激活条件。
5. 调用 `TryAssignStateToStateSlot()` 真正入槽。

### 2.3.2 `CheckStateApplyConditions`

这个函数遍历 `StateDef->ActiveConditions`，只执行 `bCheckWhenApplying` 为真的条件。

调用链是：

```text
StateDef.ActiveConditions[i]
  -> ConditionClass.GetDefaultObject()
  -> CheckCondition(StateInstance, Payload)
```

只要有一个条件失败，整个申请流程就失败，不会进入槽位。

## 2.4 状态进入槽位与槽位激活刷新

这一段是 State 模块最容易误判的地方。当前实现不是“先正式提交，再事后收拾”，而是：

```text
TryAssignStateToStateSlot
   -> 读取 StateSlot 缓存的 StateSlotDef
  -> StateSlot->States.Add(StateInstance)
  -> RequestUpdateStateSlotActivation
     -> UpdateStateSlotActivation
        -> Broadcast OnPrepareStateSlotActivation
        -> EnforceSlotGateConsistency
        -> ProcessStateSlotByActivationMode
           -> Activate / HangUp / Pause / Cancel
  -> 只有状态仍然有效时，才 AddInstance + NotifyStateApplySuccess
```

也就是说，实例虽然先被放进槽位数组，但它是否最终“算应用成功”，要等槽位刷新和可能发生的 Buff 合并结束之后才决定。

### 2.4.1 `TryAssignStateToStateSlot`

这个函数负责把状态送入目标槽位，并触发后续激活刷新。

它内部做了以下校验和动作：

1. 校验 `StateDef` 是否存在。
2. 校验 `StateDef->StateSlotType` 是否有效。
3. 校验 `StateInstance->GetOwnerStateComponent() == this`。
4. 从 `RuntimeStateSlots` 里找到目标 `FTcsStateSlot`。
5. 直接从 `FTcsStateSlot` 中读取已经缓存的 `StateSlotDef`。
6. 调用 `ClearStateSlotExpiredStates()` 清理槽位中过期状态。
7. 拒绝重复入槽。
8. 如果 Gate 关闭且策略是 `Cancel`，直接拒绝申请。
9. 在 `PriorityOnly + CancelLowerPriority` 场景下，若新状态优先级低于现存最高优先级，直接拒绝。
10. 把实例加入 `StateSlot->States`。
11. 如果当前 Gate 关闭，则按 `GateCloseBehavior` 先把阶段设为 `HangUp` 或 `Pause`。
12. 调用 `RequestUpdateStateSlotActivation()`。
13. 刷新结束后，如果实例仍然有效，再写入 `StateInstanceIndex` 并广播 `NotifyStateApplySuccess()`。

这最后一步很关键：

- 如果实例在槽位刷新过程中被合并淘汰或取消，它就不会进入 `StateInstanceIndex`。
- 也不会收到 `ApplySuccess`。

这也是 Buff 模块当前能够在“申请过程中就把重复实例合并掉”的根本基础。

### 2.4.2 `RequestUpdateStateSlotActivation`

这是槽位刷新调度器。

如果当前已经处于槽位刷新过程中，新的刷新请求不会立刻递归执行，而是先塞进 `PendingSlotActivationUpdates`，等本轮结束后再统一 Drain。

这解决的是“槽位刷新内部又触发新的槽位刷新”导致的重入问题。

### 2.4.3 `UpdateStateSlotActivation`

这个函数是槽位刷新真正的中枢。

对一个槽位的处理顺序是：

1. `ClearStateSlotExpiredStates(StateSlot)`
2. `SortStatesByPriority(StateSlot->States)`
3. 广播 `PrepareStateSlotActivationEvent`
4. 基于 `StateSlot` 内缓存的 `StateSlotDef` 执行 `EnforceSlotGateConsistency(StateSlotTag)`
5. 如果 Gate 关闭：只做清理，不做正常激活流程
6. 如果 Gate 开启：继续基于缓存的 `StateSlotDef` 执行 `ProcessStateSlotByActivationMode()`
7. `CleanupInvalidStates(StateSlot)`
8. 如果 Gate 处于开启分支，调用 `OnStateSlotChanged(StateSlotTag)`
9. `DrainPendingSlotActivationUpdates()`

其中第三步就是 Buff 插入合并流程的关键时机。

而 `OnStateSlotChanged()` 在当前实现里仍然只是一个占位钩子，内部只有 TODO 和一条 `VeryVerbose` 日志，还没有实际业务逻辑。

### 2.4.4 `ProcessStateSlotByActivationMode`

当前主要有两类激活模式：

- `SSAM_PriorityOnly`
- `SSAM_AllActive`

#### `ProcessPriorityOnlyMode`

它的逻辑是：

1. 找出当前槽位中最高优先级。
2. 收集所有最高优先级状态。
3. 如果最高优先级状态有多个，并且配置了 `SamePriorityPolicy`，就用该策略对候选者排序。
4. 取排序后的第一个作为最终激活者。
5. 对这个赢家调用 `ActivateState()`。
6. 对其他状态按 `PreemptionPolicy` 处理：
   - `CancelLowerPriority` -> `CancelState()`
   - `HangUpLowerPriority` -> `HangUpState()`
   - `PauseLowerPriority` -> `PauseState()`

#### `ProcessAllActiveMode`

它更简单：

- 只要状态有效且当前不是 `SS_Active`，就调用 `ActivateState()`。

### 2.4.5 `ActivateState`

`ActivateState()` 负责把状态切到 `SS_Active`，并驱动内层 StateTree 启动。

它的顺序是：

1. 校验实例有效。
2. 如果已经是 `SS_Active`，直接返回。
3. `SetCurrentStage(SS_Active)`。
4. 读取 `StateDef->TickPolicy`。
5. 根据 TickPolicy 决定如何启动内层 StateTree：
   - `RunOnce`：`RestartStateTree()` 后立即 `TickStateTree(0.f)`，若还在运行则强制停止。
   - `ManualOnly`：`RestartStateTree()`，但不加入调度器。
   - `WhileActive`：`RestartStateTree()`，若运行成功则加入 `StateTreeTickScheduler`。
6. 广播 `NotifyStateStageChanged()`。

### 2.4.6 Gate 关闭时的一致性处理

`EnforceSlotGateConsistency()` 只在 Gate 关闭时起作用。

它会根据 `GateCloseBehavior` 对槽位内所有状态做一致性收敛：

- `HangUp`：把 Active 状态挂起。
- `Pause`：把 Active 或 HangUp 状态暂停。
- `Cancel`：直接取消状态。

在按策略分支处理完之后，代码还会再做一轮兜底扫描，把任何仍然停留在 `SS_Active` 的状态强制 `HangUp`。最后才会额外断言：

- 关闭 Gate 的槽位中不应再存在 `SS_Active` 状态。

## 2.5 外层 StateTree 如何驱动 StateSlot Gate

这条链决定了 Layer 1 StateTree 怎样影响 Layer 2 运行时槽位。

但必须先强调一个当前实现前提：这条链不是外层 StateTree 自己天然就会触发的。当前代码是通过在外层树里显式放入 `FTcsStateChangeNotifyTask`，在 `EnterState / ExitState` 中回调 `UTcsStateComponent::OnStateTreeStateChanged()`，再继续后面的 Gate 联动。

调用链：

```text
外层 StateTree 中显式配置 FTcsStateChangeNotifyTask
  -> FTcsStateChangeNotifyTask::EnterState / ExitState
     -> UTcsStateComponent::OnStateTreeStateChanged
        -> RefreshSlotsForStateChange
           -> SetSlotGateOpen
              -> NotifySlotGateStateChanged
              -> RequestUpdateStateSlotActivation
```

### 2.5.1 `OnStateTreeStateChanged`

当外层 StateTree 中的 `FTcsStateChangeNotifyTask` 在 `EnterState` 或 `ExitState` 里显式回调时，`UTcsStateComponent::OnStateTreeStateChanged()` 会：

1. 从 `FStateTreeExecutionContext` 取当前激活状态名数组。
2. 与 `CachedActiveStateNames` 比较。
3. 如果发生变化，则调用 `RefreshSlotsForStateChange()`。
4. 最后更新缓存。

### 2.5.2 `RefreshSlotsForStateChange`

这个函数会根据“哪些 StateTree 状态新增激活 / 取消激活”，反推出每个 `SlotTag` 此刻应不应该打开 Gate。

桥接依据是：

- `Mapping_StateSlotToStateTreeStateName`

如果某个映射状态名现在处于激活状态，则其对应槽位 Gate 应打开；反之关闭。

### 2.5.3 `SetSlotGateOpen`

`SetSlotGateOpen()` 不只是设置一个布尔值，它同时会：

1. 更新 `FTcsStateSlot::bIsGateOpen`
2. 广播 `NotifySlotGateStateChanged()`
3. 立即调用 `RequestUpdateStateSlotActivation()` 触发整个槽位刷新链

所以“Gate 改变”在当前实现中不是静态标志变化，而是直接推动运行时状态重排。

## 2.6 内层 StateTree 的启动、Tick、暂停与恢复

这里的核心关系是：

- `UTcsStateComponent` 负责“什么时候该启动/恢复/暂停/停止”。
- `UTcsStateInstance` 负责“具体如何和 StateTree Runtime API 交互”。

### 2.6.1 Tick 主链

```text
UTcsStateComponent::TickComponent
  -> TickStateTrees
     -> StateTreeTickScheduler.RefreshInstances
     -> 遍历 RunningInstances
        -> 仅保留 WhileActive 且当前阶段为 Active 的实例
        -> UTcsStateInstance::TickStateTree
```

`TickStateTrees()` 会跳过以下实例：

- 无效实例
- 已不再运行内层 StateTree 的实例
- 当前阶段不是 `SS_Active`
- TickPolicy 不是 `WhileActive`

也就是说，进入调度器只是前提；真正每帧会不会被 Tick，还要再次满足阶段与策略条件。

### 2.6.2 `UTcsStateInstance::RestartStateTree`

`RestartStateTree()` 实际调用的是 `StartStateTreeInternal(true)`。

主要工作：

1. 取 `StateDef->StateTreeRef.GetStateTree()`。
2. 需要重置时清空 `StateTreeInstanceData`。
3. 构造 `FStateTreeExecutionContext`。
4. 调用 `SetContextRequirements(Context)` 设置上下文。
5. `Context.Start(StartParams)` 启动。
6. 如果返回 `Running`，置 `bStateTreeRunning = true`。

### 2.6.3 `UTcsStateInstance::TickStateTree`

每帧 Tick 时：

1. 再次构造 `FStateTreeExecutionContext`。
2. 再次设置上下文需求。
3. 执行 `Context.Tick(DeltaTime)`。
4. 根据返回的 `EStateTreeRunStatus` 更新 `bStateTreeRunning`。

如果运行结果变成：

- `Succeeded`
- `Failed`
- `Stopped`

实例会停止标记为运行中，随后会在组件侧调度器清理中被移出。

### 2.6.4 `PauseStateTree` 与 `ResumeStateTree`

这里有一个很重要的实现细节：

- `PauseStateTree()` 并不真的 Stop 内层 StateTree，它只是把自己从 `StateTreeTickScheduler` 里移除。
- `ResumeStateTree()` 会确保 StateTree 已启动，然后根据 `TickPolicy` 和当前阶段决定要不要重新进调度器。

所以当前的 Pause / HangUp 语义更接近“停止继续 Tick”，而不是“销毁并重建内层状态树运行现场”。

## 2.7 状态移除与过期收敛链

这是 State 模块第二条最关键的统一主链。

调用入口很多，但最终都汇聚到：

```text
RequestStateRemoval
  -> FinalizeStateRemoval
```

### 2.7.1 进入统一移除链的入口

以下路径都会汇聚到 `RequestStateRemoval()`：

- `RemoveState()`
- `RemoveStatesByDefId()`
- `RemoveAllStatesInSlot()`
- `RemoveAllStates()`
- `CancelState()`
- `ExpireState()`
- Buff 模块中的 `RemoveBuff()` / `RemoveBuffInstance()` / `ExpireBuffInstance()`
- Buff 合并淘汰时的 `RequestStateRemoval(...MergedOut)`
- Buff 叠层耗尽时的 `RequestStateRemoval(...StackDepleted)`

### 2.7.2 `RequestStateRemoval`

这个函数本身非常薄，而且当前实现明显是在信任调用方：

1. 无效实例直接失败。
2. 如果实例当前已经是 `SS_Expired`，直接返回成功。
3. 其他情况统一调用 `FinalizeStateRemoval()`。

它不会在这里额外校验：

- 这个实例是否属于当前 `UTcsStateComponent`
- 它是否仍然在当前组件的某个槽位里

真正带“归属校验”的外层入口是 `RemoveState()`，不是 `RequestStateRemoval()` 本身。

### 2.7.3 `FinalizeStateRemoval`

真正的移除收敛逻辑全部在这里。

执行顺序是：

1. 校验实例和 `StateDef`。
2. 记录 `SlotTag` 和 `PreviousStage`。
3. 如果内层 StateTree 还在运行，先 `StopStateTree()`。
4. 把阶段切到 `SS_Expired`。
5. 从 `StateTreeTickScheduler` 移除。
6. 从 `StateInstanceIndex` 移除。
7. 如果实例携带 `SourceHandle`，则清理该 SourceHandle 在 Owner 身上创建的 Attribute Modifiers。
8. 广播 `NotifyStateStageChanged(...SS_Expired)`。
9. 广播 `NotifyStateRemoved(StateInstance, RemovalReason)`。
10. 从所属 `StateSlot->States` 中移除自己。
11. 对所属槽位再次调用 `RequestUpdateStateSlotActivation()`，让剩余状态重新整理。
12. `MarkPendingGC()`。

这里有两个关键性质：

- SourceHandle 清理是统一的，不论这是普通 State、Skill 还是 Buff，只要 Modifier 生命周期跟着这个状态走，就都在这里收尾。
- 槽位内移除一个状态后，一定会触发下一轮槽位刷新，因此“老大死了，老二顶上来”这种逻辑不需要额外人工补调。

## 三、Buff 模块关键流程

## 3.1 Buff 模块的定位

`UTcsBuffComponent` 的定位不是“Buff 的独立状态系统”，而是“挂在共享 State 主流程上的 Buff 语义扩展器”。

它自己不负责：

- 创建通用状态实例
- 决定槽位激活模式
- 维护统一状态索引
- 收敛最终移除链

它只负责：

- Buff 入口校验
- Buff 专属运行时参数语义
- 时长跟踪
- 合并
- 叠层
- 调试叠加层

## 3.2 Buff 组件初始化与宿主绑定

主链如下：

```text
UTcsBuffComponent::BeginPlay
  -> ResolveOwnerStateComponent
     -> FindComponentByClass<UTcsStateComponent>
     -> BindOwnerStateEvents
```

以及另一条按需创建链：

```text
UTcsBuffComponent::GetOrCreateForActor
  -> FindComponentByClass<UTcsBuffComponent>
  -> 不存在则 NewObject + AddOwnedComponent + RegisterComponent
   -> 如果 Owner 上已有 UTcsStateComponent，则 InitializeOwnerStateComponent(StateComponent)
```

### 3.2.1 `ResolveOwnerStateComponent`

这是 Buff 和 State 建立连接的关键函数。

它会：

1. 优先返回已缓存的 `OwnerStateComponent`。
2. 若未缓存，则从 OwnerActor 上查找 `UTcsStateComponent`。
3. 只有找到 `UTcsStateComponent` 后，才缓存并立即调用 `BindOwnerStateEvents()`。

所以当前实现能保证的是“Buff 组件会尽量解析并绑定 State 宿主”，而不是“只要创建了 Buff 组件就必然已经绑定好了 State 宿主”。如果 OwnerActor 上本来就没有 `UTcsStateComponent`，`ApplyBuff()` 这条入口会直接失败返回。

### 3.2.2 `BindOwnerStateEvents`

当前 Buff 组件会绑定四个关键扩展点：

1. `OnStateApplySuccess` -> `HandleOwnerStateApplySuccess`
2. `OnStateRemoved` -> `HandleOwnerStateRemoved`
3. `OnPrepareStateSlotActivation()` -> `HandleOwnerStateSlotActivation`
4. `OnBuildStateDebugOverlay()` -> `HandleOwnerStateDebugOverlay`

这四个点正好分别对应：

- 成功进入主流程后的注册
- 离开主流程后的注销
- 槽位刷新前的 Buff 合并
- 调试展示补充

## 3.3 Buff 申请入口

### 3.3.1 调用链

```text
UTcsBuffComponent::ApplyBuff
  -> ResolveOwnerStateComponent
   -> 校验 StateComponent / BuffDefId / Instigator
   -> GetStateManager
  -> GetStateDefinition(BuffDefId)
  -> 校验是否是 UTcsBuffDefinition
  -> StateComponent->TryApplyState(BuffDefId, Instigator, BuffLevel, ParentSourceHandle)
```

### 3.3.2 `ApplyBuff` 的本质

`ApplyBuff()` 自己不创建 BuffInstance，也不自己决定是否合并。

但它也不只是“类型校验 + 转发”。当前实现里它先做一轮自己的前置输入校验：

- `OwnerStateComponent` 是否可解析
- `BuffDefId` 是否有效
- `Instigator` 是否有效
- `StateManager` 是否存在

现在这些前置失败分支里，只要 `UTcsStateComponent` 已经可用，`ApplyBuff()` 就会补发 `NotifyStateApplyFailed()`：

- `BuffDefId` 无效 -> `InvalidInput`
- `Instigator` 无效 -> `InvalidInput`
- `StateManager` 无法解析 -> `InvalidInput`
- `BuffDefId` 对应定义不存在 -> `InvalidStateDefinition`
- 定义存在但不是 `UTcsBuffDefinition` -> `InvalidStateDefinition`

唯一仍然只能直接返回 `false` 的情况，是连 `OwnerStateComponent` 自己都解析不到；因为失败事件的广播面本身就挂在 `UTcsStateComponent` 上，没有组件也就没有可用的广播宿主。

在通过这些前置检查之后，它才做下面两件事：

1. 确保目标定义真的是 `UTcsBuffDefinition`。
2. 把请求转发给 `UTcsStateComponent::TryApplyState()`。

这意味着 Buff 申请路径和普通状态申请路径在“创建实例、参数求值、条件检查、入槽、激活、移除”这些核心环节上共用同一条主流程，但两者在最外层入口的前置校验与失败上报语义上并不完全一致。

## 3.4 Buff 实例运行时初始化

### 3.4.1 `UTcsBuffInstance::InitializeRuntimeParameters`

Buff 派生实例在 `UTcsStateInstance::Initialize()` 阶段会进入自己的 `InitializeRuntimeParameters()`。

它主要做五件事情：

1. `ResolveOwnerBuffComponent()`，确保 Buff 宿主已经建立好，并且事件已经绑定。
2. 按 `DurationType` 初始化 Buff 自己的 `TotalDuration` 与 `RemainingDuration`。
3. 把 `UTcsBuffDefinition::Period` 缓存到 Buff 自己的 `Period`。
4. 把 `UTcsBuffDefinition::MaxStackCount` 缓存到 Buff 自己的 `MaxStackCount`。
5. 如果允许叠层，则把 `StackCount` 初始值写成 `1`。

这一步非常关键，因为它发生在实例真正入槽之前。也就是说，新建出来的 Buff 实例在进入共享 State 主流程时，已经具备：

- Buff 宿主绑定
- 总时长字段
- 剩余时长字段
- 周期字段
- 最大叠层数字段
- 初始叠层字段

这里也意味着一个明确变化：`TotalDuration` / `RemainingDuration` / `Period` / `MaxStackCount` / `StackCount` 这类 Buff 专属值，当前已经不再借道 `UTcsStateInstance::NumericParameters`，而是直接作为 `UTcsBuffInstance` 自己的专用成员存在。

## 3.5 Buff 成功进入主流程后的注册

调用链：

```text
StateComponent.NotifyStateApplySuccess
  -> UTcsBuffComponent::HandleOwnerStateApplySuccess
     -> RegisterBuffInstance
        -> DurationTracker.Add
```

### 3.5.1 `HandleOwnerStateApplySuccess`

Buff 组件只处理属于自己 Owner 的成功应用事件，然后调用 `RegisterBuffInstance()`。

### 3.5.2 `RegisterBuffInstance`

`RegisterBuffInstance()` 会先把 `UTcsStateInstance` 转成 `UTcsBuffInstance`，只有转换成功才继续。

如果该 Buff 的持续时间类型是 `SDT_Duration`，就把该实例加入 `FTcsBuffDurationTracker` 的跟踪集合。

这里要注意：当前 `FTcsBuffDurationTracker` 已经不再保存“实例 -> 剩余时间”的数值映射，它现在只负责维护“哪些有限时长 Buff 需要被 `BuffComponent` 每帧推进”的注册表。真正的 `RemainingDuration` 值已经下沉到 `UTcsBuffInstance` 自己持有。

这说明当前的时长跟踪只对“有限时长 Buff”生效；无限时长 Buff 不进入倒计时表。

## 3.6 Buff 时长倒计时与自然过期

调用链：

```text
UTcsBuffComponent::TickComponent
  -> TickBuffLifecycles
     -> 遍历 DurationTracker.TrackedInstances
     -> 直接修改 BuffInstance.RemainingDuration
     -> 到零后 ExpireBuffInstance
        -> RemoveBuffInstance(...Expired)
           -> StateComponent->RequestStateRemoval
```

### 3.6.1 `TickBuffLifecycles`

这个函数只对 `DurationTracker` 中登记的有限时长 Buff 实例进行管理。

每帧处理逻辑是：

1. 跳过无效实例。
2. 跳过已经 `SS_Expired` 的实例，并加入无效清理列表。
3. 只对 `SS_Active` 或 `SS_HangUp` 阶段的实例做时长推进。
4. 把剩余时间减去 `DeltaTime`。
5. 如果归零，则加入 `ExpiredStates`。
6. 对所有过期实例调用 `ExpireBuffInstance()`。
7. 对无效实例调用 `UnregisterBuffInstance()`。

这里有一个值得注意的实现选择：

- `SS_HangUp` 状态下，Buff 仍然会继续流逝时长。
- `SS_Pause` 状态下，不会继续推进时长。

这不是抽象设计推导，而是当前代码里的明确行为。

### 3.6.2 `ExpireBuffInstance`

`ExpireBuffInstance()` 本身不直接改 Stage，也不自己从容器里删实例。

它只是把移除原因定为 `TcsBuffRemovalReasons::Expired`，然后重新走回 State 模块统一移除链。

这保证了：

- Buff 过期和普通 State 过期共享同样的清理流程。
- SourceHandle 清理、槽位重排、事件广播不会因为 Buff 过期而出现分叉逻辑。

## 3.7 Buff 合并流程

这是 Buff 模块最核心、也最特殊的一条链。

调用链：

```text
UTcsStateComponent::UpdateStateSlotActivation
  -> Broadcast OnPrepareStateSlotActivation
     -> UTcsBuffComponent::HandleOwnerStateSlotActivation
        -> ProcessBuffMerging(StateSlot)
           -> 按 StateDefId 分组
           -> MergeBuffStateGroup(每组)
              -> BuffInstance.GetMergerType()
              -> UTcsBuffMerger::Merge
           -> RemoveMergedOutBuffs
              -> RequestStateRemoval(...MergedOut)
```

### 3.7.1 合并发生的时机

合并不是在 `ApplyBuff()` 入口处发生，也不是在 `NotifyStateApplySuccess()` 之后发生。

它发生在：

- 状态已经被加入 `StateSlot->States`
- 但本轮申请还没有最终被记入 `StateInstanceIndex`
- 也还没有广播 `ApplySuccess`

的这个中间阶段。

因此当前 Buff 合并语义更准确地说是：

- “新候选实例先进入槽位候选集，再在槽位刷新过程中与同定义 Buff 发生合并，最后只有幸存者才算正式生效。”

### 3.7.2 `ProcessBuffMerging`

这个函数会先把同一槽位内可解析成 `UTcsBuffInstance` 的实例筛出来，再按 `StateDefId` 分组。

原因很直接：

- 只对“同一 Buff 定义”的状态做合并。
- 不同 `StateDefId` 的 Buff 不在这里互相吞并。

然后它会对每组调用 `MergeBuffStateGroup()`。

### 3.7.3 `MergeBuffStateGroup`

这个函数会：

1. 直接接收 `UTcsBuffInstance` 数组作为合并输入，不再让 merger 接口继续暴露共享 `UTcsStateInstance`。
2. 从实例上拿到 `GetMergerType()`。
3. 通过 `MergerClass->GetDefaultObject<UTcsBuffMerger>()` 取出合并策略对象。
4. 调用 `Merger->Merge(StatesToMerge, OutMergedStates, OutMergedOutBuffs)`。

这里说明当前 Buff 合并策略的来源链是：

```text
UTcsBuffDefinition::MergerType
  -> UTcsBuffInstance::GetMergerType()
  -> UTcsBuffMerger::Merge
```

也就是说，具体如何“保留谁、淘汰谁、怎么改叠层”，完全由合并器实现决定，`UTcsBuffComponent` 只负责编排调用和后续清理。

这里还有一个这轮迁移补漏后的关键收口：`UTcsBuffMerger::Merge()` 的三个参数现在都已经直接使用 `UTcsBuffInstance`，不再让 Buff 合并链继续以 `UTcsStateInstance` 作为名义类型再在内部反复转型。

这里有一个这轮修正后的关键收敛点：合并器现在不只返回“谁被保留”，还要显式返回“谁是这轮被淘汰的实例”。后面的移除链不再通过“槽位全集减去保留集”去反推淘汰者，而是直接消费 `OutMergedOutBuffs`。

### 3.7.4 `RemoveMergedOutBuffs`

`RemoveMergedOutBuffs()` 现在已经收缩成纯移除函数。

它只做两件事：

1. 遍历 `MergedOutBuffs` 中显式登记的淘汰者。
2. 对仍留在槽位里的淘汰者调用 `StateComponent->RequestStateRemoval(State, TcsBuffRemovalReasons::MergedOut)`。

这里再次体现了 Buff 和 State 的边界：

- Buff 合并器负责显式判定谁被保留、谁被淘汰。
- State 负责真正把淘汰者从系统中收走。

## 3.8 Buff 叠层与时长 API

### 3.8.1 时长 API

`UTcsBuffComponent` 提供了三类时长接口：

- `GetBuffRemainingDuration()`
- `RefreshBuffRemainingDuration()`
- `SetBuffRemainingDuration()`

而 `UTcsBuffInstance` 自己还提供：

- `ResetRemainingDuration()`
- `GetTotalDuration()`
- `SetTotalDuration()`

语义分别是：

- 读当前剩余时长。
- 把 `RemainingDuration` 恢复成当前实例持有的 `TotalDuration`。
- 直接写入新的剩余时长。

有限时长 Buff 的剩余时长当前直接来自 `UTcsBuffInstance::RemainingDuration`；无限时长 Buff 则返回 `-1.0f`。`BuffComponent` 保留的 `DurationTracker` 现在只负责登记“哪些实例需要参与时长 Tick”，而不再托管实际数值。

这里还要注意：`GetTotalDuration()` 当前读的是 `UTcsBuffInstance::TotalDuration`。它在初始化时默认来自 `UTcsBuffDefinition::Duration`，但之后可以通过 `SetTotalDuration()` 直接改写。因此 `ResetRemainingDuration()` 的语义，是把剩余时长恢复成“当前 Buff 实例持有的总时长值”，不一定等于定义资产上的原始持续时间。

### 3.8.2 周期 API

`UTcsBuffInstance` 当前已经把周期配置抽成了 Buff 自己的专属字段 `Period`，并提供了：

- `HasPeriod()`
- `GetPeriod()`
- `SetPeriod()`

当前这一步的意义主要是完成 Buff 专属数据的归位，把周期值从共享参数表语义里剥离出来。注意这还不等于“周期触发执行链已经完整实现”；它目前首先是一个 Buff 自己持有的专用运行时变量。

当前实现还补上了独立的周期变化事件：当 `UTcsBuffInstance::SetPeriod()` 真正改写周期值时，会通过 `UTcsBuffComponent::NotifyBuffPeriodChanged()` 广播单独通知。这样玩法层可以把“周期配置被调整”和“剩余时长被刷新”明确区分开，而不是混在 `OnBuffDurationRefreshed` 这类别的语义里。

### 3.8.3 叠层 API

BuffInstance 的叠层逻辑都在 `UTcsBuffInstance` 里：

- `GetStackCount()`
- `GetMaxStackCount()`
- `SetMaxStackCount()`
- `ResetMaxStackCount()`
- `SetStackCount()`
- `AddStack()`
- `RemoveStack()`

这里要先区分两个层面：

- `MaxStackCount` 现在是 `UTcsBuffInstance` 自己持有的“运行时可修改最大层数”
- `StackCount` 是 `UTcsBuffInstance` 自己持有的“当前实际层数”

其中 `MaxStackCount` 在初始化时默认来自 `UTcsBuffDefinition::MaxStackCount`，但之后可以通过 `SetMaxStackCount()` 在运行时调整，也可以通过 `ResetMaxStackCount()` 恢复为定义资产上的默认值。

其中 `SetStackCount()` 最关键：

1. 先按 `MaxStackCount` 做 Clamp。
2. 如果结果不变，直接返回。
3. 如果新叠层数变成 `0`，不自己销毁，而是回调 `BuffComponent->RemoveBuffInstance(this, StackDepleted)`。
4. 否则写回 `UTcsBuffInstance::StackCount`，并通过 `NotifyBuffStackChanged()` 广播叠层变化。

`SetMaxStackCount()` 的当前实现还有一个配套语义：如果新的最大层数低于当前 `StackCount`，它会立即把当前层数也裁到新的上限，避免实例出现“当前层数已经超过最大层数”的非法状态。

此外，当前实现还额外提供了独立的最大叠层数变化事件：当 `MaxStackCount` 真正发生变化时，`UTcsBuffInstance::SetMaxStackCount()` 会通过 `UTcsBuffComponent::NotifyBuffMaxStackCountChanged()` 广播单独的变化通知。这个事件和普通的 `NotifyBuffStackChanged()` 分开，便于玩法层区分“上限变化”和“当前层数变化”两类语义。

因此叠层耗尽也不是 Buff 本地私自删除，而是回到 State 主流程统一收敛。

## 3.9 Buff 移除同步与调试展示

### 3.9.1 移除同步

调用链：

```text
StateComponent.NotifyStateRemoved
  -> UTcsBuffComponent::HandleOwnerStateRemoved
     -> UnregisterBuffInstance
        -> DurationTracker.Remove
```

这意味着 Buff 组件从不假设“自己一定能第一个知道实例离开系统”，而是以 State 的 Removed 事件作为最终事实来源。

### 3.9.2 调试叠加层

调用链：

```text
UTcsStateComponent::BuildStateDebugOverlay
  -> Broadcast OnBuildStateDebugOverlay
     -> UTcsBuffComponent::HandleOwnerStateDebugOverlay
        -> GetDebugStateOverlay
```

`GetDebugStateOverlay()` 会为 Buff 补两项信息：

- `OutStackCount`
- `OutDurationText`

其中：

- 无限时长显示 `Inf`
- 有限时长显示剩余秒数文本

## 四、State 与 Buff 的关键协作断点

## 4.1 Buff 申请并不绕开 State 主流程

`ApplyBuff()` 只是 Buff 语义入口，不是单独的实例提交流程。真正的应用路径仍然是：

```text
ApplyBuff -> TryApplyState -> TryApplyStateInstance -> TryAssignStateToStateSlot
```

## 4.2 Buff 合并发生在 ApplySuccess 之前

这决定了当前系统具备下面这个性质：

- 新 Buff 候选者可以在“正式成功应用”之前就被合并淘汰。
- 因此不会污染 `StateInstanceIndex`。
- 也不会误触发 `OnStateApplySuccess`。

## 4.3 Buff 的所有离场都收敛到 State 移除链

无论 Buff 为什么离开系统：

- 自然过期
- 主动移除
- 合并淘汰
- 叠层耗尽

最终都会进入：

```text
UTcsStateComponent::RequestStateRemoval
  -> UTcsStateComponent::FinalizeStateRemoval
```

## 4.4 当前责任边界已经很清晰

可以把当前边界总结成一张表：

| 事项 | State 模块 | Buff 模块 |
|------|------------|-----------|
| 创建实例 | 负责 | 不负责 |
| 参数求值 | 负责 | 通过派生实例补运行时参数 |
| 应用条件 | 负责 | 不负责 |
| 槽位归属 | 负责 | 不负责 |
| 激活模式裁决 | 负责 | 不负责 |
| 外层 StateTree Gate 联动 | 负责（需外层树显式配置通知任务） | 不负责 |
| 内层 StateTree 启停与 Tick | 负责 | 不负责 |
| 时长跟踪 | 不负责 | 负责 |
| 合并策略执行 | 提供扩展点 | 负责编排和收敛 |
| 叠层语义 | 不负责 | 负责 |
| 移除总收敛 | 负责 | 借道调用 |
| SourceHandle Modifier 清理 | 负责 | 不负责 |
| 调试叠层信息 | 提供扩展点 | 负责补充 |

## 五、阅读当前实现时最值得记住的几个结论

1. `UTcsStateComponent` 是共享状态宿主，`UTcsBuffComponent` 是语义扩展层，不是第二宿主。
2. 状态申请不是“先成功再整理”，而是“先进入槽位候选集，再经过刷新/合并/裁决，最后幸存者才正式成功”。
3. 外层 StateTree 只负责槽位 Gate，内层 StateTree 才负责单个状态实例自己的逻辑；不过当前 Gate 联动还依赖外层树里显式配置状态变化通知任务，不是自动内建链路。
4. Pause / HangUp 当前的实质是停止调度 Tick，而不是销毁内层 StateTree 运行现场。
5. Buff 的过期、合并淘汰、叠层耗尽都不会自己私下删除实例，最终都回到 State 的统一移除链。
6. 当前 Buff 生命周期的绝大多数关键动作，都不是主动轮询整个状态系统，而是挂在 State 的事件与扩展点上。

如果后续还要继续拆分 State / Buff / Skill 的职责，这份文档里的调用链就是当前实现的真实边界基线。