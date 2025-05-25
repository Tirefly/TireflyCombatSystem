// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseMinimum.h"



void UTireflyAttrModMerger_UseMinimum::Merge_Implementation(TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance MinMagnitudeMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		const float* BiggestMagnitude = MinMagnitudeMod.Operands.Find(FName("Magnitude"));
		const float* ModifierMagnitude = Modifier.Operands.Find(FName("Magnitude"));
		if (BiggestMagnitude && ModifierMagnitude)
		{
			if (*ModifierMagnitude < *BiggestMagnitude)
			{
				MinMagnitudeMod = Modifier;
			}
		}
	}

	MergedModifiers.Add(MinMagnitudeMod);
}
