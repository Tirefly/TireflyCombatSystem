// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModMerger/TcsAttrModMerger_UseAdditiveSum.h"

#include "Attribute/AttrModOperation/TcsAttributeModifierOperation.h"



void UTcsAttrModMerger_UseAdditiveSum::Merge_Implementation(
	TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	if (ModifiersToMerge.Num() == 1)
	{
		MergedModifiers.Append(ModifiersToMerge);
		return;
	}

	FTcsAttributeModifierInstance MergedInstance = ModifiersToMerge[0];
	TMap<FName, FTcsEvaluatedAttributeOperation> AggregatedOperations;
	AggregatedOperations.Reserve(MergedInstance.AppliedOperations.Num());

	for (const FTcsAttributeModifierInstance& ModifierInstance : ModifiersToMerge)
	{
		for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
		{
			if (FTcsEvaluatedAttributeOperation* const Existing = AggregatedOperations.Find(Operation.OperationId))
			{
				Existing->EvaluatedOperand += Operation.EvaluatedOperand;
			}
			else
			{
				AggregatedOperations.Add(Operation.OperationId, Operation);
			}
		}
	}

	MergedInstance.AppliedOperations.Reset();
	AggregatedOperations.GenerateValueArray(MergedInstance.AppliedOperations);
	MergedInstance.AppliedOperations.Sort([](const FTcsEvaluatedAttributeOperation& Left, const FTcsEvaluatedAttributeOperation& Right)
	{
		return Left.OperationId.LexicalLess(Right.OperationId);
	});
	MergedModifiers.Add(MergedInstance);
}
