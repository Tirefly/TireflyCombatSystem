// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TcsAttrModMerger_UseOldest.h"



void UTcsAttrModMerger_UseOldest::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}
	
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
