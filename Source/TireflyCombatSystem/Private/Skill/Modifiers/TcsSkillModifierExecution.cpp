#include "Skill/Modifiers/TcsSkillModifierExecution.h"

#include "Skill/Modifiers/TcsSkillModifierInstance.h"
#include "Skill/Modifiers/TcsSkillModifierEffect.h"

void UTcsSkillModifierExecution::ExecuteToEffect_Implementation(
	UTcsSkillInstance* SkillInstance,
	const FTcsSkillModifierInstance& ModInst,
	FTcsAggregatedParamEffect& InOutEffect)
{
	// 默认实现为空，实际逻辑由子类覆盖
}

void UTcsSkillModifierExecution::Execute_Implementation(UTcsSkillInstance* SkillInstance, FTcsSkillModifierInstance& SkillModInstance)
{
	// 默认实现为空，实际逻辑由子类覆盖
}
