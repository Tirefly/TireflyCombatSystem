// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsBuffPeriodDriverTask.h"

#include "StructUtils/InstancedStruct.h"
#include "Buff/TcsBuffInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"
#include "TcsGameplayTags.h"



FTcsBuffPeriodDriverTask::FTcsBuffPeriodDriverTask()
{
	bShouldCallTick = true;
	bShouldStateChangeOnReselect = true;
}

bool FTcsBuffPeriodDriverTask::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(BuffInstanceHandle);
	return bResult;
}

EStateTreeRunStatus FTcsBuffPeriodDriverTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	InstanceData.ElapsedTime = 0.f;
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FTcsBuffPeriodDriverTask::Tick(
	FStateTreeExecutionContext& Context,
	const float DeltaTime) const
{
	if (DeltaTime <= 0.f)
	{
		return EStateTreeRunStatus::Running;
	}

	UTcsBuffInstance& BuffInstance = Context.GetExternalData(BuffInstanceHandle);

	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	const float ResolvedPeriod = InstanceData.PeriodOverride > 0.f
		? InstanceData.PeriodOverride
		: BuffInstance.GetPeriod();
    if (ResolvedPeriod <= 0.f)
	{
		return EStateTreeRunStatus::Running;
	}

	InstanceData.ElapsedTime = FMath::Max(0.f, InstanceData.ElapsedTime + DeltaTime);

	FInstancedStruct EventPayload;
	EventPayload.InitializeAs<FTcsBuffPeriodTickEventPayload>();
	EventPayload.GetMutable<FTcsBuffPeriodTickEventPayload>().ResolvedPeriod = ResolvedPeriod;

	while (InstanceData.ElapsedTime >= ResolvedPeriod)
	{
		InstanceData.ElapsedTime -= ResolvedPeriod;
		BuffInstance.SendStateTreeEvent(TcsGameplayTags::Event_Buff_PeriodTick, EventPayload);
	}

	return EStateTreeRunStatus::Running;
}

#if WITH_EDITOR
FText FTcsBuffPeriodDriverTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Drive Buff period and emit Event.Buff.PeriodTick"));
}
#endif