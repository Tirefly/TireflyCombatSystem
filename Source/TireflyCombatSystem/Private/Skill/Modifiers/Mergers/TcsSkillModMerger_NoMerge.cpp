#include "Skill/Modifiers/Mergers/TcsSkillModMerger_NoMerge.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"

void UTcsSkillModMerger_NoMerge::Merge_Implementation(TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers)
{
	OutModifiers.Append(SourceModifiers);
}
