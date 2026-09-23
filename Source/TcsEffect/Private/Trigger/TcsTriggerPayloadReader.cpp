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

void FTcsTriggerPayloadReaderRegistry::Register(const UScriptStruct* PayloadStruct, FTcsTriggerPayloadRead Reader)
{
	ensureMsgf(PayloadStruct != nullptr, TEXT("FTcsTriggerPayloadReaderRegistry::Register: 载荷类型为空——拒绝登记"));

	if (!PayloadStruct)
	{
		return;
	}

	if (Readers.Contains(PayloadStruct))
	{
		ensureMsgf(false, TEXT("FTcsTriggerPayloadReaderRegistry::Register: 载荷类型 %s 重复登记——保留首个登记"),
			*PayloadStruct->GetName());
		return;
	}

	Readers.Add(PayloadStruct, MoveTemp(Reader));
}

const FTcsTriggerPayloadRead* FTcsTriggerPayloadReaderRegistry::Find(const UScriptStruct* PayloadStruct)
{
	if (!bPendingResolved)
	{
		ResolvePending();
	}

	return PayloadStruct ? Readers.Find(PayloadStruct) : nullptr;
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
