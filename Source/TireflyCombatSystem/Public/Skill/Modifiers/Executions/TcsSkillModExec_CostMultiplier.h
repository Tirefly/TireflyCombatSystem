#pragma once

#include "Skill/Modifiers/TcsSkillModifierExecution.h"
#include "TcsSkillModExec_CostMultiplier.generated.h"

/**
 * 消耗倍率执行器
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModExec_CostMultiplier : public UTcsSkillModifierExecution
{
	GENERATED_BODY()

public:
	virtual void ExecuteToEffect_Implementation(
		UTcsSkillInstance* SkillInstance,
		const FTcsSkillModifierInstance& ModInst,
		FTcsAggregatedParamEffect& InOutEffect) override;

	virtual void Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance) override;
};
