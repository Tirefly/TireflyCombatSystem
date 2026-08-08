// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperation/TcsAttributeCurrentValueOperand.h"

#include "Attribute/AttrModOperation/TcsAttributeOperandEvaluatorContext.h"



bool UTcsAttributeModifierCurrentValueOperandEvaluator::Evaluate(
	const FTcsAttributeOperandEvaluatorContext& Context,
	const FInstancedStruct& Payload,
	float& OutOperand) const
{
	const FTcsAttributeCurrentValueOperandPayload* const AttributePayload =
		Payload.GetPtr<FTcsAttributeCurrentValueOperandPayload>();
	if (!AttributePayload || AttributePayload->AttributeId.IsNone())
	{
		return false;
	}

	return Context.ReadTargetAttributeCurrentValue(AttributePayload->AttributeId, OutOperand) &&
		FMath::IsFinite(OutOperand);
}
