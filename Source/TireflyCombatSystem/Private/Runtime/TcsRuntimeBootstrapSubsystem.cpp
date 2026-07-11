// Copyright Tirefly. All Rights Reserved.

#include "Runtime/TcsRuntimeBootstrapSubsystem.h"

#include "Components/ActorComponent.h"
#include "Engine/GameInstance.h"
#include "Attribute/TcsAttributeComponent.h"
#include "Buff/TcsBuffComponent.h"
#include "Skill/TcsSkillComponent.h"
#include "State/TcsStateComponent.h"
#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsEntityInterface.h"


bool FTcsTrackedEntityRuntimeData::HasAnyTrackedData() const
{
	return bAttributeRegistered
		|| bAttributeReady
		|| bStateRegistered
		|| bStateReady
		|| bBuffRegistered
		|| bBuffReady
		|| bSkillRegistered
		|| bSkillReady;
}

void UTcsRuntimeBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Collection.InitializeDependency<UTcsDefinitionManagerSubsystem>();

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		DefinitionManagerSubsystem = GameInstance->GetSubsystem<UTcsDefinitionManagerSubsystem>();
	}
}

void UTcsRuntimeBootstrapSubsystem::Deinitialize()
{
	PendingEntityRegistrations.Empty();
	RegisteredEntities.Empty();
	TrackedEntityRuntimeData.Empty();
	DefinitionManagerSubsystem = nullptr;

	Super::Deinitialize();
}

ETcsRegisterEntityResult UTcsRuntimeBootstrapSubsystem::RegisterEntity(
	AActor* Entity,
	FTcsOnEntityReadyDynamicDelegate OnReady)
{
	if (!IsRuntimeEntityValid(Entity))
	{
		return ETcsRegisterEntityResult::InvalidEntity;
	}

	const TObjectKey<AActor> EntityKey(Entity);
	if (PendingEntityRegistrations.Contains(EntityKey))
	{
		return ETcsRegisterEntityResult::AlreadyWaiting;
	}

	RegisteredEntities.Add(EntityKey);

	TryAdvanceEntityRuntime(Entity);

	const FTcsEntityRuntimeStateResult RuntimeState = EvaluateEntityRuntimeState(Entity);
	if (RuntimeState.State == ETcsEntityRuntimeState::Ready)
	{
		return ETcsRegisterEntityResult::ReadyNow;
	}

	FTcsPendingEntityRegistration& PendingRegistration = PendingEntityRegistrations.Add(EntityKey);
	PendingRegistration.Entity = Entity;
	PendingRegistration.OnReady = OnReady;

	TryPromoteWaitingEntityToReady(Entity);
	return ETcsRegisterEntityResult::RegisteredWaiting;
}

bool UTcsRuntimeBootstrapSubsystem::IsEntityRegisteredForRuntimeBootstrap(AActor* Entity) const
{
	return IsEntityRegistered(Entity);
}

FTcsEntityRuntimeStateResult UTcsRuntimeBootstrapSubsystem::EvaluateEntityRuntimeState(AActor* Entity) const
{
	FTcsEntityRuntimeStateResult Result;

	if (!IsValid(Entity))
	{
		Result.State = ETcsEntityRuntimeState::Invalid;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::InvalidEntity;
		return Result;
	}

	if (!Entity->GetClass()->ImplementsInterface(UTcsEntityInterface::StaticClass()))
	{
		Result.State = ETcsEntityRuntimeState::Invalid;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::MissingEntityInterface;
		return Result;
	}

	if (!IsEntityRegistered(Entity))
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::NotRegistered;
		return Result;
	}

	if (!DefinitionManagerSubsystem || !DefinitionManagerSubsystem->IsRuntimeReady())
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::DefinitionManagerNotReady;
		return Result;
	}

	UTcsAttributeComponent* AttributeComponent = nullptr;
	UTcsStateComponent* StateComponent = nullptr;
	UTcsBuffComponent* BuffComponent = nullptr;
	UTcsSkillComponent* SkillComponent = nullptr;
	ResolveEntityComponents(Entity, AttributeComponent, StateComponent, BuffComponent, SkillComponent);

	if (!AttributeComponent)
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::MissingAttributeComponent;
		return Result;
	}

	if (!StateComponent)
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::MissingStateComponent;
		return Result;
	}

	if (!AttributeComponent->IsRuntimePrepared())
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::AttributeComponentNotReady;
		return Result;
	}

	if (!StateComponent->IsRuntimeReady())
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::StateComponentNotReady;
		return Result;
	}

	if (BuffComponent && !BuffComponent->IsRuntimeReady())
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::BuffComponentNotReady;
		return Result;
	}

	if (SkillComponent && !SkillComponent->IsRuntimeReady())
	{
		Result.State = ETcsEntityRuntimeState::Waiting;
		Result.BlockReason = ETcsEntityRuntimeBlockReason::SkillComponentNotReady;
		return Result;
	}

	Result.State = ETcsEntityRuntimeState::Ready;
	Result.BlockReason = ETcsEntityRuntimeBlockReason::None;
	return Result;
}

void UTcsRuntimeBootstrapSubsystem::NotifyComponentRegistered(UActorComponent* Component)
{
	AActor* OwnerActor = IsValid(Component) ? Component->GetOwner() : nullptr;
	if (!IsRuntimeEntityValid(OwnerActor))
	{
		return;
	}

	FTcsTrackedEntityRuntimeData& TrackedData = GetOrAddTrackedEntityRuntimeData(OwnerActor);
	if (Cast<UTcsAttributeComponent>(Component))
	{
		TrackedData.bAttributeRegistered = true;
	}
	else if (Cast<UTcsStateComponent>(Component))
	{
		TrackedData.bStateRegistered = true;
	}
	else if (Cast<UTcsBuffComponent>(Component))
	{
		TrackedData.bBuffRegistered = true;
	}
	else if (Cast<UTcsSkillComponent>(Component))
	{
		TrackedData.bSkillRegistered = true;
	}
	else
	{
		return;
	}

	ReevaluateWaitingEntity(OwnerActor);
}

void UTcsRuntimeBootstrapSubsystem::NotifyComponentUnregistered(UActorComponent* Component)
{
	AActor* OwnerActor = IsValid(Component) ? Component->GetOwner() : nullptr;
	if (!IsRuntimeEntityValid(OwnerActor))
	{
		return;
	}

	FTcsTrackedEntityRuntimeData& TrackedData = GetOrAddTrackedEntityRuntimeData(OwnerActor);
	if (Cast<UTcsAttributeComponent>(Component))
	{
		TrackedData.bAttributeRegistered = false;
		TrackedData.bAttributeReady = false;
	}
	else if (Cast<UTcsStateComponent>(Component))
	{
		TrackedData.bStateRegistered = false;
		TrackedData.bStateReady = false;
	}
	else if (Cast<UTcsBuffComponent>(Component))
	{
		TrackedData.bBuffRegistered = false;
		TrackedData.bBuffReady = false;
	}
	else if (Cast<UTcsSkillComponent>(Component))
	{
		TrackedData.bSkillRegistered = false;
		TrackedData.bSkillReady = false;
	}
	else
	{
		return;
	}

	CleanupTrackedEntityRuntimeDataIfEmpty(OwnerActor);
	ReevaluateWaitingEntity(OwnerActor);
}

void UTcsRuntimeBootstrapSubsystem::NotifyComponentRuntimeStateChanged(UActorComponent* Component)
{
	AActor* OwnerActor = IsValid(Component) ? Component->GetOwner() : nullptr;
	if (!IsRuntimeEntityValid(OwnerActor))
	{
		return;
	}

	FTcsTrackedEntityRuntimeData& TrackedData = GetOrAddTrackedEntityRuntimeData(OwnerActor);
	if (UTcsAttributeComponent* AttributeComponent = Cast<UTcsAttributeComponent>(Component))
	{
		TrackedData.bAttributeReady = AttributeComponent->IsRuntimePrepared();
	}
	else if (UTcsStateComponent* StateComponent = Cast<UTcsStateComponent>(Component))
	{
		TrackedData.bStateReady = StateComponent->IsRuntimeReady();
	}
	else if (UTcsBuffComponent* BuffComponent = Cast<UTcsBuffComponent>(Component))
	{
		TrackedData.bBuffReady = BuffComponent->IsRuntimeReady();
	}
	else if (UTcsSkillComponent* SkillComponent = Cast<UTcsSkillComponent>(Component))
	{
		TrackedData.bSkillReady = SkillComponent->IsRuntimeReady();
	}
	else
	{
		return;
	}

	ReevaluateWaitingEntity(OwnerActor);
}

void UTcsRuntimeBootstrapSubsystem::NotifyStateRuntimeReadyChanged(UTcsStateComponent* StateComponent, const bool bIsRuntimeReady)
{
	AActor* OwnerActor = IsValid(StateComponent) ? StateComponent->GetOwner() : nullptr;
	if (!IsRuntimeEntityValid(OwnerActor))
	{
		return;
	}

	FTcsTrackedEntityRuntimeData& TrackedData = GetOrAddTrackedEntityRuntimeData(OwnerActor);
	TrackedData.bStateReady = bIsRuntimeReady;

	UTcsAttributeComponent* AttributeComponent = nullptr;
	UTcsStateComponent* ResolvedStateComponent = nullptr;
	UTcsBuffComponent* BuffComponent = nullptr;
	UTcsSkillComponent* SkillComponent = nullptr;
	ResolveEntityComponents(OwnerActor, AttributeComponent, ResolvedStateComponent, BuffComponent, SkillComponent);
	if (BuffComponent)
	{
		TrackedData.bBuffReady = BuffComponent->PrepareBuffRuntime();
	}
	if (SkillComponent)
	{
		TrackedData.bSkillReady = SkillComponent->PrepareSkillRuntime();
	}

	ReevaluateWaitingEntity(OwnerActor);
}

bool UTcsRuntimeBootstrapSubsystem::IsRuntimeEntityValid(AActor* Entity) const
{
	return IsValid(Entity) && Entity->GetClass()->ImplementsInterface(UTcsEntityInterface::StaticClass());
}

bool UTcsRuntimeBootstrapSubsystem::IsEntityRegistered(AActor* Entity) const
{
	return IsValid(Entity) && RegisteredEntities.Contains(TObjectKey<AActor>(Entity));
}

void UTcsRuntimeBootstrapSubsystem::ResolveEntityComponents(
	AActor* Entity,
	UTcsAttributeComponent*& OutAttributeComponent,
	UTcsStateComponent*& OutStateComponent,
	UTcsBuffComponent*& OutBuffComponent,
	UTcsSkillComponent*& OutSkillComponent) const
{
	OutAttributeComponent = nullptr;
	OutStateComponent = nullptr;
	OutBuffComponent = nullptr;
	OutSkillComponent = nullptr;

	if (!IsRuntimeEntityValid(Entity))
	{
		return;
	}

	OutAttributeComponent = ITcsEntityInterface::Execute_GetAttributeComponent(Entity);
	OutStateComponent = ITcsEntityInterface::Execute_GetStateComponent(Entity);
	OutBuffComponent = ITcsEntityInterface::Execute_GetBuffComponent(Entity);
	OutSkillComponent = ITcsEntityInterface::Execute_GetSkillComponent(Entity);
}

FTcsTrackedEntityRuntimeData& UTcsRuntimeBootstrapSubsystem::GetOrAddTrackedEntityRuntimeData(AActor* Entity)
{
	return TrackedEntityRuntimeData.FindOrAdd(TObjectKey<AActor>(Entity));
}

void UTcsRuntimeBootstrapSubsystem::CleanupTrackedEntityRuntimeDataIfEmpty(AActor* Entity)
{
	if (!IsValid(Entity))
	{
		return;
	}

	const TObjectKey<AActor> EntityKey(Entity);
	const FTcsTrackedEntityRuntimeData* TrackedData = TrackedEntityRuntimeData.Find(EntityKey);
	if (TrackedData && !TrackedData->HasAnyTrackedData())
	{
		TrackedEntityRuntimeData.Remove(EntityKey);
		if (!PendingEntityRegistrations.Contains(EntityKey))
		{
			RegisteredEntities.Remove(EntityKey);
		}
	}
}

void UTcsRuntimeBootstrapSubsystem::TryAdvanceEntityRuntime(AActor* Entity)
{
	if (!IsRuntimeEntityValid(Entity))
	{
		return;
	}

	if (!DefinitionManagerSubsystem || !DefinitionManagerSubsystem->IsRuntimeReady())
	{
		return;
	}

	UTcsAttributeComponent* AttributeComponent = nullptr;
	UTcsStateComponent* StateComponent = nullptr;
	UTcsBuffComponent* BuffComponent = nullptr;
	UTcsSkillComponent* SkillComponent = nullptr;
	ResolveEntityComponents(Entity, AttributeComponent, StateComponent, BuffComponent, SkillComponent);

	FTcsTrackedEntityRuntimeData& TrackedData = GetOrAddTrackedEntityRuntimeData(Entity);

	if (AttributeComponent && !TrackedData.bAttributeReady)
	{
		TrackedData.bAttributeReady = AttributeComponent->PrepareAttributeRuntime();
	}

	if (StateComponent)
	{
		TrackedData.bStateReady = StateComponent->IsRuntimeReady();
		if (!TrackedData.bStateReady)
		{
			const bool bStatePrepared = StateComponent->PrepareStateRuntime();
			if (bStatePrepared)
			{
				TrackedData.bStateReady = StateComponent->IsRuntimeReady();
				if (!TrackedData.bStateReady)
				{
					TrackedData.bStateReady = StateComponent->StartStateRuntime();
				}
			}
		}
	}

	if (TrackedData.bStateReady)
	{
		if (BuffComponent && !TrackedData.bBuffReady)
		{
			TrackedData.bBuffReady = BuffComponent->PrepareBuffRuntime();
		}

		if (SkillComponent && !TrackedData.bSkillReady)
		{
			TrackedData.bSkillReady = SkillComponent->PrepareSkillRuntime();
		}
	}
}

void UTcsRuntimeBootstrapSubsystem::ReevaluateWaitingEntity(AActor* Entity)
{
	if (!IsValid(Entity))
	{
		return;
	}

	if (PendingEntityRegistrations.Contains(TObjectKey<AActor>(Entity)))
	{
		TryPromoteWaitingEntityToReady(Entity);
	}
}

void UTcsRuntimeBootstrapSubsystem::TryPromoteWaitingEntityToReady(AActor* Entity)
{
	if (!IsValid(Entity))
	{
		return;
	}

	TryAdvanceEntityRuntime(Entity);

	const TObjectKey<AActor> EntityKey(Entity);
	FTcsPendingEntityRegistration* PendingRegistration = PendingEntityRegistrations.Find(EntityKey);
	if (!PendingRegistration)
	{
		return;
	}

	const FTcsEntityRuntimeStateResult RuntimeState = EvaluateEntityRuntimeState(Entity);
	if (RuntimeState.State != ETcsEntityRuntimeState::Ready)
	{
		return;
	}

	const FTcsOnEntityReadyDynamicDelegate ReadyDelegate = PendingRegistration->OnReady;
	PendingEntityRegistrations.Remove(EntityKey);

	if (ReadyDelegate.IsBound())
	{
		ReadyDelegate.Execute(Entity);
	}
}
