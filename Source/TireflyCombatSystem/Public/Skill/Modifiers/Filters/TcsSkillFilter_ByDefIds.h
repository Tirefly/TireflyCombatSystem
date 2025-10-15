#pragma once

#include "Skill/Modifiers/TcsSkillFilter.h"
#include "TcsSkillFilter_ByDefIds.generated.h"

/**
 * 按技能定义 ID 列表过滤
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter_ByDefIds : public UTcsSkillFilter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	TArray<FName> SkillDefIds;

	virtual void Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills) override;
};
