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
	
	auto GetEffectiveTimestamp = [](const FTcsAttributeModifierInstance& Modifier) -> int64
	{
		return (Modifier.UpdateTimestamp >= 0) ? Modifier.UpdateTimestamp : Modifier.ApplyTimestamp;
	};

	int32 OldestModIndex = 0;
	for (int32 i = 1; i < ModifiersToMerge.Num(); i++)
	{
		if (GetEffectiveTimestamp(ModifiersToMerge[i]) < GetEffectiveTimestamp(ModifiersToMerge[OldestModIndex]))
		{
			OldestModIndex = i;
		}
	}

	MergedModifiers.Add(ModifiersToMerge[OldestModIndex]);
}
