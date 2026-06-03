// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateInstance.h"

#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "StateTree.h"
#include "StateTreeExecutionContext.h"
#include "TcsLogChannels.h"



#if 0 // Removed: InitializeStateTree() was unused; keep code disabled for history.
bool UTcsStateInstance::InitializeStateTree()
{
	if (!StateDef || !StateDef->StateTreeRef.IsValid())
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateTreeRef is invalid of State %s"),
			*FString(__FUNCTION__),
			*StateDefId.ToString());
		return false;
	}

	// 重置StateTree实例数据
	StateTreeInstanceData.Reset();
	bStateTreeRunning = false;
	CurrentStateTreeStatus = EStateTreeRunStatus::Unset;

	return true;
}
#endif

void UTcsStateInstance::StartStateTree()
{
	StartStateTreeInternal(false);
}

void UTcsStateInstance::RestartStateTree()
{
	StartStateTreeInternal(true);
}

void UTcsStateInstance::StartStateTreeInternal(bool bResetInstanceData)
{
	if (bStateTreeRunning)
	{
		return;
	}

	if (!StateDef)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateDef is invalid for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	const UStateTree* StateTree = StateDef->StateTreeRef.GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 如果需要重置InstanceData（冷启动）
	if (bResetInstanceData)
	{
		StateTreeInstanceData.Reset();
		CurrentStateTreeStatus = EStateTreeRunStatus::Unset;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetContextRequirements(Context))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to set StateTree context for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 启动StateTree
	FStateTreeExecutionContext::FStartParameters StartParams;
	CurrentStateTreeStatus = Context.Start(StartParams);

	if (CurrentStateTreeStatus == EStateTreeRunStatus::Running)
	{
		bStateTreeRunning = true;
		UE_LOG(LogTcsStateTree, Log, TEXT("[%s] StateTree started successfully for StateInstance: %s (Reset: %s)"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString(),
			bResetInstanceData ? TEXT("Yes") : TEXT("No"));
	}
	else
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to start StateTree for StateInstance: %s, Status: %d"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString(),
			(int32)CurrentStateTreeStatus);
	}
}

void UTcsStateInstance::TickStateTree(float DeltaTime)
{
	if (!bStateTreeRunning)
	{
		return;
	}

	if (!StateDef)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateDef is invalid for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	const UStateTree* StateTree = StateDef->StateTreeRef.GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (!SetContextRequirements(Context))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("Failed to set StateTree context during tick for StateInstance: %s"), *GetStateDefId().ToString());
		StopStateTree();
		return;
	}

	// 执行Tick
	CurrentStateTreeStatus = Context.Tick(DeltaTime);

	// 检查运行状态
	switch (CurrentStateTreeStatus)
	{
	case EStateTreeRunStatus::Running:
		// 继续运行
		break;
	case EStateTreeRunStatus::Succeeded:
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree completed successfully for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	case EStateTreeRunStatus::Failed:
		UE_LOG(LogTcsStateTree, Warning, TEXT("StateTree failed for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	case EStateTreeRunStatus::Stopped:
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree was stopped for StateInstance: %s"), *GetStateDefId().ToString());
		bStateTreeRunning = false;
		break;
	default:
		UE_LOG(LogTcsStateTree, Warning, TEXT("Unexpected StateTree status for StateInstance: %s, Status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
		break;
	}
}

void UTcsStateInstance::StopStateTree()
{
	if (!bStateTreeRunning)
	{
		return;
	}

	if (!StateDef)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateDef is invalid for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	const UStateTree* StateTree = StateDef->StateTreeRef.GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求（即使是停止也需要有效的上下文）
	if (SetContextRequirements(Context))
	{
		// 停止StateTree
		CurrentStateTreeStatus = Context.Stop(EStateTreeRunStatus::Stopped);
		UE_LOG(LogTcsStateTree, Log, TEXT("StateTree stopped for StateInstance: %s with status: %d"), 
			*GetStateDefId().ToString(), (int32)CurrentStateTreeStatus);
	}

	bStateTreeRunning = false;
}

void UTcsStateInstance::PauseStateTree()
{
	// Note: State stage must be managed by UTcsStateComponent.
	// This function only stops ticking StateTree (it does not stop/lose internal data).
	if (OwnerStateCmp.IsValid())
	{
		OwnerStateCmp->RemoveFromStateTreeTickScheduler(this);
	}
}

void UTcsStateInstance::ResumeStateTree()
{
	// Note: State stage must be managed by UTcsStateComponent.
	// This function ensures StateTree is running and restores tick scheduling when applicable.
	if (!bStateTreeRunning)
	{
		StartStateTree();
	}

	if (!bStateTreeRunning || !OwnerStateCmp.IsValid() || !StateDef)
	{
		return;
	}

	switch (StateDef->TickPolicy)
	{
	case ETcsStateTreeTickPolicy::WhileActive:
		if (Stage == ETcsStateStage::SS_Active)
		{
			OwnerStateCmp->AddToStateTreeTickScheduler(this);
		}
		else
		{
			OwnerStateCmp->RemoveFromStateTreeTickScheduler(this);
		}
		break;
	case ETcsStateTreeTickPolicy::RunOnce:
	case ETcsStateTreeTickPolicy::ManualOnly:
	default:
		OwnerStateCmp->RemoveFromStateTreeTickScheduler(this);
		break;
	}
}

EStateTreeRunStatus UTcsStateInstance::GetStateTreeRunStatus() const
{
	return CurrentStateTreeStatus;
}

void UTcsStateInstance::SendStateTreeEvent(FGameplayTag EventTag, const FInstancedStruct& EventPayload)
{
	if (!bStateTreeRunning)
	{
		return;
	}

	if (!StateDef)
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] StateDef is invalid for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	const UStateTree* StateTree = StateDef->StateTreeRef.GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsStateTree, Error, TEXT("[%s] Failed to get StateTree for StateInstance: %s"),
			*FString(__FUNCTION__),
			*GetStateDefId().ToString());
		return;
	}

	// 创建执行上下文
	FStateTreeExecutionContext Context(*this, *StateTree, StateTreeInstanceData);

	// 设置上下文需求
	if (SetContextRequirements(Context))
	{
		// 发送事件
		Context.SendEvent(EventTag, FConstStructView(EventPayload));
	}
}

bool UTcsStateInstance::SetContextRequirements(FStateTreeExecutionContext& Context)
{
	UE_LOG(LogTcsStateTree, Error,
		TEXT("%s Generic UTcsStateInstance no longer provides a concrete StateTree schema. Use a concrete runtime subclass instead."),
		*FString(__FUNCTION__));
	return false;
}

bool UTcsStateInstance::CollectExternalData(
	const FStateTreeExecutionContext& Context, 
	const UStateTree* StateTree, 
	TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs, 
	TArrayView<FStateTreeDataView> OutDataViews)
{
	UE_LOG(LogTcsStateTree, Error,
		TEXT("%s Generic UTcsStateInstance no longer provides external data collection. Use a concrete runtime subclass instead."),
		*FString(__FUNCTION__));
	return false;
}
