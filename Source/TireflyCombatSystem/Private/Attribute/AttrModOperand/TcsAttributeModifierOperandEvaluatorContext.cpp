// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluatorContext.h"

#include "Attribute/TcsAttributeComponent.h"
#include "Buff/TcsBuffInstance.h"
#include "Skill/TcsSkillEntry.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateParamInstance.h"



bool FTcsAttributeOperandEvaluatorContext::ReadTargetAttributeCurrentValue(
	FName AttributeId,
	float& OutValue) const
{
	if (!AttributeSnapshot || !AttributeSnapshot->GetCurrentValue(AttributeId, OutValue))
	{
		return false;
	}

	if (DependencyCollector)
	{
		DependencyCollector->AddUnique(
			FTcsAttributeModifierDependencyKey::MakeAttributeCurrentValue(AttributeId));
	}

	return true;
}

bool FTcsAttributeOperandEvaluatorContext::ReadTargetAttributeBaseValue(
	FName AttributeId,
	float& OutValue) const
{
	return AttributeSnapshot && AttributeSnapshot->GetBaseValue(AttributeId, OutValue);
}

bool FTcsAttributeOperandEvaluatorContext::ReadSourceStateNumericParamEffectiveValue(
	FGameplayTag ParamTag,
	float& OutValue) const
{
	UTcsStateInstance* const StateInstance = SourceStateInstance;
	if (!StateInstance || !ParamTag.IsValid())
	{
		return false;
	}

	const FTcsNumericStateParamInstance* const ParamInstance = StateInstance->GetNumericParamInstance(ParamTag);
	if (!ParamInstance)
	{
		return false;
	}

	OutValue = ParamInstance->GetModifiedValue();
	if (!FMath::IsFinite(OutValue))
	{
		return false;
	}

	const UTcsBuffInstance* const BuffInstance = Cast<UTcsBuffInstance>(StateInstance);
	if (DependencyCollector &&
		BuffInstance &&
		BuffInstance->GetOwnerAttributeComponent() == TargetAttributeComponent)
	{
		DependencyCollector->AddUnique(
			FTcsAttributeModifierDependencyKey::MakeBuffNumericStateParam(*BuffInstance, ParamTag));
	}

	return true;
}

bool FTcsAttributeOperandEvaluatorContext::ReadSourceSkillEntryNumericParamEffectiveValue(
	FGameplayTag ParamTag,
	float& OutValue) const
{
	UTcsSkillEntry* const SkillEntry = SourceSkillEntry;
	if (!SkillEntry || !ParamTag.IsValid())
	{
		return false;
	}

	const FTcsNumericStateParamInstance* const ParamInstance = SkillEntry->FindNumericParamInstance(ParamTag);
	if (!ParamInstance)
	{
		return false;
	}

	OutValue = ParamInstance->GetModifiedValue();
	return FMath::IsFinite(OutValue);
}
