// Copyright Tirefly. All Rights Reserved.

#include "State/StateMerger/TcsStateMerger_UseNewest.h"

#include "State/TcsState.h"



void UTcsStateMerger_UseNewest::Merge_Implementation(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& MergedStates)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 按时间戳排序，最新的状态在最前面
	StatesToMerge.Sort([](
		const UTcsStateInstance& A,
		const UTcsStateInstance& B) {
		return A.GetApplyTimestamp() > B.GetApplyTimestamp();
	});

	// 只保留最新的状态
	MergedStates.Add(StatesToMerge[0]);

	// 把合并操作后剩余的状态实例标记为待回到对象池中
	for (int32 i = 1; i < StatesToMerge.Num(); ++i)
	{
		// TODO: 把合并后剩余的状态实例标记为待回到对象池中
	}
} 