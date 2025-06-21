// Copyright Tirefly. All Rights Reserved.


#include "State/StateMerger/TireflyStateMerger_Stack.h"

#include "TireflyCombatSystemLogChannels.h"
#include "State/TireflyState.h"



void UTireflyStateMerger_Stack::Merge_Implementation(
	TArray<UTireflyStateInstance*>& StatesToMerge,
	TArray<UTireflyStateInstance*>& MergedStates)
{
	if (StatesToMerge.IsEmpty())
	{
		return;
	}

	// 获取第一个状态作为参考状态，用于获取StateDefId
	UTireflyStateInstance* ReferenceState = StatesToMerge[0];
	const FName ReferenceStateDefId = ReferenceState->GetStateDefId();

	// 按Instigator分组
	TMap<AActor*, TArray<UTireflyStateInstance*>> StatesByInstigator;
	for (UTireflyStateInstance* State : StatesToMerge)
	{
		// 验证StateDefId是否相同
		if (State->GetStateDefId() != ReferenceStateDefId)
		{
			UE_LOG(LogTcsStateMerger, Error, TEXT("[%s] StateDefId mismatch."), *FString(__FUNCTION__));
			return;
		}

		AActor* Instigator = State->GetOwner();
		if (!StatesByInstigator.Contains(Instigator))
		{
			StatesByInstigator.Add(Instigator, TArray<UTireflyStateInstance*>());
		}
		StatesByInstigator[Instigator].Add(State);
	}

	// 对每个Instigator的状态进行合并
	for (auto& InstigatorStates : StatesByInstigator)
	{
		TArray<UTireflyStateInstance*>& States = InstigatorStates.Value;
		
		// 按时间戳排序，最新的状态在最前面
		States.Sort([](
			const UTireflyStateInstance& A,
			const UTireflyStateInstance& B) {
			return A.GetApplyTimestamp() > B.GetApplyTimestamp();
		});

		// 计算该Instigator的总叠层数
		int32 TotalStackCount = 0;
		for (UTireflyStateInstance* State : States)
		{
			TotalStackCount += State->GetStackCount();
		}

		// 验证总叠层数是否超过最大限制
		const int32 MaxStackCount = ReferenceState->GetStateDef().MaxStackCount;
		if (TotalStackCount > MaxStackCount)
		{
			TotalStackCount = MaxStackCount;
		}

		// 使用最新的状态作为基础状态
		UTireflyStateInstance* BaseState = States[0];
		
		// 设置基础状态的叠层数
		BaseState->SetStackCount(TotalStackCount);
		MergedStates.Add(BaseState);

		// 把叠层合并后剩余的状态实例标记为待回到对象池中
		for (int32 i = 1; i < States.Num(); ++i)
		{
			// TODO: 把合并后剩余的状态实例标记为待回到对象池中
		}
	}
} 