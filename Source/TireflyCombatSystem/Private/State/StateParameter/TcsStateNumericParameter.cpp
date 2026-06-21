// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateNumericParameter.h"



bool UTcsStateNumericParamEvaluator::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	float& OutValue) const
{
	if (auto ConstNumericParam = Payload.GetPtr<FTcsStateNumericParam_Constant>())
	{
		OutValue = ConstNumericParam->NumericValue;
		return true;
	}

	return false;
}
