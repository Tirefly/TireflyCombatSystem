// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateComponent.h"

#include "GameFramework/Actor.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateSlotDefinition.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "TcsLogChannels.h"



bool UTcsStateComponent::InitStateSlotMappings()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] OwnerActor is invalid"), *FString(__FUNCTION__));
		return false;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateManagerSubsystem is invalid for %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	const UStateTree* StateTree = GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree is required before State runtime can be prepared: %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	const TValueOrError<void, FString> StateTreeValidation = HasValidStateTreeReference();
	if (StateTreeValidation.HasError())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree reference is invalid for %s: %s"),
			*FString(__FUNCTION__), *GetPathName(), *StateTreeValidation.GetError());
		return false;
	}

	// StateSlot 初始化分两步：
	// 1. 先按当前顶层 StateTree 自动反推出要参与运行时的槽位集合，并重建 RuntimeStateSlots；
	// 2. 再按当前 StateTree 建立可成功绑定的槽位 <-> 状态名关系。
	const bool bRuntimeSlotsReady = RebuildStateSlotRuntimeData();
	if (!bRuntimeSlotsReady)
	{
		Mapping_StateSlotToStateTreeStateName.Empty();
		Mapping_StateTreeStateNameToStateSlotTags.Empty();
		return false;
	}

	const bool bStateTreeBindingsReady = bRuntimeSlotsReady && RebuildStateTreeSlotBindings();

	UE_LOG(LogTcsState, Log,
		TEXT("[%s] Initialized %d state slots and %d StateTree bindings for %s. Ready=%s"),
		*FString(__FUNCTION__),
		RuntimeStateSlots.Num(),
		Mapping_StateSlotToStateTreeStateName.Num(),
		*OwnerActor->GetName(),
		bStateTreeBindingsReady ? TEXT("true") : TEXT("false"));

	return bRuntimeSlotsReady && bStateTreeBindingsReady;
}

bool UTcsStateComponent::RebuildStateSlotRuntimeData()
{
	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateManagerSubsystem is invalid for %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	const UStateTree* StateTree = GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree is required before StateSlot runtime data can be rebuilt for %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	TSet<FName> AvailableStateNames;
	const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
	for (const FCompactStateTreeState& State : States)
	{
		if (!State.Name.IsNone())
		{
			AvailableStateNames.Add(State.Name);
		}
	}

	if (AvailableStateNames.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree %s has no named states for runtime slot derivation on %s"),
			*FString(__FUNCTION__), *GetNameSafe(StateTree), *GetPathName());
		return false;
	}

	// 先搬走旧容器，再按当前顶层 StateTree 自动反推出的槽位子集重新生成 RuntimeStateSlots。
	// 这样可以在定义发生变动时剔除不再被当前树使用的槽位，同时尽量保留仍然存在槽位的运行时状态。
	TMap<FGameplayTag, FTcsStateSlot> ExistingStateSlots = MoveTemp(RuntimeStateSlots);
	RuntimeStateSlots.Empty();
	bool bHasInvalidSlotDefinition = false;
	TSet<FGameplayTag> SeenSlotTags;
	bool bHasMatchedSlotDefinition = false;

	for (const FName& StateSlotDefName : LocalStateMgr->GetAllStateSlotDefNames())
	{
		const UTcsStateSlotDefinition* SlotDefAsset = LocalStateMgr->GetStateSlotDefinition(StateSlotDefName);
		if (!SlotDefAsset)
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDefinition %s could not be loaded for %s"),
				*FString(__FUNCTION__), *StateSlotDefName.ToString(), *GetPathName());
			continue;
		}

		const FName& StateTreeStateName = SlotDefAsset->StateTreeStateName;
		if (!AvailableStateNames.Contains(StateTreeStateName))
		{
			continue;
		}

		bHasMatchedSlotDefinition = true;

		if (!SlotDefAsset->SlotTag.IsValid())
		{
			bHasInvalidSlotDefinition = true;
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Matched StateSlotDefinition %s has invalid SlotTag on %s"),
				*FString(__FUNCTION__), *StateSlotDefName.ToString(), *GetPathName());
			continue;
		}

		if (SeenSlotTags.Contains(SlotDefAsset->SlotTag))
		{
			bHasInvalidSlotDefinition = true;
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Duplicate matched StateSlot tag %s on %s"),
				*FString(__FUNCTION__), *SlotDefAsset->SlotTag.ToString(), *GetPathName());
			continue;
		}
		SeenSlotTags.Add(SlotDefAsset->SlotTag);

		// 默认先创建一个空槽位；如果旧容器里有同名槽位，则把原有运行时数据迁移过来。
		// 这里迁移的是 FTcsStateSlot 本身，因此其中的 States 和 Gate 状态都会被保留。
		FTcsStateSlot RuntimeSlot;
		if (FTcsStateSlot* ExistingSlot = ExistingStateSlots.Find(SlotDefAsset->SlotTag))
		{
			RuntimeSlot = MoveTemp(*ExistingSlot);
		}

		RuntimeSlot.CacheStateSlotDef(SlotDefAsset);
		RuntimeSlot.MarkBuffMergeRequiresFullRebuild();
		RuntimeSlot.MarkAllBuffMergeGroupsDirty(ETcsBuffMergeDirtyReason::ForceRebuild);

		RuntimeStateSlots.Add(SlotDefAsset->SlotTag, MoveTemp(RuntimeSlot));
	}

	if (!bHasMatchedSlotDefinition)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree %s could not derive any runtime StateSlots for %s"),
			*FString(__FUNCTION__), *GetNameSafe(StateTree), *GetPathName());
		RuntimeStateSlots.Empty();
		return false;
	}

	if (RuntimeStateSlots.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] No valid StateSlot runtime data was built for %s"),
			*FString(__FUNCTION__), *GetPathName());
		RuntimeStateSlots.Empty();
		return false;
	}

	if (bHasInvalidSlotDefinition)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot runtime data build failed because one or more definitions are invalid for %s"),
			*FString(__FUNCTION__), *GetPathName());
		RuntimeStateSlots.Empty();
		return false;
	}

	return true;
}

bool UTcsStateComponent::RebuildStateTreeSlotBindings()
{
	// 绑定表只负责 StateSlot 与 StateTree 状态名的桥接，不持有任何槽位运行时数据，
	// 因此每次都从当前 StateTree 重新扫描并完整重建。
	Mapping_StateSlotToStateTreeStateName.Empty();
	Mapping_StateTreeStateNameToStateSlotTags.Empty();

	const UStateTree* StateTree = GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree is required before StateSlotMapping can be built for %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	// 先把当前 StateTree 中真实存在的状态名收集出来，后面只接受能在树里找到的绑定。
	TSet<FName> AvailableStateNames;
	const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
	for (const FCompactStateTreeState& State : States)
	{
		if (!State.Name.IsNone())
		{
			AvailableStateNames.Add(State.Name);
		}
	}

	if (AvailableStateNames.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree %s has no named states for StateSlotMapping on %s"),
			*FString(__FUNCTION__), *GetNameSafe(StateTree), *GetPathName());
		return false;
	}

	if (RuntimeStateSlots.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] RuntimeStateSlots is empty before StateSlotMapping on %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	AActor* OwnerActor = GetOwner();
	TMap<FGameplayTag, FName> NewMapping_StateSlotToStateTreeStateName;
	TMultiMap<FName, FGameplayTag> NewMapping_StateTreeStateNameToStateSlotTags;
	bool bAllBindingsValid = true;
	for (const TPair<FGameplayTag, FTcsStateSlot>& Pair : RuntimeStateSlots)
	{
		const FGameplayTag StateSlotTag = Pair.Key;
		const FTcsStateSlot& RuntimeSlot = Pair.Value;
		const UTcsStateSlotDefinition* StateSlotDef = RuntimeSlot.GetStateSlotDef();
		if (!IsValid(StateSlotDef) || !StateSlotTag.IsValid())
		{
			bAllBindingsValid = false;
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid runtime StateSlot %s on %s"),
				*FString(__FUNCTION__), *StateSlotTag.ToString(), *GetPathName());
			continue;
		}

		const FName& StateTreeStateName = StateSlotDef->StateTreeStateName;
		if (StateTreeStateName.IsNone())
		{
			bAllBindingsValid = false;
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s has no StateTreeStateName on %s"),
				*FString(__FUNCTION__), *StateSlotTag.ToString(), *GetPathName());
			continue;
		}

		// 只有当定义里声明的状态名确实存在于当前 StateTree 中时，
		// 才把这个槽位加入绑定表，避免运行时出现悬空映射。
		const bool bMapped = AvailableStateNames.Contains(StateTreeStateName);
		if (bMapped)
		{
			NewMapping_StateSlotToStateTreeStateName.Add(StateSlotTag, StateTreeStateName);
			NewMapping_StateTreeStateNameToStateSlotTags.Add(StateTreeStateName, StateSlotTag);
		}
		else
		{
			bAllBindingsValid = false;
		}

		UE_LOG(LogTcsState, Log, TEXT("[%s] State Slot [%s] -> StateTree State [%s] %s of %s"),
			*FString(__FUNCTION__),
			*StateSlotTag.ToString(),
			*StateTreeStateName.ToString(),
			bMapped ? TEXT("mapped") : TEXT("not found"),
			*GetNameSafe(OwnerActor));
	}

	if (NewMapping_StateSlotToStateTreeStateName.IsEmpty())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] No StateSlotMapping was built for %s"),
			*FString(__FUNCTION__), *GetPathName());
		Mapping_StateSlotToStateTreeStateName.Empty();
		Mapping_StateTreeStateNameToStateSlotTags.Empty();
		return false;
	}

	if (!bAllBindingsValid)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotMapping build failed because one or more slots could not bind to StateTree %s on %s"),
			*FString(__FUNCTION__), *GetNameSafe(StateTree), *GetPathName());
		Mapping_StateSlotToStateTreeStateName.Empty();
		Mapping_StateTreeStateNameToStateSlotTags.Empty();
		return false;
	}

	Mapping_StateSlotToStateTreeStateName = MoveTemp(NewMapping_StateSlotToStateTreeStateName);
	Mapping_StateTreeStateNameToStateSlotTags = MoveTemp(NewMapping_StateTreeStateNameToStateSlotTags);
	return true;
}

void UTcsStateComponent::DiffStateTreeStateNames(
	const TArray<FName>& NewStates,
	const TArray<FName>& OldStates,
	TSet<FName>& AddedStates,
	TSet<FName>& RemovedStates) const
{
	TSet<FName> NewStateSet(NewStates);
	NewStateSet.Remove(NAME_None);
	TSet<FName> OldStateSet(OldStates);
	OldStateSet.Remove(NAME_None);

	AddedStates = NewStateSet;
	for (const FName OldState : OldStateSet)
	{
		AddedStates.Remove(OldState);
	}

	RemovedStates = OldStateSet;
	for (const FName NewState : NewStateSet)
	{
		RemovedStates.Remove(NewState);
	}
}

void UTcsStateComponent::CollectAffectedSlotTagsForStateChanges(
	const TSet<FName>& AddedStates,
	const TSet<FName>& RemovedStates,
	TSet<FGameplayTag>& OutAffectedSlotTags) const
{
	OutAffectedSlotTags.Reset();

	auto CollectFromReverseBindings = [this, &OutAffectedSlotTags](const TSet<FName>& StateNames)
	{
		TArray<FGameplayTag> SlotTags;
		for (const FName StateName : StateNames)
		{
			SlotTags.Reset();
			Mapping_StateTreeStateNameToStateSlotTags.MultiFind(StateName, SlotTags);
			for (const FGameplayTag SlotTag : SlotTags)
			{
				if (SlotTag.IsValid())
				{
					OutAffectedSlotTags.Add(SlotTag);
				}
			}
		}
	};

	if (!Mapping_StateTreeStateNameToStateSlotTags.IsEmpty())
	{
		CollectFromReverseBindings(AddedStates);
		CollectFromReverseBindings(RemovedStates);
		return;
	}

	// 反向缓存不可用时，保守回退到正向绑定表扫描，保持语义正确。
	for (const TPair<FGameplayTag, FName>& Pair : Mapping_StateSlotToStateTreeStateName)
	{
		if (AddedStates.Contains(Pair.Value) || RemovedStates.Contains(Pair.Value))
		{
			OutAffectedSlotTags.Add(Pair.Key);
		}
	}
}

bool UTcsStateComponent::TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef"), *FString(__FUNCTION__));
		return false;
	}

	if (!StateDef->StateSlotType.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateDef %s does not specify StateSlotType."),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("StateDef does not specify a valid StateSlotType."));
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance owner component mismatch. State=%s Component=%s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString(),
			*GetPathName());
		return false;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateDef->StateSlotType);
	if (!StateSlot)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlot %s not found in owner StateComponent."),
			*FString(__FUNCTION__),
			*StateDef->StateSlotType.ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlot,
			FString::Printf(TEXT("StateSlot %s not found."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	const UTcsStateSlotDefinition* StateSlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(StateSlotDef))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDef %s not found."),
			*FString(__FUNCTION__),
			*StateDef->StateSlotType.ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlotDefinition,
			FString::Printf(TEXT("StateSlotDef %s not found."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	ClearStateSlotExpiredStates(StateSlot);

	if (StateSlot->States.Contains(StateInstance))
	{
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::AlreadyInSlot,
			TEXT("StateInstance already exists in target slot."));
		return false;
	}

	if (!StateSlot->bIsGateOpen && StateSlotDef->GateCloseBehavior == ETcsStateSlotGateClosePolicy::SSGCP_Cancel)
	{
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::SlotGateClosed_CancelPolicy,
			FString::Printf(TEXT("StateSlot %s gate is closed (Cancel policy)."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	if (StateSlot->bIsGateOpen
		&& StateSlotDef->ActivationMode == ETcsStateSlotActivationMode::SSAM_PriorityOnly
		&& StateSlotDef->PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
	{
		bool bHasExisting = false;
		int32 BestPriority = TNumericLimits<int32>::Lowest();
		for (const UTcsStateInstance* Existing : StateSlot->States)
		{
			if (!IsValid(Existing) || Existing->GetCurrentStage() == ETcsStateStage::SS_Expired)
			{
				continue;
			}

			bHasExisting = true;
			const UTcsStateDefinition* ExistingStateDef = Existing->GetStateDef();
			if (ExistingStateDef)
			{
				BestPriority = FMath::Max(BestPriority, ExistingStateDef->Priority);
			}
		}

		if (bHasExisting && StateDef->Priority < BestPriority)
		{
			NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::LowerPriorityRejected,
				TEXT("State application rejected: lower priority than existing state in PriorityOnly slot."));
			return false;
		}
	}

	StateSlot->States.Add(StateInstance);
	StateSlot->MarkBuffMergeGroupDirty(StateInstance->GetStateDefId(), ETcsBuffMergeDirtyReason::MembershipChanged);
	StateSlot->MarkBuffMergeRequiresFullRebuild();

	if (!StateSlot->bIsGateOpen)
	{
		const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
		bool bStageChanged = false;
		switch (StateSlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			bStageChanged = StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp);
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
		default:
			bStageChanged = StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause);
			break;
		}

		if (bStageChanged)
		{
			NotifyStateStageChanged(StateInstance, PreviousStage, StateInstance->GetCurrentStage());
		}
	}

	RequestUpdateStateSlotActivation(StateDef->StateSlotType);

	if (IsStateStillValid(StateInstance))
	{
		StateInstanceIndex.AddInstance(StateInstance);
		NotifyStateApplySuccess(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			StateInstance,
			StateDef->StateSlotType,
			StateInstance->GetCurrentStage());
	}
	else
	{
		UE_LOG(LogTcsState, Verbose,
			TEXT("[%s] State '%s' was merged and removed, skipping ApplySuccess notification"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
	}

	return true;
}

void UTcsStateComponent::RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates)
{
	TSet<FName> AddedStates;
	TSet<FName> RemovedStates;
	DiffStateTreeStateNames(NewStates, OldStates, AddedStates, RemovedStates);
	if (AddedStates.IsEmpty() && RemovedStates.IsEmpty())
	{
		return;
	}

	TSet<FName> NewStateSet(NewStates);
	NewStateSet.Remove(NAME_None);

	TSet<FGameplayTag> AffectedSlotTags;
	CollectAffectedSlotTagsForStateChanges(AddedStates, RemovedStates, AffectedSlotTags);
	if (AffectedSlotTags.IsEmpty())
	{
		return;
	}

	BeginStateSlotActivationBatch();
	for (const FGameplayTag SlotTag : AffectedSlotTags)
	{
		const FName* MappedStateName = Mapping_StateSlotToStateTreeStateName.Find(SlotTag);
		if (!MappedStateName)
		{
			continue;
		}

		const bool bShouldOpen = NewStateSet.Contains(*MappedStateName);

		const bool bWasOpen = IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			SetSlotGateOpen(SlotTag, bShouldOpen);
			UE_LOG(LogTcsState, Log,
				TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				*SlotTag.ToString(),
				bShouldOpen ? TEXT("opened") : TEXT("closed"),
				*MappedStateName->ToString());
		}
	}
	EndStateSlotActivationBatch();
}
