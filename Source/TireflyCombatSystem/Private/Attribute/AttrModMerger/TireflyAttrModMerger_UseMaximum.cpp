// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseMaximum.h"



void UTireflyAttrModMerger_UseMaximum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance MaxMagnitudeMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		const float* BiggestMagnitude = MaxMagnitudeMod.Operands.Find(FName("Magnitude"));
		const float* ModifierMagnitude = Modifier.Operands.Find(FName("Magnitude"));
		if (BiggestMagnitude && ModifierMagnitude)
		{
			if (*ModifierMagnitude > *BiggestMagnitude)
			{
				MaxMagnitudeMod = Modifier;
			}
		}
	}

	MergedModifiers.Add(MaxMagnitudeMod);
}
