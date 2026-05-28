// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateSlotDebugEvaluator.h"

#include "TcsConsoleCommands.h"
#include "State/TcsStateInstance.h"
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
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}

void FTcsStateSlotDebugEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData<FInstanceDataType>(*this);

	// 槽位调试快照的字符串构造成本较高，因此这里改成显式开关控制。
	// 未开启调试命令开关时，Evaluator 只清空旧快照并直接返回，不再在常驻 Tick 路径里构造大字符串。
	if (!TcsConsoleCommandRuntime::IsStateDebugEvaluatorSnapshotsEnabled())
	{
		InstanceData.Snapshot.Reset();
		InstanceData.TimeSinceLastUpdate = 0.0f;
		return;
	}

	InstanceData.TimeSinceLastUpdate += DeltaTime;

	if (InstanceData.TimeSinceLastUpdate < UpdateInterval)
	{
		return;
	}

	InstanceData.TimeSinceLastUpdate = 0.0f;

	const UTcsStateInstance& StateInstance = Context.GetExternalData(StateInstanceHandle);
	if (const UTcsStateComponent* StateComponent = StateInstance.GetOwnerStateComponent())
	{
		InstanceData.Snapshot = StateComponent->GetSlotDebugSnapshot(SlotFilter);
	}
}
