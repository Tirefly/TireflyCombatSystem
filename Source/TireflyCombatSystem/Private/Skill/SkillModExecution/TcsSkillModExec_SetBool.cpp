// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillModExecution/TcsSkillModExec_SetBool.h"

#include "Skill/TcsSkillEntry.h"



bool UTcsSkillModExec_SetBool::Evaluate(
	bool CurrentValue,
	const FStateParamBoolModifierInstance& ModifierInst,
	UTcsSkillEntry* SkillEntry,
	AActor* Instigator) const
{
	if (const auto* Config = ModifierInst.Config.GetPtr<FTcsSkillModifierBoolConfig>())
	{
		return Config->Value;
	}
	return CurrentValue;
}
