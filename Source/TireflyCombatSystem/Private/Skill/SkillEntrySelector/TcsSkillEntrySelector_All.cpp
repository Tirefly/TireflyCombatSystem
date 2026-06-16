// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillEntrySelector/TcsSkillEntrySelector_All.h"

#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillEntry.h"



TArray<UTcsSkillEntry*> UTcsSkillEntrySelector_All::ResolveTargets(
	const FInstancedStruct& Config,
	UTcsSkillComponent* SkillComp) const
{
	return SkillComp->GetAllSkillEntries();
}
