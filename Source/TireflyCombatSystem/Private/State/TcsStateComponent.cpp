// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"



UTcsStateComponent::UTcsStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bStartLogicAutomatically = true;
}

void UTcsStateComponent::BeginPlay()
{
	// 获取状态管理器子系统
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] World is invalid."), *FString(__FUNCTION__));
		return;
	}

	StateMgr = World->GetSubsystem<UTcsStateManagerSubsystem>();
	if (!StateMgr)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get TcsStateManagerSubsystem."),
			*FString(__FUNCTION__));
		return;
	}

    // 初始化StateSlot和StateTreeState的映射
	StateMgr->InitStateSlotMappings(GetOwner());

	// 各项初始化之后，再执行状态管理StateTree
	Super::BeginPlay();
}

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 更新持续时间状态的剩余时间
	UpdateActiveStateDurations(DeltaTime);

	// Tick 激活状态的 StateTree
	StateTreeTickScheduler.RefreshInstances();

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

	// Warning-only: pending removal requests that never stop their StateTree.
	{
		static const double PendingRemovalWarnSeconds = 5.0;
		const int64 NowTicks = FDateTime::UtcNow().GetTicks();
		for (UTcsStateInstance* StateInstance : StateInstanceIndex.Instances)
		{
			if (!IsValid(StateInstance))
			{
				continue;
			}

			if (!StateInstance->HasPendingRemovalRequest() || !StateInstance->IsStateTreeRunning())
			{
				continue;
			}

			if (StateInstance->HasPendingRemovalRequestWarningIssued())
			{
				continue;
			}

			const int64 StartTicks = StateInstance->GetPendingRemovalRequestStartTimeTicks();
			if (StartTicks <= 0)
			{
				continue;
			}

			const double ElapsedSeconds = (FDateTime(NowTicks) - FDateTime(StartTicks)).GetTotalSeconds();
			if (ElapsedSeconds >= PendingRemovalWarnSeconds)
			{
				UE_LOG(LogTcsState, Warning, TEXT("[%s] Pending removal request still running after %.2fs. State=%s Id=%d Stage=%s"),
					*FString(__FUNCTION__),
					ElapsedSeconds,
					*StateInstance->GetStateDefId().ToString(),
					StateInstance->GetInstanceId(),
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(StateInstance->GetCurrentStage())));
				StateInstance->MarkPendingRemovalRequestWarningIssued();
			}
		}
	}

	TArray<UTcsStateInstance*> InstancesToRemove;
	for (UTcsStateInstance* RunningState : StateTreeTickScheduler.RunningInstances)
	{
		if (!IsValid(RunningState))
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		if (!RunningState->IsStateTreeRunning())
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		const bool bPendingRemoval = RunningState->HasPendingRemovalRequest();
		bool bShouldTick = bPendingRemoval;
		if (!bShouldTick)
		{
			bShouldTick =
				(RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
				(RunningState->GetStateDef().TickPolicy == ETcsStateTreeTickPolicy::WhileActive);
		}

		if (!bShouldTick)
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		RunningState->TickStateTree(DeltaTime);
		if (!RunningState->IsStateTreeRunning())
		{
			InstancesToRemove.Add(RunningState);
		}
	}

	for (UTcsStateInstance* StateInstance : InstancesToRemove)
	{
		StateTreeTickScheduler.Remove(StateInstance);
	}

	if (IsValid(StateMgr))
	{
		TArray<UTcsStateInstance*> PendingFinalize;
		for (UTcsStateInstance* StateInstance : StateInstanceIndex.Instances)
		{
			if (!IsValid(StateInstance))
			{
				continue;
			}

			if (StateInstance->HasPendingRemovalRequest() && !StateInstance->IsStateTreeRunning())
			{
				PendingFinalize.Add(StateInstance);
			}
		}

		for (UTcsStateInstance* StateInstance : PendingFinalize)
		{
			StateMgr->FinalizePendingRemovalRequest(StateInstance);
		}
	}
}

float UTcsStateComponent::GetStateRemainingDuration(const UTcsStateInstance* StateInstance) const
{
	if (IsValid(StateInstance) && StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		return -1.0f;
	}

	float Remaining = 0.f;
	if (DurationTracker.GetRemaining(StateInstance, Remaining))
	{
		return Remaining;
	}

	if (IsValid(StateInstance) && StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Duration)
	{
		return StateInstance->GetTotalDuration();
	}
	
	return 0.0f;
}

void UTcsStateComponent::RefreshStateRemainingDuration(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		// Infinite duration state has no remaining duration to refresh.
		return;
	}

	if (StateInstance->GetStateDef().DurationType != ETcsStateDurationType::SDT_Duration)
	{
		return;
	}

	// 刷新为总持续时间
	const float NewRemaining = StateInstance->GetTotalDuration();
	if (!DurationTracker.SetRemaining(StateInstance, NewRemaining))
	{
		DurationTracker.Add(StateInstance, NewRemaining);
	}

	// 广播状态持续时间刷新事件
	NotifyStateDurationRefreshed(StateInstance, NewRemaining);
}

void UTcsStateComponent::SetStateRemainingDuration(UTcsStateInstance* StateInstance, float InDurationRemaining)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		// Infinite duration state ignores manual duration changes.
		return;
	}

	if (StateInstance->GetStateDef().DurationType != ETcsStateDurationType::SDT_Duration)
	{
		return;
	}

	// 设置新的剩余时间
	if (!DurationTracker.SetRemaining(StateInstance, InDurationRemaining))
	{
		DurationTracker.Add(StateInstance, InDurationRemaining);
	}

	// 广播状态持续时间刷新事件
	NotifyStateDurationRefreshed(StateInstance, InDurationRemaining);
}

void UTcsStateComponent::UpdateActiveStateDurations(float DeltaTime)
{
	if (!StateMgr)
	{
		return;
	}

	// 收集过期状态，避免在遍历过程中修改容器
	TArray<UTcsStateInstance*> ExpiredStates;
	TArray<UTcsStateInstance*> InvalidStates;

	for (auto& DurationPair : DurationTracker.RemainingByInstance)
	{
		UTcsStateInstance* StateInstance = DurationPair.Key;
		float& RemainingDuration = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		// 确认状态当前的状态阶段
		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();

		// SS_Expired 状态已经过期，跳过
		if (CurrentStage == ETcsStateStage::SS_Expired)
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		if (CurrentStage != ETcsStateStage::SS_Active
			&& CurrentStage != ETcsStateStage::SS_HangUp
			&& CurrentStage != ETcsStateStage::SS_Inactive
			&& CurrentStage != ETcsStateStage::SS_Pause)
		{
			continue;
		}

		// 检查状态所属的状态槽的状态
		const FGameplayTag StateSlotTag = StateInstance->GetStateDef().StateSlotType;
		FTcsStateSlotDefinition SlotDefinition;
		if (!StateMgr->TryGetStateSlotDefinition(StateSlotTag, SlotDefinition))
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get StateSlotDefinition for %s."),
				*FString(__FUNCTION__),
				*StateSlotTag.ToString());
			continue;
		}

		// Pause 阶段默认冻结；如槽位配置允许，则 Pause 也遵循 DurationTickPolicy。
		if (CurrentStage == ETcsStateStage::SS_Pause && SlotDefinition.bFreezeDurationWhenPaused)
		{
			continue;
		}

		// 判断状态是否处于"激活"状态（Active 或 HangUp 都视为激活，因为 HangUp 仍计算持续时间）
		const bool bStateIsActive = (CurrentStage == ETcsStateStage::SS_Active);
		const bool bStateIsHangUp = (CurrentStage == ETcsStateStage::SS_HangUp);

		const FTcsStateSlot* SlotData = StateSlotTag.IsValid() ? StateSlotsX.Find(StateSlotTag) : nullptr;
		const bool bGateOpen = !StateSlotTag.IsValid() || (SlotData && SlotData->bIsGateOpen);

		bool bShouldTick = false;
		// 根据状态槽的持续时间流失策略决定是否递减剩余持续时间
		switch (SlotDefinition.DurationTickPolicy)
		{
		case ETcsDurationTickPolicy::DTP_Always:
			// 始终计时（包括 HangUp）
			bShouldTick = true;
			break;
		case ETcsDurationTickPolicy::DTP_OnlyWhenGateOpen:
			// 仅 Gate 开启时计时
			bShouldTick = bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOrGateOpen:
			// Active/HangUp 或 Gate 开启时计时
			bShouldTick = bStateIsActive || bStateIsHangUp || bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveAndGateOpen:
			// 仅 Active 且 Gate 开启时计时（HangUp 不计时）
			bShouldTick = bStateIsActive && bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOnly:
		default:
			// 仅 Active 时计时（HangUp 不计时）
			bShouldTick = bStateIsActive;
			break;
		}

		if (!bShouldTick)
		{
			continue;
		}

		// If already expired by duration, request expire once and keep remaining at 0 until finalized.
		if (RemainingDuration <= 0.0f)
		{
			if (!StateInstance->HasPendingRemovalRequest())
			{
				ExpiredStates.Add(StateInstance);
			}
			continue;
		}

		// 递减剩余持续时间，并检查是否到期
		RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
		if (RemainingDuration <= 0.0f && !StateInstance->HasPendingRemovalRequest())
		{
			ExpiredStates.Add(StateInstance);
		}
	}

	// 遍历结束后，统一处理过期状态
	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (IsValid(ExpiredState))
		{
			// 通过 StateManagerSubsystem 正确处理过期流程
			// ExpireState 内部会处理：设置 Stage、从容器移除、广播事件
			StateMgr->ExpireState(ExpiredState);
		}
		else
		{
			// 无效指针：直接从 Map 移除
			InvalidStates.Add(ExpiredState);
		}
	}

	for (UTcsStateInstance* InvalidState : InvalidStates)
	{
		DurationTracker.Remove(InvalidState);
	}
}

void UTcsStateComponent::NotifyStateStageChanged(UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (!IsValid(StateInstance) || PreviousStage == NewStage)
	{
		return;
	}

	if (OnStateStageChanged.IsBound())
	{
		OnStateStageChanged.Broadcast(this, StateInstance, PreviousStage, NewStage);
	}
}

void UTcsStateComponent::NotifyStateDeactivated(
	UTcsStateInstance* StateInstance,
	ETcsStateStage NewStage,
	FName DeactivateReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateDeactivated.IsBound())
	{
		OnStateDeactivated.Broadcast(this, StateInstance, NewStage, DeactivateReason);
	}
}

void UTcsStateComponent::NotifyStateRemoved(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateRemoved.IsBound())
	{
		OnStateRemoved.Broadcast(this, StateInstance, RemovalReason);
	}
}

void UTcsStateComponent::NotifyStateStackChanged(UTcsStateInstance* StateInstance, int32 OldStackCount, int32 NewStackCount)
{
	if (!IsValid(StateInstance) || OldStackCount == NewStackCount)
	{
		return;
	}

	if (OnStateStackChanged.IsBound())
	{
		OnStateStackChanged.Broadcast(this, StateInstance, OldStackCount, NewStackCount);
	}
}

void UTcsStateComponent::NotifyStateLevelChanged(UTcsStateInstance* StateInstance, int32 OldLevel, int32 NewLevel)
{
	if (!IsValid(StateInstance) || OldLevel == NewLevel)
	{
		return;
	}

	if (OnStateLevelChanged.IsBound())
	{
		OnStateLevelChanged.Broadcast(this, StateInstance, OldLevel, NewLevel);
	}
}

void UTcsStateComponent::NotifyStateDurationRefreshed(UTcsStateInstance* StateInstance, float NewDuration)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateDurationRefreshed.IsBound())
	{
		OnStateDurationRefreshed.Broadcast(this, StateInstance, NewDuration);
	}
}

void UTcsStateComponent::NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen)
{
	if (!SlotTag.IsValid())
	{
		return;
	}

	if (OnSlotGateStateChanged.IsBound())
	{
		OnSlotGateStateChanged.Broadcast(this, SlotTag, bIsOpen);
	}
}

void UTcsStateComponent::NotifyStateParameterChanged(
	UTcsStateInstance* StateInstance,
	ETcsStateParameterKeyType KeyType,
	FName ParameterName,
	FGameplayTag ParameterTag,
	ETcsStateParameterType ParameterType)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateParameterChanged.IsBound())
	{
		OnStateParameterChanged.Broadcast(StateInstance, KeyType, ParameterName, ParameterTag, ParameterType);
	}
}

void UTcsStateComponent::NotifyStateMerged(UTcsStateInstance* TargetStateInstance, UTcsStateInstance* SourceStateInstance, int32 ResultStackCount)
{
	if (!IsValid(TargetStateInstance) || !IsValid(SourceStateInstance))
	{
		return;
	}

	if (OnStateMerged.IsBound())
	{
		OnStateMerged.Broadcast(this, TargetStateInstance, SourceStateInstance, ResultStackCount);
	}
}

void UTcsStateComponent::NotifyStateApplySuccess(
	AActor* TargetActor,
	FName StateDefId,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag TargetSlot,
	ETcsStateStage AppliedStage)
{
	if (!IsValid(TargetActor) || StateDefId.IsNone() || !IsValid(CreatedStateInstance) || !TargetSlot.IsValid())
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] ApplySuccess: Target=%s State=%s Id=%d Slot=%s Stage=%s P=%d Tick=%s"),
		*FString(__FUNCTION__),
		*TargetActor->GetName(),
		*StateDefId.ToString(),
		CreatedStateInstance->GetInstanceId(),
		*TargetSlot.ToString(),
		*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(AppliedStage)),
		CreatedStateInstance->GetStateDef().Priority,
		*StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(CreatedStateInstance->GetStateDef().TickPolicy)));

	if (OnStateApplySuccess.IsBound())
	{
		OnStateApplySuccess.Broadcast(TargetActor, StateDefId, CreatedStateInstance, TargetSlot, AppliedStage);
	}
}

void UTcsStateComponent::NotifyStateApplyFailed(
	AActor* TargetActor,
	FName StateDefId,
	ETcsStateApplyFailReason FailureReason,
	const FString& FailureMessage)
{
	if (!IsValid(TargetActor) || StateDefId.IsNone())
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] ApplyFailed: Target=%s State=%s Reason=%s Message=%s"),
		*FString(__FUNCTION__),
		*TargetActor->GetName(),
		*StateDefId.ToString(),
		*StaticEnum<ETcsStateApplyFailReason>()->GetNameStringByValue(static_cast<int64>(FailureReason)),
		*FailureMessage);

	if (OnStateApplyFailed.IsBound())
	{
		OnStateApplyFailed.Broadcast(TargetActor, StateDefId, FailureReason, FailureMessage);
	}
}

FStateTreeReference UTcsStateComponent::GetStateTreeReference() const
{
	return StateTreeRef;
}

const UStateTree* UTcsStateComponent::GetStateTree() const
{
	return StateTreeRef.GetStateTree();
}

void UTcsStateComponent::OnStateSlotChanged(FGameplayTag SlotTag)
{
	// TODO: StateTree状态槽变化事件处理
	// 这里可以添加状态槽变化的响应逻辑
	// 例如：通知StateTree系统、发送游戏事件等
	
	UE_LOG(LogTcsState, VeryVerbose, TEXT("State slot [%s] changed"), *SlotTag.ToString());
}

FString UTcsStateComponent::GetSlotDebugSnapshot(FGameplayTag SlotFilter) const
{
	auto BuildLine = [this](const FGameplayTag& SlotTag, const FTcsStateSlot& Slot) -> FString
	{
		FString Line = FString::Printf(TEXT("[%s] Gate=%s"),
			*SlotTag.ToString(),
			Slot.bIsGateOpen ? TEXT("Open") : TEXT("Closed"));

		if (IsValid(StateMgr))
		{
			FTcsStateSlotDefinition SlotDef;
			if (StateMgr->TryGetStateSlotDefinition(SlotTag, SlotDef))
			{
				Line += FString::Printf(TEXT(" Mode=%s Preempt=%s DurPolicy=%s PauseFreeze=%s"),
					*StaticEnum<ETcsStateSlotActivationMode>()->GetNameStringByValue(static_cast<int64>(SlotDef.ActivationMode)),
					*StaticEnum<ETcsStatePreemptionPolicy>()->GetNameStringByValue(static_cast<int64>(SlotDef.PreemptionPolicy)),
					*StaticEnum<ETcsDurationTickPolicy>()->GetNameStringByValue(static_cast<int64>(SlotDef.DurationTickPolicy)),
					SlotDef.bFreezeDurationWhenPaused ? TEXT("1") : TEXT("0"));
			}
		}

		auto FormatState = [](UTcsStateInstance* State) -> FString
		{
			if (!IsValid(State))
			{
				return TEXT("<invalid>");
			}

			const FString StateId = State->GetStateDefId().ToString();
			const int32 InstanceId = State->GetInstanceId();
			const int32 Priority = State->GetStateDef().Priority;
			const int32 StackCount = State->GetStackCount();
			const int32 Level = State->GetLevel();
			const float DurRemaining = State->GetDurationRemaining();
			const ETcsStateDurationType DurType = State->GetStateDef().DurationType;
			const ETcsStateTreeTickPolicy TickPolicy = State->GetStateDef().TickPolicy;
			const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));
			const FString DurStr = (DurType == ETcsStateDurationType::SDT_Infinite || DurRemaining < 0.0f)
				? TEXT("Inf")
				: FString::Printf(TEXT("%.2f"), DurRemaining);

			FString RemovalStr = TEXT("-");
			if (State->HasPendingRemovalRequest())
			{
				RemovalStr = State->GetPendingRemovalRequest().ToRemovalReasonName().ToString();
			}

			const AActor* Instigator = State->GetInstigator();
			const FString InstigatorName = Instigator ? Instigator->GetName() : TEXT("None");

			return FString::Printf(TEXT("%s#%d(P=%d,Stack=%d,Lv=%d,Dur=%s,Tick=%s,Rem=%s,Inst=%s)"),
				*StateId,
				InstanceId,
				Priority,
				StackCount,
				Level,
				*DurStr,
				*TickPolicyStr,
				*RemovalStr,
				*InstigatorName);
		};

		auto SortStates = [](TArray<UTcsStateInstance*>& States)
		{
			States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
			{
				const int32 AP = A.GetStateDef().Priority;
				const int32 BP = B.GetStateDef().Priority;
				if (AP != BP)
				{
					return AP > BP;
				}
				return A.GetInstanceId() < B.GetInstanceId();
			});
		};

		TArray<FString> ActiveStates;
		TArray<FString> HangUpStates;
		TArray<FString> PauseStates;
		TArray<FString> StoredStates;

		int32 ActiveCount = 0;
		int32 HangUpCount = 0;
		int32 PauseCount = 0;
		int32 InactiveCount = 0;

		TArray<UTcsStateInstance*> ActiveToSort;
		TArray<UTcsStateInstance*> HangUpToSort;
		TArray<UTcsStateInstance*> PauseToSort;
		TArray<UTcsStateInstance*> StoredToSort;

		for (UTcsStateInstance* State : Slot.States)
		{
			if (!IsValid(State))
			{
				continue;
			}

			switch (State->GetCurrentStage())
			{
			case ETcsStateStage::SS_Active:
				ActiveToSort.Add(State);
				ActiveCount++;
				break;
			case ETcsStateStage::SS_HangUp:
				HangUpToSort.Add(State);
				HangUpCount++;
				break;
			case ETcsStateStage::SS_Pause:
				PauseToSort.Add(State);
				PauseCount++;
				break;
			case ETcsStateStage::SS_Inactive:
				StoredToSort.Add(State);
				InactiveCount++;
				break;
			default:
				StoredToSort.Add(State);
				break;
			}
		}

		SortStates(ActiveToSort);
		SortStates(HangUpToSort);
		SortStates(PauseToSort);
		SortStates(StoredToSort);

		for (UTcsStateInstance* State : ActiveToSort)
		{
			ActiveStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : HangUpToSort)
		{
			HangUpStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : PauseToSort)
		{
			PauseStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : StoredToSort)
		{
			if (!IsValid(State))
			{
				continue;
			}

			const FString StateDesc = FormatState(State);
			if (State->GetCurrentStage() == ETcsStateStage::SS_Inactive)
			{
				StoredStates.Add(StateDesc);
			}
			else
			{
				StoredStates.Add(FString::Printf(TEXT("%s(%s)"),
					*StateDesc,
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage()))));
			}
		}

		Line += FString::Printf(TEXT(" N=%d(A=%d,H=%d,P=%d,I=%d)"),
			Slot.States.Num(),
			ActiveCount,
			HangUpCount,
			PauseCount,
			InactiveCount);

		auto AppendList = [&Line](const TCHAR* Label, const TArray<FString>& Names)
		{
			if (Names.Num() > 0)
			{
				Line += FString::Printf(TEXT(" %s={%s}"), Label, *FString::Join(Names, TEXT(", ")));
			}
		};

		AppendList(TEXT("Active"), ActiveStates);
		AppendList(TEXT("HangUp"), HangUpStates);
		AppendList(TEXT("Pause"), PauseStates);
		AppendList(TEXT("Stored"), StoredStates);

		return Line;
	};

	if (SlotFilter.IsValid())
	{
		if (const FTcsStateSlot* Slot = StateSlotsX.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	TArray<FGameplayTag> SlotTags;
	StateSlotsX.GetKeys(SlotTags);
	SlotTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& Tag : SlotTags)
	{
		const FTcsStateSlot& Slot = StateSlotsX[Tag];
		if (!Accumulator.IsEmpty())
		{
			Accumulator += TEXT("\n");
		}
		Accumulator += BuildLine(Tag, Slot);
	}

	if (Accumulator.IsEmpty())
	{
		Accumulator = TEXT("<no slots>");
	}

	return Accumulator;
}

FString UTcsStateComponent::GetStateDebugSnapshot(FName StateDefIdFilter) const
{
	auto FormatStateLine = [this](const UTcsStateInstance* State) -> FString
	{
		if (!IsValid(State))
		{
			return TEXT("<invalid>");
		}

		const FGameplayTag SlotTag = State->GetStateDef().StateSlotType;
		const FTcsStateSlot* Slot = SlotTag.IsValid() ? StateSlotsX.Find(SlotTag) : nullptr;
		const bool bGateOpen = SlotTag.IsValid() ? (Slot && Slot->bIsGateOpen) : true;

		const ETcsStateTreeTickPolicy TickPolicy = State->GetStateDef().TickPolicy;
		const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));

		const ETcsStateDurationType DurType = State->GetStateDef().DurationType;
		const float DurRemaining = State->GetDurationRemaining();
		const FString DurStr = (DurType == ETcsStateDurationType::SDT_Infinite || DurRemaining < 0.0f)
			? TEXT("Inf")
			: FString::Printf(TEXT("%.2f"), DurRemaining);

		FString RemovalStr = TEXT("-");
		if (State->HasPendingRemovalRequest())
		{
			RemovalStr = State->GetPendingRemovalRequest().ToRemovalReasonName().ToString();
		}

		const AActor* OwnerActor = State->GetOwner();
		const AActor* Instigator = State->GetInstigator();

		return FString::Printf(TEXT("State=%s Id=%d Slot=%s Gate=%s Stage=%s P=%d Lv=%d Stack=%d Dur=%s Tick=%s Rem=%s Owner=%s Inst=%s"),
			*State->GetStateDefId().ToString(),
			State->GetInstanceId(),
			*SlotTag.ToString(),
			bGateOpen ? TEXT("Open") : TEXT("Closed"),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())),
			State->GetStateDef().Priority,
			State->GetLevel(),
			State->GetStackCount(),
			*DurStr,
			*TickPolicyStr,
			*RemovalStr,
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
			Instigator ? *Instigator->GetName() : TEXT("None"));
	};

	TArray<UTcsStateInstance*> Instances = StateInstanceIndex.Instances;
	Instances.RemoveAll([StateDefIdFilter](const UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}
		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			return true;
		}
		if (!StateDefIdFilter.IsNone() && State->GetStateDefId() != StateDefIdFilter)
		{
			return true;
		}
		return false;
	});

	Instances.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const FString AS = A.GetStateDef().StateSlotType.ToString();
		const FString BS = B.GetStateDef().StateSlotType.ToString();
		if (AS != BS)
		{
			return AS < BS;
		}

		const int32 AP = A.GetStateDef().Priority;
		const int32 BP = B.GetStateDef().Priority;
		if (AP != BP)
		{
			return AP > BP;
		}

		return A.GetInstanceId() < B.GetInstanceId();
	});

	FString Accumulator = FString::Printf(TEXT("Total=%d Filter=%s"), Instances.Num(), *StateDefIdFilter.ToString());
	for (const UTcsStateInstance* State : Instances)
	{
		Accumulator += TEXT("\n");
		Accumulator += FormatStateLine(State);
	}

	return Accumulator;
}

void UTcsStateComponent::SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen)
{
    FTcsStateSlot* Slot = StateSlotsX.Find(SlotTag);
    if (!Slot)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlot %s"),
        	*FString(__FUNCTION__),
        	*SlotTag.ToString());
        return;
    }

	if (Slot->bIsGateOpen != bOpen)
	{
		Slot->bIsGateOpen = bOpen;
		UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));

		// 广播槽位Gate状态变化事件
		NotifySlotGateStateChanged(SlotTag, bOpen);

		// 直接调用 Subsystem 更新槽位激活状态
		if (IsValid(StateMgr))
		{
			StateMgr->UpdateStateSlotActivation(this, SlotTag);
		}
	}
}

bool UTcsStateComponent::IsSlotGateOpen(FGameplayTag SlotTag) const
{
    if (const FTcsStateSlot* Slot = StateSlotsX.Find(SlotTag))
    {
        return Slot->bIsGateOpen;
    }
    return false;
}

TArray<FName> UTcsStateComponent::GetCurrentActiveStateTreeStates() const
{
	TArray<FName> ActiveStateNames;

	if (!StateTreeRef.IsValid())
	{
		return ActiveStateNames;
	}

	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		return ActiveStateNames;
	}

	// StateTree API 目前只提供 const 访问接口，这里通过 const_cast 获取可写指针以创建 ExecutionContext。
	UTcsStateComponent* MutableThis = const_cast<UTcsStateComponent*>(this);
	FStateTreeInstanceData* MutableInstanceData = &MutableThis->InstanceData;
	FStateTreeExecutionContext Context(*MutableThis, *StateTree, *MutableInstanceData);
	ActiveStateNames = Context.GetActiveStateNames();

	// 如果StateTree没有激活状态，则返回缓存，避免外部逻辑误判为发生变化。
	if (ActiveStateNames.IsEmpty() && !CachedActiveStateNames.IsEmpty())
	{
		ActiveStateNames = CachedActiveStateNames;
	}

	return ActiveStateNames;
}

void UTcsStateComponent::OnStateTreeStateChanged(const FStateTreeExecutionContext& Context)
{
	// 【关键API】从ExecutionContext获取当前激活状态
	TArray<FName> CurrentActiveStates = Context.GetActiveStateNames();

	// 检测变化
	if (!AreStateNamesEqual(CurrentActiveStates, CachedActiveStateNames))
	{
		// 委托给 Subsystem 处理槽位 Gate 刷新
		if (IsValid(StateMgr))
		{
			StateMgr->RefreshSlotsForStateChange(this, CurrentActiveStates, CachedActiveStateNames);
		}
		CachedActiveStateNames = CurrentActiveStates;

		UE_LOG(LogTcsState, Log,
			   TEXT("[StateTree Event] State changed: %d active states"),
			   CurrentActiveStates.Num());
	}
}

bool UTcsStateComponent::AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	// Treat arrays as sets to avoid false positives due to unstable ordering from StateTree.
	TArray<FName> SortedA = A;
	TArray<FName> SortedB = B;
	SortedA.Sort([](const FName& L, const FName& R) { return L.LexicalLess(R); });
	SortedB.Sort([](const FName& L, const FName& R) { return L.LexicalLess(R); });

	for (int32 i = 0; i < SortedA.Num(); ++i)
	{
		if (SortedA[i] != SortedB[i])
		{
			return false;
		}
	}

	return true;
}
