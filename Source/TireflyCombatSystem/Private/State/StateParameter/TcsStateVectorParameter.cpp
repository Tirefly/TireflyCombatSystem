// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TcsStateVectorParameter.h"



void UTcsStateVectorParamEvaluator::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	FVector& OutValue) const
{
	if (auto ConstVectorParam = InstancedStruct.GetPtr<FTcsStateVectorParam_Constant>())
	{
		OutValue = ConstVectorParam->VectorValue;
	}
}
