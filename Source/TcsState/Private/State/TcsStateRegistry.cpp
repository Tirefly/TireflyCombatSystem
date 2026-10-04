// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateRegistry.h"



// 槽位
FTcsStateHandle FStateBucket::AllocateSlot()
{
	uint32 SlotIndex;
	if (FreeSlots.Num() > 0)
	{
		SlotIndex = FreeSlots.Pop(EAllowShrinking::No);
		SlotGenerations[SlotIndex] += 1;
	}
	else
	{
		SlotIndex = static_cast<uint32>(Instances.Num());
		Instances.Emplace();
		SlotGenerations.Add(1);
	}

	return MakeHandle(SlotIndex);
}

void FStateBucket::ReleaseSlot(uint32 SlotIndex)
{
	if (!SlotGenerations.IsValidIndex(static_cast<int32>(SlotIndex)))
	{
		return;
	}

	// 代际 +1 使旧句柄悬空（与 TTcsInstancePool::Free 同款语义）；槽位内容不跨生命周期残留
	SlotGenerations[SlotIndex] += 1;
	Instances[SlotIndex] = FTcsStateInstance();
	FreeSlots.Push(SlotIndex);
}



// 访问
FTcsStateInstance* FStateBucket::Find(const FTcsStateHandle& Handle)
{
	return const_cast<FTcsStateInstance*>(static_cast<const FStateBucket*>(this)->Find(Handle));
}

const FTcsStateInstance* FStateBucket::Find(const FTcsStateHandle& Handle) const
{
	if (!Handle.IsValid() || !SlotGenerations.IsValidIndex(Handle.Index))
	{
		return nullptr;
	}

	// 代际校验：失配 = 句柄指向已被回收（或复用）的槽位
	if (static_cast<int32>(SlotGenerations[Handle.Index]) != Handle.Generation)
	{
		return nullptr;
	}

	// 奇偶簿记：偶数 = 空闲（与 TTcsInstancePool 同款判据）
	if ((SlotGenerations[Handle.Index] & 1u) == 0u)
	{
		return nullptr;
	}

	return &Instances[Handle.Index];
}

bool FStateBucket::IsValidHandle(const FTcsStateHandle& Handle) const
{
	return Find(Handle) != nullptr;
}

void FStateBucket::ForEach(TFunctionRef<bool(const FTcsStateInstance&)> Visitor) const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 0u)
		{
			continue;
		}

		if (!Visitor(Instances[SlotIndex]))
		{
			return;
		}
	}
}

int32 FStateBucket::Num() const
{
	int32 Count = 0;
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 1u)
		{
			++Count;
		}
	}

	return Count;
}



// 生命周期
void FStateBucket::Reset()
{
	// 逐槽归还（代际 +1）使全部旧句柄失效——与逐个 ReleaseSlot 等价，但不建空闲栈（桶即将被弃）
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 1u)
		{
			SlotGenerations[SlotIndex] += 1;
		}
	}

	Instances.Reset();
	SlotGenerations.Reset();
	FreeSlots.Reset();
}



// 内核
FTcsStateHandle FStateBucket::MakeHandle(uint32 SlotIndex) const
{
	FTcsStateHandle Handle;
	Handle.Index = static_cast<int32>(SlotIndex);
	Handle.Generation = static_cast<int32>(SlotGenerations[SlotIndex]);
	return Handle;
}



// 分配
FStateBucket& FTcsStateRegistry::FindOrAddBucket(FTcsCombatEntityHandle Unit)
{
	if (const TUniquePtr<FStateBucket>* Existing = Buckets.Find(Unit))
	{
		return **Existing;
	}

	return *Buckets.Add(Unit, MakeUnique<FStateBucket>());
}



// 访问
FTcsStateInstance* FTcsStateRegistry::Find(FTcsCombatEntityHandle Unit, const FTcsStateHandle& Handle)
{
	FStateBucket* Bucket = FindBucket(Unit);
	return Bucket ? Bucket->Find(Handle) : nullptr;
}

const FTcsStateInstance* FTcsStateRegistry::Find(FTcsCombatEntityHandle Unit, const FTcsStateHandle& Handle) const
{
	const FStateBucket* Bucket = FindBucket(Unit);
	return Bucket ? Bucket->Find(Handle) : nullptr;
}

FStateBucket* FTcsStateRegistry::FindBucket(FTcsCombatEntityHandle Unit)
{
	const TUniquePtr<FStateBucket>* Found = Buckets.Find(Unit);
	return Found ? Found->Get() : nullptr;
}

const FStateBucket* FTcsStateRegistry::FindBucket(FTcsCombatEntityHandle Unit) const
{
	const TUniquePtr<FStateBucket>* Found = Buckets.Find(Unit);
	return Found ? Found->Get() : nullptr;
}

bool FTcsStateRegistry::ContainsUnit(FTcsCombatEntityHandle Unit) const
{
	return Buckets.Contains(Unit);
}

int32 FTcsStateRegistry::NumBuckets() const
{
	return Buckets.Num();
}

int32 FTcsStateRegistry::NumInstances() const
{
	int32 Count = 0;
	for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>& Pair : Buckets)
	{
		if (Pair.Value.IsValid())
		{
			Count += Pair.Value->Num();
		}
	}

	return Count;
}

void FTcsStateRegistry::ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, const FStateBucket&)> Visitor) const
{
	for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>& Pair : Buckets)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		if (!Visitor(Pair.Key, *Pair.Value))
		{
			return;
		}
	}
}



// 生命周期
bool FTcsStateRegistry::RemoveBucket(FTcsCombatEntityHandle Unit)
{
	// 桶随 TUniquePtr 析构而消失——先 Reset 使代际递增（旧句柄凭代际失配判悬空，
	// 比"查不到桶"更精确：将来若桶按单位句柄复用，失配判据依然成立）
	if (TUniquePtr<FStateBucket> Removed; Buckets.RemoveAndCopyValue(Unit, Removed))
	{
		if (Removed.IsValid())
		{
			Removed->Reset();
		}

		return true;
	}

	return false;
}

void FTcsStateRegistry::Reset()
{
	for (TPair<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>& Pair : Buckets)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Reset();
		}
	}

	Buckets.Reset();
}
