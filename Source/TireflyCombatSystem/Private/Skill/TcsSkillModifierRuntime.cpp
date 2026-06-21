// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillModifierRuntime.h"



namespace
{
	template <typename KeyType>
	void AddRuntimeIdToIndex(TMap<KeyType, TArray<int32>>& Index, const KeyType& Key, int32 RuntimeModifierId)
	{
		Index.FindOrAdd(Key).Add(RuntimeModifierId);
	}


	template <typename KeyType>
	void RemoveRuntimeIdFromIndex(TMap<KeyType, TArray<int32>>& Index, const KeyType& Key, int32 RuntimeModifierId)
	{
		if (TArray<int32>* FoundRuntimeIds = Index.Find(Key))
		{
			FoundRuntimeIds->RemoveSingleSwap(RuntimeModifierId);
			if (FoundRuntimeIds->IsEmpty())
			{
				Index.Remove(Key);
			}
		}
	}


	template <typename KeyType>
	bool FindEntriesByIndex(
		const TMap<KeyType, TArray<int32>>& Index,
		const KeyType& Key,
		const TMap<int32, FTcsSkillModifierRuntimeEntry>& RuntimeEntriesById,
		TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries)
	{
		const TArray<int32>* FoundRuntimeIds = Index.Find(Key);
		if (!FoundRuntimeIds)
		{
			return false;
		}

		OutEntries.Reserve(OutEntries.Num() + FoundRuntimeIds->Num());
		for (const int32 RuntimeModifierId : *FoundRuntimeIds)
		{
			if (const FTcsSkillModifierRuntimeEntry* FoundEntry = RuntimeEntriesById.Find(RuntimeModifierId))
			{
				OutEntries.Add(FoundEntry);
			}
		}

		return OutEntries.Num() > 0;
	}
}


bool FTcsSkillModifierConflictKey::operator==(const FTcsSkillModifierConflictKey& Other) const
{
	return ModifierId == Other.ModifierId
		&& TargetSkillEntry == Other.TargetSkillEntry;
}


uint32 GetTypeHash(const FTcsSkillModifierConflictKey& Key)
{
	const uint32 ModifierIdHash = GetTypeHash(Key.ModifierId);
	const uint32 TargetEntryHash = GetTypeHash(Key.TargetSkillEntry);
	return HashCombine(ModifierIdHash, TargetEntryHash);
}


FTcsSkillModifierConflictKey FTcsSkillModifierRuntimeEntry::MakeConflictKey() const
{
	FTcsSkillModifierConflictKey Key;
	Key.ModifierId = ModifierId;
	Key.TargetSkillEntry = TargetSkillEntry;
	return Key;
}


bool FTcsSkillModifierRuntimeIndex::AddRuntimeEntry(const FTcsSkillModifierRuntimeEntry& Entry)
{
	if (Entry.RuntimeModifierId == INDEX_NONE
		|| Entry.ModifierId.IsNone()
		|| !Entry.TargetSkillEntry.IsValid()
		|| !Entry.TargetParamTag.IsValid()
		|| !Entry.SourceHandle.IsValid()
		|| RuntimeEntriesById.Contains(Entry.RuntimeModifierId))
	{
		return false;
	}

	RuntimeEntriesById.Add(Entry.RuntimeModifierId, Entry);
	AddRuntimeIdToIndex(RuntimeIdsBySourceHandleId, Entry.SourceHandle.Id, Entry.RuntimeModifierId);
	AddRuntimeIdToIndex(RuntimeIdsByTargetEntry, Entry.TargetSkillEntry, Entry.RuntimeModifierId);
	AddRuntimeIdToIndex(RuntimeIdsByConflictKey, Entry.MakeConflictKey(), Entry.RuntimeModifierId);
	return true;
}


bool FTcsSkillModifierRuntimeIndex::RemoveRuntimeEntry(int32 RuntimeModifierId, FTcsSkillModifierRuntimeEntry* OutRemovedEntry)
{
	const FTcsSkillModifierRuntimeEntry* FoundEntry = RuntimeEntriesById.Find(RuntimeModifierId);
	if (!FoundEntry)
	{
		return false;
	}

	const FTcsSkillModifierRuntimeEntry RemovedEntry = *FoundEntry;
	RemoveRuntimeIdFromIndex(RuntimeIdsBySourceHandleId, RemovedEntry.SourceHandle.Id, RuntimeModifierId);
	RemoveRuntimeIdFromIndex(RuntimeIdsByTargetEntry, RemovedEntry.TargetSkillEntry, RuntimeModifierId);
	RemoveRuntimeIdFromIndex(RuntimeIdsByConflictKey, RemovedEntry.MakeConflictKey(), RuntimeModifierId);
	RuntimeEntriesById.Remove(RuntimeModifierId);

	if (OutRemovedEntry)
	{
		*OutRemovedEntry = RemovedEntry;
	}

	return true;
}


const FTcsSkillModifierRuntimeEntry* FTcsSkillModifierRuntimeIndex::FindRuntimeEntry(int32 RuntimeModifierId) const
{
	return RuntimeEntriesById.Find(RuntimeModifierId);
}


FTcsSkillModifierRuntimeEntry* FTcsSkillModifierRuntimeIndex::FindRuntimeEntryMutable(int32 RuntimeModifierId)
{
	return RuntimeEntriesById.Find(RuntimeModifierId);
}


bool FTcsSkillModifierRuntimeIndex::FindBySourceHandle(
	const FTcsSourceHandle& SourceHandle,
	TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const
{
	if (!SourceHandle.IsValid())
	{
		return false;
	}

	return FindEntriesByIndex(RuntimeIdsBySourceHandleId, SourceHandle.Id, RuntimeEntriesById, OutEntries);
}


bool FTcsSkillModifierRuntimeIndex::FindBySkillEntry(
	UTcsSkillEntry* SkillEntry,
	TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const
{
	if (!IsValid(SkillEntry))
	{
		return false;
	}

	return FindEntriesByIndex(RuntimeIdsByTargetEntry, TWeakObjectPtr<UTcsSkillEntry>(SkillEntry), RuntimeEntriesById, OutEntries);
}


bool FTcsSkillModifierRuntimeIndex::FindConflictSet(
	const FTcsSkillModifierConflictKey& Key,
	TArray<const FTcsSkillModifierRuntimeEntry*>& OutEntries) const
{
	return FindEntriesByIndex(RuntimeIdsByConflictKey, Key, RuntimeEntriesById, OutEntries);
}


bool FTcsSkillModifierRuntimeIndex::RemoveAllForSkillEntry(
	UTcsSkillEntry* SkillEntry,
	TArray<FTcsSkillModifierRuntimeEntry>& OutRemovedEntries)
{
	if (!IsValid(SkillEntry))
	{
		return false;
	}

	const TArray<int32>* FoundRuntimeIds = RuntimeIdsByTargetEntry.Find(SkillEntry);
	if (!FoundRuntimeIds)
	{
		return false;
	}

	const TArray<int32> RuntimeIdsToRemove = *FoundRuntimeIds;
	OutRemovedEntries.Reserve(OutRemovedEntries.Num() + RuntimeIdsToRemove.Num());

	for (const int32 RuntimeModifierId : RuntimeIdsToRemove)
	{
		FTcsSkillModifierRuntimeEntry RemovedEntry;
		if (RemoveRuntimeEntry(RuntimeModifierId, &RemovedEntry))
		{
			OutRemovedEntries.Add(MoveTemp(RemovedEntry));
		}
	}

	return RuntimeIdsToRemove.Num() > 0;
}


void FTcsSkillModifierRuntimeIndex::Reset()
{
	RuntimeEntriesById.Reset();
	RuntimeIdsBySourceHandleId.Reset();
	RuntimeIdsByTargetEntry.Reset();
	RuntimeIdsByConflictKey.Reset();
}