// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateRemovalConfirmTask.h"

#include "State/TcsState.h"
#include "StateTreeExecutionContext.h"
#include "TcsLogChannels.h"

FTcsStateRemovalConfirmTask::FTcsStateRemovalConfirmTask()
{
	bShouldCallTick = false;
	bShouldStateChangeOnReselect = true;
}

EStateTreeRunStatus FTcsStateRemovalConfirmTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	UTcsStateInstance* StateInstance = Cast<UTcsStateInstance>(Context.GetOwner());
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsStateTree, Warning, TEXT("[%s] Invalid StateInstance owner."), *FString(__FUNCTION__));
		return EStateTreeRunStatus::Failed;
	}

	if (bCancelled && StateInstance->HasPendingRemovalRequest())
	{
		const ETcsStateRemovalRequestReason Reason = StateInstance->GetPendingRemovalRequest().Reason;
		if (Reason != ETcsStateRemovalRequestReason::Cancelled)
		{
			UE_LOG(LogTcsStateTree, Warning,
				TEXT("[%s] bCancelled=true but pending reason is %s. State=%s Id=%d"),
				*FString(__FUNCTION__),
				*StaticEnum<ETcsStateRemovalRequestReason>()->GetNameStringByValue(static_cast<int64>(Reason)),
				*StateInstance->GetStateDefId().ToString(),
				StateInstance->GetInstanceId());
		}
	}

	Context.Stop(EStateTreeRunStatus::Stopped);
	return EStateTreeRunStatus::Stopped;
}

#if WITH_EDITOR
FText FTcsStateRemovalConfirmTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Confirm pending removal by stopping StateTree execution"));
}
#endif

