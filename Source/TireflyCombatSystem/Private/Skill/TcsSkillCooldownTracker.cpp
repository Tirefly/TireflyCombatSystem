// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillCooldownTracker.h"

#include "Skill/TcsSkillEntry.h"



void FTcsSkillCooldownTracker::Add(UTcsSkillEntry* Entry)
{
	if (!Entry)
	{
		return;
	}

	if (const UTcsSkillDefinition* Def = Entry->GetSkillDefinition())
	{
		TrackedEntries.Add(Def->GetFName(), Entry);
	}
}

void FTcsSkillCooldownTracker::Remove(UTcsSkillEntry* Entry)
{
	if (!Entry)
	{
		return;
	}

	if (const UTcsSkillDefinition* Def = Entry->GetSkillDefinition())
	{
		TrackedEntries.Remove(Def->GetFName());
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