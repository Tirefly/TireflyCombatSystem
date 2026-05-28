// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "Attribute/TcsAttributeComponent.h"
#include "TcsLogChannels.h"
#include "TcsEntityInterface.h"
#include "GameFramework/Actor.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateSlotDefinition.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"
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

UTcsStateInstance* UTcsStateComponent::CreateStateInstance(
	FName StateDefRowId,
	AActor* Instigator,
	int32 InLevel,
	const FTcsSourceHandle& ParentSourceHandle,
	ETcsStateApplyFailReason* OutFailureReason,
	FString* OutFailureMessage,
	bool* bOutFailureLogged)
{
	auto ReturnCreateStateFailure = [OutFailureReason, OutFailureMessage, bOutFailureLogged](
		ETcsStateApplyFailReason FailureReason,
		FString FailureMessage,
		bool bFailureLogged = false) -> UTcsStateInstance*
	{
		if (OutFailureReason)
		{
			*OutFailureReason = FailureReason;
		}

		if (OutFailureMessage)
		{
			*OutFailureMessage = MoveTemp(FailureMessage);
		}

		if (bOutFailureLogged)
		{
			*bOutFailureLogged = bFailureLogged;
		}

		return nullptr;
	};

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(Instigator))
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::InvalidInput,
			FString::Printf(TEXT("Invalid owner or instigator while creating state instance. Owner=%s State=%s Instigator=%s"),
				*GetNameSafe(OwnerActor),
				*StateDefRowId.ToString(),
				*GetNameSafe(Instigator)));
	}

	if (!OwnerActor->Implements<UTcsEntityInterface>() || !Instigator->Implements<UTcsEntityInterface>())
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::InvalidInput,
			FString::Printf(TEXT("Owner or Instigator does not implement TcsEntityInterface. State=%s Owner=%s Instigator=%s"),
				*StateDefRowId.ToString(),
				*GetNameSafe(OwnerActor),
				*GetNameSafe(Instigator)));
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			TEXT("Failed to resolve StateManagerSubsystem while creating StateInstance."));
	}

	const UTcsStateDefinition* StateDef = LocalStateMgr->GetStateDefinition(StateDefRowId);
	if (!StateDef)
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::InvalidStateDefinition,
			FString::Printf(TEXT("Invalid state definition: %s"), *StateDefRowId.ToString()));
	}

	UClass* StateInstanceClass = StateDef->ResolveStateInstanceClass();
	if (!StateInstanceClass)
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			FString::Printf(TEXT("State definition '%s' did not provide a runtime class."), *StateDefRowId.ToString()));
	}

	if (!StateInstanceClass->IsChildOf(UTcsStateInstance::StaticClass()))
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			FString::Printf(
				TEXT("State definition '%s' resolved invalid runtime class '%s' that does not derive from UTcsStateInstance."),
				*StateDefRowId.ToString(),
				*GetNameSafe(StateInstanceClass)));
	}

	if (StateInstanceClass->HasAnyClassFlags(CLASS_Abstract))
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			FString::Printf(
				TEXT("State definition '%s' resolved abstract runtime class '%s'. Provide a concrete runtime subclass instead."),
				*StateDefRowId.ToString(),
				*GetNameSafe(StateInstanceClass)));
	}

	UTcsStateInstance* TempStateInstance = NewObject<UTcsStateInstance>(OwnerActor, StateInstanceClass);
	if (!IsValid(TempStateInstance))
	{
		const FString FailureMessage = FString::Printf(
			TEXT("Failed to create temporary state instance for parameter validation. StateDef=%s"),
			*StateDefRowId.ToString());

		UE_LOG(LogTcsState, Error, TEXT("[%s] %s"), TEXT("CreateStateInstance"), *FailureMessage);
		return ReturnCreateStateFailure(ETcsStateApplyFailReason::CreateInstanceFailed, FailureMessage, true);
	}

	TempStateInstance->Initialize(
		StateDef,
		StateDefRowId,
		OwnerActor,
		Instigator,
		LocalStateMgr->AllocateStateInstanceId(),
		InLevel);

	if (!TempStateInstance->IsInitialized())
	{
		const FString FailureMessage = FString::Printf(
			TEXT("Failed to initialize temporary StateInstance for parameter validation. StateDef=%s Owner=%s Instigator=%s"),
			*StateDefRowId.ToString(),
			*OwnerActor->GetName(),
			*Instigator->GetName());

		UE_LOG(LogTcsState, Error, TEXT("[%s] %s"), TEXT("CreateStateInstance"), *FailureMessage);
		TempStateInstance->MarkPendingGC();
		return ReturnCreateStateFailure(ETcsStateApplyFailReason::CreateInstanceFailed, FailureMessage, true);
	}

	TArray<FName> FailedParams;
	if (!EvaluateAndApplyStateParameters(StateDef, Instigator, TempStateInstance, FailedParams))
	{
		FString FailedParamNames;
		for (int32 i = 0; i < FailedParams.Num(); ++i)
		{
			FailedParamNames += FailedParams[i].ToString();
			if (i < FailedParams.Num() - 1)
			{
				FailedParamNames += TEXT(", ");
			}
		}

		TempStateInstance->MarkPendingGC();
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			FString::Printf(TEXT("Parameter evaluation failed for state '%s'. Failed parameters: [%s]. Owner=%s Instigator=%s"),
				*StateDefRowId.ToString(),
				FailedParamNames.IsEmpty() ? TEXT("Unknown") : *FailedParamNames,
				*OwnerActor->GetName(),
				*Instigator->GetName()));
	}

	UTcsStateInstance* StateInstance = TempStateInstance;
	StateInstance->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());

	TArray<FPrimaryAssetId> NewCausalityChain = ParentSourceHandle.CausalityChain;
	if (ParentSourceHandle.IsValid())
	{
		NewCausalityChain.Add(StateDef->GetPrimaryAssetId());
	}

	if (UTcsAttributeManagerSubsystem* LocalAttrMgr = ResolveAttributeManager())
	{
		StateInstance->SetSourceHandle(LocalAttrMgr->CreateSourceHandle(NewCausalityChain, Instigator));
	}
	else
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get AttributeManagerSubsystem, SourceHandle not initialized for state '%s'"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString());
	}

	return StateInstance;
}

bool UTcsStateComponent::EvaluateAndApplyStateParameters(
	const UTcsStateDefinition* StateDef,
	AActor* Instigator,
	UTcsStateInstance* StateInstance,
	TArray<FName>& OutFailedParams)
{
	OutFailedParams.Reset();

	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateDef is null during parameter evaluation"), *FString(__FUNCTION__));
		return false;
	}

	// 如果没有参数需要评估，直接返回成功
	if (StateDef->Parameters.IsEmpty() && StateDef->TagParameters.IsEmpty())
	{
		return true;
	}

	AActor* OwnerActor = GetOwner();
	bool bAllSuccess = true;

	for (const TPair<FName, FTcsStateParameter>& ParamPair : StateDef->Parameters)
	{
		const FName& ParamName = ParamPair.Key;
		const FTcsStateParameter& Param = ParamPair.Value;

		switch (Param.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!Param.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] NumericParamEvaluator for parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				float ParamValue;
				auto ParamEvaluator = Param.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate numeric parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetNumericParam(ParamName, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!Param.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] BoolParamEvaluator for parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = Param.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate bool parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetBoolParam(ParamName, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!Param.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] VectorParamEvaluator for parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = Param.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate vector parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamName.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetVectorParam(ParamName, ParamValue);
				break;
			}
		default:
			UE_LOG(LogTcsState, Warning,
				TEXT("[%s] Unknown parameter type for parameter '%s'"),
				*FString(__FUNCTION__),
				*ParamName.ToString());
			break;
		}
	}

	for (const TPair<FGameplayTag, FTcsStateParameter>& ParamPair : StateDef->TagParameters)
	{
		const FGameplayTag& ParamTag = ParamPair.Key;
		const FTcsStateParameter& Param = ParamPair.Value;
		const FName ParamName = ParamTag.GetTagName();

		switch (Param.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			{
				if (!Param.NumericParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] NumericParamEvaluator for tag parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				float ParamValue;
				auto ParamEvaluator = Param.NumericParamEvaluator->GetDefaultObject<UTcsStateNumericParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate numeric tag parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetNumericParamByTag(ParamTag, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Bool:
			{
				if (!Param.BoolParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] BoolParamEvaluator for tag parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				bool ParamValue;
				auto ParamEvaluator = Param.BoolParamEvaluator->GetDefaultObject<UTcsStateBoolParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate bool tag parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetBoolParamByTag(ParamTag, ParamValue);
				break;
			}
		case ETcsStateParameterType::SPT_Vector:
			{
				if (!Param.VectorParamEvaluator)
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] VectorParamEvaluator for tag parameter '%s' is null"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}

				FVector ParamValue;
				auto ParamEvaluator = Param.VectorParamEvaluator->GetDefaultObject<UTcsStateVectorParamEvaluator>();
				if (!ParamEvaluator->Evaluate(Instigator, OwnerActor, StateInstance, Param.ParamValueContainer, ParamValue))
				{
					UE_LOG(LogTcsState, Error,
						TEXT("[%s] Failed to evaluate vector tag parameter '%s'"),
						*FString(__FUNCTION__),
						*ParamTag.ToString());
					OutFailedParams.Add(ParamName);
					bAllSuccess = false;
					break;
				}
				StateInstance->SetVectorParamByTag(ParamTag, ParamValue);
				break;
			}
		default:
			UE_LOG(LogTcsState, Warning,
				TEXT("[%s] Unknown parameter type for tag parameter '%s'"),
				*FString(__FUNCTION__),
				*ParamTag.ToString());
			break;
		}
	}

	return bAllSuccess;
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

void UTcsStateComponent::InitStateSlotMappings()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] OwnerActor is invalid"), *FString(__FUNCTION__));
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return;
	}

	// StateSlot 初始化分两步：
	// 1. 先按 StateSlotDefinition 重建 RuntimeStateSlots，确保组件自身持有完整的槽位运行时容器；
	// 2. 再按当前 StateTree 建立可成功绑定的槽位 <-> 状态名关系。
	RebuildStateSlotRuntimeData();
	RebuildStateTreeSlotBindings();

	UE_LOG(LogTcsState, Log,
		TEXT("[%s] Initialized %d state slots and %d StateTree bindings for %s"),
		*FString(__FUNCTION__),
		RuntimeStateSlots.Num(),
		Mapping_StateSlotToStateTreeStateName.Num(),
		*OwnerActor->GetName());
}

void UTcsStateComponent::RebuildStateSlotRuntimeData()
{
	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return;
	}

	// 先搬走旧容器，再按最新的定义表重新生成 RuntimeStateSlots。
	// 这样可以在定义发生变动时剔除无效槽位，同时尽量保留仍然存在槽位的运行时状态。
	TMap<FGameplayTag, FTcsStateSlot> ExistingStateSlots = MoveTemp(RuntimeStateSlots);
	RuntimeStateSlots.Empty();

	const TArray<FName> SlotDefIds = LocalStateMgr->GetAllStateSlotDefNames();
	for (const FName& SlotDefId : SlotDefIds)
	{
		const UTcsStateSlotDefinition* SlotDefAsset = LocalStateMgr->GetStateSlotDefinition(SlotDefId);
		if (!SlotDefAsset || !SlotDefAsset->SlotTag.IsValid())
		{
			continue;
		}

		// 同一个 SlotTag 只保留一个运行时槽位，避免重复定义把容器污染成多份状态副本。
		if (RuntimeStateSlots.Contains(SlotDefAsset->SlotTag))
		{
			continue;
		}

		// 默认先创建一个空槽位；如果旧容器里有同名槽位，则把原有运行时数据迁移过来。
		// 这里迁移的是 FTcsStateSlot 本身，因此其中的 States 和 Gate 状态都会被保留。
		FTcsStateSlot RuntimeSlot;
		if (FTcsStateSlot* ExistingSlot = ExistingStateSlots.Find(SlotDefAsset->SlotTag))
		{
			RuntimeSlot = MoveTemp(*ExistingSlot);
		}

		RuntimeSlot.CacheStateSlotDef(SlotDefAsset);
		RuntimeSlot.MarkBuffMergeRequiresFullRebuild();
		RuntimeSlot.MarkAllBuffMergeGroupsDirty(ETcsBuffMergeDirtyReason::ForceRebuild);

		RuntimeStateSlots.Add(SlotDefAsset->SlotTag, MoveTemp(RuntimeSlot));
	}
}

void UTcsStateComponent::RebuildStateTreeSlotBindings()
{
	// 绑定表只负责 StateSlot 与 StateTree 状态名的桥接，不持有任何槽位运行时数据，
	// 因此每次都从当前 StateTree 重新扫描并完整重建。
	Mapping_StateSlotToStateTreeStateName.Empty();
	Mapping_StateTreeStateNameToStateSlotTags.Empty();

	const UStateTree* StateTree = GetStateTree();
	if (!IsValid(StateTree))
	{
		if (AActor* OwnerActor = GetOwner())
		{
			UE_LOG(LogTcsState, Verbose,
				TEXT("[%s] No StateTree assigned on StateComponent of %s; runtime slots initialized without bindings"),
				*FString(__FUNCTION__),
				*OwnerActor->GetName());
		}

		return;
	}

	// 先把当前 StateTree 中真实存在的状态名收集出来，后面只接受能在树里找到的绑定。
	TSet<FName> AvailableStateNames;
	const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
	for (const FCompactStateTreeState& State : States)
	{
		if (!State.Name.IsNone())
		{
			AvailableStateNames.Add(State.Name);
		}
	}

	AActor* OwnerActor = GetOwner();
	for (const TPair<FGameplayTag, FTcsStateSlot>& Pair : RuntimeStateSlots)
	{
		const FGameplayTag StateSlotTag = Pair.Key;
		const FTcsStateSlot& RuntimeSlot = Pair.Value;
		const UTcsStateSlotDefinition* StateSlotDef = RuntimeSlot.GetStateSlotDef();
		if (!IsValid(StateSlotDef) || !StateSlotTag.IsValid())
		{
			continue;
		}

		const FName& StateTreeStateName = StateSlotDef->StateTreeStateName;
		if (StateTreeStateName.IsNone())
		{
			// 没配置 StateTreeStateName 的槽位依然存在于 RuntimeStateSlots 中，
			// 只是不会参与 StateTree 驱动的 Gate 开关联动。
			continue;
		}

		// 只有当定义里声明的状态名确实存在于当前 StateTree 中时，
		// 才把这个槽位加入绑定表，避免运行时出现悬空映射。
		const bool bMapped = AvailableStateNames.Contains(StateTreeStateName);
		if (bMapped)
		{
			Mapping_StateSlotToStateTreeStateName.Add(StateSlotTag, StateTreeStateName);
			Mapping_StateTreeStateNameToStateSlotTags.Add(StateTreeStateName, StateSlotTag);
		}

		UE_LOG(LogTcsState, Verbose, TEXT("[%s] No StateTree assigned on StateComponent of %s"),
			*FString(__FUNCTION__),
			TEXT(""));

		UE_LOG(LogTcsState, Log, TEXT("[%s] State Slot [%s] -> StateTree State [%s] %s of %s"),
			*FString(__FUNCTION__),
			*StateSlotTag.ToString(),
			*StateTreeStateName.ToString(),
			bMapped ? TEXT("mapped") : TEXT("not found"),
			*GetNameSafe(OwnerActor));
	}
}

void UTcsStateComponent::DiffStateTreeStateNames(
	const TArray<FName>& NewStates,
	const TArray<FName>& OldStates,
	TSet<FName>& AddedStates,
	TSet<FName>& RemovedStates) const
{
	TSet<FName> NewStateSet(NewStates);
	NewStateSet.Remove(NAME_None);
	TSet<FName> OldStateSet(OldStates);
	OldStateSet.Remove(NAME_None);

	AddedStates = NewStateSet;
	for (const FName OldState : OldStateSet)
	{
		AddedStates.Remove(OldState);
	}

	RemovedStates = OldStateSet;
	for (const FName NewState : NewStateSet)
	{
		RemovedStates.Remove(NewState);
	}
}

void UTcsStateComponent::CollectAffectedSlotTagsForStateChanges(
	const TSet<FName>& AddedStates,
	const TSet<FName>& RemovedStates,
	TSet<FGameplayTag>& OutAffectedSlotTags) const
{
	OutAffectedSlotTags.Reset();

	auto CollectFromReverseBindings = [this, &OutAffectedSlotTags](const TSet<FName>& StateNames)
	{
		TArray<FGameplayTag> SlotTags;
		for (const FName StateName : StateNames)
		{
			SlotTags.Reset();
			Mapping_StateTreeStateNameToStateSlotTags.MultiFind(StateName, SlotTags);
			for (const FGameplayTag SlotTag : SlotTags)
			{
				if (SlotTag.IsValid())
				{
					OutAffectedSlotTags.Add(SlotTag);
				}
			}
		}
	};

	if (!Mapping_StateTreeStateNameToStateSlotTags.IsEmpty())
	{
		CollectFromReverseBindings(AddedStates);
		CollectFromReverseBindings(RemovedStates);
		return;
	}

	// 反向缓存不可用时，保守回退到正向绑定表扫描，保持语义正确。
	for (const TPair<FGameplayTag, FName>& Pair : Mapping_StateSlotToStateTreeStateName)
	{
		if (AddedStates.Contains(Pair.Value) || RemovedStates.Contains(Pair.Value))
		{
			OutAffectedSlotTags.Add(Pair.Key);
		}
	}
}

bool UTcsStateComponent::TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)
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

	if (!StateDef->StateSlotType.IsValid())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateDef %s does not specify StateSlotType."),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("StateDef does not specify a valid StateSlotType."));
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance owner component mismatch. State=%s Component=%s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString(),
			*GetPathName());
		return false;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateDef->StateSlotType);
	if (!StateSlot)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlot %s not found in owner StateComponent."),
			*FString(__FUNCTION__),
			*StateDef->StateSlotType.ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlot,
			FString::Printf(TEXT("StateSlot %s not found."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	const UTcsStateSlotDefinition* StateSlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(StateSlotDef))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDef %s not found."),
			*FString(__FUNCTION__),
			*StateDef->StateSlotType.ToString());
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlotDefinition,
			FString::Printf(TEXT("StateSlotDef %s not found."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	ClearStateSlotExpiredStates(StateSlot);

	if (StateSlot->States.Contains(StateInstance))
	{
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::AlreadyInSlot,
			TEXT("StateInstance already exists in target slot."));
		return false;
	}

	if (!StateSlot->bIsGateOpen && StateSlotDef->GateCloseBehavior == ETcsStateSlotGateClosePolicy::SSGCP_Cancel)
	{
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::SlotGateClosed_CancelPolicy,
			FString::Printf(TEXT("StateSlot %s gate is closed (Cancel policy)."), *StateDef->StateSlotType.ToString()));
		return false;
	}

	if (StateSlot->bIsGateOpen
		&& StateSlotDef->ActivationMode == ETcsStateSlotActivationMode::SSAM_PriorityOnly
		&& StateSlotDef->PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
	{
		bool bHasExisting = false;
		int32 BestPriority = TNumericLimits<int32>::Lowest();
		for (const UTcsStateInstance* Existing : StateSlot->States)
		{
			if (!IsValid(Existing) || Existing->GetCurrentStage() == ETcsStateStage::SS_Expired)
			{
				continue;
			}

			bHasExisting = true;
			const UTcsStateDefinition* ExistingStateDef = Existing->GetStateDef();
			if (ExistingStateDef)
			{
				BestPriority = FMath::Max(BestPriority, ExistingStateDef->Priority);
			}
		}

		if (bHasExisting && StateDef->Priority < BestPriority)
		{
			NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::LowerPriorityRejected,
				TEXT("State application rejected: lower priority than existing state in PriorityOnly slot."));
			return false;
		}
	}

	StateSlot->States.Add(StateInstance);
	StateSlot->MarkBuffMergeGroupDirty(StateInstance->GetStateDefId(), ETcsBuffMergeDirtyReason::MembershipChanged);
	StateSlot->MarkBuffMergeRequiresFullRebuild();

	if (!StateSlot->bIsGateOpen)
	{
		const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
		bool bStageChanged = false;
		switch (StateSlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			bStageChanged = StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp);
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
		default:
			bStageChanged = StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause);
			break;
		}

		if (bStageChanged)
		{
			NotifyStateStageChanged(StateInstance, PreviousStage, StateInstance->GetCurrentStage());
		}
	}

	RequestUpdateStateSlotActivation(StateDef->StateSlotType);

	if (IsStateStillValid(StateInstance))
	{
		StateInstanceIndex.AddInstance(StateInstance);
		NotifyStateApplySuccess(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			StateInstance,
			StateDef->StateSlotType,
			StateInstance->GetCurrentStage());
	}
	else
	{
		UE_LOG(LogTcsState, Verbose,
			TEXT("[%s] State '%s' was merged and removed, skipping ApplySuccess notification"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
	}

	return true;
}

void UTcsStateComponent::RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates)
{
	TSet<FName> AddedStates;
	TSet<FName> RemovedStates;
	DiffStateTreeStateNames(NewStates, OldStates, AddedStates, RemovedStates);
	if (AddedStates.IsEmpty() && RemovedStates.IsEmpty())
	{
		return;
	}

	TSet<FName> NewStateSet(NewStates);
	NewStateSet.Remove(NAME_None);

	TSet<FGameplayTag> AffectedSlotTags;
	CollectAffectedSlotTagsForStateChanges(AddedStates, RemovedStates, AffectedSlotTags);
	if (AffectedSlotTags.IsEmpty())
	{
		return;
	}

	BeginStateSlotActivationBatch();
	for (const FGameplayTag SlotTag : AffectedSlotTags)
	{
		const FName* MappedStateName = Mapping_StateSlotToStateTreeStateName.Find(SlotTag);
		if (!MappedStateName)
		{
			continue;
		}

		const bool bShouldOpen = NewStateSet.Contains(*MappedStateName);

		const bool bWasOpen = IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			SetSlotGateOpen(SlotTag, bShouldOpen);
			UE_LOG(LogTcsState, Log,
				TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				*SlotTag.ToString(),
				bShouldOpen ? TEXT("opened") : TEXT("closed"),
				*MappedStateName->ToString());
		}
	}
	EndStateSlotActivationBatch();
}

void UTcsStateComponent::RequestUpdateStateSlotActivation(FGameplayTag SlotTag)
{
	// 先做最便宜的输入校验。
	// 这条入口会被 Gate 变化、状态移除、显式刷新请求等多条路径复用，
	// 因此无效槽位要在这里统一拦下，避免把脏请求继续传播到批处理队列。
	if (!SlotTag.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlotTag"), *FString(__FUNCTION__));
		return;
	}

	// 只要当前已经在执行槽位刷新，或者调用方显式进入了批处理作用域，
	// 本次请求就不能立刻重跑 `UpdateStateSlotActivation()`：
	// 1. 正在刷新时立刻重入，会把同一轮收敛拆成多次嵌套执行；
	// 2. 批处理期间立刻执行，会把“同批次多次语义变化”重新放大成多次完整结算。
	//
	// 因此这里统一改为“按槽位去重入队”。这样同一槽位在同一批次里无论被请求多少次，
	// 最终都只会在批次末或外层刷新结束后结算一次最终结果。
	if (bIsUpdatingSlotActivation || StateSlotActivationBatchDepth > 0)
	{
		PendingSlotActivationUpdates.Add(SlotTag);
		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deferred slot activation update: Component=%s Slot=%s"),
			*FString(__FUNCTION__),
			*GetNameSafe(GetOwner()),
			*SlotTag.ToString());
		return;
	}

	// 只有在“不重入、也不批处理”的普通路径下，才立即同步刷新槽位。
	// 这样单次变更依然保持当前帧、当前调用链内完成收敛，不会被无谓地延迟。
	UpdateStateSlotActivation(SlotTag);
}

void UTcsStateComponent::BeginStateSlotActivationBatch()
{
	++StateSlotActivationBatchDepth;
}

void UTcsStateComponent::EndStateSlotActivationBatch()
{
	check(StateSlotActivationBatchDepth > 0);
	--StateSlotActivationBatchDepth;

	// 如果批处理尾声组件自身或 Owner 已经进入销毁流程，再去排空待处理槽位只会在 teardown 阶段
	// 额外重跑一次收敛逻辑，既没有收益，也更容易撞上对象清理顺序。
	// 这里直接丢弃积压请求，让批处理以“安全结束”为第一目标。
	if (StateSlotActivationBatchDepth == 0 && (IsBeingDestroyed() || !IsValid(GetOwner())))
	{
		PendingSlotActivationUpdates.Empty();
		return;
	}

	// 只有最外层批处理结束、组件仍处于可结算状态、且当前不在刷新重入中时，
	// 才统一排空请求。这样既保留当前帧内完成最终收敛的语义，也避免嵌套批处理提前触发结算。
	if (StateSlotActivationBatchDepth == 0 && !bIsUpdatingSlotActivation && !PendingSlotActivationUpdates.IsEmpty())
	{
		DrainPendingSlotActivationUpdates();
	}
}

void UTcsStateComponent::DrainPendingSlotActivationUpdates()
{
	const int32 MaxIterations = 10;
	int32 Iteration = 0;

	while (!PendingSlotActivationUpdates.IsEmpty() && Iteration < MaxIterations)
	{
		const TSet<FGameplayTag> ToProcess = PendingSlotActivationUpdates;
		PendingSlotActivationUpdates.Empty();

		for (const FGameplayTag& SlotTag : ToProcess)
		{
			if (RuntimeStateSlots.Contains(SlotTag))
			{
				UpdateStateSlotActivation(SlotTag);
			}
		}

		Iteration++;
	}

	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("[%s] Max iterations (%d) reached, possible infinite loop. Remaining pending updates: %d"),
			*FString(__FUNCTION__),
			MaxIterations,
			PendingSlotActivationUpdates.Num());
		PendingSlotActivationUpdates.Empty();
	}
}

void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag StateSlotTag)
{
	if (bIsUpdatingSlotActivation)
	{
		PendingSlotActivationUpdates.Add(StateSlotTag);
		return;
	}

	{
		TGuardValue<bool> Guard(bIsUpdatingSlotActivation, true);

		if (!StateSlotTag.IsValid())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlotTag"), *FString(__FUNCTION__));
		}
		else if (FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateSlotTag))
		{
			ClearStateSlotExpiredStates(StateSlot);
			SortStatesByPriority(StateSlot->States);
			if (PrepareStateSlotActivationEvent.IsBound())
			{
				PrepareStateSlotActivationEvent.Broadcast(this, StateSlotTag, StateSlot);
			}
			EnforceSlotGateConsistency(StateSlotTag);

			if (!StateSlot->bIsGateOpen)
			{
				CleanupInvalidStates(StateSlot);
			}
			else
			{
				ProcessStateSlotByActivationMode(StateSlot, StateSlotTag);
				CleanupInvalidStates(StateSlot);
				OnStateSlotChanged(StateSlotTag);
			}
		}
		else
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
				*FString(__FUNCTION__),
				*StateSlotTag.ToString());
		}
	}

	if (StateSlotActivationBatchDepth == 0)
	{
		DrainPendingSlotActivationUpdates();
	}
}

void UTcsStateComponent::EnforceSlotGateConsistency(FGameplayTag StateSlotTag)
{
	if (!StateSlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateSlotTag);
	if (!StateSlot || StateSlot->bIsGateOpen)
	{
		return;
	}

	const UTcsStateSlotDefinition* SlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(SlotDef))
	{
		return;
	}

	TArray<UTcsStateInstance*> StatesToCancel;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!IsValid(State))
		{
			continue;
		}

		const ETcsStateStage Stage = State->GetCurrentStage();
		if (Stage == ETcsStateStage::SS_Expired)
		{
			continue;
		}

		switch (SlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			if (Stage == ETcsStateStage::SS_Active)
			{
				HangUpState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
			if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
			{
				PauseState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Cancel:
			StatesToCancel.Add(State);
			break;
		}
	}

	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (IsValid(State))
		{
			CancelState(State);
		}
	}

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			HangUpState(State);
		}
	}

#if !UE_BUILD_SHIPPING
	for (const UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State))
		{
			checkf(State->GetCurrentStage() != ETcsStateStage::SS_Active,
				TEXT("[EnforceSlotGateConsistency] Invariant violation: Active state found in closed gate slot. Slot=%s State=%s Stage=%s"),
				*StateSlotTag.ToString(),
				*State->GetStateDefId().ToString(),
				*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())));
		}
	}
#endif
}

void UTcsStateComponent::ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot is null."), *FString(__FUNCTION__));
		return;
	}

	StateSlot->States.RemoveAll([this](UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}

		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			StateTreeTickScheduler.Remove(State);
			StateInstanceIndex.RemoveInstance(State);
			return true;
		}

		return false;
	});
	StateInstanceIndex.RefreshInstances();
}

void UTcsStateComponent::SortStatesByPriority(TArray<UTcsStateInstance*>& States)
{
	// 槽位内没有状态，或只有一个状态时，排序不会改变结果，直接短路即可。
	if (States.Num() < 2)
	{
		return;
	}

	States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const UTcsStateDefinition* AStateDef = A.GetStateDef();
		const UTcsStateDefinition* BStateDef = B.GetStateDef();
		if (!AStateDef || !BStateDef)
		{
			return false;
		}
		return AStateDef->Priority > BStateDef->Priority;
	});
}

void UTcsStateComponent::ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag)
{
	if (!StateSlot)
	{
		return;
	}

	const UTcsStateSlotDefinition* SlotDef = StateSlot->GetStateSlotDef();
	if (!IsValid(SlotDef))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDef %s not found"),
			*FString(__FUNCTION__),
			*SlotTag.ToString());
		return;
	}

	switch (SlotDef->ActivationMode)
	{
	case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
		ProcessPriorityOnlyMode(StateSlot, SlotDef);
		break;
	case ETcsStateSlotActivationMode::SSAM_AllActive:
		ProcessAllActiveMode(StateSlot);
		break;
	}
}

void UTcsStateComponent::ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinition* SlotDef)
{
	if (!StateSlot || StateSlot->States.Num() == 0 || !SlotDef)
	{
		return;
	}

	int32 HighestPriority = TNumericLimits<int32>::Lowest();
	for (UTcsStateInstance* Candidate : StateSlot->States)
	{
		if (IsValid(Candidate))
		{
			const UTcsStateDefinition* CandidateStateDef = Candidate->GetStateDef();
			if (CandidateStateDef)
			{
				HighestPriority = FMath::Max(HighestPriority, CandidateStateDef->Priority);
			}
		}
	}

	TArray<UTcsStateInstance*> HighestPriorityStates;
	for (UTcsStateInstance* Candidate : StateSlot->States)
	{
		if (IsValid(Candidate))
		{
			const UTcsStateDefinition* CandidateStateDef = Candidate->GetStateDef();
			if (CandidateStateDef && CandidateStateDef->Priority == HighestPriority)
			{
				HighestPriorityStates.Add(Candidate);
			}
		}
	}

	if (HighestPriorityStates.Num() == 0)
	{
		return;
	}

	if (HighestPriorityStates.Num() > 1 && SlotDef->SamePriorityPolicy)
	{
		const UTcsStateSamePriorityPolicy* Policy = SlotDef->SamePriorityPolicy->GetDefaultObject<UTcsStateSamePriorityPolicy>();
		if (IsValid(Policy))
		{
			HighestPriorityStates.Sort([Policy](const UTcsStateInstance& A, const UTcsStateInstance& B)
			{
				const int64 KeyA = Policy->GetOrderKey(&A);
				const int64 KeyB = Policy->GetOrderKey(&B);
				return KeyA > KeyB;
			});
		}
	}

	UTcsStateInstance* HighestPriorityState = HighestPriorityStates[0];
	if (!IsValid(HighestPriorityState))
	{
		return;
	}

	if (HighestPriorityState->GetCurrentStage() != ETcsStateStage::SS_Active)
	{
		ActivateState(HighestPriorityState);
	}

	TArray<UTcsStateInstance*> StatesToCancel;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!IsValid(State) || State == HighestPriorityState)
		{
			continue;
		}

		if (SlotDef->PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
		{
			StatesToCancel.Add(State);
			continue;
		}

		ApplyPreemptionPolicyToState(State, SlotDef->PreemptionPolicy);
	}

	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (IsValid(State))
		{
			CancelState(State);
		}
	}
}

void UTcsStateComponent::ProcessAllActiveMode(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() != ETcsStateStage::SS_Active)
		{
			ActivateState(State);
		}
	}
}

void UTcsStateComponent::ApplyPreemptionPolicyToState(
	UTcsStateInstance* State,
	ETcsStatePreemptionPolicy Policy)
{
	if (!IsValid(State))
	{
		return;
	}

	switch (Policy)
	{
	case ETcsStatePreemptionPolicy::SPP_HangUpLowerPriority:
		if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			HangUpState(State);
		}
		break;
	case ETcsStatePreemptionPolicy::SPP_PauseLowerPriority:
		if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			PauseState(State);
		}
		break;
	case ETcsStatePreemptionPolicy::SPP_CancelLowerPriority:
		break;
	}
}

void UTcsStateComponent::CleanupInvalidStates(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	StateSlot->States.RemoveAll([](const UTcsStateInstance* State)
	{
		return !IsValid(State);
	});
}

void UTcsStateComponent::RemoveStateFromSlot(
	FTcsStateSlot* StateSlot,
	UTcsStateInstance* State,
	bool bDeactivateIfNeeded)
{
	if (!StateSlot || !IsValid(State))
	{
		return;
	}

	StateSlot->States.Remove(State);
	if (bDeactivateIfNeeded && State->GetCurrentStage() != ETcsStateStage::SS_Inactive)
	{
		DeactivateState(State);
	}
}

bool UTcsStateComponent::GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const
{
	if (!SlotTag.IsValid())
	{
		OutStates.Empty();
		return false;
	}

	return StateInstanceIndex.GetInstancesBySlot(SlotTag, OutStates);
}

bool UTcsStateComponent::GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const
{
	if (StateDefId.IsNone())
	{
		OutStates.Empty();
		return false;
	}

	return StateInstanceIndex.GetInstancesByName(StateDefId, OutStates);
}

bool UTcsStateComponent::GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const
{
	OutStates.Empty();
	for (UTcsStateInstance* State : StateInstanceIndex.Instances)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			OutStates.Add(State);
		}
	}
	return OutStates.Num() > 0;
}

bool UTcsStateComponent::HasStateWithDefId(FName StateDefId) const
{
	TArray<UTcsStateInstance*> States;
	return GetStatesByDefId(StateDefId, States);
}

bool UTcsStateComponent::HasActiveStateInSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsStateInstance*> States;
	if (!GetStatesInSlot(SlotTag, States))
	{
		return false;
	}

	for (const UTcsStateInstance* State : States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			return true;
		}
	}

	return false;
}

FTcsStateSlot* UTcsStateComponent::FindRuntimeStateSlot(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return nullptr;
	}

	return RuntimeStateSlots.Find(SlotTag);
}

const FTcsStateSlot* UTcsStateComponent::FindRuntimeStateSlot(FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid())
	{
		return nullptr;
	}

	return RuntimeStateSlots.Find(SlotTag);
}

void UTcsStateComponent::RequestStateSlotRefresh(FGameplayTag SlotTag)
{
	RequestUpdateStateSlotActivation(SlotTag);
}

bool UTcsStateComponent::RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return true;
	}

	FinalizeStateRemoval(StateInstance, RemovalReason);
	return true;
}

bool UTcsStateComponent::RemoveState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance %s does not belong to component %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString(),
			*GetPathName());
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return false;
	}

	const FGameplayTag SlotTag = StateDef->StateSlotType;
	if (!RuntimeStateSlots.Contains(SlotTag))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
			*FString(__FUNCTION__),
			*SlotTag.ToString());
		return false;
	}

	return RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Removed);
}

int32 UTcsStateComponent::RemoveStatesByDefId(FName StateDefId, bool bRemoveAll)
{
	if (StateDefId.IsNone())
	{
		return 0;
	}

	// 先通过定义名索引直接命中候选实例，避免为了移除同一种状态而扫描所有槽位。
	TArray<UTcsStateInstance*> IndexedStates;
	if (!StateInstanceIndex.GetInstancesByName(StateDefId, IndexedStates))
	{
		return 0;
	}

	// 再按槽位分组，保持“同槽位状态集中回收”的执行节奏，
	// 同时配合批处理，把每个受影响槽位压成一次最终刷新。
	TMap<FGameplayTag, TArray<UTcsStateInstance*>> StatesBySlot;
	TArray<FGameplayTag> SlotOrder;
	for (UTcsStateInstance* State : IndexedStates)
	{
		if (!IsValid(State))
		{
			continue;
		}

		const UTcsStateDefinition* StateDef = State->GetStateDef();
		if (!StateDef || !StateDef->StateSlotType.IsValid())
		{
			continue;
		}

		TArray<UTcsStateInstance*>& SlotStates = StatesBySlot.FindOrAdd(StateDef->StateSlotType);
		if (SlotStates.IsEmpty())
		{
			SlotOrder.Add(StateDef->StateSlotType);
		}

		SlotStates.Add(State);
		if (!bRemoveAll)
		{
			break;
		}
	}

	int32 RemovedCount = 0;
	BeginStateSlotActivationBatch();
	for (const FGameplayTag& SlotTag : SlotOrder)
	{
		const TArray<UTcsStateInstance*>* StatesToRemove = StatesBySlot.Find(SlotTag);
		if (!StatesToRemove)
		{
			continue;
		}

		for (UTcsStateInstance* State : *StatesToRemove)
		{
			if (RequestStateRemoval(State, TcsStateRemovalReasons::Removed))
			{
				RemovedCount++;
			}
		}

		if (!bRemoveAll && RemovedCount > 0)
		{
			break;
		}
	}

	EndStateSlotActivationBatch();

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStatesInSlot(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return 0;
	}

	FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(SlotTag);
	if (!StateSlot)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	const TArray<UTcsStateInstance*> StatesToRemove = StateSlot->States;
	BeginStateSlotActivationBatch();

	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (IsValid(State))
		{
			RequestStateRemoval(State, TcsStateRemovalReasons::Removed);
			RemovedCount++;
		}
	}

	EndStateSlotActivationBatch();

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStates()
{
	ensureMsgf(!IsInStateTreeUpdateContext(), TEXT("[%s] RemoveAllStates called during StateTree update on %s. Prefer frame-boundary reclaim to avoid overlapping callback teardown."),
		*FString(__FUNCTION__),
		*GetPathName());

	TArray<UTcsStateInstance*> StatesToRemove;
	for (auto& Pair : RuntimeStateSlots)
	{
		for (UTcsStateInstance* State : Pair.Value.States)
		{
			if (IsValid(State))
			{
				StatesToRemove.Add(State);
			}
		}
	}

	int32 TotalRemoved = 0;
	BeginStateSlotActivationBatch();
	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (RequestStateRemoval(State, TcsStateRemovalReasons::Removed))
		{
			TotalRemoved++;
		}
	}
	EndStateSlotActivationBatch();

	return TotalRemoved;
}

void UTcsStateComponent::ActivateState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Activating state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Active))
	{
		return;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	switch (StateDef->TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->RestartStateTree();
		{
			TGuardValue<bool> StateTreeTickGuard(bIsInStateTreeCallback, true);
			StateInstance->TickStateTree(0.f);
		}
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->RestartStateTree();
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->RestartStateTree();
		if (StateInstance->IsStateTreeRunning())
		{
			StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
}

void UTcsStateComponent::DeactivateState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
	if (PreviousStage == ETcsStateStage::SS_Inactive)
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deactivating state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Inactive))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Inactive);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Inactive, FName(TEXT("Deactivated")));
	}
}

void UTcsStateComponent::HangUpState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Hanging up state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_HangUp);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_HangUp, FName(TEXT("HangUp")));
	}
}

void UTcsStateComponent::ResumeState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Resuming state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Active))
	{
		return;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	switch (StateDef->TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->ResumeStateTree();
		{
			TGuardValue<bool> StateTreeTickGuard(bIsInStateTreeCallback, true);
			StateInstance->TickStateTree(0.f);
		}
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->ResumeStateTree();
		StateTreeTickScheduler.Remove(StateInstance);
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->ResumeStateTree();
		if (StateInstance->IsStateTreeRunning())
		{
			StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
}

void UTcsStateComponent::PauseState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] Pausing state: %s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString());

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause))
	{
		return;
	}

	StateInstance->PauseStateTree();
	StateTreeTickScheduler.Remove(StateInstance);
	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Pause);
	if (PreviousStage == ETcsStateStage::SS_Active)
	{
		NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Pause, FName(TEXT("Pause")));
	}
}

void UTcsStateComponent::CancelState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Cancelled);
}

void UTcsStateComponent::ExpireState(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"), *FString(__FUNCTION__));
		return;
	}

	RequestStateRemoval(StateInstance, TcsStateRemovalReasons::Expired);
}

bool UTcsStateComponent::IsStateStillValid(UTcsStateInstance* StateInstance) const
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return false;
	}

	if (StateInstance->GetOwnerStateComponent() != this)
	{
		return false;
	}

	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		return false;
	}

	const FTcsStateSlot* StateSlot = RuntimeStateSlots.Find(StateDef->StateSlotType);
	return StateSlot && StateSlot->States.Contains(StateInstance);
}

void UTcsStateComponent::FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	const UTcsStateDefinition* StateDef = StateInstance->GetStateDef();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDef: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return;
	}

	const FGameplayTag SlotTag = StateDef->StateSlotType;
	UE_LOG(LogTcsState, Verbose, TEXT("[%s] FinalizeRemoval: State=%s Id=%d Reason=%s Owner=%s Slot=%s Stage=%s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString(),
		StateInstance->GetInstanceId(),
		*RemovalReason.ToString(),
		OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
		*SlotTag.ToString(),
		*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(StateInstance->GetCurrentStage())));

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	if (StateInstance->IsStateTreeRunning())
	{
		StateInstance->StopStateTree();
	}

	if (!StateInstance->SetCurrentStage(ETcsStateStage::SS_Expired))
	{
		return;
	}

	StateTreeTickScheduler.Remove(StateInstance);
	StateInstanceIndex.RemoveInstance(StateInstance);

	if (StateInstance->GetSourceHandle().IsValid())
	{
		if (UTcsAttributeComponent* OwnerAttrComp = StateInstance->GetOwnerAttributeComponent())
		{
			OwnerAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
		}
		else if (AActor* MutableOwnerActor = GetOwner())
		{
			if (UTcsAttributeComponent* FallbackAttrComp = MutableOwnerActor->FindComponentByClass<UTcsAttributeComponent>())
			{
				FallbackAttrComp->RemoveModifiersBySourceHandle(StateInstance->GetSourceHandle());
			}
		}
	}

	NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Expired);
	NotifyStateRemoved(StateInstance, RemovalReason);

	if (SlotTag.IsValid())
	{
		if (FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag))
		{
			Slot->States.Remove(StateInstance);
			Slot->MarkBuffMergeGroupDirty(StateInstance->GetStateDefId(), ETcsBuffMergeDirtyReason::MembershipChanged);
			Slot->MarkBuffMergeRequiresFullRebuild();
			RequestUpdateStateSlotActivation(SlotTag);
		}
	}

	StateInstance->MarkPendingGC();
}

void UTcsStateComponent::NotifyStateStageChanged(UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (!IsValid(StateInstance) || PreviousStage == NewStage)
	{
		return;
	}

	if (InternalStateStageChangedEvent.IsBound())
	{
		InternalStateStageChangedEvent.Broadcast(this, StateInstance, PreviousStage, NewStage);
	}

	if (OnStateStageChanged.IsBound())
	{
		OnStateStageChanged.Broadcast(this, StateInstance, PreviousStage, NewStage);
	}
}

void UTcsStateComponent::NotifyStateDeactivated(
	UTcsStateInstance* StateInstance,
	ETcsStateStage NewStage,
	FName DeactivateReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateDeactivated.IsBound())
	{
		OnStateDeactivated.Broadcast(this, StateInstance, NewStage, DeactivateReason);
	}
}

void UTcsStateComponent::NotifyStateRemoved(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (InternalStateRemovedEvent.IsBound())
	{
		InternalStateRemovedEvent.Broadcast(this, StateInstance, RemovalReason);
	}

	if (OnStateRemoved.IsBound())
	{
		OnStateRemoved.Broadcast(this, StateInstance, RemovalReason);
	}
}

void UTcsStateComponent::NotifyStateLevelChanged(UTcsStateInstance* StateInstance, int32 OldLevel, int32 NewLevel)
{
	if (!IsValid(StateInstance) || OldLevel == NewLevel)
	{
		return;
	}

	if (OnStateLevelChanged.IsBound())
	{
		OnStateLevelChanged.Broadcast(this, StateInstance, OldLevel, NewLevel);
	}
}

void UTcsStateComponent::NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen)
{
	if (!SlotTag.IsValid())
	{
		return;
	}

	if (InternalSlotGateStateChangedEvent.IsBound())
	{
		InternalSlotGateStateChangedEvent.Broadcast(this, SlotTag, bIsOpen);
	}

	if (OnSlotGateStateChanged.IsBound())
	{
		OnSlotGateStateChanged.Broadcast(this, SlotTag, bIsOpen);
	}
}

void UTcsStateComponent::NotifyStateParameterChanged(
	UTcsStateInstance* StateInstance,
	ETcsStateParameterKeyType KeyType,
	FName ParameterName,
	FGameplayTag ParameterTag,
	ETcsStateParameterType ParameterType)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateParameterChanged.IsBound())
	{
		OnStateParameterChanged.Broadcast(StateInstance, KeyType, ParameterName, ParameterTag, ParameterType);
	}
}

void UTcsStateComponent::NotifyStateApplySuccess(
	AActor* TargetActor,
	FName StateDefId,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag TargetSlot,
	ETcsStateStage AppliedStage)
{
	if (InternalStateApplySuccessEvent.IsBound())
	{
		InternalStateApplySuccessEvent.Broadcast(TargetActor, StateDefId, CreatedStateInstance, TargetSlot, AppliedStage);
	}

	if (OnStateApplySuccess.IsBound())
	{
		OnStateApplySuccess.Broadcast(TargetActor, StateDefId, CreatedStateInstance, TargetSlot, AppliedStage);
	}
}

void UTcsStateComponent::NotifyStateApplyFailed(
	AActor* TargetActor,
	FName StateDefId,
	ETcsStateApplyFailReason FailureReason,
	const FString& FailureMessage)
{
	if (InternalStateApplyFailedEvent.IsBound())
	{
		InternalStateApplyFailedEvent.Broadcast(TargetActor, StateDefId, FailureReason, FailureMessage);
	}

	if (OnStateApplyFailed.IsBound())
	{
		OnStateApplyFailed.Broadcast(TargetActor, StateDefId, FailureReason, FailureMessage);
	}
}

FStateTreeReference UTcsStateComponent::GetStateTreeReference() const
{
	return StateTreeRef;
}

const UStateTree* UTcsStateComponent::GetStateTree() const
{
	return StateTreeRef.GetStateTree();
}

void UTcsStateComponent::OnStateSlotChanged(FGameplayTag SlotTag)
{
	// TODO: StateTree状态槽变化事件处理
	// 这里可以添加状态槽变化的响应逻辑
	// 例如：通知StateTree系统、发送游戏事件等
	
	UE_LOG(LogTcsState, VeryVerbose, TEXT("State slot [%s] changed"), *SlotTag.ToString());
}

FString UTcsStateComponent::GetSlotDebugSnapshot(FGameplayTag SlotFilter) const
{
	auto BuildLine = [this](const FGameplayTag& SlotTag, const FTcsStateSlot& Slot) -> FString
	{
		FString Line = FString::Printf(TEXT("[%s] Gate=%s"),
			*SlotTag.ToString(),
			Slot.bIsGateOpen ? TEXT("Open") : TEXT("Closed"));

		if (const UTcsStateSlotDefinition* SlotDef = Slot.GetStateSlotDef())
		{
			Line += FString::Printf(TEXT(" Mode=%s Preempt=%s"),
				*StaticEnum<ETcsStateSlotActivationMode>()->GetNameStringByValue(static_cast<int64>(SlotDef->ActivationMode)),
				*StaticEnum<ETcsStatePreemptionPolicy>()->GetNameStringByValue(static_cast<int64>(SlotDef->PreemptionPolicy)));
		}

		auto FormatState = [this](UTcsStateInstance* State) -> FString
		{
			if (!IsValid(State))
			{
				return TEXT("<invalid>");
			}

			const FString StateId = State->GetStateDefId().ToString();
			const int32 InstanceId = State->GetInstanceId();
			const UTcsStateDefinition* StateDef = State->GetStateDef();
			const int32 Priority = StateDef ? StateDef->Priority : 0;
			const int32 Level = State->GetLevel();
			const ETcsStateTreeTickPolicy TickPolicy = StateDef ? StateDef->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
			const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));
			int32 StackCount = -1;
			FString DurStr = TEXT("0.00");
			BuildStateDebugOverlay(State, StackCount, DurStr);

			const AActor* Instigator = State->GetInstigator();
			const FString InstigatorName = Instigator ? Instigator->GetName() : TEXT("None");

			return FString::Printf(TEXT("%s#%d(P=%d,Stack=%d,Lv=%d,Dur=%s,Tick=%s,Inst=%s)"),
				*StateId,
				InstanceId,
				Priority,
				StackCount,
				Level,
				*DurStr,
				*TickPolicyStr,
				*InstigatorName);
		};

		auto SortStates = [](TArray<UTcsStateInstance*>& States)
		{
			States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
			{
				const UTcsStateDefinition* AStateDef = A.GetStateDef();
				const UTcsStateDefinition* BStateDef = B.GetStateDef();
				const int32 AP = AStateDef ? AStateDef->Priority : 0;
				const int32 BP = BStateDef ? BStateDef->Priority : 0;
				if (AP != BP)
				{
					return AP > BP;
				}
				return A.GetInstanceId() < B.GetInstanceId();
			});
		};

		TArray<FString> ActiveStates;
		TArray<FString> HangUpStates;
		TArray<FString> PauseStates;
		TArray<FString> StoredStates;

		int32 ActiveCount = 0;
		int32 HangUpCount = 0;
		int32 PauseCount = 0;
		int32 InactiveCount = 0;

		TArray<UTcsStateInstance*> ActiveToSort;
		TArray<UTcsStateInstance*> HangUpToSort;
		TArray<UTcsStateInstance*> PauseToSort;
		TArray<UTcsStateInstance*> StoredToSort;

		for (UTcsStateInstance* State : Slot.States)
		{
			if (!IsValid(State))
			{
				continue;
			}

			switch (State->GetCurrentStage())
			{
			case ETcsStateStage::SS_Active:
				ActiveToSort.Add(State);
				ActiveCount++;
				break;
			case ETcsStateStage::SS_HangUp:
				HangUpToSort.Add(State);
				HangUpCount++;
				break;
			case ETcsStateStage::SS_Pause:
				PauseToSort.Add(State);
				PauseCount++;
				break;
			case ETcsStateStage::SS_Inactive:
				StoredToSort.Add(State);
				InactiveCount++;
				break;
			default:
				StoredToSort.Add(State);
				break;
			}
		}

		SortStates(ActiveToSort);
		SortStates(HangUpToSort);
		SortStates(PauseToSort);
		SortStates(StoredToSort);

		for (UTcsStateInstance* State : ActiveToSort)
		{
			ActiveStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : HangUpToSort)
		{
			HangUpStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : PauseToSort)
		{
			PauseStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : StoredToSort)
		{
			if (!IsValid(State))
			{
				continue;
			}

			const FString StateDesc = FormatState(State);
			if (State->GetCurrentStage() == ETcsStateStage::SS_Inactive)
			{
				StoredStates.Add(StateDesc);
			}
			else
			{
				StoredStates.Add(FString::Printf(TEXT("%s(%s)"),
					*StateDesc,
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage()))));
			}
		}

		Line += FString::Printf(TEXT(" N=%d(A=%d,H=%d,P=%d,I=%d)"),
			Slot.States.Num(),
			ActiveCount,
			HangUpCount,
			PauseCount,
			InactiveCount);

		auto AppendList = [&Line](const TCHAR* Label, const TArray<FString>& Names)
		{
			if (Names.Num() > 0)
			{
				Line += FString::Printf(TEXT(" %s={%s}"), Label, *FString::Join(Names, TEXT(", ")));
			}
		};

		AppendList(TEXT("Active"), ActiveStates);
		AppendList(TEXT("HangUp"), HangUpStates);
		AppendList(TEXT("Pause"), PauseStates);
		AppendList(TEXT("Stored"), StoredStates);

		return Line;
	};

	if (SlotFilter.IsValid())
	{
		if (const FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	TArray<FGameplayTag> SlotTags;
	RuntimeStateSlots.GetKeys(SlotTags);
	SlotTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& Tag : SlotTags)
	{
		const FTcsStateSlot& Slot = RuntimeStateSlots[Tag];
		if (!Accumulator.IsEmpty())
		{
			Accumulator += TEXT("\n");
		}
		Accumulator += BuildLine(Tag, Slot);
	}

	if (Accumulator.IsEmpty())
	{
		Accumulator = TEXT("<no slots>");
	}

	return Accumulator;
}

FString UTcsStateComponent::GetStateDebugSnapshot(FName StateDefIdFilter) const
{
	auto FormatStateLine = [this](const UTcsStateInstance* State) -> FString
	{
		if (!IsValid(State))
		{
			return TEXT("<invalid>");
		}

		const UTcsStateDefinition* StateDef = State->GetStateDef();
		const FGameplayTag SlotTag = StateDef ? StateDef->StateSlotType : FGameplayTag();
		const FTcsStateSlot* Slot = SlotTag.IsValid() ? RuntimeStateSlots.Find(SlotTag) : nullptr;
		const bool bGateOpen = SlotTag.IsValid() ? (Slot && Slot->bIsGateOpen) : true;

		const ETcsStateTreeTickPolicy TickPolicy = StateDef ? StateDef->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
		const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));

		int32 StackCount = -1;
		FString DurStr = TEXT("0.00");
		BuildStateDebugOverlay(State, StackCount, DurStr);

		const AActor* OwnerActor = State->GetOwner();
		const AActor* Instigator = State->GetInstigator();

		const int32 Priority = StateDef ? StateDef->Priority : 0;

		return FString::Printf(TEXT("State=%s Id=%d Slot=%s Gate=%s Stage=%s P=%d Lv=%d Stack=%d Dur=%s Tick=%s Owner=%s Inst=%s"),
			*State->GetStateDefId().ToString(),
			State->GetInstanceId(),
			*SlotTag.ToString(),
			bGateOpen ? TEXT("Open") : TEXT("Closed"),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())),
			Priority,
			State->GetLevel(),
			StackCount,
			*DurStr,
			*TickPolicyStr,
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
			Instigator ? *Instigator->GetName() : TEXT("None"));
	};

	TArray<UTcsStateInstance*> Instances = StateInstanceIndex.Instances;
	Instances.RemoveAll([StateDefIdFilter](const UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}
		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			return true;
		}
		if (!StateDefIdFilter.IsNone() && State->GetStateDefId() != StateDefIdFilter)
		{
			return true;
		}
		return false;
	});

	Instances.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const UTcsStateDefinition* AStateDef = A.GetStateDef();
		const UTcsStateDefinition* BStateDef = B.GetStateDef();

		const FString AS = AStateDef ? AStateDef->StateSlotType.ToString() : TEXT("");
		const FString BS = BStateDef ? BStateDef->StateSlotType.ToString() : TEXT("");
		if (AS != BS)
		{
			return AS < BS;
		}

		const int32 AP = AStateDef ? AStateDef->Priority : 0;
		const int32 BP = BStateDef ? BStateDef->Priority : 0;
		if (AP != BP)
		{
			return AP > BP;
		}

		return A.GetInstanceId() < B.GetInstanceId();
	});

	FString Accumulator = FString::Printf(TEXT("Total=%d Filter=%s"), Instances.Num(), *StateDefIdFilter.ToString());
	for (const UTcsStateInstance* State : Instances)
	{
		Accumulator += TEXT("\n");
		Accumulator += FormatStateLine(State);
	}

	return Accumulator;
}

void UTcsStateComponent::SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen)
{
    FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag);
    if (!Slot)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlot %s"),
        	*FString(__FUNCTION__),
        	*SlotTag.ToString());
        return;
    }

	if (Slot->bIsGateOpen != bOpen)
	{
		Slot->bIsGateOpen = bOpen;
		UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));

		// 广播槽位Gate状态变化事件
		NotifySlotGateStateChanged(SlotTag, bOpen);

		// 请求更新槽位激活状态
		RequestUpdateStateSlotActivation(SlotTag);
	}
}

bool UTcsStateComponent::IsSlotGateOpen(FGameplayTag SlotTag) const
{
    if (const FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotTag))
    {
        return Slot->bIsGateOpen;
    }
    return false;
}

TArray<FName> UTcsStateComponent::GetCurrentActiveStateTreeStates() const
{
	TArray<FName> ActiveStateNames;

	if (!StateTreeRef.IsValid())
	{
		return ActiveStateNames;
	}

	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		return ActiveStateNames;
	}

	// StateTree API 目前只提供 const 访问接口，这里通过 const_cast 获取可写指针以创建 ExecutionContext。
	UTcsStateComponent* MutableThis = const_cast<UTcsStateComponent*>(this);
	FStateTreeInstanceData* MutableInstanceData = &MutableThis->InstanceData;
	FStateTreeExecutionContext Context(*MutableThis, *StateTree, *MutableInstanceData);
	ActiveStateNames = Context.GetActiveStateNames();

	// 如果StateTree没有激活状态，则返回缓存，避免外部逻辑误判为发生变化。
	if (ActiveStateNames.IsEmpty() && !CachedActiveStateNames.IsEmpty())
	{
		ActiveStateNames = CachedActiveStateNames;
	}

	return ActiveStateNames;
}

void UTcsStateComponent::OnStateTreeStateChanged(const FStateTreeExecutionContext& Context)
{
	TGuardValue<bool> StateTreeCallbackGuard(bIsInStateTreeCallback, true);

	// 【关键API】从ExecutionContext获取当前激活状态
	TArray<FName> CurrentActiveStates = Context.GetActiveStateNames();

	// 检测变化
	if (!AreStateNamesEqual(CurrentActiveStates, CachedActiveStateNames))
	{
		RefreshSlotsForStateChange(CurrentActiveStates, CachedActiveStateNames);
		CachedActiveStateNames = CurrentActiveStates;

		UE_LOG(LogTcsState, Log,
			   TEXT("[StateTree Event] State changed: %d active states"),
			   CurrentActiveStates.Num());
	}
}

bool UTcsStateComponent::AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	// StateTree 的激活状态顺序不稳定，但这里仍需保留“同一名称出现次数”这一层语义。
	// 因此不再复制+排序，而是直接比较名称计数表。
	TMap<FName, int32> StateNameCounts;
	StateNameCounts.Reserve(A.Num());
	for (const FName StateName : A)
	{
		++StateNameCounts.FindOrAdd(StateName);
	}

	for (const FName StateName : B)
	{
		int32* Count = StateNameCounts.Find(StateName);
		if (!Count)
		{
			return false;
		}

		--(*Count);
		if (*Count < 0)
		{
			return false;
		}

		if (*Count == 0)
		{
			StateNameCounts.Remove(StateName);
		}
	}

	return StateNameCounts.IsEmpty();
}
