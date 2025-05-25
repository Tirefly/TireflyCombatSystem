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

	FTireflyAttributeModifierInstance MergedModifier = ModifiersToMerge[0];
	for (const FTireflyAttributeModifierInstance& Modifier : ModifiersToMerge)
	{
		for (const TPair<FName, float>& ModOperand : Modifier.Operands)
		{
			if (float* OperandValue = MergedModifier.Operands.Find(ModOperand.Key))
			{
				*OperandValue += ModOperand.Value;
			}
		}
	}

	MergedModifiers.Add(MergedModifier);
}
