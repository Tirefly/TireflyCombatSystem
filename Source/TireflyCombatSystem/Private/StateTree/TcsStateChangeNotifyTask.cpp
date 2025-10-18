// Copyright Tirefly. All Rights Reserved.

#include "StateTree/TcsStateChangeNotifyTask.h"
#include "State/TcsStateComponent.h"
#include "StateTreeExecutionContext.h"

FTcsStateChangeNotifyTask::FTcsStateChangeNotifyTask()
{
	// 禁用Tick，该Task不需要Tick
	bShouldCallTick = false;

	// 即使State被重新选择也调用EnterState/ExitState
	bShouldStateChangeOnReselect = true;
}

EStateTreeRunStatus FTcsStateChangeNotifyTask::EnterState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	// 获取TcsStateComponent
	UTcsStateComponent* StateComponent = InstanceData.StateComponent;
	if (!StateComponent)
	{
		// 尝试从Owner自动获取
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			StateComponent = Owner->FindComponentByClass<UTcsStateComponent>();
		}
	}

	if (StateComponent)
	{
		// 【关键】通知TcsStateComponent状态变更
		StateComponent->OnStateTreeStateChanged(Context);
	}

	return EStateTreeRunStatus::Running;
}

void FTcsStateChangeNotifyTask::ExitState(
	FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);

	UTcsStateComponent* StateComponent = InstanceData.StateComponent;
	if (!StateComponent)
	{
		if (AActor* Owner = Cast<AActor>(Context.GetOwner()))
		{
			StateComponent = Owner->FindComponentByClass<UTcsStateComponent>();
		}
	}

	if (StateComponent)
	{
		// 【关键】通知TcsStateComponent状态变更
		StateComponent->OnStateTreeStateChanged(Context);
	}
}

#if WITH_EDITOR
FText FTcsStateChangeNotifyTask::GetDescription(
	const FGuid& ID,
	FStateTreeDataView InstanceDataView,
	const IStateTreeBindingLookup& BindingLookup,
	EStateTreeNodeFormatting Formatting) const
{
	return FText::FromString(TEXT("Notify TcsStateComponent of state changes"));
}
#endif
