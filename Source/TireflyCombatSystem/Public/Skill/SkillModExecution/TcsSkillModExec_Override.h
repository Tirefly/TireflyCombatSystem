// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_Override.generated.h"



// SkillModifier 执行器：强制覆盖
UCLASS(Blueprintable, Meta = (DisplayName = "SkillModifier 执行器：强制覆盖"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_Override : public UTcsStateParamNumericModifierExecution
{
	GENERATED_BODY()

public:
	virtual float Evaluate(
		float CurrentValue,
		const FStateParamNumericModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry,
		AActor* Instigator) const override;
};
