// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillRegistry.h"

#include "TcsSkillLogChannel.h"



namespace
{
	// 出线到 TcsSkill DLL：跨桶、跨世界共用，世界拆解不复位。奇数为在册，偶数为释放态。
	uint32 GTcsSkillRegistryNextGeneration = 1;

	uint32 AllocateSkillRegistryGeneration()
	{
		check(IsInGameThread());
		if (GTcsSkillRegistryNextGeneration > static_cast<uint32>(MAX_int32))
		{
			// 反射句柄的 int32 上限不能回绕：即便 Shipping 也必须拒绝身份复用。
			UE_LOG(LogTcsSkill, Fatal, TEXT("技能账本条目代际空间已耗尽，不能复用旧身份"));
		}

		const uint32 Generation = GTcsSkillRegistryNextGeneration;
		GTcsSkillRegistryNextGeneration += 2;
		return Generation;
	}
}



// 槽位
FTcsSkillEntryHandle FSkillBucket::AllocateSlot()
{
	uint32 SlotIndex;
	if (FreeSlots.Num() > 0)
	{
		SlotIndex = FreeSlots.Pop(EAllowShrinking::No);
		SlotGenerations[SlotIndex] = AllocateSkillRegistryGeneration();
	}
	else
	{
		SlotIndex = static_cast<uint32>(Entries.Num());
		Entries.Emplace();
		SlotGenerations.Add(AllocateSkillRegistryGeneration());
	}

	return MakeHandle(SlotIndex);
}

void FSkillBucket::ReleaseSlot(uint32 SlotIndex)
{
	if (!SlotGenerations.IsValidIndex(static_cast<int32>(SlotIndex)))
	{
		return;
	}

	// 代际 +1 使旧句柄悬空（与 TTcsInstancePool::Free 同款语义）；槽位内容不跨生命周期残留
	SlotGenerations[SlotIndex] += 1;
	Entries[SlotIndex] = FTcsLearnedSkillEntry();
	FreeSlots.Push(SlotIndex);
}



// 访问
FTcsLearnedSkillEntry* FSkillBucket::Find(const FTcsSkillEntryHandle& Handle)
{
	return const_cast<FTcsLearnedSkillEntry*>(static_cast<const FSkillBucket*>(this)->Find(Handle));
}

const FTcsLearnedSkillEntry* FSkillBucket::Find(const FTcsSkillEntryHandle& Handle) const
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

	return &Entries[Handle.Index];
}

bool FSkillBucket::IsValidHandle(const FTcsSkillEntryHandle& Handle) const
{
	return Find(Handle) != nullptr;
}

FTcsLearnedSkillEntry* FSkillBucket::FindByDefTag(FGameplayTag DefTag)
{
	return const_cast<FTcsLearnedSkillEntry*>(static_cast<const FSkillBucket*>(this)->FindByDefTag(DefTag));
}

const FTcsLearnedSkillEntry* FSkillBucket::FindByDefTag(FGameplayTag DefTag) const
{
	// "同一单位会同一个技能"至多一条（授予路径按 `DefTag` 刷新而非新建）⇒ 先命中即唯一
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 0u)
		{
			continue;
		}

		if (Entries[SlotIndex].DefTag == DefTag)
		{
			return &Entries[SlotIndex];
		}
	}

	return nullptr;
}

void FSkillBucket::ForEach(TFunctionRef<bool(const FTcsLearnedSkillEntry&)> Visitor) const
{
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 0u)
		{
			continue;
		}

		if (!Visitor(Entries[SlotIndex]))
		{
			return;
		}
	}
}

void FSkillBucket::ForEach(TFunctionRef<bool(FTcsLearnedSkillEntry&)> Visitor)
{
	// 可写重载：可见范围与只读重载完全一致（只多给了字段写权限）——"就地改字段"是允许的，
	// 增删条目仍 MUST NOT（那会改动槽位与空闲栈）
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 0u)
		{
			continue;
		}

		if (!Visitor(Entries[SlotIndex]))
		{
			return;
		}
	}
}

int32 FSkillBucket::Num() const
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
void FSkillBucket::Reset()
{
	// 逐槽归还（代际 +1）使全部旧句柄失效——与逐个 ReleaseSlot 等价，但不建空闲栈（桶即将被弃）
	for (int32 SlotIndex = 0; SlotIndex < SlotGenerations.Num(); ++SlotIndex)
	{
		if ((SlotGenerations[SlotIndex] & 1u) == 1u)
		{
			SlotGenerations[SlotIndex] += 1;
		}
	}

	Entries.Reset();
	SlotGenerations.Reset();
	FreeSlots.Reset();
}



// 内核
FTcsSkillEntryHandle FSkillBucket::MakeHandle(uint32 SlotIndex) const
{
	FTcsSkillEntryHandle Handle;
	Handle.Index = static_cast<int32>(SlotIndex);
	Handle.Generation = static_cast<int32>(SlotGenerations[SlotIndex]);
	return Handle;
}
