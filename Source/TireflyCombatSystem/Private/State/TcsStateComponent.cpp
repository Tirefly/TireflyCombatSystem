// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateComponent.h"

#include "TcsLogChannels.h"
#include "TcsEntityInterface.h"
#include "GameFramework/Actor.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateSlotDefinition.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "State/TcsStateDefinition.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"



UTcsStateComponent::UTcsStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bStartLogicAutomatically = true;
}

void UTcsStateComponent::BeginPlay()
{
	// 获取状态管理器子系统
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] World is invalid."), *FString(__FUNCTION__));
		return;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (GI)
	{
		StateMgr = GI->GetSubsystem<UTcsStateManagerSubsystem>();
		AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
	}

	if (!StateMgr)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get TcsStateManagerSubsystem."),
			*FString(__FUNCTION__));
		return;
	}

	// 初始化 StateSlot 和 StateTreeState 的映射
	InitStateSlotMappings();

	// 各项初始化之后，再执行状态管理StateTree
	Super::BeginPlay();

#if !UE_BUILD_SHIPPING
	// 预热自测断言：GameInstanceSubsystem 在 BeginPlay 之前必然完成 Initialize，
	// 若此处仍为空表明 Subsystem 生命周期被破坏，立即暴露。
	checkf(StateMgr, TEXT("StateMgr resolve failed in BeginPlay for %s; GameInstanceSubsystem lifecycle broken."), *GetPathName());
	checkf(AttrMgr, TEXT("AttrMgr resolve failed in BeginPlay for %s; GameInstanceSubsystem lifecycle broken."), *GetPathName());
#endif
}

UTcsStateManagerSubsystem* UTcsStateComponent::ResolveStateManager()
{
	if (!StateMgr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				StateMgr = GI->GetSubsystem<UTcsStateManagerSubsystem>();
			}
		}
		ensureMsgf(StateMgr, TEXT("[%s] Failed to resolve StateManagerSubsystem for %s"),
			*FString(__FUNCTION__), *GetPathName());
	}
	return StateMgr;
}

UTcsAttributeManagerSubsystem* UTcsStateComponent::ResolveAttributeManager()
{
	if (!AttrMgr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
			}
		}
		ensureMsgf(AttrMgr, TEXT("[%s] Failed to resolve AttributeManagerSubsystem for %s"),
			*FString(__FUNCTION__), *GetPathName());
	}
	return AttrMgr;
}

void UTcsStateComponent::BuildStateDebugOverlay(
	const UTcsStateInstance* StateInstance,
	int32& OutStackCount,
	FString& OutDurationText) const
{
	if (!IsValid(StateInstance) || !StateDebugOverlayEvent.IsBound())
	{
		return;
	}

	StateDebugOverlayEvent.Broadcast(const_cast<UTcsStateComponent*>(this), StateInstance, OutStackCount, OutDurationText);
}

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickStateTrees(DeltaTime);
}

void UTcsStateComponent::TickStateTrees(float DeltaTime)
{
	StateTreeTickScheduler.RefreshInstances();	

	TArray<UTcsStateInstance*> InstancesToRemove;
	for (UTcsStateInstance* RunningState : StateTreeTickScheduler.RunningInstances)
	{
		if (!IsValid(RunningState))
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		if (!RunningState->IsStateTreeRunning())
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		const UTcsStateDefinition* StateDef = RunningState->GetStateDef();
		const bool bShouldTick = (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
    								(StateDef && (StateDef->TickPolicy == ETcsStateTreeTickPolicy::WhileActive));

		if (!bShouldTick)
		{
			InstancesToRemove.Add(RunningState);
			continue;
		}

		TGuardValue<bool> StateTreeTickGuard(bIsInStateTreeCallback, true);
		RunningState->TickStateTree(DeltaTime);
		if (!RunningState->IsStateTreeRunning())
		{
			InstancesToRemove.Add(RunningState);
		}
	}

	for (UTcsStateInstance* StateInstance : InstancesToRemove)
	{
		StateTreeTickScheduler.Remove(StateInstance);
	}
}

bool UTcsStateComponent::TryApplyState(
	FName StateDefId,
	AActor* Instigator,
	int32 StateLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	AActor* OwnerActor = GetOwner();
	auto ReportApplyFailure = [this, OwnerActor, StateDefId](
		ETcsStateApplyFailReason FailureReason,
		const FString& FailureMessage,
		bool bFailureAlreadyLogged = false) -> bool
	{
		if (!bFailureAlreadyLogged)
		{
			const UEnum* FailureReasonEnum = StaticEnum<ETcsStateApplyFailReason>();
			const FString FailureReasonText = FailureReasonEnum
				? FailureReasonEnum->GetNameStringByValue(static_cast<int64>(FailureReason))
				: TEXT("Unknown");

			UE_LOG(LogTcsState, Warning,
				TEXT("[%s] State apply failed. Target=%s State=%s Reason=%s Message=%s"),
				TEXT("TryApplyState"),
				*GetNameSafe(OwnerActor),
				*StateDefId.ToString(),
				*FailureReasonText,
				*FailureMessage);
		}

		if (IsValid(OwnerActor) && !StateDefId.IsNone())
		{
			NotifyStateApplyFailed(
				OwnerActor,
				StateDefId,
				FailureReason,
				FailureMessage);
		}

		return false;
	};

	if (!IsValid(OwnerActor) || !IsValid(Instigator) || StateDefId.IsNone())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			FString::Printf(TEXT("Invalid input while applying state. Owner=%s State=%s Instigator=%s"),
				*GetNameSafe(OwnerActor),
				*StateDefId.ToString(),
				*GetNameSafe(Instigator)));
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve StateManagerSubsystem."));
	}

	const UTcsStateDefinition* StateDef = LocalStateMgr->GetStateDefinition(StateDefId);
	if (!StateDef)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Invalid state definition."));
	}

	ETcsStateApplyFailReason CreateFailureReason = ETcsStateApplyFailReason::CreateInstanceFailed;
	FString CreateFailureMessage = TEXT("Failed to create StateInstance.");
	bool bCreateFailureLogged = false;
	UTcsStateInstance* NewStateInstance = CreateStateInstance(
		StateDefId,
		Instigator,
		StateLevel,
		ParentSourceHandle,
		&CreateFailureReason,
		&CreateFailureMessage,
		&bCreateFailureLogged);
	if (!IsValid(NewStateInstance))
	{
		return ReportApplyFailure(CreateFailureReason, CreateFailureMessage, bCreateFailureLogged);
	}

	return TryApplyStateInstance(NewStateInstance);
}

bool UTcsStateComponent::TryApplyStateInstance(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (StateInstance->GetOwner() != OwnerActor)
	{
		if (IsValid(OwnerActor) && !StateInstance->GetStateDefId().IsNone())
		{
			NotifyStateApplyFailed(
				OwnerActor,
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::InvalidInput,
				TEXT("StateInstance owner does not match target StateComponent owner."));
		}
		return false;
	}

	if (!StateInstance->IsInitialized())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is not initialized. StateDef=%s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		NotifyStateApplyFailed(
			OwnerActor,
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("StateInstance is not initialized."));
		return false;
	}

	if (!CheckStateApplyConditions(StateInstance))
	{
		NotifyStateApplyFailed(
			OwnerActor,
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::ApplyConditionsFailed,
			TEXT("State apply conditions check failed."));
		return false;
	}

	return TryAssignStateToStateSlot(StateInstance);
}

bool UTcsStateComponent::CheckStateApplyConditions(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef"), *FString(__FUNCTION__));
		return false;
	}

	for (int32 Index = 0; Index < StateDef->ActiveConditions.Num(); ++Index)
	{
		if (!StateDef->ActiveConditions[Index].IsValid())
		{
			UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid state condition of index %d in StateDef %s"),
				*FString(__FUNCTION__),
				Index,
				*StateInstance->GetStateDefId().ToString());
			return false;
		}

		if (!StateDef->ActiveConditions[Index].bCheckWhenApplying)
		{
			continue;
		}

		UTcsStateCondition* Condition = StateDef->ActiveConditions[Index].ConditionClass.GetDefaultObject();
		if (!Condition->CheckCondition(StateInstance, StateDef->ActiveConditions[Index].Payload))
		{
			return false;
		}
	}

	return true;
}
