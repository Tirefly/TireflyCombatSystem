// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"

#include "Attribute/AttrModOperation/TcsAttributeModifierCustomOperator.h"
#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluatorContext.h"
#include "Attribute/TcsAttributeComponent_AttrModHelpers.h"
#include "Attribute/TcsAttributeModifierCompatibility.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "TcsDeveloperSettings.h"
#include "TcsLogChannels.h"


bool UTcsAttributeComponent::BuildEvaluatedAttributeOperations(
	const UTcsAttributeModifierDefinition& ModifierDefinition,
	const TMap<FName, FTcsAttributeModifierOperationOverride>& OperationOverrides,
	const FTcsSourceHandle& SourceHandle,
	UTcsStateInstance* SourceStateInstance,
	UTcsSkillEntry* SourceSkillEntry,
	const FTcsAttributeEvaluationSnapshot& Snapshot,
	TArray<FTcsEvaluatedAttributeOperation>& OutOperations,
	TArray<FTcsAttributeModifierDependencyKey>* OutDependencyKeys,
	FTcsAttributeModifierApplicationResult* InOutResult) const
{
	OutOperations.Reset();
	if (OutDependencyKeys)
	{
		OutDependencyKeys->Reset();
	}

	for (const TPair<FName, FTcsAttributeModifierOperationOverride>& OverridePair : OperationOverrides)
	{
		if (!ModifierDefinition.Operations.Contains(OverridePair.Key))
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationOverride);
		}
	}

	TArray<FName> OperationIds;
	TcsAttributeModPrivate::GetSortedOperationIds(ModifierDefinition, OperationIds);
	OutOperations.Reserve(OperationIds.Num());

	FTcsAttributeOperandEvaluatorContext Context;
	Context.TargetAttributeComponent = this;
	Context.Target = GetOwner();
	Context.Instigator = SourceHandle.Instigator.Get();
	Context.SourceHandle = &SourceHandle;
	Context.SourceStateInstance = SourceStateInstance;
	Context.SourceSkillEntry = SourceSkillEntry;
	Context.AttributeSnapshot = &Snapshot;
	Context.DependencyCollector = OutDependencyKeys;

	for (const FName OperationId : OperationIds)
	{
		const FTcsAttributeOperationSpec* const OperationSpec = ModifierDefinition.Operations.Find(OperationId);
		if (!OperationSpec)
		{
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
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
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		if (!Attributes.Contains(OperationSpec->TargetAttributeId))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing);
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

		if (!EvaluatorClass || EvaluatorClass->HasAnyClassFlags(CLASS_Abstract) || !TcsAttributeModPrivate::IsValidOperandPayload(OperandPayload))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
		}

		if (OperationSpec->Operator == ETcsAttributeModifierOperator::AMO_None ||
			(OperationSpec->Operator == ETcsAttributeModifierOperator::AMO_Custom &&
				(!OperationSpec->CustomOperatorClass || OperationSpec->CustomOperatorClass->HasAnyClassFlags(CLASS_Abstract))))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperationSpec);
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
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_EvaluatorFailed);
		}

		if (!FMath::IsFinite(EvaluatedOperand))
		{
			if (OperationResult)
			{
				OperationResult->Failure = ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperand;
			}
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperand);
		}

		FTcsEvaluatedAttributeOperation& EvaluatedOperation = OutOperations.AddDefaulted_GetRef();
		EvaluatedOperation.OperationId = OperationId;
		EvaluatedOperation.TargetAttributeId = OperationSpec->TargetAttributeId;
		EvaluatedOperation.Operator = OperationSpec->Operator;
		EvaluatedOperation.CustomOperatorClass = OperationSpec->CustomOperatorClass;
		EvaluatedOperation.EvaluatedOperand = EvaluatedOperand;
	}

	if (OutDependencyKeys)
	{
		OutDependencyKeys->Sort([](
			const FTcsAttributeModifierDependencyKey& Left,
			const FTcsAttributeModifierDependencyKey& Right)
		{
			if (Left.Type != Right.Type)
			{
				return static_cast<uint8>(Left.Type) < static_cast<uint8>(Right.Type);
			}
			if (Left.AttributeId != Right.AttributeId)
			{
				return Left.AttributeId.LexicalLess(Right.AttributeId);
			}
			if (Left.StateParamTag != Right.StateParamTag)
			{
				return Left.StateParamTag.GetTagName().LexicalLess(Right.StateParamTag.GetTagName());
			}
			return GetTypeHash(Left.SourceBuffObjectKey) < GetTypeHash(Right.SourceBuffObjectKey);
		});
	}

	return true;
}

bool UTcsAttributeComponent::ValidateAttributeModifierDefinitionCompatibility(
	const UTcsAttributeModifierDefinition& ModifierDefinition,
	FTcsAttributeModifierApplicationResult* InOutResult) const
{
	const UTcsDeveloperSettings* const Settings = GetDefault<UTcsDeveloperSettings>();
	TArray<FText> Errors;
	TArray<FText> Warnings;
	if (FTcsAttributeModifierCompatibility::ValidateModifierDefinitionCompatibility(
		ModifierDefinition,
		Errors,
		Warnings,
		Settings))
	{
		return true;
	}

	const bool bHasForbidden = Errors.ContainsByPredicate([](const FText& Error)
	{
		return Error.ToString().Contains(TEXT("Forbidden"));
	});
	return TcsAttributeModPrivate::SetApplicationFailure(
		InOutResult,
		bHasForbidden
			? ETcsAttributeModifierApplicationFailure::AMAF_IncompatibleOperatorMerger
			: ETcsAttributeModifierApplicationFailure::AMAF_UnsupportedMerger);
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
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_TargetAttributeMissing);
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
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_OperatorFailed);
		}

		if (!FMath::IsFinite(NewValue))
		{
			SetOperationFailure(ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
			return TcsAttributeModPrivate::SetApplicationFailure(InOutResult, ETcsAttributeModifierApplicationFailure::AMAF_InvalidOperatorResult);
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
