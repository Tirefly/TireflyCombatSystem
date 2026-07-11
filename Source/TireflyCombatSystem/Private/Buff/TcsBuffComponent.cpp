// Copyright Tirefly. All Rights Reserved.

#include "Buff/TcsBuffComponent.h"

#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Buff/BuffMerger/TcsBuffMerger.h"
#include "Engine/World.h"
#include "Buff/TcsBuffDefinition.h"
#include "Buff/TcsBuffInstance.h"
#include "Misc/ScopeExit.h"
#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"


namespace
{
	bool LogBuffRuntimeNotReady_Component(const UTcsBuffComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Buff runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
	}

	void LogBuffRuntimeNotReadyVoid_Component(const UTcsBuffComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Buff runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
	}

	float LogBuffRuntimeNotReadyFloat_Component(const UTcsBuffComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Buff runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return 0.0f;
	}
}



UTcsBuffComponent::UTcsBuffComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTcsBuffComponent::InitializeComponent()
{
	Super::InitializeComponent();

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = ResolveRuntimeBootstrapSubsystem();
	if (RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRegistered(this);
	}

	if (PrepareBuffRuntime() && RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
	}
}

void UTcsBuffComponent::UninitializeComponent()
{
	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyComponentUnregistered(this);
	}

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = nullptr;
	OwnerStateComponent.Reset();

	Super::UninitializeComponent();
}

void UTcsBuffComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveOwnerStateComponent();
	if (PrepareBuffRuntime())
	{
		if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
		{
			BootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
		}
	}
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
	if (!IsRuntimeReady())
	{
		return;
	}

	TickBuffLifecycles(DeltaTime);
}

bool UTcsBuffComponent::IsRuntimeReady() const
{
	const UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	return bRuntimePrepared && IsValid(StateComponent) && StateComponent->IsRuntimeReady();
}

bool UTcsBuffComponent::PrepareBuffRuntime()
{
	UTcsStateComponent* StateComponent = ResolveOwnerStateComponent();
	bRuntimePrepared = IsValid(StateComponent) && StateComponent->IsRuntimeReady();
	if (bRuntimePrepared)
	{
		BindOwnerStateEvents(StateComponent);
	}
	else if (IsValid(StateComponent))
	{
		UnbindOwnerStateEvents(StateComponent);
	}

	return bRuntimePrepared;
}

UTcsRuntimeBootstrapSubsystem* UTcsBuffComponent::ResolveRuntimeBootstrapSubsystem()
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
	if (bRuntimePrepared && IsValid(InStateComponent) && InStateComponent->IsRuntimeReady())
	{
		BindOwnerStateEvents(InStateComponent);
	}
}

float UTcsBuffComponent::GetBuffRemainingDuration(const UTcsBuffInstance* BuffInstance) const
{
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReadyFloat_Component(this, TEXT(__FUNCTION__));
	}

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
	if (!IsRuntimeReady())
	{
		LogBuffRuntimeNotReadyVoid_Component(this, TEXT(__FUNCTION__));
		return;
	}

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
	if (!IsRuntimeReady())
	{
		LogBuffRuntimeNotReadyVoid_Component(this, TEXT(__FUNCTION__));
		return;
	}

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
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Component(this, TEXT(__FUNCTION__));
	}

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
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Component(this, TEXT(__FUNCTION__));
	}

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
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Component(this, TEXT(__FUNCTION__));
	}

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
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Component(this, TEXT(__FUNCTION__));
	}

	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsByDefId(BuffDefId, Buffs);
}

bool UTcsBuffComponent::HasActiveBuffInSlot(FGameplayTag SlotTag) const
{
	if (!IsRuntimeReady())
	{
		return LogBuffRuntimeNotReady_Component(this, TEXT(__FUNCTION__));
	}

	TArray<UTcsBuffInstance*> Buffs;
	return GetBuffsInSlot(SlotTag, Buffs);
}

