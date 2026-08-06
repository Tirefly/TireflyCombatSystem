// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeModifierCompatibility.h"

#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_UseAdditiveSum.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMaximum.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMinimum.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_UseNewest.h"
#include "Attribute/AttrModMerger/TcsAttrModMerger_UseOldest.h"
#include "Attribute/AttrModOperation/TcsAttributeModifierCustomOperator.h"
#include "Attribute/TcsAttributeModifierDefinition.h"
#include "TcsDeveloperSettings.h"



namespace
{
	bool IsExactOrChildOf(const UClass* ClassToTest, const UClass* ParentClass)
	{
		return ClassToTest && ParentClass && (ClassToTest == ParentClass || ClassToTest->IsChildOf(ParentClass));
	}

	ETcsAttributeOperatorMergerCompatibility EvaluateBuiltInDefault(
		TSubclassOf<UTcsAttributeModifierMerger> MergerType,
		ETcsAttributeModifierOperator Operator,
		const UTcsDeveloperSettings* Settings)
	{
		if (!MergerType || Operator == ETcsAttributeModifierOperator::AMO_None)
		{
			return ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
		}

		if (IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_NoMerge::StaticClass()) ||
			IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseNewest::StaticClass()) ||
			IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseOldest::StaticClass()))
		{
			return ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
		}

		if (IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseMaximum::StaticClass()) ||
			IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseMinimum::StaticClass()))
		{
			if (Operator == ETcsAttributeModifierOperator::AMO_Override ||
				Operator == ETcsAttributeModifierOperator::AMO_Custom)
			{
				return ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
			}

			if (Operator == ETcsAttributeModifierOperator::AMO_Add ||
				Operator == ETcsAttributeModifierOperator::AMO_MultiplyAdditive ||
				Operator == ETcsAttributeModifierOperator::AMO_MultiplyCompound)
			{
				return ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
			}

			return ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
		}

		if (IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseAdditiveSum::StaticClass()))
		{
			if (Operator == ETcsAttributeModifierOperator::AMO_Add)
			{
				return ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
			}

			if (Operator == ETcsAttributeModifierOperator::AMO_MultiplyAdditive)
			{
				return Settings && Settings->bAllowMultiplyAdditiveWithUseAdditiveSum
					? ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed
					: ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
			}

			return ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
		}

		// Custom Merger：默认允许全部内建 Operator；设置只能收紧。
		return ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
	}

	bool RuleMatches(
		const FTcsAttributeOperatorMergerRule& Rule,
		TSubclassOf<UTcsAttributeModifierMerger> MergerType,
		ETcsAttributeModifierOperator Operator,
		TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass)
	{
		if (Rule.Operator != Operator)
		{
			return false;
		}

		if (Rule.MergerType && !IsExactOrChildOf(MergerType.Get(), Rule.MergerType.Get()))
		{
			return false;
		}

		if (Operator == ETcsAttributeModifierOperator::AMO_Custom &&
			Rule.CustomOperatorClass &&
			!IsExactOrChildOf(CustomOperatorClass.Get(), Rule.CustomOperatorClass.Get()))
		{
			return false;
		}

		return true;
	}
}



ETcsAttributeOperatorMergerCompatibility FTcsAttributeModifierCompatibility::EvaluateOperatorMergerCompatibility(
	TSubclassOf<UTcsAttributeModifierMerger> MergerType,
	ETcsAttributeModifierOperator Operator,
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass,
	const UTcsDeveloperSettings* Settings)
{
	if (Settings)
	{
		for (const FTcsAttributeOperatorMergerRule& Rule : Settings->AttributeOperatorMergerRules)
		{
			if (Rule.Compatibility == ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden &&
				RuleMatches(Rule, MergerType, Operator, CustomOperatorClass))
			{
				return ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden;
			}
		}

		for (const FTcsAttributeOperatorMergerRule& Rule : Settings->AttributeOperatorMergerRules)
		{
			if (Rule.Compatibility == ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed &&
				RuleMatches(Rule, MergerType, Operator, CustomOperatorClass))
			{
				return ETcsAttributeOperatorMergerCompatibility::AOMC_Allowed;
			}
		}
	}

	return EvaluateBuiltInDefault(MergerType, Operator, Settings);
}

bool FTcsAttributeModifierCompatibility::IsSelectionOrAggregationMerger(
	TSubclassOf<UTcsAttributeModifierMerger> MergerType)
{
	if (!MergerType || IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_NoMerge::StaticClass()))
	{
		return false;
	}

	return IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseNewest::StaticClass()) ||
		IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseOldest::StaticClass()) ||
		IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseMaximum::StaticClass()) ||
		IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseMinimum::StaticClass()) ||
		IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_UseAdditiveSum::StaticClass()) ||
		!IsExactOrChildOf(MergerType.Get(), UTcsAttrModMerger_NoMerge::StaticClass());
}

bool FTcsAttributeModifierCompatibility::ValidateModifierDefinitionCompatibility(
	const UTcsAttributeModifierDefinition& ModifierDefinition,
	TArray<FText>& OutErrors,
	TArray<FText>& OutWarnings,
	const UTcsDeveloperSettings* Settings)
{
	OutErrors.Reset();
	OutWarnings.Reset();

	if (!ModifierDefinition.MergerType)
	{
		OutErrors.Add(FText::FromString(TEXT("MergerType cannot be empty.")));
		return false;
	}

	if (ModifierDefinition.MergerType->HasAnyClassFlags(CLASS_Abstract))
	{
		OutErrors.Add(FText::FromString(TEXT("MergerType cannot reference an abstract class.")));
	}

	if (ModifierDefinition.Operations.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("Operations cannot be empty.")));
	}

	const bool bMultiOperation = ModifierDefinition.Operations.Num() > 1;
	const bool bSelectionOrAggregation = IsSelectionOrAggregationMerger(ModifierDefinition.MergerType);
	if (bMultiOperation && bSelectionOrAggregation)
	{
		const FText MultiOpMessage = FText::FromString(TEXT(
			"Multiple Operations with a selection/aggregation Merger is not allowed by default. Use NoMerge or reduce to a single Operation."));
		const bool bAsError = !Settings || Settings->bMultiOperationSelectionMergerIsError;
		if (bAsError)
		{
			OutErrors.Add(MultiOpMessage);
		}
		else
		{
			OutWarnings.Add(MultiOpMessage);
		}
	}

	for (const TPair<FName, FTcsAttributeOperationSpec>& OperationPair : ModifierDefinition.Operations)
	{
		const FName OperationId = OperationPair.Key;
		const FTcsAttributeOperationSpec& OperationSpec = OperationPair.Value;

		if (OperationId.IsNone())
		{
			OutErrors.Add(FText::FromString(TEXT("OperationId cannot be None.")));
		}

		if (OperationSpec.TargetAttributeId.IsNone())
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' is missing TargetAttributeId."),
				*OperationId.ToString())));
		}

		if (!OperationSpec.OperandEvaluatorClass ||
			OperationSpec.OperandEvaluatorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' requires a concrete OperandEvaluatorClass."),
				*OperationId.ToString())));
		}

		const UScriptStruct* const PayloadStruct = OperationSpec.OperandPayload.GetScriptStruct();
		if (!PayloadStruct || !PayloadStruct->IsChildOf(FTcsAttributeOperandPayload::StaticStruct()))
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' requires a valid OperandPayload."),
				*OperationId.ToString())));
		}

		if (OperationSpec.Operator == ETcsAttributeModifierOperator::AMO_None)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' requires a non-None Operator."),
				*OperationId.ToString())));
			continue;
		}

		if (OperationSpec.Operator == ETcsAttributeModifierOperator::AMO_Custom &&
			(!OperationSpec.CustomOperatorClass || OperationSpec.CustomOperatorClass->HasAnyClassFlags(CLASS_Abstract)))
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' requires a concrete CustomOperatorClass."),
				*OperationId.ToString())));
		}

		const ETcsAttributeOperatorMergerCompatibility Compatibility = EvaluateOperatorMergerCompatibility(
			ModifierDefinition.MergerType,
			OperationSpec.Operator,
			OperationSpec.CustomOperatorClass,
			Settings);
		if (Compatibility == ETcsAttributeOperatorMergerCompatibility::AOMC_Forbidden)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("Operation '%s' Operator is Forbidden with Merger '%s'."),
				*OperationId.ToString(),
				*GetNameSafe(ModifierDefinition.MergerType.Get()))));
		}
	}

	return OutErrors.IsEmpty();
}
