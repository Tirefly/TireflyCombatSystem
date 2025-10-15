#pragma once

#include "Skill/Modifiers/TcsSkillModifierCondition.h"
#include "TcsSkillModCond_SkillLevelInRange.generated.h"

/**
 * 条件：技能等级落在指定范围
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_SkillLevelInRange : public UTcsSkillModifierCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	int32 MinLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	int32 MaxLevel = 9999;

	virtual bool Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const override;
};
