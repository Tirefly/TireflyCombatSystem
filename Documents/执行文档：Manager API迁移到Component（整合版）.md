# TCS 重构：Manager API 迁移到 Component 最终执行文档（整合版）

## 0. 文档定位

本文是 `Manager API 迁移到 Component` 议题的**整合版执行文档**。

它有三个明确目标：

1. 以**当前源码**为准，修正旧方案里对 `StateRemoval` 的滞后理解。
2. 综合此前 Codex 与 Claude 方案的优点，给出一份**可直接执行**的迁移方案。
3. 明确写清楚：**deprecated 兼容层只允许临时存在，议题完成前必须删除所有废弃 API**，确保 `Component` 和 `Manager` 最终职责单一、边界清晰、无重复实现。

本文不替换历史文档；历史文档保留做参考与追溯。当前议题的执行应以本文为准。

---

## 1. 当前源码事实（Source Of Truth）

### 1.1 StateRemoval 的当前真实语义

当前运行时代码里的 `StateRemoval` 已经是**单阶段移除**：

- `RequestStateRemoval(UTcsStateInstance*, FName)`
- 直接进入 `FinalizeStateRemoval(UTcsStateInstance*, FName)`

当前运行时**不存在**以下机制：

- `PendingRemoval`
- `FTcsStateRemovalRequest`
- `FinalizePendingRemovalRequest`
- `RemovalFlowPolicy`
- `HardTimeout`

这意味着：

- 任何基于“两阶段握手移除”的新设计，都会与当前代码现实冲突。
- 本议题**不能**把旧的 `PendingRemoval` 机制重新带回来。

### 1.2 当前架构的真实问题

虽然 `StateRemoval` 已经简化，但 `State` / `Attribute` 的核心业务逻辑仍然集中在两个 `GameInstanceSubsystem`：

- `UTcsStateManagerSubsystem`
- `UTcsAttributeManagerSubsystem`

而两个 `Component` 目前主要还是：

- 数据容器
- 事件广播者
- 少量 Tick / Debug 辅助

这直接导致：

- 开发者即使继承 `UTcsStateComponent` / `UTcsAttributeComponent`，也很难重写真正的核心流程。
- `Manager` 依旧直接操作 `Component` 的内部状态，耦合过深。
- `StateRemoval` 虽然语义已经简化，但移除路径仍然高度依附 `StateManagerSubsystem`，扩展性不够。

---

## 2. 对旧方案的吸收与舍弃

### 2.1 吸收 Codex 方案的部分

保留以下优点：

- 允许短暂存在的 deprecated 兼容层，减少迁移期蓝图 / C++ 爆炸半径。
- 迁移期间要求“内部调用优先改走 Component”，不能只搬 API 不搬调用路径。
- 强调补测试，不只做接口重排。

### 2.2 吸收 Claude 方案的部分

保留以下优点：

- API 拆分更细，哪些是核心扩展点、哪些是查询/包装器，粒度更清晰。
- 明确把 `Allocate*Id()` 之类的全局工厂接口留在 `Manager`。
- 明确把槽位激活的防重入状态迁移到每个 `StateComponent` 实例。
- 明确提出删除 `friend class UTcsStateManagerSubsystem`。

### 2.3 本文明确舍弃的内容

以下内容**不进入**本次执行方案：

- 任何 `PendingRemoval` / `FinalizePendingRemovalRequest` 的重新引入
- 任何 `RemovalFlowPolicy` / `HardTimeout` 的重新引入
- “Manager 长期保留 actor-local 业务 API 作为并行实现”的做法
- “迁移完成后保留 deprecated 包装器不删”的做法

---

## 3. StateRemoval 相关遗留问题完整清单

| 编号 | 遗留问题 | 当前表现 | 风险 | 本次处理 |
|---|---|---|---|---|
| SR-01 | 旧文档/旧方案仍在描述两阶段移除 | `_archive` 里的历史文档和部分后续改进文档仍提到 `PendingRemoval` | 后续设计继续引用过时语义，方案漂移 | 本文明确以当前源码为准；活跃文档不再沿用旧语义 |
| SR-02 | `StateRemoval` 入口仍由 `StateManagerSubsystem` 掌控 | `RequestStateRemoval` / `FinalizeStateRemoval` 仍在 `StateManagerSubsystem` | 开发者无法通过继承 `StateComponent` 改写移除流程 | 把移除主流程下沉到 `UTcsStateComponent` |
| SR-03 | 叠层归零仍回跳到 `StateManagerSubsystem` | `UTcsStateInstance::SetStackCount()` 里仍通过 `StateMgr->RequestStateRemoval(...)` | 内部调用路径不统一，局部 override 无法生效 | 改为 `OwnerStateCmp->RequestStateRemoval(...)` |
| SR-04 | Duration 到期仍回跳到 `StateManagerSubsystem` | `UTcsStateComponent::UpdateActiveStateDurations()` 里仍通过 `StateMgr->ExpireState(...)` | 同上；Component 无法完全控制到期行为 | 改为 `ExpireState(...)` 本地调用 |
| SR-05 | 最终移除时清理 Modifier 的路径过于绕 | `FinalizeStateRemoval()` 仍先拿 `AttrMgr` 再传 `OwnerActor` 去删 Modifier | 多一次跨系统查找，且忽略了 `StateInstance` 已缓存的 OwnerAttributeComp | 优先直接用 `StateInstance->GetOwnerAttributeComponent()` |
| SR-06 | 调试注释仍带着旧 PendingRemoval 残影 | `GetStateDebugSnapshot` 注释仍提到 `PendingRemoval`，调试字符串里还有占位的 `RemovalStr` | 文档和调试输出继续误导维护者 | 清理注释与无意义字段，统一为当前单阶段语义 |
| SR-07 | 移除后查询与槽位状态仍过度依赖全表扫描 | 查询 API 主要遍历 `StateSlotsX`，没有充分使用 `StateInstanceIndex` | 移除后的查询行为分散、重复、性能差 | 查询 API 下沉到 `StateComponent` 后优先使用 `StateInstanceIndex` |
| SR-08 | 槽位激活防重入仍是全局状态 | `bIsUpdatingSlotActivation` 和 `PendingSlotActivationUpdates` 仍挂在 `StateManagerSubsystem` | 不同 Actor 的槽位更新彼此耦合 | 迁移到每个 `StateComponent` 实例 |
| SR-09 | 如果 deprecated 包装器长期保留，会形成双实现 | 迁移期容易把包装器“先留着”变成“永久留着” | `Manager` 与 `Component` 职责重新重叠 | 把“删除包装器”写成完成定义，未删除即未完成 |

---

## 4. 最终架构目标

### 4.1 最终保留在 Manager 的职责

#### `UTcsStateManagerSubsystem` 最终只保留

- 定义缓存与加载
  - `Initialize`
  - `LoadFromDeveloperSettings`
  - `LoadFromAssetManager`
  - `LoadStateOnDemand`
  - `PreloadAllStates`
  - `PreloadCommonStates`
- 定义查询
  - `GetStateDefinitionAsset`
  - `GetStateDefinitionAssetByTag`
  - `GetStateSlotDefinitionAsset`
  - `GetStateSlotDefinitionAssetByTag`
  - `GetAllStateDefNames`
- 全局状态实例 ID 分配
  - `AllocateStateInstanceId()`
- 跨 Actor 便捷门面
  - `TryApplyStateToTarget(...)`

#### `UTcsAttributeManagerSubsystem` 最终只保留

- 定义缓存与加载
  - `Initialize`
  - `LoadFromDeveloperSettings`
  - `LoadFromAssetManager`
- 定义/Tag 查询
  - `GetAttributeDefinitionAsset(FName)`
  - `GetModifierDefinitionAsset(FName)`
  - `TryResolveAttributeNameByTag(...)`
  - `TryGetAttributeTagByName(...)`
- 全局 ID 分配
  - `AllocateAttributeInstanceId()`
  - `AllocateModifierInstanceId()`
  - `AllocateModifierChangeBatchId()`
- 全局 `SourceHandle` 工厂
  - `CreateSourceHandle(...)`

### 4.2 最终下沉到 Component 的职责

#### `UTcsStateComponent` 最终负责

- 状态创建与应用
- 参数评估
- 槽位分配与槽位激活
- 生命周期阶段转换
- 状态移除与批量移除
- 状态查询
- 与 `StateTreeTickScheduler` / `DurationTracker` / `StateInstanceIndex` 的一致性维护

#### `UTcsAttributeComponent` 最终负责

- 属性增删改
- Modifier 创建、应用、移除、更新
- `SourceHandle` 到 Modifier 的本地索引维护
- 属性重算
- Clamp 与范围传播

---

## 5. 迁移原则

### 5.1 兼容层可以短暂存在，但最终必须硬删除

允许在迁移中间阶段保留 deprecated 包装器，但它只服务于：

- 编译过渡
- 蓝图节点迁移
- 分阶段提交

**注意：这不是最终形态。**

议题完成前，必须删除：

- `Manager` 上所有 actor-local 废弃 API
- 对应的实现
- 对应的 `DeprecatedFunction` 标记
- 对应的 `UE_DEPRECATED` 声明

### 5.2 不重新设计 StateRemoval 语义

本次迁移只做“归属迁移”和“耦合清理”，不重新发明移除模型。

需要保留的现有事实：

- `RequestStateRemoval` 是入口
- `FinalizeStateRemoval` 是实际收敛
- 当前移除是立即收敛，不存在 Pending 状态
- `RequestStateRemoval` 通过 `SS_Expired` 短路保证幂等

### 5.3 internal call 必须 component-first

迁移不是只把代码从 `Manager.cpp` 复制到 `Component.cpp`。

以下路径都必须同步改掉：

- `SetStackCount -> RequestStateRemoval`
- `UpdateActiveStateDurations -> ExpireState`
- `SetSlotGateOpen -> RequestUpdateStateSlotActivation`
- `OnStateTreeStateChanged -> RefreshSlotsForStateChange`
- `BeginPlay -> InitStateSlotMappings`

### 5.4 利用已有缓存，而不是重复查找

迁移后应优先利用已有结构：

- `StateInstance->GetOwnerStateComponent()`
- `StateInstance->GetOwnerAttributeComponent()`
- `StateComponent->StateInstanceIndex`

不要在关键路径里再次：

- 先找 Actor
- 再走 Interface
- 再找 Component

### 5.5 Manager 缓存不能只依赖 BeginPlay

`StateMgr` / `AttrMgr` 仍然会作为缓存指针存在于组件上，但运行时方法不能假设：

- 组件一定已经 `BeginPlay`
- 缓存一定已经填好

因此两个组件都需要新增 lazy resolve helper，例如：

```cpp
protected:
    UTcsStateManagerSubsystem* ResolveStateManager();
    UTcsAttributeManagerSubsystem* ResolveAttributeManager();
```

`BeginPlay` 负责预热，但真正的业务方法需要在缓存为空时自行补拉取。

### 5.6 只把真正的扩展点做成 virtual

本次迁移面向的是**C++ 子类扩展**。

因此：

- 核心流程点标记为 `virtual`
- 便捷包装器尽量非 `virtual`
- 清理型底层 helper 尽量保持 non-virtual / static

如果未来要支持蓝图覆写，再单独设计 `BlueprintNativeEvent`，不在本轮一起扩面。

---

## 6. 目标 API 去向

## 6.1 State 侧

### `UTcsStateComponent` 对外本地 API

```cpp
UFUNCTION(BlueprintCallable, Category = "State")
virtual bool TryApplyState(
    FName StateDefId,
    AActor* Instigator,
    int32 StateLevel = 1,
    const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

UFUNCTION(BlueprintCallable, Category = "State|Removal")
virtual bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);

UFUNCTION(BlueprintCallable, Category = "State|Removal")
virtual bool RemoveState(UTcsStateInstance* StateInstance);

UFUNCTION(BlueprintCallable, Category = "State|Removal")
virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true);

UFUNCTION(BlueprintCallable, Category = "State|Removal")
virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag);

UFUNCTION(BlueprintCallable, Category = "State|Removal")
virtual int32 RemoveAllStates();
```

### `UTcsStateComponent` 的核心 virtual 扩展点

- `CreateStateInstance(...)`
- `EvaluateAndApplyStateParameters(...)`
- `CheckStateApplyConditions(...)`
- `TryApplyStateInstance(...)`
- `TryAssignStateToStateSlot(...)`
- `ActivateState(...)`
- `DeactivateState(...)`
- `HangUpState(...)`
- `ResumeState(...)`
- `PauseState(...)`
- `FinalizeStateRemoval(...)`
- `InitStateSlotMappings()`
- `RefreshSlotsForStateChange(...)`
- `UpdateStateSlotActivation(FGameplayTag)`
- `EnforceSlotGateConsistency(FGameplayTag)`
- `ProcessStateSlotMerging(...)`
- `ProcessStateSlotByActivationMode(...)`
- `ApplyPreemptionPolicyToState(...)`

### `UTcsStateComponent` 的非 virtual 包装/辅助

- `CancelState(...)`
- `ExpireState(...)`
- `RequestUpdateStateSlotActivation(...)`
- `DrainPendingSlotActivationUpdates()`
- `RemoveStateFromSlot(...)`
- `MergeStateGroup(...)`
- `RemoveUnmergedStates(...)`
- `SortStatesByPriority(...)`
- `CleanupInvalidStates(...)`
- `IsStateStillValid(...)`

### 查询 API 的实现要求

下沉到 `UTcsStateComponent` 后，查询 API 应优先使用 `StateInstanceIndex`：

- `GetStatesInSlot` -> `StateInstanceIndex.GetInstancesBySlot(...)`
- `GetStatesByDefId` -> `StateInstanceIndex.GetInstancesByName(...)`
- `GetAllActiveStates` -> 遍历 `StateInstanceIndex.Instances`
- `HasStateWithDefId` / `HasActiveStateInSlot` -> 基于上述结果构建

注意：

- `StateSlotsX` 仍然是槽位激活顺序与 Gate 状态的真源。
- `StateInstanceIndex` 是查询缓存，不是替代槽位运行时容器。

### 一个重要修正：`ClearStateSlotExpiredStates` 不应该变成“无 this 的 static”

旧方案里曾把它设计成移除到 `StateComponent` 后仍然是 `static void ClearStateSlotExpiredStates(FTcsStateSlot*)`。

这不合理，因为当前实现还要同步清理：

- `StateTreeTickScheduler`
- `DurationTracker`
- `StateInstanceIndex`

因此本次执行里应把它改为**成员方法**：

```cpp
protected:
    void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot);
```

这样它才能正确维护组件内部缓存一致性。

## 6.2 Attribute 侧

### `UTcsAttributeComponent` 对外本地 API

```cpp
UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool AddAttribute(FName AttributeName, float InitValue = 0.f);

UFUNCTION(BlueprintCallable, Category = "Attribute")
bool AddAttributeByTag(const FGameplayTag& AttributeTag, float InitValue = 0.f);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool SetAttributeBaseValue(FName AttributeName, float NewValue, bool bTriggerEvents = true);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool SetAttributeCurrentValue(FName AttributeName, float NewValue, bool bTriggerEvents = true);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool ResetAttribute(FName AttributeName);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool RemoveAttribute(FName AttributeName);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool CreateAttributeModifier(FName ModifierId, AActor* Instigator, FTcsAttributeModifierInstance& OutModifierInst);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool CreateAttributeModifierWithOperands(
    FName ModifierId,
    AActor* Instigator,
    const TMap<FName, float>& Operands,
    FTcsAttributeModifierInstance& OutModifierInst);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual void ApplyModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual void HandleModifierUpdated(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

UFUNCTION(BlueprintCallable, Category = "Attribute")
virtual bool RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);
```

### `UTcsAttributeComponent` 的核心 virtual 扩展点

- `AddAttribute(...)`
- `SetAttributeBaseValue(...)`
- `SetAttributeCurrentValue(...)`
- `ResetAttribute(...)`
- `RemoveAttribute(...)`
- `CreateAttributeModifier(...)`
- `CreateAttributeModifierWithOperands(...)`
- `ApplyModifier(...)`
- `RemoveModifier(...)`
- `HandleModifierUpdated(...)`
- `RemoveModifiersBySourceHandle(...)`
- `RecalculateAttributeBaseValues(...)`
- `RecalculateAttributeCurrentValues(...)`
- `MergeAttributeModifiers(...)`
- `ClampAttributeValueInRange(...)`
- `EnforceAttributeRangeConstraints()`

### `UTcsAttributeComponent` 的非 virtual 包装/辅助

- `AddAttributes(...)`
- `AddAttributeByTag(...)`
- `ApplyModifierWithSourceHandle(...)`
- `GetModifiersBySourceHandle(...)`

---

## 7. 详细执行阶段

## Phase A：先对齐 StateRemoval 的真实语义与遗留残影

### A-1. 明确活跃文档的基线

活跃文档全部以当前源码为准：

- 当前移除是单阶段
- `RemovalReason` 使用 `FName`
- 不存在 PendingRemoval

`_archive` 中的文档保留历史价值，但不作为当前实施依据。

### A-2. 统一移除原因命名

当前移除原因分散在多个 cpp 的字符串字面量里，例如：

- `"Removed"`
- `"Cancelled"`
- `"Expired"`
- `"MergedOut"`
- `"StackDepleted"`

迁移过程中建议统一成一个常量命名区，例如放在 `TcsState.h`：

```cpp
namespace TcsStateRemovalReasons
{
    static const FName Removed(TEXT("Removed"));
    static const FName Cancelled(TEXT("Cancelled"));
    static const FName Expired(TEXT("Expired"));
    static const FName MergedOut(TEXT("MergedOut"));
    static const FName StackDepleted(TEXT("StackDepleted"));
}
```

后续所有调用统一替换成常量，避免迁移期间拼写漂移。

### A-3. 清理遗留注释和调试残影

需要同步清理：

- `GetStateDebugSnapshot` 注释里对 `PendingRemoval` 的描述
- `GetSlotDebugSnapshot` / `GetStateDebugSnapshot` 中无实际意义的 `RemovalStr` 占位逻辑
- 任何仍在活跃文档里把当前移除描述为“两阶段”的表述

---

## Phase B：准备公共基础设施

### B-1. StateManagerSubsystem 暴露全局 ID 工厂

文件：

- `Public/State/TcsStateManagerSubsystem.h`

新增：

```cpp
public:
    int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }
```

### B-2. AttributeManagerSubsystem 暴露定义查询与全局 ID 工厂

文件：

- `Public/Attribute/TcsAttributeManagerSubsystem.h`
- `Private/Attribute/TcsAttributeManagerSubsystem.cpp`

新增 public 方法：

- `GetAttributeDefinitionAsset(FName) const`
- `GetModifierDefinitionAsset(FName) const`
- `AllocateAttributeInstanceId()`
- `AllocateModifierInstanceId()`
- `AllocateModifierChangeBatchId()`

### B-3. 两个 Component 增加 lazy resolve helper

文件：

- `Public/State/TcsStateComponent.h`
- `Private/State/TcsStateComponent.cpp`
- `Public/Attribute/TcsAttributeComponent.h`
- `Private/Attribute/TcsAttributeComponent.cpp`

新增成员：

- `TObjectPtr<UTcsStateManagerSubsystem> StateMgr`
- `TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr`

新增 helper：

```cpp
protected:
    UTcsStateManagerSubsystem* ResolveStateManager();
    UTcsAttributeManagerSubsystem* ResolveAttributeManager();
```

要求：

- `BeginPlay` 里继续预热缓存
- 业务方法里发现缓存为空时，仍能自行拉起

---

## Phase C：先迁 Attribute，再迁 State

## C-1. 迁移 Attribute 业务到 `UTcsAttributeComponent`

### 目标

把下面这几类能力全部迁到 `UTcsAttributeComponent`：

- 属性增删改
- Modifier 创建/应用/移除/更新
- 重算与 Clamp
- SourceHandle 本地索引维护

### 文件级执行细节

#### `Public/Attribute/TcsAttributeComponent.h`

新增：

- `AttrMgr` 缓存
- `ResolveAttributeManager()`
- 新的 public 本地 API
- protected virtual 计算核心

注意：

- `TcsSourceHandle.h` 需要纳入 include
- 现有事件与容器成员继续保留，不拆

#### `Private/Attribute/TcsAttributeComponent.cpp`

从 `TcsAttributeManagerSubsystem.cpp` 迁入实现时遵循以下替换规则：

- `CombatEntity` -> `GetOwner()`
- `GetAttributeComponent(CombatEntity)` -> `this`
- `++GlobalAttributeInstanceIdMgr` -> `ResolveAttributeManager()->AllocateAttributeInstanceId()`
- `++GlobalAttributeModifierInstanceIdMgr` -> `ResolveAttributeManager()->AllocateModifierInstanceId()`
- `++GlobalAttributeModifierChangeBatchIdMgr` -> `ResolveAttributeManager()->AllocateModifierChangeBatchId()`
- `AttributeDefinitions.Find(...)` -> `ResolveAttributeManager()->GetAttributeDefinitionAsset(...)`
- `AttributeModifierDefinitions.Find(...)` -> `ResolveAttributeManager()->GetModifierDefinitionAsset(...)`

必须保留的行为顺序：

- Modifier 应用前后 `BatchId` / `ApplyTimestamp` / `UpdateTimestamp` 的写入顺序
- `SourceHandleIdToModifierInstIds` 与 `ModifierInstIdToIndex` 的维护逻辑
- 事件广播顺序
- `EnforceAttributeRangeConstraints()` 的最终收敛调用

#### `Public/Attribute/TcsAttributeManagerSubsystem.h/.cpp`

迁移期允许暂留旧 API，但它们必须变成**薄转发包装器**：

- 不再包含业务逻辑
- 只负责找目标 `AttributeComponent` 并转发
- 全部打上 deprecation 标记

例如：

```cpp
UFUNCTION(BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use AttributeComponent::AddAttribute"))
bool AddAttribute(AActor* CombatEntity, FName AttributeName, float InitValue = 0.f);
```

最终阶段删除：

- `AddAttribute`
- `AddAttributes`
- `AddAttributeByTag`
- `SetAttributeBaseValue`
- `SetAttributeCurrentValue`
- `ResetAttribute`
- `RemoveAttribute`
- `CreateAttributeModifier`
- `CreateAttributeModifierWithOperands`
- `ApplyModifier`
- `ApplyModifierWithSourceHandle`
- `RemoveModifier`
- `RemoveModifiersBySourceHandle`
- `GetModifiersBySourceHandle`
- `HandleModifierUpdated`
- `GetAttributeComponent`

其中 `GetAttributeComponent(const AActor*)` 的 fallback `FindComponentByClass` 逻辑只允许在过渡包装器里短暂存在，最终必须删掉，避免和 `ITcsEntityInterface` 路线继续分叉。

---

## Phase D：优先迁移 StateRemoval 与生命周期

这是本议题最重要的一步，因为它直接决定 `StateRemoval` 是否真正变成可继承/可改写的本地流程。

### D-1. `UTcsStateComponent` 新增移除与生命周期 API

文件：

- `Public/State/TcsStateComponent.h`
- `Private/State/TcsStateComponent.cpp`

迁入：

- `ActivateState`
- `DeactivateState`
- `HangUpState`
- `ResumeState`
- `PauseState`
- `CancelState`
- `ExpireState`
- `RequestStateRemoval`
- `FinalizeStateRemoval`
- `RemoveState`
- `RemoveStatesByDefId`
- `RemoveAllStatesInSlot`
- `RemoveAllStates`
- `IsStateStillValid`

### D-2. `FinalizeStateRemoval` 必须保留的执行顺序

迁入 `UTcsStateComponent` 后，顺序必须与当前语义保持一致：

1. 校验 `StateInstance` 与 `StateDef`
2. 如果 `StateTree` 正在运行，先 `StopStateTree()`
3. 尝试把阶段设置为 `SS_Expired`，若失败则直接返回，保证幂等
4. 从本地运行时缓存中移除
   - `StateTreeTickScheduler`
   - `DurationTracker`
   - `StateInstanceIndex`
5. 如果 `SourceHandle` 有效，清理由该状态创建的 Modifier
6. 广播
   - `NotifyStateStageChanged`
   - `NotifyStateRemoved`
7. 从槽位容器里移除状态，并请求槽位刷新
8. `MarkPendingGC()`

### D-3. Modifier 清理路径的修正

这里不再使用：

- `AttrMgr->RemoveModifiersBySourceHandle(OwnerActor, SourceHandle)`

改为优先直接使用：

```cpp
if (UTcsAttributeComponent* OwnerAttrComp = StateInstance->GetOwnerAttributeComponent())
{
    OwnerAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
}
```

如果担心极端情况下弱引用已失效，可以做一次 fallback，但**主路径必须是 OwnerAttributeComponent 直达**。

### D-4. 同步改掉当前两个内部调用点

#### `Private/State/TcsState.cpp`

`UTcsStateInstance::SetStackCount()` 中：

- 原来：`StateMgr->RequestStateRemoval(this, ...)`
- 改为：`OwnerStateCmp->RequestStateRemoval(this, ...)`

#### `Private/State/TcsStateComponent.cpp`

`UpdateActiveStateDurations()` 中：

- 原来：`StateMgr->ExpireState(ExpiredState)`
- 改为：`ExpireState(ExpiredState)`

---

## Phase E：迁移状态应用、槽位链路与查询

## E-1. 迁移状态创建与应用主流程

迁入 `UTcsStateComponent`：

- `TryApplyState(...)`
- `CreateStateInstance(...)`
- `EvaluateAndApplyStateParameters(...)`
- `TryApplyStateInstance(...)`
- `CheckStateApplyConditions(...)`

### 关键实现要求

#### `CreateStateInstance(...)`

- `Owner` 不再作为参数传入，直接使用 `GetOwner()`
- `StateDefAsset` 通过 `ResolveStateManager()->GetStateDefinitionAsset(...)` 获取
- `StateInstanceId` 通过 `ResolveStateManager()->AllocateStateInstanceId()` 获取
- `SourceHandle` 仍然通过 `UTcsAttributeManagerSubsystem::CreateSourceHandle(...)` 创建

#### `TryApplyStateInstance(...)`

必须保留现有顺序：

1. 检查初始化
2. 检查应用条件
3. 分配到槽位
4. 槽位激活更新后，若状态仍有效，再写入 `StateInstanceIndex`
5. 最后广播 `NotifyStateApplySuccess`

这里尤其要保留当前行为：

- `StateInstanceIndex.AddInstance(...)` 是在槽位处理之后才做
- 如果状态在合并过程中已经被淘汰，则不应被写入索引

### `TryApplyStateToTarget(...)` 的最终形态

`UTcsStateManagerSubsystem::TryApplyStateToTarget(...)` 最终只做门面：

1. 校验 Target
2. 解析 `StateComponent`
3. 调用 `StateComp->TryApplyState(...)`

它不再保留 actor-local 的应用逻辑。

## E-2. 迁移槽位链路与防重入

迁入 `UTcsStateComponent`：

- `InitStateSlotMappings()`
- `TryAssignStateToStateSlot(...)`
- `RefreshSlotsForStateChange(...)`
- `RequestUpdateStateSlotActivation(...)`
- `DrainPendingSlotActivationUpdates()`
- `UpdateStateSlotActivation(...)`
- `EnforceSlotGateConsistency(...)`
- `ProcessStateSlotMerging(...)`
- `MergeStateGroup(...)`
- `RemoveUnmergedStates(...)`
- `ProcessStateSlotByActivationMode(...)`
- `ProcessPriorityOnlyMode(...)`
- `ProcessAllActiveMode(...)`
- `ApplyPreemptionPolicyToState(...)`
- `CleanupInvalidStates(...)`
- `RemoveStateFromSlot(...)`

新增成员：

```cpp
protected:
    bool bIsUpdatingSlotActivation = false;
    TSet<FGameplayTag> PendingSlotActivationUpdates;
```

要求：

- 防重入状态必须按 `StateComponent` 实例隔离
- `MaxIterations` 保护仍然保留
- `InitStateSlotMappings()` 仍然保持“先创建所有槽位，再处理可选的 StateTree 映射”的两步语义

## E-3. 同步改掉 `StateComponent` 内部调用点

需要更新：

- `BeginPlay()`：改为直接调用 `InitStateSlotMappings()`
- `SetSlotGateOpen()`：改为直接调用 `RequestUpdateStateSlotActivation(SlotTag)`
- `OnStateTreeStateChanged()`：改为直接调用 `RefreshSlotsForStateChange(...)`

## E-4. 查询 API 下沉并改用 `StateInstanceIndex`

迁入 `UTcsStateComponent`：

- `GetStatesInSlot`
- `GetStatesByDefId`
- `GetAllActiveStates`
- `HasStateWithDefId`
- `HasActiveStateInSlot`

实现要求：

- 优先使用 `StateInstanceIndex`
- 不要再重复遍历 `StateSlotsX` 做同样的查询
- 不要在每个查询里偷偷调用 `RefreshInstances()` 修改运行时状态

---

## Phase F：临时 deprecated 兼容层

### F-1. 允许存在，但必须是薄转发

迁移期可在两个 `Manager` 中保留旧 API 包装器，但它们必须满足：

- 只做参数校验、目标组件解析、转发
- 不再保留原始业务逻辑
- 不能出现 manager 与 component 两边各自维护一套状态变化

### F-2. 标记方式

#### Blueprint 可见的 UFUNCTION

使用：

- `meta = (DeprecatedFunction, DeprecationMessage = "...")`

#### 仅 C++ 调用的旧接口

使用：

- `UE_DEPRECATED(...)`

### F-3. 包装器建议集中放在单独区域

建议在两个 `Manager` 头文件/实现文件里单独划区：

```cpp
#pragma region Deprecated_MigrationOnly
...
#pragma endregion
```

这样最终删除时可以整段移除，避免残留。

### F-4. 兼容层存在的时间窗口

兼容层只允许出现在：

- 组件 API 已经落地
- 内部调用点正在批量替换
- 蓝图节点正在迁移

一旦以下条件全部满足，就必须进入下一阶段删除：

- 所有 C++ 内部调用已改到 Component
- 所有蓝图节点已完成迁移并可编译
- 回归测试通过

---

## Phase G：最终硬删除（议题完成前必须执行）

### G-1. 从 `UTcsStateManagerSubsystem` 删除全部 actor-local 废弃 API

最终只保留第 4 章列出的全局职责。

必须删除：

- `TryApplyStateInstance`
- `CreateStateInstance`
- `EvaluateAndApplyStateParameters`
- `CheckStateApplyConditions`
- `InitStateSlotMappings`
- `TryAssignStateToStateSlot`
- `RequestUpdateStateSlotActivation`
- `DrainPendingSlotActivationUpdates`
- `UpdateStateSlotActivation`
- `EnforceSlotGateConsistency`
- `RefreshSlotsForStateChange`
- 所有生命周期方法
- 所有查询方法
- 所有移除方法

### G-2. 从 `UTcsAttributeManagerSubsystem` 删除全部 actor-local 废弃 API

最终只保留第 4 章列出的全局职责。

### G-3. 删除 `friend class UTcsStateManagerSubsystem`

文件：

- `Public/State/TcsStateComponent.h`

最终应只保留真正需要的友元关系；`StateManagerSubsystem` 不再直接伸手访问 `StateComponent` 私有成员。

### G-4. 删除所有废弃标记与包装实现

这一步必须做完整：

- 删除头文件声明
- 删除 cpp 实现
- 删除 `DeprecatedFunction` metadata
- 删除 `UE_DEPRECATED`
- 删除只为旧包装器服务的 include / helper

### G-5. 删除活跃文档中的过时表述

除本文外，非 `_archive` 的活跃文档若仍在描述 `PendingRemoval` / 两阶段移除，应同步修正或移入 archive。

---

## 8. 涉及文件清单

### 核心代码文件

- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateComponent.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateComponent.cpp`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsState.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/State/TcsState.cpp`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeManagerSubsystem.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeManagerSubsystem.cpp`

### 文档文件

- 本文
- 其他非 `_archive` 的活跃文档（如仍描述两阶段移除，需要同步修正）

---

## 9. 验证清单

### 9.1 编译验证

至少执行：

- `TireflyGameplayUtilsEditor Win64 Development` 编译

### 9.2 行为验证

至少覆盖以下场景：

- 状态直接移除
- `StackDepleted` 触发移除
- Duration 到期触发 `ExpireState`
- 合并淘汰 `MergedOut`
- Gate 关闭时的 `Cancel` 路径
- `RemoveState` / `RemoveStatesByDefId` / `RemoveAllStatesInSlot` / `RemoveAllStates`
- `FinalizeStateRemoval` 触发 `SourceHandle` 对应 Modifier 清理
- 槽位激活防重入在多个 Actor 间互不干扰
- 子类重写 `StateComponent` / `AttributeComponent` 核心 virtual 后确实生效

### 9.3 查询一致性验证

确认以下查询在状态新增、合并、移除后仍保持正确：

- `GetStatesInSlot`
- `GetStatesByDefId`
- `GetAllActiveStates`
- `HasStateWithDefId`
- `HasActiveStateInSlot`

### 9.4 废弃 API 删除验证

最终阶段必须确认：

- grep 不再出现旧的内部调用路径
- `Manager` 头文件中不再存在 `Deprecated_MigrationOnly` 区域
- 蓝图重新编译无旧节点引用

---

## 10. 完成定义（Done Definition）

只有在以下条件全部满足时，本议题才算完成：

1. `StateRemoval`、生命周期、槽位、查询、属性与 Modifier 逻辑都已经下沉到对应 `Component`。
2. `StateManagerSubsystem` 与 `AttributeManagerSubsystem` 只保留全局定义/工厂/门面职责。
3. 迁移期 deprecated 包装器已全部删除。
4. `friend class UTcsStateManagerSubsystem` 已删除。
5. 活跃文档不再把当前系统描述成 `PendingRemoval` 两阶段模型。
6. 编译通过，行为验证通过，关键测试场景通过。

**只要 deprecated 包装器还在，或者 Manager 上还残留 actor-local 业务 API，本议题就不能算完成。**
