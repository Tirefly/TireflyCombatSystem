// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseNewest.h"

void UTireflyAttrModMerger_UseNewest::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTireflyAttributeModifierInstance NewestMod = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (Modifier.ApplyTimestamp > NewestMod.ApplyTimestamp)
		{
			NewestMod = Modifier;
		}
	}

	MergedModifiers.Add(NewestMod);
}
