// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeComponent.h"



UTcsAttributeComponent::UTcsAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTcsAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
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
