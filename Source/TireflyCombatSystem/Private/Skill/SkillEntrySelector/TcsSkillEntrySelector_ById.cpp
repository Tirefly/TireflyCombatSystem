// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillEntrySelector/TcsSkillEntrySelector_ById.h"

#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillEntry.h"



TArray<UTcsSkillEntry*> UTcsSkillEntrySelector_ById::ResolveTargets(
	const FInstancedStruct& Config,
	UTcsSkillComponent* SkillComp) const
{
	TArray<UTcsSkillEntry*> Result;
	if (const auto* Cfg = Config.GetPtr<FTcsSkillEntrySelector_ByIdConfig>())
	{
		if (UTcsSkillEntry* Entry = SkillComp->GetSkillEntry(Cfg->SkillDefId))
		{
			Result.Add(Entry);
		}
	}
	return Result;
}
