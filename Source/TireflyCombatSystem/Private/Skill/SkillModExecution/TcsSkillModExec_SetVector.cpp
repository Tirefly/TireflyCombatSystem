// Copyright Tirefly. All Rights Reserved.

#include "Skill/SkillModExecution/TcsSkillModExec_SetVector.h"

#include "Skill/TcsSkillEntry.h"



FVector UTcsSkillModExec_SetVector::Evaluate(
	FVector CurrentValue,
	const FStateParamVectorModifierInstance& ModifierInst,
	UTcsSkillEntry* SkillEntry,
	AActor* Instigator) const
{
	if (const auto* Config = ModifierInst.Config.GetPtr<FTcsSkillModifierVectorConfig>())
	{
		return Config->Value;
	}
	return CurrentValue;
}
