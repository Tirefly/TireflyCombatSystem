// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateComponent.h"

#include "Attribute/TcsAttributeComponent.h"
#include "TcsLogChannels.h"
#include "GameFramework/Actor.h"
#include "State/TcsStateDefinition.h"



bool UTcsStateComponent::RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
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

bool UTcsStateComponent::RemoveState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance %s does not belong to component %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString(),
			*GetPathName());
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return false;
	}

	const FGameplayTag SlotTag = StateDef->StateSlotType;
	if (!RuntimeStateSlots.Contains(SlotTag))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
			*FString(__FUNCTION__),
			*SlotTag.ToString());
		return false;
	}

	return RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Removed);
}

int32 UTcsStateComponent::RemoveStatesByDefId(FName StateDefId, bool bRemoveAll)
{
	if (StateDefId.IsNone())
	{
		return 0;
	}

	// 先通过定义名索引直接命中候选实例，避免为了移除同一种状态而扫描所有槽位。
	TArray<UTcsStateInstance*> IndexedStates;
	if (!StateInstanceIndex.GetInstancesByName(StateDefId, IndexedStates))
	{
		return 0;
	}

	// 再按槽位分组，保持“同槽位状态集中回收”的执行节奏，
	// 同时配合批处理，把每个受影响槽位压成一次最终刷新。
	TMap<FGameplayTag, TArray<UTcsStateInstance*>> StatesBySlot;
	TArray<FGameplayTag> SlotOrder;
	for (UTcsStateInstance* State : IndexedStates)
	{
		if (!IsValid(State))
		{
			continue;
		}

		const UTcsStateDefinition* StateDef = State->GetStateDef();
		if (!StateDef || !StateDef->StateSlotType.IsValid())
		{
			continue;
		}

		TArray<UTcsStateInstance*>& SlotStates = StatesBySlot.FindOrAdd(StateDef->StateSlotType);
		if (SlotStates.IsEmpty())
		{
			SlotOrder.Add(StateDef->StateSlotType);
		}

		SlotStates.Add(State);
		if (!bRemoveAll)
		{
			break;
		}
	}

	int32 RemovedCount = 0;
	BeginStateSlotActivationBatch();
	for (const FGameplayTag& SlotTag : SlotOrder)
	{
		const TArray<UTcsStateInstance*>* StatesToRemove = StatesBySlot.Find(SlotTag);
		if (!StatesToRemove)
		{
			continue;
		}

		for (UTcsStateInstance* State : *StatesToRemove)
		{
			if (RequestStateRemoval(State, TcsStateRemovalReasons::Removed))
			{
				RemovedCount++;
			}
		}

		if (!bRemoveAll && RemovedCount > 0)
		{
			break;
		}
	}

	EndStateSlotActivationBatch();

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStatesInSlot(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return 0;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(SlotTag);
	if (!StateSlot)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	const TArray<UTcsStateInstance*> StatesToRemove = StateSlot->States;
	BeginStateSlotActivationBatch();

	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (IsValid(State))
		{
			RequestStateRemoval(State, TcsStateRemovalReasons::Removed);
			RemovedCount++;
		}
	}

	EndStateSlotActivationBatch();

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStates()
{
	ensureMsgf(!IsInStateTreeUpdateContext(), TEXT("[%s] RemoveAllStates called during StateTree update on %s. Prefer frame-boundary reclaim to avoid overlapping callback teardown."),
		*FString(__FUNCTION__),
		*GetPathName());

	TArray<UTcsStateInstance*> StatesToRemove;
	for (auto& Pair : RuntimeStateSlots)
	{
		for (UTcsStateInstance* State : Pair.Value.States)
		{
			if (IsValid(State))
			{
				StatesToRemove.Add(State);
			}
		}
	}

	int32 TotalRemoved = 0;
	BeginStateSlotActivationBatch();
	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (RequestStateRemoval(State, TcsStateRemovalReasons::Removed))
		{
			TotalRemoved++;
		}
	}
	EndStateSlotActivationBatch();

	return TotalRemoved;
}

void UTcsStateComponent::ActivateState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Activating state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Active))
	{
		return;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	switch (StateDef->TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->RestartStateTree();
		{
			TGuardValue<bool> StateTreeTickGuard(bIsInStateTreeCallback, true);
			StateInstance->TickStateTree(0.f);
		}
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->RestartStateTree();
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->RestartStateTree();
		if (StateInstance->IsStateTreeRunning())
		{
			StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
}

void UTcsStateComponent::DeactivateState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
	if (PreviousStage == ETcsStateStage::SS_Inactive)
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deactivating state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Inactive))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Inactive);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Inactive, FName(TEXT("Deactivated")));
	}
}

void UTcsStateComponent::HangUpState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Hanging up state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_HangUp);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_HangUp, FName(TEXT("HangUp")));
	}
}

void UTcsStateComponent::ResumeState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Resuming state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Active))
	{
		return;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	switch (StateDef->TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->ResumeStateTree();
		{
			TGuardValue<bool> StateTreeTickGuard(bIsInStateTreeCallback, true);
			StateInstance->TickStateTree(0.f);
		}
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->ResumeStateTree();
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->ResumeStateTree();
		if (StateInstance->IsStateTreeRunning())
		{
			StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
}

void UTcsStateComponent::PauseState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Pausing state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Pause);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Pause, FName(TEXT("Pause")));
	}
}

void UTcsStateComponent::CancelState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Cancelled);
}

void UTcsStateComponent::ExpireState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Expired);
}

bool UTcsStateComponent::IsStateStillValid(UTcsStateInstance* StateInstance) const
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		return false;
	}

	const FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateDef->StateSlotType);
	return StateSlot && StateSlot->States.Contains(StateInstance);
}

void UTcsStateComponent::FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	const FGameplayTag SlotTag = StateDef->StateSlotType;
	UE_LOG(LogTcsState, Verbose, TEXT("[%s] FinalizeRemoval: State=%s Id=%d Reason=%s Owner=%s Slot=%s Stage=%s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString(),
		StateInstance->GetInstanceId(),
		*RemovalReason.ToString(),
		OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
		*SlotTag.ToString(),
		*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(StateInstance->GetCurrentStage())));

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	if (StateInstance->IsStateTreeRunning())
	{
		StateInstance->StopStateTree();
	}

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Expired))
	{
		return;
	}

	StateTreeTickScheduler.Remove(StateInstance);
	StateInstanceIndex.RemoveInstance(StateInstance);

	if (StateInstance->GetSourceHandle().IsValid())
	{
		if (UTcsAttributeComponent* OwnerAttrComp = StateInstance->GetOwnerAttributeComponent())
		{
			OwnerAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
		}
		else if (AActor* MutableOwnerActor = GetOwner())
		{
			if (UTcsAttributeComponent* FallbackAttrComp = MutableOwnerActor->FindComponentByClass<UTcsAttributeComponent>())
			{
				FallbackAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
			}
		}
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Expired);
	NotifyStateRemoved(StateInstance, RemovalReason);

	if (SlotTag.IsValid())
	{
		if (FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag))
		{
			Slot->States.Remove(StateInstance);
			Slot->MarkBuffMergeGroupDirty(StateInstance->GetStateDefId(), ETcsBuffMergeDirtyReason::MembershipChanged);
			Slot->MarkBuffMergeRequiresFullRebuild();
			RequestUpdateStateSlotActivation(SlotTag);
		}
	}

	StateInstance->MarkPendingGC();
}

void UTcsStateComponent::NotifyStateStageChanged(UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (!IsValid(StateInstance) || PreviousStage == NewStage)
	{
		return;
	}

	if (InternalStateStageChangedEvent.IsBound())
	{
		InternalStateStageChangedEvent.Broadcast(this, StateInstance, PreviousStage, NewStage);
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

	if (InternalStateRemovedEvent.IsBound())
	{
		InternalStateRemovedEvent.Broadcast(this, StateInstance, RemovalReason);
	}

	if (OnStateRemoved.IsBound())
	{
		OnStateRemoved.Broadcast(this, StateInstance, RemovalReason);
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

void UTcsStateComponent::NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen)
{
	if (!SlotTag.IsValid())
	{
		return;
	}

	if (InternalSlotGateStateChangedEvent.IsBound())
	{
		InternalSlotGateStateChangedEvent.Broadcast(this, SlotTag, bIsOpen);
	}

	if (OnSlotGateStateChanged.IsBound())
	{
		OnSlotGateStateChanged.Broadcast(this, SlotTag, bIsOpen);
	}
}

void UTcsStateComponent::NotifyStateParameterChanged(
	UTcsStateInstance* StateInstance,
	FGameplayTag ParameterTag,
	ETcsStateParameterType ParameterType)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateParameterChanged.IsBound())
	{
		OnStateParameterChanged.Broadcast(StateInstance, ParameterTag, ParameterType);
	}
}

void UTcsStateComponent::NotifyStateApplySuccess(
	AActor* TargetActor,
	FName StateDefId,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag TargetSlot,
	ETcsStateStage AppliedStage)
{
	if (InternalStateApplySuccessEvent.IsBound())
	{
		InternalStateApplySuccessEvent.Broadcast(TargetActor, StateDefId, CreatedStateInstance, TargetSlot, AppliedStage);
	}

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
	if (InternalStateApplyFailedEvent.IsBound())
	{
		InternalStateApplyFailedEvent.Broadcast(TargetActor, StateDefId, FailureReason, FailureMessage);
	}

	if (OnStateApplyFailed.IsBound())
	{
		OnStateApplyFailed.Broadcast(TargetActor, StateDefId, FailureReason, FailureMessage);
	}
}
