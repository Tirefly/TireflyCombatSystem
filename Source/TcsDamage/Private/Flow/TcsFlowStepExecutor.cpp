// Copyright Tirefly. All Rights Reserved.

#include "Flow/TcsFlowStepExecutor.h"

#include "TcsDamageLogChannel.h"



FTcsFlowStepExecutorRegistrar::FTcsFlowStepExecutorRegistrar(
	UScriptStruct* (*InStepStructGetter)(),
	FTcsFlowStepExecute InExecutor)
{
	// 静态初始化期只入待解析表——不调用 getter（该时点不假定 UObject 设施就绪）
	FTcsFlowStepExecutorRegistry::Get().AddPending(FTcsFlowStepExecutorEntry{ InStepStructGetter, MoveTemp(InExecutor) });
}

FTcsFlowStepExecutorRegistry& FTcsFlowStepExecutorRegistry::Get()
{
	static FTcsFlowStepExecutorRegistry Instance;
	return Instance;
}

void FTcsFlowStepExecutorRegistry::AddPending(FTcsFlowStepExecutorEntry Entry)
{
	PendingEntries.Add(MoveTemp(Entry));
}

void FTcsFlowStepExecutorRegistry::Register(const UScriptStruct* StepStruct, FTcsFlowStepExecute Executor)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(StepStruct, TEXT("FTcsFlowStepExecutorRegistry::Register: 步骤反射类型为空——拒绝登记")))
	{
		return;
	}

	ResolvePending();

	if (Executors.Contains(StepStruct))
	{
		ensureMsgf(false, TEXT("FTcsFlowStepExecutorRegistry::Register: 步骤类型 %s 已有执行器——拒绝重复登记（保留首个）"),
			*StepStruct->GetName());
		return;
	}

	Executors.Add(StepStruct, MoveTemp(Executor));
}

const FTcsFlowStepExecute* FTcsFlowStepExecutorRegistry::Find(const UScriptStruct* StepStruct)
{
	if (!StepStruct)
	{
		return nullptr;
	}

	ResolvePending();
	return Executors.Find(StepStruct);
}

void FTcsFlowStepExecutorRegistry::ResolvePending()
{
	if (bPendingResolved)
	{
		return;
	}
	bPendingResolved = true;

	for (FTcsFlowStepExecutorEntry& Entry : PendingEntries)
	{
		// 反射类型在此刻才解析（首次查询已晚于引擎启动——UObject 设施就绪）
		UScriptStruct* StepStruct = Entry.GetStepStruct ? Entry.GetStepStruct() : nullptr;
		if (!StepStruct)
		{
			UE_LOG(LogTcsDamage, Error, TEXT("FTcsFlowStepExecutorRegistry: 待解析登记项的类型 getter 返回空——该项丢弃"));
			continue;
		}

		if (Executors.Contains(StepStruct))
		{
			ensureMsgf(false, TEXT("FTcsFlowStepExecutorRegistry: 步骤类型 %s 被重复登记——保留首次登记"), *StepStruct->GetName());
			continue;
		}

		Executors.Add(StepStruct, MoveTemp(Entry.Executor));
	}

	PendingEntries.Reset();

	UE_LOG(LogTcsDamage, Log, TEXT("FTcsFlowStepExecutorRegistry: 流程步骤执行器登记表已解析（共 %d 类）"), Executors.Num());
}
