// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TcsAttrModMerger_UseNewest.h"

void UTcsAttrModMerger_UseNewest::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	int32 NewestModIndex = 0;
	for (int32 i = 1; i < ModifiersToMerge.Num(); i++)
	{
		if (ModifiersToMerge[i].ApplyTimestamp > ModifiersToMerge[NewestModIndex].ApplyTimestamp)
		{
			NewestModIndex = i;
		}
	}

	MergedModifiers.Add(ModifiersToMerge[NewestModIndex]);
}
