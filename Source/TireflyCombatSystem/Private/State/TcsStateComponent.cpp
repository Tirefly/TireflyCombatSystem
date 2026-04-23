// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "Attribute/TcsAttributeComponent.h"
#include "TcsLogChannels.h"
#include "TcsEntityInterface.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateSlotDefinitionAsset.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"
#include "State/TcsStateDefinitionAsset.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"


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

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateActiveStateDurations(DeltaTime);
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

		const UTcsStateDefinitionAsset* StateDefAsset = RunningState->GetStateDefAsset();
		const bool bShouldTick = (RunningState->GetCurrentStage() == ETcsStateStage::SS_Active) &&
    								(StateDefAsset && (StateDefAsset->TickPolicy == ETcsStateTreeTickPolicy::WhileActive));

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

float UTcsStateComponent::GetStateRemainingDuration(const UTcsStateInstance* StateInstance) const
{
	if (IsValid(StateInstance))
	{
		const UTcsStateDefinitionAsset* StateDefAsset = StateInstance->GetStateDefAsset();
		if (StateDefAsset && StateDefAsset->DurationType == ETcsStateDurationType::SDT_Infinite)
		{
			return -1.0f;
		}
	}

	float Remaining = 0.f;
	if (DurationTracker.GetRemaining(StateInstance, Remaining))
	{
		return Remaining;
	}

	if (IsValid(StateInstance))
	{
		const UTcsStateDefinitionAsset* StateDefAsset = StateInstance->GetStateDefAsset();
		if (StateDefAsset && StateDefAsset->DurationType == ETcsStateDurationType::SDT_Duration)
		{
			return StateInstance->GetTotalDuration();
		}
	}

	return 0.0f;
}

void UTcsStateComponent::RefreshStateRemainingDuration(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	const UTcsStateDefinitionAsset* StateDefAsset = StateInstance->GetStateDefAsset();
	if (!StateDefAsset)
	{
		return;
	}

	if (StateDefAsset->DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		// Infinite duration state has no remaining duration to refresh.
		return;
	}

	if (StateDefAsset->DurationType != ETcsStateDurationType::SDT_Duration)
	{
		return;
	}

	// 刷新为总持续时间
	const float NewRemaining = StateInstance->GetTotalDuration();
	if (!DurationTracker.SetRemaining(StateInstance, NewRemaining))
	{
		DurationTracker.Add(StateInstance, NewRemaining);
	}

	// 广播状态持续时间刷新事件
	NotifyStateDurationRefreshed(StateInstance, NewRemaining);
}

void UTcsStateComponent::SetStateRemainingDuration(UTcsStateInstance* StateInstance, float InDurationRemaining)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	const UTcsStateDefinitionAsset* StateDefAsset = StateInstance->GetStateDefAsset();
	if (!StateDefAsset)
	{
		return;
	}

	if (StateDefAsset->DurationType == ETcsStateDurationType::SDT_Infinite)
	{
		// Infinite duration state ignores manual duration changes.
		return;
	}

	if (StateDefAsset->DurationType != ETcsStateDurationType::SDT_Duration)
	{
		return;
	}

	// 设置新的剩余时间
	if (!DurationTracker.SetRemaining(StateInstance, InDurationRemaining))
	{
		DurationTracker.Add(StateInstance, InDurationRemaining);
	}

	// 广播状态持续时间刷新事件
	NotifyStateDurationRefreshed(StateInstance, InDurationRemaining);
}

void UTcsStateComponent::UpdateActiveStateDurations(float DeltaTime)
{
	// 收集过期状态，避免在遍历过程中修改容器
	TArray<UTcsStateInstance*> ExpiredStates;
	TArray<UTcsStateInstance*> InvalidStates;

	for (auto& DurationPair : DurationTracker.RemainingByInstance)
	{
		UTcsStateInstance* StateInstance = DurationPair.Key;
		float& RemainingDuration = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		// 确认状态当前的状态阶段
		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();

		// SS_Expired 状态已经过期，跳过
		if (CurrentStage == ETcsStateStage::SS_Expired)
		{
			InvalidStates.Add(StateInstance);
			continue;
		}

		// 只有 Active 和 HangUp 阶段才计算持续时间
		// Pause 阶段冻结，Inactive 阶段不计时
		if (CurrentStage != ETcsStateStage::SS_Active
			&& CurrentStage != ETcsStateStage::SS_HangUp)
		{
			continue;
		}

		// If already expired by duration, request expire once and keep remaining at 0 until finalized.
		if (RemainingDuration <= 0.0f)
		{
    		ExpiredStates.Add(StateInstance);
    		continue;
		}

		RemainingDuration = FMath::Max(0.0f, RemainingDuration - DeltaTime);
		if (RemainingDuration <= 0.0f)
		{
    		ExpiredStates.Add(StateInstance);
		}
	}

	// 遍历结束后，统一处理过期状态
	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (IsValid(ExpiredState))
		{
			ExpireState(ExpiredState);
			if (IsBeingDestroyed() || !IsValid(GetOwner()))
			{
				return;
			}
		}
		else
		{
			// 无效指针：直接从 Map 移除
			InvalidStates.Add(ExpiredState);
		}
	}

	for (UTcsStateInstance* InvalidState : InvalidStates)
	{
		DurationTracker.Remove(InvalidState);
	}
}

bool UTcsStateComponent::TryApplyState(
	FName StateDefId,
	AActor* Instigator,
	int32 StateLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(Instigator) || StateDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid input to apply state. Owner=%s State=%s Instigator=%s"),
			*FString(__FUNCTION__),
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
			*StateDefId.ToString(),
			Instigator ? *Instigator->GetName() : TEXT("None"));

		if (IsValid(OwnerActor) && !StateDefId.IsNone())
		{
			NotifyStateApplyFailed(
				OwnerActor,
				StateDefId,
				ETcsStateApplyFailReason::InvalidInput,
				TEXT("Invalid input while applying state."));
		}
		return false;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		NotifyStateApplyFailed(
			OwnerActor,
			StateDefId,
			ETcsStateApplyFailReason::InvalidInput,
			TEXT("Failed to resolve StateManagerSubsystem."));
		return false;
	}

	const UTcsStateDefinitionAsset* StateDef = LocalStateMgr->GetStateDefinitionAsset(StateDefId);
	if (!StateDef)
	{
		NotifyStateApplyFailed(
			OwnerActor,
			StateDefId,
			ETcsStateApplyFailReason::InvalidStateDefinition,
			TEXT("Invalid state definition."));
		return false;
	}

	UTcsStateInstance* NewStateInstance = CreateStateInstance(StateDefId, Instigator, StateLevel, ParentSourceHandle);
	if (!IsValid(NewStateInstance))
	{
		NotifyStateApplyFailed(
			OwnerActor,
			StateDefId,
			ETcsStateApplyFailReason::CreateInstanceFailed,
			TEXT("Failed to create StateInstance."));
		return false;
	}

	return TryApplyStateInstance(NewStateInstance);
}

UTcsStateInstance* UTcsStateComponent::CreateStateInstance(
	FName StateDefRowId,
	AActor* Instigator,
	int32 InLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !IsValid(Instigator))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid owner or instigator to create state instance %s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString());
		return nullptr;
	}

	if (!OwnerActor->Implements<UTcsEntityInterface>() || !Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[%s] Owner or Instigator does not implement TcsEntityInterface. StateDef=%s Owner=%s Instigator=%s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString(),
			*OwnerActor->GetName(),
			*Instigator->GetName());
		return nullptr;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return nullptr;
	}

	const UTcsStateDefinitionAsset* StateDefAsset = LocalStateMgr->GetStateDefinitionAsset(StateDefRowId);
	if (!StateDefAsset)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid state definition: %s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString());
		return nullptr;
	}

	UTcsStateInstance* TempStateInstance = NewObject<UTcsStateInstance>(OwnerActor);
	if (!IsValid(TempStateInstance))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to create temporary state instance for parameter validation. StateDef=%s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString());
		return nullptr;
	}

	TempStateInstance->SetStateDefId(StateDefRowId);
	TempStateInstance->Initialize(
		StateDefAsset,
		StateDefRowId,
		OwnerActor,
		Instigator,
		LocalStateMgr->AllocateStateInstanceId(),
		InLevel);

	if (!TempStateInstance->IsInitialized())
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[%s] Failed to initialize temporary StateInstance for parameter validation. StateDef=%s Owner=%s Instigator=%s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString(),
			*OwnerActor->GetName(),
			*Instigator->GetName());
		TempStateInstance->MarkPendingGC();
		return nullptr;
	}

	TArray<FName> FailedParams;
	if (!EvaluateAndApplyStateParameters(StateDefAsset, Instigator, TempStateInstance, FailedParams))
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

		UE_LOG(LogTcsState, Error,
			TEXT("[%s] Parameter evaluation failed for state '%s'. Failed parameters: [%s]. Owner=%s Instigator=%s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString(),
			*FailedParamNames,
			*OwnerActor->GetName(),
			*Instigator->GetName());

		TempStateInstance->MarkPendingGC();
		return nullptr;
	}

	UTcsStateInstance* StateInstance = TempStateInstance;
	StateInstance->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());

	TArray<FPrimaryAssetId> NewCausalityChain = ParentSourceHandle.CausalityChain;
	if (ParentSourceHandle.IsValid())
	{
		NewCausalityChain.Add(StateDefAsset->GetPrimaryAssetId());
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
	const UTcsStateDefinitionAsset* StateDefAsset,
	AActor* Instigator,
	UTcsStateInstance* StateInstance,
	TArray<FName>& OutFailedParams)
{
	OutFailedParams.Reset();

	if (!StateDefAsset)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateDefAsset is null during parameter evaluation"), *FString(__FUNCTION__));
		return false;
	}

	if (StateDefAsset->Parameters.IsEmpty() && StateDefAsset->TagParameters.IsEmpty())
	{
		return true;
	}

	AActor* OwnerActor = GetOwner();
	bool bAllSuccess = true;

	for (const TPair<FName, FTcsStateParameter>& ParamPair : StateDefAsset->Parameters)
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

	for (const TPair<FGameplayTag, FTcsStateParameter>& ParamPair : StateDefAsset->TagParameters)
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

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset"), *FString(__FUNCTION__));
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

	Mapping_StateSlotToStateHandle.Empty();
	Mapping_StateHandleToStateSlot.Empty();

	const TArray<FName> SlotDefIds = LocalStateMgr->GetAllStateSlotDefNames();
	for (const FName& SlotDefId : SlotDefIds)
	{
		const UTcsStateSlotDefinitionAsset* SlotDefAsset = LocalStateMgr->GetStateSlotDefinitionAsset(SlotDefId);
		if (SlotDefAsset && SlotDefAsset->SlotTag.IsValid())
		{
			StateSlotsX.FindOrAdd(SlotDefAsset->SlotTag);
		}
	}

	const UStateTree* StateTree = GetStateTree();
	if (!IsValid(StateTree))
	{
		UE_LOG(LogTcsState, Verbose, TEXT("[%s] No StateTree assigned on StateComponent of %s"),
			*FString(__FUNCTION__),
			*OwnerActor->GetName());
		return;
	}

	for (const FName& SlotDefId : SlotDefIds)
	{
		const UTcsStateSlotDefinitionAsset* StateSlotDef = LocalStateMgr->GetStateSlotDefinitionAsset(SlotDefId);
		if (!StateSlotDef)
		{
			continue;
		}

		const FGameplayTag StateSlotTag = StateSlotDef->SlotTag;
		if (StateSlotDef->StateTreeStateName.IsNone())
		{
			continue;
		}

		bool bMapped = false;
		const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
		for (int32 Index = 0; Index < States.Num(); ++Index)
		{
			const FCompactStateTreeState& State = States[Index];
			if (State.Name == StateSlotDef->StateTreeStateName)
			{
				FStateTreeStateHandle Handle(Index);
				Mapping_StateSlotToStateHandle.Add(StateSlotTag, Handle);
				Mapping_StateHandleToStateSlot.Add(Handle, StateSlotTag);
				StateSlotsX.FindOrAdd(StateSlotTag);
				bMapped = true;
				break;
			}
		}

		UE_LOG(LogTcsState, Log, TEXT("[%s] State Slot [%s] -> StateTree State [%s] %s of %s"),
			*FString(__FUNCTION__),
			*StateSlotTag.ToString(),
			*StateSlotDef->StateTreeStateName.ToString(),
			bMapped ? TEXT("mapped") : TEXT("not found"),
			*OwnerActor->GetName());
	}
}

bool UTcsStateComponent::TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return false;
	}

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset"), *FString(__FUNCTION__));
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

	FTcsStateSlot* StateSlot = StateSlotsX.Find(StateDef->StateSlotType);
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

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlotDefinition,
			TEXT("Failed to resolve StateManagerSubsystem."));
		return false;
	}

	const UTcsStateSlotDefinitionAsset* StateSlotDef = LocalStateMgr->GetStateSlotDefinitionAssetByTag(StateDef->StateSlotType);
	if (!StateSlotDef)
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
			const UTcsStateDefinitionAsset* ExistingStateDef = Existing->GetStateDefAsset();
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

	if (StateDef->DurationType != ETcsStateDurationType::SDT_None)
	{
		if (StateDef->DurationType == ETcsStateDurationType::SDT_Duration)
		{
			DurationTracker.Add(StateInstance, StateInstance->GetTotalDuration());
		}
	}

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
	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return;
	}

	TSet<FName> AddedStates(NewStates);
	for (const FName& OldState : OldStates)
	{
		AddedStates.Remove(OldState);
	}

	TSet<FName> RemovedStates(OldStates);
	for (const FName& NewState : NewStates)
	{
		RemovedStates.Remove(NewState);
	}

	for (const auto& Pair : Mapping_StateSlotToStateHandle)
	{
		const FGameplayTag SlotTag = Pair.Key;
		const UTcsStateSlotDefinitionAsset* SlotDef = LocalStateMgr->GetStateSlotDefinitionAssetByTag(SlotTag);
		if (!SlotDef)
		{
			continue;
		}

		const FName& MappedStateName = SlotDef->StateTreeStateName;
		bool bShouldOpen = false;
		if (AddedStates.Contains(MappedStateName))
		{
			bShouldOpen = true;
		}
		else if (RemovedStates.Contains(MappedStateName))
		{
			bShouldOpen = false;
		}
		else
		{
			bShouldOpen = NewStates.Contains(MappedStateName);
		}

		const bool bWasOpen = IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			SetSlotGateOpen(SlotTag, bShouldOpen);
			UE_LOG(LogTcsState, Log,
				TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				*SlotTag.ToString(),
				bShouldOpen ? TEXT("opened") : TEXT("closed"),
				*MappedStateName.ToString());
		}
	}
}

void UTcsStateComponent::RequestUpdateStateSlotActivation(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateSlotTag"), *FString(__FUNCTION__));
		return;
	}

	if (bIsUpdatingSlotActivation)
	{
		PendingSlotActivationUpdates.Add(SlotTag);
		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deferred slot activation update: Component=%s Slot=%s"),
			*FString(__FUNCTION__),
			*GetNameSafe(GetOwner()),
			*SlotTag.ToString());
		return;
	}

	UpdateStateSlotActivation(SlotTag);
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
			if (StateSlotsX.Contains(SlotTag))
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
		else if (FTcsStateSlot* StateSlot = StateSlotsX.Find(StateSlotTag))
		{
			ClearStateSlotExpiredStates(StateSlot);
			SortStatesByPriority(StateSlot->States);
			ProcessStateSlotMerging(StateSlot);
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

	DrainPendingSlotActivationUpdates();
}

void UTcsStateComponent::EnforceSlotGateConsistency(FGameplayTag StateSlotTag)
{
	if (!StateSlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateSlotsX.Find(StateSlotTag);
	if (!StateSlot || StateSlot->bIsGateOpen)
	{
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return;
	}

	const UTcsStateSlotDefinitionAsset* SlotDef = LocalStateMgr->GetStateSlotDefinitionAssetByTag(StateSlotTag);
	if (!SlotDef)
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
			DurationTracker.Remove(State);
			StateInstanceIndex.RemoveInstance(State);
			return true;
		}

		return false;
	});

	StateTreeTickScheduler.RefreshInstances();
	DurationTracker.RefreshInstances();
	StateInstanceIndex.RefreshInstances();
}

void UTcsStateComponent::SortStatesByPriority(TArray<UTcsStateInstance*>& States)
{
	States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const UTcsStateDefinitionAsset* AStateDef = A.GetStateDefAsset();
		const UTcsStateDefinitionAsset* BStateDef = B.GetStateDefAsset();
		if (!AStateDef || !BStateDef)
		{
			return false;
		}
		return AStateDef->Priority > BStateDef->Priority;
	});
}

void UTcsStateComponent::ProcessStateSlotMerging(FTcsStateSlot* StateSlot)
{
	if (!StateSlot)
	{
		return;
	}

	TMap<FName, TArray<UTcsStateInstance*>> StatesByDefId;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State))
		{
			StatesByDefId.FindOrAdd(State->GetStateDefId()).Add(State);
		}
	}

	TArray<UTcsStateInstance*> AllMergedStates;
	TMap<FName, UTcsStateInstance*> MergePrimaryByDefId;
	for (auto& Pair : StatesByDefId)
	{
		TArray<UTcsStateInstance*> MergedGroup;
		MergeStateGroup(Pair.Value, MergedGroup);
		AllMergedStates.Append(MergedGroup);
		if (MergedGroup.Num() > 0 && IsValid(MergedGroup[0]))
		{
			MergePrimaryByDefId.Add(Pair.Key, MergedGroup[0]);
		}
	}

	RemoveUnmergedStates(StateSlot, AllMergedStates, MergePrimaryByDefId);
}

void UTcsStateComponent::MergeStateGroup(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& OutMergedStates)
{
	if (StatesToMerge.Num() == 0)
	{
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		OutMergedStates = StatesToMerge;
		return;
	}

	const UTcsStateDefinitionAsset* StateDef = LocalStateMgr->GetStateDefinitionAsset(StatesToMerge[0]->GetStateDefId());
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get state definition for %s"),
			*FString(__FUNCTION__),
			*StatesToMerge[0]->GetStateDefId().ToString());
		OutMergedStates = StatesToMerge;
		return;
	}

	TSubclassOf<UTcsStateMerger> MergerClass = StateDef->MergerType;
	if (!MergerClass)
	{
		OutMergedStates = StatesToMerge;
		return;
	}

	UTcsStateMerger* Merger = MergerClass->GetDefaultObject<UTcsStateMerger>();
	if (!IsValid(Merger))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get merger instance for %s"),
			*FString(__FUNCTION__),
			*MergerClass->GetName());
		OutMergedStates = StatesToMerge;
		return;
	}

	Merger->Merge(StatesToMerge, OutMergedStates);
}

void UTcsStateComponent::RemoveUnmergedStates(
	FTcsStateSlot* StateSlot,
	const TArray<UTcsStateInstance*>& MergedStates,
	const TMap<FName, UTcsStateInstance*>& MergePrimaryByDefId)
{
	if (!StateSlot)
	{
		return;
	}

	TArray<UTcsStateInstance*> StatesToRemove;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!MergedStates.Contains(State))
		{
			StatesToRemove.Add(State);
		}
	}

	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (!IsValid(State))
		{
			continue;
		}

		UTcsStateInstance* MergeTarget = nullptr;
		for (UTcsStateInstance* Candidate : MergedStates)
		{
			if (!IsValid(Candidate) || Candidate->GetStateDefId() != State->GetStateDefId())
			{
				continue;
			}

			if (Candidate->GetInstigator() == State->GetInstigator())
			{
				MergeTarget = Candidate;
				break;
			}

			if (!MergeTarget)
			{
				MergeTarget = Candidate;
			}
		}

		if (!IsValid(MergeTarget))
		{
			if (UTcsStateInstance* const* Primary = MergePrimaryByDefId.Find(State->GetStateDefId()))
			{
				MergeTarget = IsValid(*Primary) ? *Primary : nullptr;
			}
		}

		if (IsValid(MergeTarget))
		{
			NotifyStateMerged(MergeTarget, State, MergeTarget->GetStackCount());
		}

		RequestStateRemoval(State, TcsStateRemovalReasons::MergedOut);
	}
}

void UTcsStateComponent::ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag)
{
	if (!StateSlot)
	{
		return;
	}

	UTcsStateManagerSubsystem* LocalStateMgr = ResolveStateManager();
	if (!LocalStateMgr)
	{
		return;
	}

	const UTcsStateSlotDefinitionAsset* SlotDef = LocalStateMgr->GetStateSlotDefinitionAssetByTag(SlotTag);
	if (!SlotDef)
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

void UTcsStateComponent::ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinitionAsset* SlotDef)
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
			const UTcsStateDefinitionAsset* CandidateStateDef = Candidate->GetStateDefAsset();
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
			const UTcsStateDefinitionAsset* CandidateStateDef = Candidate->GetStateDefAsset();
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

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset: %s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());
		return false;
	}

	const FGameplayTag SlotTag = StateDef->StateSlotType;
	if (!StateSlotsX.Contains(SlotTag))
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

	int32 RemovedCount = 0;

	for (auto& Pair : StateSlotsX)
	{
		FTcsStateSlot& StateSlot = Pair.Value;

		TArray<UTcsStateInstance*> StatesToRemove;
		for (UTcsStateInstance* State : StateSlot.States)
		{
			if (IsValid(State) && State->GetStateDefId() == StateDefId)
			{
				StatesToRemove.Add(State);
				if (!bRemoveAll)
				{
					break;
				}
			}
		}

		for (UTcsStateInstance* State : StatesToRemove)
		{
			RequestStateRemoval(State, TcsStateRemovalReasons::Removed);
			RemovedCount++;
		}

		if (!bRemoveAll && RemovedCount > 0)
		{
			break;
		}
	}

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStatesInSlot(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return 0;
	}

	FTcsStateSlot* StateSlot = StateSlotsX.Find(SlotTag);
	if (!StateSlot)
	{
		return 0;
	}

	int32 RemovedCount = 0;
	const TArray<UTcsStateInstance*> StatesToRemove = StateSlot->States;

	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (IsValid(State))
		{
			RequestStateRemoval(State, TcsStateRemovalReasons::Removed);
			RemovedCount++;
		}
	}

	return RemovedCount;
}

int32 UTcsStateComponent::RemoveAllStates()
{
	ensureMsgf(!IsInStateTreeUpdateContext(), TEXT("[%s] RemoveAllStates called during StateTree update on %s. Prefer frame-boundary reclaim to avoid overlapping callback teardown."),
		*FString(__FUNCTION__),
		*GetPathName());

	TArray<UTcsStateInstance*> StatesToRemove;
	for (auto& Pair : StateSlotsX)
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
	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (RequestStateRemoval(State, TcsStateRemovalReasons::Removed))
		{
			TotalRemoved++;
		}
	}

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

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset: %s"),
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

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset: %s"),
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

	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		return false;
	}

	const FTcsStateSlot* StateSlot = StateSlotsX.Find(StateDef->StateSlotType);
	return StateSlot && StateSlot->States.Contains(StateInstance);
}

void UTcsStateComponent::FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	const UTcsStateDefinitionAsset* StateDef = StateInstance->GetStateDefAsset();
	if (!StateDef)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance has invalid StateDefAsset: %s"),
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
	DurationTracker.Remove(StateInstance);
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
		if (FTcsStateSlot* Slot = StateSlotsX.Find(SlotTag))
		{
			Slot->States.Remove(StateInstance);
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

	if (OnStateRemoved.IsBound())
	{
		OnStateRemoved.Broadcast(this, StateInstance, RemovalReason);
	}
}

void UTcsStateComponent::NotifyStateStackChanged(UTcsStateInstance* StateInstance, int32 OldStackCount, int32 NewStackCount)
{
	if (!IsValid(StateInstance) || OldStackCount == NewStackCount)
	{
		return;
	}

	if (OnStateStackChanged.IsBound())
	{
		OnStateStackChanged.Broadcast(this, StateInstance, OldStackCount, NewStackCount);
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

void UTcsStateComponent::NotifyStateDurationRefreshed(UTcsStateInstance* StateInstance, float NewDuration)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (OnStateDurationRefreshed.IsBound())
	{
		OnStateDurationRefreshed.Broadcast(this, StateInstance, NewDuration);
	}
}

void UTcsStateComponent::NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen)
{
	if (!SlotTag.IsValid())
	{
		return;
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

void UTcsStateComponent::NotifyStateMerged(UTcsStateInstance* TargetStateInstance, UTcsStateInstance* SourceStateInstance, int32 ResultStackCount)
{
	if (!IsValid(TargetStateInstance) || !IsValid(SourceStateInstance))
	{
		return;
	}

	if (OnStateMerged.IsBound())
	{
		OnStateMerged.Broadcast(this, TargetStateInstance, SourceStateInstance, ResultStackCount);
	}
}

void UTcsStateComponent::NotifyStateApplySuccess(
	AActor* TargetActor,
	FName StateDefId,
	UTcsStateInstance* CreatedStateInstance,
	FGameplayTag TargetSlot,
	ETcsStateStage AppliedStage)
{
	if (!IsValid(TargetActor) || StateDefId.IsNone() || !IsValid(CreatedStateInstance) || !TargetSlot.IsValid())
	{
		return;
	}

	const UTcsStateDefinitionAsset* StateDefAsset = CreatedStateInstance->GetStateDefAsset();
	const int32 Priority = StateDefAsset ? StateDefAsset->Priority : 0;
	const ETcsStateTreeTickPolicy TickPolicy = StateDefAsset ? StateDefAsset->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] ApplySuccess: Target=%s State=%s Id=%d Slot=%s Stage=%s P=%d Tick=%s"),
		*FString(__FUNCTION__),
		*TargetActor->GetName(),
		*StateDefId.ToString(),
		CreatedStateInstance->GetInstanceId(),
		*TargetSlot.ToString(),
		*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(AppliedStage)),
		Priority,
		*StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy)));

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
	if (!IsValid(TargetActor) || StateDefId.IsNone())
	{
		return;
	}

	UE_LOG(LogTcsState, Verbose, TEXT("[%s] ApplyFailed: Target=%s State=%s Reason=%s Message=%s"),
		*FString(__FUNCTION__),
		*TargetActor->GetName(),
		*StateDefId.ToString(),
		*StaticEnum<ETcsStateApplyFailReason>()->GetNameStringByValue(static_cast<int64>(FailureReason)),
		*FailureMessage);

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

		if (IsValid(StateMgr))
		{
			const UTcsStateSlotDefinitionAsset* SlotDef = StateMgr->GetStateSlotDefinitionAssetByTag(SlotTag);
			if (SlotDef)
			{
				Line += FString::Printf(TEXT(" Mode=%s Preempt=%s"),
					*StaticEnum<ETcsStateSlotActivationMode>()->GetNameStringByValue(static_cast<int64>(SlotDef->ActivationMode)),
					*StaticEnum<ETcsStatePreemptionPolicy>()->GetNameStringByValue(static_cast<int64>(SlotDef->PreemptionPolicy)));
			}
		}

		auto FormatState = [](UTcsStateInstance* State) -> FString
		{
			if (!IsValid(State))
			{
				return TEXT("<invalid>");
			}

			const FString StateId = State->GetStateDefId().ToString();
			const int32 InstanceId = State->GetInstanceId();
			const UTcsStateDefinitionAsset* StateDefAsset = State->GetStateDefAsset();
			const int32 Priority = StateDefAsset ? StateDefAsset->Priority : 0;
			const int32 StackCount = State->GetStackCount();
			const int32 Level = State->GetLevel();
			const float DurRemaining = State->GetDurationRemaining();
			const TEnumAsByte<ETcsStateDurationType> DurType = StateDefAsset ? StateDefAsset->DurationType : TEnumAsByte<ETcsStateDurationType>(ETcsStateDurationType::SDT_None);
			const ETcsStateTreeTickPolicy TickPolicy = StateDefAsset ? StateDefAsset->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
			const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));
			const FString DurStr = (DurType == ETcsStateDurationType::SDT_Infinite || DurRemaining < 0.0f)
				? TEXT("Inf")
				: FString::Printf(TEXT("%.2f"), DurRemaining);

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
				const UTcsStateDefinitionAsset* AStateDef = A.GetStateDefAsset();
				const UTcsStateDefinitionAsset* BStateDef = B.GetStateDefAsset();
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
		if (const FTcsStateSlot* Slot = StateSlotsX.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	TArray<FGameplayTag> SlotTags;
	StateSlotsX.GetKeys(SlotTags);
	SlotTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& Tag : SlotTags)
	{
		const FTcsStateSlot& Slot = StateSlotsX[Tag];
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

		const UTcsStateDefinitionAsset* StateDefAsset = State->GetStateDefAsset();
		const FGameplayTag SlotTag = StateDefAsset ? StateDefAsset->StateSlotType : FGameplayTag();
		const FTcsStateSlot* Slot = SlotTag.IsValid() ? StateSlotsX.Find(SlotTag) : nullptr;
		const bool bGateOpen = SlotTag.IsValid() ? (Slot && Slot->bIsGateOpen) : true;

		const ETcsStateTreeTickPolicy TickPolicy = StateDefAsset ? StateDefAsset->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
		const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));

		const TEnumAsByte<ETcsStateDurationType> DurType = StateDefAsset ? StateDefAsset->DurationType : TEnumAsByte<ETcsStateDurationType>(ETcsStateDurationType::SDT_None);
		const float DurRemaining = State->GetDurationRemaining();
		const FString DurStr = (DurType == ETcsStateDurationType::SDT_Infinite || DurRemaining < 0.0f)
			? TEXT("Inf")
			: FString::Printf(TEXT("%.2f"), DurRemaining);

		const AActor* OwnerActor = State->GetOwner();
		const AActor* Instigator = State->GetInstigator();

		const int32 Priority = StateDefAsset ? StateDefAsset->Priority : 0;

		return FString::Printf(TEXT("State=%s Id=%d Slot=%s Gate=%s Stage=%s P=%d Lv=%d Stack=%d Dur=%s Tick=%s Owner=%s Inst=%s"),
			*State->GetStateDefId().ToString(),
			State->GetInstanceId(),
			*SlotTag.ToString(),
			bGateOpen ? TEXT("Open") : TEXT("Closed"),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())),
			Priority,
			State->GetLevel(),
			State->GetStackCount(),
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
		const UTcsStateDefinitionAsset* AStateDef = A.GetStateDefAsset();
		const UTcsStateDefinitionAsset* BStateDef = B.GetStateDefAsset();

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
    FTcsStateSlot* Slot = StateSlotsX.Find(SlotTag);
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
    if (const FTcsStateSlot* Slot = StateSlotsX.Find(SlotTag))
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

	// Treat arrays as sets to avoid false positives due to unstable ordering from StateTree.
	TArray<FName> SortedA = A;
	TArray<FName> SortedB = B;
	SortedA.Sort([](const FName& L, const FName& R) { return L.LexicalLess(R); });
	SortedB.Sort([](const FName& L, const FName& R) { return L.LexicalLess(R); });

	for (int32 i = 0; i < SortedA.Num(); ++i)
	{
		if (SortedA[i] != SortedB[i])
		{
			return false;
		}
	}

	return true;
}
