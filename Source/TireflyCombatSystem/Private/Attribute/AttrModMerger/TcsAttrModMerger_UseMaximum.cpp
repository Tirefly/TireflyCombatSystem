// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMaximum.h"



void UTcsAttrModMerger_UseMaximum::Merge_Implementation(
	UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	const FName MagnitudeKey = FName("Magnitude");
	int32 MaxIndex = 0;
	const float* MaxMagnitude = ModifiersToMerge[0].Operands.Find(MagnitudeKey);

	for (int32 i = 1; i < ModifiersToMerge.Num(); ++i)
	{
		const float* CurrentMagnitude = ModifiersToMerge[i].Operands.Find(MagnitudeKey);
		if (CurrentMagnitude && (!MaxMagnitude || *CurrentMagnitude > *MaxMagnitude))
		{
			MaxMagnitude = CurrentMagnitude;
			MaxIndex = i;
		}
	}

	MergedModifiers.Add(ModifiersToMerge[MaxIndex]);
}
