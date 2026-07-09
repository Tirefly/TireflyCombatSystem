// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "TcsDefinitionManagerSubsystem.h"
#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Runtime/TcsRuntimeBootstrapSubsystem.h"
#include "TcsLogChannels.h"


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
	AttrMgr = nullptr;
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

#if !UE_BUILD_SHIPPING
	// 预热自测断言：GameInstanceSubsystem 在 BeginPlay 之前必然完成 Initialize，
	// 若此处仍为空表明 Subsystem 生命周期被破坏，立即暴露。
	checkf(AttrMgr, TEXT("AttrMgr resolve failed in BeginPlay for %s; GameInstanceSubsystem lifecycle broken."), *GetPathName());
#endif
}

UTcsAttributeManagerSubsystem* UTcsAttributeComponent::ResolveAttributeManager()
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
	UTcsAttributeManagerSubsystem* LocalAttributeManager = ResolveAttributeManager();
	bRuntimePrepared = (LocalAttributeManager != nullptr) && LocalAttributeManager->IsRuntimeReady();
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

	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
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

	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
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

	UTcsAttributeManagerSubsystem* Mgr = const_cast<UTcsAttributeComponent*>(this)->ResolveAttributeManager();
	if (!Mgr)
	{
		return false;
	}

	FName AttributeName;
	if (!Mgr->TryResolveAttributeNameByTag(AttributeTag, AttributeName))
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
