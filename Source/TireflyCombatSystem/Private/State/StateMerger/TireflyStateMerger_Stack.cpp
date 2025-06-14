// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TireflyStateMerger_Stack.h"

void UTireflyStateMerger_Stack::Merge_Implementation(
	TArray<FTireflyStateInstance>& StatesToMerge,
	TArray<FTireflyStateInstance>& MergedStates,
	bool bSameInstigator)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 按时间戳排序，最新的状态在最前面
	StatesToMerge.Sort([](const FTireflyStateInstance& A, const FTireflyStateInstance& B) {
		return A.ApplyTimestamp > B.ApplyTimestamp;
	});

	// 获取第一个状态作为基础状态
	FTireflyStateInstance BaseState = StatesToMerge[0];
	
	// 叠加其他状态的效果
	for (int32 i = 1; i < StatesToMerge.Num(); i++)
	{
		// TODO: 实现状态叠加逻辑
		// 这里需要根据具体的状态类型来实现叠加效果
		// 例如：叠加持续时间、叠加效果值等
	}

	MergedStates.Add(BaseState);
} 