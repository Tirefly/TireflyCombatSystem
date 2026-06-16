// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_SetBool.generated.h"



// SkillModifier 执行器：设置布尔值
UCLASS(Blueprintable, Meta = (DisplayName = "SkillModifier 执行器：设置布尔值"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_SetBool : public UTcsStateParamBoolModifierExecution
{
	GENERATED_BODY()

public:
	virtual bool Evaluate(
		bool CurrentValue,
		const FStateParamBoolModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry,
		AActor* Instigator) const override;
};
