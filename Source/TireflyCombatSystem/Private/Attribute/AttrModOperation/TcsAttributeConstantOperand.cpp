// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperation/TcsAttributeConstantOperand.h"



bool UTcsAttributeModifierConstantOperandEvaluator::Evaluate(
	const FTcsAttributeOperandEvaluatorContext& Context,
	const FInstancedStruct& Payload,
	float& OutOperand) const
{
	(void)Context;

	const FTcsAttributeConstantOperandPayload* const ConstantPayload = Payload.GetPtr<FTcsAttributeConstantOperandPayload>();

	if (!ConstantPayload)
	{
		return false;
	}

	OutOperand = ConstantPayload->Value;
	return true;
}
