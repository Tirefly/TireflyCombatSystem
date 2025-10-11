# TCS 顶层 StateTree 与 槽位映射联动 实现方案（完整版）

## 1. 背景与目标
- 目标：让“顶层 StateTree”作为战斗实体的“槽位 Gate/策略编排器”，状态切换可在编辑器 StateTree 调试器中直观呈现；与底层“状态实例/槽位激活”形成完整闭环。
- 现状：
  - 已有 `UTcsStateComponent : UStateTreeComponent`、`BuildStateSlotMappings()`（`StateTreeStateName ↔ SlotTag`）、槽位激活模式、状态实例 StateTree 执行与 Tick。
  - 节点已适配 UE5.6 的外部/上下文数据句柄。

## 2. 设计概述
- 三层分工：
  - 顶层 StateTree（可视化）：驱动“槽位 Gate 开/关与时序”。
  - 槽位与组件层：存储/优先级/激活/合并/持续时间。
  - 状态实例层：各自 StateTree 执行技能/Buff 逻辑。
- 核心机制：StateTree 状态进入/退出 → 槽位 Gate 切换 → 触发 `UpdateStateSlotActivation(SlotTag)` → 根据模式/优先级激活/回退。

## 3. 数据结构与 API 变更
- 组件新增（已存在的保持兼容）：
  - `TMap<FGameplayTag, FStateTreeStateHandle> SlotToStateHandleMap;`
  - `TMap<FStateTreeStateHandle, FGameplayTag> StateHandleToSlotMap;`
  - `TMap<FGameplayTag, bool> SlotGateMap; // 新增：槽位 Gate 开关`
- 新增/调整 API：
  - `void BuildStateSlotMappings(); // 已实现`
  - `bool IsStateTreeSlotActive(FGameplayTag SlotTag) const; // 已实现（回退判断）`
  - `void SetSlotGateOpen(FGameplayTag Slot, bool bOpen);`
  - `bool IsSlotGateOpen(FGameplayTag Slot) const;`
  - 在 `FTcsStateCondition_SlotAvailable` 中同时参考 Gate 与占用情况（条件与顶层一致）。

## 4. 运行时联动流程
- 顶层 StateTree 状态变化（进入/退出）
  1) `UTcsStateComponent` 订阅/覆盖 StateTree 状态变化回调（基于 `FStateTreeTransitionResult` 列表）。
  2) 对每个 Transition：
     - 如果 `StateHandleToSlotMap` 命中 `SlotTag`：
       - Enter → `SetSlotGateOpen(SlotTag, true)`；Exit → `SetSlotGateOpen(SlotTag, false)`。
       - 调用 `UpdateStateSlotActivation(SlotTag)`（让槽位内状态根据模式/优先级切换）。
- 槽位内状态的添加/合并/激活
  - `AssignStateToStateSlot()` 按优先级插入，命中同定义ID时执行 `UTcsStateMerger_*`；若 Gate 关闭则不激活（但可存储，等待 Gate 打开时自动激活）。
- 条件与任务
  - `FTcsStateCondition_SlotAvailable`：`return IsSlotGateOpen(Slot)&&!IsStateSlotOccupied(Slot)`（可扩展优先级/预占位策略）。
  - `FTcsCombatTask_ApplyState`：继续走 `UTcsStateManagerSubsystem::ApplyState` → 槽位分配。
  - 策略调用时机（设计）：
    - 免疫（CheckImmunity）：发生在 `ApplyState` 管线开始处；若命中免疫则不创建实例；未命中则进入分配（不返回“缩放/抗性”）。
    - 净化（Cleanse）：在顶层 StateTree 的合适节点（如 OnEnter/OnExit）通过“Cleanse 任务”触发，传入“选择器策略句柄 + 策略参数”执行移除。

## 5. 编辑器与配置规范
- 槽位配置表：`FTcsStateSlotDefinition` 中填 `SlotTag`、`ActivationMode`、`StateTreeStateName`（用于映射）。
- 顶层 StateTree：对应槽位的“Gate 状态”使用一级/二级 State 节点组织；命名与表中 `StateTreeStateName` 一致。
- 命名唯一：同一 StateTree 资产中被映射的 `StateTreeStateName` 不重复。

## 6. 边界与容错
- 找不到映射状态名 → 记录日志，联动跳过（槽位仍可手动/脚本驱动）。
- Gate 关闭时的行为：
  - PriorityOnly：强制仅最高优先级且 Gate 打开时才激活；Gate 关闭时可停用当前激活或保持 Inactive 存储（根据设计选项）。
  - AllActive：Gate 关闭时全部设为 Inactive；打开后批量激活。
- 循环与抖动保护：同帧批量 Transition 合并处理；避免重复排序/激活。

## 7. 任务拆解（实施顺序）
1) 组件侧：
   - 新增 `SlotGateMap`、`SetSlotGateOpen/IsSlotGateOpen`；
   - 覆盖/订阅 StateTree 状态变化回调，基于 `StateHandleToSlotMap` 切 Gate + 调 `UpdateStateSlotActivation()`；
   - `AssignStateToStateSlot()` 在 Gate 关闭时仅“存储不激活”。
2) 条件侧：
   - `FTcsStateCondition_SlotAvailable` 增加 Gate 判断；说明文档更新。
3) 验证：
   - 顶层 StateTree 切换分支，观察槽位 Gate 与槽内状态激活切换；
   - 两种模式（PriorityOnly/AllActive）+ 多状态优先级/合并碰撞用例；
   - 开关频繁切换无抖动。

## 8. 可选增强与优化

### PriorityHangUp 模式（同槽策略统一）
ActivationMode 新增 `PriorityHangUp`：同槽仅最高优先级实例 Active，其余实例进入“挂起（HangUp）”，不 Stop/不 Exit，暂停 StateTree Tick；
当高优先级退出时，按优先级恢复挂起实例的 Active。需要状态实例层提供 Pause/Resume 能力（第 12.3 节）。

### 调试可视化
Evaluator 动态输出每个槽位当前激活状态名/叠层数到 StateTree 参数，便于调试器查看。

### Gate 生命周期策略
支持 Gate 关闭时保留/停止状态树、是否刷新剩余时间等策略开关。

### 事件与遥测
通过 `GameplayMessageRouter` 广播 Gate/激活变化；打点统计状态切换质量与时延。

### 性能优化
激活更新批处理；避免同帧重复排序；日志等级细化（VeryVerbose）。

### （新增）合并策略接入 UTcsStateMerger_*
- 目标：替代当前重复状态的简化策略，使 `AssignStateToStateSlot()` 在命中同定义ID时根据“同源/异源”选取合并器并执行：
  - 同源：`SameInstigatorMergerType`（如 `UTcsStateMerger_Stack` 合并叠层、`UseNewest/UseOldest` 保留策略）。
  - 异源：`DiffInstigatorMergerType`（不同施加者的共存/合并规则）。
- 实施要点：
  1) 收集 Slot 中同 DefId 的状态 → 传入 Merger::Merge(StatesToMerge, MergedStates)。
  2) 以合并结果替换原集合（回收被合并/淘汰的实例，维护索引与持续时间映射）。
  3) 重新排序并调用 `UpdateStateSlotActivation(SlotTag)`。

### （新增）顶层调试 Evaluator（Slot 概览）
- 目标：在顶层 StateTree 调试器面板中直观查看各槽位当前激活数量/最高优先级状态等信息，便于快速定位问题。
- 方案：添加轻量 Evaluator（例如 `FTcsSlotDebugEvaluator`）：
  - Link 阶段获取 `UTcsStateComponent` 外部数据；
  - Tick 阶段收集：
    - `TMap<FGameplayTag, int32> ActiveCountBySlot`；
    - `TMap<FGameplayTag, FName> HighestActiveDefIdBySlot`；
    - 可选：`TMap<FGameplayTag, int32> TotalStoredBySlot`、`GateOpenBySlot`；
  - 将结果写入 Evaluator 的实例数据/参数，调试器中可见；必要时添加 `GetDescription()` 输出摘要。

## 9. 验收标准
- 顶层 StateTree 切换能在编辑器调试器直观呈现；槽位 Gate 状态与槽内激活行为一致；
- 两种激活模式行为正确（PriorityOnly 回退/AllActive 批量）；
- 条件/任务对上下文/外部数据读取稳定（UE5.6 句柄）；
- 无显著性能抖动；日志可追踪关键决策。

---

备注：本方案不直接写入属性值（遵循“所有属性修改通过 AttributeModifier 管道”的项目原则）。

## 10. 优化：持续时间计时策略（通用方案）
为适配不同业务对“Gate 关闭/状态激活”与“持续时间递减”的关系，定义通用的计时策略枚举，并将其配置到 `FTcsStateDefinition`：

- 枚举：`ETcsDurationTickPolicy`
  - `ActiveOnly`（默认，保持兼容）：仅当状态处于 `SS_Active` 且 `DurationType == SDT_Duration` 时递减。
  - `Always`：只要 `DurationType == SDT_Duration` 就递减，与激活/Gate 无关。
  - `OnlyWhenGateOpen`：仅当槽位 Gate 为 Open 时递减（无论 Active 与否）。
  - `ActiveOrGateOpen`：当 Active 或 Gate Open 时递减（两者其一即可）。
  - 可扩展：`ActiveAndGateOpen`（仅当 Active 且 Gate Open 时递减）等。

- 运行时判定（在 `UTcsStateComponent::TickComponent` 中对每个有时长的状态执行）：
  - 计算：`bIsActive = (Stage == SS_Active)`；`bGateClosed = (HasSlot && !IsSlotGateOpen(SlotTag))`。
  - 根据策略决定是否递减 `RemainingDuration`：
    - `ActiveOnly`：`bIsActive`
    - `Always`：`true`
    - `OnlyWhenGateOpen`：`!bGateClosed`
    - `ActiveOrGateOpen`：`bIsActive || !bGateClosed`
    - `ActiveAndGateOpen`：`bIsActive && !bGateClosed`

- 语义说明与边界：
  - 未映射槽位（无 SlotTag）视为 Gate 一直 Open。
  - `SDT_Infinite` 不参与计时；`SDT_None` 忽略。
  - 此策略仅影响“时长递减”行为；不改变激活/停用、Gate 开关与子 StateTree Start/Stop 的现有逻辑。
  - 若后续引入“暂停原因（PauseReason/Flags）”，可在策略计算中加入更细粒度的控制（例如因手动挂起而不计时、但 Gate 关闭仍可按策略计时）。

- 数据与兼容：
  - 为保持旧行为，默认策略为 `ActiveOnly`；已有数据无需迁移即可保持一致语义。
  - 为有特殊需求的状态（例如希望 Gate 关闭时持续流逝）设为 `OnlyWhenGateOpen` 或 `Always` 即可。

## 11. 两种暂停/恢复方案：多槽位（跨槽暂停）与同槽（抢占暂停）

为满足“可并行但存在条件性恢复”的复杂业务（如：蓄力期间触发闪避，完美闪避则恢复，否则取消），同时提供两条实现路径，供按场景选择或混用。

### 11.1 方案A：多槽位 + 跨槽暂停/恢复（推荐用于域分离）
- 建模：`Slot.Charge`（蓄力进度/逻辑）与 `Slot.Evasion`（闪避/I-Frame）分属不同槽位。
- Gate 策略：在 `FTcsStateSlotDefinition` 配置 Gate 与行为；顶层 StateTree 通过 `StateTreeStateName` 可视化控制 Gate。
- Gate 关闭行为：`PauseOnGateClose`（挂起）或 `CancelOnGateClose`（直接取消）。
- 计时策略：使用 10 节定义的 `ETcsDurationTickPolicy` 控制挂起期间是否递减时长。
- 结果回传：Evasion 退出时广播“PerfectDodge 成功/失败”，组件据此对“挂起的 Charge”执行 Resume 或 Cancel。

### 11.2 方案B：同槽 + 优先级抢占暂停/恢复（单轴互斥）
- 建模：将 Charge 与 Evasion 放在同一互斥轴（如 `Slot.Action`），`PriorityOnly` 控制抢占。
- 抢占策略：`PauseOnPreempt`（挂起保留上下文）或 `CancelOnPreempt`（取消）。
- 计时策略：仍用 `ETcsDurationTickPolicy`。
- 结果回传：高优先级状态退出时广播结果；组件对被挂起的低优先级状态 Resume/Cancel。

### 11.3 共同积木与落地要点
- 挂起：设置 `Stage=SS_HangUp`，不 Stop/不 Exit/不清空 `FStateTreeInstanceData`；暂停 Tick。
- 恢复：将 `Stage` 设回 `SS_Active`，下一帧直接 Tick（不 Start）。
- 取消：`Stop` + 清理，进入 `SS_Expired/Inactive`。
- 结果回传渠道：GameplayMessageRouter（推荐）或 `UTcsStateComponent` 回调；顶层也可通过 StateTree 参数承载结果。

## 12. 具体数据结构草案

为支持上述功能，建议在以下数据结构中新增字段/枚举，便于数据驱动：

### 12.1 槽位定义（FTcsStateSlotDefinition，已存在字段略）
- 新增：
```
// 槽位 Gate 关闭时的行为
UENUM(BlueprintType)
enum class ETcsGateCloseBehavior : uint8
{
    PauseOnGateClose UMETA(ToolTip = "Gate 关闭时对槽内状态执行挂起 (HangUp)"),
    CancelOnGateClose UMETA(ToolTip = "Gate 关闭时对槽内状态执行取消 (Stop)"),
};

// FTcsStateSlotDefinition
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slot Configuration")
ETcsGateCloseBehavior GateCloseBehavior = ETcsGateCloseBehavior::CancelOnGateClose;
```

### 12.2 状态定义（FTcsStateDefinition，已存在字段略）
- 新增：
```
// 抢占时的行为（同槽优先级被更高状态抢占时）
UENUM(BlueprintType)
enum class ETcsPreemptionPolicy : uint8
{
    PauseOnPreempt UMETA(ToolTip = "被抢占时挂起，保留子StateTree上下文"),
    CancelOnPreempt UMETA(ToolTip = "被抢占时取消"),
};

// Gate/激活与持续时间递减的关系（见第10节）
UENUM(BlueprintType)
enum class ETcsDurationTickPolicy : uint8
{
    ActiveOnly,
    Always,
    OnlyWhenGateOpen,
    ActiveOrGateOpen,
    ActiveAndGateOpen,
};

// FTcsStateDefinition
UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
ETcsDurationTickPolicy DurationTickPolicy = ETcsDurationTickPolicy::ActiveOnly;

UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Preemption")
ETcsPreemptionPolicy PreemptionPolicy = ETcsPreemptionPolicy::CancelOnPreempt;
```

### 12.3 状态实例与组件 API（草案）
```
// UTcsStateInstance
UFUNCTION(BlueprintCallable) void Pause();   // Stage=SS_HangUp，暂停Tick
UFUNCTION(BlueprintCallable) void Resume();  // Stage=SS_Active，继续Tick
bool IsPaused() const;                       // Stage==SS_HangUp

// UTcsStateComponent（按需公开给蓝图或仅内部使用）
void PauseStateInstance(UTcsStateInstance* Inst);
void ResumeStateInstance(UTcsStateInstance* Inst);
// 槽级别操作（对槽内所有状态按 GateCloseBehavior 处理）
void PauseSlot(FGameplayTag SlotTag);
void ResumeSlot(FGameplayTag SlotTag);
```

### 12.4 结果回传（PerfectDodge 等）
```
// GameplayMessageRouter：建议定义统一的路由Tag
// Tag: TCS.Event.EvasionResult
USTRUCT(BlueprintType)
struct FTcsEvasionResult
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bPerfectDodge = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TWeakObjectPtr<AActor> Owner;
};

// 组件侧订阅消息：
// - 若 bPerfectDodge=true：Resume 被挂起的 Charge；否则 Cancel。
```

### 12.5 执行与计时逻辑（摘要）
- 槽位 Gate 关闭：
  - 若 `GateCloseBehavior == PauseOnGateClose`：对槽内状态调用 Pause（Stage=SS_HangUp，停止子StateTree Tick）。
  - 若 `GateCloseBehavior == CancelOnGateClose`：保持现有“设为 Inactive 并 Stop”的行为。
- 组件 Tick：
  - 对 `SDT_Duration` 的状态实例，根据 `DurationTickPolicy` 与 Gate 状态/Stage 判定是否递减。
- 抢占（同槽优先级）：
  - 若 `PreemptionPolicy == PauseOnPreempt`：对被抢占者 Pause；
  - 否则 Cancel（保持现有语义）。

## 13. 合并策略接入 UTcsStateMerger_*（数据结构与落地草案）

### 13.1 目标
在 `UTcsStateComponent::AssignStateToStateSlot()` 中替代“重复状态跳过”的简化策略：当命中同 `StateDefId` 时，根据“同源/异源”选择合并器执行合并，得到唯一且有序的状态集合，并据此更新激活。

### 13.2 选择逻辑（已有字段）
- `FTcsStateDefinition::SameInstigatorMergerType`：同一 Instigator（来源）时使用的合并器类型。
- `FTcsStateDefinition::DiffInstigatorMergerType`：不同 Instigator 时使用的合并器类型。
- 已有合并器类型：`UTcsStateMerger_Stack`、`UTcsStateMerger_UseNewest`、`UTcsStateMerger_UseOldest`、`UTcsStateMerger_NoMerge`。

### 13.3 算法草案
```
AssignStateToStateSlot(NewState, SlotTag):
  SlotStates = StateSlots[SlotTag];
  CleanupExpiredStates(SlotStates);

  // 1) 收集同 DefId 的候选
  Candidates = { S in SlotStates | S.StateDefId == NewState.StateDefId } ∪ { NewState }
  if Candidates.size == 1:
    插入 NewState，排序 → UpdateStateSlotActivation(SlotTag) → return

  // 2) 依来源（Instigator）分组或按需整体合并
  if 存在多来源且 DiffInstigatorMergerType 有效:
    Merged = DiffMerger.Merge(Candidates)
  else if SameInstigatorMergerType 有效:
    Merged = SameMerger.Merge(Candidates)
  else:
    Merged = UseNewest/NoMerge 的回退策略（配置或内置默认）

  // 3) 用 Merged 替换原集合
  - 从 SlotStates 移除 Candidates 中未被保留的实例（回收/标记GC，清理索引与持续时间映射）
  - 确保 Merged 中的实例均在 SlotStates（必要时补充 NewState 的初始化）
  - 对 SlotStates 重新按 Priority 排序
  - 调用 UpdateStateSlotActivation(SlotTag)
```

### 13.4 数据与事件（可选）
- 可在合并结束时广播 `OnStatesMerged(SlotTag, DefId, Removed, Kept)` 事件用于调试。
- 合并器实现中推荐使用 `ApplyTimestamp`、`StackCount` 等元数据指导决策。

### 13.5 性能与一致性
- 避免分配抖动：重用临时数组（Candidates/Merged）。
- 确保移除时同步清理：`ActiveStateInstances`、`StateInstancesById/ByDefId`、`StateDurationMap`。
- 合并后统一由组件侧调用 `UpdateStateSlotActivation()` 保持行为一致。

## 14. 顶层调试 Evaluator（Slot 概览）草案

### 14.1 目标
在顶层 StateTree 调试器中直观展示各槽位的当前状态，便于快速定位问题：包括 Gate 开关、激活数量、最高优先级状态等。

### 14.2 类型与实例数据
```
// 实例条目（避免在 StateTree 实例数据中使用 TMap，改用数组）
USTRUCT(BlueprintType)
struct FTcsSlotDebugEntry
{
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameplayTag SlotTag;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGateOpen = true;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ActiveCount = 0;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TotalStored = 0;
  UPROPERTY(EditAnywhere, BlueprintReadWrite) FName HighestActiveDefId = NAME_None;
};

// Evaluator 实例数据
USTRUCT()
struct FTcsSlotDebugEvaluatorInstanceData
{
  GENERATED_BODY()
  UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FTcsSlotDebugEntry> Snapshot;
};

// Evaluator 声明（头文件）
USTRUCT(meta=(DisplayName="TCS Slot Debug Evaluator"))
struct FTcsSlotDebugEvaluator : public FStateTreeEvaluatorBase
{
  GENERATED_BODY()
  using FInstanceDataType = FTcsSlotDebugEvaluatorInstanceData;

  // 句柄绑定：从上下文获取 UTcsStateComponent
  virtual bool Link(FStateTreeLinker& Linker) override { Linker.LinkExternalData(StateCompHandle); return true; }
  virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
#if WITH_EDITOR
  virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
private:
  TStateTreeExternalDataHandle<UTcsStateComponent> StateCompHandle;
};
```

### 14.3 Tick 与描述（实现摘要）
```
void FTcsSlotDebugEvaluator::Tick(...)
{
  auto* Comp = Context.GetExternalData(StateCompHandle);
  auto& Data = Context.GetInstanceData(*this);
  Data.Snapshot.Reset();

  for (auto& Pair : Comp->StateSlotDefinitions)
  {
    FTcsSlotDebugEntry E;
    E.SlotTag = Pair.Key;
    E.bGateOpen = Comp->IsSlotGateOpen(E.SlotTag);
    TArray<UTcsStateInstance*> Active = Comp->GetActiveStatesInStateSlot(E.SlotTag);
    E.ActiveCount = Active.Num();
    E.TotalStored = Comp->GetAllStatesInStateSlot(E.SlotTag).Num();
    if (UTcsStateInstance* Top = Comp->GetHighestPriorityActiveState(E.SlotTag))
    {
      E.HighestActiveDefId = Top->GetStateDefId();
    }
    Data.Snapshot.Add(E);
  }
}

FText FTcsSlotDebugEvaluator::GetDescription(...)
{
  const auto* Data = InstanceDataView.GetPtr<FTcsSlotDebugEvaluatorInstanceData>();
  if (!Data) return FText::GetEmpty();
  int32 OpenGates = 0; for (auto& E : Data->Snapshot) { OpenGates += E.bGateOpen ? 1 : 0; }
  return FText::FromString(FString::Printf(TEXT("Slots: %d, OpenGates: %d"), Data->Snapshot.Num(), OpenGates));
}
```

### 14.4 用法与注意
- 用法：在顶层 StateTree 添加该 Evaluator（全局常驻或放于根状态），无需额外配置即可显示快照。
- 注意：
  - 调试用途，避免重资产复制；
  - 若槽位很多，可降低 Tick 频率或仅在 Editor 调试时启用；
  - 可通过 PropertyBindings 将 `Snapshot` 绑定到 UI/Inspector 面板做更详细的展示。


## 15. 槽位排队与延迟应用（合并自“StateTree集成优化开发计划”，尚未实现）

- 目标：当目标槽位 Gate 未开启或不允许激活时，暂存待应用的状态实例；当槽位变为可用时自动应用，避免丢包与竞态。
- 触发：顶层 StateTree 未进入 Gate；互斥策略暂不允许；跨实体延迟到本体 Gate 打开。
- 组件侧 API 草案：
  - QueueStateForSlot(StateInstance, SlotTag)
  - ProcessQueuedStates(DeltaTime)（在 TickComponent 或 Gate 变化时调用）
  - ClearQueuedStatesForSlot(SlotTag)
- 数据：FQueuedStateData{ StateInstance, TargetSlot, EnqueueTime, TTL, IsExpired() }；组件维护数组 QueuedStates。
- 流程：
  1) AssignStateToStateSlot 检测 Gate 关闭或策略不允许时 → QueueStateForSlot → 返回 true（队列接管）。
  2) OnStateTreeStateChanged 或 ProcessQueuedStates 发现 IsSlotGateOpen(Slot) 为真 → 取出对应队列项 → 再次调用 AssignStateToStateSlot；成功后移除队列项。
  3) 过期/无效项在 ProcessQueuedStates 中清理。
- Gate 联动：Gate 打开批量尝试应用；Gate 关闭可选清队或保留（按业务配置）。
- 说明：与第 13 节“合并策略”配合，确保去重与顺序一致性。

## 16. 管理器扩展：ApplyStateToSpecificSlot（合并自“StateTree集成优化开发计划”，尚未实现）

- 目标：跨实体/任务路径可直达指定槽位，并与 Gate/队列机制协同。
- 服务层 API 草案：ApplyStateToSpecificSlot(TargetActor, StateDefId, SourceActor, TargetSlot, Parameters)
- 行为：
  - 无 UTcsStateComponent → 直接启动状态（与 ApplyState 的降级策略一致）。
  - Gate 关闭 → 组件 QueueStateForSlot 排队。
  - Gate 开启 → CreateStateInstance + AssignStateToStateSlot。
  - 参数透传：按需写入 UTcsStateInstance 的 Numeric/Bool/Vector 容器。
- 与现有 API：ApplyState 语义不变（用定义表里的 StateSlotType）；Only 当需要覆盖定义表槽位时使用本接口。

## 17. 未实现清单总览（统一版）

- 合并器接入 AssignStateToStateSlot（Same/Diff Instigator 路径；候选/保留/清理一致性）—见 13 节
- 持续时间策略 ETcsDurationTickPolicy（ActiveOnly/Always/OnlyWhenGateOpen/…）—见 10 节
- 暂停/恢复与抢占策略（GateCloseBehavior、PreemptionPolicy、HangUp 语义）—见 11–12 节
- 槽位排队与延迟应用（Queue/Process/Clear；与 Gate 联动）—见 15 节
- 管理器接口 ApplyStateToSpecificSlot（跨实体/任务直达槽位）—见 16 节
- 顶层 Slot Debug Evaluator（调试可视化）—见 14 节

注：本清单已合并“状态槽架构改进”“StateTree 集成优化计划”“TireflyCombatSystem 开发计划”等文档中尚未落地的条目；与本方案现有设计保持不重复，仅提供统一入口与索引。
