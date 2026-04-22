// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/TcsAttributeManagerSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"



UTcsAttributeComponent::UTcsAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTcsAttributeComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			AttrMgr = GI->GetSubsystem<UTcsAttributeManagerSubsystem>();
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

bool UTcsAttributeComponent::GetAttributeValue(FName AttributeName, float& OutValue) const
{
	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->CurrentValue;
		return true;
	}

	return false;
}

bool UTcsAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	if (const FTcsAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeValues() const
{
	TMap<FName, float> AttributeValues;
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
	}
	
	return AttributeValues;
}

TMap<FName, float> UTcsAttributeComponent::GetAttributeBaseValues() const
{
	TMap<FName, float> AttributeValues;
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

void UTcsAttributeComponent::BroadcastAttributeModifierAddedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierAdded.IsBound())
	{
		OnAttributeModifierAdded.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierRemovedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierRemoved.IsBound())
	{
		OnAttributeModifierRemoved.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeModifierUpdatedEvent(
	const FTcsAttributeModifierInstance& ModifierInstance) const
{
	if (OnAttributeModifierUpdated.IsBound())
	{
		OnAttributeModifierUpdated.Broadcast(ModifierInstance);
	}
}

void UTcsAttributeComponent::BroadcastAttributeReachedBoundaryEvent(
	FName AttributeName,
	bool bIsMaxBoundary,
	float OldValue,
	float NewValue,
	float BoundaryValue) const
{
	if (OnAttributeReachedBoundary.IsBound())
	{
		OnAttributeReachedBoundary.Broadcast(AttributeName, bIsMaxBoundary, OldValue, NewValue, BoundaryValue);
	}
}
