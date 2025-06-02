// Copyright Tirefly. All Rights Reserved.


#include "TireflyCombatSystem/Public/Attribute/TireflyAttributeComponent.h"


UTireflyAttributeComponent::UTireflyAttributeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UTireflyAttributeComponent::BeginPlay()
{
	Super::BeginPlay();
}

bool UTireflyAttributeComponent::GetAttributeValue(FName AttributeName, float& OutValue) const
{
	if (const FTireflyAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->CurrentValue;
		return true;
	}

	return false;
}

bool UTireflyAttributeComponent::GetAttributeBaseValue(FName AttributeName, float& OutValue) const
{
	if (const FTireflyAttributeInstance* AttrInst = Attributes.Find(AttributeName))
	{
		OutValue = AttrInst->BaseValue;
		return true;
	}

	return false;
}

TMap<FName, float> UTireflyAttributeComponent::GetAttributeValues() const
{
	TMap<FName, float> AttributeValues;
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.CurrentValue);
	}
	
	return AttributeValues;
}

TMap<FName, float> UTireflyAttributeComponent::GetAttributeBaseValues() const
{
	TMap<FName, float> AttributeValues;
	for (const auto& AttrInst : Attributes)
	{
		AttributeValues.Add(AttrInst.Key, AttrInst.Value.BaseValue);
	}
	
	return AttributeValues;
}

void UTireflyAttributeComponent::BroadcastAttributeValueChangeEvent(
	const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeValueChanged.IsBound())
	{
		OnAttributeValueChanged.Broadcast(Payloads);
	}
}

void UTireflyAttributeComponent::BroadcastAttributeBaseValueChangeEvent(
	const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const
{
	if (!Payloads.IsEmpty() && OnAttributeBaseValueChanged.IsBound())
	{
		OnAttributeBaseValueChanged.Broadcast(Payloads);
	}
}
