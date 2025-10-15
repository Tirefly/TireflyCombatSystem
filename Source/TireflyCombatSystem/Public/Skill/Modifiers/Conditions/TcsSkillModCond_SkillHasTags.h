#pragma once

#include "Skill/Modifiers/TcsSkillModifierCondition.h"
#include "GameplayTagContainer.h"
#include "TcsSkillModCond_SkillHasTags.generated.h"

/**
 * 条件：技能拥有/不拥有特定标签
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillModCond_SkillHasTags : public UTcsSkillModifierCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FGameplayTagContainer RequiredTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FGameplayTagContainer BlockedTags;

	virtual bool Evaluate_Implementation(AActor* Owner, UTcsSkillInstance* SkillInstance, UTcsStateInstance* ActiveStateInstance, const FTcsSkillModifierInstance& ModInst) const override;
};
