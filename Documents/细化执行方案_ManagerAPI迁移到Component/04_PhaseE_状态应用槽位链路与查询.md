# Phase E：状态应用、槽位链路与查询迁移

> 本文档精确到代码行号，可直接定位执行。
> 文件路径缩写见 [总��文档](./00_总览与索引.md)。

---

## E-1. 状态创建与应用主流程（6 个函数）

### 1. TryApplyStateToTarget（保留在 Manager，改为薄转发）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:186-192` |
| **实现** | `StateMgr.cpp:762-811`（50 行） |
| **签名** | `bool TryApplyStateToTarget(AActor* Target, FName StateDefId, AActor* Instigator, int32 StateLevel = 1, const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle())` |
| **UFUNCTION** | BlueprintCallable |
| **最终形态** | 保留在 Manager，但只做门面：校验 Target → 解析 StateComponent → `StateComp->TryApplyState(...)` |

### 2. TryApplyState（新增到 StateComponent）

这是迁移后 StateComponent 的入口方法，由 `TryApplyStateToTarget` 内部逻辑拆出。

| 项 | 详情 |
|----|------|
| **当前不存在** | 需从 `TryApplyStateToTarget`（`StateMgr.cpp:762-811`）中提取 actor-local 逻辑 |
| **迁移后签名** | `virtual bool TryApplyState(FName StateDefId, AActor* Instigator, int32 StateLevel = 1, const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle())` |
| **实现** | 获取定义 → `CreateStateInstance()` → `TryApplyStateInstance()` → 失败则 `NotifyStateApplyFailed()` |

### 3. CreateStateInstance

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:137-142`（protected） |
| **实现** | `StateMgr.cpp:411-527`（117 行） |
| **签名** | `UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Owner, AActor* Instigator, int32 InLevel = 1, const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle())` |
| **迁移后签名** | `virtual UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Instigator, int32 InLevel = 1, const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle())` |

**关键迁移替换**:

| 原代码 | 替换为 | 行号 |
|--------|--------|------|
| `Owner` 参数 | `GetOwner()` | 参数移除 |
| `GetStateDefinitionAsset(StateDefRowId)` | `ResolveStateManager()->GetStateDefinitionAsset(StateDefRowId)` | 427 |
| `++GlobalStateInstanceIdMgr` | `ResolveStateManager()->AllocateStateInstanceId()` | 459 |
| `NewObject<UTcsStateInstance>(Owner)` | `NewObject<UTcsStateInstance>(GetOwner())` | 456 |
| `UTcsAttributeManagerSubsystem::CreateSourceHandle(...)` | `ResolveAttributeManager()->CreateSourceHandle(...)` | 514-517 |

**因果链构建逻辑**（`StateMgr.cpp:506-524`）：
- 行 506-510: 构建 CausalityChain = ParentSourceHandle.CausalityChain + ParentStateDefAsset.PrimaryAssetId
- 行 514-517: 创建 SourceHandle
- 行 520-524: 设置到 StateInstance

### 4. EvaluateAndApplyStateParameters

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:158-163`（protected） |
| **实现** | `StateMgr.cpp:529-760`（232 行） |
| **签名** | `bool EvaluateAndApplyStateParameters(const UTcsStateDefinitionAsset* StateDefAsset, AActor* Owner, AActor* Instigator, UTcsStateInstance* StateInstance, TArray<FName>& OutFailedParams)` |
| **迁移后签名** | `virtual bool EvaluateAndApplyStateParameters(const UTcsStateDefinitionAsset* StateDefAsset, AActor* Instigator, UTcsStateInstance* StateInstance, TArray<FName>& OutFailedParams)` |
| **内部依赖** | `UTcsStateNumericParamEvaluator::Evaluate(...)` / `UTcsStateBoolParamEvaluator::Evaluate(...)` / `UTcsStateVectorParamEvaluator::Evaluate(...)` |
| **变化** | 移除 `Owner` 参数，内部用 `GetOwner()` |

### 5. CheckStateApplyConditions

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:204`（protected static） |
| **实现** | `StateMgr.cpp:859-902`（44 行） |
| **签名** | `static bool CheckStateApplyConditions(UTcsStateInstance* StateInstance)` |
| **迁移后签名** | `virtual bool CheckStateApplyConditions(UTcsStateInstance* StateInstance)` |
| **内部依赖** | `StateInstance->GetStateDefAsset()` → 遍历 `ActiveConditions` → `UTcsStateCondition::CheckCondition()` |
| **变化** | static → virtual 成员方法 |

### 6. TryApplyStateInstance（State 应用核心入口，独立迁移）

> **定位**：`TryApplyStateInstance` 是整合版 §6.1 明确列出的 **StateComponent 核心 virtual 扩展点**。它既是 `TryApplyState` 的内部步骤，也是对外暴露给"已有 StateInstance、想直接走后半段流程"的公共入口（BlueprintCallable），因此**必须作为独立的迁移条目**，而不是隐藏在 `TryApplyState` 的内部实现里。

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:199-200` |
| **实现** | `StateMgr.cpp:813-857`（45 行） |
| **签名** | `bool TryApplyStateInstance(UTcsStateInstance* StateInstance)` |
| **UFUNCTION** | BlueprintCallable, Category = "State Manager" |
| **迁移后签名** | `virtual bool TryApplyStateInstance(UTcsStateInstance* StateInstance)` |
| **UFUNCTION（迁移后）** | BlueprintCallable, Category = "State" |
| **最终形态** | `UTcsStateComponent` 的核心 virtual 扩展点 |

**内部依赖与替换**：

| 原代码 / 调用 | 替换为 | 行号 |
|---------------|--------|------|
| `StateInstance->IsInitialized()` 校验 | 保持不变 | 822 |
| `StateInstance->GetOwnerStateComponent()` 上的 `NotifyStateApplyFailed(...)` | 改为 `this->NotifyStateApplyFailed(...)`（当 `StateInstance->GetOwner() == GetOwner()` 时） | 827-833, 843-849 |
| `CheckStateApplyConditions(StateInstance)` | `this->CheckStateApplyConditions(StateInstance)`（迁移后为成员 virtual） | 841 |
| `TryAssignStateToStateSlot(StateInstance)` | `this->TryAssignStateToStateSlot(StateInstance)` | 855 |
| `// TODO: 未来实现CheckImmunity` | 原样保留 | 840 |

**归属校验（新增）**：
- 迁移后 `TryApplyStateInstance` 运行在具体 `StateComponent` 上，需要在入口校验 `StateInstance->GetOwner() == GetOwner()`；若不匹配，走 `NotifyStateApplyFailed(ETcsStateApplyFailReason::InvalidInput, ...)` 并返回 false
- 避免调用方把 A Actor 的 StateInstance 递给 B Actor 的 StateComponent

**`TryApplyState` 与 `TryApplyStateInstance` 的分工**：

| 方法 | 职责 | 调用关系 |
|------|------|----------|
| `TryApplyState(DefId, Instigator, Level, ParentSourceHandle)` | 从 DefId 走**完整流程**：创建实例 → 初始化 → 参数求值 → 应用 | 内部调用 `CreateStateInstance()` + `TryApplyStateInstance()` |
| `TryApplyStateInstance(StateInstance)` | 跳过创建，对**已有实例**走**后半段**：条件检查 → 槽位分配 → 索引注册 → 成功通知 | 由 `TryApplyState` 调用；也允许外部直接调用 |

**兼容层映射**（Phase F）：
- `UTcsStateManagerSubsystem::TryApplyStateInstance(UTcsStateInstance*)` 标记 `UE_DEPRECATED`，内部转发到 `StateInstance->GetOwnerStateComponent()->TryApplyStateInstance(StateInstance)`

### TryApplyStateInstance 完整调用链保序

```
TryApplyState (新入口)
  → CreateStateInstance
      → ResolveStateManager()->GetStateDefinitionAsset()
      → NewObject<UTcsStateInstance>
      → Initialize()
      → EvaluateAndApplyStateParameters()
      → ResolveAttributeManager()->CreateSourceHandle() [因果链]
  → TryApplyStateInstance (原 StateMgr.cpp:813-857)
      → CheckStateApplyConditions()           [条件检查]
      → TryAssignStateToStateSlot()            [槽位分配]
      → [槽位处理后] StateInstanceIndex.AddInstance [注意：在合并处理之后才注册]
      → NotifyStateApplySuccess()              [成功通知]
```

**关键保序点**:
- `StateInstanceIndex.AddInstance()` 必须在 `TryAssignStateToStateSlot()` 完成后
- 如果状态在合并中被淘汰（MergedOut），不应写入索引
- 当前实现在 `TryAssignStateToStateSlot` 内部行 1152 才做 `AddInstance`

---

## E-2. 槽位链路（18 个函数）

### 初始化

#### 6. InitStateSlotMappings

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:214` |
| **实现** | `StateMgr.cpp:904-981`（78 行） |
| **签名** | `void InitStateSlotMappings(AActor* CombatEntity)` |
| **迁移后** | `virtual void InitStateSlotMappings()` |
| **内部依赖** | `StateSlotDefinitions` 遍历 → `ResolveStateManager()` 获取 |
| | `StateSlotsX.FindOrAdd(SlotTag)` |
| | `GetStateTree()` → `StateTree->GetStates()` (UE5.6 API) |
| | `Mapping_StateSlotToStateHandle` / `Mapping_StateHandleToStateSlot` |

### 槽位分配

#### 7. TryAssignStateToStateSlot

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:217` |
| **实现** | `StateMgr.cpp:983-1170`（188 行） |
| **签名** | `bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)` |
| **迁移后** | `virtual bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)` |
| **内部依赖** | 槽位查找 → `ClearStateSlotExpiredStates()` → Gate 检查 → 优先级快速拒绝 → `StateSlot->States.Add()` → `DurationTracker.Add()` → `RequestUpdateStateSlotActivation()` → `StateInstanceIndex.AddInstance()` → `NotifyStateApplySuccess()` |
| **关键行号** | Gate关闭拒绝: 1067-1075, PriorityOnly快速拒绝: 1078-1108, 入槽: 1111, Duration跟踪: 1119, 激活请求: 1145, 索引注册: 1152 |

### 刷新与防重入

#### 8. RefreshSlotsForStateChange

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:237-240` |
| **实现** | `StateMgr.cpp:1209-1274`（66 行） |
| **签名** | `void RefreshSlotsForStateChange(UTcsStateComponent* StateComponent, const TArray<FName>& NewStates, const TArray<FName>& OldStates)` |
| **迁移后** | `virtual void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates)` |

#### 9. RequestUpdateStateSlotActivation

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:225` |
| **实现** | `StateMgr.cpp:1276-1301`（26 行） |
| **签名** | `void RequestUpdateStateSlotActivation(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag)` |
| **迁移后** | `void RequestUpdateStateSlotActivation(FGameplayTag StateSlotTag)` |
| **内部依赖** | 检查 `bIsUpdatingSlotActivation` → 延迟入 `PendingSlotActivationUpdates` 或立即执行 `UpdateStateSlotActivation()` |

#### 10. DrainPendingSlotActivationUpdates

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:285`（protected） |
| **实现** | `StateMgr.cpp:1303-1341`（39 行） |
| **签名** | `void DrainPendingSlotActivationUpdates()` |
| **迁移后** | `void DrainPendingSlotActivationUpdates()` |
| **MaxIterations** | 10 次（`StateMgr.cpp:1305`）— 保留 |

### 槽位激活核心流程

#### 11. UpdateStateSlotActivation

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:282`（protected） |
| **实现** | `StateMgr.cpp:1343-1404`（62 行） |
| **签名** | `void UpdateStateSlotActivation(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag)` |
| **迁移后** | `virtual void UpdateStateSlotActivation(FGameplayTag StateSlotTag)` |
| **8步流程** | |
| Step 1 | 设置 `bIsUpdatingSlotActivation = true`（行 1348） |
| Step 2 | `ClearStateSlotExpiredStates()`（行 1372） |
| Step 3 | `SortStatesByPriority()`（行 1375） |
| Step 4 | `ProcessStateSlotMerging()`（行 1378） |
| Step 5 | `EnforceSlotGateConsistency()`（行 1381）+ Gate关闭提前返回（行 1384-1389） |
| Step 6 | `ProcessStateSlotByActivationMode()`（行 1393） |
| Step 7 | `CleanupInvalidStates()`（行 1396） |
| Step 8 | `OnStateSlotChanged` 广播（行 1399）+ 清除标志（行 1402）+ `DrainPendingSlotActivationUpdates()`（行 1403） |

#### 12. EnforceSlotGateConsistency

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:228` |
| **实现** | `StateMgr.cpp:1472-1566`（95 行） |
| **迁移后** | `virtual void EnforceSlotGateConsistency(FGameplayTag StateSlotTag)` |
| **内部依赖** | 按 `GateCloseBehavior` 分派: `HangUpState()` / `PauseState()` / `CancelState()` |
| **安全保障** | Gate关闭时不允许Active残留（行 1543-1550），Shipping 模式 checkf（行 1553-1565） |

### 合并处理

#### 13. ProcessStateSlotMerging

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:300`（protected） |
| **实现** | `StateMgr.cpp:1420-1470`（51 行） |
| **迁移后** | `virtual void ProcessStateSlotMerging(FTcsStateSlot* StateSlot)` |
| **内部依赖** | 按 DefId 分组 → `MergeStateGroup()` → `RemoveUnmergedStates()` |

#### 14. MergeStateGroup

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:302`（protected） |
| **实现** | `StateMgr.cpp:1568-1606`（39 行） |
| **迁移后** | `void MergeStateGroup(TArray<UTcsStateInstance*>& StatesToMerge, TArray<UTcsStateInstance*>& OutMergedStates)` |
| **内部依赖** | `ResolveStateManager()->GetStateDefinitionAsset()` → `StateDef->MergerType` → CDO `UTcsStateMerger::Merge()` |

#### 15. RemoveUnmergedStates

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:304-308`（protected） |
| **实现** | `StateMgr.cpp:1608-1672`（65 行） |
| **迁移后** | `void RemoveUnmergedStates(FTcsStateSlot* StateSlot, const TArray<UTcsStateInstance*>& MergedStates, const TMap<FName, UTcsStateInstance*>& MergePrimaryByDefId)` |
| **内部依赖** | `NotifyStateMerged()` → `RequestStateRemoval(State, TcsStateRemovalReasons::MergedOut)` |

### 激活模式处理

#### 16. ProcessStateSlotByActivationMode

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:311`（protected） |
| **实现** | `StateMgr.cpp:1674-1706`（33 行） |
| **迁移后** | `virtual void ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag)` |
| **内部依赖** | 分派 `ProcessPriorityOnlyMode()` 或 `ProcessAllActiveMode()` |

#### 17. ProcessPriorityOnlyMode

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:313`（protected） |
| **实现** | `StateMgr.cpp:1708-1815`（108 行） |
| **迁移后** | `void ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinitionAsset* SlotDef)` |
| **内部依赖** | 找最高优先级 → 同优先级策略 `SlotDef->SamePriorityPolicy`（行 1752-1765）→ `ActivateState()` → `ApplyPreemptionPolicyToState()` 或 `CancelState()` |

#### 18. ProcessAllActiveMode

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:315`（protected） |
| **实现** | `StateMgr.cpp:1817-1832`（16 行） |
| **���移后** | `void ProcessAllActiveMode(FTcsStateSlot* StateSlot)` |
| **内部依赖** | 对所有非 Active 状态调用 `ActivateState()` |

### 辅助方法

#### 19. ApplyPreemptionPolicyToState

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:317`（protected） |
| **实现** | `StateMgr.cpp:1834-1863`（30 行） |
| **迁移后** | `virtual void ApplyPreemptionPolicyToState(UTcsStateInstance* State, ETcsStatePreemptionPolicy Policy)` |
| **内部依赖** | `HangUpState()` / `PauseState()` |

#### 20. CleanupInvalidStates

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:320`（protected static） |
| **实现** | `StateMgr.cpp:1865-1876`（12 行） |
| **迁移后** | `void CleanupInvalidStates(FTcsStateSlot* StateSlot)` |
| **变化** | static → 成员方法 |

#### 21. RemoveStateFromSlot

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:322`（protected） |
| **实现** | `StateMgr.cpp:1878-1896`（19 行） |
| **迁移后** | `void RemoveStateFromSlot(FTcsStateSlot* StateSlot, UTcsStateInstance* State, bool bDeactivateIfNeeded = true)` |
| **内部依赖** | `DeactivateState()` |

#### 22. ClearStateSlotExpiredStates（改为成员方法）

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:294`（protected static） |
| **实现** | `StateMgr.cpp:1172-1207`（36 行） |
| **签名** | `static void ClearStateSlotExpiredStates(UTcsStateComponent* StateComponent, FTcsStateSlot* StateSlot)` |
| **迁移后** | `void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot)` — **必须改为成员方法** |
| **内部依赖** | `StateTreeTickScheduler.Remove()`（行 1197）、`DurationTracker.Remove()`（行 1198）、`StateInstanceIndex.RemoveInstance()`（行 1199）、三个容器的 `RefreshInstances()`（行 1204-1206） |
| **变化理由** | 需要访问组件内部缓存（Scheduler/Duration/Index），不能是无 this 的 static |

#### 23. SortStatesByPriority

| 项 | 详情 |
|----|------|
| **声明** | `StateMgr.h:297`（protected static） |
| **实现** | `StateMgr.cpp:1406-1418`（13 行） |
| **签名** | `static void SortStatesByPriority(TArray<UTcsStateInstance*>& States)` |
| **迁移后** | `virtual void SortStatesByPriority(TArray<UTcsStateInstance*>& States)` — 成员 virtual 方法 |
| **可见性** | protected |
| **变化** | static → virtual 成员方法（不保留 static） |
| **设计理由** | 排序比较器是合理的子类扩展点：项目可能需要按 DefId / 自定义 Tag / 时戳权重 / 多关键字比较等方式重定义优先级。保持 static 会阻止这种扩展，与本轮迁移「Component 子类为扩展单位」的原则不一致 |
| **性能说明** | virtual 方法调用开销在排序本身的 O(N log N) 面前可忽略，且大部分调用站在单槽内执行（N 通常很小） |

---

## E-3. 防重入状态迁移

### 需要迁移的成员变量

| 变量 | 当前位置 | 迁移后位置 |
|------|---------|-----------|
| `bIsUpdatingSlotActivation` | `StateMgr.h:288` | `StateCmp.h` (protected) |
| `PendingSlotActivationUpdates` | `StateMgr.h:292` | `StateCmp.h` (protected) |

### 类型变化 & 设计修正

当前 `PendingSlotActivationUpdates` 是 `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>>`（全局 Manager 管理多个 Component 的队列），`bIsUpdatingSlotActivation` 也是全局标志。

**这是原有设计的缺陷**：`UpdateStateSlotActivation` 的 8 步流程全部操作传入的单个 Component 的 `StateSlotsX`，内部没有任何跨 Actor 调用，防重入本来就应该是 Actor 自身的状态管理逻辑。全局锁会导致 Actor A 的槽位更新意外阻塞无关的 Actor B 的更新请求。

迁移到 Component 后不仅是简化，而是**修正了原有设计缺陷**，让防重入粒度与实际操作粒度一致：

```cpp
protected:
    bool bIsUpdatingSlotActivation = false;
    TSet<FGameplayTag> PendingSlotActivationUpdates;
```

### 使用位置对照

| 操作 | 当前行号 | 所在函数 |
|------|---------|---------|
| 检查标志 | `StateMgr.cpp:1287` | `RequestUpdateStateSlotActivation` |
| 设为 true | `StateMgr.cpp:1348` | `UpdateStateSlotActivation` |
| 提前返回重置 | `StateMgr.cpp:1355, 1357, 1387-1388` | `UpdateStateSlotActivation` |
| 正常结束重置 | `StateMgr.cpp:1402` | `UpdateStateSlotActivation` |
| 入队 | `StateMgr.cpp:1290` | `RequestUpdateStateSlotActivation` |
| IsEmpty | `StateMgr.cpp:1308` | `DrainPendingSlotActivationUpdates` |
| 拷贝+清空 | `StateMgr.cpp:1311-1312` | `DrainPendingSlotActivationUpdates` |
| 超限清空 | `StateMgr.cpp:1339` | `DrainPendingSlotActivationUpdates` |

### S2 场景保护：用 `TGuardValue` 管理防重入标志（必做，见整合版 §11.2）

迁移后 `UpdateStateSlotActivation` 必须用 `TGuardValue<bool>` 管理 `bIsUpdatingSlotActivation`，理由：
- 旧实现用"手动置 true + 多处手动置 false"模式（上表的 4 处重置），一旦未来新增提前 `return` 分支就可能遗漏重置，导致"卡死防重入锁"——后续所有同帧请求都被当作重入而入队，但入队者再也不会被 drain
- `TGuardValue` 在作用域结束（含 `return` / 异常）时自动还原，完全消除这类遗漏

**迁移后实现骨架**：
```cpp
void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag SlotTag)
{
    if (bIsUpdatingSlotActivation)
    {
        PendingSlotActivationUpdates.Add(SlotTag);
        return;
    }

    // 作用域守卫：无论从哪个分支返回都会自动置回 false
    TGuardValue<bool> Guard(bIsUpdatingSlotActivation, true);

    // ... 原 8 步流程（StateMgr.cpp:1343-1404），删除手动的 bIsUpdatingSlotActivation = false 赋值 ...

    DrainPendingSlotActivationUpdates();
}
```

**相关测试**（Phase E PR 必带）：`FTcs_SameFrameMultiApplySpec`——同帧对同一 Actor 多次 `TryApplyState`，断言最终 Slot 状态正确、`PendingSlotActivationUpdates` 为空、无重复激活/停用事件。

---

## E-4. 查询 API 下沉（5 个函数）

### 当前实现与优化方向

| 函数 | 当前位置 | 当前行范围 | 当前实现方式 | 优化方向 |
|------|---------|-----------|-------------|---------|
| `GetStatesInSlot` | `StateMgr.h:337-341` | `cpp:2203-2230` | 遍历 `StateSlotsX.Find(SlotTag)` | 改用 `StateInstanceIndex.GetInstancesBySlot()` |
| `GetStatesByDefId` | `StateMgr.h:350-354` | `cpp:2232-2258` | 遍历**所有**`StateSlotsX` 逐个比较 DefId | 改用 `StateInstanceIndex.GetInstancesByName()` |
| `GetAllActiveStates` | `StateMgr.h:362-365` | `cpp:2260-2285` | 遍历所有`StateSlotsX`筛选 Active | 遍历 `StateInstanceIndex.Instances` |
| `HasStateWithDefId` | `StateMgr.h:373-376` | `cpp:2287-2310` | 遍历所有`StateSlotsX` | 改用 `StateInstanceIndex.InstancesByName.Contains()` |
| `HasActiveStateInSlot` | `StateMgr.h:383-386` | `cpp:2312-2336` | `StateSlotsX.Find` 后遍历 | 改用 `StateInstanceIndex.GetInstancesBySlot()` + 筛选 |

### StateInstanceIndex 数据结构

**定义**: `TcsStateContainer.h:31-65`

```cpp
struct FTcsStateInstanceIndex
{
    TArray<UTcsStateInstance*> Instances;                           // 全量列表
    TMap<int32, UTcsStateInstance*> InstancesById;                 // 按 ID 索引
    TMap<FName, FTcsStateInstanceArray> InstancesByName;           // 按 DefId 索引
    TMap<FGameplayTag, FTcsStateInstanceArray> InstancesBySlot;    // 按 SlotTag 索引
};
```

**在 StateComponent 中的声明**: `StateCmp.h:296`

**当前使用**（仅增删，查询未用）：

| 操作 | 行号 | 函数 |
|------|------|------|
| `AddInstance` | `StateMgr.cpp:1152` | `TryAssignStateToStateSlot` |
| `RemoveInstance` | `StateMgr.cpp:1199` | `ClearStateSlotExpiredStates` |
| `RemoveInstance` | `StateMgr.cpp:2564` | `FinalizeStateRemoval` |
| `RefreshInstances` | `StateMgr.cpp:1206` | `ClearStateSlotExpiredStates` |
| 读取 `Instances` | `StateCmp.cpp:735` | `GetStateDebugSnapshot` |

### 迁移后签名

```cpp
// 全部在 UTcsStateComponent 上：均不加 virtual（设计决定，见下方说明）
bool GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const;
bool GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const;
bool GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const;
bool HasStateWithDefId(FName StateDefId) const;
bool HasActiveStateInSlot(FGameplayTag SlotTag) const;
```

**注意**: 查询 API 不需要 `StateComponent` 参数（已是 Component 自身方法），也不应在查询中偷偷调用 `RefreshInstances()` 修改运行时状态。

### 为什么查询 API 不标 `virtual`（设计决定，非遗漏）

上述 5 个查询 API 有意设为非 `virtual`，不列入整合版 §6.1 的「核心 virtual 扩展点」清单：

| 理由 | 说明 |
|------|------|
| 纯读语义 | 查询不改状态；子类覆写容易引入副作用或绕过 `StateInstanceIndex`，破坏契约 |
| 索引是内部缓存 | `StateInstanceIndex` 的维护责任属于写路径（`TryAssignStateToStateSlot` / `FinalizeStateRemoval`），不应因覆写查询而耦合给子类 |
| 延伸点在业务层 | Tag/Gate 等过滤需求应在业务层基于查询结果构建新 API，而不是覆写现有 5 个查询 |
| 与写路径对比 | `TryApplyState*` / `CreateStateInstance` / `CheckStateApplyConditions` 等子类有合理插入需求，才标 virtual；查询没有等价动机 |

未来确实需要扩展查询行为时，应单独设计 `BlueprintNativeEvent` 或 Hook 点，**不在本次迁移一道为 5 个查询 API 补 `virtual`**。

---

## E-5. StateComponent 内部调用点修改

### BeginPlay（`StateCmp.cpp:24-47`）

| 行号 | 当前调用 | 迁移后 |
|------|---------|--------|
| 43 | `StateMgr->InitStateSlotMappings(GetOwner())` | `InitStateSlotMappings()` |

**时序保持**: 先获取 Manager 缓存 → `InitStateSlotMappings()` → `Super::BeginPlay()`

### SetSlotGateOpen（`StateCmp.cpp:785-810`）

| 行号 | 当前调用 | 迁移后 |
|------|---------|--------|
| 807 | `StateMgr->RequestUpdateStateSlotActivation(this, SlotTag)` | `RequestUpdateStateSlotActivation(SlotTag)` |

### OnStateTreeStateChanged（`StateCmp.cpp:851-870`）

| 行号 | 当前调用 | 迁移后 |
|------|---------|--------|
| 862 | `StateMgr->RefreshSlotsForStateChange(this, CurrentActiveStates, CachedActiveStateNames)` | `RefreshSlotsForStateChange(CurrentActiveStates, CachedActiveStateNames)` |

---

## E-附录：状态应用完整调用链

```
TryApplyStateToTarget (StateMgr, 保留为门面)
  → StateComp->TryApplyState()                    [新入口]
    → ResolveStateManager()->GetStateDefinitionAsset()
    → CreateStateInstance()                         [从StateMgr迁入]
        → NewObject<UTcsStateInstance>(GetOwner())
        → Initialize()
        → EvaluateAndApplyStateParameters()         [从StateMgr迁入]
        → ResolveAttributeManager()->CreateSourceHandle() [因果链]
    → TryApplyStateInstance()                       [从StateMgr迁入]
        → CheckStateApplyConditions()               [从StateMgr迁入]
        → TryAssignStateToStateSlot()               [从StateMgr迁入]
            → ClearStateSlotExpiredStates()
            → Gate/Priority 快速拒绝
            → StateSlot->States.Add()
            → DurationTracker.Add()
            → RequestUpdateStateSlotActivation()
                → UpdateStateSlotActivation()       [8步流程]
                    → ClearStateSlotExpiredStates
                    → SortStatesByPriority
                    → ProcessStateSlotMerging
                        → MergeStateGroup → RemoveUnmergedStates
                    → EnforceSlotGateConsistency
                    → ProcessStateSlotByActivationMode
                        → ProcessPriorityOnlyMode / ProcessAllActiveMode
                    → CleanupInvalidStates
                    → OnStateSlotChanged
                    → DrainPendingSlotActivationUpdates
            → StateInstanceIndex.AddInstance()
            → NotifyStateApplySuccess()
```
