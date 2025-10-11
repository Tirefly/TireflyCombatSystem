# TCS 调用顺序图：关键模块（设计示意）

说明：本文汇总当前 TCS 项目若干关键模块的典型调用顺序，作为设计阶段的参考。仅示意，不代表最终实现细节。应你的建议，本稿将顺序图改为“伪代码 + 要点”。

目录
- 0) API 概览（并入 StateManager 后）
- 1) ApplyState 管线（含 CanApplyLight）
- 2) 顶层 StateTree ↔ 槽位 Gate 联动
- 3) AssignStateToStateSlot 与 ActivationMode/合并器
- 4) Cleanse 任务（选择器策略句柄 + 标准净化参数）
- 5) 同槽抢占与挂起（PriorityHangUp + PreemptionPolicy）
- 6) 持续时间计时策略（ETcsDurationTickPolicy）
- 7) 策略解析器注入与调用（项目侧）

——

## 0) API 概览（并入 StateManager 后）

伪代码（接口签名，仅文档）：
```cpp
// StateManager 对外主要接口
class UTcsStateManagerSubsystem {
public:
  bool CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& OutDecision) const; // 仅免疫判定
  bool ApplyState(AActor* Target, const FTcsStateDefinition& Def, AActor* Source, const FTcsEvalContext& Ctx, /*out*/FTcsStateHandle& OutHandle);
  int32 Cleanse(AActor* Target, const FTcsEvalContext& Ctx, const FTcsSelectorHandle& InlineSelector, const FTcsCleanseParams& InlineParams) const;

  // 策略解析器接入（项目侧）
  void AddPolicyResolver(ITcsPolicyResolver* Resolver);
  void ClearPolicyResolvers();
  FTcsCollectPoliciesDelegate OnCollectImmunityPolicies; // 可选委托
  FTcsCollectPoliciesDelegate OnCollectCleansePolicies;  // 可选委托
};
```

## 1) ApplyState 管线（含 CheckImmunity）

伪代码：
```cpp
bool UTcsStateManagerSubsystem::ApplyState(AActor* Target, const FTcsStateDefinition& Def, AActor* Source, const FTcsEvalContext& Ctx, FTcsStateHandle& Out)
{
  // 1) 轻量前置：免疫判定
  FTcsImmunityDecision Dec; if (!CheckImmunity(Def, Ctx, Dec)) { return false; }

  // 2) 交给组件按槽位分配
  UTcsStateComponent* SC = GetStateComponent(Target);
  if (!SC) return false;
  const bool ok = SC->AssignStateToStateSlot(Def);
  if (ok) { Out = MakeHandle(Target, Def); }
  return ok;
}

bool UTcsStateManagerSubsystem::CheckImmunity(const FTcsStateDefinition& Def, const FTcsEvalContext& Ctx, FTcsImmunityDecision& OutDec) const
{
  TArray<FTcsImmunityPolicyHandle> Policies; // 来自 Provider + 解析器 + 委托（见第7节）
  CollectImmunityPolicies(Ctx, Policies);

  FGameplayTag reason;
  for (const FTcsImmunityPolicyHandle& H : Policies) {
    if (!H.Class) continue;
    const auto* CDO = H.Class->GetDefaultObject<UTcsImmunityPolicy>();
    FTcsImmunityDecision D; const bool hit = CDO->Evaluate(Def, Ctx, H.PolicyParams, D);
    if (!hit) continue;
    if (!D.bAllowed) { OutDec = { false, D.ReasonTag }; return false; } // 命中免疫即阻断
    reason = D.ReasonTag.IsValid() ? D.ReasonTag : reason; // 诊断用途
  }
  OutDec = { true, reason };
  return true;
}
```

要点
- CheckImmunity：策略仅返回“命中免疫 → 阻断/允许 + ReasonTag”；无“缩放/抗性”。
- 失败早返回，避免创建实例与后续分配开销。

## 2) 顶层 StateTree ↔ 槽位 Gate 联动

伪代码：
```cpp
void UTcsStateComponent::OnStateTreeStateChanged(const FStateTreeTransitionResult& Tr)
{
  const FGameplayTag* Slot = StateHandleToSlotMap.Find(Tr.State);
  if (!Slot) return;
  const bool bOpen = (Tr.Change == EStateTreeStateChange::SucceedEntering || Tr.Change == EStateTreeStateChange::GotoEntering);
  SetSlotGateOpen(*Slot, bOpen);
  UpdateStateSlotActivation(*Slot);
}
```

要点
- 顶层 StateTree 负责 Gate 的开/关与时序；组件负责槽内状态的具体激活。
- 多个 StateTree 状态可映射不同 SlotTag，建议命名唯一。

## 3) AssignStateToStateSlot 与 ActivationMode/合并器

伪代码：
```cpp
bool UTcsStateComponent::AssignStateToStateSlot(const FTcsStateDefinition& Def)
{
  TArray<FTcsStateInstance*> Candidates = CollectSameDefIdCandidates(Def);
  Candidates.Add(CreateTempInstance(Def));

  TArray<FTcsStateInstance*> NewSet;
  if (ShouldMerge(Def, Candidates)) {
    UTcsStateMergerBase* Merger = PickMerger(Def, Candidates);
    NewSet = Merger->Merge(Candidates);
  } else {
    NewSet = InsertSortedByPriority(Candidates);
  }

  ReplaceSlotSet(Def.StateSlotType, NewSet);
  if (!IsSlotGateOpen(Def.StateSlotType)) {
    MarkInactive(Def.StateSlotType); return true; // 存储不激活
  }
  return ApplyActivationMode(Def.StateSlotType); // PriorityOnly / PriorityHangUp / AllActive
}
```

要点
- PriorityOnly：仅最高优先级 Active，较低优先 Cancel。
- PriorityHangUp：较低优先进入挂起（暂停 Tick，不 Stop/Exit），高优先退出时恢复。
- AllActive：并发 Active，由组件保障并发语义。

## 4) Cleanse 任务（选择器策略句柄 + 标准净化参数）

伪代码：
```cpp
int32 UTcsStateManagerSubsystem::Cleanse(AActor* Target, const FTcsEvalContext& Ctx, const FTcsSelectorHandle& InlineSelector, const FTcsCleanseParams& InlineParams) const
{
  TArray<FTcsCleansePolicyHandle> Policies; // 解析器 + 委托提供的净化策略
  CollectCleansePolicies(Ctx, Policies);

  // 将任务内联的 Selector+Params 组装成一个“临时净化策略”并加入队列（可选）
  Policies.Insert(MakeInlineCleansePolicy(InlineSelector, InlineParams), 0);

  int32 total = 0; UTcsStateComponent* SC = GetStateComponent(Target);
  for (const FTcsCleansePolicyHandle& H : Policies) {
    if (!H.Class) continue;
    const auto* CP = H.Class->GetDefaultObject<UTcsCleansePolicy>();
    total += CP->Apply(Ctx, H.PolicyParams); // 策略内部：遍历 SC 的状态，按 Selector 命中后执行移除
  }
  return total;
}
```

要点
- Cleanse 的命中范围由“选择器策略句柄”表达（ByIds/ByTags）。
- 标准净化参数：MaxRemoveCount（-1 表示全部）。
- 项目侧可决定 CleansePolicies 的选择逻辑；也可仅使用“标准净化策略”。

## 5) 同槽抢占与挂起（PriorityHangUp + PreemptionPolicy）

伪代码：
```cpp
bool UTcsStateComponent::ApplyActivationMode(FGameplayTag Slot)
{
  switch (GetActivationMode(Slot)) {
    case PriorityOnly:   return ActivateTopPriorityAndCancelOthers(Slot);
    case AllActive:      return ActivateAll(Slot);
    case PriorityHangUp: return ActivateTopPriorityAndHangUpOthers(Slot);
  }
  return false;
}

void UTcsStateComponent::OnPreempted(UTcsStateInstance* Loser)
{
  switch (Loser->GetPreemptionPolicy()) {
    case PauseOnPreempt:  Loser->Pause(); break;   // Stage=HangUp，保留上下文
    case CancelOnPreempt: Loser->Stop();  break;   // 直接取消
  }
}
```

要点
- 抢占策略（预留）：被更高优先级抢占时，Pause 或 Cancel（由 PreemptionPolicy 决定）。
- 恢复顺序：按优先级恢复被挂起的实例。

## 6) 持续时间计时策略（ETcsDurationTickPolicy）

伪代码：
```cpp
void UTcsStateComponent::TickComponent(float DeltaTime)
{
  for (UTcsStateInstance* Inst : ActiveAndStoredStates) {
    if (Inst->GetDurationType() != SDT_Duration) continue;
    const bool Active  = Inst->IsActive();
    const bool GateOpen= IsSlotGateOpen(Inst->GetSlot());
    const auto  P = Inst->GetDurationTickPolicy();
    const bool DoTick =
      (P==ActiveOnly        && Active) ||
      (P==Always            ) ||
      (P==OnlyWhenGateOpen  && GateOpen) ||
      (P==ActiveOrGateOpen  && (Active || GateOpen)) ||
      (P==ActiveAndGateOpen && (Active && GateOpen));
    if (DoTick) Inst->DecRemaining(DeltaTime);
  }
}
```

要点
- 与挂起/抢占配合，避免不期望的“后台倒计时”。
- 该策略不改变激活/停用，仅影响剩余时长的递减。

## 7) 策略解析器注入与调用（项目侧）

伪代码：
```cpp
// 启动/配置
void GameModule::StartupModule()
{
  ITcsPolicyResolver* GlobalResolver = NewObject<UTcsPolicyResolver_DataDriven>(...);
  StateManager.AddPolicyResolver(GlobalResolver);
  // 或：注册委托 StateManager.OnCollectImmunityPolicies.BindUObject(...)
}

// 收集策略（StateManager 内）
void UTcsStateManagerSubsystem::CollectImmunityPolicies(const FTcsEvalContext& Ctx, TArray<FTcsImmunityPolicyHandle>& Out) const
{
  // 1) Provider（目标本地策略）
  if (auto* Prov = Cast<ITcsPolicyProvider>(Ctx.Target)) { Prov->GetLocalImmunityPolicies(Out); }
  // 2) 解析器（全局/关卡等）
  for (ITcsPolicyResolver* R : Resolvers) { R->CollectImmunityPolicies(Ctx, Out); }
  // 3) 委托（可选）
  if (OnCollectImmunityPolicies.IsBound()) { OnCollectImmunityPolicies.Execute(Ctx, Out); }
}
```

要点
- 解析器按项目需求实现：可基于 ContextTags/关卡/模式/职业等选择策略集合。
- StateManager 仅负责调用解析器并执行评估/聚合逻辑。

——

附：相关文档
- TCS_状态关系规则系统设计_规则资产与状态管理器.md
- TCS_状态系统整体方案_顶层StateTree_槽位_实例联动与扩展.md
- TCS_顶层StateTree与槽位映射联动实现方案.md
