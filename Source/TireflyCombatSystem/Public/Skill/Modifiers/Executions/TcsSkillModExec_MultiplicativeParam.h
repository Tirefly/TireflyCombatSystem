#pragma once

#include "Skill/Modifiers/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_MultiplicativeParam.generated.h"

/**
 * 默认乘法参数执行器
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_MultiplicativeParam : public UTcsSkillModifierExecution
{
	GENERATED_BODY()

public:
	virtual void ExecuteToEffect_Implementation(
		UTcsSkillInstance* SkillInstance,
		const FTcsSkillModifierInstance& ModInst,
		FTcsAggregatedParamEffect& InOutEffect) override;

	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance) override;
};
