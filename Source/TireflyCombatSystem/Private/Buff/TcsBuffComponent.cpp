// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "GameFramework/Actor.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "Misc/ScopeExit.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateManagerSubsystem.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"

void FTcsBuffDurationTracker::Add(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		return;
	}

	TrackedInstances.Add(BuffInstance);
}

void FTcsBuffDurationTracker::Remove(UTcsBuffInstance* BuffInstance)
{
	if (!BuffInstance)
	{
		return;
	}

	TrackedInstances.Remove(BuffInstance);
}

void FTcsBuffDurationTracker::RefreshInstances()
{
	TArray<TObjectPtr<UTcsBuffInstance>> InvalidBuffs;
	for (const TObjectPtr<UTcsBuffInstance>& BuffInstance : TrackedInstances)
	{
		if (!IsValid(BuffInstance) || BuffInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			InvalidBuffs.Add(BuffInstance);
		}
	}

	for (const TObjectPtr<UTcsBuffInstance>& Buff : InvalidBuffs)
	{
		TrackedInstances.Remove(Buff);
	}
}

UTcsBuffComponent::UTcsBuffComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTcsBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwnerStateComponent();
}

void UTcsBuffComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UTcsStateComponent* StateComponent = OwnerStateComponent.Get())
	{
		UnbindOwnerStateEvents(StateComponent);
	}

	Super::EndPlay(EndPlayReason);
}

void UTcsBuffComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickBuffLifecycles(DeltaTime);
}

UTcsBuffComponent* UTcsBuffComponent::GetOrCreateForActor(AActor* OwnerActor)
{
	if (!IsValid(OwnerActor))
	{
		return nullptr;
	}

	UTcsBuffComponent* BuffComponent = OwnerActor->FindComponentByClass<UTcsBuffComponent>();
	if (!BuffComponent)
	{
		BuffComponent = NewObject<UTcsBuffComponent>(OwnerActor, TEXT("TcsBuffComponent"));
		if (BuffComponent)
		{
			OwnerActor->AddOwnedComponent(BuffComponent);
			BuffComponent->RegisterComponent();
		}
	}

	if (BuffComponent)
	{
		if (UTcsStateComponent* StateComponent = OwnerActor->FindComponentByClass<UTcsStateComponent>())
		{
			BuffComponent->InitializeOwnerStateComponent(StateComponent);
		}
	}

	return BuffComponent;
}

UTcsStateComponent* UTcsBuffComponent::GetOwnerStateComponent() const
{
	return ResolveOwnerStateComponent();
}

void UTcsBuffComponent::InitializeOwnerStateComponent(UTcsStateComponent* InStateComponent)
{
	UTcsStateComponent* PreviousStateComponent = OwnerStateComponent.Get();
	if (IsValid(PreviousStateComponent))
	{
		if (PreviousStateComponent == InStateComponent)
		{
			return;
		}

		UnbindOwnerStateEvents(PreviousStateComponent);
	}

	OwnerStateComponent = InStateComponent;
	BindOwnerStateEvents(InStateComponent);
}

float UTcsBuffComponent::GetBuffRemainingDuration(const UTcsBuffInstance* BuffInstance) const
{
	if (IsValid(BuffInstance) && BuffInstance->HasInfiniteDuration())
	{
		return -1.0f;
	}

	if (IsValid(BuffInstance) && BuffInstance->HasFiniteDuration())
	{
		return BuffInstance->RemainingDuration;
	}

	return 0.0f;
}

void UTcsBuffComponent::RefreshBuffRemainingDuration(UTcsBuffInstance* BuffInstance)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	const float NewRemaining = BuffInstance->GetTotalDuration();
	BuffInstance->RemainingDuration = NewRemaining;
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, NewRemaining);
}

void UTcsBuffComponent::SetBuffRemainingDuration(UTcsBuffInstance* BuffInstance, float InDurationRemaining)
{
	if (!IsValid(BuffInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] BuffInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!BuffInstance->GetStateDef() || BuffInstance->HasInfiniteDuration() || !BuffInstance->HasFiniteDuration())
	{
		return;
	}

	BeginPublicEventBatch();
	ON_SCOPE_EXIT
	{
		EndPublicEventBatch();
	};

	BuffInstance->RemainingDuration = FMath::Max(0.0f, InDurationRemaining);
	DurationTracker.Add(BuffInstance);

	NotifyBuffDurationRefreshed(BuffInstance, BuffInstance->RemainingDuration);
}

bool UTcsBuffComponent::GetBuffsInSlot(FGameplayTag SlotTag, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesInSlot;
	if (!StateComponent->GetStatesInSlot(SlotTag, StatesInSlot))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesInSlot)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetBuffsByDefId(FName BuffDefId, TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> StatesByDefId;
	if (!StateComponent->GetStatesByDefId(BuffDefId, StatesByDefId))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : StatesByDefId)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::GetAllActiveBuffs(TArray<UTcsBuffInstance*>& OutBuffs) const
{
	OutBuffs.Reset();

	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	TArray<UTcsStateInstance*> ActiveStates;
	if (!StateComponent->GetAllActiveStates(ActiveStates))
	{
		return false;
	}

	for (UTcsStateInstance* StateInstance : ActiveStates)
	{
		if (UTcsBuffInstance* BuffInstance = ResolveBuffInstance(StateInstance))
		{
			OutBuffs.Add(BuffInstance);
		}
	}

	return OutBuffs.Num() > 0;
}

bool UTcsBuffComponent::HasBuffWithDefId(FName BuffDefId) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsByDefId(BuffDefId, Buffs);
}

bool UTcsBuffComponent::HasActiveBuffInSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsInSlot(SlotTag, Buffs);
}

