// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseMinimum.h"



void UTireflyAttrModMerger_UseMinimum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	const FName MagnitudeKey = FName("Magnitude");
	int32 MinIndex = 0;
	const float* MinMagnitude = ModifiersToMerge[0].Operands.Find(MagnitudeKey);

	for (int32 i = 1; i < ModifiersToMerge.Num(); ++i)
	{
		const float* CurrentMagnitude = ModifiersToMerge[i].Operands.Find(MagnitudeKey);
		if (CurrentMagnitude && (!MinMagnitude || *CurrentMagnitude < *MinMagnitude))
		{
			MinMagnitude = CurrentMagnitude;
			MinIndex = i;
		}
	}

	MergedModifiers.Add(ModifiersToMerge[MinIndex]);
}
