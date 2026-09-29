// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerPayloadReader.h"

#include "TcsEffectLogChannel.h"



// ===== 注册表 =====

FTcsTriggerPayloadReaderRegistrar::FTcsTriggerPayloadReaderRegistrar(
	UScriptStruct* (*InPayloadStructGetter)(),
	FTcsTriggerPayloadRead InReader)
{
	FTcsTriggerPayloadReaderEntry Entry;
	Entry.GetPayloadStruct = InPayloadStructGetter;
	Entry.Reader = MoveTemp(InReader);

	FTcsTriggerPayloadReaderRegistry::Get().AddPending(MoveTemp(Entry));
}

FTcsTriggerPayloadReaderRegistry& FTcsTriggerPayloadReaderRegistry::Get()
{
	static FTcsTriggerPayloadReaderRegistry Registry;
	return Registry;
}

void FTcsTriggerPayloadReaderRegistry::AddPending(FTcsTriggerPayloadReaderEntry Entry)
{
	// 静态初始化期调用：只入待解析表，**不调用 getter**（那时 UObject 设施未就绪）
	PendingEntries.Add(MoveTemp(Entry));
}

void FTcsTriggerPayloadReaderRegistry::Register(
	const UScriptStruct* PayloadStruct,
	FTcsTriggerPayloadRead Reader,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	ensureMsgf(PayloadStruct != nullptr, TEXT("FTcsTriggerPayloadReaderRegistry::Register: 载荷类型为空——拒绝登记"));

	if (!PayloadStruct)
	{
		return;
	}

	// 拒绝门 = "同世界活对象重复"：既有条目**已失效**时可替换；**有效**（或静态自注册项）时拒绝保留首个
	if (Readers.Contains(PayloadStruct))
	{
		if (DiscardIfStale(PayloadStruct, LifetimeWorld))
		{
			UE_LOG(LogTcsEffect, Log,
				TEXT("FTcsTriggerPayloadReaderRegistry: 载荷类型 %s 的既有动态登记已失效（旧世界残留）——替换为新登记"),
				*PayloadStruct->GetName());
		}
		else
		{
			ensureMsgf(false, TEXT("FTcsTriggerPayloadReaderRegistry::Register: 载荷类型 %s 重复登记——保留首个登记"),
				*PayloadStruct->GetName());
			return;
		}
	}

	Readers.Add(PayloadStruct, MoveTemp(Reader));
	RecordLifetime(PayloadStruct, LifetimeObject, LifetimeWorld);
}

bool FTcsTriggerPayloadReaderRegistry::Unregister(const UScriptStruct* PayloadStruct)
{
	if (!PayloadStruct)
	{
		return false;
	}

	// 静态自注册项是**代码**而非登记——MUST NOT 可移除（本入口只服务动态登记）
	if (!Lifetimes.Contains(PayloadStruct))
	{
		return false;
	}

	Lifetimes.Remove(PayloadStruct);
	Readers.Remove(PayloadStruct);
	return true;
}

TArray<const UScriptStruct*> FTcsTriggerPayloadReaderRegistry::GetDynamicKeys() const
{
	TArray<const UScriptStruct*> Keys;
	Keys.Reserve(Lifetimes.Num());

	for (const TPair<const UScriptStruct*, FTcsTriggerPayloadReaderLifetime>& Pair : Lifetimes)
	{
		Keys.Add(Pair.Key);
	}

	return Keys;
}

const FTcsTriggerPayloadRead* FTcsTriggerPayloadReaderRegistry::Find(const UScriptStruct* PayloadStruct, const UWorld* World)
{
	if (!bPendingResolved)
	{
		ResolvePending();
	}

	if (!PayloadStruct)
	{
		return nullptr;
	}

	const FTcsTriggerPayloadRead* Found = Readers.Find(PayloadStruct);
	if (!Found)
	{
		return nullptr;
	}

	// 寿命校验（只对动态登记项生效；属主模块登记的领域读取器全走自注册 ⇒ 不在 Lifetimes 表 ⇒ 永不过期）
	const FTcsTriggerPayloadReaderLifetime* Lifetime = Lifetimes.Find(PayloadStruct);
	if (Lifetime)
	{
		if (!Lifetime->Object.IsValid())
		{
			// 对象已被 GC：该条登记自然失效（MUST NOT 解引用）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsTriggerPayloadReaderRegistry: 载荷类型 %s 的宿主读取器已被回收——该条登记视为失效"),
				*PayloadStruct->GetName());
			Lifetimes.Remove(PayloadStruct);
			Readers.Remove(PayloadStruct);
			return nullptr;
		}

		if (Lifetime->World.IsValid() && World && Lifetime->World.Get() != World)
		{
			// 跨世界：**可诊断**，且 MUST NOT 与"未注册载荷类型"的 Verbose 路径混淆
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsTriggerPayloadReaderRegistry: 载荷类型 %s 的动态登记属另一世界（登记世界 %s ≠ 查询世界 %s）——该条登记视为失效"),
				*PayloadStruct->GetName(),
				*Lifetime->World->GetName(),
				*World->GetName());
			Lifetimes.Remove(PayloadStruct);
			Readers.Remove(PayloadStruct);
			return nullptr;
		}
	}

	return Found;
}

void FTcsTriggerPayloadReaderRegistry::RecordLifetime(
	const UScriptStruct* PayloadStruct,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	if (!LifetimeObject && !LifetimeWorld)
	{
		// 纯 C++ 路径（无 UObject 寿命约束）——不建条目，该登记永不过期
		Lifetimes.Remove(PayloadStruct);
		return;
	}

	FTcsTriggerPayloadReaderLifetime& Lifetime = Lifetimes.FindOrAdd(PayloadStruct);
	Lifetime.Object = LifetimeObject;
	Lifetime.World = LifetimeWorld;
}

bool FTcsTriggerPayloadReaderRegistry::DiscardIfStale(const UScriptStruct* PayloadStruct, const UWorld* World)
{
	// 静态自注册项（不在 Lifetimes 表）不是"失效"——它是代码，MUST NOT 被替换
	const FTcsTriggerPayloadReaderLifetime* Lifetime = Lifetimes.Find(PayloadStruct);
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

	Lifetimes.Remove(PayloadStruct);
	Readers.Remove(PayloadStruct);
	return true;
}

void FTcsTriggerPayloadReaderRegistry::ResolvePending()
{
	// 幂等：只跑一次（此后 Register 直接进 Readers）
	bPendingResolved = true;

	for (FTcsTriggerPayloadReaderEntry& Entry : PendingEntries)
	{
		// 此刻才调用 getter（静态初始化期已过，UObject 设施就绪）
		UScriptStruct* PayloadStruct = Entry.GetPayloadStruct ? Entry.GetPayloadStruct() : nullptr;
		if (!PayloadStruct)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("FTcsTriggerPayloadReaderRegistry: 待解析项的类型 getter 返回空——跳过"));
			continue;
		}

		if (Readers.Contains(PayloadStruct))
		{
			ensureMsgf(false, TEXT("FTcsTriggerPayloadReaderRegistry: 载荷类型 %s 重复登记（静态自注册）——保留首个"),
				*PayloadStruct->GetName());
			continue;
		}

		Readers.Add(PayloadStruct, MoveTemp(Entry.Reader));
	}

	PendingEntries.Reset();
}
