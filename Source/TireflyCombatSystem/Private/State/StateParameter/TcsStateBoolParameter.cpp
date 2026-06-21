// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateBoolParameter.h"



bool UTcsStateBoolParamEvaluator::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	bool& OutValue) const
{
	if (auto ConstBoolParam = Payload.GetPtr<FTcsStateBoolParam_Constant>())
	{
		OutValue = ConstBoolParam->bBoolValue;
		return true;
	}

	return false;
}
