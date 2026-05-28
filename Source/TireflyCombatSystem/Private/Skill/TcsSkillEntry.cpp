// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillEntry.h"



UTcsSkillEntry::UTcsSkillEntry()
{
}

UWorld* UTcsSkillEntry::GetWorld() const
{
	return GetOuter() ? GetOuter()->GetWorld() : nullptr;
}