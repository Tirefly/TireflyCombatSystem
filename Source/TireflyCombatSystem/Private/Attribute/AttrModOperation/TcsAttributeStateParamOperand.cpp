// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperation/TcsAttributeStateParamOperand.h"

#include "Attribute/AttrModOperation/TcsAttributeOperandEvaluatorContext.h"
#include "Skill/TcsSkillEntry.h"
#include "State/TcsStateInstance.h"
#include "State/TcsStateParamInstance.h"



bool UTcsAttributeModifierStateParamOperandEvaluator::Evaluate(
	const FTcsAttributeOperandEvaluatorContext& Context,
	const FInstancedStruct& Payload,
	float& OutOperand) const
{
	const FTcsStateParamOperandPayload* const StateParamPayload = Payload.GetPtr<FTcsStateParamOperandPayload>();
	if (!StateParamPayload || !StateParamPayload->StateParamTag.IsValid())
	{
		return false;
	}

	FTcsNumericStateParamInstance* NumericParamInstance = nullptr;
	switch (StateParamPayload->Source)
	{
	case ETcsAttributeStateParamOperandSource::ASPOS_SourceStateInstance:
		if (!Context.SourceStateInstance)
		{
			return false;
		}

		NumericParamInstance = Context.SourceStateInstance->GetNumericParamInstance(StateParamPayload->StateParamTag);
		break;

	case ETcsAttributeStateParamOperandSource::ASPOS_SourceSkillEntry:
		if (!Context.SourceSkillEntry)
		{
			return false;
		}

		NumericParamInstance = Context.SourceSkillEntry->FindNumericParamInstance(StateParamPayload->StateParamTag);
		break;

	default:
		return false;
	}

	if (!NumericParamInstance)
	{
		return false;
	}

	OutOperand = NumericParamInstance->GetModifiedValue();
	return FMath::IsFinite(OutOperand);
}
