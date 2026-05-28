// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsSkillDefinition.h"

#include "Skill/TcsSkillEntry.h"
#include "Skill/TcsSkillInstance.h"



UClass* UTcsSkillDefinition::ResolveSkillEntryClass() const
{
	return SkillEntryClass ? SkillEntryClass.Get() : UTcsSkillEntry::StaticClass();
}

UClass* UTcsSkillDefinition::ResolveStateInstanceClass() const
{
	return SkillInstanceClass ? SkillInstanceClass.Get() : UTcsSkillInstance::StaticClass();
}