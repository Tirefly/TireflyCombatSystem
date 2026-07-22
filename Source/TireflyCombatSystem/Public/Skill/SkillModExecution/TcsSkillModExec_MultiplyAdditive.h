// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_MultiplyAdditive.generated.h"



// SkillModifier 执行器：加法倍率
UCLASS(Blueprintable, Meta = (DisplayName = "SkillModifier 执行器：加法倍率"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_MultiplyAdditive : public UTcsStateParamNumericModifierExecution
{
	GENERATED_BODY()

protected:
	virtual float Evaluate(
		float CurrentValue,
		const FStateParamNumericModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry,
		AActor* Instigator) const override;
};
