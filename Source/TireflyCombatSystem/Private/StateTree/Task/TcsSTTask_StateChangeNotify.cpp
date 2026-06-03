// Copyright Tirefly. All Rights Reserved.

#include "StateTree/Task/TcsSTTask_StateChangeNotify.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeLinker.h"

FTcsSTTask_StateChangeNotify::FTcsSTTask_StateChangeNotify()
{
	// 禁用Tick，该Task不需要Tick
	bShouldCallTick = false;

	// 即使State被重新选择也调用EnterState/ExitState
	bShouldStateChangeOnReselect = true;
}

bool FTcsSTTask_StateChangeNotify::Link(FStateTreeLinker& Linker)
{
	const bool bResult = Super::Link(Linker);
	Linker.LinkExternalData(StateInstanceHandle);
	return bResult;
}

EStateTreeRunStatus FTcsSTTask_StateChangeNotify::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取TcsStateComponent
	UTcsStateComponent* StateComponent = InstanceData.StateComponent;
	if (!StateComponent)
	{
		StateComponent = Context.GetExternalData(StateInstanceHandle).GetOwnerStateComponent();
	}

	if (StateComponent)
	{
		// 【关键】通知TcsStateComponent状态变更
		StateComponent->OnStateTreeStateChanged(Context);
	}

	return EStateTreeRunStatus::Running;
}

void FTcsSTTask_StateChangeNotify::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UTcsStateComponent* StateComponent = InstanceData.StateComponent;
	if (!StateComponent)
	{
		StateComponent = Context.GetExternalData(StateInstanceHandle).GetOwnerStateComponent();
	}

	if (StateComponent)
	{
		// 【关键】通知TcsStateComponent状态变更
		StateComponent->OnStateTreeStateChanged(Context);
	}
}

#if WITH_EDITOR
FText FTcsSTTask_StateChangeNotify::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Notify TcsStateComponent of state changes"));
}
#endif
