#include "Skill/Modifiers/TcsSkillModifierMerger.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"

void UTcsSkillModifierMerger::Merge_Implementation(TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers)
{
	// 默认实现为直接追加
	OutModifiers.Append(SourceModifiers);
}
