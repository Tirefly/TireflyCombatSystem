// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"
#include "Attribute/AttrModOperation/TcsAttributeModifierCustomOperator.h"
#include "Attribute/AttrModOperation/TcsAttributeOperandEvaluatorContext.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "DefinitionManager/TcsDefinitionManagerSubsystem.h"
#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "State/TcsStateComponent.h"
#include "State/TcsStateInstance.h"
#include "TcsLogChannels.h"



namespace
{
	bool SetApplicationFailure(
		FTcsAttributeModifierApplicationResult* Result,
		ETcsAttributeModifierApplicationFailure Failure)
	{
		if (Result && Result->Failure == ETcsAttributeModifierApplicationFailure::AMAF_None)
		{
			Result->Failure = Failure;
		}

		return false;
	}

	bool IsValidOperandPayload(const FInstancedStruct& Payload)
	{
		const UScriptStruct* const PayloadStruct = Payload.GetScriptStruct();
		return PayloadStruct && PayloadStruct->IsChildOf(FTcsAttributeOperandPayload::StaticStruct());
	}

	void GetSortedOperationIds(
		const UTcsAttributeModifierDefinition& ModifierDefinition,
		TArray<FName>& OutOperationIds)
	{
		ModifierDefinition.Operations.GetKeys(OutOperationIds);
		OutOperationIds.Sort([](const FName& Left, const FName& Right)
		{
			return Left.LexicalLess(Right);
		});
	}

	UTcsSkillEntry* GetSourceSkillEntry(
		UTcsStateInstance* SourceStateInstance,
		UTcsSkillEntry* ExplicitSourceSkillEntry)
	{
		if (ExplicitSourceSkillEntry)
		{
			return ExplicitSourceSkillEntry;
		}

		if (const UTcsSkillInstance* const SkillInstance = Cast<UTcsSkillInstance>(SourceStateInstance))
		{
			return SkillInstance->GetSkillEntry();
		}

		return nullptr;
	}

	bool IsStateInstanceRegisteredWithOwner(UTcsStateInstance* StateInstance)
	{
		if (!StateInstance)
		{
			return false;
		}

		UTcsStateComponent* const OwnerStateComponent = StateInstance->GetOwnerStateComponent();
		if (!OwnerStateComponent)
		{
			return false;
		}

		TArray<UTcsStateInstance*> RegisteredStates;
		if (!OwnerStateComponent->GetStatesByDefId(StateInstance->GetStateDefId(), RegisteredStates))
		{
			return false;
		}

		return RegisteredStates.Contains(StateInstance);
	}
}



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
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_RuntimeNotPrepared);
	}

	if (Request.ModifierDefId.IsNone() ||
		(Request.ApplicationMode != ETcsAttributeModifierApplicationMode::AMAM_Instant &&
			Request.ApplicationMode != ETcsAttributeModifierApplicationMode::AMAM_Ongoing))
	{
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidRequest);
	}

	if (!Request.SourceHandle.IsValid())
	{
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidSourceHandle);
	}

	UTcsDefinitionManagerSubsystem* const DefinitionManager = ResolveDefinitionManager();
	if (!DefinitionManager)
	{
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DefinitionNotFound);
	}

	const UTcsAttributeModifierDefinition* const ModifierDefinition =
		DefinitionManager->GetAttributeModifierDefinition(Request.ModifierDefId);
	if (!ModifierDefinition)
	{
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DefinitionNotFound);
	}

	if (ModifierDefinition->Operations.IsEmpty())
	{
		return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_NoOperations);
	}

	UTcsStateInstance* const SourceStateInstance = Request.SourceStateInstance.Get();
	UTcsSkillEntry* const SourceSkillEntry = GetSourceSkillEntry(
		SourceStateInstance,
		Request.SourceSkillEntry.Get());

	if (Request.ApplicationMode == ETcsAttributeModifierApplicationMode::AMAM_Ongoing)
	{
		if (!SourceStateInstance ||
			!SourceStateInstance->IsInitialized() ||
			SourceStateInstance->IsPendingGC() ||
			SourceStateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired ||
			SourceStateInstance->GetOwnerAttributeComponent() != this ||
			!IsStateInstanceRegisteredWithOwner(SourceStateInstance) ||
			SourceStateInstance->GetSourceHandle() != Request.SourceHandle ||
			Cast<UTcsSkillInstance>(SourceStateInstance) ||
			Request.SourceSkillEntry.IsValid())
		{
			return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (ModifierDefinition->MergerType != UTcsAttrModMerger_NoMerge::StaticClass())
		{
			return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
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
				return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_DuplicateOngoingDefinition);
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
			return SetApplicationFailure(&OutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
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
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (ModifierInstance.ModifierDef->MergerType != UTcsAttrModMerger_NoMerge::StaticClass())
		{
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
		}

		FTcsAttributeModifierApplicationResult OperationResult;
		if (!ApplyEvaluatedOperationsToValues(
			ModifierInstance.AppliedOperations,
			OutSnapshot.CurrentValues,
			&OperationResult))
		{
			return SetApplicationFailure(InOutResult, OperationResult.Failure);
		}
	}

	if (!ClampCandidateAttributeValues(OutSnapshot.BaseValues) ||
		!ClampCandidateAttributeValues(OutSnapshot.CurrentValues))
	{
		return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
	}

	return true;
}

bool UTcsAttributeComponent::BuildEvaluatedAttributeOperations(
	const UTcsAttributeModifierDefinition& ModifierDefinition,
	const TMap<FName, FTcsAttributeModifierOperationOverride>& OperationOverrides,
	const FTcsSourceHandle& SourceHandle,
	UTcsStateInstance* SourceStateInstance,
	UTcsSkillEntry* SourceSkillEntry,
	const FTcsAttributeEvaluationSnapshot& Snapshot,
	TArray<FTcsEvaluatedAttributeOperation>& OutOperations,
	FTcsAttributeModifierApplicationResult* InOutResult) const
{
	OutOperations.Reset();

	for (const TPair<FName, FTcsAttributeModifierOperationOverride>& OverridePair : OperationOverrides)
	{
		if (!ModifierDefinition.Operations.Contains(OverridePair.Key))
		{
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationOverride);
		}
	}

	TArray<FName> OperationIds;
	GetSortedOperationIds(ModifierDefinition, OperationIds);
	OutOperations.Reserve(OperationIds.Num());

	FTcsAttributeOperandEvaluatorContext Context;
	Context.TargetAttributeComponent = this;
	Context.Target = GetOwner();
	Context.Instigator = SourceHandle.Instigator.Get();
	Context.SourceHandle = &SourceHandle;
	Context.SourceStateInstance = SourceStateInstance;
	Context.SourceSkillEntry = SourceSkillEntry;
	Context.AttributeSnapshot = &Snapshot;

	for (const FName OperationId : OperationIds)
	{
		const FTcsAttributeOperationSpec* const OperationSpec = ModifierDefinition.Operations.Find(OperationId);
		if (!OperationSpec)
		{
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		FTcsAttributeModifierOperationApplicationResult* OperationResult = nullptr;
		if (InOutResult)
		{
			OperationResult = &InOutResult->OperationResults.AddDefaulted_GetRef();
			OperationResult->OperationId = OperationId;
			OperationResult->TargetAttributeId = OperationSpec->TargetAttributeId;
			OperationResult->SourceHandle = SourceHandle;
		}

		if (OperationSpec->TargetAttributeId.IsNone())
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		if (!Attributes.Contains(OperationSpec->TargetAttributeId))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing);
		}

		TSubclassOf<UTcsAttributeModifierNumericEvaluator> EvaluatorClass = OperationSpec->OperandEvaluatorClass;
		FInstancedStruct OperandPayload = OperationSpec->OperandPayload;
		if (const FTcsAttributeModifierOperationOverride* const OperationOverride = OperationOverrides.Find(OperationId))
		{
			if (OperationOverride->bOverrideOperandEvaluator)
			{
				EvaluatorClass = OperationOverride->OperandEvaluatorClass;
			}

			if (OperationOverride->bOverrideOperandPayload)
			{
				OperandPayload = OperationOverride->OperandPayload;
			}
		}

		if (!EvaluatorClass || EvaluatorClass->HasAnyClassFlags(CLASS_Abstract) || !IsValidOperandPayload(OperandPayload))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		if (OperationSpec->Operator == ETcsAttributeModifierOperator::AMO_None ||
			(OperationSpec->Operator == ETcsAttributeModifierOperator::AMO_Custom &&
				(!OperationSpec->CustomOperatorClass || OperationSpec->CustomOperatorClass->HasAnyClassFlags(CLASS_Abstract))))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		float EvaluatedOperand = 0.f;
		if (!EvaluatorClass->GetDefaultObject<UTcsAttributeModifierNumericEvaluator>()->Evaluate(
			Context,
			OperandPayload,
			EvaluatedOperand))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_EvaluatorFailed;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_EvaluatorFailed);
		}

		if (!FMath::IsFinite(EvaluatedOperand))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperand;
			}
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperand);
		}

		FTcsEvaluatedAttributeOperation& EvaluatedOperation = OutOperations.AddDefaulted_GetRef();
		EvaluatedOperation.OperationId = OperationId;
		EvaluatedOperation.TargetAttributeId = OperationSpec->TargetAttributeId;
		EvaluatedOperation.Operator = OperationSpec->Operator;
		EvaluatedOperation.CustomOperatorClass = OperationSpec->CustomOperatorClass;
		EvaluatedOperation.EvaluatedOperand = EvaluatedOperand;
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
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOngoingOwner);
		}

		if (ModifierInstance.ModifierDef->MergerType != UTcsAttrModMerger_NoMerge::StaticClass())
		{
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
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
		UTcsSkillEntry* const SourceSkillEntry = GetSourceSkillEntry(SourceStateInstance, nullptr);
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
			return SetApplicationFailure(InOutResult, ResultForModifier->Failure);
		}

		ModifierInstance.AppliedOperations = MoveTemp(EvaluatedOperations);
		ModifierInstance.UpdateTimestamp = FDateTime::UtcNow().GetTicks();
	}

	OutCurrentValues = BaseValues;
	TArray<FTcsAttributeModifierInstance> SortedModifierInstances = OutUpdatedModifierInstances;
	SortedModifierInstances.Sort();
	for (const FTcsAttributeModifierInstance& ModifierInstance : SortedModifierInstances)
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
		|| SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
}

bool UTcsAttributeComponent::ApplyEvaluatedOperationsToValues(
	const TArray<FTcsEvaluatedAttributeOperation>& Operations,
	TMap<FName, float>& InOutValues,
	FTcsAttributeModifierApplicationResult* InOutResult) const
{
	for (const FTcsEvaluatedAttributeOperation& Operation : Operations)
	{
		auto SetOperationFailure = [InOutResult, &Operation](ETcsAttributeModifierApplicationFailure Failure)
		{
			if (InOutResult)
			{
				for (FTcsAttributeModifierOperationApplicationResult& OperationResult : InOutResult->OperationResults)
				{
					if (OperationResult.OperationId == Operation.OperationId &&
						OperationResult.TargetAttributeId == Operation.TargetAttributeId)
					{
						OperationResult.Failure = Failure;
						break;
					}
				}
			}
		};

		float* const CurrentValue = InOutValues.Find(Operation.TargetAttributeId);
		if (!CurrentValue)
		{
			SetOperationFailure(ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing);
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing);
		}

		const float OldValue = *CurrentValue;
		float NewValue = 0.f;
		if (!ApplyTcsAttributeModifierOperator(
			Operation.Operator,
			Operation.CustomOperatorClass,
			OldValue,
			Operation.EvaluatedOperand,
			NewValue))
		{
			SetOperationFailure(ETcsAttributeModifierApplicationFailure::AMAF_OperatorFailed);
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_OperatorFailed);
		}

		if (!FMath::IsFinite(NewValue))
		{
			SetOperationFailure(ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
			return SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
		}

		*CurrentValue = NewValue;

		if (InOutResult)
		{
			for (FTcsAttributeModifierOperationApplicationResult& OperationResult : InOutResult->OperationResults)
			{
				if (OperationResult.OperationId == Operation.OperationId &&
					OperationResult.TargetAttributeId == Operation.TargetAttributeId)
				{
					OperationResult.OldValue = OldValue;
					OperationResult.NewValue = NewValue;
					OperationResult.bSucceeded = true;
					OperationResult.Failure = ETcsAttributeModifierApplicationFailure::AMAF_None;
					break;
				}
			}
		}
	}

	return true;
}

bool UTcsAttributeComponent::ClampCandidateAttributeValues(TMap<FName, float>& InOutValues)
{
	constexpr int32 MaxIterations = 8;
	TArray<FName> AttributeIds;
	InOutValues.GetKeys(AttributeIds);
	AttributeIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (int32 Iteration = 0; Iteration < MaxIterations; ++Iteration)
	{
		bool bAnyChanged = false;
		for (const FName AttributeId : AttributeIds)
		{
			float* const Value = InOutValues.Find(AttributeId);
			if (!Value)
			{
				return false;
			}

			const float PreviousValue = *Value;
			ClampAttributeValueInRange(AttributeId, *Value, nullptr, nullptr, &InOutValues);
			bAnyChanged |= !FMath::IsNearlyEqual(PreviousValue, *Value);
		}

		if (!bAnyChanged)
		{
			return true;
		}
	}

	return false;
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
