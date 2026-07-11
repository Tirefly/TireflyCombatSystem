// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "TcsLogChannels.h"



int32 UTcsAttributeComponent::NextAttributeInstanceId = -1;
int32 UTcsAttributeComponent::NextModifierInstanceId = -1;
int64 UTcsAttributeComponent::NextModifierChangeBatchId = -1;



namespace
{
	bool LogAttributeRuntimeNotReady_Query(const UTcsAttributeComponent* Component, const TCHAR* FunctionName)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"), FunctionName, *GetPathNameSafe(Component));
		return false;
	}
}


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
	// 直接检查 DefinitionManager 是否 runtime-ready
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			if (UTcsDefinitionManagerSubsystem* DefMgr = GI->GetSubsystem<UTcsDefinitionManagerSubsystem>())
			{
				bRuntimePrepared = DefMgr->IsRuntimeReady();
				return bRuntimePrepared;
			}
		}
	}
	bRuntimePrepared = false;
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

bool UTcsAttributeComponent::GetAttributeValue(FName AttributeName, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->CurrentValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::HasAttributeByTag(const FGameplayTag& AttributeTag) const
{
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return Attributes.Contains(AttributeName);
}

bool UTcsAttributeComponent::GetAttributeValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return GetAttributeValue(AttributeName, OutValue);
}

bool UTcsAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::GetAttributeBaseValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const
{
	OutValue = 0.0f;
	if (!IsRuntimePrepared())
	{
		return LogAttributeRuntimeNotReady_Query(this, TEXT(__FUNCTION__));
	}

	FName AttributeName = const_cast<UTcsAttributeComponent*>(this)->ResolveDefinitionManager()->ResolveAttributeDefIdByTag(AttributeTag);
	if (AttributeName.IsNone())
	{
		return false;
	}

	return GetAttributeBaseValue(AttributeName, OutValue);
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeValues() const
{
	TMap<FName, float> AttributeValues;
	AttributeValues.Reserve(Attributes.Num());
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
	}
	
	return AttributeValues;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeBaseValues() const
{
	TMap<FName, float> AttributeValues;
	AttributeValues.Reserve(Attributes.Num());
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.BaseValue);
	}
	
	return AttributeValues;
}

void UTcsAttributeComponent::BroadcastAttributeValueChangeEvent(
	const TArray<FTcsAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeValueChanged.IsBound())
	{
		OnAttributeValueChanged.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeBaseValueChangeEvent(
	const TArray<FTcsAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeBaseValueChanged.IsBound())
	{
		OnAttributeBaseValueChanged.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierAddedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersAdded.IsBound())
	{
		OnAttributeModifiersAdded.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierRemovedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersRemoved.IsBound())
	{
		OnAttributeModifiersRemoved.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierUpdatedBatchEvent(
	const TArray<FTcsAttributeModifierEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributeModifiersUpdated.IsBound())
	{
		OnAttributeModifiersUpdated.Broadcast(Payloads);
	}
}

void UTcsAttributeComponent::BroadcastAttributeReachedBoundaryBatchEvent(
	const TArray<FTcsAttributeBoundaryEventPayload>& Payloads) const
{
	if (Payloads.IsEmpty())
	{
		return;
	}

	if (OnAttributesReachedBoundary.IsBound())
	{
		OnAttributesReachedBoundary.Broadcast(Payloads);
	}
}
