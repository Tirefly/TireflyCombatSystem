#include "Skill/Modifiers/Conditions/TcsSkillModCond_SkillHasTags.h"

#include "Skill/TcsSkillInstance.h"

bool UTcsSkillModCond_SkillHasTags::Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const
{
	if (!IsValid(SkillInstance))
	{
		return false;
	}

	const FGameplayTagContainer& Tags = SkillInstance->GetSkillDef().FunctionTags;

	if (!RequiredTags.IsEmpty() && !Tags.HasAll(RequiredTags))
	{
		return false;
	}

	if (!BlockedTags.IsEmpty() && Tags.HasAny(BlockedTags))
	{
		return false;
	}

	return true;
}
