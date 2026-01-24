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

	const FName MagnitudeKey(TEXT("Magnitude"));

	// NOTE:
	// - Do NOT reference ModifiersToMerge[0] directly: we should not mutate caller-owned input arrays.
	// - Initialize magnitude to 0 then sum all instances (avoids double-counting the first element).
	FTcsAttributeModifierInstance MergedModifier = ModifiersToMerge[0];
	float& MagnitudeToMerge = MergedModifier.Operands.FindOrAdd(MagnitudeKey);
	MagnitudeToMerge = 0.0f;

	for (const FTcsAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		if (const float* Magnitude = Modifier.Operands.Find(MagnitudeKey))
		{
			MagnitudeToMerge += *Magnitude;
		}
	}

	MergedModifiers.Add(MergedModifier);
}
