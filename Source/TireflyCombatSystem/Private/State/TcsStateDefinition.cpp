// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateDefinition.h"

#include "TcsDeveloperSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



namespace
{
	bool IsAbstractTcsStrategyClass(const UClass* StrategyClass)
	{
		return StrategyClass && StrategyClass->HasAnyClassFlags(CLASS_Abstract);
	}

	bool ValidateStateParameterSelectionForDefinition(const FTcsStateParameter& StateParameter, FString& OutError)
	{
		OutError.Reset();

		switch (StateParameter.ParameterType)
		{
		case ETcsStateParameterType::SPT_Numeric:
			if (!StateParameter.NumericParamEvaluator)
			{
				OutError = TEXT("NumericParamEvaluator cannot be empty when ParameterType is Numeric");
				return false;
			}

			if (IsAbstractTcsStrategyClass(StateParameter.NumericParamEvaluator.Get()))
			{
				OutError = TEXT("NumericParamEvaluator cannot reference an abstract class");
				return false;
			}

			return true;

		case ETcsStateParameterType::SPT_Bool:
			if (!StateParameter.BoolParamEvaluator)
			{
				OutError = TEXT("BoolParamEvaluator cannot be empty when ParameterType is Bool");
				return false;
			}

			if (IsAbstractTcsStrategyClass(StateParameter.BoolParamEvaluator.Get()))
			{
				OutError = TEXT("BoolParamEvaluator cannot reference an abstract class");
				return false;
			}

			return true;

		case ETcsStateParameterType::SPT_Vector:
			if (!StateParameter.VectorParamEvaluator)
			{
				OutError = TEXT("VectorParamEvaluator cannot be empty when ParameterType is Vector");
				return false;
			}

			if (IsAbstractTcsStrategyClass(StateParameter.VectorParamEvaluator.Get()))
			{
				OutError = TEXT("VectorParamEvaluator cannot reference an abstract class");
				return false;
			}

			return true;

		default:
			OutError = TEXT("Unsupported StateParameterType");
			return false;
		}
	}

	void NormalizeStateDefinitionParameterDefaults(TMap<FGameplayTag, FTcsStateParameter>& Parameters)
	{
		for (TPair<FGameplayTag, FTcsStateParameter>& Pair : Parameters)
		{
			NormalizeStateParameterStrategyDefaults(Pair.Value);
		}
	}
}



UTcsStateDefinition::UTcsStateDefinition()
{
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		if (Settings->DefaultLevelParamTag.IsValid())
		{
			LevelParamTag = Settings->DefaultLevelParamTag;
		}
	}
}

// 定义 PrimaryAssetType 静态变量
const FPrimaryAssetType UTcsStateDefinition::PrimaryAssetType = FPrimaryAssetType("TcsStateDef");



FPrimaryAssetId UTcsStateDefinition::GetPrimaryAssetId() const
{
	// 使用 StateDefId 作为 PrimaryAssetName
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}

UClass* UTcsStateDefinition::ResolveStateInstanceClass() const
{
	return UTcsStateInstance::StaticClass();
}


#if WITH_EDITOR
void UTcsStateDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	NormalizeStateDefinitionParameterDefaults(Parameters);
}

EDataValidationResult UTcsStateDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	// 验证 StateDefId
	if (StateDefId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("StateDefId cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	// 验证 StateTag（推荐但不强制）
	if (!StateTag.IsValid())
	{
		Context.AddWarning(FText::FromString(TEXT("StateTag is empty, recommend setting to TCS.State.<StateDefId>")));
		if (Result == EDataValidationResult::Valid)
		{
			Result = EDataValidationResult::NotValidated;
		}
	}

	// 验证 StateSlotType
	if (!StateSlotType.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("StateSlotType cannot be empty")));
		Result = EDataValidationResult::Invalid;
	}

	for (const TPair<FGameplayTag, FTcsStateParameter>& Pair : Parameters)
	{
		if (!Pair.Key.IsValid())
		{
			Context.AddError(FText::FromString(TEXT("Parameters contains an invalid GameplayTag key")));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		FString StrategyError;
		if (!ValidateStateParameterSelectionForDefinition(Pair.Value, StrategyError))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Parameter '%s' invalid: %s"),
				*Pair.Key.ToString(),
				*StrategyError)));
			Result = EDataValidationResult::Invalid;
		}
	}

	for (int32 ConditionIndex = 0; ConditionIndex < ActiveConditions.Num(); ++ConditionIndex)
	{
		const FTcsStateConditionConfig& ConditionConfig = ActiveConditions[ConditionIndex];

		if (!ConditionConfig.ConditionClass)
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("ActiveConditions[%d].ConditionClass cannot be empty"),
				ConditionIndex)));
			Result = EDataValidationResult::Invalid;
			continue;
		}

		if (IsAbstractTcsStrategyClass(ConditionConfig.ConditionClass.Get()))
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("ActiveConditions[%d].ConditionClass cannot reference an abstract class"),
				ConditionIndex)));
			Result = EDataValidationResult::Invalid;
		}

		if (!ConditionConfig.Payload.IsValid())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("ActiveConditions[%d].Payload must be valid when a condition entry exists"),
				ConditionIndex)));
			Result = EDataValidationResult::Invalid;
		}
	}

	return Result;
}
#endif
