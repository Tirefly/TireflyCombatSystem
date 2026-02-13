// Copyright Tirefly. All Rights Reserved.


#include "State/StateMerger/TcsStateMerger_StackDirectly.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"



void UTcsStateMerger_StackDirectly::Merge_Implementation(
	TArray<UTcsStateInstance*>& StatesToMerge,
	TArray<UTcsStateInstance*>& MergedStates)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTcsStateInstance* ReferenceState = StatesToMerge[0];
	const FName ReferenceStateDefId = ReferenceState->GetStateDefId();

	// 验证所有状态的StateDefId是否相同
	for (UTcsStateInstance* State : StatesToMerge)
	{
		if (State->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsStateMerger, Error, TEXT("[%s] StateDefId mismatch."),
				*FString(__FUNCTION__));
			return;
		}
	}

	/**
	 * 按时间戳排序，最旧的状态在数组最后，也就是栈顶，
	 * 之所以选择最旧的状态实例作为栈顶基础状态，因为
	 * 该状态更早被应用，状态树已经开始执行，剩余持续
	 * 时间也已经开始计时
	 */
	StatesToMerge.Sort([](
		const UTcsStateInstance& A,
		const UTcsStateInstance& B) {
		return A.GetApplyTimestamp() > B.GetApplyTimestamp();
	});

	// 计算总叠层数
	int32 TotalStackCount = 0;
	for (UTcsStateInstance* State : StatesToMerge)
	{
		const int32 StateStackCount = State->GetStackCount();
		if (StateStackCount < 0)
		{
			UE_LOG(LogTcsStateMerger, Error,
				TEXT("[%s] State '%s' has invalid StackCount (%d). "
					 "Check DataTable config: MaxStackCount should be > 0 when using StackDirectly Merger."),
				*FString(__FUNCTION__),
				*State->GetStateDefId().ToString(),
				StateStackCount);
			return;
		}

		// StackCount为0的状态即将被移除，跳过
		if (StateStackCount == 0)
		{
			UE_LOG(LogTcsStateMerger, Verbose,
				TEXT("[%s] Skipping state '%s' with StackCount=0 (pending removal)"),
				*FString(__FUNCTION__),
				*State->GetStateDefId().ToString());
			continue;
		}

		TotalStackCount += StateStackCount;
	}

	// 从排序后的状态中弹出最新的状态作为基础状态
	// NOTE: Pop() returns the oldest instance here because we sort by ApplyTimestamp descending.
	UTcsStateInstance* BaseState = StatesToMerge.Pop();
		
	// 设置基础状态的叠层数
	BaseState->SetStackCount(TotalStackCount);
	MergedStates.Add(BaseState);
}
