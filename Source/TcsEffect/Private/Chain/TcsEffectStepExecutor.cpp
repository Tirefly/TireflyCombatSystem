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

void FTcsEffectStepExecutorRegistry::Register(
	const UScriptStruct* StepStruct,
	FTcsStepExecute Executor,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(StepStruct, TEXT("FTcsEffectStepExecutorRegistry::Register: 步骤反射类型为空——拒绝登记")))
	{
		return;
	}

	ResolvePending();

	// 拒绝门 = "同世界活对象重复"：既有条目**已失效**时可替换；**有效**（或静态自注册项）时拒绝保留首个
	if (Executors.Contains(StepStruct))
	{
		if (DiscardIfStale(StepStruct, LifetimeWorld))
		{
			UE_LOG(LogTcsEffect, Log,
				TEXT("FTcsEffectStepExecutorRegistry: 步骤类型 %s 的既有动态登记已失效（旧世界残留）——替换为新登记"),
				*StepStruct->GetName());
		}
		else
		{
			ensureMsgf(false, TEXT("FTcsEffectStepExecutorRegistry::Register: 步骤类型 %s 已有执行器——拒绝重复登记（保留首个）"),
				*StepStruct->GetName());
			return;
		}
	}

	Executors.Add(StepStruct, MoveTemp(Executor));
	RecordLifetime(StepStruct, LifetimeObject, LifetimeWorld);
}

bool FTcsEffectStepExecutorRegistry::Unregister(const UScriptStruct* StepStruct)
{
	if (!StepStruct)
	{
		return false;
	}

	ResolvePending();

	// 静态自注册项是**代码**而非登记——MUST NOT 可移除（本入口只服务动态登记）
	if (!Lifetimes.Contains(StepStruct))
	{
		return false;
	}

	Lifetimes.Remove(StepStruct);
	Executors.Remove(StepStruct);
	return true;
}

TArray<const UScriptStruct*> FTcsEffectStepExecutorRegistry::GetDynamicKeys() const
{
	TArray<const UScriptStruct*> Keys;
	Keys.Reserve(Lifetimes.Num());

	for (const TPair<const UScriptStruct*, FTcsEffectStepExecutorLifetime>& Pair : Lifetimes)
	{
		Keys.Add(Pair.Key);
	}

	return Keys;
}

const FTcsStepExecute* FTcsEffectStepExecutorRegistry::Find(const UScriptStruct* StepStruct, const UWorld* World)
{
	if (!StepStruct)
	{
		return nullptr;
	}

	ResolvePending();

	const FTcsStepExecute* Found = Executors.Find(StepStruct);
	if (!Found)
	{
		return nullptr;
	}

	// 寿命校验（只对动态登记项生效；静态自注册项不在 Lifetimes 表里，故永不过期）
	const FTcsEffectStepExecutorLifetime* Lifetime = Lifetimes.Find(StepStruct);
	if (Lifetime)
	{
		if (!Lifetime->Object.IsValid())
		{
			// 对象已被 GC：该条登记自然失效（MUST NOT 解引用——这正是本批修的核心缺陷）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsEffectStepExecutorRegistry: 步骤类型 %s 的宿主执行器已被回收——该条登记视为失效"),
				*StepStruct->GetName());
			Lifetimes.Remove(StepStruct);
			Executors.Remove(StepStruct);
			return nullptr;
		}

		if (Lifetime->World.IsValid() && World && Lifetime->World.Get() != World)
		{
			// 跨世界：**可诊断**（MUST NOT 静默按"未登记"处理——那会把"世界已更换"表现成"步骤类型写漏"）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsEffectStepExecutorRegistry: 步骤类型 %s 的动态登记属另一世界（登记世界 %s ≠ 查询世界 %s）——该条登记视为失效"),
				*StepStruct->GetName(),
				*Lifetime->World->GetName(),
				*World->GetName());
			Lifetimes.Remove(StepStruct);
			Executors.Remove(StepStruct);
			return nullptr;
		}
	}

	return Found;
}

void FTcsEffectStepExecutorRegistry::RecordLifetime(
	const UScriptStruct* StepStruct,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	if (!LifetimeObject && !LifetimeWorld)
	{
		// 纯 C++ 路径（无 UObject 寿命约束）——不建条目，该登记永不过期
		Lifetimes.Remove(StepStruct);
		return;
	}

	FTcsEffectStepExecutorLifetime& Lifetime = Lifetimes.FindOrAdd(StepStruct);
	Lifetime.Object = LifetimeObject;
	Lifetime.World = LifetimeWorld;
}

bool FTcsEffectStepExecutorRegistry::DiscardIfStale(const UScriptStruct* StepStruct, const UWorld* World)
{
	// 静态自注册项（不在 Lifetimes 表）不是"失效"——它是代码，MUST NOT 被替换
	const FTcsEffectStepExecutorLifetime* Lifetime = Lifetimes.Find(StepStruct);
	if (!Lifetime)
	{
		return false;
	}

	const bool bObjectCollected = !Lifetime->Object.IsValid();
	const bool bWorldGone = Lifetime->World.IsValid() == false;
	const bool bWorldMismatch = Lifetime->World.IsValid() && World && Lifetime->World.Get() != World;

	if (!bObjectCollected && !bWorldGone && !bWorldMismatch)
	{
		return false;
	}

	Lifetimes.Remove(StepStruct);
	Executors.Remove(StepStruct);
	return true;
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
