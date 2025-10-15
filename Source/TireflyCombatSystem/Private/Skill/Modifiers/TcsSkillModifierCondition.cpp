#include "Skill/Modifiers/TcsSkillModifierCondition.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"

bool UTcsSkillModifierCondition::Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const
{
	// 默认实现：始终视为通过
	return true;
}
