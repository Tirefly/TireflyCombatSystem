// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TcsStateMerger_NoMerge.h"



void UTcsStateMerger_NoMerge::Merge_Implementation(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& MergedStates)
{
	// 不合并，直接返回所有状态
	MergedStates = StatesToMerge;
} 