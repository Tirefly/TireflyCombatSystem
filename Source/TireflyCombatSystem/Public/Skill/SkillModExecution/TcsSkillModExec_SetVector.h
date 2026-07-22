// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillModExecution/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_SetVector.generated.h"



// SkillModifier 执行器：设置向量值
UCLASS(Blueprintable, Meta = (DisplayName = "SkillModifier 执行器：设置向量值"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_SetVector : public UTcsStateParamVectorModifierExecution
{
	GENERATED_BODY()

protected:
	virtual FVector Evaluate(
		FVector CurrentValue,
		const FStateParamVectorModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry,
		AActor* Instigator) const override;
};
