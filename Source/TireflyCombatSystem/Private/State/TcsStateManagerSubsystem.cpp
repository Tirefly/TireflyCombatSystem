// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateManagerSubsystem.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "State/TcsStateComponent.h"



void UTcsStateManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bIsRuntimeReady = false;

	Collection.InitializeDependency<UTcsDefinitionManagerSubsystem>();

	bIsRuntimeReady = true;
}

void UTcsStateManagerSubsystem::Deinitialize()
{
	bIsRuntimeReady = false;
	Super::Deinitialize();
}

bool UTcsStateManagerSubsystem::TryApplyStateToTarget(
	AActor* Target,
	FName StateDefId,
	AActor* Instigator,
	int32 StateLevel,
	const FTcsSourceHandle& ParentSourceHandle)
{
	if (!IsValid(Target) || !IsValid(Instigator) || StateDefId.IsNone())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid target, instigator, or StateDefId."),
			*FString(__FUNCTION__));
		return false;
	}

	UTcsStateComponent* TargetStateCmp = UTcsGenericLibrary::GetStateComponent(Target);
	if (!IsValid(TargetStateCmp))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Target does not have state component: %s"),
			*FString(__FUNCTION__),
			*Target->GetName());
		return false;
	}

	return TargetStateCmp->TryApplyState(StateDefId, Instigator, StateLevel, ParentSourceHandle);
}
