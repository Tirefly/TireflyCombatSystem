#pragma once

#include "Skill/Modifiers/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_CooldownMultiplier.generated.h"

/**
 * 冷却倍率执行器
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_CooldownMultiplier : public UTcsSkillModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance) override;
};
