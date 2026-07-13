// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillCooldownTracker.h"

#include "Skill/TcsSkillEntry.h"



void FTcsSkillCooldownTracker::Add(UTcsSkillEntry* Entry)
{
	if (!Entry)
	{
		return;
	}

	const FName SkillDefId = Entry->GetSkillDefId();
	if (!SkillDefId.IsNone())
	{
		TrackedEntries.Add(SkillDefId, Entry);
	}
}

void FTcsSkillCooldownTracker::Remove(UTcsSkillEntry* Entry)
{
	if (!Entry)
	{
		return;
	}

	const FName SkillDefId = Entry->GetSkillDefId();
	if (!SkillDefId.IsNone())
	{
		TrackedEntries.Remove(SkillDefId);
	}
}

void FTcsSkillCooldownTracker::Tick(float DeltaTime)
{
	TArray<FName> ExpiredEntries;
	for (auto& Pair : TrackedEntries)
	{
		if (Pair.Value.IsValid())
		{
			Pair.Value->TickCooldown(DeltaTime);
			if (!Pair.Value->IsOnCooldown())
			{
				ExpiredEntries.Add(Pair.Key);
			}
		}
		else
		{
			ExpiredEntries.Add(Pair.Key);
		}
	}

	for (const FName& Key : ExpiredEntries)
	{
		TrackedEntries.Remove(Key);
	}
}
