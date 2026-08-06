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
			&OutResult))
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
	if (!BuildOngoingAttributeValues(
		BaseValues,
		CandidateModifierInstances,
		UpdatedModifierInstances,
		CandidateCurrentValues,
		NewModifierInstance.ModifierInstId,
		&OutResult))
	{
		return false;
	}

	CommitAttributeModifierTransaction(
		BaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		true);
	OutResult.bSucceeded = true;
	return true;
}

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

	TMap<FName, float> BaseValues = GetAttributeBaseValues();
	if (!ClampCandidateAttributeValues(BaseValues))
	{
		return false;
	}
	TArray<FTcsAttributeModifierInstance> UpdatedModifierInstances;
	TMap<FName, float> CandidateCurrentValues;
	if (!BuildOngoingAttributeValues(
		BaseValues,
		CandidateModifierInstances,
		UpdatedModifierInstances,
		CandidateCurrentValues))
	{
		return false;
	}

	CommitAttributeModifierTransaction(
		BaseValues,
		CandidateCurrentValues,
		UpdatedModifierInstances,
		true);
	return true;
}

void UTcsAttributeComponent::CommitAttributeModifierTransaction(
	const TMap<FName, float>& BaseValues,
	const TMap<FName, float>& CurrentValues,
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	bool bCommitModifierInstances,
	bool bBroadcastEvents)
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

	if (bBroadcastEvents)
	{
		BroadcastAttributeStateDiffs(PreviousBaseValues, PreviousCurrentValues);
	}
}
