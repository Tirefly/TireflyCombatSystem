// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillModifierDefinition.h"

#include "Skill/SkillModExecution/TcsSkillModExec_Addition.h"
#include "Skill/SkillModExecution/TcsSkillModExec_SetBool.h"
#include "Skill/SkillModExecution/TcsSkillModExec_SetVector.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



namespace
{
	bool IsAbstractSkillModifierEvaluatorClass(const UClass* StrategyClass)
	{
		return StrategyClass && StrategyClass->HasAnyClassFlags(CLASS_Abstract);
	}
}



const FPrimaryAssetType UTcsSkillModifierDefinition::PrimaryAssetType = TEXT("TcsSkillModifierDef");


void NormalizeSkillModifierStrategyDefaults(UTcsSkillModifierDefinition& SkillModifierDefinition)
{
	if (!SkillModifierDefinition.NumericEvaluatorClass)
	{
		SkillModifierDefinition.NumericEvaluatorClass = UTcsSkillModExec_Addition::StaticClass();
	}

	if (!SkillModifierDefinition.BoolEvaluatorClass)
	{
		SkillModifierDefinition.BoolEvaluatorClass = UTcsSkillModExec_SetBool::StaticClass();
	}

	if (!SkillModifierDefinition.VectorEvaluatorClass)
	{
		SkillModifierDefinition.VectorEvaluatorClass = UTcsSkillModExec_SetVector::StaticClass();
	}
}


bool ValidateSkillModifierStrategySelection(
	const UTcsSkillModifierDefinition& SkillModifierDefinition,
	FString& OutError)
{
	OutError.Reset();

	if (!SkillModifierDefinition.TargetParamTag.IsValid())
	{
		OutError = TEXT("TargetParamTag cannot be empty");
		return false;
	}

	switch (SkillModifierDefinition.TargetParamType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		if (!SkillModifierDefinition.NumericEvaluatorClass)
		{
			OutError = TEXT("NumericEvaluatorClass cannot be empty when TargetParamType is Numeric");
			return false;
		}

		if (IsAbstractSkillModifierEvaluatorClass(SkillModifierDefinition.NumericEvaluatorClass.Get()))
		{
			OutError = TEXT("NumericEvaluatorClass cannot reference an abstract class");
			return false;
		}

		return true;

	case ETcsStateParameterType::SPT_Bool:
		if (!SkillModifierDefinition.BoolEvaluatorClass)
		{
			OutError = TEXT("BoolEvaluatorClass cannot be empty when TargetParamType is Bool");
			return false;
		}

		if (IsAbstractSkillModifierEvaluatorClass(SkillModifierDefinition.BoolEvaluatorClass.Get()))
		{
			OutError = TEXT("BoolEvaluatorClass cannot reference an abstract class");
			return false;
		}

		return true;

	case ETcsStateParameterType::SPT_Vector:
		if (!SkillModifierDefinition.VectorEvaluatorClass)
		{
			OutError = TEXT("VectorEvaluatorClass cannot be empty when TargetParamType is Vector");
			return false;
		}

		if (IsAbstractSkillModifierEvaluatorClass(SkillModifierDefinition.VectorEvaluatorClass.Get()))
		{
			OutError = TEXT("VectorEvaluatorClass cannot reference an abstract class");
			return false;
		}

		return true;

	default:
		OutError = TEXT("Unsupported TargetParamType");
		return false;
	}
}


UTcsSkillModifierDefinition::UTcsSkillModifierDefinition()
{
	NormalizeSkillModifierStrategyDefaults(*this);
}


FPrimaryAssetId UTcsSkillModifierDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, ModifierId);
}


UClass* UTcsSkillModifierDefinition::ResolveActiveEvaluatorClass() const
{
	switch (TargetParamType)
	{
	case ETcsStateParameterType::SPT_Numeric:
		return NumericEvaluatorClass.Get();

	case ETcsStateParameterType::SPT_Bool:
		return BoolEvaluatorClass.Get();

	case ETcsStateParameterType::SPT_Vector:
		return VectorEvaluatorClass.Get();

	default:
		return nullptr;
	}
}


#if WITH_EDITOR
void UTcsSkillModifierDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, TargetParamType) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, NumericEvaluatorClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, BoolEvaluatorClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, VectorEvaluatorClass) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillModifierDefinition, TargetParamTag))
	{
		NormalizeSkillModifierStrategyDefaults(*this);
	}
}


EDataValidationResult UTcsSkillModifierDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (ModifierId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("ModifierId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	if (!EntrySelectorClass)
	{
		Context.AddWarning(FText::FromString(TEXT("EntrySelectorClass is empty, SkillModifier currently cannot resolve target SkillEntry")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	FString StrategyError;
	if (!ValidateSkillModifierStrategySelection(*this, StrategyError))
	{
		Context.AddError(FText::FromString(StrategyError));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
