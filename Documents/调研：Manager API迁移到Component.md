# TCS 重构：Manager API 迁移到 Component 执行方案

## Context

TCS 插件当前所有核心业务逻辑集中在两个 GameInstanceSubsystem 中（StateManagerSubsystem、AttributeManagerSubsystem），对应的 Component 只是纯数据容器。开发者无法通过子类化 Component 来重写/扩展 TCS 的核心流程。

**目标**：将业务逻辑从 Subsystem 下沉到 Component，并标记关键方法为 virtual，使开发者能通过继承 Component 来定制战斗系统行为。

**前置条件**：StateRemoval 流程简化已完成（两阶段→单阶段）。

**设计决策**：
- Subsystem 不保留转发 API（用户决策），**唯一例外**：`TryApplyStateToTarget` 保留在 StateManagerSubsystem 上作为跨 Actor 便捷入口
- Subsystem 只保留全局职责：定义缓存/加载、全局 ID 分配、Tag 映射、CreateSourceHandle
- Component 接收所有业务逻辑并标记关键方法 virtual

---

## Phase A：AttributeManagerSubsystem → AttributeComponent

> Attribute 系统比 State 系统简单，先迁移

### A-1. AttributeManagerSubsystem 新增全局 ID 分配公开接口

**文件**：`Public/Attribute/TcsAttributeManagerSubsystem.h`

在 `#pragma region AttributeDefinitions` 中新增 public 方法（供 Component 调用）：

```cpp
public:
    // 获取属性定义资产（供 Component 内部使用）
    const UTcsAttributeDefinitionAsset* GetAttributeDefinitionAsset(FName AttributeName) const;

    // 获取属性修改器定义资产（供 Component 内部使用）
    const UTcsAttributeModifierDefinitionAsset* GetModifierDefinitionAsset(FName ModifierId) const;
```

在 `#pragma region AttributeInstance` 中将 protected ID 管理器改为 public 分配接口：

```cpp
public:
    // 分配全局唯一的属性实例 ID
    int32 AllocateAttributeInstanceId() { return ++GlobalAttributeInstanceIdMgr; }
```

在 `#pragma region AttributeModifier` 中同理：

```cpp
public:
    // 分配全局唯一的修改器实例 ID
    int32 AllocateModifierInstanceId() { return ++GlobalAttributeModifierInstanceIdMgr; }

    // 分配全局唯一的修改器变更批次 ID
    int64 AllocateModifierChangeBatchId() { return ++GlobalAttributeModifierChangeBatchIdMgr; }
```

**实现**：`GetAttributeDefinitionAsset` 和 `GetModifierDefinitionAsset` 从 `AttributeDefinitions` / `AttributeModifierDefinitions` 中查找并返回，逻辑从现有 `AddAttribute` 和 `CreateAttributeModifier` 中提取。

### A-2. AttributeComponent 新增 AttrMgr 缓存指针

**文件**：`Public/Attribute/TcsAttributeComponent.h`

新增前向声明和 protected 成员：

```cpp
class UTcsAttributeManagerSubsystem;

protected:
    // 属性管理器子系统缓存（BeginPlay 中初始化）
    UPROPERTY()
    TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;
```

**文件**：`Private/Attribute/TcsAttributeComponent.cpp`

`BeginPlay()` 中初始化：

```cpp
void UTcsAttributeComponent::BeginPlay()
{
    Super::BeginPlay();

    UWorld* World = GetWorld();
    if (World && World->GetGameInstance())
    {
        AttrMgr = World->GetGameInstance()->GetSubsystem<UTcsAttributeManagerSubsystem>();
    }
}
```

### A-3. 迁移计算方法（static → 成员方法 virtual）

**从 AttributeManagerSubsystem 迁移到 AttributeComponent**：

| 原方法（Subsystem static） | 新方法（Component virtual） | 签名变化 |
|---|---|---|
| `static void RecalculateAttributeBaseValues(const AActor*, const TArray<...>&)` | `virtual void RecalculateAttributeBaseValues(const TArray<FTcsAttributeModifierInstance>& Modifiers)` | 删除 `AActor*` 参数（用自身数据） |
| `static void RecalculateAttributeCurrentValues(const AActor*, int64)` | `virtual void RecalculateAttributeCurrentValues(int64 ChangeBatchId = -1)` | 同上 |
| `static void MergeAttributeModifiers(const AActor*, const TArray<...>&, TArray<...>&)` | `virtual void MergeAttributeModifiers(const TArray<...>& Modifiers, TArray<...>& OutMerged)` | 同上 |
| `static void ClampAttributeValueInRange(UTcsAttributeComponent*, const FName&, float&, ...)` | `virtual void ClampAttributeValueInRange(const FName& AttributeName, float& NewValue, ...)` | 删除 `AttributeComponent*` 参数 |
| `static void EnforceAttributeRangeConstraints(UTcsAttributeComponent*)` | `virtual void EnforceAttributeRangeConstraints()` | 同上 |
| `static UTcsAttributeComponent* GetAttributeComponent(const AActor*)` | 删除（不再需要，Component 就是自身） | — |

**实现迁移**：将 `TcsAttributeManagerSubsystem.cpp` 中这些 static 方法的函数体直接搬到 `TcsAttributeComponent.cpp`，把所有 `GetAttributeComponent(CombatEntity)` 替换为 `this`，把所有 `CombatEntity->...` 替换为 `GetOwner()->...`（仅在需要 Actor 引用时）。

**访问级别**：protected virtual，因为这些是内部计算方法，不需要暴露给蓝图。

### A-4. 迁移属性管理方法（public virtual）

| 原方法（Subsystem） | 新方法（Component） | 签名变化 |
|---|---|---|
| `bool AddAttribute(AActor*, FName, float)` | `virtual bool AddAttribute(FName AttributeName, float InitValue = 0.f)` | 删除 `AActor*`，通过 `AttrMgr` 获取定义和 ID |
| `void AddAttributes(AActor*, const TArray<FName>&)` | `void AddAttributes(const TArray<FName>& AttributeNames)` | 非 virtual，循环调用 `AddAttribute` |
| `bool AddAttributeByTag(AActor*, const FGameplayTag&, float)` | `bool AddAttributeByTag(const FGameplayTag& AttributeTag, float InitValue = 0.f)` | 非 virtual，通过 `AttrMgr->TryResolveAttributeNameByTag` 后调用 `AddAttribute` |
| `bool SetAttributeBaseValue(AActor*, FName, float, bool)` | `virtual bool SetAttributeBaseValue(FName AttributeName, float NewValue, bool bTriggerEvents = true)` | 删除 `AActor*` |
| `bool SetAttributeCurrentValue(AActor*, FName, float, bool)` | `virtual bool SetAttributeCurrentValue(FName AttributeName, float NewValue, bool bTriggerEvents = true)` | 同上 |
| `bool ResetAttribute(AActor*, FName)` | `virtual bool ResetAttribute(FName AttributeName)` | 同上 |
| `bool RemoveAttribute(AActor*, FName)` | `virtual bool RemoveAttribute(FName AttributeName)` | 同上 |

**UFUNCTION Meta 变化**：
- 删除 `Meta = (DefaultToSelf = "CombatEntity")`（Component 本身就在 Actor 上）
- 保留 `UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))`

### A-5. 迁移 Modifier 管理方法

| 原方法（Subsystem） | 新方法（Component） | 签名变化 |
|---|---|---|
| `bool CreateAttributeModifier(FName, AActor* Instigator, AActor* Target, Out&)` | `virtual bool CreateAttributeModifier(FName ModifierId, AActor* Instigator, FTcsAttributeModifierInstance& Out)` | 删除 `Target`（隐式为 GetOwner()），通过 `AttrMgr` 获取定义和 ID |
| `bool CreateAttributeModifierWithOperands(FName, AActor*, AActor*, TMap, Out&)` | `virtual bool CreateAttributeModifierWithOperands(FName ModifierId, AActor* Instigator, const TMap<FName, float>& Operands, Out&)` | 同上 |
| `void ApplyModifier(AActor*, TArray<...>&)` | `virtual void ApplyModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` | 删除 `AActor*`，通过 `AttrMgr->AllocateModifierChangeBatchId()` 获取 BatchId |
| `bool ApplyModifierWithSourceHandle(AActor*, const FTcsSourceHandle&, const TArray<FName>&, TArray<...>&)` | `virtual bool ApplyModifierWithSourceHandle(const FTcsSourceHandle& SourceHandle, const TArray<FName>& ModifierIds, TArray<...>& Out)` | 删除 `AActor*` |
| `void RemoveModifier(AActor*, TArray<...>&)` | `virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` | 同上 |
| `bool RemoveModifiersBySourceHandle(AActor*, const FTcsSourceHandle&)` | `virtual bool RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)` | 同上 |
| `bool GetModifiersBySourceHandle(AActor*, const FTcsSourceHandle&, TArray<...>&) const` | `bool GetModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle, TArray<...>& Out) const` | 非 virtual，纯查询 |
| `void HandleModifierUpdated(AActor*, TArray<...>&)` | `virtual void HandleModifierUpdated(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers)` | 同上 |

### A-6. 清理 AttributeManagerSubsystem

迁移完成后，从 `TcsAttributeManagerSubsystem.h/cpp` 中删除所有已迁移的方法。

**Subsystem 最终保留的 API**：
- `Initialize` / `LoadFromDeveloperSettings` / `LoadFromAssetManager`
- `GetAttributeDefinitionAsset` / `GetModifierDefinitionAsset`（新增 public）
- `TryResolveAttributeNameByTag` / `TryGetAttributeTagByName`
- `AllocateAttributeInstanceId` / `AllocateModifierInstanceId` / `AllocateModifierChangeBatchId`（新增 public）
- `CreateSourceHandle`
- 删除 `GetAttributeComponent`（static 工具方法，不再需要）

### A-7. 更新 AttributeManagerSubsystem 的外部调用点

**文件**：`Private/State/TcsStateManagerSubsystem.cpp` 中 `FinalizeStateRemoval`

原代码：
```cpp
if (UTcsAttributeManagerSubsystem* AttrMgr = GetGameInstance()->GetSubsystem<UTcsAttributeManagerSubsystem>())
{
    AttrMgr->RemoveModifiersBySourceHandle(StateInstance->GetOwner(), StateInstance->GetSourceHandle());
}
```

改为（通过 EntityInterface 获取 AttributeComponent）：
```cpp
if (AActor* OwnerActor = StateInstance->GetOwner())
{
    if (OwnerActor->Implements<UTcsEntityInterface>())
    {
        if (UTcsAttributeComponent* AttrComp = ITcsEntityInterface::Execute_GetAttributeComponent(OwnerActor))
        {
            AttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
        }
    }
}
```

---

## Phase B：StateManagerSubsystem → StateComponent

### B-1. StateManagerSubsystem 新增全局 ID 分配公开接口

**文件**：`Public/State/TcsStateManagerSubsystem.h`

在 `#pragma region MetaData` 中：

```cpp
public:
    // 分配全局唯一的状态实例 ID
    int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }
```

### B-2. 迁移查询方法到 StateComponent

**从 StateManagerSubsystem 删除，新增到 StateComponent**：

| 原方法（Subsystem） | 新方法（Component） | 签名变化 |
|---|---|---|
| `bool GetStatesInSlot(UTcsStateComponent*, FGameplayTag, TArray<...>&) const` | `bool GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const` | 删除 `StateComponent*`，操作自身 `StateSlotsX` |
| `bool GetStatesByDefId(UTcsStateComponent*, FName, TArray<...>&) const` | `bool GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const` | 同上 |
| `bool GetAllActiveStates(UTcsStateComponent*, TArray<...>&) const` | `bool GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const` | 同上 |
| `bool HasStateWithDefId(UTcsStateComponent*, FName) const` | `bool HasStateWithDefId(FName StateDefId) const` | 同上 |
| `bool HasActiveStateInSlot(UTcsStateComponent*, FGameplayTag) const` | `bool HasActiveStateInSlot(FGameplayTag SlotTag) const` | 同上 |

所有标记为 `UFUNCTION(BlueprintCallable)`，非 virtual。

### B-3. 迁移生命周期方法到 StateComponent

| 原方法（Subsystem） | 新方法（Component） | virtual |
|---|---|---|
| `void ActivateState(UTcsStateInstance*)` | `virtual void ActivateState(UTcsStateInstance* StateInstance)` | virtual |
| `void DeactivateState(UTcsStateInstance*)` | `virtual void DeactivateState(UTcsStateInstance* StateInstance)` | virtual |
| `void HangUpState(UTcsStateInstance*)` | `virtual void HangUpState(UTcsStateInstance* StateInstance)` | virtual |
| `void ResumeState(UTcsStateInstance*)` | `virtual void ResumeState(UTcsStateInstance* StateInstance)` | virtual |
| `void PauseState(UTcsStateInstance*)` | `virtual void PauseState(UTcsStateInstance* StateInstance)` | virtual |
| `void CancelState(UTcsStateInstance*)` | `void CancelState(UTcsStateInstance* StateInstance)` | 非 virtual（转发到 RequestStateRemoval） |
| `void ExpireState(UTcsStateInstance*)` | `void ExpireState(UTcsStateInstance* StateInstance)` | 非 virtual（转发到 RequestStateRemoval） |

### B-4. 迁移移除方法到 StateComponent

| 原方法（Subsystem） | 新方法（Component） | virtual |
|---|---|---|
| `bool RequestStateRemoval(UTcsStateInstance*, FName)` | `virtual bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` | virtual |
| `void FinalizeStateRemoval(UTcsStateInstance*, FName)` | `virtual void FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)` | virtual |
| `bool RemoveState(UTcsStateInstance*)` | `virtual bool RemoveState(UTcsStateInstance* StateInstance)` | virtual |
| `int32 RemoveStatesByDefId(UTcsStateComponent*, FName, bool)` | `virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true)` | virtual |
| `int32 RemoveAllStatesInSlot(UTcsStateComponent*, FGameplayTag)` | `virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag)` | virtual |
| `int32 RemoveAllStates(UTcsStateComponent*)` | `virtual int32 RemoveAllStates()` | virtual |
| `static bool IsStateStillValid(UTcsStateInstance*)` | `static bool IsStateStillValid(UTcsStateInstance* StateInstance)` | static |

### B-5. 迁移核心应用流程到 StateComponent

| 原方法（Subsystem） | 新方法（Component） | virtual |
|---|---|---|
| `UTcsStateInstance* CreateStateInstance(FName, AActor*, AActor*, int32, const FTcsSourceHandle&)` | `virtual UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Instigator, int32 InLevel = 1, const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle())` | virtual（删除 Owner 参数，用 GetOwner()） |
| `bool EvaluateAndApplyStateParameters(...)` | `virtual bool EvaluateAndApplyStateParameters(const UTcsStateDefinitionAsset*, AActor* Instigator, UTcsStateInstance*, TArray<FName>&)` | virtual（删除 Owner 参数） |
| `bool TryApplyStateInstance(UTcsStateInstance*)` | `virtual bool TryApplyStateInstance(UTcsStateInstance* StateInstance)` | virtual |
| `static bool CheckStateApplyConditions(UTcsStateInstance*)` | `virtual bool CheckStateApplyConditions(UTcsStateInstance* StateInstance)` | virtual（从 static 改为成员方法） |

**新增核心入口**（Component 上的对外 API）：

```cpp
// 尝试向自身应用状态（Component 本地入口）
UFUNCTION(BlueprintCallable, Category = "State")
virtual bool TryApplyState(
    FName StateDefId,
    AActor* Instigator,
    int32 StateLevel = 1,
    const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());
```

实现：从 Subsystem 的 `TryApplyStateToTarget` 中提取 per-actor 部分（CreateStateInstance → TryApplyStateInstance）。

### B-6. 迁移槽位管理方法到 StateComponent

| 原方法（Subsystem） | 新方法（Component） | 访问级别 |
|---|---|---|
| `void InitStateSlotMappings(AActor*)` | `void InitStateSlotMappings()` | protected（删除 Actor 参数，用 GetOwner()） |
| `bool TryAssignStateToStateSlot(UTcsStateInstance*)` | `bool TryAssignStateToStateSlot(UTcsStateInstance*)` | protected |
| `void RequestUpdateStateSlotActivation(UTcsStateComponent*, FGameplayTag)` | `void RequestUpdateStateSlotActivation(FGameplayTag SlotTag)` | protected（删除 Component 参数） |
| `void UpdateStateSlotActivation(UTcsStateComponent*, FGameplayTag)` | `virtual void UpdateStateSlotActivation(FGameplayTag SlotTag)` | protected virtual |
| `void DrainPendingSlotActivationUpdates()` | `void DrainPendingSlotActivationUpdates()` | protected |
| `void EnforceSlotGateConsistency(UTcsStateComponent*, FGameplayTag)` | `void EnforceSlotGateConsistency(FGameplayTag SlotTag)` | protected（删除 Component 参数） |
| `void RefreshSlotsForStateChange(UTcsStateComponent*, TArray, TArray)` | `void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates)` | protected |
| `static void ClearStateSlotExpiredStates(UTcsStateComponent*, FTcsStateSlot*)` | `static void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot)` | protected static |
| `static void SortStatesByPriority(TArray<...>&)` | `static void SortStatesByPriority(TArray<UTcsStateInstance*>& States)` | protected static |
| `void ProcessStateSlotMerging(FTcsStateSlot*)` | `void ProcessStateSlotMerging(FTcsStateSlot* StateSlot)` | protected |
| `void MergeStateGroup(...)` | `void MergeStateGroup(...)` | protected |
| `void RemoveUnmergedStates(...)` | `void RemoveUnmergedStates(FTcsStateSlot*, const TArray<...>&, const TMap<...>&)` | protected（删除 Component 参数） |
| `void ProcessStateSlotByActivationMode(...)` | `void ProcessStateSlotByActivationMode(FTcsStateSlot*, FGameplayTag)` | protected（删除 Component 参数） |
| `void ProcessPriorityOnlyMode(...)` | `void ProcessPriorityOnlyMode(...)` | protected |
| `void ProcessAllActiveMode(...)` | `void ProcessAllActiveMode(...)` | protected |
| `void ApplyPreemptionPolicyToState(...)` | `void ApplyPreemptionPolicyToState(...)` | protected |
| `static void CleanupInvalidStates(...)` | `static void CleanupInvalidStates(...)` | protected static |
| `void RemoveStateFromSlot(...)` | `void RemoveStateFromSlot(...)` | protected |

### B-7. 防重入机制迁移

**从 StateManagerSubsystem 迁移到 StateComponent**：

Subsystem 中删除：
```cpp
bool bIsUpdatingSlotActivation = false;
TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> PendingSlotActivationUpdates;
```

Component 中新增：
```cpp
protected:
    // 防重入标志
    bool bIsUpdatingSlotActivation = false;

    // 待处理的槽位激活更新请求
    TSet<FGameplayTag> PendingSlotActivationUpdates;
```

注意：原实现用 `TMap<Component, Set<Tag>>` 是因为 Subsystem 是全局的需要区分 Component。迁移后每个 Component 只需要 `TSet<FGameplayTag>`。

`DrainPendingSlotActivationUpdates` 实现也要简化——不再遍历 Map，直接遍历自身的 `PendingSlotActivationUpdates`。

### B-8. StateManagerSubsystem 保留的最终形态

**保留方法**：
- `Initialize` / `LoadFromDeveloperSettings` / `LoadFromAssetManager` / `LoadStateOnDemand` / `PreloadAllStates` / `PreloadCommonStates`
- `GetStateDefinitionAsset` / `GetStateDefinitionAssetByTag` / `GetStateSlotDefinitionAsset` / `GetStateSlotDefinitionAssetByTag` / `GetAllStateDefNames`
- `AllocateStateInstanceId()`（新增 public）
- `TryApplyStateToTarget(AActor* Target, FName StateDefId, AActor* Instigator, int32, const FTcsSourceHandle&)`（**唯一保留的跨 Actor 便捷 API**）

**`TryApplyStateToTarget` 改为门面实现**：
```cpp
bool UTcsStateManagerSubsystem::TryApplyStateToTarget(
    AActor* Target, FName StateDefId, AActor* Instigator,
    int32 StateLevel, const FTcsSourceHandle& ParentSourceHandle)
{
    if (!IsValid(Target) || !Target->Implements<UTcsEntityInterface>())
    {
        return false;
    }

    UTcsStateComponent* StateComp = ITcsEntityInterface::Execute_GetStateComponent(Target);
    if (!IsValid(StateComp))
    {
        return false;
    }

    return StateComp->TryApplyState(StateDefId, Instigator, StateLevel, ParentSourceHandle);
}
```

**删除所有其他方法**（生命周期、查询、移除、槽位管理等）。

### B-9. 更新 TcsState.cpp 中的 SetStackCount 调用路径

**文件**：`Private/State/TcsState.cpp` `SetStackCount()` 方法

原代码（通过 Subsystem 调用）：
```cpp
UTcsStateManagerSubsystem* StateMgr = World->GetGameInstance()->GetSubsystem<UTcsStateManagerSubsystem>();
if (StateMgr)
{
    StateMgr->RequestStateRemoval(this, FName("StackDepleted"));
}
```

改为（通过已有的 OwnerStateCmp 调用）：
```cpp
if (OwnerStateCmp.IsValid())
{
    OwnerStateCmp->RequestStateRemoval(this, FName("StackDepleted"));
}
```

### B-10. 更新 StateComponent 内部调用点

**`SetSlotGateOpen`**（TcsStateComponent.cpp）中调用了 `StateMgr->RequestUpdateStateSlotActivation(this, SlotTag)`，改为直接调用 `RequestUpdateStateSlotActivation(SlotTag)`。

**`OnStateTreeStateChanged`** 中调用了 `StateMgr->RefreshSlotsForStateChange(this, ...)`，改为直接调用 `RefreshSlotsForStateChange(...)`。

**`BeginPlay`** 中调用了 `StateMgr->InitStateSlotMappings(GetOwner())`，改为直接调用 `InitStateSlotMappings()`。

### B-11. 更新 friend class 声明

**文件**：`Public/State/TcsStateComponent.h` 行 148

StateManagerSubsystem 不再直接操作 Component 数据成员，但 `TryApplyStateToTarget` 门面会调用 `TryApplyState`（public 方法），不需要 friend。

删除：
```cpp
friend class UTcsStateManagerSubsystem;
```

保留：
```cpp
friend class UTcsStateInstance;
```

---

## Phase C：收尾清理

### C-1. 删除 AttributeManagerSubsystem 上已迁移的方法

从头文件和实现文件中删除所有已迁移的方法声明和实现。

### C-2. 删除 StateManagerSubsystem 上已迁移的方法

同上。

### C-3. 更新 include 依赖

- `TcsStateComponent.h` 新增：`#include "TcsSourceHandle.h"`（TryApplyState 参数需要）
- `TcsAttributeComponent.h` 新增：`#include "TcsSourceHandle.h"`（RemoveModifiersBySourceHandle 参数需要）
- 各 cpp 文件中检查不再需要的 include 并清理

### C-4. 更新 GetSlotDebugSnapshot/GetStateDebugSnapshot

这两个方法中引用了 `StateMgr->GetStateSlotDefinitionAssetByTag`。迁移后 StateMgr 仍然可用（Component 持有缓存指针），调用方式不变。

---

## 涉及文件完整清单

| 文件 | 修改类型 |
|------|----------|
| `Public/Attribute/TcsAttributeManagerSubsystem.h` | 新增 ID 分配和定义查询公开接口，删除已迁移方法 |
| `Private/Attribute/TcsAttributeManagerSubsystem.cpp` | 新增公开接口实现，删除已迁移方法实现 |
| `Public/Attribute/TcsAttributeComponent.h` | 大幅扩展，接收所有业务逻辑方法 + virtual |
| `Private/Attribute/TcsAttributeComponent.cpp` | 大幅扩展 |
| `Public/State/TcsStateManagerSubsystem.h` | 新增 ID 分配接口，TryApplyStateToTarget 改为门面，删除其他已迁移方法 |
| `Private/State/TcsStateManagerSubsystem.cpp` | TryApplyStateToTarget 改为转发实现，删除其他已迁移方法实现 |
| `Public/State/TcsStateComponent.h` | 大幅扩展，接收所有业务逻辑方法 + virtual，新增防重入成员 |
| `Private/State/TcsStateComponent.cpp` | 大幅扩展，更新内部调用路径 |
| `Private/State/TcsState.cpp` | SetStackCount 调用路径改为通过 Component |

---

## 执行顺序

1. Phase A-1：AttributeManagerSubsystem 新增公开接口
2. Phase A-2：AttributeComponent 新增 AttrMgr 缓存指针
3. Phase A-3：迁移计算方法
4. Phase A-4 + A-5：迁移属性管理和 Modifier 管理方法
5. Phase A-6：清理 AttributeManagerSubsystem
6. Phase A-7：更新 FinalizeStateRemoval 中的调用路径
7. **编译验证（Phase A 完成）**
8. Phase B-1：StateManagerSubsystem 新增公开接口
9. Phase B-2 + B-3：迁移查询和生命周期方法
10. Phase B-4：迁移移除方法
11. Phase B-5：迁移核心应用流程
12. Phase B-6 + B-7：迁移槽位管理和防重入机制
13. Phase B-8：StateManagerSubsystem 精简为门面
14. Phase B-9 + B-10 + B-11：更新调用点和 friend
15. **编译验证（Phase B 完成）**
16. Phase C：收尾清理

---

## 验证方式

1. UBT 编译验证（Editor Development 配置）
2. 搜索所有对旧 Subsystem API 的调用点（Grep `AttributeManagerSubsystem->` 和 `StateManagerSubsystem->`），确保已更新
3. 验证 `TryApplyStateToTarget` 门面能正确转发到 Component
4. 验证 `FinalizeStateRemoval` 能通过 AttributeComponent 清理 Modifier
