// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillModExecution/TcsSkillModExec_Override.h"

#include "Skill/TcsSkillEntry.h"



float UTcsSkillModExec_Override::Evaluate(
	float CurrentValue,
	const FStateParamNumericModifierInstance& ModifierInst,
	UTcsSkillEntry* SkillEntry,
	AActor* Instigator) const
{
	if (const auto* Config = ModifierInst.Config.GetPtr<FTcsSkillModifierFloatConfig>())
	{
		return Config->Operand;
	}
	return CurrentValue;
}
