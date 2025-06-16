// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TireflyStateMerger_NoMerge.h"



void UTireflyStateMerger_NoMerge::Merge_Implementation(
	TArray<UTireflyStateInstance*>& StatesToMerge,
	TArray<UTireflyStateInstance*>& MergedStates)
{
	// 不合并，直接返回所有状态
	MergedStates = StatesToMerge;
} 