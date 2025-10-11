// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TcsAttrModMerger_UseAdditiveSum.h"



void UTcsAttrModMerger_UseAdditiveSum::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.Num() <= 1)
	{
		MergedModifiers.Append(ModifiersToMerge);
		return;
	}

	const FName MagnitudeKey = FName("Magnitude");
	FTcsAttributeModifierInstance& MergedModifier = ModifiersToMerge[0];
	float* MagnitudeToMerge = MergedModifier.Operands.Find(MagnitudeKey);
	
	for (const FTcsAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (const float* Magnitude = Modifier.Operands.Find(MagnitudeKey))
		{
			*MagnitudeToMerge += *Magnitude;
		}
	}

	MergedModifiers.Add(MergedModifier);
}
