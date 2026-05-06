# Phase D：StateRemoval 与生命周期迁移到 StateComponent

> 本文档精确到代码行号，可直接定位执行。
> 文件路径缩写见 [总览文档](./00_总览与索引.md)。

---

## D-1. 需要迁移的 14 个函数

### 生命周期方法（8 个）

#### 1. ActivateState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:442` |
| **实现** | `StateMgr.cpp:1898-1974`（77 行） |
| **签名** | `void ActivateState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual void ActivateState(UTcsStateInstance* StateInstance)` |
| **内部依赖** | `StateInstance->SetCurrentStage(SS_Active)` |
| | `StateInstance->RestartStateTree()` / `TickStateTree()` / `StopStateTree()` |
| | `StateComponent->StateTreeTickScheduler`（添加/管理 Tick） |
| | `StateComponent->NotifyStateStageChanged()` |
| | `StateDef->TickPolicy` |

#### 2. DeactivateState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:445` |
| **实现** | `StateMgr.cpp:1976-2014`（39 行） |
| **签名** | `void DeactivateState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual void DeactivateState(UTcsStateInstance* StateInstance)` |
| **内部依赖** | `StateInstance->SetCurrentStage(SS_Inactive)` |
| | `StateInstance->PauseStateTree()` |
| | `StateComponent->StateTreeTickScheduler.Remove()` |
| | `StateComponent->NotifyStateStageChanged()` / `NotifyStateDeactivated()` |

#### 3. HangUpState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:448` |
| **实现** | `StateMgr.cpp:2016-2049`（34 行） |
| **签名** | `void HangUpState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual void HangUpState(UTcsStateInstance* StateInstance)` |
| **内部依赖** | 同 `DeactivateState`，但阶段设为 `SS_HangUp` |

#### 4. ResumeState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:451` |
| **实现** | `StateMgr.cpp:2051-2122`（72 行） |
| **签名** | `void ResumeState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual void ResumeState(UTcsStateInstance* StateInstance)` |
| **内部依赖** | 同 `ActivateState`，从 HangUp/Pause 恢复到 Active |

#### 5. PauseState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:454` |
| **实现** | `StateMgr.cpp:2124-2159`（36 行） |
| **签名** | `void PauseState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual void PauseState(UTcsStateInstance* StateInstance)` |
| **内部依赖** | 同 `DeactivateState`，但阶段设为 `SS_Pause` |

#### 6. CancelState（非 virtual 包装器）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:457` |
| **实现** | `StateMgr.cpp:2161-2171`（11 行） |
| **签名** | `void CancelState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `void CancelState(UTcsStateInstance* StateInstance)` |
| **实现** | 直接委托到 `RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Cancelled)` |

#### 7. ExpireState（非 virtual 包装器）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:460` |
| **实现** | `StateMgr.cpp:2173-2185`（13 行） |
| **签名** | `void ExpireState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `void ExpireState(UTcsStateInstance* StateInstance)` |
| **实现** | 直接委托到 `RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Expired)` |

#### 8. IsStateStillValid（static → 成员方法）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:472`（protected static） |
| **实现** | `StateMgr.cpp:2481-2515`（35 行） |
| **签名** | `static bool IsStateStillValid(UTcsStateInstance* StateInstance)` |
| **迁移后** | `bool IsStateStillValid(UTcsStateInstance* StateInstance) const` |
| **内部依赖** | `StateInstance->GetCurrentStage()`、`GetOwnerStateComponent()`、`GetStateDef()`、`StateSlotsX` |

---

### 移除 API（6 个）

#### 9. RequestStateRemoval

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:462-463` |
| **实现** | `StateMgr.cpp:2187-2201`（15 行） |
| **签名** | `bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` |
| **迁移后** | `virtual bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` |
| **UFUNCTION** | BlueprintCallable |
| **内部依赖** | 幂等检查（`GetCurrentStage() == SS_Expired` 则返回） → `FinalizeStateRemoval()` |

#### 10. FinalizeStateRemoval（核心，详见 D-2）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:475`（protected） |
| **实现** | `StateMgr.cpp:2517-2592`（76 行） |
| **签名** | `void FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` |
| **迁移后** | `virtual void FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` |

#### 11. RemoveState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:400-401` |
| **实现** | `StateMgr.cpp:2338-2376`（39 行） |
| **签名** | `bool RemoveState(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual bool RemoveState(UTcsStateInstance* StateInstance)` |
| **UFUNCTION** | BlueprintCallable |
| **内部依赖** | 校验 → `RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Removed)` |

#### 12. RemoveStatesByDefId

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:410-414` |
| **实现** | `StateMgr.cpp:2378-2421`（44 行） |
| **原签名** | `int32 RemoveStatesByDefId(UTcsStateComponent* StateComponent, FName StateDefId, bool bRemoveAll = true)` |
| **迁移后** | `virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true)` |
| **UFUNCTION** | BlueprintCallable |
| **内部依赖** | 遍历 `StateSlotsX` → `RequestStateRemoval()` |

#### 13. RemoveAllStatesInSlot

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:422-425` |
| **实现** | `StateMgr.cpp:2423-2451`（29 行） |
| **原签名** | `int32 RemoveAllStatesInSlot(UTcsStateComponent* StateComponent, FGameplayTag SlotTag)` |
| **迁移后** | `virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag)` |
| **UFUNCTION** | BlueprintCallable |
| **内部依赖** | `StateSlotsX.Find(SlotTag)` → 遍历 → `RequestStateRemoval()` |

#### 14. RemoveAllStates

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:432-433` |
| **实现** | `StateMgr.cpp:2453-2479`（27 行） |
| **原签名** | `int32 RemoveAllStates(UTcsStateComponent* StateComponent)` |
| **迁移后** | `virtual int32 RemoveAllStates()` |
| **UFUNCTION** | BlueprintCallable |
| **内部依赖** | 遍历所有 `StateSlotsX` → `RequestStateRemoval()` |
| **S3 场景防护** | 见整合版 §11.3。**UE 5.6 引擎侧 `FStateTreeExecutionContext::Stop()` 已对"自 Tick / Task 内重入 Stop"做延迟处理**（`StateTreeExecutionContext.cpp:1292-1300`：`if (Exec.CurrentPhase != Unset) { Exec.RequestedStop = ...; return Running; }`），迁移层**无需自建** `bIsInStateTreeCallback`/`bPendingRemoveAllStates` 兜底 Flag。但 `FinalizeStateRemoval.Step2~8`（Scheduler/Duration/Index 清理、Modifier 移除、`MarkPendingGC`）会在 StateTree 延迟 Stop 之前同步执行，可能早于 Task 的 `ExitState`。处置：① 调用方合约约束"归还在帧间触发"；② `RemoveAllStates` 入口加 `ensureMsgf(!IsInStateTreeUpdateContext(), ...)` 诊断，不改变控制流；③ 可选保留一个 `bIsInStateTreeCallback` 标志仅用于 ensure，不再做 pending 延迟。 |

---

## D-2. FinalizeStateRemoval 逐行详解

**实现**: `StateMgr.cpp:2517-2592`

| 步骤 | 行号 | 操作 | 迁移注意 |
|------|------|------|---------|
| 校验 | 2519-2521 | `IsValid(StateInstance)` | 不变 |
| 获取元数据 | 2524-2542 | Owner、StateDef、SlotTag、日志 | `Owner` 改为 `GetOwner()` |
| 记录前阶段 | 2544 | `PreviousStage = StateInstance->GetCurrentStage()` | 不变 |
| **Step 1** | 2547-2550 | `if (IsStateTreeRunning()) StopStateTree()` | 不变 |
| **Step 2** | 2553-2556 | `SetCurrentStage(SS_Expired)` — 幂等保护 | 不变 |
| 获取 Component | 2558-2559 | `StateComponent = GetOwnerStateComponent()` | 改为 `this` |
| **Step 3** | 2562 | `StateTreeTickScheduler.Remove(StateInstance)` | 直接访问 `this->` |
| | 2563 | `DurationTracker.Remove(StateInstance)` | 直接访问 `this->` |
| | 2564 | `StateInstanceIndex.RemoveInstance(StateInstance)` | 直接访问 `this->` |
| **Step 3.5** | 2567-2573 | SourceHandle Modifier 清理 | **关键修改点**（见下） |
| **Step 4** | 2576 | `NotifyStateStageChanged(PreviousStage, SS_Expired)` | `this->Notify...` |
| | 2577 | `NotifyStateRemoved(RemovalReason)` | `this->Notify...` |
| **Step 5** | 2579-2583 | 从 `StateSlotsX` 找 Slot → `RemoveStateFromSlot()` | `this->RemoveStateFromSlot()` |
| | 2584 | `RequestUpdateStateSlotActivation(StateComponent, SlotTag)` | `this->RequestUpdateStateSlotActivation(SlotTag)` |
| **Step 6** | 2589 | `StateInstance->MarkPendingGC()` | 不变 |

### Step 3.5 Modifier 清理路径修正

**当前实现**（`StateMgr.cpp:2567-2573`）：

```cpp
if (UTcsAttributeManagerSubsystem* AttrMgr = GetGameInstance()->GetSubsystem<UTcsAttributeManagerSubsystem>())
{
    AttrMgr->RemoveModifiersBySourceHandle(Owner, SourceHandle);
}
```

**迁移后应改为**：

```cpp
if (UTcsAttributeComponent* OwnerAttrComp = StateInstance->GetOwnerAttributeComponent())
{
    OwnerAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
}
```

**原因**:
- 优先使用 `StateInstance` 已缓存的 `OwnerAttributeComponent`，避免多一次跨系统查找
- Phase C 完成后 `RemoveModifiersBySourceHandle` 已是 `AttributeComponent` 的方法

> **硬依赖：Phase C 必须先于 Phase D 完成**
>
> Step 3.5 的迁移后代码直接调用 `UTcsAttributeComponent::RemoveModifiersBySourceHandle(...)`——该 API 是 Phase C-1 的迁移产物，Phase C 合入前不存在。
>
> 因此：
>
> - **不允许并行开发**：两个分支并行会逼迫 D 分支 mock / stub `RemoveModifiersBySourceHandle`，mock 行为与真实行为的偏差会让 D 阶段测试结果失真
> - **不允许"暂时保留旧调用"**：若在 D 阶段暂留 `AttrMgr->RemoveModifiersBySourceHandle(Owner, SourceHandle)`，等 C 合入后再改，会遗留一次"二次替换"，且违反 §5.3「internal call 必须 component-first」
> - **正确做法**：等 Phase C PR 合入 main 后，Phase D 分支从最新 main rebase，再开始 `FinalizeStateRemoval` 的迁移工作

---

## D-3. 必须同步修改的外部调用点

### 调用点 1: SetStackCount → RequestStateRemoval

| 项 | 详情 |
|----|------|
| **文件** | `State.cpp` |
| **方法** | `UTcsStateInstance::SetStackCount()`（行 272-299） |
| **当前调用** | 行 294-296: 获取 `StateMgr` → `StateMgr->RequestStateRemoval(this, FName("StackDepleted"))` |
| **迁移后** | `GetOwnerStateComponent()->RequestStateRemoval(this, TcsStateRemovalReasons::StackDepleted)` |
| **注意** | 需确保 `GetOwnerStateComponent()` 返回有效指针 |

### 调用点 2: UpdateActiveStateDurations → ExpireState

| 项 | 详情 |
|----|------|
| **文件** | `StateCmp.cpp` |
| **方法** | `UTcsStateComponent::UpdateActiveStateDurations()`（行 199-273） |
| **当前调用** | 行 260: `StateMgr->ExpireState(ExpiredState)` |
| **迁移后** | `ExpireState(ExpiredState)` 或 `this->ExpireState(ExpiredState)` |
| **注意** | 变为 Component 内部自调用，无需经过 Manager |

#### 附加修改：S1 场景自毁防护（必做，见整合版 §11.1）

`ExpireState` → `FinalizeStateRemoval` → 移除 Modifier → 属性回调 / 蓝图回调 中，回调可能 `Destroy(Owner)` 或 `UnregisterComponent()`。本方法第二阶段的 `for (ExpiredState : ExpiredStates)` 循环，**每次 `ExpireState` 返回后必须检查 `this` 自身是否仍有效**，否则后续元素会踩悬空指针。

在 `StateCmp.cpp:258-267` 第二阶段循环中按下例修改（`ExpireState` 的调用同步改为本地调用）：

```cpp
for (UTcsStateInstance* ExpiredState : ExpiredStates)
{
    if (IsValid(ExpiredState))
    {
        ExpireState(ExpiredState);                      // 从 StateMgr->ExpireState 改为本地调用

        // S1 防护：ExpireState 链路可能触发用户回调销毁 Owner / Component
        if (IsBeingDestroyed() || !IsValid(GetOwner()))
        {
            return;                                     // 剩余元素不再处理，DurationTracker 随 Component 一起释放
        }
    }
    else
    {
        InvalidStates.Add(ExpiredState);
    }
}
```

**性能说明**：`IsBeingDestroyed()` 是 `UActorComponent` 的原生内联查询，无反射/GC 开销。

### 调用点 3-N: StateMgr 内部互调（Phase E 一并迁移后自动解决）

以下调用点在 `StateMgr.cpp` 内部，随 Phase D+E 全部迁移到 StateComponent 后，变为 Component 内部自调用：

| 调用者 | 被调用方法 | 当前行号 |
|--------|----------|---------|
| `RemoveUnmergedStates()` | `RequestStateRemoval(State, "MergedOut")` | 1669 |
| `EnforceSlotGateConsistency()` | `HangUpState(State)` | 1517 |
| `EnforceSlotGateConsistency()` | `PauseState(State)` | 1523 |
| `EnforceSlotGateConsistency()` | `CancelState(State)` | 1540 |
| `EnforceSlotGateConsistency()` | `HangUpState(State)`（安全兜底） | 1548 |
| `ProcessPriorityOnlyMode()` | `ActivateState(...)` | 1778 |
| `ProcessPriorityOnlyMode()` | `CancelState(State)` | 1813 |
| `ApplyPreemptionPolicyToState()` | `HangUpState(...)` / `PauseState(...)` | 1848/1855 |
| `ProcessAllActiveMode()` | `ActivateState(State)` | 1829 |
| `RemoveStateFromSlot()` | `DeactivateState(State)` | 1894 |
| `FinalizeStateRemoval()` | `RemoveStateFromSlot()` + `RequestUpdateStateSlotActivation()` | 2583-2584 |

---

## D-4. StateComponent 当前已有的相关方法

以下方法已在 StateComponent 上，无需迁移：

| 方法 | 声明行 | 实现行 | 说明 |
|------|--------|--------|------|
| `UpdateActiveStateDurations(float)` | `StateCmp.h:400` | `StateCmp.cpp:199-273` | 每帧 Tick，调用 `ExpireState` |
| `TickStateTrees(float)` | `StateCmp.h:403` | `StateCmp.cpp:57-97` | Tick StateTree |
| `GetStateRemainingDuration()` | `StateCmp.h:388` | `StateCmp.cpp:99-126` | 查询 |
| `RefreshStateRemainingDuration()` | `StateCmp.h:392` | `StateCmp.cpp:128-162` | 刷新 |
| `SetStateRemainingDuration()` | `StateCmp.h:396` | `StateCmp.cpp:164-197` | 设置 |
| `SetSlotGateOpen()` | `StateCmp.h:336` | `StateCmp.cpp:785-810` | Gate 控制 |

### Notify 方法（纯事件广播，全部在 StateComponent 上）

| 方法 | 声明行 |
|------|--------|
| `NotifyStateStageChanged` | `StateCmp.h:163` |
| `NotifyStateDeactivated` | `StateCmp.h:170` |
| `NotifyStateRemoved` | `StateCmp.h:176` |
| `NotifyStateStackChanged` | `StateCmp.h:179` |
| `NotifyStateLevelChanged` | `StateCmp.h:182` |
| `NotifyStateDurationRefreshed` | `StateCmp.h:185` |
| `NotifySlotGateStateChanged` | `StateCmp.h:188` |
| `NotifyStateParameterChanged` | `StateCmp.h:191` |
| `NotifyStateMerged` | `StateCmp.h:199` |
| `NotifyStateApplySuccess` | `StateCmp.h:202` |
| `NotifyStateApplyFailed` | `StateCmp.h:210` |
