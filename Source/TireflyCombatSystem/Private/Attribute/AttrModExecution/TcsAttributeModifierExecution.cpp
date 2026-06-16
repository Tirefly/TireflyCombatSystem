// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModExecution/TcsAttributeModifierExecution.h"

#include "Attribute/TcsAttributeModifierDefinition.h"



bool UTcsAttributeModifierExecution::CollectTouchedAttributes_Implementation(
	const FTcsAttributeModifierInstance& ModInst,
	TArray<FName>& OutAttributeNames) const
{
	OutAttributeNames.Reset();

	if (!ModInst.ModifierDef)
	{
		return false;
	}

	const FName AttributeName = ModInst.ModifierDef->AttributeName;
	if (AttributeName.IsNone())
	{
		return false;
	}

	OutAttributeNames.Add(AttributeName);
	return true;
}
