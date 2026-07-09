// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillDefinition.h"

#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"
#include "TcsDeveloperSettings.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif



namespace
{
	bool IsAbstractStateParamStrategyClass(const UClass* StrategyClass)
	{
		return StrategyClass && StrategyClass->HasAnyClassFlags(CLASS_Abstract);
	}

	void NormalizeSkillCooldownParameter(FTcsStateParameter& CooldownParam)
	{
		CooldownParam.ParameterType = ETcsStateParameterType::SPT_Numeric;
		NormalizeStateParameterStrategyDefaults(CooldownParam);
	}

	bool ValidateCooldownStateParameter(const FTcsStateParameter& CooldownParam, FString& OutError)
	{
		OutError.Reset();

		if (!CooldownParam.NumericParamEvaluator)
		{
			OutError = TEXT("NumericParamEvaluator cannot be empty when ParameterType is Numeric");
			return false;
		}

		if (IsAbstractStateParamStrategyClass(CooldownParam.NumericParamEvaluator.Get()))
		{
			OutError = TEXT("NumericParamEvaluator cannot reference an abstract class");
			return false;
		}

		return true;
	}
}



const FPrimaryAssetType UTcsSkillDefinition::PrimaryAssetType = FPrimaryAssetType("TcsSkillDef");



UTcsSkillDefinition::UTcsSkillDefinition()
{
	// 从 DeveloperSettings 读取冷却参数默认 Tag
	if (const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>())
	{
		if (Settings->DefaultSkillCooldownParamTag.IsValid())
		{
			CooldownParamTag = Settings->DefaultSkillCooldownParamTag;
		}
	}

	NormalizeSkillCooldownParameter(CooldownParam);
}


FPrimaryAssetId UTcsSkillDefinition::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(PrimaryAssetType, StateDefId);
}


UClass* UTcsSkillDefinition::ResolveSkillEntryClass() const
{
	return SkillEntryClass ? SkillEntryClass.Get() : UTcsSkillEntry::StaticClass();
}

UClass* UTcsSkillDefinition::ResolveStateInstanceClass() const
{
	return SkillInstanceClass ? SkillInstanceClass.Get() : UTcsSkillInstance::StaticClass();
}


#if WITH_EDITOR
void UTcsSkillDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillDefinition, CooldownParam) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(UTcsSkillDefinition, CooldownParamTag))
	{
		NormalizeSkillCooldownParameter(CooldownParam);
	}
}


EDataValidationResult UTcsSkillDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);

	if (!CooldownParamTag.IsValid())
	{
		return Result;
	}

	if (CooldownParam.ParameterType != ETcsStateParameterType::SPT_Numeric)
	{
		Context.AddError(FText::FromString(TEXT("CooldownParam.ParameterType must stay Numeric")));
		Result = EDataValidationResult::Invalid;
	}

	FString StrategyError;
	if (!ValidateCooldownStateParameter(CooldownParam, StrategyError))
	{
		Context.AddError(FText::FromString(FString::Printf(
			TEXT("CooldownParam invalid: %s"),
			*StrategyError)));
		Result = EDataValidationResult::Invalid;
	}

	return Result;
}
#endif
