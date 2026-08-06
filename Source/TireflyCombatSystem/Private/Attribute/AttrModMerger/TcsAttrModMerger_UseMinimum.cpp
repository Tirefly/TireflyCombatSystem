// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMinimum.h"



void UTcsAttrModMerger_UseMinimum::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	ensureMsgf(ModifiersToMerge.IsEmpty() && MergedModifiers.IsEmpty(),
		TEXT("[%s] Legacy Magnitude-based AttributeModifier mergers are unsupported after the Operation refactor."),
		*FString(__FUNCTION__));
}
