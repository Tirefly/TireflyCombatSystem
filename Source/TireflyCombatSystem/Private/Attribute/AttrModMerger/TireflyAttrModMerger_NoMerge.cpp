// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_NoMerge.h"

void UTireflyAttrModMerger_NoMerge::Merge_Implementation(TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	MergedModifiers.Append(ModifiersToMerge);
}
