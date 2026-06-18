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

	const FName AttributeId = ModInst.ModifierDef->AttributeId;
	if (AttributeId.IsNone())
	{
		return false;
	}

	OutAttributeNames.Add(AttributeId);
	return true;
}
