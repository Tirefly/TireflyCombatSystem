# Codex

## 可直接删的步骤（低风险）

- 在 RequestStateRemoval 里删掉 StateInstanceIndex.AddInstance(StateInstance)，这一步是冗余的（已有去重）。
- FinalizePendingRemovalRequest 可以保留为公共收敛点，不建议删；但可以把调用入口收敛成更少分支。

## 建议“条件化”的步骤（按策略启用，不是全删）

- StartStateTree -> SendRemovalEvent -> TickStateTree(0) 只对“需要 StateTree 做移除握手”的 State 执行。
- 对“无需握手”的 State 直接 FinalizeStateRemoval，跳过上述三步。

## 必须保留的步骤（不能删）

- FinalizeStateRemoval 内的容器清理、SourceHandle 关联 Modifier 清理、事件广播、槽位重计算、GC 标记。
- TickPendingRemovals 的“树停止后 finalize”逻辑。

## 必须补的自动化兜底（否则仍依赖开发者）

- 给 PendingRemoval 加硬超时（不是只有 warning），超时后强制 StopStateTree + FinalizePendingRemovalRequest。
- 这样开发者即使漏配 RemovalConfirmTask，也不会无限 pending。

## 推荐目标流程（精简后）

- RequestStateRemoval 统一入口。
- 根据 RemovalFlowPolicy 走 DirectFinalize 或 StateTreeHandshake。
- 握��模式下等待树停或硬超时。
- 一律进入 FinalizeStateRemoval 收敛。

# Claude

## 删除 PendingRemoval 机制

**删除的概念**：PendingRemovalRequest、Event_RemovalRequested、两阶段等待

**TcsState.h** — 删除 `#pragma region RemovalRequest` 整个区域：

- 删除字段：`bPendingRemovalRequest`, `PendingRemovalRequest`, `PendingRemovalRequestStartTimeTicks`, `bPendingRemovalRequestWarningIssued`
- 删除方法：`HasPendingRemovalRequest()`, `GetPendingRemovalRequest()`, `ClearPendingRemovalRequest()`, `SetPendingRemovalRequest()`, `GetPendingRemovalRequestStartTimeTicks()`, `HasPendingRemovalRequestWarningIssued()`, `MarkPendingRemovalRequestWarningIssued()`

**TcsState.cpp** — 删除 `ClearPendingRemovalRequest()` 和 `SetPendingRemovalRequest()` 的实现

**评估 FTcsStateRemovalRequest 结构体**：简化为 FName RemovalReason（不再需要结构体包装）。如果 FTcsStateRemovalRequest 只剩 Reason 字段，直接用 FName 替代。

## 简化 StateManagerSubsystem 移除流程

**RequestStateRemoval** — 重写为直接调用 FinalizeStateRemoval：

```cpp
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
    if (!IsValid(StateInstance)) return false;
    if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired) return true;
    FinalizeStateRemoval(StateInstance, RemovalReason);
    return true;
}
```

**FinalizePendingRemovalRequest** — 完全删除

**CancelState / ExpireState** — 简化为直接调用 RequestStateRemoval + FName：

```cpp
void CancelState(UTcsStateInstance* StateInstance)
{
    RequestStateRemoval(StateInstance, FName("Cancelled"));
}
```

**RemoveState / RemoveStatesByDefId / RemoveAllStatesInSlot / RemoveAllStates** — 内部改为直接调用简化后的 RequestStateRemoval

## 简化 StateComponent Tick

**TcsStateComponent.h** — 删除：

- `void TickPendingRemovals()` 声明

**TcsStateComponent.cpp**：

- `TickComponent` 中删除 `TickPendingRemovals()` 调用
- 删除 `TickPendingRemovals()` 整个实现
- `TickStateTrees` 中删除 PendingRemoval 相关的调度逻辑（行 62-74 的 safety check、行 91-92 的 bPendingRemoval 判断）

## 清理 GameplayTag

**TcsGameplayTags** — 如果 `Tcs.Event.RemovalRequested` Tag 仅用于 PendingRemoval 机制，删除其注册

## 删除 StateTree Task

**FTcsStateRemovalConfirmTask** — 删除整个 Task（两个文件）：

- `Public/StateTree/TcsStateRemovalConfirmTask.h`
- `Private/StateTree/TcsStateRemovalConfirmTask.cpp`

原因：该 Task 的唯一职责是在退场分支末尾调用 `Context.Stop()`，且内部直接引用了 `HasPendingRemovalRequest()` 和 `GetPendingRemovalRequest()`（将被删除的 API）。两阶段机制移除后此 Task 无存在意义。

## 删除 ETcsStateRemovalRequestReason 枚举

**TcsState.h** — 删除 `ETcsStateRemovalRequestReason` 枚举（仅被 FTcsStateRemovalRequest 和 RemovalConfirmTask 使用）

**TcsState.h** — 删除 `FTcsStateRemovalRequest` 结构体，所有移除 API 改用 `FName RemovalReason` 参数

---

# Claude 方案执行细节

> 以下所有行号基于当前代码快照。路径相对于 `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/`

## 文件 1: `Public/State/TcsState.h`

### 1-A. 删除 `ETcsStateRemovalRequestReason` 枚举 (行 89-98)

删除从注释 `// StateTree Tick 策略` 后面开始到 `};` 结束的整个枚举块���
```cpp
// 行 89-98 整体删除
// State removal request reason (used for StateTree-driven removal handling)
UENUM(BlueprintType)
enum class ETcsStateRemovalRequestReason : uint8
{
    Removed = 0     UMETA(...),
    Cancelled       UMETA(...),
    Expired         UMETA(...),
    Custom          UMETA(...),
};
```

### 1-B. 删除 `FTcsStateRemovalRequest` 结构体 (行 100-117)

删除整个结构体定义：
```cpp
// 行 100-117 整体删除
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateRemovalRequest
{
    GENERATED_BODY()
    ...
    FName ToRemovalReasonName() const;
};
```

### 1-C. 修正 `ETcsStateTreeTickPolicy` 的注释 (行 119)

原行 89 的注释 `// StateTree Tick 策略` 被复用到了 RemovalRequest 上，需要确保 `ETcsStateTreeTickPolicy` 枚举上方有正确的注释。删除枚举后 `ETcsStateTreeTickPolicy` 直接跟在 `ETcsStateApplyFailReason` 后面，检查空行和注释即可。

### 1-D. 删除 `#pragma region RemovalRequest` 整个区域 (行 631-663)

整体删除：
```cpp
// 行 631-663 整体删除
#pragma region RemovalRequest

public:
    UFUNCTION(BlueprintPure, Category = "State|Removal")
    bool HasPendingRemovalRequest() const { return bPendingRemovalRequest; }

    UFUNCTION(BlueprintPure, Category = "State|Removal")
    FTcsStateRemovalRequest GetPendingRemovalRequest() const { return PendingRemovalRequest; }

    int64 GetPendingRemovalRequestStartTimeTicks() const { ... }
    bool HasPendingRemovalRequestWarningIssued() const { ... }
    void MarkPendingRemovalRequestWarningIssued() { ... }

    UFUNCTION(BlueprintCallable, Category = "State|Removal")
    void ClearPendingRemovalRequest();

    void SetPendingRemovalRequest(const FTcsStateRemovalRequest& Request);

protected:
    UPROPERTY()
    bool bPendingRemovalRequest = false;

    UPROPERTY()
    FTcsStateRemovalRequest PendingRemovalRequest;

    UPROPERTY()
    int64 PendingRemovalRequestStartTimeTicks = 0;

    UPROPERTY()
    bool bPendingRemovalRequestWarningIssued = false;

#pragma endregion
```

---

## 文件 2: `Private/State/TcsState.cpp`

### 2-A. 删除 `FTcsStateRemovalRequest::ToRemovalReasonName` 实现 (行 26-40)

整体删除：
```cpp
// 行 26-40 整体删除
FName FTcsStateRemovalRequest::ToRemovalReasonName() const
{
    switch (Reason)
    {
    case ETcsStateRemovalRequestReason::Removed: ...
    ...
    }
}
```

### 2-B. 删除 `Initialize()` 中的 PendingRemoval 初始化 (行 149-150)

删除这两行：
```cpp
// 行 149-150 删除
bPendingRemovalRequest = false;
PendingRemovalRequest = FTcsStateRemovalRequest();
```

### 2-C. 删除 `ClearPendingRemovalRequest()` 实现 (行 198-204)

整体删除：
```cpp
// 行 198-204 整体删除
void UTcsStateInstance::ClearPendingRemovalRequest()
{
    bPendingRemovalRequest = false;
    PendingRemovalRequest = FTcsStateRemovalRequest();
    PendingRemovalRequestStartTimeTicks = 0;
    bPendingRemovalRequestWarningIssued = false;
}
```

### 2-D. 删除 `SetPendingRemovalRequest()` 实现 (行 206-216)

整体删除：
```cpp
// 行 206-216 整体删除
void UTcsStateInstance::SetPendingRemovalRequest(const FTcsStateRemovalRequest& Request)
{
    ...
}
```

### 2-E. 修改 `SetStackCount()` 中的移除调用 (行 326-344)

原代码 (行 326-344):
```cpp
if (NewStackCount == 0)
{
    UWorld* World = GetWorld();
    if (World)
    {
        UTcsStateManagerSubsystem* StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();
        if (StateMgr)
        {
            FTcsStateRemovalRequest RemovalRequest;
            RemovalRequest.Reason = (InStackCount == 0)
                ? ETcsStateRemovalRequestReason::Removed
                : ETcsStateRemovalRequestReason::Expired;

            StateMgr->RequestStateRemoval(this, RemovalRequest);
        }
    }
    return;
}
```

改为：
```cpp
if (NewStackCount == 0)
{
    UWorld* World = GetWorld();
    if (World)
    {
        UTcsStateManagerSubsystem* StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();
        if (StateMgr)
        {
            StateMgr->RequestStateRemoval(this, FName("StackDepleted"));
        }
    }
    return;
}
```

---

## 文件 3: `Public/State/TcsStateManagerSubsystem.h`

### 3-A. 修改 `RequestStateRemoval` 签名 (行 462-463)

原代码：
```cpp
UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FTcsStateRemovalRequest Request);
```

改为：
```cpp
UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);
```

### 3-B. 删除 `FinalizePendingRemovalRequest` 声明 (行 465)

删除：
```cpp
void FinalizePendingRemovalRequest(UTcsStateInstance* StateInstance);
```

### 3-C. 删除 `#include "State/TcsState.h"` 依赖检查

头文件行 8 `#include "State/TcsState.h"` 引入了 `FTcsStateRemovalRequest`。删除该结构体后，检查此 include 是否仍被需要（答案：是，因为 `ETcsStateStage` 等仍在使用）。无需修改。

---

## 文件 4: `Private/State/TcsStateManagerSubsystem.cpp`

### 4-A. 修改 `CancelState` (行 2183-2194)

原代码：
```cpp
void UTcsStateManagerSubsystem::CancelState(UTcsStateInstance* StateInstance)
{
    ...
    FTcsStateRemovalRequest Request;
    Request.Reason = ETcsStateRemovalRequestReason::Cancelled;
    RequestStateRemoval(StateInstance, Request);
}
```

改为：
```cpp
void UTcsStateManagerSubsystem::CancelState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    RequestStateRemoval(StateInstance, FName("Cancelled"));
}
```

### 4-B. 修改 `ExpireState` (行 2197-2211)

原代码同理，改为：
```cpp
void UTcsStateManagerSubsystem::ExpireState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    RequestStateRemoval(StateInstance, FName("Expired"));
}
```

### 4-C. 重写 `RequestStateRemoval` (行 2213-2261)

替换整个方法体：
```cpp
bool UTcsStateManagerSubsystem::RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
    if (!IsValid(StateInstance))
    {
        return false;
    }

    if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
    {
        return true;
    }

    FinalizeStateRemoval(StateInstance, RemovalReason);
    return true;
}
```

### 4-D. 删除 `FinalizePendingRemovalRequest` (行 2263-2283)

整体删除。

### 4-E. 修改 `RemoveState` (行 2420-2460)

将内部的 `FTcsStateRemovalRequest` 构造改为：
```cpp
// 原代码 (行 2457-2459):
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Removed;
return RequestStateRemoval(StateInstance, Request);
// 改为:
return RequestStateRemoval(StateInstance, FName("Removed"));
```

### 4-F. 修改 `RemoveStatesByDefId` (行 2462-2507)

同理，将所有 `FTcsStateRemovalRequest` 构造 (行 2494-2496) 改为：
```cpp
// 原代码:
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Removed;
RequestStateRemoval(State, Request);
// 改为:
RequestStateRemoval(State, FName("Removed"));
```

### 4-G. 修改 `RemoveAllStatesInSlot` (行 2509-2539)

同理 (行 2531-2533)：
```cpp
RequestStateRemoval(State, FName("Removed"));
```

### 4-H. 修改 `RemoveAllStates` (行 2541-2569)

同理 (行 2559-2561)：
```cpp
RequestStateRemoval(State, FName("Removed"));
```

### 4-I. 修改 `RemoveUnmergedStates` 中的移除调用 (行 1684-1688)

原代码：
```cpp
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Custom;
Request.CustomReason = FName("MergedOut");
RequestStateRemoval(State, Request);
```

改为：
```cpp
RequestStateRemoval(State, FName("MergedOut"));
```

### 4-J. 替换所有 `HasPendingRemovalRequest()` 调用

简化后不存在 PendingRemoval 状态，这些检查的语义需要逐一分析替换：

| 位置(行) | 原代码 | 替换为 | 原因 |
|----------|--------|--------|------|
| 1087 | `Existing->HasPendingRemovalRequest() \|\| Existing->GetCurrentStage() == ETcsStateStage::SS_Expired` | `Existing->GetCurrentStage() == ETcsStateStage::SS_Expired` | PendingRemoval 不再存在，移除即 Expired |
| 1439 | `if (State->HasPendingRemovalRequest()) { AlwaysKeepStates.Add(State); continue; }` | 删除这整个 if 块（行 1438-1443） | 不再有 PendingRemoval 状态 |
| 1518 | `if (State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1518-1521） | 同上 |
| 1568 | `if (IsValid(State) && !State->HasPendingRemovalRequest())` | `if (IsValid(State))` | 同上 |
| 1634 | `if (IsValid(State) && State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1634-1637） | 同上 |
| 1740 | `if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest())` | `if (IsValid(Candidate))` | 同上 |
| 1754 | `if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest())` | `if (IsValid(Candidate))` | 同上 |
| 1810 | `if (State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1810-1813） | 同上 |
| 1849 | `if (IsValid(State) && !State->HasPendingRemovalRequest() && ...)` | `if (IsValid(State) && ...)` | 同上 |

### 4-K. 删除 `#include "TcsGameplayTags.h"` (如果不再被使用)

搜索 `TcsGameplayTags` 在此文件中的其他用途。当前仅行 2251 使用 `TcsGameplayTags::Event_RemovalRequested`，删除该调用后此 include 可移除。

---

## 文件 5: `Public/State/TcsStateComponent.h`

### 5-A. 删除 `TickPendingRemovals` 声明 (行 406)

删除：
```cpp
// 处理 Pending Removal 实例：超时警告 + Finalize 已停止的 StateTree
void TickPendingRemovals();
```

### 5-B. 修改注释 (行 405)

删除行 405 的注释，因为它描述的是已删除的方法。

---

## 文件 6: `Private/State/TcsStateComponent.cpp`

### 6-A. 修改 `TickComponent` (行 49-56)

原代码：
```cpp
void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateActiveStateDurations(DeltaTime);
    TickPendingRemovals();
    TickStateTrees(DeltaTime);
}
```

改为（删除 `TickPendingRemovals()` 调用）：
```cpp
void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateActiveStateDurations(DeltaTime);
    TickStateTrees(DeltaTime);
}
```

### 6-B. 简化 `TickStateTrees` (行 58-118)

删除行 62-74 的 PendingRemoval safety check：
```cpp
// 删除行 62-74
// Safety: ensure pending removal instances are scheduled while running.
for (UTcsStateInstance* StateInstance : StateInstanceIndex.Instances)
{
    if (!IsValid(StateInstance))
    {
        continue;
    }

    if (StateInstance->HasPendingRemovalRequest() && StateInstance->IsStateTreeRunning())
    {
        StateTreeTickScheduler.Add(StateInstance);
    }
}
```

修改行 91-99 的 bShouldTick 判断：
原代码：
```cpp
const bool bPendingRemoval = RunningState->HasPendingRemovalRequest();
bool bShouldTick = bPendingRemoval;
if (!bShouldTick)
{
    const UTcsStateDefinitionAsset* StateDefAsset = RunningState->GetStateDefAsset();
    bShouldTick =
        (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
        (StateDefAsset && StateDefAsset->TickPolicy == ETcsStateTreeTickPolicy::WhileActive);
}
```

改为（删除 PendingRemoval 优先判断）：
```cpp
const UTcsStateDefinitionAsset* StateDefAsset = RunningState->GetStateDefAsset();
const bool bShouldTick =
    (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
    (StateDefAsset && StateDefAsset->TickPolicy == ETcsStateTreeTickPolicy::WhileActive);
```

### 6-C. 删除 `TickPendingRemovals()` 整个实现 (行 120-185)

整体删除。

### 6-D. 修改 `UpdateActiveStateDurations` 中的 PendingRemoval 检查 (行 328-339)

原代码：
```cpp
if (RemainingDuration <= 0.0f)
{
    if (!StateInstance->HasPendingRemovalRequest())
    {
        ExpiredStates.Add(StateInstance);
    }
    continue;
}

RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
if (RemainingDuration <= 0.0f && !StateInstance->HasPendingRemovalRequest())
{
    ExpiredStates.Add(StateInstance);
}
```

改为（去掉 PendingRemoval 检查）：
```cpp
if (RemainingDuration <= 0.0f)
{
    ExpiredStates.Add(StateInstance);
    continue;
}

RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
if (RemainingDuration <= 0.0f)
{
    ExpiredStates.Add(StateInstance);
}
```

### 6-E. 修改调试输出中的 PendingRemoval 显示

**`GetSlotDebugSnapshot` (行 607-611):**
原代码：
```cpp
FString RemovalStr = TEXT("-");
if (State->HasPendingRemovalRequest())
{
    RemovalStr = State->GetPendingRemovalRequest().ToRemovalReasonName().ToString();
}
```
改为（删除 PendingRemoval 分支，保留 RemovalStr 字段用于其他用途或直接删掉 Rem= 输出）：
```cpp
FString RemovalStr = TEXT("-");
```

**`GetStateDebugSnapshot` (行 808-812):**
同理修改：
```cpp
FString RemovalStr = TEXT("-");
```

---

## 文件 7: `Public/TcsGameplayTags.h`

### 7-A. 删除 Event_RemovalRequested 声明 (行 11)

删除：
```cpp
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_RemovalRequested);
```

如果 namespace 因此变空，删除整个 namespace + include：
```cpp
// 整个文件内容变为空，可以清空或删除文件
```

---

## 文件 8: `Private/TcsGameplayTags.cpp`

### 8-A. 删除 Event_RemovalRequested 定义 (行 9)

删除：
```cpp
UE_DEFINE_GAMEPLAY_TAG(Event_RemovalRequested, "Tcs.Event.RemovalRequested");
```

如果 namespace 因此变空，同上处理。

---

## 文件 9: `Public/StateTree/TcsStateRemovalConfirmTask.h` — 整个文件删除

## 文件 10: `Private/StateTree/TcsStateRemovalConfirmTask.cpp` — 整个文件删除

同时需要从 `.Build.cs` 或模块的 `PublicDependencyModuleNames` 中检查是否有对应注册（UE 的 StateTree Task 通过 USTRUCT 自动注册，无需手动注册，所以只需删除文件）。

---

## 文件 11: `FinalizeStateRemoval` 不需要修改

`FinalizeStateRemoval` (行 2607-2682) 的当前实现已经是最终清理逻辑，不涉及 PendingRemoval 概念，无需修改。简化后 `RequestStateRemoval` 直接调用它即可。

---

## 执行顺序建议

1. 先删 `TcsState.h` 中的枚举/结构体/region（文件 1）
2. 再删 `TcsState.cpp` 中的对应实现（文件 2）
3. 修改 `TcsStateManagerSubsystem.h` 签名（文件 3）
4. 修改 `TcsStateManagerSubsystem.cpp` 所有引用点（文件 4，最多修改）
5. 修改 `TcsStateComponent.h`（文件 5）
6. 修改 `TcsStateComponent.cpp`（文件 6）
7. 删除 GameplayTag 文件（文件 7-8）
8. 删除 RemovalConfirmTask 文件（文件 9-10）
9. UBT 编译验证

---

## 风险点

1. **已有的 StateTree 资产**：如果任何 StateTree 资产中使用了 `FTcsStateRemovalConfirmTask` 节点，删除该 Task 后打开 StateTree 编辑器时该节点会显示为无效。需要手动清理这些 StateTree 资产。
2. **蓝图引用**：如果有蓝图调用了 `HasPendingRemovalRequest` 或 `GetPendingRemovalRequest`，编译蓝图时会报错。需要搜索蓝图资产中的引用。
3. **`UpdateActiveStateDurations` 的重复过期问题**：简化后不再有 `HasPendingRemovalRequest()` 防重复检查，但 `RequestStateRemoval` 内部有 `SS_Expired` 短路返回，所以 `ExpireState` 被多次调用不会导致重复移除。安全。
# Codex

## 可直接删的步骤（低风险）

- 在 RequestStateRemoval 里删掉 StateInstanceIndex.AddInstance(StateInstance)，这一步是冗余的（已有去重）。
- FinalizePendingRemovalRequest 可以保留为公共收敛点，不建议删；但可以把调用入口收敛成更少分支。

## 建议“条件化”的步骤（按策略启用，不是全删）

- StartStateTree -> SendRemovalEvent -> TickStateTree(0) 只对“需要 StateTree 做移除握手”的 State 执行。
- 对“无需握手”的 State 直接 FinalizeStateRemoval，跳过上述三步。

## 必须保留的步骤（不能删）

- FinalizeStateRemoval 内的容器清理、SourceHandle 关联 Modifier 清理、事件广播、槽位重计算、GC 标记。
- TickPendingRemovals 的“树停止后 finalize”逻辑。

## 必须补的自动化兜底（否则仍依赖开发者）

- 给 PendingRemoval 加硬超时（不是只有 warning），超时后强制 StopStateTree + FinalizePendingRemovalRequest。
- 这样开发者即使漏配 RemovalConfirmTask，也不会无限 pending。

## 推荐目标流程（精简后）

- RequestStateRemoval 统一入口。
- 根据 RemovalFlowPolicy 走 DirectFinalize 或 StateTreeHandshake。
- 握��模式下等待树停或硬超时。
- 一律进入 FinalizeStateRemoval 收敛。

# Claude

## 删除 PendingRemoval 机制

**删除的概念**：PendingRemovalRequest、Event_RemovalRequested、两阶段等待

**TcsState.h** — 删除 `#pragma region RemovalRequest` 整个区域：

- 删除字段：`bPendingRemovalRequest`, `PendingRemovalRequest`, `PendingRemovalRequestStartTimeTicks`, `bPendingRemovalRequestWarningIssued`
- 删除方法：`HasPendingRemovalRequest()`, `GetPendingRemovalRequest()`, `ClearPendingRemovalRequest()`, `SetPendingRemovalRequest()`, `GetPendingRemovalRequestStartTimeTicks()`, `HasPendingRemovalRequestWarningIssued()`, `MarkPendingRemovalRequestWarningIssued()`

**TcsState.cpp** — 删除 `ClearPendingRemovalRequest()` 和 `SetPendingRemovalRequest()` 的实现

**评估 FTcsStateRemovalRequest 结构体**：简化为 FName RemovalReason（不再需要结构体包装）。如果 FTcsStateRemovalRequest 只剩 Reason 字段，直接用 FName 替代。

## 简化 StateManagerSubsystem 移除流程

**RequestStateRemoval** — 重写为直接调用 FinalizeStateRemoval：

```cpp
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
    if (!IsValid(StateInstance)) return false;
    if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired) return true;
    FinalizeStateRemoval(StateInstance, RemovalReason);
    return true;
}
```

**FinalizePendingRemovalRequest** — 完全删除

**CancelState / ExpireState** — 简化为直接调用 RequestStateRemoval + FName：

```cpp
void CancelState(UTcsStateInstance* StateInstance)
{
    RequestStateRemoval(StateInstance, FName("Cancelled"));
}
```

**RemoveState / RemoveStatesByDefId / RemoveAllStatesInSlot / RemoveAllStates** — 内部改为直接调用简化后的 RequestStateRemoval

## 简化 StateComponent Tick

**TcsStateComponent.h** — 删除：

- `void TickPendingRemovals()` 声明

**TcsStateComponent.cpp**：

- `TickComponent` 中删除 `TickPendingRemovals()` 调用
- 删除 `TickPendingRemovals()` 整个实现
- `TickStateTrees` 中删除 PendingRemoval 相关的调度逻辑（行 62-74 的 safety check、行 91-92 的 bPendingRemoval 判断）

## 清理 GameplayTag

**TcsGameplayTags** — 如果 `Tcs.Event.RemovalRequested` Tag 仅用于 PendingRemoval 机制，删除其注册

## 删除 StateTree Task

**FTcsStateRemovalConfirmTask** — 删除整个 Task（两个文件）：

- `Public/StateTree/TcsStateRemovalConfirmTask.h`
- `Private/StateTree/TcsStateRemovalConfirmTask.cpp`

原因：该 Task 的唯一职责是在退场分支末尾调用 `Context.Stop()`，且内部直接引用了 `HasPendingRemovalRequest()` 和 `GetPendingRemovalRequest()`（将被删除的 API）。两阶段机制移除后此 Task 无存在意义。

## 删除 ETcsStateRemovalRequestReason 枚举

**TcsState.h** — 删除 `ETcsStateRemovalRequestReason` 枚举（仅被 FTcsStateRemovalRequest 和 RemovalConfirmTask 使用）

**TcsState.h** — 删除 `FTcsStateRemovalRequest` 结构体，所有移除 API 改用 `FName RemovalReason` 参数

---

# Claude 方案执行细节

> 以下所有行号基于当前代码快照。路径相对于 `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/`

## 文件 1: `Public/State/TcsState.h`

### 1-A. 删除 `ETcsStateRemovalRequestReason` 枚举 (行 89-98)

删除从注释 `// StateTree Tick 策略` 后面开始到 `};` 结束的整个枚举块���
```cpp
// 行 89-98 整体删除
// State removal request reason (used for StateTree-driven removal handling)
UENUM(BlueprintType)
enum class ETcsStateRemovalRequestReason : uint8
{
    Removed = 0     UMETA(...),
    Cancelled       UMETA(...),
    Expired         UMETA(...),
    Custom          UMETA(...),
};
```

### 1-B. 删除 `FTcsStateRemovalRequest` 结构体 (行 100-117)

删除整个结构体定义：
```cpp
// 行 100-117 整体删除
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateRemovalRequest
{
    GENERATED_BODY()
    ...
    FName ToRemovalReasonName() const;
};
```

### 1-C. 修正 `ETcsStateTreeTickPolicy` 的注释 (行 119)

原行 89 的注释 `// StateTree Tick 策略` 被复用到了 RemovalRequest 上，需要确保 `ETcsStateTreeTickPolicy` 枚举上方有正确的注释。删除枚举后 `ETcsStateTreeTickPolicy` 直接跟在 `ETcsStateApplyFailReason` 后面，检查空行和注释即可。

### 1-D. 删除 `#pragma region RemovalRequest` 整个区域 (行 631-663)

整体删除：
```cpp
// 行 631-663 整体删除
#pragma region RemovalRequest

public:
    UFUNCTION(BlueprintPure, Category = "State|Removal")
    bool HasPendingRemovalRequest() const { return bPendingRemovalRequest; }

    UFUNCTION(BlueprintPure, Category = "State|Removal")
    FTcsStateRemovalRequest GetPendingRemovalRequest() const { return PendingRemovalRequest; }

    int64 GetPendingRemovalRequestStartTimeTicks() const { ... }
    bool HasPendingRemovalRequestWarningIssued() const { ... }
    void MarkPendingRemovalRequestWarningIssued() { ... }

    UFUNCTION(BlueprintCallable, Category = "State|Removal")
    void ClearPendingRemovalRequest();

    void SetPendingRemovalRequest(const FTcsStateRemovalRequest& Request);

protected:
    UPROPERTY()
    bool bPendingRemovalRequest = false;

    UPROPERTY()
    FTcsStateRemovalRequest PendingRemovalRequest;

    UPROPERTY()
    int64 PendingRemovalRequestStartTimeTicks = 0;

    UPROPERTY()
    bool bPendingRemovalRequestWarningIssued = false;

#pragma endregion
```

---

## 文件 2: `Private/State/TcsState.cpp`

### 2-A. 删除 `FTcsStateRemovalRequest::ToRemovalReasonName` 实现 (行 26-40)

整体删除：
```cpp
// 行 26-40 整体删除
FName FTcsStateRemovalRequest::ToRemovalReasonName() const
{
    switch (Reason)
    {
    case ETcsStateRemovalRequestReason::Removed: ...
    ...
    }
}
```

### 2-B. 删除 `Initialize()` 中的 PendingRemoval 初始化 (行 149-150)

删除这两行：
```cpp
// 行 149-150 删除
bPendingRemovalRequest = false;
PendingRemovalRequest = FTcsStateRemovalRequest();
```

### 2-C. 删除 `ClearPendingRemovalRequest()` 实现 (行 198-204)

整体删除：
```cpp
// 行 198-204 整体删除
void UTcsStateInstance::ClearPendingRemovalRequest()
{
    bPendingRemovalRequest = false;
    PendingRemovalRequest = FTcsStateRemovalRequest();
    PendingRemovalRequestStartTimeTicks = 0;
    bPendingRemovalRequestWarningIssued = false;
}
```

### 2-D. 删除 `SetPendingRemovalRequest()` 实现 (行 206-216)

整体删除：
```cpp
// 行 206-216 整体删除
void UTcsStateInstance::SetPendingRemovalRequest(const FTcsStateRemovalRequest& Request)
{
    ...
}
```

### 2-E. 修改 `SetStackCount()` 中的移除调用 (行 326-344)

原代码 (行 326-344):
```cpp
if (NewStackCount == 0)
{
    UWorld* World = GetWorld();
    if (World)
    {
        UTcsStateManagerSubsystem* StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();
        if (StateMgr)
        {
            FTcsStateRemovalRequest RemovalRequest;
            RemovalRequest.Reason = (InStackCount == 0)
                ? ETcsStateRemovalRequestReason::Removed
                : ETcsStateRemovalRequestReason::Expired;

            StateMgr->RequestStateRemoval(this, RemovalRequest);
        }
    }
    return;
}
```

改为：
```cpp
if (NewStackCount == 0)
{
    UWorld* World = GetWorld();
    if (World)
    {
        UTcsStateManagerSubsystem* StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();
        if (StateMgr)
        {
            StateMgr->RequestStateRemoval(this, FName("StackDepleted"));
        }
    }
    return;
}
```

---

## 文件 3: `Public/State/TcsStateManagerSubsystem.h`

### 3-A. 修改 `RequestStateRemoval` 签名 (行 462-463)

原代码：
```cpp
UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FTcsStateRemovalRequest Request);
```

改为：
```cpp
UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);
```

### 3-B. 删除 `FinalizePendingRemovalRequest` 声明 (行 465)

删除：
```cpp
void FinalizePendingRemovalRequest(UTcsStateInstance* StateInstance);
```

### 3-C. 删除 `#include "State/TcsState.h"` 依赖检查

头文件行 8 `#include "State/TcsState.h"` 引入了 `FTcsStateRemovalRequest`。删除该结构体后，检查此 include 是否仍被需要（答案：是，因为 `ETcsStateStage` 等仍在使用）。无需修改。

---

## 文件 4: `Private/State/TcsStateManagerSubsystem.cpp`

### 4-A. 修改 `CancelState` (行 2183-2194)

原代码：
```cpp
void UTcsStateManagerSubsystem::CancelState(UTcsStateInstance* StateInstance)
{
    ...
    FTcsStateRemovalRequest Request;
    Request.Reason = ETcsStateRemovalRequestReason::Cancelled;
    RequestStateRemoval(StateInstance, Request);
}
```

改为：
```cpp
void UTcsStateManagerSubsystem::CancelState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    RequestStateRemoval(StateInstance, FName("Cancelled"));
}
```

### 4-B. 修改 `ExpireState` (行 2197-2211)

原代码同理，改为：
```cpp
void UTcsStateManagerSubsystem::ExpireState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    RequestStateRemoval(StateInstance, FName("Expired"));
}
```

### 4-C. 重写 `RequestStateRemoval` (行 2213-2261)

替换整个方法体：
```cpp
bool UTcsStateManagerSubsystem::RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
    if (!IsValid(StateInstance))
    {
        return false;
    }

    if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
    {
        return true;
    }

    FinalizeStateRemoval(StateInstance, RemovalReason);
    return true;
}
```

### 4-D. 删除 `FinalizePendingRemovalRequest` (行 2263-2283)

整体删除。

### 4-E. 修改 `RemoveState` (行 2420-2460)

将内部的 `FTcsStateRemovalRequest` 构造改为：
```cpp
// 原代码 (行 2457-2459):
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Removed;
return RequestStateRemoval(StateInstance, Request);
// 改为:
return RequestStateRemoval(StateInstance, FName("Removed"));
```

### 4-F. 修改 `RemoveStatesByDefId` (行 2462-2507)

同理，将所有 `FTcsStateRemovalRequest` 构造 (行 2494-2496) 改为：
```cpp
// 原代码:
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Removed;
RequestStateRemoval(State, Request);
// 改为:
RequestStateRemoval(State, FName("Removed"));
```

### 4-G. 修改 `RemoveAllStatesInSlot` (行 2509-2539)

同理 (行 2531-2533)：
```cpp
RequestStateRemoval(State, FName("Removed"));
```

### 4-H. 修改 `RemoveAllStates` (行 2541-2569)

同理 (行 2559-2561)：
```cpp
RequestStateRemoval(State, FName("Removed"));
```

### 4-I. 修改 `RemoveUnmergedStates` 中的移除调用 (行 1684-1688)

原代码：
```cpp
FTcsStateRemovalRequest Request;
Request.Reason = ETcsStateRemovalRequestReason::Custom;
Request.CustomReason = FName("MergedOut");
RequestStateRemoval(State, Request);
```

改为：
```cpp
RequestStateRemoval(State, FName("MergedOut"));
```

### 4-J. 替换所有 `HasPendingRemovalRequest()` 调用

简化后不存在 PendingRemoval 状态，这些检查的语义需要逐一分析替换：

| 位置(行) | 原代码 | 替换为 | 原因 |
|----------|--------|--------|------|
| 1087 | `Existing->HasPendingRemovalRequest() \|\| Existing->GetCurrentStage() == ETcsStateStage::SS_Expired` | `Existing->GetCurrentStage() == ETcsStateStage::SS_Expired` | PendingRemoval 不再存在，移除即 Expired |
| 1439 | `if (State->HasPendingRemovalRequest()) { AlwaysKeepStates.Add(State); continue; }` | 删除这整个 if 块（行 1438-1443） | 不再有 PendingRemoval 状态 |
| 1518 | `if (State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1518-1521） | 同上 |
| 1568 | `if (IsValid(State) && !State->HasPendingRemovalRequest())` | `if (IsValid(State))` | 同上 |
| 1634 | `if (IsValid(State) && State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1634-1637） | 同上 |
| 1740 | `if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest())` | `if (IsValid(Candidate))` | 同上 |
| 1754 | `if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest())` | `if (IsValid(Candidate))` | 同上 |
| 1810 | `if (State->HasPendingRemovalRequest()) { continue; }` | 删除这个 if 块（行 1810-1813） | 同上 |
| 1849 | `if (IsValid(State) && !State->HasPendingRemovalRequest() && ...)` | `if (IsValid(State) && ...)` | 同上 |

### 4-K. 删除 `#include "TcsGameplayTags.h"` (如果不再被使用)

搜索 `TcsGameplayTags` 在此文件中的其他用途。当前仅行 2251 使用 `TcsGameplayTags::Event_RemovalRequested`，删除该调用后此 include 可移除。

---

## 文件 5: `Public/State/TcsStateComponent.h`

### 5-A. 删除 `TickPendingRemovals` 声明 (行 406)

删除：
```cpp
// 处理 Pending Removal 实例：超时警告 + Finalize 已停止的 StateTree
void TickPendingRemovals();
```

### 5-B. 修改注释 (行 405)

删除行 405 的注释，因为它描述的是已删除的方法。

---

## 文件 6: `Private/State/TcsStateComponent.cpp`

### 6-A. 修改 `TickComponent` (行 49-56)

原代码：
```cpp
void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateActiveStateDurations(DeltaTime);
    TickPendingRemovals();
    TickStateTrees(DeltaTime);
}
```

改为（删除 `TickPendingRemovals()` 调用）：
```cpp
void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdateActiveStateDurations(DeltaTime);
    TickStateTrees(DeltaTime);
}
```

### 6-B. 简化 `TickStateTrees` (行 58-118)

删除行 62-74 的 PendingRemoval safety check：
```cpp
// 删除行 62-74
// Safety: ensure pending removal instances are scheduled while running.
for (UTcsStateInstance* StateInstance : StateInstanceIndex.Instances)
{
    if (!IsValid(StateInstance))
    {
        continue;
    }

    if (StateInstance->HasPendingRemovalRequest() && StateInstance->IsStateTreeRunning())
    {
        StateTreeTickScheduler.Add(StateInstance);
    }
}
```

修改行 91-99 的 bShouldTick 判断：
原代码：
```cpp
const bool bPendingRemoval = RunningState->HasPendingRemovalRequest();
bool bShouldTick = bPendingRemoval;
if (!bShouldTick)
{
    const UTcsStateDefinitionAsset* StateDefAsset = RunningState->GetStateDefAsset();
    bShouldTick =
        (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
        (StateDefAsset && StateDefAsset->TickPolicy == ETcsStateTreeTickPolicy::WhileActive);
}
```

改为（删除 PendingRemoval 优先判断）：
```cpp
const UTcsStateDefinitionAsset* StateDefAsset = RunningState->GetStateDefAsset();
const bool bShouldTick =
    (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
    (StateDefAsset && StateDefAsset->TickPolicy == ETcsStateTreeTickPolicy::WhileActive);
```

### 6-C. 删除 `TickPendingRemovals()` 整个实现 (行 120-185)

整体删除。

### 6-D. 修改 `UpdateActiveStateDurations` 中的 PendingRemoval 检查 (行 328-339)

原代码：
```cpp
if (RemainingDuration <= 0.0f)
{
    if (!StateInstance->HasPendingRemovalRequest())
    {
        ExpiredStates.Add(StateInstance);
    }
    continue;
}

RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
if (RemainingDuration <= 0.0f && !StateInstance->HasPendingRemovalRequest())
{
    ExpiredStates.Add(StateInstance);
}
```

改为（去掉 PendingRemoval 检查）：
```cpp
if (RemainingDuration <= 0.0f)
{
    ExpiredStates.Add(StateInstance);
    continue;
}

RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
if (RemainingDuration <= 0.0f)
{
    ExpiredStates.Add(StateInstance);
}
```

### 6-E. 修改调试输出中的 PendingRemoval 显示

**`GetSlotDebugSnapshot` (行 607-611):**
原代码：
```cpp
FString RemovalStr = TEXT("-");
if (State->HasPendingRemovalRequest())
{
    RemovalStr = State->GetPendingRemovalRequest().ToRemovalReasonName().ToString();
}
```
改为（删除 PendingRemoval 分支，保留 RemovalStr 字段用于其他用途或直接删掉 Rem= 输出）：
```cpp
FString RemovalStr = TEXT("-");
```

**`GetStateDebugSnapshot` (行 808-812):**
同理修改：
```cpp
FString RemovalStr = TEXT("-");
```

---

## 文件 7: `Public/TcsGameplayTags.h`

### 7-A. 删除 Event_RemovalRequested 声明 (行 11)

删除：
```cpp
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_RemovalRequested);
```

如果 namespace 因此变空，删除整个 namespace + include：
```cpp
// 整个文件内容变为空，可以清空或删除文件
```

---

## 文件 8: `Private/TcsGameplayTags.cpp`

### 8-A. 删除 Event_RemovalRequested 定义 (行 9)

删除：
```cpp
UE_DEFINE_GAMEPLAY_TAG(Event_RemovalRequested, "Tcs.Event.RemovalRequested");
```

如果 namespace 因此变空，同上处理。

---

## 文件 9: `Public/StateTree/TcsStateRemovalConfirmTask.h` — 整个文件删除

## 文件 10: `Private/StateTree/TcsStateRemovalConfirmTask.cpp` — 整个文件删除

同时需要从 `.Build.cs` 或模块的 `PublicDependencyModuleNames` 中检查是否有对应注册（UE 的 StateTree Task 通过 USTRUCT 自动注册，无需手动注册，所以只需删除文件）。

---

## 文件 11: `FinalizeStateRemoval` 不需要修改

`FinalizeStateRemoval` (行 2607-2682) 的当前实现已经是最终清理逻辑，不涉及 PendingRemoval 概念，无需修改。简化后 `RequestStateRemoval` 直接调用它即可。

---

## 执行顺序建议

1. 先删 `TcsState.h` 中的枚举/结构体/region（文件 1）
2. 再删 `TcsState.cpp` 中的对应实现（文件 2）
3. 修改 `TcsStateManagerSubsystem.h` 签名（文件 3）
4. 修改 `TcsStateManagerSubsystem.cpp` 所有引用点（文件 4，最多修改）
5. 修改 `TcsStateComponent.h`（文件 5）
6. 修改 `TcsStateComponent.cpp`（文件 6）
7. 删除 GameplayTag 文件（文件 7-8）
8. 删除 RemovalConfirmTask 文件（文件 9-10）
9. UBT 编译验证

---

## 风险点

1. **已有的 StateTree 资产**：如果任何 StateTree 资产中使用了 `FTcsStateRemovalConfirmTask` 节点，删除该 Task 后打开 StateTree 编辑器时该节点会显示为无效。需要手动清理这些 StateTree 资产。
2. **蓝图引用**：如果有蓝图调用了 `HasPendingRemovalRequest` 或 `GetPendingRemovalRequest`，编译蓝图时会报错。需要搜索蓝图资产中的引用。
3. **`UpdateActiveStateDurations` 的重复过期问题**：简化后不再有 `HasPendingRemovalRequest()` 防重复检查，但 `RequestStateRemoval` 内部有 `SS_Expired` 短路返回，所以 `ExpireState` 被多次调用不会导致重复移除。安全。
