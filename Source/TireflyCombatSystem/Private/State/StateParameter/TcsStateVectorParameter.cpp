// Copyright Tirefly. All Rights Reserved.

#include "State/StateParameter/TcsStateVectorParameter.h"



bool UTcsStateVectorParamEvaluator::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& Payload,
	FVector& OutValue) const
{
	if (auto ConstVectorParam = Payload.GetPtr<FTcsStateVectorParam_Constant>())
	{
		OutValue = ConstVectorParam->VectorValue;
		return true;
	}

	return false;
}
