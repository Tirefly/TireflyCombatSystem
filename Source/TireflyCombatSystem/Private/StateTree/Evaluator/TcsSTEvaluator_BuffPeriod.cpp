// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Evaluator/TcsSTEvaluator_BuffPeriod.h"

#include "Buff/TcsBuffInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"



FTcsSTEvaluator_BuffPeriod::FTcsSTEvaluator_BuffPeriod()
{
}

bool FTcsSTEvaluator_BuffPeriod::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(BuffInstanceHandle);
	return bResult;
}

void FTcsSTEvaluator_BuffPeriod::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	if (DeltaTime <= 0.f)
	{
		return;
	}

	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	// 每帧复位信号，仅在到点的那一帧输出 true
	InstanceData.bIsPeriodBoundary = false;

	const UTcsBuffInstance& BuffInstance = Context.GetExternalData(BuffInstanceHandle);
	const float ResolvedPeriod = PeriodOverride > 0.f
		? PeriodOverride
		: BuffInstance.GetPeriod();

	if (ResolvedPeriod <= 0.f)
	{
		return;
	}

	InstanceData.ElapsedTime += DeltaTime;

	if (InstanceData.ElapsedTime >= ResolvedPeriod)
	{
		InstanceData.ElapsedTime -= ResolvedPeriod;
		InstanceData.bIsPeriodBoundary = true;
	}
}

#if WITH_EDITOR
FText FTcsSTEvaluator_BuffPeriod::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Output bIsPeriodBoundary each Period interval"));
}
#endif
