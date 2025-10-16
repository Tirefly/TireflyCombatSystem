#include "StateTree/TcsStateSlotDebugEvaluator.h"

#include "State/TcsStateComponent.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FTcsStateSlotDebugEvaluator::FTcsStateSlotDebugEvaluator()
	: UpdateInterval(0.25f)
{
}

bool FTcsStateSlotDebugEvaluator::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateComponentHandle);
	return bResult;
}

void FTcsStateSlotDebugEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);
	InstanceData.TimeSinceLastUpdate += DeltaTime;

	if (InstanceData.TimeSinceLastUpdate < UpdateInterval)
	{
		return;
	}

	InstanceData.TimeSinceLastUpdate = 0.0f;

	if (const UTcsStateComponent* StateComponent = &Context.GetExternalData(StateComponentHandle))
	{
		InstanceData.Snapshot = StateComponent->GetSlotDebugSnapshot(SlotFilter);
	}
}
