// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "GameFramework/Actor.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"

void FTcsBuffDurationTracker::Add(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	TrackedInstances.Add(BuffInstance);
}

void FTcsBuffDurationTracker::Remove(UTcsBuffInstance* BuffInstance)
{
	if (!BuffInstance)
	{
		return;
	}

	TrackedInstances.Remove(BuffInstance);
}

void FTcsBuffDurationTracker::RefreshInstances()
{
	TArray<TObjectPtr<UTcsBuffInstance>> InvalidBuffs;
	for (const TObjectPtr<UTcsBuffInstance>& BuffInstance : TrackedInstances)
	{
		if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidBuffs.Add(BuffInstance);
		}
	}

	for (const TObjectPtr<UTcsBuffInstance>& Buff : InvalidBuffs)
	{
		TrackedInstances.Remove(Buff);
	}
}

UTcsBuffComponent::UTcsBuffComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTcsBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwnerStateComponent();
}

void UTcsBuffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTcsStateComponent* StateComponent = OwnerStateComponent.Get())
	{
		UnbindOwnerStateEvents(StateComponent);
	}

	Super::EndPlay(EndPlayReason);
}

void UTcsBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickBuffLifecycles(DeltaTime);
}

UTcsBuffComponent* UTcsBuffComponent::GetOrCreateForActor(AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	UTcsBuffComponent* BuffComponent = OwnerActor->FindComponentByClass<UTcsBuffComponent>();
	if (!BuffComponent)
	{
		BuffComponent = NewObject<UTcsBuffComponent>(OwnerActor, TEXT("TcsBuffComponent"));
		if (BuffComponent)
		{
			OwnerActor->AddOwnedComponent(BuffComponent);
			BuffComponent->RegisterComponent();
		}
	}

	if (BuffComponent)
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			BuffComponent->InitializeOwnerStateComponent(StateComponent);
		}
	}

	return BuffComponent;
}

UTcsStateComponent* UTcsBuffComponent::GetOwnerStateComponent() const
{
	return ResolveOwnerStateComponent();
}

void UTcsBuffComponent::InitializeOwnerStateComponent(UTcsStateComponent* InStateComponent)
{
	UTcsStateComponent* PreviousStateComponent = OwnerStateComponent.Get();
	if (IsValid(PreviousStateComponent) && PreviousStateComponent != InStateComponent)
	{
		UnbindOwnerStateEvents(PreviousStateComponent);
	}

	OwnerStateComponent = InStateComponent;
	BindOwnerStateEvents(InStateComponent);
}

float UTcsBuffComponent::GetBuffRemainingDuration(const UTcsBuffInstance* BuffInstance) const
{
	if (IsValid(BuffInstance) && BuffInstance->HasInfiniteDuration())
	{
		return -1.0f;
	}

	if (IsValid(BuffInstance) && BuffInstance->HasFiniteDuration())
	{
		return BuffInstance->RemainingDuration;
	}

	return 0.0f;
}

void UTcsBuffComponent::RefreshBuffRemainingDuration(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	const float NewRemaining = BuffInstance->GetTotalDuration();
	BuffInstance->RemainingDuration = NewRemaining;
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, NewRemaining);
}

void UTcsBuffComponent::SetBuffRemainingDuration(UTcsBuffInstance* BuffInstance, float InDurationRemaining)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	BuffInstance->RemainingDuration = FMath::Max(0.0f, InDurationRemaining);
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, BuffInstance->RemainingDuration);
}

bool UTcsBuffComponent::GetBuffsInSlot(FGameplayTag SlotTag, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesInSlot;
	if (!StateComponent->GetStatesInSlot(SlotTag, StatesInSlot))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesInSlot)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetBuffsByDefId(FName BuffDefId, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesByDefId;
	if (!StateComponent->GetStatesByDefId(BuffDefId, StatesByDefId))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesByDefId)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetAllActiveBuffs(TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> ActiveStates;
	if (!StateComponent->GetAllActiveStates(ActiveStates))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : ActiveStates)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::HasBuffWithDefId(FName BuffDefId) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsByDefId(BuffDefId, Buffs);
}

bool UTcsBuffComponent::HasActiveBuffInSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsInSlot(SlotTag, Buffs);
}

bool UTcsBuffComponent::ApplyBuff(
	FName BuffDefId,
	AActor* Instigator,
	int32 BuffLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	auto ReportApplyFailure = [&](ETcsStateApplyFailReason FailureReason, const FString& FailureMessage)
	{
		if (IsValid(StateComponent))
		{
			if (AActor* OwnerActor = StateComponent->GetOwner())
			{
				StateComponent->NotifyStateApplyFailed(OwnerActor, BuffDefId, FailureReason, FailureMessage);
			}
		}

		return false;
	};

	if (!IsValid(StateComponent))
	{
		return false;
	}

	if (BuffDefId.IsNone())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("BuffDefId is None."));
	}

	if (!IsValid(Instigator))
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Instigator is invalid."));
	}

	UTcsStateManagerSubsystem* StateManager = StateComponent->GetStateManager();
	if (!StateManager)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve StateManagerSubsystem."));
	}

	const UTcsStateDefinition* StateDef = StateManager->GetStateDefinition(BuffDefId);
	if (!StateDef)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Invalid buff definition."));
	}

	if (!StateDef->IsA<UTcsBuffDefinition>())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State definition %s is not a BuffDefinition."),
			*FString(__FUNCTION__),
			*BuffDefId.ToString());

		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Target definition is not a BuffDefinition."));
	}

	return StateComponent->TryApplyState(BuffDefId, Instigator, BuffLevel, ParentSourceHandle);
}

bool UTcsBuffComponent::RemoveBuff(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	if (!IsValid(BuffInstance))
	{
		return false;
	}

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	if (RemovalReason.IsNone())
	{
		RemovalReason = TcsStateRemovalReasons::Removed;
	}

	return StateComponent->RequestStateRemoval(BuffInstance, RemovalReason);
}

void UTcsBuffComponent::RegisterBuffInstance(UTcsStateInstance* StateInstance)
{
	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	if (BuffInstance->GetDurationType() == ETcsBuffDurationType::SDT_Duration)
	{
		DurationTracker.Add(BuffInstance);
	}
}

void UTcsBuffComponent::UnregisterBuffInstance(UTcsStateInstance* StateInstance)
{
	if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
	{
		DurationTracker.Remove(BuffInstance);
	}
}

void UTcsBuffComponent::RemoveBuffInstance(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (UTcsStateComponent* StateComponent = ResolveOwnerStateComponent())
	{
		StateComponent->RequestStateRemoval(StateInstance, RemovalReason);
	}
}

void UTcsBuffComponent::ExpireBuffInstance(UTcsStateInstance* StateInstance)
{
	RemoveBuffInstance(StateInstance, TcsBuffRemovalReasons::Expired);
}

void UTcsBuffComponent::TickBuffLifecycles(float DeltaTime)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return;
	}

	TArray<UTcsStateInstance*> ExpiredStates;
	TArray<UTcsStateInstance*> InvalidStates;

	for (const TObjectPtr<UTcsBuffInstance>& TrackedBuffInstance : DurationTracker.TrackedInstances)
	{
		UTcsBuffInstance* BuffInstance = TrackedBuffInstance.Get();
		UTcsStateInstance* StateInstance = BuffInstance;

		if (!IsValid(StateInstance))
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();
		if (CurrentStage == ETcsStateStage::SS_Expired)
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		if (CurrentStage != ETcsStateStage::SS_Active && CurrentStage != ETcsStateStage::SS_HangUp)
		{
			continue;
		}

		if (BuffInstance->RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
			continue;
		}

		BuffInstance->RemainingDuration = FMath::Max(0.0f, BuffInstance->RemainingDuration - DeltaTime);
		if (BuffInstance->RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
		}
	}

	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (UTcsBuffInstance* ExpiredBuff = ResolveBuffInstance(ExpiredState))
		{
			HandleBuffDurationExpired(ExpiredBuff);
			if (StateComponent->IsBeingDestroyed() || !IsValid(StateComponent->GetOwner()))
			{
				return;
			}
		}
		else
		{
			InvalidStates.Add(ExpiredState);
		}
	}

	for (UTcsStateInstance* InvalidState : InvalidStates)
	{
		UnregisterBuffInstance(InvalidState);
	}
}

void UTcsBuffComponent::HandleBuffStackCountChangedInternal(
	UTcsBuffInstance* BuffInstance,
	int32 OldStackCount,
	int32 NewStackCount)
{
	if (!IsValid(BuffInstance) || NewStackCount <= OldStackCount)
	{
		return;
	}

	const UTcsBuffDefinition* BuffDef = BuffInstance->GetBuffDef();
	if (!BuffDef || BuffDef->MaxStackCount <= 1)
	{
		return;
	}

	switch (BuffDef->OnStackIncrease.DurationPolicy)
	{
	case ETcsBuffDurationRefreshPolicy::RefreshRemainingToTotal:
		RefreshBuffRemainingDuration(BuffInstance);
		break;

	case ETcsBuffDurationRefreshPolicy::None:
	default:
		break;
	}
}

void UTcsBuffComponent::HandleBuffDurationExpired(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	const UTcsBuffDefinition* BuffDef = BuffInstance->GetBuffDef();
	const bool bUsesReactiveExpiration = BuffDef
		&& BuffDef->DurationType == ETcsBuffDurationType::SDT_Duration
		&& BuffDef->MaxStackCount > 1;
	const ETcsBuffStackExpirationPolicy ExpirationPolicy = bUsesReactiveExpiration
		? BuffDef->OnDurationExpired.ExpirationPolicy
		: ETcsBuffStackExpirationPolicy::ClearEntireBuff;
	const int32 CurrentStackCount = BuffInstance->GetStackCount();

	if (ExpirationPolicy == ETcsBuffStackExpirationPolicy::ClearEntireBuff || CurrentStackCount <= 1)
	{
		RemoveBuffInstance(BuffInstance, TcsBuffRemovalReasons::Expired);
		return;
	}

	BuffInstance->SetStackCount(CurrentStackCount - 1);
	if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return;
	}

	if (ExpirationPolicy == ETcsBuffStackExpirationPolicy::RemoveSingleStackAndRefreshDuration)
	{
		RefreshBuffRemainingDuration(BuffInstance);
	}
}

void UTcsBuffComponent::RefreshTrackedBuffs()
{
	DurationTracker.RefreshInstances();
}

void UTcsBuffComponent::RebuildBuffMergeGroups(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	TMap<FName, ETcsBuffMergeDirtyReason> ExistingDirtyReasons;
	for (const TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (Pair.Value.HasDirty())
		{
			ExistingDirtyReasons.Add(Pair.Key, Pair.Value.DirtyReasons);
		}
	}

	StateSlot->BuffMergeGroups.Empty();

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(State))
		{
			FTcsBuffMergeGroupRuntime& GroupRuntime = StateSlot->BuffMergeGroups.FindOrAdd(BuffInstance->GetStateDefId());
			GroupRuntime.StateDefId = BuffInstance->GetStateDefId();
			GroupRuntime.AddMember(BuffInstance);
		}
	}

	for (TPair<FName, FTcsBuffMergeGroupRuntime>& Pair : StateSlot->BuffMergeGroups)
	{
		if (const ETcsBuffMergeDirtyReason* ExistingReasons = ExistingDirtyReasons.Find(Pair.Key))
		{
			Pair.Value.MarkDirty(*ExistingReasons);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}

		if (StateSlot->bBuffMergeRequiresFullRebuild)
		{
			Pair.Value.MarkDirty(ETcsBuffMergeDirtyReason::ForceRebuild);
			StateSlot->DirtyBuffMergeStateDefIds.Add(Pair.Key);
		}
	}

	StateSlot->bBuffMergeRequiresFullRebuild = false;
}

bool UTcsBuffComponent::ShouldProcessBuffMergeGroup(
	ETcsBuffMergeDirtyReason DirtyReasons,
	ETcsBuffMergeDependencyFlags DependencyFlags) const
{
	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ForceRebuild))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::MembershipChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::MemberSet))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::RuntimeValueChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::RuntimeStack))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::ExecutionStageChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::ExecutionStage))
	{
		return true;
	}

	if (EnumHasAnyFlags(DirtyReasons, ETcsBuffMergeDirtyReason::SlotGateChanged)
		&& EnumHasAnyFlags(DependencyFlags, ETcsBuffMergeDependencyFlags::SlotGateState))
	{
		return true;
	}

	return false;
}

void UTcsBuffComponent::ProcessBuffMerging(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	if (StateSlot->bBuffMergeRequiresFullRebuild)
	{
		RebuildBuffMergeGroups(StateSlot);
	}

	if (StateSlot->DirtyBuffMergeStateDefIds.IsEmpty())
	{
		return;
	}

	const TSet<FName> DirtyStateDefIds = StateSlot->DirtyBuffMergeStateDefIds;
	StateSlot->DirtyBuffMergeStateDefIds.Empty();

	TArray<UTcsBuffInstance*> AllMergedOutBuffs;
	for (const FName& StateDefId : DirtyStateDefIds)
	{
		FTcsBuffMergeGroupRuntime* GroupRuntime = StateSlot->BuffMergeGroups.Find(StateDefId);
		if (!GroupRuntime)
		{
			continue;
		}

		TArray<UTcsBuffInstance*> GroupMembers;
		GroupRuntime->GatherValidMembers(GroupMembers);
		if (GroupMembers.IsEmpty())
		{
			StateSlot->BuffMergeGroups.Remove(StateDefId);
			continue;
		}

		UTcsBuffInstance* MergeSource = GroupMembers[0];
		TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource ? MergeSource->GetMergerType() : nullptr;
		UTcsBuffMerger* Merger = MergerClass ? MergerClass->GetDefaultObject<UTcsBuffMerger>() : nullptr;
		const ETcsBuffMergeDependencyFlags DependencyFlags = Merger
			? Merger->GetDependencyFlags()
			: ETcsBuffMergeDependencyFlags::None;
		GroupRuntime->DependencyFlags = DependencyFlags;
		const ETcsBuffMergeDirtyReason PendingDirtyReasons = GroupRuntime->DirtyReasons;

		if (!ShouldProcessBuffMergeGroup(PendingDirtyReasons, DependencyFlags))
		{
			GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
			GroupRuntime->ClearDirty(PendingDirtyReasons);
			continue;
		}

		TArray<UTcsBuffInstance*> MergedGroup;
		TArray<UTcsBuffInstance*> MergedOutGroup;
		MergeBuffStateGroup(GroupMembers, MergedGroup, MergedOutGroup);
		GroupRuntime->SetMembers(MergedGroup);
		GroupRuntime->LastProcessedDirtyReasons = PendingDirtyReasons;
		GroupRuntime->ClearDirty(PendingDirtyReasons);
		AllMergedOutBuffs.Append(MergedOutGroup);
	}

	RemoveMergedOutBuffs(StateSlot, AllMergedOutBuffs);
}

void UTcsBuffComponent::MergeBuffStateGroup(
	TArray<UTcsBuffInstance*>& BuffsToMerge,
	TArray<UTcsBuffInstance*>& OutMergedBuffs,
	TArray<UTcsBuffInstance*>& OutMergedOutBuffs)
{
	if (BuffsToMerge.Num() == 0)
	{
		return;
	}

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = StateComponent->GetStateManager();
	if (!LocalStateMgr)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	const UTcsStateDefinition* StateDef = LocalStateMgr->GetStateDefinition(BuffsToMerge[0]->GetStateDefId());
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get state definition for %s"),
			*FString(__FUNCTION__),
			*BuffsToMerge[0]->GetStateDefId().ToString());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffInstance* MergeSource = BuffsToMerge[0];
	if (!MergeSource)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	TSubclassOf<UTcsBuffMerger> MergerClass = MergeSource->GetMergerType();
	if (!MergerClass)
	{
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	UTcsBuffMerger* Merger = MergerClass->GetDefaultObject<UTcsBuffMerger>();
	if (!IsValid(Merger))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get merger instance for %s"),
			*FString(__FUNCTION__),
			*MergerClass->GetName());
		OutMergedBuffs = BuffsToMerge;
		OutMergedOutBuffs.Reset();
		return;
	}

	Merger->Merge(BuffsToMerge, OutMergedBuffs, OutMergedOutBuffs);
}

void UTcsBuffComponent::RemoveMergedOutBuffs(
	FTcsStateSlot* StateSlot,
	const TArray<UTcsBuffInstance*>& MergedOutBuffs)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !StateSlot)
	{
		return;
	}

	for (UTcsBuffInstance* BuffInstance : MergedOutBuffs)
	{
		if (!IsValid(BuffInstance) || !StateSlot->States.Contains(BuffInstance))
		{
			continue;
		}

		StateComponent->RequestStateRemoval(BuffInstance, TcsBuffRemovalReasons::MergedOut);
	}
}

void UTcsBuffComponent::MarkBuffMergeGroupDirty(UTcsBuffInstance* BuffInstance, ETcsBuffMergeDirtyReason DirtyReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	const UTcsStateDefinition* StateDef = IsValid(BuffInstance) ? BuffInstance->GetStateDef() : nullptr;
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || !StateDef || !StateDef->StateSlotType.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(StateDef->StateSlotType);
	if (!StateSlot)
	{
		return;
	}

	StateSlot->MarkBuffMergeGroupDirty(BuffInstance->GetStateDefId(), DirtyReason);
	StateComponent->RequestStateSlotRefresh(StateDef->StateSlotType);
}

void UTcsBuffComponent::GetDebugStateOverlay(const UTcsStateInstance* StateInstance, int32& OutStackCount, FString& OutDurationText) const
{
	OutStackCount = -1;
	OutDurationText = TEXT("0.00");

	const UTcsBuffInstance* BuffInstance = Cast<UTcsBuffInstance>(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	OutStackCount = BuffInstance->GetStackCount();
	const float RemainingDuration = GetBuffRemainingDuration(BuffInstance);
	OutDurationText = (BuffInstance->HasInfiniteDuration() || RemainingDuration < 0.0f)
		? TEXT("Inf")
		: FString::Printf(TEXT("%.2f"), RemainingDuration);
}

bool UTcsBuffComponent::GetBuffMergeDebugLines(FGameplayTag SlotTag, TArray<FString>& OutDebugLines) const
{
	OutDebugLines.Reset();

	if (!SlotTag.IsValid())
	{
		return false;
	}

	const UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	const FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(SlotTag);
	if (!StateSlot)
	{
		return false;
	}

	OutDebugLines.Add(FString::Printf(
		TEXT("Slot=%s Gate=%s FullRebuild=%s DirtyGroupCount=%d GroupCount=%d"),
		*SlotTag.ToString(),
		StateSlot->bIsGateOpen ? TEXT("Open") : TEXT("Closed"),
		StateSlot->bBuffMergeRequiresFullRebuild ? TEXT("true") : TEXT("false"),
		StateSlot->DirtyBuffMergeStateDefIds.Num(),
		StateSlot->BuffMergeGroups.Num()));

	TArray<FName> StateDefIds;
	StateSlot->BuffMergeGroups.GetKeys(StateDefIds);
	StateDefIds.Sort([](const FName& A, const FName& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FName& StateDefId : StateDefIds)
	{
		const FTcsBuffMergeGroupRuntime* GroupRuntime = StateSlot->BuffMergeGroups.Find(StateDefId);
		if (!GroupRuntime)
		{
			continue;
		}

		OutDebugLines.Add(FString::Printf(
			TEXT("Group=%s Members=%d Pending=%s Dirty=%s LastProcessed=%s Dependencies=%s"),
			*StateDefId.ToString(),
			TcsBuffMergeRuntime::CountValidMembers(*GroupRuntime),
			StateSlot->DirtyBuffMergeStateDefIds.Contains(StateDefId) ? TEXT("true") : TEXT("false"),
			*TcsBuffMergeRuntime::FormatDirtyReasons(GroupRuntime->DirtyReasons),
			*TcsBuffMergeRuntime::FormatDirtyReasons(GroupRuntime->LastProcessedDirtyReasons),
			*TcsBuffMergeRuntime::FormatDependencyFlags(GroupRuntime->DependencyFlags)));
	}

	return true;
}

void UTcsBuffComponent::NotifyBuffStackChanged(UTcsBuffInstance* BuffInstance, int32 OldStackCount, int32 NewStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldStackCount == NewStackCount)
	{
		return;
	}

	if (OnBuffStackChanged.IsBound())
	{
		OnBuffStackChanged.Broadcast(StateComponent, BuffInstance, OldStackCount, NewStackCount);
	}

	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);
}

void UTcsBuffComponent::NotifyBuffMaxStackCountChanged(UTcsBuffInstance* BuffInstance, int32 OldMaxStackCount, int32 NewMaxStackCount)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldMaxStackCount == NewMaxStackCount)
	{
		return;
	}

	if (OnBuffMaxStackCountChanged.IsBound())
	{
		OnBuffMaxStackCountChanged.Broadcast(StateComponent, BuffInstance, OldMaxStackCount, NewMaxStackCount);
	}

	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::RuntimeValueChanged);
}

void UTcsBuffComponent::NotifyBuffPeriodChanged(UTcsBuffInstance* BuffInstance, float OldPeriod, float NewPeriod)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance) || OldPeriod == NewPeriod)
	{
		return;
	}

	if (OnBuffPeriodChanged.IsBound())
	{
		OnBuffPeriodChanged.Broadcast(StateComponent, BuffInstance, OldPeriod, NewPeriod);
	}
}

void UTcsBuffComponent::NotifyBuffDurationRefreshed(UTcsBuffInstance* BuffInstance, float NewDuration)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	if (OnBuffDurationRefreshed.IsBound())
	{
		OnBuffDurationRefreshed.Broadcast(StateComponent, BuffInstance, NewDuration);
	}
}

void UTcsBuffComponent::NotifyBuffRemoved(UTcsBuffInstance* BuffInstance, FName RemovalReason)
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent) || !IsValid(BuffInstance))
	{
		return;
	}

	if (OnBuffRemoved.IsBound())
	{
		OnBuffRemoved.Broadcast(StateComponent, BuffInstance, RemovalReason);
	}
}

void UTcsBuffComponent::BindOwnerStateEvents(UTcsStateComponent* InStateComponent)
{
	if (!IsValid(InStateComponent))
	{
		return;
	}

	InStateComponent->OnStateApplySuccess.AddUniqueDynamic(this, &UTcsBuffComponent::HandleOwnerStateApplySuccess);
	InStateComponent->OnStateRemoved.AddUniqueDynamic(this, &UTcsBuffComponent::HandleOwnerStateRemoved);
	InStateComponent->OnStateStageChanged.AddUniqueDynamic(this, &UTcsBuffComponent::HandleOwnerStateStageChanged);
	InStateComponent->OnSlotGateStateChanged.AddUniqueDynamic(this, &UTcsBuffComponent::HandleOwnerSlotGateStateChanged);
	if (!OwnerStateSlotActivationHandle.IsValid())
	{
		OwnerStateSlotActivationHandle = InStateComponent->OnPrepareStateSlotActivation().AddUObject(
			this,
			&UTcsBuffComponent::HandleOwnerStateSlotActivation);
	}
	if (!OwnerStateDebugOverlayHandle.IsValid())
	{
		OwnerStateDebugOverlayHandle = InStateComponent->OnBuildStateDebugOverlay().AddUObject(
			this,
			&UTcsBuffComponent::HandleOwnerStateDebugOverlay);
	}
}

void UTcsBuffComponent::UnbindOwnerStateEvents(UTcsStateComponent* InStateComponent)
{
	if (!IsValid(InStateComponent))
	{
		return;
	}

	InStateComponent->OnStateApplySuccess.RemoveDynamic(this, &UTcsBuffComponent::HandleOwnerStateApplySuccess);
	InStateComponent->OnStateRemoved.RemoveDynamic(this, &UTcsBuffComponent::HandleOwnerStateRemoved);
	InStateComponent->OnStateStageChanged.RemoveDynamic(this, &UTcsBuffComponent::HandleOwnerStateStageChanged);
	InStateComponent->OnSlotGateStateChanged.RemoveDynamic(this, &UTcsBuffComponent::HandleOwnerSlotGateStateChanged);
	if (OwnerStateSlotActivationHandle.IsValid())
	{
		InStateComponent->OnPrepareStateSlotActivation().Remove(OwnerStateSlotActivationHandle);
		OwnerStateSlotActivationHandle.Reset();
	}
	if (OwnerStateDebugOverlayHandle.IsValid())
	{
		InStateComponent->OnBuildStateDebugOverlay().Remove(OwnerStateDebugOverlayHandle);
		OwnerStateDebugOverlayHandle.Reset();
	}
}

void UTcsBuffComponent::HandleOwnerStateApplySuccess(
	AActor* TargetActor,
	FName /*StateDefId*/,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag /*TargetSlot*/,
	ETcsStateStage /*AppliedStage*/)
{
	if (TargetActor != GetOwner())
	{
		return;
	}

	RegisterBuffInstance(CreatedStateInstance);
}

void UTcsBuffComponent::HandleOwnerStateRemoved(
	UTcsStateComponent* StateComponent,
	UTcsStateInstance* StateInstance,
	FName RemovalReason)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	UnregisterBuffInstance(BuffInstance);
	NotifyBuffRemoved(BuffInstance, RemovalReason);
}

void UTcsBuffComponent::HandleOwnerStateStageChanged(
	UTcsStateComponent* StateComponent,
	UTcsStateInstance* StateInstance,
	ETcsStateStage /*PreviousStage*/,
	ETcsStateStage NewStage)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent() || NewStage == ETcsStateStage::SS_Expired)
	{
		return;
	}

	UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance);
	if (!BuffInstance)
	{
		return;
	}

	MarkBuffMergeGroupDirty(BuffInstance, ETcsBuffMergeDirtyReason::ExecutionStageChanged);
}

void UTcsBuffComponent::HandleOwnerSlotGateStateChanged(
	UTcsStateComponent* StateComponent,
	FGameplayTag SlotTag,
	bool /*bIsOpen*/)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent() || !SlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->FindRuntimeStateSlot(SlotTag);
	if (!StateSlot)
	{
		return;
	}

	StateSlot->MarkAllBuffMergeGroupsDirty(ETcsBuffMergeDirtyReason::SlotGateChanged);
}

void UTcsBuffComponent::HandleOwnerStateSlotActivation(
	UTcsStateComponent* StateComponent,
	FGameplayTag /*SlotTag*/,
	FTcsStateSlot* StateSlot)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	ProcessBuffMerging(StateSlot);
}

void UTcsBuffComponent::HandleOwnerStateDebugOverlay(
	UTcsStateComponent* StateComponent,
	const UTcsStateInstance* StateInstance,
	int32& OutStackCount,
	FString& OutDurationText)
{
	if (!IsValid(StateComponent) || StateComponent != ResolveOwnerStateComponent())
	{
		return;
	}

	GetDebugStateOverlay(StateInstance, OutStackCount, OutDurationText);
}

UTcsBuffInstance* UTcsBuffComponent::ResolveBuffInstance(UTcsStateInstance* StateInstance) const
{
	return Cast<UTcsBuffInstance>(StateInstance);
}

const UTcsBuffInstance* UTcsBuffComponent::ResolveBuffInstance(const UTcsStateInstance* StateInstance) const
{
	return Cast<UTcsBuffInstance>(StateInstance);
}

UTcsStateComponent* UTcsBuffComponent::ResolveOwnerStateComponent() const
{
	UTcsStateComponent* StateComponent = OwnerStateComponent.Get();
	if (IsValid(StateComponent))
	{
		return StateComponent;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>();
		if (IsValid(StateComponent))
		{
			UTcsBuffComponent* MutableThis = const_cast<UTcsBuffComponent*>(this);
			MutableThis->OwnerStateComponent = StateComponent;
			MutableThis->BindOwnerStateEvents(StateComponent);
		}
	}

	return StateComponent;
}
