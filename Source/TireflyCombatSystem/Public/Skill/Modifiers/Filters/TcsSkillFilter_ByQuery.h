#pragma once

#include "Skill/Modifiers/TcsSkillFilter.h"
#include "Skill/TcsSkillComponent.h"
#include "TcsSkillFilter_ByQuery.generated.h"

/**
 * 按 FTcsSkillQuery 进行筛选
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillFilter_ByQuery : public UTcsSkillFilter
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FTcsSkillQuery Query;

	virtual void Filter_Implementation(AActor* SkillOwner, TArray<UTcsSkillInstance*>& OutSkills) override;
};
