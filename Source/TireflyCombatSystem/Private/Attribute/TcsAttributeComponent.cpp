// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "TcsLogChannels.h"



int32 UTcsAttributeComponent::NextAttributeInstanceId = -1;
int32 UTcsAttributeComponent::NextModifierInstanceId = -1;


UTcsAttributeComponent::UTcsAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTcsAttributeComponent::InitializeComponent()
{
	Super::InitializeComponent();

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = ResolveRuntimeBootstrapSubsystem();
	if (RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRegistered(this);
	}

	if (PrepareAttributeRuntime() && RuntimeBootstrapSubsystem)
	{
		RuntimeBootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
	}
}

void UTcsAttributeComponent::UninitializeComponent()
{
	if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
	{
		BootstrapSubsystem->NotifyComponentUnregistered(this);
	}

	bRuntimePrepared = false;
	RuntimeBootstrapSubsystem = nullptr;

	Super::UninitializeComponent();
}

void UTcsAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (PrepareAttributeRuntime())
	{
		if (UTcsRuntimeBootstrapSubsystem* BootstrapSubsystem = ResolveRuntimeBootstrapSubsystem())
		{
			BootstrapSubsystem->NotifyComponentRuntimeStateChanged(this);
		}
	}
}

UTcsDefinitionManagerSubsystem* UTcsAttributeComponent::ResolveDefinitionManager()
{
	if (!DefinitionMgr)
	{
		if (UWorld* World = GetWorld())
		{
			if (UGameInstance* GameInstance = World->GetGameInstance())
			{
				DefinitionMgr = GameInstance->GetSubsystem<UTcsDefinitionManagerSubsystem>();
			}
		}
		ensureMsgf(DefinitionMgr, TEXT("[%s] Failed to resolve DefinitionManagerSubsystem for %s"),
			*FString(__FUNCTION__), *GetPathName());
	}
	return DefinitionMgr;
}

bool UTcsAttributeComponent::PrepareAttributeRuntime()
{
	// Attribute runtime 按实际查询需要加载 Attribute Definition，不等待无关域的全局预加载批次。
	bRuntimePrepared = ResolveDefinitionManager() != nullptr;
	return bRuntimePrepared;
}

UTcsRuntimeBootstrapSubsystem* UTcsAttributeComponent::ResolveRuntimeBootstrapSubsystem()
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
