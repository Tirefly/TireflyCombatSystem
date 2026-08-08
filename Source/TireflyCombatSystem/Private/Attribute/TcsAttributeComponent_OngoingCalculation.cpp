// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "TcsLogChannels.h"



void UTcsAttributeComponent::BroadcastAttributeStateDiffs(
	const TMap<FName, float>& PreviousBaseValues,
	const TMap<FName, float>& PreviousCurrentValues)
{
	if (bIsBroadcastingAttributeStateDiffs)
	{
		return;
	}
	TGuardValue<bool> BroadcastGuard(bIsBroadcastingAttributeStateDiffs, true);
	ActiveBroadcastBaseValues = PreviousBaseValues;
	ActiveBroadcastCurrentValues = PreviousCurrentValues;

	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;

		const float PreviousBase = PreviousBaseValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousBase, Attribute.BaseValue))
		{
			BaseChangePayloads.Emplace(AttributeName, Attribute.BaseValue, PreviousBase, TMap<FTcsSourceHandle, float>());

			float RangeMin = Attribute.BaseValue;
			float RangeMax = Attribute.BaseValue;
			float BoundaryCandidate = Attribute.BaseValue;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute.BaseValue, RangeMin) || FMath::IsNearlyEqual(Attribute.BaseValue, RangeMax))
			{
				const bool bReachedMax = FMath::IsNearlyEqual(Attribute.BaseValue, RangeMax);
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousBase,
					Attribute.BaseValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}

		const float PreviousCurrent = PreviousCurrentValues.FindRef(AttributeName);
		if (!FMath::IsNearlyEqual(PreviousCurrent, Attribute.CurrentValue))
		{
			CurrentChangePayloads.Emplace(AttributeName, Attribute.CurrentValue, PreviousCurrent, TMap<FTcsSourceHandle, float>());

			float RangeMin = Attribute.CurrentValue;
			float RangeMax = Attribute.CurrentValue;
			float BoundaryCandidate = Attribute.CurrentValue;
			ClampAttributeValueInRange(AttributeName, BoundaryCandidate, &RangeMin, &RangeMax);
			if (FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMin) || FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMax))
			{
				const bool bReachedMax = FMath::IsNearlyEqual(Attribute.CurrentValue, RangeMax);
				BoundaryPayloads.Emplace(
					AttributeName,
					bReachedMax,
					PreviousCurrent,
					Attribute.CurrentValue,
					bReachedMax ? RangeMax : RangeMin);
			}
		}
	}

	ActiveBroadcastBaseValues = GetAttributeBaseValues();
	uint64 BroadcastSerial = AttributeStateCommitSerial;
	BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	const bool bChangedDuringBaseEvents = BroadcastSerial != AttributeStateCommitSerial;

	if (!bChangedDuringBaseEvents)
	{
		ActiveBroadcastCurrentValues = GetAttributeValues();
		BroadcastSerial = AttributeStateCommitSerial;
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
		if (BroadcastSerial == AttributeStateCommitSerial)
		{
			BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
		}
	}

	ActiveBroadcastBaseValues.Reset();
	ActiveBroadcastCurrentValues.Reset();
}

void UTcsAttributeComponent::RecalculateAttributeCurrentValues(bool bBroadcastEvents)
{
	if (bIsBroadcastingAttributeStateDiffs)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Reentrant Ongoing recalculation was rejected during Attribute event publication for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return;
	}

	if (!IsRuntimePrepared())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return;
	}

	TMap<FName, float> CandidateBaseValues = GetAttributeBaseValues();
	if (!ClampCandidateAttributeValues(CandidateBaseValues))
	{
		if (AttributeModifiers.IsEmpty())
		{
			UE_LOG(LogTcsAttribute, Fatal, TEXT("[%s] BaseValue/Range configuration cannot produce a legal value for %s"),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this));
		}
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to clamp Attribute BaseValue candidates for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return;
	}

	TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
	TMap<FName, float> CandidateCurrentValues;
	if (!BuildOngoingAttributeValues(
		CandidateBaseValues,
		AttributeModifiers,
		UpdatedModifierInstances,
		CandidateCurrentValues,
		INDEX_NONE,
		nullptr,
		true))
	{
		UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to rebuild Ongoing AttributeModifier values for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return;
	}

	CommitAttributeModifierTransaction(
		CandidateBaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		!AttributeModifiers.IsEmpty(),
		bBroadcastEvents);
}

void UTcsAttributeComponent::RebuildModifierRuntimeCaches()
{
	ModifierInstIdToIndex.Reset();
	SourceHandleIdToModifierInstIds.Reset();
	ReverseDependencyIndex.Reset();
	ModifierInstIdToIndex.Reserve(AttributeModifiers.Num());
	SourceHandleIdToModifierInstIds.Reserve(AttributeModifiers.Num());
	ReverseDependencyIndex.Reserve(AttributeModifiers.Num());

	for (int32 Index = 0; Index < AttributeModifiers.Num(); ++Index)
	{
		const FTcsAttributeModifierInstance& ModifierInstance = AttributeModifiers[Index];
		if (!ModifierInstance.IsValid())
		{
			continue;
		}

		ModifierInstIdToIndex.Add(ModifierInstance.ModifierInstId, Index);
		SourceHandleIdToModifierInstIds.FindOrAdd(ModifierInstance.SourceHandle.Id).Add(ModifierInstance.ModifierInstId);
		for (const FTcsAttributeModifierDependencyRecord& DependencyRecord : ModifierInstance.DependencyRecords)
		{
			DependencyRevisions.FindOrAdd(DependencyRecord.Key) = FMath::Max(
				DependencyRevisions.FindRef(DependencyRecord.Key),
				DependencyRecord.ObservedRevision);
			ReverseDependencyIndex.FindOrAdd(DependencyRecord.Key).Add(ModifierInstance.ModifierInstId);
		}
	}

	for (auto DirtyIt = DirtyOngoingModifierInstIds.CreateIterator(); DirtyIt; ++DirtyIt)
	{
		if (!ModifierInstIdToIndex.Contains(*DirtyIt))
		{
			DirtyIt.RemoveCurrent();
		}
	}
	for (auto RevisionIt = DependencyRevisions.CreateIterator(); RevisionIt; ++RevisionIt)
	{
		if (!ReverseDependencyIndex.Contains(RevisionIt.Key()))
		{
			RevisionIt.RemoveCurrent();
		}
	}
}
