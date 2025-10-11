// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TcsStateParameter_ConstNumeric.h"



void UTcsStateParamParser_ConstNumeric::Evaluate_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTcsStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto ConstNumericParam = InstancedStruct.GetPtr<FTcsStateParam_ConstNumeric>())
	{
		OutValue = ConstNumericParam->NumericValue;
	}
}
