// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillEntrySelector/TcsSkillEntrySelector_ByGameplayTag.h"

#include "Skill/TcsSkillComponent.h"
#include "Skill/TcsSkillDefinition.h"
#include "Skill/TcsSkillEntry.h"



TArray<UTcsSkillEntry*> UTcsSkillEntrySelector_ByGameplayTag::ResolveTargets(
	const FInstancedStruct& Config,
	UTcsSkillComponent* SkillComp) const
{
	TArray<UTcsSkillEntry*> Result;
	if (const auto* Cfg = Config.GetPtr<FTcsSkillEntrySelector_ByGameplayTagConfig>())
	{
		TArray<UTcsSkillEntry*> AllEntries = SkillComp->GetAllSkillEntries();
		for (UTcsSkillEntry* Entry : AllEntries)
		{
			if (!Entry)
			{
				continue;
			}
			const UTcsSkillDefinition* Def = Entry->GetSkillDefinition();
			if (!Def)
			{
				continue;
			}

			// 合并 CategoryTags + FunctionTags 进行匹配
			FGameplayTagContainer CombinedTags = Def->CategoryTags;
			CombinedTags.AppendTags(Def->FunctionTags);

			bool bMatched = false;
			if (Cfg->bMatchExactly)
			{
				bMatched = Cfg->MatchType == EGameplayContainerMatchType::All
					? CombinedTags.HasAllExact(Cfg->FilterTags)
					: CombinedTags.HasAnyExact(Cfg->FilterTags);
			}
			else
			{
				bMatched = Cfg->MatchType == EGameplayContainerMatchType::All
					? CombinedTags.HasAll(Cfg->FilterTags)
					: CombinedTags.HasAny(Cfg->FilterTags);
			}

			if (bMatched)
			{
				Result.Add(Entry);
			}
		}
	}
	return Result;
}
