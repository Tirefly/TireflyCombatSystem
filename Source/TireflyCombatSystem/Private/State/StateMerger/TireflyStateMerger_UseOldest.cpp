// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TireflyStateMerger_UseOldest.h"

void UTireflyStateMerger_UseOldest::Merge_Implementation(
	TArray<FTireflyStateInstance>& StatesToMerge,
	TArray<FTireflyStateInstance>& MergedStates,
	bool bSameInstigator)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	int32 OldestStateIndex = 0;
	for (int32 i = 1; i < StatesToMerge.Num(); i++)
	{
		if (StatesToMerge[i].ApplyTimestamp < StatesToMerge[OldestStateIndex].ApplyTimestamp)
		{
			OldestStateIndex = i;
		}
	}

	MergedStates.Add(StatesToMerge[OldestStateIndex]);
} 