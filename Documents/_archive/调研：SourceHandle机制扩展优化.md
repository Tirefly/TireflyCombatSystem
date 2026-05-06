# TCS SourceHandle 机制扩展优化调研

> **状态**: P0 实施完成
> **创建日期**: 2026-03-06
> **最后更新**: 2026-03-12

---

## 一、SourceHandle 现有设计概述

### 1.1 当前结构定义

```cpp
// TcsSourceHandle.h（当前版本）
USTRUCT(BlueprintType)
struct FTcsSourceHandle
{
    int32 Id = -1;                              // 全局唯一来源 ID（单调递增）
    FDataTableRowHandle SourceDefinition;        // Source 定义的 DataTable 引用
    FName SourceName = NAME_None;                // Source 名称（冗余，快速访问/调试）
    FGameplayTagContainer SourceTags;            // Source 类型标签（分类/过滤）
    TWeakObjectPtr<AActor> Instigator;           // 施加者（弱指针）
};
```

#### 1.1.1 改造后目标结构

```cpp
// TcsSourceHandle.h（改造后）
USTRUCT(BlueprintType)
struct FTcsSourceHandle
{
    // 全局唯一来源 ID（单调递增）
    UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
    int32 Id = -1;

    // 来源标签（可选，用于分类/过滤）
    UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
    FGameplayTagContainer SourceTags;

    // 施加者（弱指针）
    UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
    TWeakObjectPtr<AActor> Instigator;

    // 因果链：从根源到直接父级的 PrimaryAssetId 有序链（不包含自身）
    // 空数组表示自身就是根源
    UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
    TArray<FPrimaryAssetId> CausalityChain;
};
```

**删除字段说明**：
- `SourceDefinition (FDataTableRowHandle)` → 删除。各实例（Modifier、State、Skill）自身已持有定义引用（如 `FTcsAttributeModifierInstance::ModifierDef`、`UTcsStateInstance::StateDef`），SourceHandle 不应重复承担"标识来源定义"的职责
- `SourceName (FName)` → 删除。可从各实例自身的定义中获取，属于冗余字段

**新增字段说明**：
- `CausalityChain (TArray<FPrimaryAssetId>)` → 新增。使用 `FPrimaryAssetId` 存储因果链，天然兼容所有 `UPrimaryDataAsset` 子类（`UTcsStateDefinition`、`UTcsAttributeModifierDefinition`、未来的 SkillDefinitionAsset）

### 1.2 现有设计优点

- **Source vs Instigator 分离**：效果定义（技能 Definition）和施加者（角色 Actor）明确区分
- **全局唯一 ID**：int32 单调递增，支持 O(1) 哈希查询
- **网络同步支持**：自定义 `NetSerialize` 已实现，条件复制（Instigator 有效时才序列化）
- **O(1) 查询**：`SourceHandleIdToModifierInstIds` 索引映射，快速定位某个 SourceHandle 关联的所有 Modifier
- **事件归因**：`FTcsAttributeChangeEventPayload.ChangeSourceRecord` 记录每个 SourceHandle 的贡献值
- **蓝图完整支持**：所有字段 `BlueprintReadOnly`，所有 API 都有 `UFUNCTION`

### 1.3 现有 API

```cpp
// UTcsAttributeManagerSubsystem 中的 SourceHandle API（当前版本）
FTcsSourceHandle CreateSourceHandle(SourceDefinition, SourceName, SourceTags, Instigator);
FTcsSourceHandle CreateSourceHandleSimple(SourceName, Instigator);
bool ApplyModifierWithSourceHandle(CombatEntity, SourceHandle, ModifierIds, OutModifiers);
bool RemoveModifiersBySourceHandle(CombatEntity, SourceHandle);
bool GetModifiersBySourceHandle(CombatEntity, SourceHandle, OutModifiers);
```

#### 1.3.1 改造后目标 API

```cpp
// CreateSourceHandle 合并为单一入口，移除 SourceDefinition 和 SourceName 参数
FTcsSourceHandle CreateSourceHandle(
    const TArray<FPrimaryAssetId>& CausalityChain,  // 必须：因果链（根源传空数组）
    AActor* Instigator,                              // 必须：施加者
    const FGameplayTagContainer& SourceTags = FGameplayTagContainer());  // 可选：来源标签

// 以下 API 保持不变
bool ApplyModifierWithSourceHandle(CombatEntity, SourceHandle, ModifierIds, OutModifiers);
bool RemoveModifiersBySourceHandle(CombatEntity, SourceHandle);
bool GetModifiersBySourceHandle(CombatEntity, SourceHandle, OutModifiers);
```

---

## 二、已确认的扩展项

### 2.1 StateInstance 携带 SourceHandle【P0 - 必须】

#### 问题描述

当前 StateInstance 的 StateTree 内部创建 AttributeModifier 时，需要手动构造 SourceHandle。缺少从 StateInstance 到 Modifier 的自动关联机制，导致：

1. StateTree 内创建 Modifier 需要额外的样板代码来构造 SourceHandle
2. StateInstance 移除时，无法自动清理其创建的 Modifier
3. 无法回答"这个 Modifier 来自哪个 State"这个核心问题

#### 设计方案

在 `UTcsStateInstance` 中新增 `SourceHandle` 字段：

```cpp
class UTcsStateInstance : public UObject
{
    // 新增：此状态实例的来源句柄
    UPROPERTY(BlueprintReadOnly, Category = "Meta")
    FTcsSourceHandle SourceHandle;
};
```

**初始化时机**：在 `UTcsStateManagerSubsystem::CreateStateInstance()` 中，`Initialize()` 之后、返回之前：

```
CreateStateInstance() 流程：
  1. 参数校验（Owner、Instigator、Interface 检查）
  2. 获取 StateDefinitionAsset
  3. NewObject<UTcsStateInstance>()
  4. Initialize()（设置 DefAsset、Owner、Instigator、Level 等）
  5. EvaluateAndApplyStateParameters()
  6. SetApplyTimestamp()
→ 7. 【新增】根据 ParentSourceHandle 构建 CausalityChain，创建 SourceHandle 并赋值
  8. return StateInstance
```

**Step 7 内部逻辑**：

```cpp
// 从 ParentSourceHandle 构建因果链
TArray<FPrimaryAssetId> NewChain = ParentSourceHandle.CausalityChain;
if (ParentSourceHandle.IsValid())
{
    // 获取父级 State 的 PrimaryAssetId 并 append 到链尾
    // ParentSourceHandle 来自施加者 StateInstance，其 DefAsset 的 PrimaryAssetId 即为父级定义
    NewChain.Add(ParentStateDef->GetPrimaryAssetId());
}
StateInstance->SourceHandle = CreateSourceHandle(NewChain, Instigator);
```

**TryApplyStateToTarget 签名扩展**：

当前签名缺少因果链传递参数，需要扩展：

```cpp
// 当前签名
bool TryApplyStateToTarget(AActor* Target, FName StateDefId, AActor* Instigator, int32 StateLevel = 1);

// 扩展后签名：新增可选的 ParentSourceHandle 参数
bool TryApplyStateToTarget(
    AActor* Target,
    FName StateDefId,
    AActor* Instigator,
    int32 StateLevel = 1,
    const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());
```

- 根源场景（玩家直接释放技能）：不传 ParentSourceHandle，默认空 SourceHandle，CausalityChain 为空
- 派生场景（StateTree 中施加子 State）：传入 `StateInstance->SourceHandle`，内部自动构建因果链

**使用场景**：StateTree Task 中创建 Modifier 时直接使用 `StateInstance->SourceHandle`，无需手动构造。StateTree Task 中施加子 State 时传入 `StateInstance->SourceHandle` 作为 ParentSourceHandle。

---

### 2.2 SourceHandle 因果链（Causality Chain）【P0 - 必须】

#### 问题描述

当前 SourceHandle 只能表示"谁造成了这个效果"，无法表示"这个效果的完整因果链"。例如：

- 玩家A 使用 火球术 → 命中目标 → 施加 燃烧Buff → 燃烧Buff 造成持续伤害
- 当前只能追踪到最后一环（燃烧Buff 的 SourceHandle），无法回溯到"起因是火球术"

#### 设计方案

在 `FTcsSourceHandle` 中新增 `TArray<FPrimaryAssetId> CausalityChain`，存储从根源到直接父级的完整定义有序链（不包含自身）：

```cpp
struct FTcsSourceHandle
{
    // ... 现有字段（Id, SourceTags, Instigator） ...

    // 因果链：从根源到直接父级的 PrimaryAssetId 有序链（不包含自身）
    // 空数组表示自身就是根源
    UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
    TArray<FPrimaryAssetId> CausalityChain;
};
```

#### 设计决策过程

**否决方案1：`int32 ParentSourceHandleId` + 全局注册表**

最初考虑使用 `ParentSourceHandleId` 指向父级 SourceHandle，通过全局注册表回溯完整链路。否决理由：
- 父级 SourceHandle 的生命周期可能先于子级结束（如 Buff 过期后，其创建的 Modifier 仍然存在于其他 State 中）
- 已确认 State 之间不应根据 SourceHandle 互相影响生命周期，因此"查无此人"是必然发生的
- 全局注册表引入了额外的内存开销和生命周期管理复杂度

**否决方案2：`RootSourceDefinition` + `ParentSourceDefinition` 分拆字段**

考虑只记录根源和直接父级两个引用。否决理由：
- 丢失了中间层级信息（三层以上链路时无法还原完整路径）
- 不如直接存完整链灵活

**否决方案3：`TSet<FPrimaryAssetId>` 替代 `TArray`**

考虑用 TSet 去重。否决理由：
- TSet 没有顺序，因果链本质上是有序的（A → B → C），丢失顺序后无法查根源和直接父级
- 实际场景可能存在合理的重复层级（如：技能A → BuffB → 技能A 再次触发）

**否决方案4：`TArray<FDataTableRowHandle>` 存储因果链**

早期方案使用 `FDataTableRowHandle`。否决理由：
- SourceHandle 中的 `SourceDefinition (FDataTableRowHandle)` 字段已被删除（各实例自身已持有定义引用，SourceHandle 不应重复承担此职责）
- `UTcsStateDefinition` 不是 DataTable 行，无法用 `FDataTableRowHandle` 表示
- `FPrimaryAssetId` 天然兼容所有 `UPrimaryDataAsset` 子类，是更通用的选择

**最终方案：`TArray<FPrimaryAssetId> CausalityChain`**

选择理由：
- `FPrimaryAssetId` 是静态资产标识（两个 FName），不会在运行时被销毁，不存在悬空引用问题
- 有序数组保留了完整的因果路径信息
- 不需要全局注册表，无额外查询开销
- 子级 SourceHandle 创建时，从父级拷贝链并 append 父级的 PrimaryAssetId，拿来即用
- SourceHandle 不需要承载"具体某次释放"的数据职能（`SourceHandle.Id` 已经是每次创建唯一的），因果链只需回答"这条因果链经过了哪些定义"

#### 因果链构建场景

```
1. 玩家释放火球术，创建 StateInstance_火球
   SourceHandle_火球:
     CausalityChain = []                        ← 根源，空链

2. 火球命中，施加燃烧Buff，创建 StateInstance_燃烧
   SourceHandle_燃烧:
     CausalityChain = [Fireball_PrimaryAssetId] ← 父级链 [] + 父级的 PrimaryAssetId

3. 燃烧Buff 的 StateTree 创建持续伤害 Modifier
   Modifier 直接使用 StateInstance_燃烧 的 SourceHandle
     CausalityChain = [Fireball_PrimaryAssetId]

4. 假设燃烧Buff 又触发了爆炸Buff
   SourceHandle_爆炸:
     CausalityChain = [Fireball_PrimaryAssetId, Burning_PrimaryAssetId]
```

#### 查询方式

```cpp
// 查根源
FPrimaryAssetId GetRootSource(const FTcsSourceHandle& Handle)
{
    return Handle.CausalityChain.IsEmpty()
        ? FPrimaryAssetId()                  // 自身就是根源，需从实例的 DefAsset 获取
        : Handle.CausalityChain[0];
}

// 查直接父级
FPrimaryAssetId GetParentSource(const FTcsSourceHandle& Handle)
{
    return Handle.CausalityChain.IsEmpty()
        ? FPrimaryAssetId()                  // 无父级
        : Handle.CausalityChain.Last();
}
```

**网络同步**：`TArray<FPrimaryAssetId>` 支持序列化。`FPrimaryAssetId` 本质是两个 FName，实际因果链深度通常不超过 3-4 层，序列化开销可忽略。

---

### 2.3 FinalizeStateRemoval 中清理关联 Modifier【P0 - 必须】

#### 问题描述

当前 `FinalizeStateRemoval` 流程中**完全没有处理与 StateInstance 相关的 AttributeModifier 清理**。

如果 StateTree 没有正确清理自己创建的 Modifier（或者状态根本没有 StateTree），Modifier 会成为孤儿，永远残留在 AttributeComponent 上。

#### 当前 FinalizeStateRemoval 完整流程

```cpp
FinalizeStateRemoval(StateInstance, RemovalReason)
{
    // Step 1: 停止 StateTree
    if (StateInstance->IsStateTreeRunning())
        StateInstance->StopStateTree();

    // Step 2: 设置 Stage = Expired（终态不可逆）
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Expired);

    // Step 3: 清理容器
    StateComponent->StateTreeTickScheduler.Remove(StateInstance);
    StateComponent->DurationTracker.Remove(StateInstance);
    StateComponent->StateInstanceIndex.RemoveInstance(StateInstance);

    // Step 4: 广播事件
    StateComponent->NotifyStateStageChanged(→ SS_Expired);
    StateComponent->NotifyStateRemoved(RemovalReason);

    // Step 5: 从槽位移除
    RemoveStateFromSlot();
    RequestUpdateStateSlotActivation();

    // Step 6: 标记待 GC
    StateInstance->MarkPendingGC();
}
```

#### 设计决策

**所有 AttributeModifier（以及未来的 SkillModifier）的生命周期都应完整跟随 StateInstance。** 状态移除时无条件清理所有通过该 StateInstance 的 SourceHandle 创建的 Modifier。

理由：
- "永久效果"不应该用有生命周期的 State 来承载，而应该直接使用无 State 的 Modifier
- 简化心智模型：State 移除 = 该 State 的所有副作用一并移除
- 避免孤儿 Modifier 残留造成的数据不一致

#### 插入位置

在 Step 3（清理容器）之后、Step 4（广播事件）之前插入 Modifier 清理：

```cpp
FinalizeStateRemoval(StateInstance, RemovalReason)
{
    // Step 1-3: ...（不变）

    // Step 3.5【新增】: 清理通过 SourceHandle 创建的所有 Modifier
    if (StateInstance->SourceHandle.IsValid())
    {
        if (auto* AttrMgr = GetGameInstance()->GetSubsystem<UTcsAttributeManagerSubsystem>())
        {
            // 只清理 Owner 上的 Modifier
            AttrMgr->RemoveModifiersBySourceHandle(
                StateInstance->GetOwner(),
                StateInstance->SourceHandle);
        }
    }

    // Step 4-6: ...（不变）
}
```

#### 跨 Actor Modifier 清理策略（已确认）

**结论：FinalizeStateRemoval 只需要清理 Owner 上的 Modifier，不需要处理其他 Actor。**

穷举分析如下：

| # | 场景 | Target Actor | 是否需要在 FinalizeStateRemoval 中清理 | 理由 |
|---|------|-------------|--------------------------------------|------|
| 1 | 修改自身属性 | Owner | 是 ✅ | 最常见场景，SourceHandle 兜底清理 |
| 2 | 修改施加者属性 | Instigator | 否 ❌ | 应通过在 Instigator 上施加独立 State 实现 |
| 3 | 修改队友属性 | 其他 Actor | 否 ❌ | 应通过在目标上施加独立 State 实现 |
| 4 | 修改敌人属性 | 其他 Actor | 否 ❌ | 应通过在目标上施加独立 State 实现 |
| 5 | 修改特定交互对象 | 其他 Actor | 否 ❌ | 应通过在目标上施加独立 State 实现 |

**设计规范**：StateTree 中不应跨 Actor 直接 ApplyModifier。跨 Actor 的属性修改必须通过在目标上施加独立 State 来实现，由目标 State 的 SourceHandle 管理其 Modifier 生命周期。

这与 TCS "一切皆状态" 的核心设计理念一致——无论是光环给队友加攻击力、还是吸血 Buff 给攻击者回血，都应该在目标 Actor 上创建一个独立的 StateInstance（带有自己的 SourceHandle），而不是从源 StateTree 中直接操作目标的 AttributeComponent。

---

### 2.4 SourceHandle 查询 API 增强【P1 - 重要】

#### 问题描述

当前 `UTcsAttributeManagerSubsystem` 中的查询 API 只支持按单个 SourceHandle 查询。缺少按 Tag、按 Instigator 批量查询的能力。

#### 拟新增 API

```cpp
// 按 Tag 查询所有活跃的 SourceHandle 关联的 Modifier
TArray<FTcsSourceHandle> GetActiveSourceHandlesByTags(
    AActor* CombatEntity,
    const FGameplayTagContainer& RequiredTags,
    const FGameplayTagContainer& IgnoredTags) const;

// 按 Instigator 查询
TArray<FTcsSourceHandle> GetActiveSourceHandlesByInstigator(
    AActor* CombatEntity,
    AActor* Instigator) const;
```

**使用场景**：
- 移除所有来自特定玩家的 Buff 效果
- 检查角色身上是否有控制类效果（通过 Tag 查询）
- 统计某个施加者造成的所有效果

---

## 三、已否决的扩展项

### 3.1 增加 UObject 类型 SourceObject 引用 — 否决

**提案**：增加 `TWeakObjectPtr<UObject> SourceObject` 用于追踪效果载体对象（投射物、陷阱等）。

**否决理由**：
- 投射物 Actor 在命中后通常被回收到对象池或直接销毁，弱指针会失效，记录无意义
- 投射物本质上由 Instigator 发射，Instigator 已经是完整的施加者信息
- `FDataTableRowHandle SourceDefinition` 已经存储了 Source 的核心配置信息（技能/装备定义）
- 将 StateInstance 作为 SourceObject 同样没有必要，因为 StateInstance 的 SourceHandle 通过 ID 和 SourceDefinition 已经能够完整标识来源

### 3.2 SourceHandle 元数据扩展 — 否决

**提案**：
- 增加 `int64 CreationTimestamp`（创建时间戳，用于时间窗口查询）
- 增加 `FInstancedStruct CustomMetadata`（自定义元数据，如暴击标记）

**否决理由**：
- SourceHandle 的职责是"标识来源"，不是"记录战斗事件"
- 时间戳应该由伤害事件记录系统负责（完整记录：时间戳 + DamageValue + SourceHandle）
- 暴击标记等战斗数据应该在其他系统中扩展（如伤害事件 Payload），而不是塞进来源标识
- 职责混淆会导致 SourceHandle 结构体膨胀，影响每次复制和序列化的性能

### 3.3 SourceHandle 生命周期事件 — 否决

**提案**：增加 `OnSourceHandleCreated` / `OnSourceHandleDestroyed` 委托。

**否决理由**：
- SourceHandle 是一个**值类型标识符**（USTRUCT），不是有生命周期的实体（UObject）
- 给标识符加生命周期事件违背了它的定位
- 如果需要追踪 Modifier 的添加/移除，已有 `BroadcastAttributeModifierAddedEvent` / `BroadcastAttributeModifierRemovedEvent`
- 如果需要追踪 StateInstance 的创建/销毁，已有 `NotifyStateApplySuccess` / `NotifyStateRemoved`

---

## 四、StateRemoval 流程完整分析

### 4.1 移除路径总览

所有状态移除最终都汇聚到 `RequestStateRemoval` → `FinalizeStateRemoval` 管线：

```
外部调用入口
├── RemoveState()                    → RequestStateRemoval(Reason=Removed)
├── RemoveStatesByDefId()            → RequestStateRemoval(Reason=Removed) x N
├── RemoveAllStatesInSlot()          → RequestStateRemoval(Reason=Removed) x N
├── RemoveAllStates()                → RequestStateRemoval(Reason=Removed) x N
├── CancelState()                    → RequestStateRemoval(Reason=Cancelled)
├── ExpireState()                    → RequestStateRemoval(Reason=Expired)
│
内部触发
├── UpdateActiveStateDurations()     → ExpireState()（持续时间到期）
└── ProcessPriorityOnlyMode()        → RequestStateRemoval()（优先级抢占）
```

### 4.2 RequestStateRemoval 核心流程

```
RequestStateRemoval(StateInstance, Request)
│
├── 1. 前置检查
│   ├── StateInstance 有效性
│   ├── StateComponent 有效性
│   └── 阶段检查：已经 Expired → return true
│
├── 2. 标记 PendingRemovalRequest
│   └── StateInstance->SetPendingRemovalRequest(Request)
│
├── 3. 确保 StateTree 运行（给予退场机会）
│   └── 未运行 → 尝试 StartStateTree()
│
├── 4. StateTree 是否在运行？
│   │
│   ├── 【未运行】→ 直接 FinalizeStateRemoval()
│   │
│   └── 【运行中】→ 发送 Event_RemovalRequested 给 StateTree
│       │
│       ├── Tick(0) 后 StateTree 停止
│       │   └── FinalizePendingRemovalRequest()
│       │       └── FinalizeStateRemoval()
│       │
│       └── StateTree 仍在运行 → 进入异步等待
│           └── 由 TcsStateComponent::TickPendingRemovals() 轮询
│               ├── 超过 5 秒发出 Warning
│               └── StateTree 停止后 → FinalizePendingRemovalRequest()
│                                       └── FinalizeStateRemoval()
```

### 4.3 FinalizeStateRemoval 详细步骤

```
FinalizeStateRemoval(StateInstance, RemovalReason)
│
├── Step 1: 停止 StateTree
│   └── StateInstance->StopStateTree()
│
├── Step 2: 设置 Stage = Expired（终态不可逆）
│   └── StateInstance->SetCurrentStage(SS_Expired)
│   └── 如果已经是 Expired → 提前返回（防止重复清理）
│
├── Step 3: 清理容器
│   ├── StateTreeTickScheduler.Remove()
│   ├── DurationTracker.Remove()
│   └── StateInstanceIndex.RemoveInstance()
│
├── 【待新增】Step 3.5: 清理通过 SourceHandle 关联的 Modifier
│   └── RemoveModifiersBySourceHandle(Owner, SourceHandle)
│
├── Step 4: 广播事件
│   ├── NotifyStateStageChanged(PreviousStage → SS_Expired)
│   └── NotifyStateRemoved(RemovalReason)
│
├── Step 5: 从槽位移除
│   ├── RemoveStateFromSlot()
│   └── RequestUpdateStateSlotActivation()
│
└── Step 6: 标记待 GC
    └── StateInstance->MarkPendingGC()
```

### 4.4 状态阶段转换矩阵

```
从 \ 到       Inactive    Active    HangUp    Pause    Expired
Inactive        -          Yes       No        No       Yes
Active          Yes         -        Yes       Yes      Yes
HangUp          Yes        Yes        -        No       Yes
Pause           Yes        Yes       No         -       Yes
Expired         No         No        No        No        -

规则：
- Expired 是终态，不可逆转
- Inactive 不能直接跳到 HangUp/Pause
- HangUp 和 Pause 之间不可互相转换
- 所有阶段都可以转到 Expired
```

### 4.5 StateTree 退场机制

`RequestStateRemoval` 的设计允许 StateTree 在收到移除请求后执行清理逻辑：

1. 通过 `Event_RemovalRequested` GameplayTag 事件通知 StateTree
2. StateTree 中的 `TcsStateRemovalConfirmTask` 可以监听此事件
3. StateTree 可以执行退场动画、清理效果等
4. StateTree 完成后自然停止，`TickPendingRemovals` 检测到后调用 `FinalizeStateRemoval`
5. 超过 5 秒未停止会发出 Warning（防御性设计）

### 4.6 StateTree Stop 时的 Task 退出行为验证

#### 核心问题

StateRemoval 过程中 `StopStateTree()` 被调用时，StateTree 内部各 Task 的 `ExitState()` 是否会被执行？这直接决定了 StateTree 中创建的动态资源（Montage、VFX、音效等）能否被正确清理。

#### 引擎源码验证

通过阅读 UE5.6 引擎源码 `StateTreeExecutionContext.cpp`，确认了完整的调用链：

```
Context.Stop(CompletionStatus)
│
├── 1. ExitState(Transition)                    // 退出所有活跃状态
│   └── 逆序遍历每个活跃 State 的每个 Task
│       └── Task.ExitState(*this, Transition)   ← 会被调用 ✅
│
└── 2. StopEvaluatorsAndGlobalTasks()           // 停止全局任务和评估器
    ├── 遍历每个 Global Task
    │   └── Task.ExitState(*this, Transition)   ← 会被调用 ✅
    ���── 遍历每个 Evaluator
        └── Eval.TreeStop(*this)                ← 会被调用 ✅
```

**关键代码位置**：
- `FStateTreeExecutionContext::Stop()` — `StateTreeExecutionContext.cpp:1263`
- `ExitState()` 中调用 `Task.ExitState()` — `StateTreeExecutionContext.cpp:3017`
- `StopEvaluatorsAndGlobalTasks()` 中调用 `Task.ExitState()` — `StateTreeExecutionContext.cpp:3649`

**结论：StateTree 被 Stop 时，引擎保证所有活跃 Task 的 ExitState() 都会被调用。**

#### StateTree 执行期间可能产生的行为完整清单

| # | 行为类别 | 典型场景 | 性质 |
|---|---------|---------|------|
| 1 | 创建 AttributeModifier | 加攻击力、减速、持续回血 | 持久性数据修改，不主动移除则永久存在 |
| 2 | 创建 SkillModifier | 减少冷却、增加消耗、修改技能参数 | 同上 |
| 3 | 对其他 Actor 施加 State | 燃烧Buff扩散、光环给队友加Buff | 创建了独立生命周期的 StateInstance |
| 4 | 生成 Actor（投射物/陷阱/召唤物） | 火球、地面陷阱、召唤宠物 | 独立 Actor，有自己的生命周期 |
| 5 | 播放 Montage 动画 | 攻击动作、施法动画、受击动画 | 绑定在 Owner 的 AnimInstance 上 |
| 6 | 播放 VFX（Niagara/Cascade） | 技能特效、Buff光环特效 | 可能 Attached 到 Actor 或世界空间 |
| 7 | 播放音效 | 技能音效、Buff循环音效 | 可能是 OneShot 或 Looping |
| 8 | 修改 GameplayTag | 添加"正在施法"、"免疫控制"等标签 | 持久性标记，不移除则永久存在 |
| 9 | 修改移动状态 | 改变 MovementMode、施加 RootMotion、锁定位移 | 影响角色物理行为 |
| 10 | 修改碰撞/物理 | 开启/关闭碰撞体、切换碰撞通道 | 影响交互逻辑 |
| 11 | 修改输入状态 | 屏蔽输入、切换输入映射 | 影响玩家操控 |
| 12 | 显示/隐藏 UI | Buff图标、施法进度条、伤害数字 | Widget 生命周期 |
| 13 | 发送 Gameplay Message | 通过 GameplayMessageSubsystem 广播事件 | 一次性通知，发完即止 |
| 14 | 设置 Timer/Delay | 延迟执行某些逻辑 | 挂在 TimerManager 上的回调 |
| 15 | 修改材质/Mesh 可见性 | Buff 发光效果、隐身透明化 | 视觉状态修改 |

#### 清理策略分档

**第一档：不处理会导致数据错误 → 通过 SourceHandle 系统级自动清理**

| 行为 | 清理机制 | 说明 |
|------|---------|------|
| #1 AttributeModifier | SourceHandle（FinalizeStateRemoval 中） | Task ExitState 可能清理不全（跨分支创建），需要系统级兜底 |
| #2 SkillModifier | SourceHandle（FinalizeStateRemoval 中） | 同上 |

SourceHandle 机制在此的核心价值：Modifier 的创建和移除不一定在同一个 Task 里，甚至可能在 StateTree 的不同分支中创建。单靠 Task 的 ExitState 无法保证所有 Modifier 都被清理。SourceHandle 提供了一个**与 StateTree 执行路径无关的、基于来源标识的全局清理能力**。

**第二档：不处理会导致表现异常 → 由 Task ExitState 自行清理（引擎保证调用）**

| 行为 | 清理方式 | 说明 |
|------|---------|------|
| #5 Montage | Task.ExitState() 中 StopMontage | Task 开发者职责 |
| #6 VFX | Task.ExitState() 中 DestroyComponent / Deactivate | Task 开发者职责 |
| #7 循环音效 | Task.ExitState() 中 Stop | Task 开发者职责 |
| #8 GameplayTag | 开发者自由选择 | 允许由 Task ExitState 托管，也允许开发者自行决定（如只添加不移除） |
| #9 移动状态 | Task.ExitState() 中恢复 MovementMode | Task 开发者职责 |
| #10 碰撞/物理 | Task.ExitState() 中恢复碰撞设置 | Task 开发者职责 |
| #11 输入状态 | Task.ExitState() 中恢复输入 | Task 开发者职责 |
| #12 UI | Task.ExitState() 中移除 Widget | Task 开发者职责 |
| #15 材质/Mesh | Task.ExitState() 中恢复 | Task 开发者职责 |

这些行为无需系统级干预，因为引擎已保证 Stop 时调用 ExitState。Task 开发者在 EnterState 中做了什么，就在 ExitState 中撤销什么。

**第三档：不需要自动处理 → 有独立生命周期或一次性行为**

| 行为 | 理由 |
|------|------|
| #3 施加的 State | 被施加的 State 有自己的生命周期管理，不应因施加者的 State 移除而连带移除（如：火球术 State 结束，但燃烧 Buff 应继续存在直到自己过期） |
| #4 生成的 Actor | 投射物已飞出、召唤物有独立 AI，不应因技能 State 结束就消失 |
| #13 Gameplay Message | 一次性广播，发完即止 |
| #14 Timer/Delay | StateTree 停止时 Task 的 ExitState 中清理；若 Timer 已触发则已执行完毕 |

---

## 五、待调研事项

### 5.1 ~~SourceHandle 全局注册表设计~~ — 已否决

> **否决理由**：因果链方案已从 `int32 ParentSourceHandleId` + 全局注册表改为 `TArray<FDataTableRowHandle> CausalityChain`。
> 使用静态资产引用（DataTableRowHandle）存储完整因果链，不存在悬空引用问题，无需全局注册表进行 ID 回溯查找。
> 详见 2.2 节设计决策过程。

### 5.2 ~~跨 Actor Modifier 清理~~ — 已完成

> **结论**：FinalizeStateRemoval 只需清理 Owner 上的 Modifier。
> 跨 Actor 的属性修改必须通过在目标上施加独立 State 实现，由目标 State 自身的 SourceHandle 管理 Modifier 生命周期。
> 详见 2.3 节"跨 Actor Modifier 清理策略"。

### 5.3 ~~SkillModifier 的 SourceHandle 集成~~ — 暂不讨论

> SkillModifier 尚处于早期设计阶段（连定义结构体都未创建），待 SkillModifier 基础架构成型后再调研 SourceHandle 集成。

### ~~5.4 CreateSourceHandle API 重构~~ — 设计完成

引入因果链并删除 SourceDefinition 后，CreateSourceHandle API 简化为单一入口：

```cpp
// 重构后：唯一的 CreateSourceHandle API
// SourceDefinition 和 SourceName 已从 SourceHandle 中删除
FTcsSourceHandle CreateSourceHandle(
    const TArray<FPrimaryAssetId>& CausalityChain,  // 必须：因果链（根源传空数组）
    AActor* Instigator,                              // 必须：施加者
    const FGameplayTagContainer& SourceTags = FGameplayTagContainer());  // 可选：来源标签
```

**设计决策**：
- 移除 `CreateSourceHandleSimple` 等多版本 API，只保留一个入口
- `SourceDefinition` 和 `SourceName` 不再作为参数（已从 SourceHandle 结构体中删除）
- `CausalityChain` 是必须参数，根源场景传空数组即可，语义清晰
- `SourceTags` 保留为可选参数，大多数场景不需要额外标签

### 5.5 网络同步影响

新增字段对网络同步的影响评估：
- `TArray<FPrimaryAssetId> CausalityChain` → 支持序列化，FPrimaryAssetId 本质是两个 FName，因果链深度通常不超过 3-4 层，开销可忽略
- 删除 `SourceDefinition` 和 `SourceName` → 减少了序列化数据量
- StateInstance 的 `SourceHandle` → 随 StateInstance 同步（目前 StateInstance 的网络同步方案待定）

### 5.6 对象池集成

StateInstance 使用对象池后，`FinalizeStateRemoval` 的清理流程需要适配：
- 回收到对象池时，SourceHandle 是否需要重置？
- 从对象池取出时，SourceHandle 如何重新初始化？

---

## 六、扩展项优先级汇总

| 扩展项 | 优先级 | 状态 | 说明 |
|--------|--------|------|------|
| StateInstance 携带 SourceHandle | P0 | ✅ 已实施 | UTcsStateInstance 新增 SourceHandle 字段及 getter/setter |
| TryApplyStateToTarget 签名扩展 | P0 | ✅ 已实施 | 新增 ParentSourceHandle 参数，支持因果链传递 |
| SourceHandle 因果链（CausalityChain） | P0 | ✅ 已实施 | `TArray<FPrimaryAssetId>` 存储完整因果路径 |
| FinalizeStateRemoval 清理 Modifier | P0 | ✅ 已实施 | 只清理 Owner，跨 Actor 由独立 State 管理 |
| CreateSourceHandle API 重构 | P0 | ✅ 已实施 | 单一 API 入口，删除 SourceDefinition/SourceName，CausalityChain + Instigator 为必须参数 |
| GetModifiersBySourceHandle const 修复 | P0 | ✅ 已实施 | 移除自愈逻辑，保持 const 语义，过期 ID 仅输出 Warning |
| SourceHandle 查询 API 增强 | P1 | 待实施 | 按 Tag/Instigator 批量查询 |
| ~~跨 Actor Modifier 清理~~ | ~~P1~~ | 已完成 | 只清理 Owner，跨 Actor 走独立 State 流程 |
| ~~SourceHandle 全局注册表~~ | ~~P1~~ | 已否决 | 因果链改用静态资产引用，无需注册表 |
| ~~SkillModifier SourceHandle 集成~~ | ~~P2~~ | 暂不讨论 | 待 SkillModifier 基础架构成型后再调研 |

---

## 附录 A：相关源文件索引

| 文件 | 说明 |
|------|------|
| `Public/TcsSourceHandle.h` | SourceHandle 结构体定义 |
| `Public/Attribute/TcsAttributeManagerSubsystem.h` | 属性管理子系统（SourceHandle API 所在） |
| `Private/Attribute/TcsAttributeManagerSubsystem.cpp` | 属性管理子系统实现 |
| `Public/State/TcsStateInstance.h` | StateInstance 定义 |
| `Private/State/TcsStateInstance.cpp` | StateInstance 实现 |
| `Public/State/TcsStateManagerSubsystem.h` | 状态管理子系统声明 |
| `Private/State/TcsStateManagerSubsystem.cpp` | 状态管理子系统实现（FinalizeStateRemoval 在此） |
| `Public/State/TcsStateComponent.h` | 状态组件声明 |
| `Private/State/TcsStateComponent.cpp` | 状态组件实现（TickPendingRemovals 在此） |
| `Public/Attribute/TcsAttributeComponent.h` | 属性组件（SourceHandleIdToModifierInstIds 索引在此） |
| `Public/Attribute/TcsAttributeModifier.h` | 修改器实例定义（SourceHandle 字段在此） |
