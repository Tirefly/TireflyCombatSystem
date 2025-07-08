// Copyright Tirefly. All Rights Reserved.


#include "State/StateParameter/TireflyStateParameter_ConstNumeric.h"



void UTireflyStateParamParser_ConstNumeric::ParseStateParameter_Implementation(
	AActor* Instigator,
	AActor* Target,
	UTireflyStateInstance* StateInstance,
	const FInstancedStruct& InstancedStruct,
	float& OutValue) const
{
	if (auto ConstNumericParam = InstancedStruct.GetPtr<FTireflyStateParam_ConstNumeric>())
	{
		OutValue = ConstNumericParam->NumericValue;
	}
}
