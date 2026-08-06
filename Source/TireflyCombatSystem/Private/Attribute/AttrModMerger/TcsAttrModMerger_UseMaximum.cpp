// Copyright Tirefly. All Rights Reserved.

#include "Attribute/AttrModMerger/TcsAttrModMerger_UseMaximum.h"

#include "Attribute/AttrModOperation/TcsAttributeModifierOperation.h"



void UTcsAttrModMerger_UseMaximum::Merge_Implementation(
	UPARAM(ref) TArray<FTcsAttributeModifierInstance>& ModifiersToMerge,
	TArray<FTcsAttributeModifierInstance>& MergedModifiers)
{
	if (ModifiersToMerge.IsEmpty())
	{
		return;
	}

	FTcsAttributeModifierInstance MergedInstance = ModifiersToMerge[0];
	TMap<FName, FTcsEvaluatedAttributeOperation> BestOperations;
	BestOperations.Reserve(MergedInstance.AppliedOperations.Num());

	for (const FTcsAttributeModifierInstance& ModifierInstance : ModifiersToMerge)
	{
		for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
		{
			if (FTcsEvaluatedAttributeOperation* const Existing = BestOperations.Find(Operation.OperationId))
			{
				if (Operation.EvaluatedOperand > Existing->EvaluatedOperand)
				{
					*Existing = Operation;
				}
			}
			else
			{
				BestOperations.Add(Operation.OperationId, Operation);
			}
		}
	}

	MergedInstance.AppliedOperations.Reset();
	BestOperations.GenerateValueArray(MergedInstance.AppliedOperations);
	MergedInstance.AppliedOperations.Sort([](const FTcsEvaluatedAttributeOperation& Left, const FTcsEvaluatedAttributeOperation& Right)
	{
		return Left.OperationId.LexicalLess(Right.OperationId);
	});
	MergedModifiers.Add(MergedInstance);
}
