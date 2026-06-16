// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillModExecution/TcsSkillModExec_Addition.h"

#include "Skill/TcsSkillEntry.h"



float UTcsSkillModExec_Addition::Evaluate(
	float CurrentValue,
	const FStateParamNumericModifierInstance& ModifierInst,
	UTcsSkillEntry* SkillEntry,
	AActor* Instigator) const
{
	if (const auto* Config = ModifierInst.Config.GetPtr<FTcsSkillModifierFloatConfig>())
	{
		return CurrentValue + Config->Operand;
	}
	return CurrentValue;
}
