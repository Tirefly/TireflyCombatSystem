// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"



bool UTcsAttributeClampStrategy::CollectDependentAttributes_Implementation(
	FName AttributeName,
	const UTcsAttributeDefinition* AttributeDef,
	const FInstancedStruct& Config,
	TArray<FName>& OutAttributeNames) const
{
	OutAttributeNames.Reset();
	return false;
}
