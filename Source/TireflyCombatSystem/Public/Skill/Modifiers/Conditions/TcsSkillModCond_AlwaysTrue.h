#pragma once

#include "Skill/Modifiers/TcsSkillModifierCondition.h"
#include "TcsSkillModCond_AlwaysTrue.generated.h"

/**
 * 条件：恒为真
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_AlwaysTrue : public UTcsSkillModifierCondition
{
	GENERATED_BODY()

public:
	virtual bool Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const override;
};
