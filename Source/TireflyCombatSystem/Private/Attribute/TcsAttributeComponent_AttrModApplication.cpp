// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/TcsAttributeComponent_AttrModHelpers.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "TcsLogChannels.h"


bool UTcsAttributeComponent::ApplyAttributeModifier(
	const FTcsAttributeModifierApplicationRequest& Request,
	FTcsAttributeModifierApplicationResult& OutResult)
{
	OutResult.Reset();
	OutResult.ModifierDefId = Request.ModifierDefId;
	OutResult.ApplicationMode = Request.ApplicationMode;
	OutResult.SourceHandle = Request.SourceHandle;
	if (bIsBroadcastingAttributeStateDiffs)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Reentrant AttributeModifier Application was rejected during Attribute event publication for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidRequest);
	}

	if (!IsRuntimePrepared())
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Attribute runtime is not ready for %s"),
			*FString(__FUNCTION__),
			*GetPathNameSafe(this));
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_RuntimeNotPrepared);
	}

	if (Request.ModifierDefId.IsNone() ||
		(Request.ApplicationMode != ETcsAttributeModifierApplicationMode::AMAM_Instant &&
			Request.ApplicationMode != ETcsAttributeModifierApplicationMode::AMAM_Ongoing))
	{
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidRequest);
	}

	if (!Request.SourceHandle.IsValid())
	{
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidSourceHandle);
	}

	UTcsDefinitionManagerSubsystem* const DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DefinitionNotFound);
	}

	const UTcsAttributeModifierDefinition* const ModifierDefinition =
		DefinitionManager->GetAttributeModifierDefinition(Request.ModifierDefId);
	if (!ModifierDefinition)
	{
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DefinitionNotFound);
	}

	if (ModifierDefinition->Operations.IsEmpty())
	{
		return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_NoOperations);
	}

	UTcsStateInstance* const SourceStateInstance = Request.SourceStateInstance.Get();
	UTcsSkillEntry* const SourceSkillEntry = TcsAttributeModPrivate::GetSourceSkillEntry(
		SourceStateInstance,
		Request.SourceSkillEntry.Get());

	if (Request.ApplicationMode == ETcsAttributeModifierApplicationMode::AMAM_Ongoing)
	{
		if (!SourceStateInstance ||
			!SourceStateInstance->IsInitialized() ||
			SourceStateInstance->IsPendingGC() ||
			SourceStateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired ||
			SourceStateInstance->GetOwnerAttributeComponent() != this ||
			!TcsAttributeModPrivate::IsStateInstanceRegisteredWithOwner(SourceStateInstance) ||
			SourceStateInstance->GetSourceHandle() != Request.SourceHandle ||
			Cast<UTcsSkillInstance>(SourceStateInstance) ||
			Request.SourceSkillEntry.IsValid())
		{
			return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (!ValidateAttributeModifierDefinitionCompatibility(*ModifierDefinition, &OutResult))
		{
			return false;
		}

		for (const FTcsAttributeModifierInstance& ModifierInstance : AttributeModifiers)
		{
			if (ModifierInstance.OwningStateInstance.Get() == SourceStateInstance &&
				ModifierInstance.ModifierDefId == Request.ModifierDefId)
			{
				UE_LOG(LogTcsAttribute, Warning,
					TEXT("[%s] StateInstance %s already owns Ongoing AttributeModifier %s for SourceHandle %s"),
					*FString(__FUNCTION__),
					*GetPathNameSafe(SourceStateInstance),
					*Request.ModifierDefId.ToString(),
					*Request.SourceHandle.ToDebugString());
				return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DuplicateOngoingDefinition);
			}
		}
	}

	const TMap<FName, float> BaseValues = GetAttributeBaseValues();
	FTcsAttributeEvaluationSnapshot Snapshot;
	if (!BuildAttributeEvaluationSnapshot(BaseValues, AttributeModifiers, INDEX_NONE, Snapshot, &OutResult))
	{
		return false;
	}

	TArray<FTcsEvaluatedAttributeOperation> EvaluatedOperations;
	if (!BuildEvaluatedAttributeOperations(
		*ModifierDefinition,
		Request.OperationOverrides,
		Request.SourceHandle,
		SourceStateInstance,
		SourceSkillEntry,
		Snapshot,
		EvaluatedOperations,
		nullptr,
		&OutResult))
	{
		return false;
	}

	if (Request.ApplicationMode == ETcsAttributeModifierApplicationMode::AMAM_Instant)
	{
		TMap<FName, float> CandidateBaseValues = BaseValues;
		if (!ApplyEvaluatedOperationsToValues(EvaluatedOperations, CandidateBaseValues, &OutResult))
		{
			return false;
		}

		if (!ClampCandidateAttributeValues(CandidateBaseValues))
		{
			return TcsAttributeModPrivate::SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
		}

		TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
		TMap<FName, float> CandidateCurrentValues;
		if (!BuildOngoingAttributeValues(
			CandidateBaseValues,
			AttributeModifiers,
			UpdatedModifierInstances,
			CandidateCurrentValues,
			INDEX_NONE,
			&OutResult,
			true))
		{
			return false;
		}

		CommitAttributeModifierTransaction(
			CandidateBaseValues,
			CandidateCurrentValues,
			UpdatedModifierInstances,
			!AttributeModifiers.IsEmpty());
		OutResult.bSucceeded = true;
		return true;
	}

	FTcsAttributeModifierInstance NewModifierInstance;
	NewModifierInstance.ModifierDef = ModifierDefinition;
	NewModifierInstance.ModifierDefId = Request.ModifierDefId;
	NewModifierInstance.ModifierInstId = AllocateModifierInstanceId();
	NewModifierInstance.SourceHandle = Request.SourceHandle;
	NewModifierInstance.OwningStateInstance = SourceStateInstance;
	NewModifierInstance.AppliedOperations = EvaluatedOperations;
	NewModifierInstance.OperationOverrides = Request.OperationOverrides;
	NewModifierInstance.ApplyTimestamp = FDateTime::UtcNow().GetTicks();
	NewModifierInstance.UpdateTimestamp = NewModifierInstance.ApplyTimestamp;

	TArray<FTcsAttributeModifierInstance> CandidateModifierInstances = AttributeModifiers;
	CandidateModifierInstances.Add(NewModifierInstance);

	TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
	TMap<FName, float> CandidateCurrentValues;
	OutResult.OperationResults.Reset();
	// Existing registered parents may temporary-skip; the new candidate itself must succeed.
	if (!BuildOngoingAttributeValues(
		BaseValues,
		CandidateModifierInstances,
		UpdatedModifierInstances,
		CandidateCurrentValues,
		NewModifierInstance.ModifierInstId,
		&OutResult,
		true))
	{
		return false;
	}

	const FTcsAttributeModifierInstance* const CommittedNewInstance = UpdatedModifierInstances.FindByPredicate(
		[&NewModifierInstance](const FTcsAttributeModifierInstance& Candidate)
		{
			return Candidate.ModifierInstId == NewModifierInstance.ModifierInstId;
		});
	if (!CommittedNewInstance || CommittedNewInstance->AppliedOperations.IsEmpty())
	{
		// Never-registered candidates must not persist as empty-contribution rows.
		return TcsAttributeModPrivate::SetApplicationFailure(
			&OutResult,
			OutResult.Failure != ETcsAttributeModifierApplicationFailure::AMAF_None
				? OutResult.Failure
				: ETcsAttributeModifierApplicationFailure::AMAF_DependencyCycle);
	}

	CommitAttributeModifierTransaction(
		BaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		true);
	OutResult.bSucceeded = true;
	return true;
}
