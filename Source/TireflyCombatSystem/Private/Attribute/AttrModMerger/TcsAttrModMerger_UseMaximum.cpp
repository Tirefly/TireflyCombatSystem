// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMaximum.h"



void UTcsAttrModMerger_UseMaximum::Merge_Implementation(
	UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	ensureMsgf(ModifiersToMerge.IsEmpty() && MergedModifiers.IsEmpty(),
		TEXT("[%s] Legacy Magnitude-based AttributeModifier mergers are unsupported after the Operation refactor."),
		*FString(__FUNCTION__));
}
