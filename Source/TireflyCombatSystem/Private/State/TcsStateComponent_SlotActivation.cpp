// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "GameFramework/Actor.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateSlotDefinition.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "TcsLogChannels.h"


void UTcsStateComponent::RequestUpdateStateSlotActivation(FGameplayTag SlotTag)
{
	// 先做最便宜的输入校验。
	// 这条入口会被 Gate 变化、状态移除、显式刷新请求等多条路径复用，
	// 因此无效槽位要在这里统一拦下，避免把脏请求继续传播到批处理队列。
	if (!SlotTag.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlotTag"), *FString(__FUNCTION__));
		return;
	}

	// 只要当前已经在执行槽位刷新，或者调用方显式进入了批处理作用域，
	// 本次请求就不能立刻重跑 `UpdateStateSlotActivation()`：
	// 1. 正在刷新时立刻重入，会把同一轮收敛拆成多次嵌套执行；
	// 2. 批处理期间立刻执行，会把“同批次多次语义变化”重新放大成多次完整结算。
	//
	// 因此这里统一改为“按槽位去重入队”。这样同一槽位在同一批次里无论被请求多少次，
	// 最终都只会在批次末或外层刷新结束后结算一次最终结果。
	if (bIsUpdatingSlotActivation || StateSlotActivationBatchDepth > 0)
	{
		PendingSlotActivationUpdates.Add(SlotTag);
		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deferred slot activation update: Component=%s Slot=%s"),
			*FString(__FUNCTION__),
			*GetNameSafe(GetOwner()),
			*SlotTag.ToString());
		return;
	}

	// 只有在“不重入、也不批处理”的普通路径下，才立即同步刷新槽位。
	// 这样单次变更依然保持当前帧、当前调用链内完成收敛，不会被无谓地延迟。
	UpdateStateSlotActivation(SlotTag);
}

void UTcsStateComponent::BeginStateSlotActivationBatch()
{
	++StateSlotActivationBatchDepth;
}

void UTcsStateComponent::EndStateSlotActivationBatch()
{
	check(StateSlotActivationBatchDepth > 0);
	--StateSlotActivationBatchDepth;

	// 如果批处理尾声组件自身或 Owner 已经进入销毁流程，再去排空待处理槽位只会在 teardown 阶段
	// 额外重跑一次收敛逻辑，既没有收益，也更容易撞上对象清理顺序。
	// 这里直接丢弃积压请求，让批处理以“安全结束”为第一目标。
	if (StateSlotActivationBatchDepth == 0 && (IsBeingDestroyed() || !IsValid(GetOwner())))
	{
		PendingSlotActivationUpdates.Empty();
		return;
	}

	// 只有最外层批处理结束、组件仍处于可结算状态、且当前不在刷新重入中时，
	// 才统一排空请求。这样既保留当前帧内完成最终收敛的语义，也避免嵌套批处理提前触发结算。
	if (StateSlotActivationBatchDepth == 0 && !bIsUpdatingSlotActivation && !PendingSlotActivationUpdates.IsEmpty())
	{
		DrainPendingSlotActivationUpdates();
	}
}

void UTcsStateComponent::DrainPendingSlotActivationUpdates()
{
	const int32 MaxIterations = 10;
	int32 Iteration = 0;

	while (!PendingSlotActivationUpdates.IsEmpty() && Iteration < MaxIterations)
	{
		const TSet<FGameplayTag> ToProcess = PendingSlotActivationUpdates;
		PendingSlotActivationUpdates.Empty();

		for (const FGameplayTag& SlotTag : ToProcess)
		{
			if (RuntimeStateSlots.Contains(SlotTag))
			{
				UpdateStateSlotActivation(SlotTag);
			}
		}

		Iteration++;
	}

	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("[%s] Max iterations (%d) reached, possible infinite loop. Remaining pending updates: %d"),
			*FString(__FUNCTION__),
			MaxIterations,
			PendingSlotActivationUpdates.Num());
		PendingSlotActivationUpdates.Empty();
	}
}

void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag StateSlotTag)
{
	if (bIsUpdatingSlotActivation)
	{
		PendingSlotActivationUpdates.Add(StateSlotTag);
		return;
	}

	{
		TGuardValue<bool> Guard(bIsUpdatingSlotActivation, true);

		if (!StateSlotTag.IsValid())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlotTag"), *FString(__FUNCTION__));
		}
		else if (FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateSlotTag))
		{
			ClearStateSlotExpiredStates(StateSlot);
			SortStatesByPriority(StateSlot->States);
			if (PrepareStateSlotActivationEvent.IsBound())
			{
				PrepareStateSlotActivationEvent.Broadcast(this, StateSlotTag, StateSlot);
			}
			EnforceSlotGateConsistency(StateSlotTag);

			if (!StateSlot->bIsGateOpen)
			{
				CleanupInvalidStates(StateSlot);
			}
			else
			{
				ProcessStateSlotByActivationMode(StateSlot, StateSlotTag);
				CleanupInvalidStates(StateSlot);
				OnStateSlotChanged(StateSlotTag);
			}
		}
		else
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
				*FString(__FUNCTION__),
				*StateSlotTag.ToString());
		}
	}

	if (StateSlotActivationBatchDepth == 0)
	{
		DrainPendingSlotActivationUpdates();
	}
}

void UTcsStateComponent::EnforceSlotGateConsistency(FGameplayTag StateSlotTag)
{
	if (!StateSlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateSlotTag);
	if (!StateSlot || StateSlot->bIsGateOpen)
	{
		return;
	}

	const UTcsStateSlotDefinition* SlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(SlotDef))
	{
		return;
	}

	TArray<UTcsStateInstance*> StatesToCancel;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!IsValid(State))
		{
			continue;
		}

		const ETcsStateStage Stage = State->GetCurrentStage();
		if (Stage == ETcsStateStage::SS_Expired)
		{
			continue;
		}

		switch (SlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			if (Stage == ETcsStateStage::SS_Active)
			{
				HangUpState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
			if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
			{
				PauseState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Cancel:
			StatesToCancel.Add(State);
			break;
		}
	}

	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (IsValid(State))
		{
			CancelState(State);
		}
	}

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			HangUpState(State);
		}
	}

#if !UE_BUILD_SHIPPING
	for (const UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State))
		{
			checkf(State->GetCurrentStage() != ETcsStateStage::SS_Active,
				TEXT("[EnforceSlotGateConsistency] Invariant violation: Active state found in closed gate slot. Slot=%s State=%s Stage=%s"),
				*StateSlotTag.ToString(),
				*State->GetStateDefId().ToString(),
				*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())));
		}
	}
#endif
}

void UTcsStateComponent::ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot is null."), *FString(__FUNCTION__));
		return;
	}

	StateSlot->States.RemoveAll([this](UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}

		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			StateTreeTickScheduler.Remove(State);
			StateInstanceIndex.RemoveInstance(State);
			return true;
		}

		return false;
	});
	StateInstanceIndex.RefreshInstances();
}

void UTcsStateComponent::SortStatesByPriority(TArray<UTcsStateInstance*>& States)
{
	// 槽位内没有状态，或只有一个状态时，排序不会改变结果，直接短路即可。
	if (States.Num() < 2)
	{
		return;
	}

	States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const UTcsStateDefinition* AStateDef = A.GetStateDef();
		const UTcsStateDefinition* BStateDef = B.GetStateDef();
		if (!AStateDef || !BStateDef)
		{
			return false;
		}
		return AStateDef->Priority > BStateDef->Priority;
	});
}

void UTcsStateComponent::ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag)
{
	if (!StateSlot)
	{
		return;
	}

	const UTcsStateSlotDefinition* SlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(SlotDef))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDef %s not found"),
			*FString(__FUNCTION__),
			*SlotTag.ToString());
		return;
	}

	switch (SlotDef->ActivationMode)
	{
	case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
		ProcessPriorityOnlyMode(StateSlot, SlotDef);
		break;
	case ETcsStateSlotActivationMode::SSAM_AllActive:
		ProcessAllActiveMode(StateSlot);
		break;
	}
}

void UTcsStateComponent::ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinition* SlotDef)
{
	if (!StateSlot || StateSlot->States.Num() == 0 || !SlotDef)
	{
		return;
	}

	int32 HighestPriority = TNumericLimits<int32>::Lowest();
	for (UTcsStateInstance* Candidate : StateSlot->States)
	{
		if (IsValid(Candidate))
		{
			const UTcsStateDefinition* CandidateStateDef = Candidate->GetStateDef();
			if (CandidateStateDef)
			{
				HighestPriority = FMath::Max(HighestPriority, CandidateStateDef->Priority);
			}
		}
	}

	TArray<UTcsStateInstance*> HighestPriorityStates;
	for (UTcsStateInstance* Candidate : StateSlot->States)
	{
		if (IsValid(Candidate))
		{
			const UTcsStateDefinition* CandidateStateDef = Candidate->GetStateDef();
			if (CandidateStateDef && CandidateStateDef->Priority == HighestPriority)
			{
				HighestPriorityStates.Add(Candidate);
			}
		}
	}

	if (HighestPriorityStates.Num() == 0)
	{
		return;
	}

	if (HighestPriorityStates.Num() > 1 && SlotDef->SamePriorityPolicy)
	{
		const UTcsStateSamePriorityPolicy* Policy = SlotDef->SamePriorityPolicy->GetDefaultObject<UTcsStateSamePriorityPolicy>();
		if (IsValid(Policy))
		{
			HighestPriorityStates.Sort([Policy](const UTcsStateInstance& A, const UTcsStateInstance& B)
			{
				const int64 KeyA = Policy->GetOrderKey(&A);
				const int64 KeyB = Policy->GetOrderKey(&B);
				return KeyA > KeyB;
			});
		}
	}

	UTcsStateInstance* HighestPriorityState = HighestPriorityStates[0];
	if (!IsValid(HighestPriorityState))
	{
		return;
	}

	if (HighestPriorityState->GetCurrentStage() != ETcsStateStage::SS_Active)
	{
		ActivateState(HighestPriorityState);
	}

	TArray<UTcsStateInstance*> StatesToCancel;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!IsValid(State) || State == HighestPriorityState)
		{
			continue;
		}

		if (SlotDef->PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
		{
			StatesToCancel.Add(State);
			continue;
		}

		ApplyPreemptionPolicyToState(State, SlotDef->PreemptionPolicy);
	}

	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (IsValid(State))
		{
			CancelState(State);
		}
	}
}

void UTcsStateComponent::ProcessAllActiveMode(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() != ETcsStateStage::SS_Active)
		{
			ActivateState(State);
		}
	}
}

void UTcsStateComponent::ApplyPreemptionPolicyToState(
	UTcsStateInstance* State,
	ETcsStatePreemptionPolicy Policy)
{
	if (!IsValid(State))
	{
		return;
	}

	switch (Policy)
	{
	case ETcsStatePreemptionPolicy::SPP_HangUpLowerPriority:
		if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			HangUpState(State);
		}
		break;
	case ETcsStatePreemptionPolicy::SPP_PauseLowerPriority:
		if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			PauseState(State);
		}
		break;
	case ETcsStatePreemptionPolicy::SPP_CancelLowerPriority:
		break;
	}
}

void UTcsStateComponent::CleanupInvalidStates(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	StateSlot->States.RemoveAll([](const UTcsStateInstance* State)
	{
		return !IsValid(State);
	});
}

void UTcsStateComponent::RemoveStateFromSlot(
	FTcsStateSlot* StateSlot,
	UTcsStateInstance* State,
	bool bDeactivateIfNeeded)
{
	if (!StateSlot || !IsValid(State))
	{
		return;
	}

	StateSlot->States.Remove(State);
	if (bDeactivateIfNeeded && State->GetCurrentStage() != ETcsStateStage::SS_Inactive)
	{
		DeactivateState(State);
	}
}

void UTcsStateComponent::RequestStateSlotRefresh(FGameplayTag SlotTag)
{
	RequestUpdateStateSlotActivation(SlotTag);
}

void UTcsStateComponent::SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen)
{
	FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag);
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

		// 请求更新槽位激活状态
		RequestUpdateStateSlotActivation(SlotTag);
	}
}

bool UTcsStateComponent::IsSlotGateOpen(FGameplayTag SlotTag) const
{
	if (const FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag))
	{
		return Slot->bIsGateOpen;
	}
	return false;
}