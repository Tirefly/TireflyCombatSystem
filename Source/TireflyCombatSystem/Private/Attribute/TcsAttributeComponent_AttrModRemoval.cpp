// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "TcsLogChannels.h"



bool UTcsAttributeComponent::RemoveOngoingModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle)
{
	if (!IsRuntimePrepared() || !SourceHandle.IsValid())
	{
		return false;
	}

	TArray<FTcsAttributeModifierInstance> CandidateModifierInstances;
	CandidateModifierInstances.Reserve(AttributeModifiers.Num());
	for (const FTcsAttributeModifierInstance& ModifierInstance : AttributeModifiers)
	{
		if (ModifierInstance.SourceHandle != SourceHandle)
		{
			CandidateModifierInstances.Add(ModifierInstance);
		}
	}

	if (CandidateModifierInstances.Num() == AttributeModifiers.Num())
	{
		return false;
	}

	const TMap<FName, float> PreviousBaseValues = GetAttributeBaseValues();
	const TMap<FName, float> PreviousCurrentValues = GetAttributeValues();

	TMap<FName, float> BaseValues = PreviousBaseValues;
	if (!ClampCandidateAttributeValues(BaseValues))
	{
		UE_LOG(LogTcsAttribute, Fatal,
			TEXT("[%s] BaseValue/Range configuration cannot produce a legal value for %s during SourceHandle cleanup."),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return false;
	}

	TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
	TMap<FName, float> CandidateCurrentValues;
	if (!BuildOngoingAttributeValues(
		BaseValues,
		CandidateModifierInstances,
		UpdatedModifierInstances,
		CandidateCurrentValues,
		INDEX_NONE,
		nullptr,
		true))
	{
		// Source is already filtered out. Prefer replaying remaining parents' last
		// committed operations over wiping every remaining contribution.
		UE_LOG(LogTcsAttribute, Error,
			TEXT("[%s] SourceHandle cleanup re-evaluation failed for %s; replaying remaining committed Operations."),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));

		UpdatedModifierInstances = CandidateModifierInstances;
		bool bReplaySucceeded = false;
		TArray<FTcsAttributeModifierInstance> MergedModifierInstances;
		if (MergeOngoingModifierInstances(UpdatedModifierInstances, MergedModifierInstances, nullptr))
		{
			CandidateCurrentValues = BaseValues;
			MergedModifierInstances.Sort();
			bReplaySucceeded = true;
			for (const FTcsAttributeModifierInstance& ModifierInstance : MergedModifierInstances)
			{
				if (!ModifierInstance.AppliedOperations.IsEmpty() &&
					!ApplyEvaluatedOperationsToValues(
						ModifierInstance.AppliedOperations,
						CandidateCurrentValues,
						nullptr))
				{
					bReplaySucceeded = false;
					break;
				}
			}
			bReplaySucceeded = bReplaySucceeded && ClampCandidateAttributeValues(CandidateCurrentValues);
		}

		if (!bReplaySucceeded)
		{
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] SourceHandle cleanup replay failed for %s; committing remaining parents with empty contributions and Base CurrentValues."),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this));
			for (FTcsAttributeModifierInstance& ModifierInstance : UpdatedModifierInstances)
			{
				ModifierInstance.AppliedOperations.Reset();
			}
			CandidateCurrentValues = BaseValues;
			if (!ClampCandidateAttributeValues(CandidateCurrentValues))
			{
				UE_LOG(LogTcsAttribute, Fatal,
					TEXT("[%s] BaseValue/Range configuration cannot produce a legal CurrentValue for %s during SourceHandle cleanup fallback."),
					*FString(__FUNCTION__),
					*GetPathNameSafe(this));
				return false;
			}
		}
	}

	TSet<int32> RecalculatedModifierInstIds;
	for (const FTcsAttributeModifierInstance& ModifierInstance : UpdatedModifierInstances)
	{
		RecalculatedModifierInstIds.Add(ModifierInstance.ModifierInstId);
	}

	CommitAttributeModifierTransaction(
		BaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		true,
		true,
		&RecalculatedModifierInstIds);
	return true;
}
