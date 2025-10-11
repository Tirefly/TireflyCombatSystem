// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TcsAttrModMerger_NoMerge.h"



void UTcsAttrModMerger_NoMerge::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	MergedModifiers.Append(ModifiersToMerge);
}
