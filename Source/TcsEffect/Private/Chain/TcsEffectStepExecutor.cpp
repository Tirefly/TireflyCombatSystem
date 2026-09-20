// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsEffectStepExecutor.h"

#include "TcsEffectLogChannel.h"



FTcsEffectStepExecutorRegistrar::FTcsEffectStepExecutorRegistrar(
	UScriptStruct* (*InStepStructGetter)(),
	FTcsStepExecute InExecutor)
{
	// 静态初始化期只入待解析表——**不调用 getter**（该时点不假定 UObject 设施就绪）
	FTcsEffectStepExecutorRegistry::Get().AddPending(FTcsEffectStepExecutorEntry{ InStepStructGetter, MoveTemp(InExecutor) });
}

FTcsEffectStepExecutorRegistry& FTcsEffectStepExecutorRegistry::Get()
{
	// 函数局部静态：首次任一路径触达时建立（静态初始化期由注册器构造触达，无需显式启动代码）
	static FTcsEffectStepExecutorRegistry Instance;
	return Instance;
}

void FTcsEffectStepExecutorRegistry::AddPending(FTcsEffectStepExecutorEntry Entry)
{
	PendingEntries.Add(MoveTemp(Entry));
}

void FTcsEffectStepExecutorRegistry::Register(const UScriptStruct* StepStruct, FTcsStepExecute Executor)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(StepStruct, TEXT("FTcsEffectStepExecutorRegistry::Register: 步骤反射类型为空——拒绝登记")))
	{
		return;
	}

	ResolvePending();

	if (Executors.Contains(StepStruct))
	{
		ensureMsgf(false, TEXT("FTcsEffectStepExecutorRegistry::Register: 步骤类型 %s 已有执行器——拒绝重复登记（保留首个）"),
			*StepStruct->GetName());
		return;
	}

	Executors.Add(StepStruct, MoveTemp(Executor));
}

const FTcsStepExecute* FTcsEffectStepExecutorRegistry::Find(const UScriptStruct* StepStruct)
{
	if (!StepStruct)
	{
		return nullptr;
	}

	ResolvePending();
	return Executors.Find(StepStruct);
}

void FTcsEffectStepExecutorRegistry::ResolvePending()
{
	if (bPendingResolved)
	{
		return;
	}
	bPendingResolved = true;

	for (FTcsEffectStepExecutorEntry& Entry : PendingEntries)
	{
		// 反射类型在此刻才解析（首次查询已晚于引擎启动——UObject 设施就绪）
		UScriptStruct* StepStruct = Entry.GetStepStruct ? Entry.GetStepStruct() : nullptr;
		if (!StepStruct)
		{
			UE_LOG(LogTcsEffect, Error, TEXT("FTcsEffectStepExecutorRegistry: 待解析登记项的类型 getter 返回空——该项丢弃"));
			continue;
		}

		if (Executors.Contains(StepStruct))
		{
			ensureMsgf(false, TEXT("FTcsEffectStepExecutorRegistry: 步骤类型 %s 被重复登记（跨模块/跨文件）——保留首次登记"),
				*StepStruct->GetName());
			continue;
		}

		Executors.Add(StepStruct, MoveTemp(Entry.Executor));
	}

	PendingEntries.Reset();

	UE_LOG(LogTcsEffect, Log, TEXT("FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析（共 %d 类）"), Executors.Num());
}
