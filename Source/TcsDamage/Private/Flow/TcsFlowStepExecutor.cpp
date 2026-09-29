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

void FTcsFlowStepExecutorRegistry::Register(
	const UScriptStruct* StepStruct,
	FTcsFlowStepExecute Executor,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	ensure(IsInGameThread());

	if (!ensureMsgf(StepStruct, TEXT("FTcsFlowStepExecutorRegistry::Register: 步骤反射类型为空——拒绝登记")))
	{
		return;
	}

	ResolvePending();

	// 拒绝门 = "同世界活对象重复"：既有条目**已失效**时可替换；**有效**（或静态自注册项）时拒绝保留首个
	if (Executors.Contains(StepStruct))
	{
		if (DiscardIfStale(StepStruct, LifetimeWorld))
		{
			UE_LOG(LogTcsDamage, Log,
				TEXT("FTcsFlowStepExecutorRegistry: 步骤类型 %s 的既有动态登记已失效（旧世界残留）——替换为新登记"),
				*StepStruct->GetName());
		}
		else
		{
			ensureMsgf(false, TEXT("FTcsFlowStepExecutorRegistry::Register: 步骤类型 %s 已有执行器——拒绝重复登记（保留首个）"),
				*StepStruct->GetName());
			return;
		}
	}

	Executors.Add(StepStruct, MoveTemp(Executor));
	RecordLifetime(StepStruct, LifetimeObject, LifetimeWorld);
}

bool FTcsFlowStepExecutorRegistry::Unregister(const UScriptStruct* StepStruct)
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

TArray<const UScriptStruct*> FTcsFlowStepExecutorRegistry::GetDynamicKeys() const
{
	TArray<const UScriptStruct*> Keys;
	Keys.Reserve(Lifetimes.Num());

	for (const TPair<const UScriptStruct*, FTcsFlowStepExecutorLifetime>& Pair : Lifetimes)
	{
		Keys.Add(Pair.Key);
	}

	return Keys;
}

const FTcsFlowStepExecute* FTcsFlowStepExecutorRegistry::Find(const UScriptStruct* StepStruct, const UWorld* World)
{
	if (!StepStruct)
	{
		return nullptr;
	}

	ResolvePending();

	const FTcsFlowStepExecute* Found = Executors.Find(StepStruct);
	if (!Found)
	{
		return nullptr;
	}

	// 寿命校验（只对动态登记项生效；静态自注册项不在 Lifetimes 表里，故永不过期）
	const FTcsFlowStepExecutorLifetime* Lifetime = Lifetimes.Find(StepStruct);
	if (Lifetime)
	{
		if (!Lifetime->Object.IsValid())
		{
			// 对象已被 GC：该条登记自然失效（MUST NOT 解引用——这正是本批修的核心缺陷）
			UE_LOG(LogTcsDamage, Warning,
				TEXT("FTcsFlowStepExecutorRegistry: 步骤类型 %s 的宿主执行器已被回收——该条登记视为失效"),
				*StepStruct->GetName());
			Lifetimes.Remove(StepStruct);
			Executors.Remove(StepStruct);
			return nullptr;
		}

		if (Lifetime->World.IsValid() && World && Lifetime->World.Get() != World)
		{
			// 跨世界：**可诊断**（MUST NOT 静默按"未登记"处理——那会把"世界已更换"表现成"步骤类型写漏"）
			UE_LOG(LogTcsDamage, Warning,
				TEXT("FTcsFlowStepExecutorRegistry: 步骤类型 %s 的动态登记属另一世界（登记世界 %s ≠ 查询世界 %s）——该条登记视为失效"),
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

void FTcsFlowStepExecutorRegistry::RecordLifetime(
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

	FTcsFlowStepExecutorLifetime& Lifetime = Lifetimes.FindOrAdd(StepStruct);
	Lifetime.Object = LifetimeObject;
	Lifetime.World = LifetimeWorld;
}

bool FTcsFlowStepExecutorRegistry::DiscardIfStale(const UScriptStruct* StepStruct, const UWorld* World)
{
	// 静态自注册项（不在 Lifetimes 表）不是"失效"——它是代码，MUST NOT 被替换
	const FTcsFlowStepExecutorLifetime* Lifetime = Lifetimes.Find(StepStruct);
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
