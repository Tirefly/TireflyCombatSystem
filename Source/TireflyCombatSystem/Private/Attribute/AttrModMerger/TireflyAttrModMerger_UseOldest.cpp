// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseOldest.h"



void UTireflyAttrModMerger_UseOldest::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance OldestMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (Modifier.ApplyTimestamp < OldestMod.ApplyTimestamp)
		{
			OldestMod = Modifier;
		}
	}

	MergedModifiers.Add(OldestMod);
}
