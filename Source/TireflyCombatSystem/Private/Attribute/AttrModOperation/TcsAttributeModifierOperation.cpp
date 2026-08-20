// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModOperation/TcsAttributeModifierOperation.h"
#include "Attribute/AttrModOperand/TcsAttrModOperand_Constant.h"
#include "Attribute/AttrModOperation/TcsAttributeModifierCustomOperator.h"



FTcsAttributeOperationSpec::FTcsAttributeOperationSpec()
{
	OperandEvaluatorClass = UTcsAttributeModifierConstantOperandEvaluator::StaticClass();
	OperandPayload.InitializeAs<FTcsAttributeConstantOperandPayload>();
}

bool ApplyTcsAttributeModifierOperator(
	ETcsAttributeModifierOperator Operator,
	TSubclassOf<UTcsAttributeModifierCustomOperator> CustomOperatorClass,
	float CurrentValue,
	float Operand,
	float& OutValue)
{
	switch (Operator)
	{
	case ETcsAttributeModifierOperator::AMO_Add:
		OutValue = CurrentValue + Operand;
		return true;

	case ETcsAttributeModifierOperator::AMO_MultiplyAdditive:
		OutValue = CurrentValue * (1.f + Operand);
		return true;

	case ETcsAttributeModifierOperator::AMO_MultiplyCompound:
		OutValue = CurrentValue * Operand;
		return true;

	case ETcsAttributeModifierOperator::AMO_Override:
		OutValue = Operand;
		return true;

	case ETcsAttributeModifierOperator::AMO_Custom:
		if (!CustomOperatorClass || CustomOperatorClass->HasAnyClassFlags(CLASS_Abstract))
		{
			return false;
		}

		return CustomOperatorClass->GetDefaultObject<UTcsAttributeModifierCustomOperator>()->Apply(
			CurrentValue,
			Operand,
			OutValue);

	case ETcsAttributeModifierOperator::AMO_None:
	default:
		return false;
	}
}
