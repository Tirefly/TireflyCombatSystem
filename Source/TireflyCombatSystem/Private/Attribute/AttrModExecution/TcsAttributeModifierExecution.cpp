// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"



bool UTcsAttributeModifierExecution::CollectTouchedAttributes_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	TArray<FName>& OutAttributeNames) const
{
	OutAttributeNames.Reset();
	return false;
}
