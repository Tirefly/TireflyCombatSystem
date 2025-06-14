// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TireflyStateMerger_UseNewest.h"

void UTireflyStateMerger_UseNewest::Merge_Implementation(
	TArray<FTireflyStateInstance>& StatesToMerge,
	TArray<FTireflyStateInstance>& MergedStates,
	bool bSameInstigator)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	int32 NewestStateIndex = 0;
	for (int32 i = 1; i < StatesToMerge.Num(); i++)
	{
		if (StatesToMerge[i].ApplyTimestamp > StatesToMerge[NewestStateIndex].ApplyTimestamp)
		{
			NewestStateIndex = i;
		}
	}

	MergedStates.Add(StatesToMerge[NewestStateIndex]);
} 