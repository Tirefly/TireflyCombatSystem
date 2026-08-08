// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"



void UTcsAttributeComponent::CommitAttributeModifierTransaction(
	const TMap<FName, float>& BaseValues,
	const TMap<FName, float>& CurrentValues,
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	bool bCommitModifierInstances,
	bool bBroadcastEvents,
	const TSet<int32>* RecalculatedModifierInstIds)
{
	const TMap<FName, float> PreviousBaseValues = GetAttributeBaseValues();
	const TMap<FName, float> PreviousCurrentValues = GetAttributeValues();

	for (TPair<FName, FTcsAttributeInstance>& AttributePair : Attributes)
	{
		if (const float* const BaseValue = BaseValues.Find(AttributePair.Key))
		{
			AttributePair.Value.BaseValue = *BaseValue;
		}

		if (const float* const CurrentValue = CurrentValues.Find(AttributePair.Key))
		{
			AttributePair.Value.CurrentValue = *CurrentValue;
		}
	}

	if (bCommitModifierInstances)
	{
		AttributeModifiers = ModifierInstances;
		RebuildModifierRuntimeCaches();
	}

	bool bCommittedStateChanged = bCommitModifierInstances;
	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		bCommittedStateChanged |=
			!FMath::IsNearlyEqual(PreviousBaseValues.FindRef(Pair.Key), Pair.Value.BaseValue) ||
			!FMath::IsNearlyEqual(PreviousCurrentValues.FindRef(Pair.Key), Pair.Value.CurrentValue);
	}
	if (bCommittedStateChanged)
	{
		++AttributeStateCommitSerial;
	}

	TSet<int32> FullRecalculatedModifierInstIds;
	const TSet<int32>* EffectiveRecalculatedModifierInstIds = RecalculatedModifierInstIds;
	if (!EffectiveRecalculatedModifierInstIds && bCommitModifierInstances)
	{
		for (const FTcsAttributeModifierInstance& ModifierInstance : AttributeModifiers)
		{
			FullRecalculatedModifierInstIds.Add(ModifierInstance.ModifierInstId);
		}
		EffectiveRecalculatedModifierInstIds = &FullRecalculatedModifierInstIds;
	}

	if (EffectiveRecalculatedModifierInstIds)
	{
		for (const int32 ModifierInstId : *EffectiveRecalculatedModifierInstIds)
		{
			DirtyOngoingModifierInstIds.Remove(ModifierInstId);
		}
	}

	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		if (!FMath::IsNearlyEqual(PreviousCurrentValues.FindRef(Pair.Key), Pair.Value.CurrentValue))
		{
			MarkAttributeCurrentValueDependencyChanged(Pair.Key, EffectiveRecalculatedModifierInstIds);
		}
	}
	if (EffectiveRecalculatedModifierInstIds)
	{
		for (const int32 ModifierInstId : *EffectiveRecalculatedModifierInstIds)
		{
			const int32* const ModifierIndex = ModifierInstIdToIndex.Find(ModifierInstId);
			if (!ModifierIndex || !AttributeModifiers.IsValidIndex(*ModifierIndex))
			{
				continue;
			}

			for (FTcsAttributeModifierDependencyRecord& DependencyRecord : AttributeModifiers[*ModifierIndex].DependencyRecords)
			{
				DependencyRecord.ObservedRevision = DependencyRevisions.FindRef(DependencyRecord.Key);
			}
		}
	}

	if (!bIsFlushingDirtyOngoingModifiers)
	{
		FlushDirtyOngoingModifiers(&PreviousBaseValues, &PreviousCurrentValues, bBroadcastEvents);
	}
}
