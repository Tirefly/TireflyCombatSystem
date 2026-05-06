# Phase F & G：兼容层与最终硬删除

> 本文档精确到代码行号，可直接定位执行。
> 文件路径缩写见 [总览文档](./00_总览与索引.md)。

---

## Phase F：临时 deprecated 兼容层

### F-1. StateManagerSubsystem 需标记 deprecated 的 API

#### UFUNCTION（蓝图可见，使用 `DeprecatedFunction` meta）

| 行号 | 函数 | 迁移目标 |
|------|------|---------|
| `StateMgr.h:199-200` | `TryApplyStateInstance(UTcsStateInstance*)` | `StateCmp::TryApplyStateInstance()` |
| `StateMgr.h:337-341` | `GetStatesInSlot(StateCmp*, FGameplayTag, TArray&)` | `StateCmp::GetStatesInSlot()` |
| `StateMgr.h:350-354` | `GetStatesByDefId(StateCmp*, FName, TArray&)` | `StateCmp::GetStatesByDefId()` |
| `StateMgr.h:362-365` | `GetAllActiveStates(StateCmp*, TArray&)` | `StateCmp::GetAllActiveStates()` |
| `StateMgr.h:373-376` | `HasStateWithDefId(StateCmp*, FName)` | `StateCmp::HasStateWithDefId()` |
| `StateMgr.h:383-386` | `HasActiveStateInSlot(StateCmp*, FGameplayTag)` | `StateCmp::HasActiveStateInSlot()` |
| `StateMgr.h:400-401` | `RemoveState(UTcsStateInstance*)` | `StateCmp::RemoveState()` |
| `StateMgr.h:410-414` | `RemoveStatesByDefId(StateCmp*, FName, bool)` | `StateCmp::RemoveStatesByDefId()` |
| `StateMgr.h:422-424` | `RemoveAllStatesInSlot(StateCmp*, FGameplayTag)` | `StateCmp::RemoveAllStatesInSlot()` |
| `StateMgr.h:432-433` | `RemoveAllStates(StateCmp*)` | `StateCmp::RemoveAllStates()` |
| `StateMgr.h:462-463` | `RequestStateRemoval(UTcsStateInstance*, FName)` | `StateCmp::RequestStateRemoval()` |

**共 11 个 UFUNCTION 需标记**

#### 纯 C++（使用 `UE_DEPRECATED`）

| 行号 | 函数 | 说明 |
|------|------|------|
| `StateMgr.h:137-142` | `CreateStateInstance(...)` | protected |
| `StateMgr.h:158-163` | `EvaluateAndApplyStateParameters(...)` | protected |
| `StateMgr.h:204` | `CheckStateApplyConditions(...)` | protected static |
| `StateMgr.h:214` | `InitStateSlotMappings(AActor*)` | public |
| `StateMgr.h:217` | `TryAssignStateToStateSlot(...)` | public |
| `StateMgr.h:225` | `RequestUpdateStateSlotActivation(...)` | public |
| `StateMgr.h:228` | `EnforceSlotGateConsistency(...)` | public |
| `StateMgr.h:237-240` | `RefreshSlotsForStateChange(...)` | public |
| `StateMgr.h:282` | `UpdateStateSlotActivation(...)` | protected |
| `StateMgr.h:285` | `DrainPendingSlotActivationUpdates()` | protected |
| `StateMgr.h:294` | `ClearStateSlotExpiredStates(...)` | protected static |
| `StateMgr.h:297` | `SortStatesByPriority(...)` | protected virtual（static → 成员方法） |
| `StateMgr.h:300` | `ProcessStateSlotMerging(...)` | protected |
| `StateMgr.h:302` | `MergeStateGroup(...)` | protected |
| `StateMgr.h:304-308` | `RemoveUnmergedStates(...)` | protected |
| `StateMgr.h:311` | `ProcessStateSlotByActivationMode(...)` | protected |
| `StateMgr.h:313` | `ProcessPriorityOnlyMode(...)` | protected |
| `StateMgr.h:315` | `ProcessAllActiveMode(...)` | protected |
| `StateMgr.h:317` | `ApplyPreemptionPolicyToState(...)` | protected |
| `StateMgr.h:320` | `CleanupInvalidStates(...)` | protected static |
| `StateMgr.h:322` | `RemoveStateFromSlot(...)` | protected |
| `StateMgr.h:442` | `ActivateState(...)` | public |
| `StateMgr.h:445` | `DeactivateState(...)` | public |
| `StateMgr.h:448` | `HangUpState(...)` | public |
| `StateMgr.h:451` | `ResumeState(...)` | public |
| `StateMgr.h:454` | `PauseState(...)` | public |
| `StateMgr.h:457` | `CancelState(...)` | public |
| `StateMgr.h:460` | `ExpireState(...)` | public |
| `StateMgr.h:472` | `IsStateStillValid(...)` | protected static |
| `StateMgr.h:475` | `FinalizeStateRemoval(...)` | protected |

**共 30 个纯 C++ 方法需标记**

---

### F-2. AttributeManagerSubsystem 需标记 deprecated 的 API

#### UFUNCTION（蓝图可见）

| 行号 | 函数 | 迁移目标 |
|------|------|---------|
| `AttrMgr.h:64-68` | `AddAttribute(AActor*, FName, float)` | `AttrCmp::AddAttribute()` |
| `AttrMgr.h:71-74` | `AddAttributes(AActor*, TArray<FName>&)` | `AttrCmp::AddAttributes()` |
| `AttrMgr.h:84-88` | `AddAttributeByTag(AActor*, FGameplayTag&, float)` | `AttrCmp::AddAttributeByTag()` |
| `AttrMgr.h:123-128` | `SetAttributeBaseValue(AActor*, FName, float, bool)` | `AttrCmp::SetAttributeBaseValue()` |
| `AttrMgr.h:139-144` | `SetAttributeCurrentValue(AActor*, FName, float, bool)` | `AttrCmp::SetAttributeCurrentValue()` |
| `AttrMgr.h:153-156` | `ResetAttribute(AActor*, FName)` | `AttrCmp::ResetAttribute()` |
| `AttrMgr.h:165-168` | `RemoveAttribute(AActor*, FName)` | `AttrCmp::RemoveAttribute()` |
| `AttrMgr.h:194-199` | `CreateAttributeModifier(FName, AActor*, AActor*, Inst&)` | `AttrCmp::CreateAttributeModifier()` |
| `AttrMgr.h:211-217` | `CreateAttributeModifierWithOperands(...)` | `AttrCmp::CreateAttributeModifierWithOperands()` |
| `AttrMgr.h:220-221` | `ApplyModifier(AActor*, TArray&)` | `AttrCmp::ApplyModifier()` |
| `AttrMgr.h:232-237` | `ApplyModifierWithSourceHandle(AActor*, SourceHandle, ...)` | `AttrCmp::ApplyModifierWithSourceHandle()` |
| `AttrMgr.h:240-241` | `RemoveModifier(AActor*, TArray&)` | `AttrCmp::RemoveModifier()` |
| `AttrMgr.h:250-253` | `RemoveModifiersBySourceHandle(AActor*, SourceHandle&)` | `AttrCmp::RemoveModifiersBySourceHandle()` |
| `AttrMgr.h:263-267` | `GetModifiersBySourceHandle(AActor*, SourceHandle&, TArray&)` | `AttrCmp::GetModifiersBySourceHandle()` |
| `AttrMgr.h:270-271` | `HandleModifierUpdated(AActor*, TArray&)` | `AttrCmp::HandleModifierUpdated()` |

**共 15 个 UFUNCTION 需标记**

#### 纯 C++

| 行号 | 函数 | 说明 |
|------|------|------|
| `AttrMgr.h:172` | `GetAttributeComponent(const AActor*)` | protected static |
| `AttrMgr.h:314` | `RecalculateAttributeBaseValues(...)` | protected static |
| `AttrMgr.h:317` | `RecalculateAttributeCurrentValues(...)` | protected static |
| `AttrMgr.h:320-323` | `MergeAttributeModifiers(...)` | protected static |
| `AttrMgr.h:327-333` | `ClampAttributeValueInRange(...)` | protected static |
| `AttrMgr.h:338` | `EnforceAttributeRangeConstraints(...)` | protected static |

**共 6 个纯 C++ 方法需标记**

---

### F-3. 薄转发包装器示例

#### UFUNCTION 包装器模板

```cpp
#pragma region Deprecated_MigrationOnly

UFUNCTION(BlueprintCallable, Category = "State Manager|Removal",
    meta = (DeprecatedFunction, DeprecationMessage = "Use UTcsStateComponent::RemoveState instead"))
bool RemoveState(UTcsStateInstance* StateInstance)
{
    if (!StateInstance) return false;
    UTcsStateComponent* StateCmp = StateInstance->GetOwnerStateComponent();
    if (!StateCmp) return false;
    return StateCmp->RemoveState(StateInstance);
}

#pragma endregion
```

#### 纯 C++ 包装器模板

```cpp
UE_DEPRECATED(5.6, "Use UTcsStateComponent::ActivateState instead")
void ActivateState(UTcsStateInstance* StateInstance)
{
    if (!StateInstance) return;
    if (UTcsStateComponent* StateCmp = StateInstance->GetOwnerStateComponent())
    {
        StateCmp->ActivateState(StateInstance);
    }
}
```

---

### F-4. 兼容层建议区域组织

两个 Manager 的头文件和 cpp 中使用 `#pragma region` 划分：

```cpp
#pragma region Deprecated_MigrationOnly
// ... 所有 deprecated 方法 ...
#pragma endregion
```

最终 Phase G 整段删除时一目了然。

---

## Phase G：最终硬删除

### G-1. StateManagerSubsystem 硬删除清单

#### 需删除的方法（头文件 + 实现）

**总计 41 个方法**（11 UFUNCTION + 30 纯 C++），精确行号见 F-1 节。

#### 需删除的成员变量

| 行号 | 成员 | 原因 |
|------|------|------|
| `StateMgr.h:288` | `bool bIsUpdatingSlotActivation` | 已迁至各 StateComponent 实例 |
| `StateMgr.h:292` | `TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> PendingSlotActivationUpdates` | 已简化迁至各 StateComponent |

#### 需保留的内容（**不要删除**）

| 行号 | 内容 | 原因 |
|------|------|------|
| `StateMgr.h:29` | `Initialize(...)` | 初始化 |
| `StateMgr.h:49-75` | 加载方法（`LoadFromDeveloperSettings` 等） | 定义缓存 |
| `StateMgr.h:85-119` | 定义查询（`GetStateDefinition` 等） | 全局查询 |
| `StateMgr.h:168` | `GlobalStateInstanceIdMgr` + 新增的 `AllocateStateInstanceId()` | 全局 ID |
| `StateMgr.h:186-192` | `TryApplyStateToTarget(...)` | 跨 Actor 门面（改为薄转发） |
| `StateMgr.h:38-44` | 定义缓存成员 | 全局数据 |

---

### G-2. AttributeManagerSubsystem 硬删除清单

**总计 21 个方法**（15 UFUNCTION + 6 纯 C++），精确行号见 F-2 节。

#### 需保留的内容（**不要删除**）

| 行号 | 内容 | 原因 |
|------|------|------|
| `AttrMgr.h:27` | `Initialize(...)` | 初始化 |
| `AttrMgr.h:50-55` | 加载方法 | 定义缓存 |
| `AttrMgr.h:97-112` | `TryResolveAttributeNameByTag` / `TryGetAttributeTagByName` | 定义/Tag 查询 |
| `AttrMgr.h:36-45` | 定义缓存成员 | 全局数据 |
| `AttrMgr.h:176-177, 276, 280` | 全局 ID 管理器 + 新增 `Allocate*()` | 全局 ID |
| `AttrMgr.h:296-304` | `CreateSourceHandle(...)` + `GlobalSourceHandleIdMgr` | 全局工厂 |
| 新增 | `GetAttributeDefinition()` / `GetModifierDefinition()` | 定义查询 |

---

### G-3. 删除 friend 声明

| 文件 | 行号 | 内容 | 处理 |
|------|------|------|------|
| `StateCmp.h` | 148 | `friend class UTcsStateManagerSubsystem;` | **删除** |
| `StateCmp.h` | 149 | `friend class UTcsStateInstance;` | 保留（StateInstance 访问 Component 内部成员仍有需要） |
| `AttrCmp.h` | — | 无 friend 声明 | 无需处理 |

---

### G-4. 已有 deprecated 标记检查

**搜索结果: 零匹配。** 当前代码中不存在任何 `DeprecatedFunction` 或 `UE_DEPRECATED` 标记。Phase F 是从零开始添加。

---

### G-5. 活跃文档过时表述清理

| 文件 | 行号 | 过时内容 | 处理建议 |
|------|------|---------|---------|
| `StateCmp.h` | 330 | 注释提到 `PendingRemoval` | 修改注释文字 |
| `Documents/调研：Manager API迁移到Component.md` | 9 | 提到两阶段→单阶段（历史事实） | 可保留，加注已完成 |
| `Documents/执行文档：Manager API迁移到Component（整合版）.md` | 多处 | 在"舍弃"上下文中提到 PendingRemoval | 本文为执行依据，无需修改 |

---

## 验证清单

### Phase F 验证

- [ ] 所有 deprecated 包装器仅做薄转发，无业务逻辑
- [ ] 编译器产生正确的 deprecation 警告
- [ ] 蓝图编辑器显示正确的废弃提示
- [ ] 所有 C++ 内部调用已改到 Component

### Phase G 验证

- [ ] `grep -r "Deprecated_MigrationOnly"` 返回空
- [ ] Manager 头文件中不再包含 actor-local API
- [ ] `grep -r "friend class UTcsStateManagerSubsystem"` 返回空（在 StateCmp.h 中）
- [ ] 蓝图重新编译无旧节点引用
- [ ] ��译通过: `TireflyGameplayUtilsEditor Win64 Development`
- [ ] 活跃文档无 PendingRemoval 两阶段描述
