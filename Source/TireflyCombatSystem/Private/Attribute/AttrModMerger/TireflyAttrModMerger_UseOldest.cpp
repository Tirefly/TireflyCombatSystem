// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseOldest.h"



void UTireflyAttrModMerger_UseOldest::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	int32 OldestModIndex = 0;
	for (int32 i = 1; i < ModifiersToMerge.Num(); i++)
	{
		if (ModifiersToMerge[i].ApplyTimestamp < ModifiersToMerge[OldestModIndex].ApplyTimestamp)
		{
			OldestModIndex = i;
		}
	}

	MergedModifiers.Add(ModifiersToMerge[OldestModIndex]);
}
