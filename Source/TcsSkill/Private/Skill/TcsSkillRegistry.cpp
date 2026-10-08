// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillRegistry.h"



// 分配
FSkillBucket& FTcsSkillRegistry::FindOrAddBucket(FTcsCombatEntityHandle Unit)
{
	if (const TUniquePtr<FSkillBucket>* Existing = Buckets.Find(Unit))
	{
		return **Existing;
	}

	return *Buckets.Add(Unit, MakeUnique<FSkillBucket>());
}



// 访问
FTcsLearnedSkillEntry* FTcsSkillRegistry::Find(FTcsCombatEntityHandle Unit, const FTcsSkillEntryHandle& Handle)
{
	FSkillBucket* Bucket = FindBucket(Unit);
	return Bucket ? Bucket->Find(Handle) : nullptr;
}

const FTcsLearnedSkillEntry* FTcsSkillRegistry::Find(FTcsCombatEntityHandle Unit, const FTcsSkillEntryHandle& Handle) const
{
	const FSkillBucket* Bucket = FindBucket(Unit);
	return Bucket ? Bucket->Find(Handle) : nullptr;
}

FSkillBucket* FTcsSkillRegistry::FindBucket(FTcsCombatEntityHandle Unit)
{
	const TUniquePtr<FSkillBucket>* Found = Buckets.Find(Unit);
	return Found ? Found->Get() : nullptr;
}

const FSkillBucket* FTcsSkillRegistry::FindBucket(FTcsCombatEntityHandle Unit) const
{
	const TUniquePtr<FSkillBucket>* Found = Buckets.Find(Unit);
	return Found ? Found->Get() : nullptr;
}

bool FTcsSkillRegistry::ContainsUnit(FTcsCombatEntityHandle Unit) const
{
	return Buckets.Contains(Unit);
}

int32 FTcsSkillRegistry::NumBuckets() const
{
	return Buckets.Num();
}

int32 FTcsSkillRegistry::NumEntries() const
{
	int32 Count = 0;
	for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FSkillBucket>>& Pair : Buckets)
	{
		if (Pair.Value.IsValid())
		{
			Count += Pair.Value->Num();
		}
	}

	return Count;
}

void FTcsSkillRegistry::ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, const FSkillBucket&)> Visitor) const
{
	for (const TPair<FTcsCombatEntityHandle, TUniquePtr<FSkillBucket>>& Pair : Buckets)
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

void FTcsSkillRegistry::ForEachBucket(TFunctionRef<bool(FTcsCombatEntityHandle, FSkillBucket&)> Visitor)
{
	// 可写重载：与只读重载同序（`TMap` 序，不定序），只多给桶的写权限
	for (TPair<FTcsCombatEntityHandle, TUniquePtr<FSkillBucket>>& Pair : Buckets)
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
bool FTcsSkillRegistry::RemoveBucket(FTcsCombatEntityHandle Unit)
{
	// 桶随 TUniquePtr 析构而消失——先 Reset 使代际递增（旧句柄凭代际失配判悬空，
	// 比"查不到桶"更精确：将来若桶按单位句柄复用，失配判据依然成立）
	if (TUniquePtr<FSkillBucket> Removed; Buckets.RemoveAndCopyValue(Unit, Removed))
	{
		if (Removed.IsValid())
		{
			Removed->Reset();
		}

		return true;
	}

	return false;
}

void FTcsSkillRegistry::Reset()
{
	for (TPair<FTcsCombatEntityHandle, TUniquePtr<FSkillBucket>>& Pair : Buckets)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->Reset();
		}
	}

	Buckets.Reset();
}
