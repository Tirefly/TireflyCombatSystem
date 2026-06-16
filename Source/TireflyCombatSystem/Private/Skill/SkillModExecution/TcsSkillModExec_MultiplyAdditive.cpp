// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillModExecution/TcsSkillModExec_MultiplyAdditive.h"

#include "Skill/TcsSkillEntry.h"



float UTcsSkillModExec_MultiplyAdditive::Evaluate(
	float CurrentValue,
	const FStateParamNumericModifierInstance& ModifierInst,
	UTcsSkillEntry* SkillEntry,
	AActor* Instigator) const
{
	if (const auto* Config = ModifierInst.Config.GetPtr<FTcsSkillModifierFloatConfig>())
	{
		return CurrentValue * (1.0f + Config->Operand);
	}
	return CurrentValue;
}
