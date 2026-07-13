// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "State/TcsStateComponent.h"



UTcsSkillComponent::UTcsSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
}

void UTcsSkillComponent::InitializeComponent()
{
	Super::InitializeComponent();

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = ResolveRuntimeBootstrapSubsystem();
	if (RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRegistered(this);
	}

	if (PrepareSkillRuntime() && RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
	}
}

void UTcsSkillComponent::UninitializeComponent()
{
	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyComponentUnregistered(this);
	}

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = nullptr;

	Super::UninitializeComponent();
}

void UTcsSkillComponent::OnUnregister()
{
	if (UTcsStateComponent* OwnerStateComponent = GetOwnerStateComponent())
	{
		UnbindOwnerStateLifecycleEvents(OwnerStateComponent);
	}

	Super::OnUnregister();
}

void UTcsSkillComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsRuntimeReady())
	{
		return;
	}

	CooldownTracker.Tick(DeltaTime);
}

bool UTcsSkillComponent::IsRuntimeReady() const
{
	const UTcsStateComponent* StateComponent = GetOwnerStateComponent();
	return bRuntimePrepared && IsValid(StateComponent) && StateComponent->IsRuntimeReady();
}

bool UTcsSkillComponent::PrepareSkillRuntime()
{
	UTcsStateComponent* StateComponent = GetOwnerStateComponent();
	bRuntimePrepared = IsValid(StateComponent) && StateComponent->IsRuntimeReady();
	if (bRuntimePrepared)
	{
		BindOwnerStateLifecycleEvents(StateComponent);
	}
	else if (IsValid(StateComponent))
	{
		UnbindOwnerStateLifecycleEvents(StateComponent);
	}

	return bRuntimePrepared;
}

UTcsRuntimeBootstrapSubsystem* UTcsSkillComponent::ResolveRuntimeBootstrapSubsystem()
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

int32 UTcsSkillComponent::AllocateSkillModifierRuntimeId()
{
	++NextSkillModifierRuntimeId;
	return NextSkillModifierRuntimeId;
}


UTcsStateComponent* UTcsSkillComponent::GetOwnerStateComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	return Owner->FindComponentByClass<UTcsStateComponent>();
}
