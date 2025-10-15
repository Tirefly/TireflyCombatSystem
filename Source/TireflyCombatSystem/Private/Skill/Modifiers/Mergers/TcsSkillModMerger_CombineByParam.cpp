#include "Skill/Modifiers/Mergers/TcsSkillModMerger_CombineByParam.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"

void UTcsSkillModMerger_CombineByParam::Merge_Implementation(TArray<FTcsSkillModifierInstance>& SourceModifiers, TArray<FTcsSkillModifierInstance>& OutModifiers)
{
	// 目前仍按顺序追加，后续在聚合阶段优化
	OutModifiers.Append(SourceModifiers);
}
