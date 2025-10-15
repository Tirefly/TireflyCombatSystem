#include "Skill/Modifiers/Conditions/TcsSkillModCond_AlwaysTrue.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"

bool UTcsSkillModCond_AlwaysTrue::Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const
{
	return true;
}
