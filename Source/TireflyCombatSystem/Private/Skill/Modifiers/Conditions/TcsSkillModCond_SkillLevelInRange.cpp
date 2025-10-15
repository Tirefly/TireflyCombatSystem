#include "Skill/Modifiers/Conditions/TcsSkillModCond_SkillLevelInRange.h"

#include "Skill/TcsSkillInstance.h"

bool UTcsSkillModCond_SkillLevelInRange::Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const
{
	if (!IsValid(SkillInstance))
	{
		return false;
	}

	const int32 Level = SkillInstance->GetCurrentLevel();
	return Level >= MinLevel && Level <= MaxLevel;
}
