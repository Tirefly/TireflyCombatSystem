// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/AttrModMerger/TcsAttributeModifierMerger.h"
#include "Attribute/TcsAttributeComponent_AttrModHelpers.h"
#include "Attribute/TcsAttributeModifierCompatibility.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "Skill/TcsSkillEntry.h"
#include "State/TcsStateInstance.h"
#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"


bool UTcsAttributeComponent::BuildAttributeEvaluationSnapshot(
	const TMap<FName, float>& BaseValues,
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	int32 ExcludedModifierInstId,
	FTcsAttributeEvaluationSnapshot& OutSnapshot,
	FTcsAttributeModifierApplicationResult* InOutResult)
{
	OutSnapshot.BaseValues = BaseValues;
	OutSnapshot.CurrentValues = BaseValues;

	TArray<FTcsAttributeModifierInstance> SortedModifierInstances = ModifierInstances;
	SortedModifierInstances.Sort();
	for (const FTcsAttributeModifierInstance& ModifierInstance : SortedModifierInstances)
	{
		if (ModifierInstance.ModifierInstId == ExcludedModifierInstId)
		{
			continue;
		}

		if (!ModifierInstance.IsValid())
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}
	}

	TArray<FTcsAttributeModifierInstance> IncludedModifierInstances;
	IncludedModifierInstances.Reserve(SortedModifierInstances.Num());
	for (const FTcsAttributeModifierInstance& ModifierInstance : SortedModifierInstances)
	{
		if (ModifierInstance.ModifierInstId != ExcludedModifierInstId)
		{
			IncludedModifierInstances.Add(ModifierInstance);
		}
	}

	TArray<FTcsAttributeModifierInstance> MergedModifierInstances;
	if (!MergeOngoingModifierInstances(IncludedModifierInstances, MergedModifierInstances, InOutResult))
	{
		return false;
	}

	MergedModifierInstances.Sort();
	for (const FTcsAttributeModifierInstance& ModifierInstance : MergedModifierInstances)
	{
		FTcsAttributeModifierApplicationResult OperationResult;
		if (!ApplyEvaluatedOperationsToValues(
			ModifierInstance.AppliedOperations,
			OutSnapshot.CurrentValues,
			&OperationResult))
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, OperationResult.Failure);
		}
	}

	if (!ClampCandidateAttributeValues(OutSnapshot.BaseValues) ||
		!ClampCandidateAttributeValues(OutSnapshot.CurrentValues))
	{
		return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
	}

	return true;
}

bool UTcsAttributeComponent::BuildOngoingAttributeValues(
	const TMap<FName, float>& BaseValues,
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	TArray<FTcsAttributeModifierInstance>& OutUpdatedModifierInstances,
	TMap<FName, float>& OutCurrentValues,
	int32 AuditedModifierInstId,
	FTcsAttributeModifierApplicationResult* InOutResult)
{
	OutUpdatedModifierInstances = ModifierInstances;

	for (FTcsAttributeModifierInstance& ModifierInstance : OutUpdatedModifierInstances)
	{
		if (!ModifierInstance.IsValid())
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (!ValidateAttributeModifierDefinitionCompatibility(*ModifierInstance.ModifierDef, InOutResult))
		{
			return false;
		}

		FTcsAttributeEvaluationSnapshot Snapshot;
		if (!BuildAttributeEvaluationSnapshot(
			BaseValues,
			ModifierInstances,
			ModifierInstance.ModifierInstId,
			Snapshot,
			InOutResult))
		{
			return false;
		}

		UTcsStateInstance* const SourceStateInstance = ModifierInstance.OwningStateInstance.Get();
		UTcsSkillEntry* const SourceSkillEntry = TcsAttributeModPrivate::GetSourceSkillEntry(SourceStateInstance, nullptr);
		TArray<FTcsEvaluatedAttributeOperation> EvaluatedOperations;
		FTcsAttributeModifierApplicationResult LocalResult;
		FTcsAttributeModifierApplicationResult* const ResultForModifier = ModifierInstance.ModifierInstId == AuditedModifierInstId
			? InOutResult
			: &LocalResult;
		if (!BuildEvaluatedAttributeOperations(
			*ModifierInstance.ModifierDef,
			ModifierInstance.OperationOverrides,
			ModifierInstance.SourceHandle,
			SourceStateInstance,
			SourceSkillEntry,
			Snapshot,
			EvaluatedOperations,
			ResultForModifier))
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ResultForModifier->Failure);
		}

		ModifierInstance.AppliedOperations = MoveTemp(EvaluatedOperations);
		ModifierInstance.UpdateTimestamp = FDateTime::UtcNow().GetTicks();
	}

	TArray<FTcsAttributeModifierInstance> MergedModifierInstances;
	if (!MergeOngoingModifierInstances(OutUpdatedModifierInstances, MergedModifierInstances, InOutResult))
	{
		return false;
	}

	OutCurrentValues = BaseValues;
	MergedModifierInstances.Sort();
	for (const FTcsAttributeModifierInstance& ModifierInstance : MergedModifierInstances)
	{
		if (!ApplyEvaluatedOperationsToValues(
			ModifierInstance.AppliedOperations,
			OutCurrentValues,
			ModifierInstance.ModifierInstId == AuditedModifierInstId ? InOutResult : nullptr))
		{
			return false;
		}
	}

	return ClampCandidateAttributeValues(OutCurrentValues)
		|| TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
}

bool UTcsAttributeComponent::MergeOngoingModifierInstances(
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	TArray<FTcsAttributeModifierInstance>& OutMergedModifierInstances,
	FTcsAttributeModifierApplicationResult* InOutResult) const
{
	OutMergedModifierInstances.Reset();
	if (ModifierInstances.IsEmpty())
	{
		return true;
	}

	TMap<FName, TArray<FTcsAttributeModifierInstance>> InstancesByDefId;
	for (const FTcsAttributeModifierInstance& ModifierInstance : ModifierInstances)
	{
		if (!ModifierInstance.IsValid() || !ModifierInstance.ModifierDef->MergerType)
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		InstancesByDefId.FindOrAdd(ModifierInstance.ModifierDefId).Add(ModifierInstance);
	}

	OutMergedModifierInstances.Reserve(ModifierInstances.Num());
	for (TPair<FName, TArray<FTcsAttributeModifierInstance>>& DefGroup : InstancesByDefId)
	{
		TArray<FTcsAttributeModifierInstance>& GroupInstances = DefGroup.Value;
		if (GroupInstances.IsEmpty())
		{
			continue;
		}

		const UTcsAttributeModifierDefinition* const ModifierDefinition = GroupInstances[0].ModifierDef;
		if (!ModifierDefinition || !ModifierDefinition->MergerType)
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		UTcsAttributeModifierMerger* const MergerCDO =
			ModifierDefinition->MergerType->GetDefaultObject<UTcsAttributeModifierMerger>();
		if (!MergerCDO)
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		TArray<FTcsAttributeModifierInstance> MergedGroup;
		MergerCDO->Merge(GroupInstances, MergedGroup);
		if (MergedGroup.IsEmpty() && !GroupInstances.IsEmpty())
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		OutMergedModifierInstances.Append(MergedGroup);
	}

	return true;
}
