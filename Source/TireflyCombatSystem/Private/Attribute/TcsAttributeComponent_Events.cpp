// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"



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
