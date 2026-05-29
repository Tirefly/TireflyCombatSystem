// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "GameFramework/Actor.h"
#include "Buff/TcsBuffInstance.h"
#include "State/TcsStateComponent.h"

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

void UTcsBuffComponent::BindOwnerStateEvents(UTcsStateComponent* InStateComponent)
{
	if (!IsValid(InStateComponent))
	{
		return;
	}

	InStateComponent->OnInternalStateApplySuccess().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateApplySuccess);
	InStateComponent->OnInternalStateRemoved().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateRemoved);
	InStateComponent->OnInternalStateStageChanged().AddUObject(this, &UTcsBuffComponent::HandleOwnerStateStageChanged);
	InStateComponent->OnInternalSlotGateStateChanged().AddUObject(this, &UTcsBuffComponent::HandleOwnerSlotGateStateChanged);
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

	InStateComponent->OnInternalStateApplySuccess().RemoveAll(this);
	InStateComponent->OnInternalStateRemoved().RemoveAll(this);
	InStateComponent->OnInternalStateStageChanged().RemoveAll(this);
	InStateComponent->OnInternalSlotGateStateChanged().RemoveAll(this);
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
