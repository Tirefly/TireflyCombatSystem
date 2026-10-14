// Copyright Tirefly. All Rights Reserved.

#include "Asset/TcsPrimaryAssetRegistry.h"

#include "TcsAssetButlerLogChannel.h"



FTcsPrimaryAssetRegistrar::FTcsPrimaryAssetRegistrar(FTcsPrimaryAssetEntry InEntry)
{
	// 静态初始化期只入待解析表——**不调用 getter**（该时点不假定 UObject 设施就绪）
	FTcsPrimaryAssetRegistry::Get().Register(MoveTemp(InEntry));
}



FTcsPrimaryAssetRegistry& FTcsPrimaryAssetRegistry::Get()
{
	// 函数局部静态：首次任一路径触达时建立（静态初始化期由注册器构造触达，无需显式启动代码）
	static FTcsPrimaryAssetRegistry Instance;
	return Instance;
}



void FTcsPrimaryAssetRegistry::Register(FTcsPrimaryAssetEntry Entry)
{
	// 待解析期：入队（解析与判重统一在 ResolvePending 里做，不由调用顺序决定结论）
	if (!bPendingResolved)
	{
		PendingEntries.Add(MoveTemp(Entry));
		return;
	}

	// 已解析期（运行期补登）：即时解析 + 即时判重——**同一套判据**（AddResolved）
	ResolveEntry(Entry);
	if (!Entry.bResolved)
	{
		return;
	}

	AddResolved(MoveTemp(Entry));
}



bool FTcsPrimaryAssetRegistry::Unregister(const FPrimaryAssetType& Type)
{
	if (!bPendingResolved)
	{
		// 待解析期：先解析再按类型摘（此时调用方拿不到由反射派生的类型名之外的判据）
		bool bRemoved = false;
		for (int32 Index = PendingEntries.Num() - 1; Index >= 0; --Index)
		{
			FTcsPrimaryAssetEntry& Entry = PendingEntries[Index];
			ResolveEntry(Entry);
			if (Entry.bResolved && Entry.Type == Type)
			{
				PendingEntries.RemoveAt(Index);
				bRemoved = true;
			}
		}
		return bRemoved;
	}

	return Entries.Remove(Type) > 0;
}



void FTcsPrimaryAssetRegistry::ForEach(TFunctionRef<void(const FTcsPrimaryAssetEntry&)> Visitor)
{
	ResolvePending();

	for (const TPair<FPrimaryAssetType, FTcsPrimaryAssetEntry>& Pair : Entries)
	{
		Visitor(Pair.Value);
	}
}



int32 FTcsPrimaryAssetRegistry::Num()
{
	ResolvePending();

	return Entries.Num();
}



const FTcsPrimaryAssetEntry* FTcsPrimaryAssetRegistry::Find(const FPrimaryAssetType& Type)
{
	ResolvePending();

	return Entries.Find(Type);
}



void FTcsPrimaryAssetRegistry::ResolveEntry(FTcsPrimaryAssetEntry& Entry)
{
	Entry.BaseClass = Entry.GetBaseClass ? Entry.GetBaseClass() : nullptr;

	if (!Entry.BaseClass)
	{
		UE_LOG(LogTcsAssetButler, Error, TEXT("FTcsPrimaryAssetRegistry: 注册项的基类 getter 返回空——该项丢弃"));
		return;
	}

	// 类型名由反射名派生（**零字符串面**——类改名不需要同步改任何注册处或 ini）
	Entry.Type = FPrimaryAssetType(Entry.BaseClass->GetFName());
	Entry.bResolved = true;
}



void FTcsPrimaryAssetRegistry::AddResolved(FTcsPrimaryAssetEntry Entry)
{
	// 唯一性判据 = 引擎语义（**不自建第二套**）：同类型只允许一条，重复即 Error 并保留首个
	if (Entries.Contains(Entry.Type))
	{
		UE_LOG(LogTcsAssetButler, Error,
			TEXT("FTcsPrimaryAssetRegistry: 主资产类型 %s 出现多条注册项——Only a single entry per type is supported."),
			*Entry.Type.ToString());
		ensureMsgf(false, TEXT("FTcsPrimaryAssetRegistry: 主资产类型 %s 重复注册——拒绝（保留首个）"), *Entry.Type.ToString());
		return;
	}

	Entries.Add(Entry.Type, MoveTemp(Entry));
}



void FTcsPrimaryAssetRegistry::ResolvePending()
{
	if (bPendingResolved)
	{
		return;
	}
	bPendingResolved = true;

	for (FTcsPrimaryAssetEntry& Pending : PendingEntries)
	{
		ResolveEntry(Pending);

		if (!Pending.bResolved)
		{
			continue;
		}

		AddResolved(MoveTemp(Pending));
	}

	PendingEntries.Empty();

	UE_LOG(LogTcsAssetButler, Log, TEXT("FTcsPrimaryAssetRegistry: 注册表就绪——已登记主资产类型 %d 个"), Entries.Num());
}
