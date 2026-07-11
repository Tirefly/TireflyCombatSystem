// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsLogChannels.h"
#include "TcsEntityInterface.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateSlotDefinition.h"
#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "State/TcsStateDefinition.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"



int32 UTcsStateComponent::NextStateInstanceId = 0;
int32 UTcsStateComponent::NextSourceHandleId = -1;



UTcsStateComponent::UTcsStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bStartLogicAutomatically = false;
}

void UTcsStateComponent::InitializeComponent()
{
	bStartLogicAutomatically = false;
	Super::InitializeComponent();

	bRuntimePrepared = false;
	bStateSlotMappingsReady = false;
	bStateRuntimeActive = false;
	bIsStartingStateRuntime = false;
	bIsStoppingStateRuntime = false;
	RuntimeBootstrapSubsystem = ResolveRuntimeBootstrapSubsystem();
	if (RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRegistered(this);
	}

	if (PrepareStateRuntime() && RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
	}
}

void UTcsStateComponent::UninitializeComponent()
{
	if (bStateRuntimeActive || IsRunning())
	{
		StopStateRuntime();
	}

	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyComponentUnregistered(this);
	}

	bRuntimePrepared = false;
	bStateSlotMappingsReady = false;
	bStateRuntimeActive = false;
	bIsStartingStateRuntime = false;
	bIsStoppingStateRuntime = false;
	RuntimeBootstrapSubsystem = nullptr;

	Super::UninitializeComponent();
}

void UTcsStateComponent::BeginPlay()
{
	bStartLogicAutomatically = false;
	Super::BeginPlay();
	if (PrepareStateRuntime())
	{
		if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
		{
			BootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
		}
	}
}

FTcsSourceHandle UTcsStateComponent::CreateSourceHandle(
	const TArray<FPrimaryAssetId>& CausalityChain,
	AActor* Instigator,
	const FGameplayTagContainer& SourceTags)
{
	return FTcsSourceHandle(++NextSourceHandleId, CausalityChain, Instigator, SourceTags);
}

bool UTcsStateComponent::IsRuntimeReady() const
{
	return bRuntimePrepared && bStateSlotMappingsReady && bStateRuntimeActive && IsRunning();
}

bool UTcsStateComponent::PrepareStateRuntime()
{
	// 仅需确保 DefinitionManager 已就绪——不再依赖独立的 State/Attribute Manager 子系统
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UTcsDefinitionManagerSubsystem* DefMgr = GI->GetSubsystem<UTcsDefinitionManagerSubsystem>())
			{
				if (!DefMgr->IsRuntimeReady())
				{
					bRuntimePrepared = false;
					bStateSlotMappingsReady = false;
					return false;
				}
			}
		}
	}

	bStateSlotMappingsReady = InitStateSlotMappings();
	bRuntimePrepared = bStateSlotMappingsReady;
	return bRuntimePrepared;
}

UTcsRuntimeBootstrapSubsystem* UTcsStateComponent::ResolveRuntimeBootstrapSubsystem()
{
	if (!RuntimeBootstrapSubsystem)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				RuntimeBootstrapSubsystem = GameInstance->GetSubsystem<UTcsRuntimeBootstrapSubsystem>();
			}
		}
	}

	return RuntimeBootstrapSubsystem;
}

bool UTcsStateComponent::StartStateRuntime()
{
	if (IsRuntimeReady())
	{
		return true;
	}

	if (bStateRuntimeActive && !IsRunning())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State runtime active flag was stale because StateTree is no longer running: %s"),
			*FString(__FUNCTION__), *GetPathName());
		bStateRuntimeActive = false;
	}

	if (!bRuntimePrepared || !bStateSlotMappingsReady)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to start state runtime because prepare or StateSlotMapping validation is incomplete: %s"),
			*FString(__FUNCTION__), *GetPathName());
		return false;
	}

	{
		TGuardValue<bool> BootstrapStartGuard(bIsStartingStateRuntime, true);
		Super::StartLogic();
	}
	bStateRuntimeActive = bRuntimePrepared && bStateSlotMappingsReady && IsRunning();
	if (!bStateRuntimeActive)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to start StateTree runtime for %s"),
			*FString(__FUNCTION__), *GetPathName());
	}

	if (bStateRuntimeActive)
	{
		if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
		{
			BootstrapSubsystem->NotifyStateRuntimeReadyChanged(this, true);
		}
	}

	return bStateRuntimeActive;
}

void UTcsStateComponent::StartLogic()
{
	if (!bIsStartingStateRuntime)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Direct StateTree start is blocked; use StartStateRuntime through runtime bootstrap: %s"),
			*FString(__FUNCTION__), *GetPathName());
		return;
	}

	Super::StartLogic();
}

void UTcsStateComponent::StopLogic(const FString& Reason)
{
	if (bIsStoppingStateRuntime)
	{
		Super::StopLogic(Reason);
		return;
	}

	if (!bStateRuntimeActive && !IsRunning())
	{
		return;
	}

	UE_LOG(LogTcsState, Warning, TEXT("[%s] Direct StateTree stop is redirected to TCS runtime stop: %s Reason=%s"),
		*FString(__FUNCTION__), *GetPathName(), *Reason);
	StopStateRuntime();
}

void UTcsStateComponent::RestartLogic()
{
	if (!bRuntimePrepared || !bStateSlotMappingsReady)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State runtime restart is blocked because prepare or StateSlotMapping validation is incomplete: %s"),
			*FString(__FUNCTION__), *GetPathName());
		return;
	}

	if (IsRunning())
	{
		StopStateRuntime();
	}

	StartStateRuntime();
}

void UTcsStateComponent::PauseLogic(const FString& Reason)
{
	if (!IsRuntimeReady())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State runtime pause is blocked because runtime is not ready: %s Reason=%s"),
			*FString(__FUNCTION__), *GetPathName(), *Reason);
		return;
	}

	Super::PauseLogic(Reason);
}

EAILogicResuming::Type UTcsStateComponent::ResumeLogic(const FString& Reason)
{
	if (!IsRuntimeReady())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State runtime resume is blocked because runtime is not ready or no running StateTree exists: %s Reason=%s"),
			*FString(__FUNCTION__), *GetPathName(), *Reason);
		// BrainComponent 的恢复结果枚举没有“失败”分支；这里保持 no-op 并返回 Continue，避免伪造 Restart 语义。
		return EAILogicResuming::Continue;
	}

	return Super::ResumeLogic(Reason);
}

void UTcsStateComponent::StopStateRuntime()
{
	if (!bStateRuntimeActive && !IsRunning())
	{
		return;
	}

	{
		TGuardValue<bool> BootstrapStopGuard(bIsStoppingStateRuntime, true);
		StopLogic(TEXT("State runtime stopped by bootstrap"));
	}
	bStateRuntimeActive = false;
	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyStateRuntimeReadyChanged(this, false);
	}
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
	if (!IsRuntimeReady())
	{
		return;
	}

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

	if (!IsRuntimeReady())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("State runtime is not ready yet."));
	}

	if (!IsValid(OwnerActor) || !IsValid(Instigator) || StateDefId.IsNone())
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			FString::Printf(TEXT("Invalid input while applying state. Owner=%s State=%s Instigator=%s"),
				*GetNameSafe(OwnerActor),
				*StateDefId.ToString(),
				*GetNameSafe(Instigator)));
	}

	UTcsDefinitionManagerSubsystem* DefinitionManager = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTcsDefinitionManagerSubsystem>()
		: nullptr;
	if (!DefinitionManager)
	{
		return ReportApplyFailure(
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve DefinitionManagerSubsystem."));
	}

	const UTcsStateDefinition* StateDef = DefinitionManager->GetStateDefinition(StateDefId);
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
	if (!IsRuntimeReady())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] State runtime is not ready for %s"), *FString(__FUNCTION__), *GetPathName());
		return false;
	}

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
