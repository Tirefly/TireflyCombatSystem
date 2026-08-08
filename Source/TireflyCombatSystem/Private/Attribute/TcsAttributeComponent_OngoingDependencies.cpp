// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Buff/TcsBuffInstance.h"



void UTcsAttributeComponent::MarkOngoingModifiersDirty(
	const FTcsAttributeModifierDependencyKey& DependencyKey,
	const TSet<int32>* SuppressedModifierInstIds)
{
	const TSet<int32>* const DependentIds = ReverseDependencyIndex.Find(DependencyKey);
	if (!DependentIds)
	{
		SetComponentTickEnabled(!DirtyOngoingModifierInstIds.IsEmpty());
		return;
	}

	for (const int32 ModifierInstId : *DependentIds)
	{
		if (ModifierInstIdToIndex.Contains(ModifierInstId) &&
			(!SuppressedModifierInstIds || !SuppressedModifierInstIds->Contains(ModifierInstId)))
		{
			DirtyOngoingModifierInstIds.Add(ModifierInstId);
		}
	}

	if (!DirtyOngoingModifierInstIds.IsEmpty())
	{
		SetComponentTickEnabled(true);
	}
}

void UTcsAttributeComponent::MarkAttributeCurrentValueDependencyChanged(
	FName AttributeId,
	const TSet<int32>* SuppressedModifierInstIds)
{
	const FTcsAttributeModifierDependencyKey DependencyKey =
		FTcsAttributeModifierDependencyKey::MakeAttributeCurrentValue(AttributeId);
	if (!ReverseDependencyIndex.Contains(DependencyKey))
	{
		return;
	}
	++DependencyRevisions.FindOrAdd(DependencyKey);
	MarkOngoingModifiersDirty(DependencyKey, SuppressedModifierInstIds);
}

void UTcsAttributeComponent::NotifyLocalBuffNumericStateParamEffectiveValueChanged(
	const UTcsBuffInstance& BuffInstance,
	FGameplayTag ParamTag)
{
	if (!ParamTag.IsValid() || BuffInstance.GetOwnerAttributeComponent() != this)
	{
		return;
	}

	const FTcsAttributeModifierDependencyKey DependencyKey =
		FTcsAttributeModifierDependencyKey::MakeBuffNumericStateParam(BuffInstance, ParamTag);
	if (!ReverseDependencyIndex.Contains(DependencyKey))
	{
		return;
	}
	++DependencyRevisions.FindOrAdd(DependencyKey);
	MarkOngoingModifiersDirty(DependencyKey);
}

bool UTcsAttributeComponent::RequestOngoingModifierRecalculation(
	const FTcsSourceHandle& SourceHandle)
{
	if (!IsRuntimePrepared() || !SourceHandle.IsValid())
	{
		return false;
	}

	const TSet<int32>* const ModifierInstIds = SourceHandleIdToModifierInstIds.Find(SourceHandle.Id);
	if (!ModifierInstIds)
	{
		return false;
	}

	bool bMarkedAny = false;
	for (const int32 ModifierInstId : *ModifierInstIds)
	{
		const int32* const ModifierIndex = ModifierInstIdToIndex.Find(ModifierInstId);
		if (!ModifierIndex || !AttributeModifiers.IsValidIndex(*ModifierIndex) ||
			AttributeModifiers[*ModifierIndex].SourceHandle != SourceHandle)
		{
			continue;
		}

		DirtyOngoingModifierInstIds.Add(ModifierInstId);
		bMarkedAny = true;
	}

	if (bMarkedAny)
	{
		SetComponentTickEnabled(true);
	}
	return bMarkedAny;
}
