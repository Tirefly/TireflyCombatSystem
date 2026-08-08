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



namespace TcsOngoingContributionPrivate
{
	void ClearAppliedOperations(
		TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		const TSet<int32>& ExcludedModifierInstIds)
	{
		for (FTcsAttributeModifierInstance& ModifierInstance : ModifierInstances)
		{
			if (ExcludedModifierInstIds.Contains(ModifierInstance.ModifierInstId))
			{
				ModifierInstance.AppliedOperations.Reset();
			}
		}
	}

	void CollectDefGroupIds(
		const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		FName ModifierDefId,
		TSet<int32>& InOutIds)
	{
		for (const FTcsAttributeModifierInstance& ModifierInstance : ModifierInstances)
		{
			if (ModifierInstance.ModifierDefId == ModifierDefId)
			{
				InOutIds.Add(ModifierInstance.ModifierInstId);
			}
		}
	}

	void CollectClampAffectedClosureIds(
		const TArray<FTcsAttributeModifierInstance>& ContributingInstances,
		const TMap<FName, TSet<FName>>* RangeDependents,
		TSet<int32>& OutAffectedModifierInstIds)
	{
		OutAffectedModifierInstIds.Reset();
		if (ContributingInstances.IsEmpty())
		{
			return;
		}

		TSet<FName> AffectedAttributes;
		for (const FTcsAttributeModifierInstance& ModifierInstance : ContributingInstances)
		{
			for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
			{
				if (!Operation.TargetAttributeId.IsNone())
				{
					AffectedAttributes.Add(Operation.TargetAttributeId);
				}
			}
		}

		if (RangeDependents && !AffectedAttributes.IsEmpty())
		{
			TArray<FName> PendingAttributes = AffectedAttributes.Array();
			while (!PendingAttributes.IsEmpty())
			{
				const FName AttributeId = PendingAttributes.Pop(EAllowShrinking::No);
				if (const TSet<FName>* const Dependents = RangeDependents->Find(AttributeId))
				{
					for (const FName DependentAttributeId : *Dependents)
					{
						if (!AffectedAttributes.Contains(DependentAttributeId))
						{
							AffectedAttributes.Add(DependentAttributeId);
							PendingAttributes.Add(DependentAttributeId);
						}
					}
				}
			}
		}

		for (const FTcsAttributeModifierInstance& ModifierInstance : ContributingInstances)
		{
			bool bAffected = false;
			for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
			{
				if (AffectedAttributes.Contains(Operation.TargetAttributeId))
				{
					bAffected = true;
					break;
				}
			}

			if (!bAffected)
			{
				for (const FTcsAttributeModifierDependencyRecord& DependencyRecord : ModifierInstance.DependencyRecords)
				{
					if (DependencyRecord.Key.Type == ETcsAttributeModifierDependencyType::AMDT_AttributeCurrentValue &&
						AffectedAttributes.Contains(DependencyRecord.Key.AttributeId))
					{
						bAffected = true;
						break;
					}
				}
			}

			if (bAffected)
			{
				OutAffectedModifierInstIds.Add(ModifierInstance.ModifierInstId);
			}
		}

		// If dependency metadata cannot identify a subset, fall back to the full contributing set.
		if (OutAffectedModifierInstIds.IsEmpty())
		{
			for (const FTcsAttributeModifierInstance& ModifierInstance : ContributingInstances)
			{
				OutAffectedModifierInstIds.Add(ModifierInstance.ModifierInstId);
			}
		}
	}

	void AssignSingleContributor(
		TArray<FTcsEvaluatedAttributeOperation>& Operations,
		int32 ModifierInstId)
	{
		for (FTcsEvaluatedAttributeOperation& Operation : Operations)
		{
			Operation.ContributorModifierInstIds.Reset();
			Operation.ContributorModifierInstIds.Add(ModifierInstId);
		}
	}

	void AssignMergedProvenance(
		const TArray<FTcsAttributeModifierInstance>& GroupInstances,
		TArray<FTcsAttributeModifierInstance>& MergedGroup)
	{
		if (GroupInstances.Num() == 1 && MergedGroup.Num() == 1)
		{
			AssignSingleContributor(MergedGroup[0].AppliedOperations, GroupInstances[0].ModifierInstId);
			return;
		}

		for (FTcsAttributeModifierInstance& MergedInstance : MergedGroup)
		{
			for (FTcsEvaluatedAttributeOperation& Operation : MergedInstance.AppliedOperations)
			{
				Operation.ContributorModifierInstIds.Reset();
				for (const FTcsAttributeModifierInstance& SourceInstance : GroupInstances)
				{
					const bool bContributed = SourceInstance.AppliedOperations.ContainsByPredicate(
						[&Operation](const FTcsEvaluatedAttributeOperation& Candidate)
						{
							return Candidate.OperationId == Operation.OperationId;
						});
					if (bContributed)
					{
						// Additive / multi-source merges may sum multiple parents into one Operand.
						// Prefer exact operand match first for Max/Min/selection mergers.
						bool bExactOperandMatch = false;
						for (const FTcsEvaluatedAttributeOperation& SourceOperation : SourceInstance.AppliedOperations)
						{
							if (SourceOperation.OperationId == Operation.OperationId &&
								FMath::IsNearlyEqual(SourceOperation.EvaluatedOperand, Operation.EvaluatedOperand))
							{
								bExactOperandMatch = true;
								break;
							}
						}
						if (bExactOperandMatch || GroupInstances.Num() > 1)
						{
							Operation.ContributorModifierInstIds.AddUnique(SourceInstance.ModifierInstId);
						}
					}
				}

				if (Operation.ContributorModifierInstIds.IsEmpty())
				{
					for (const FTcsAttributeModifierInstance& SourceInstance : GroupInstances)
					{
						Operation.ContributorModifierInstIds.AddUnique(SourceInstance.ModifierInstId);
					}
				}

				// Selection mergers keep one Operand; collapse exact single match.
				if (Operation.ContributorModifierInstIds.Num() > 1)
				{
					TArray<int32> ExactMatches;
					for (const FTcsAttributeModifierInstance& SourceInstance : GroupInstances)
					{
						for (const FTcsEvaluatedAttributeOperation& SourceOperation : SourceInstance.AppliedOperations)
						{
							if (SourceOperation.OperationId == Operation.OperationId &&
								FMath::IsNearlyEqual(SourceOperation.EvaluatedOperand, Operation.EvaluatedOperand))
							{
								ExactMatches.AddUnique(SourceInstance.ModifierInstId);
							}
						}
					}
					if (ExactMatches.Num() == 1)
					{
						Operation.ContributorModifierInstIds = MoveTemp(ExactMatches);
					}
				}
			}
		}
	}
}



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
		if (ModifierInstance.ModifierInstId != ExcludedModifierInstId &&
			!ModifierInstance.AppliedOperations.IsEmpty())
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
	FTcsAttributeModifierApplicationResult* InOutResult,
	bool bAllowSkipInvalidRegisteredParents)
{
	OutUpdatedModifierInstances = ModifierInstances;
	OutCurrentValues.Reset();

	TMap<FName, float> WorkingBaseValues = BaseValues;
	if (!ClampCandidateAttributeValues(WorkingBaseValues))
	{
		if (ModifierInstances.IsEmpty())
		{
			UE_LOG(LogTcsAttribute, Fatal,
				TEXT("[%s] BaseValue/Range configuration cannot produce a legal value for %s even without Ongoing modifiers."),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this));
		}
		return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
	}

	TSet<int32> LocalExcludedModifierInstIds;
	const int32 MaxIterations = FMath::Max(1, ModifierInstances.Num() + 1);

	auto EvaluateParent = [this, &WorkingBaseValues](
		TArray<FTcsAttributeModifierInstance>& WorkingModifierInstances,
		int32 ModifierInstId,
		FTcsAttributeModifierApplicationResult* Result) -> bool
	{
		FTcsAttributeModifierInstance* const ModifierInstance = WorkingModifierInstances.FindByPredicate(
			[ModifierInstId](const FTcsAttributeModifierInstance& Candidate)
			{
				return Candidate.ModifierInstId == ModifierInstId;
			});
		if (!ModifierInstance || !ModifierInstance->IsValid())
		{
			return TcsAttributeModPrivate::SetApplicationFailure(Result, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (!ValidateAttributeModifierDefinitionCompatibility(*ModifierInstance->ModifierDef, Result))
		{
			return false;
		}

		FTcsAttributeEvaluationSnapshot Snapshot;
		if (!BuildAttributeEvaluationSnapshot(
			WorkingBaseValues,
			WorkingModifierInstances,
			ModifierInstance->ModifierInstId,
			Snapshot,
			Result))
		{
			return false;
		}

		UTcsStateInstance* const SourceStateInstance = ModifierInstance->OwningStateInstance.Get();
		UTcsSkillEntry* const SourceSkillEntry = TcsAttributeModPrivate::GetSourceSkillEntry(SourceStateInstance, nullptr);
		TArray<FTcsEvaluatedAttributeOperation> EvaluatedOperations;
		TArray<FTcsAttributeModifierDependencyKey> DependencyKeys;
		if (!BuildEvaluatedAttributeOperations(
			*ModifierInstance->ModifierDef,
			ModifierInstance->OperationOverrides,
			ModifierInstance->SourceHandle,
			SourceStateInstance,
			SourceSkillEntry,
			Snapshot,
			EvaluatedOperations,
			&DependencyKeys,
			Result))
		{
			return false;
		}

		TcsOngoingContributionPrivate::AssignSingleContributor(EvaluatedOperations, ModifierInstId);
		ModifierInstance->AppliedOperations = MoveTemp(EvaluatedOperations);
		ModifierInstance->DependencyRecords.Reset(DependencyKeys.Num());
		for (const FTcsAttributeModifierDependencyKey& DependencyKey : DependencyKeys)
		{
			FTcsAttributeModifierDependencyRecord& DependencyRecord = ModifierInstance->DependencyRecords.AddDefaulted_GetRef();
			DependencyRecord.Key = DependencyKey;
			DependencyRecord.ObservedRevision = DependencyRevisions.FindRef(DependencyKey);
		}
		ModifierInstance->UpdateTimestamp = FDateTime::UtcNow().GetTicks();
		return true;
	};

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		const int32 ExcludedCountBefore = LocalExcludedModifierInstIds.Num();
		TcsOngoingContributionPrivate::ClearAppliedOperations(OutUpdatedModifierInstances, LocalExcludedModifierInstIds);

		TArray<int32> CandidateIds;
		CandidateIds.Reserve(OutUpdatedModifierInstances.Num());
		for (const FTcsAttributeModifierInstance& ModifierInstance : OutUpdatedModifierInstances)
		{
			if (!ModifierInstance.IsValid())
			{
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
			}
			if (!LocalExcludedModifierInstIds.Contains(ModifierInstance.ModifierInstId))
			{
				CandidateIds.Add(ModifierInstance.ModifierInstId);
			}
		}
		CandidateIds.Sort();

		// Discovery phase: evaluate every non-excluded parent against a self-excluding snapshot.
		OutUpdatedModifierInstances = ModifierInstances;
		TcsOngoingContributionPrivate::ClearAppliedOperations(OutUpdatedModifierInstances, LocalExcludedModifierInstIds);
		bool bExcludedThisPass = false;
		for (const int32 ModifierInstId : CandidateIds)
		{
			FTcsAttributeModifierApplicationResult DiscoveryResult;
			if (!EvaluateParent(OutUpdatedModifierInstances, ModifierInstId, &DiscoveryResult))
			{
				if (!bAllowSkipInvalidRegisteredParents)
				{
					return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, DiscoveryResult.Failure);
				}

				UE_LOG(LogTcsAttribute, Error,
					TEXT("[%s] Temporary skip Ongoing parent %d after Evaluator failure. Failure=%d Component=%s"),
					*FString(__FUNCTION__),
					ModifierInstId,
					static_cast<int32>(DiscoveryResult.Failure),
					*GetPathNameSafe(this));
				LocalExcludedModifierInstIds.Add(ModifierInstId);
				bExcludedThisPass = true;
			}
		}
		if (bExcludedThisPass)
		{
			continue;
		}

		TArray<int32> EvaluationOrder;
		FString CycleDiagnostic;
		TArray<TArray<int32>> CyclicSccs;
		if (!BuildOngoingDependencyEvaluationOrder(
			OutUpdatedModifierInstances,
			TSet<int32>(),
			EvaluationOrder,
			CycleDiagnostic,
			&CyclicSccs))
		{
			if (!bAllowSkipInvalidRegisteredParents)
			{
				UE_LOG(LogTcsAttribute, Error, TEXT("[%s] %s"), *FString(__FUNCTION__), *CycleDiagnostic);
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_DependencyCycle);
			}

			if (CyclicSccs.IsEmpty())
			{
				UE_LOG(LogTcsAttribute, Error, TEXT("[%s] %s"), *FString(__FUNCTION__), *CycleDiagnostic);
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_DependencyCycle);
			}

			for (const TArray<int32>& Scc : CyclicSccs)
			{
				for (const int32 ModifierInstId : Scc)
				{
					LocalExcludedModifierInstIds.Add(ModifierInstId);
				}
			}
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Temporary skip cyclic Ongoing SCC. %s Component=%s"),
				*FString(__FUNCTION__),
				*CycleDiagnostic,
				*GetPathNameSafe(this));
			continue;
		}

		// Final evaluation in topological order from the original registered records.
		OutUpdatedModifierInstances = ModifierInstances;
		TcsOngoingContributionPrivate::ClearAppliedOperations(OutUpdatedModifierInstances, LocalExcludedModifierInstIds);
		if (InOutResult && AuditedModifierInstId != INDEX_NONE)
		{
			InOutResult->OperationResults.Reset();
		}

		bExcludedThisPass = false;
		for (const int32 ModifierInstId : EvaluationOrder)
		{
			if (LocalExcludedModifierInstIds.Contains(ModifierInstId))
			{
				continue;
			}

			FTcsAttributeModifierApplicationResult LocalResult;
			FTcsAttributeModifierApplicationResult* const ResultForModifier = ModifierInstId == AuditedModifierInstId
				? InOutResult
				: &LocalResult;
			if (!EvaluateParent(OutUpdatedModifierInstances, ModifierInstId, ResultForModifier))
			{
				if (!bAllowSkipInvalidRegisteredParents)
				{
					return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ResultForModifier->Failure);
				}

				UE_LOG(LogTcsAttribute, Error,
					TEXT("[%s] Temporary skip Ongoing parent %d after ordered Evaluator failure. Failure=%d Component=%s"),
					*FString(__FUNCTION__),
					ModifierInstId,
					static_cast<int32>(ResultForModifier->Failure),
					*GetPathNameSafe(this));
				LocalExcludedModifierInstIds.Add(ModifierInstId);
				bExcludedThisPass = true;
				break;
			}
		}
		if (bExcludedThisPass)
		{
			continue;
		}

		TArray<int32> FinalEvaluationOrder;
		if (!BuildOngoingDependencyEvaluationOrder(
			OutUpdatedModifierInstances,
			TSet<int32>(),
			FinalEvaluationOrder,
			CycleDiagnostic,
			&CyclicSccs))
		{
			if (!bAllowSkipInvalidRegisteredParents)
			{
				UE_LOG(LogTcsAttribute, Error, TEXT("[%s] %s"), *FString(__FUNCTION__), *CycleDiagnostic);
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_DependencyCycle);
			}

			for (const TArray<int32>& Scc : CyclicSccs)
			{
				for (const int32 ModifierInstId : Scc)
				{
					LocalExcludedModifierInstIds.Add(ModifierInstId);
				}
			}
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Temporary skip cyclic Ongoing SCC after final evaluation. %s Component=%s"),
				*FString(__FUNCTION__),
				*CycleDiagnostic,
				*GetPathNameSafe(this));
			continue;
		}
		if (FinalEvaluationOrder != EvaluationOrder)
		{
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Ongoing dependency structure changed during final evaluation; rejecting the atomic update."),
				*FString(__FUNCTION__));
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_DependencyGraphChanged);
		}

		TArray<FTcsAttributeModifierInstance> ContributingInstances;
		ContributingInstances.Reserve(OutUpdatedModifierInstances.Num());
		for (const FTcsAttributeModifierInstance& ModifierInstance : OutUpdatedModifierInstances)
		{
			if (!LocalExcludedModifierInstIds.Contains(ModifierInstance.ModifierInstId) &&
				!ModifierInstance.AppliedOperations.IsEmpty())
			{
				ContributingInstances.Add(ModifierInstance);
			}
		}

		TArray<FTcsAttributeModifierInstance> MergedModifierInstances;
		FTcsAttributeModifierApplicationResult MergeResult;
		FName FailedModifierDefId = NAME_None;
		if (!MergeOngoingModifierInstances(
			ContributingInstances,
			MergedModifierInstances,
			&MergeResult,
			&FailedModifierDefId))
		{
			if (!bAllowSkipInvalidRegisteredParents)
			{
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, MergeResult.Failure);
			}

			if (!FailedModifierDefId.IsNone())
			{
				TcsOngoingContributionPrivate::CollectDefGroupIds(
					OutUpdatedModifierInstances,
					FailedModifierDefId,
					LocalExcludedModifierInstIds);
			}
			else
			{
				for (const FTcsAttributeModifierInstance& ModifierInstance : ContributingInstances)
				{
					LocalExcludedModifierInstIds.Add(ModifierInstance.ModifierInstId);
				}
			}
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Temporary skip Ongoing ModifierDefId group [%s] after Merger failure. Failure=%d Component=%s"),
				*FString(__FUNCTION__),
				*FailedModifierDefId.ToString(),
				static_cast<int32>(MergeResult.Failure),
				*GetPathNameSafe(this));
			continue;
		}

		OutCurrentValues = WorkingBaseValues;
		MergedModifierInstances.Sort();
		bool bOperatorFailed = false;
		TSet<int32> OperatorFailureIds;
		for (const FTcsAttributeModifierInstance& ModifierInstance : MergedModifierInstances)
		{
			FTcsAttributeModifierApplicationResult OperatorResult;
			if (!ApplyEvaluatedOperationsToValues(
				ModifierInstance.AppliedOperations,
				OutCurrentValues,
				ModifierInstance.ModifierInstId == AuditedModifierInstId ? InOutResult : &OperatorResult))
			{
				if (!bAllowSkipInvalidRegisteredParents)
				{
					return false;
				}

				for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
				{
					if (Operation.ContributorModifierInstIds.Num() == 1)
					{
						OperatorFailureIds.Add(Operation.ContributorModifierInstIds[0]);
					}
					else if (!Operation.ContributorModifierInstIds.IsEmpty())
					{
						TcsOngoingContributionPrivate::CollectDefGroupIds(
							OutUpdatedModifierInstances,
							ModifierInstance.ModifierDefId,
							OperatorFailureIds);
					}
					else
					{
						OperatorFailureIds.Add(ModifierInstance.ModifierInstId);
						TcsOngoingContributionPrivate::CollectDefGroupIds(
							OutUpdatedModifierInstances,
							ModifierInstance.ModifierDefId,
							OperatorFailureIds);
					}
				}
				if (OperatorFailureIds.IsEmpty())
				{
					OperatorFailureIds.Add(ModifierInstance.ModifierInstId);
				}
				bOperatorFailed = true;
				break;
			}
		}
		if (bOperatorFailed)
		{
			LocalExcludedModifierInstIds.Append(OperatorFailureIds);
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Temporary skip Ongoing parents after Operator failure. Component=%s"),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this));
			continue;
		}

		if (!ClampCandidateAttributeValues(OutCurrentValues))
		{
			if (ContributingInstances.IsEmpty())
			{
				UE_LOG(LogTcsAttribute, Fatal,
					TEXT("[%s] BaseValue/Range configuration cannot produce a legal CurrentValue for %s even without Ongoing modifiers."),
					*FString(__FUNCTION__),
					*GetPathNameSafe(this));
			}

			if (!bAllowSkipInvalidRegisteredParents)
			{
				return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
			}

			TMap<FName, TSet<FName>> RangeDependents;
			const bool bHasRangeDependents = TryBuildDeclaredRangeConstraintDependents(RangeDependents);
			TSet<int32> ClampAffectedModifierInstIds;
			TcsOngoingContributionPrivate::CollectClampAffectedClosureIds(
				ContributingInstances,
				bHasRangeDependents ? &RangeDependents : nullptr,
				ClampAffectedModifierInstIds);
			LocalExcludedModifierInstIds.Append(ClampAffectedModifierInstIds);
			UE_LOG(LogTcsAttribute, Error,
				TEXT("[%s] Temporary skip Ongoing dependency closure after Current Clamp/Range failure. Component=%s AffectedParents=%d"),
				*FString(__FUNCTION__),
				*GetPathNameSafe(this),
				ClampAffectedModifierInstIds.Num());
			continue;
		}

		// Success: excluded parents keep empty AppliedOperations.
		TcsOngoingContributionPrivate::ClearAppliedOperations(OutUpdatedModifierInstances, LocalExcludedModifierInstIds);
		return true;
	}

	UE_LOG(LogTcsAttribute, Error,
		TEXT("[%s] Exhausted bounded Ongoing contribution filtering for %s"),
		*FString(__FUNCTION__),
		*GetPathNameSafe(this));
	return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
}

bool UTcsAttributeComponent::MergeOngoingModifierInstances(
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	TArray<FTcsAttributeModifierInstance>& OutMergedModifierInstances,
	FTcsAttributeModifierApplicationResult* InOutResult,
	FName* OutFailedModifierDefId) const
{
	OutMergedModifierInstances.Reset();
	if (OutFailedModifierDefId)
	{
		*OutFailedModifierDefId = NAME_None;
	}
	if (ModifierInstances.IsEmpty())
	{
		return true;
	}

	TMap<FName, TArray<FTcsAttributeModifierInstance>> InstancesByDefId;
	for (const FTcsAttributeModifierInstance& ModifierInstance : ModifierInstances)
	{
		if (!ModifierInstance.IsValid() || !ModifierInstance.ModifierDef || !ModifierInstance.ModifierDef->MergerType)
		{
			if (OutFailedModifierDefId)
			{
				*OutFailedModifierDefId = ModifierInstance.ModifierDefId;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}
		if (ModifierInstance.AppliedOperations.IsEmpty())
		{
			continue;
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
			if (OutFailedModifierDefId)
			{
				*OutFailedModifierDefId = DefGroup.Key;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		UTcsAttributeModifierMerger* const MergerCDO =
			ModifierDefinition->MergerType->GetDefaultObject<UTcsAttributeModifierMerger>();
		if (!MergerCDO)
		{
			if (OutFailedModifierDefId)
			{
				*OutFailedModifierDefId = DefGroup.Key;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		TArray<FTcsAttributeModifierInstance> MergedGroup;
		MergerCDO->Merge(GroupInstances, MergedGroup);
		if (MergedGroup.IsEmpty() && !GroupInstances.IsEmpty())
		{
			if (OutFailedModifierDefId)
			{
				*OutFailedModifierDefId = DefGroup.Key;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		TcsOngoingContributionPrivate::AssignMergedProvenance(GroupInstances, MergedGroup);
		OutMergedModifierInstances.Append(MergedGroup);
	}

	return true;
}
