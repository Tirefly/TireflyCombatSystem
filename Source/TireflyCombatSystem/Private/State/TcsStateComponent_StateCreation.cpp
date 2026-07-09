// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateComponent.h"

#include "TcsDefinitionManagerSubsystem.h"
#include "TcsEntityInterface.h"
#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "GameFramework/Actor.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/StateParameter/TcsStateBoolParameter.h"
#include "State/StateParameter/TcsStateNumericParameter.h"
#include "State/StateParameter/TcsStateVectorParameter.h"



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

	UTcsDefinitionManagerSubsystem* DefinitionManager = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UTcsDefinitionManagerSubsystem>()
		: nullptr;
	if (!DefinitionManager)
	{
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			TEXT("Failed to resolve DefinitionManagerSubsystem while creating StateInstance."));
	}

	const UTcsStateDefinition* StateDef = DefinitionManager->GetStateDefinition(StateDefRowId);
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

	TArray<FName> FailedParams;
	if (!StateInstance->PopulateStateParamInstances(StateDef, Instigator, OwnerActor, FailedParams))
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

		StateInstance->MarkPendingGC();
		return ReturnCreateStateFailure(
			ETcsStateApplyFailReason::CreateInstanceFailed,
			FString::Printf(TEXT("Parameter evaluation failed for state '%s'. Failed parameters: [%s]. Owner=%s Instigator=%s"),
				*StateDefRowId.ToString(),
				FailedParamNames.IsEmpty() ? TEXT("Unknown") : *FailedParamNames,
				*OwnerActor->GetName(),
				*Instigator->GetName()));
	}

	return StateInstance;
}
