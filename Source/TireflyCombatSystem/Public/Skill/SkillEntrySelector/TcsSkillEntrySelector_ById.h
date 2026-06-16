// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/SkillEntrySelector/TcsSkillEntrySelector.h"
#include "TcsSkillEntrySelector_ById.generated.h"



/**
 * ById 选择器的目标技能配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillEntrySelector_ByIdConfig
{
	GENERATED_BODY()

	/** 目标技能 DefId。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SkillDefId;
};



// SkillEntry 选择器：按 Id 精确匹配
UCLASS(Blueprintable, Meta = (DisplayName = "SkillEntry 选择器：按 Id 精确匹配"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntrySelector_ById : public UTcsSkillEntrySelector
{
	GENERATED_BODY()

public:
	virtual TArray<UTcsSkillEntry*> ResolveTargets(
		const FInstancedStruct& Config,
		UTcsSkillComponent* SkillComp) const override;
};
