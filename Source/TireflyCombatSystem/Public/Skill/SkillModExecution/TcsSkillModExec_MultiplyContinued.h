// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_MultiplyContinued.generated.h"



// SkillModifier 执行器：连续乘算
UCLASS(Blueprintable, Meta = (DisplayName = "SkillModifier 执行器：连续乘算"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_MultiplyContinued : public UTcsStateParamNumericModifierExecution
{
	GENERATED_BODY()

public:
	virtual float Evaluate(
		float CurrentValue,
		const FStateParamNumericModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry,
		AActor* Instigator) const override;
};
