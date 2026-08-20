// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperand/TcsAttrModOperand_StateParam.h"

#include "Attribute/AttrModOperand/TcsAttributeModifierOperandEvaluatorContext.h"



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

	switch (StateParamPayload->Source)
	{
	case ETcsAttributeStateParamOperandSource::ASPOS_SourceStateInstance:
		return Context.ReadSourceStateNumericParamEffectiveValue(StateParamPayload->StateParamTag, OutOperand);

	case ETcsAttributeStateParamOperandSource::ASPOS_SourceSkillEntry:
		return Context.ReadSourceSkillEntryNumericParamEffectiveValue(StateParamPayload->StateParamTag, OutOperand);

	default:
		return false;
	}
}
