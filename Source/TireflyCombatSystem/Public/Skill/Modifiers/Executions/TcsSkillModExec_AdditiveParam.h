#pragma once

#include "Skill/Modifiers/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_AdditiveParam.generated.h"

/**
 * 默认加法参数执行器
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_AdditiveParam : public UTcsSkillModifierExecution
{
	GENERATED_BODY()

public:
	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance) override;
};
