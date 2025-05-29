// Copyright Tirefly. All Rights Reserved.


#include "Attribute/AttrModMerger/TireflyAttrModMerger_UseAdditiveSum.h"



void UTireflyAttrModMerger_UseAdditiveSum::Merge_Implementation(
	TArray<FTireflyAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTireflyAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.Num() <= 1)
	{
		MergedModifiers.Append(ModifiersToMerge);
		return;
	}

	const FName MagnitudeKey = FName("Magnitude");
	FTireflyAttributeModifierInstance& MergedModifier = ModifiersToMerge[0];
	float* MagnitudeToMerge = MergedModifier.Operands.Find(MagnitudeKey);
	
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (const float* Magnitude = Modifier.Operands.Find(MagnitudeKey))
		{
			*MagnitudeToMerge += *Magnitude;
		}
	}

	MergedModifiers.Add(MergedModifier);
}
