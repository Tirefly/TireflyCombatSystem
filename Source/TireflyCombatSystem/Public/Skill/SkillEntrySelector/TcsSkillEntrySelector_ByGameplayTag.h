// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Skill/SkillEntrySelector/TcsSkillEntrySelector.h"
#include "TcsSkillEntrySelector_ByGameplayTag.generated.h"



/**
 * ByGameplayTag 选择器的筛选配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillEntrySelector_ByGameplayTagConfig
{
	GENERATED_BODY()

	/** 匹配的 GameplayTag 容器。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer FilterTags;

	/** 容器内 Tag 的匹配精度（Any：任一命中 / All：全部命中）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EGameplayContainerMatchType MatchType = EGameplayContainerMatchType::Any;

	/** 是否精确匹配（true=只匹配完全相同的 Tag，不匹配父级）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bMatchExactly = false;
};



// SkillEntry 选择器：按 GameplayTag 筛选
UCLASS(Blueprintable, Meta = (DisplayName = "SkillEntry 选择器：按 GameplayTag 筛选"))
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntrySelector_ByGameplayTag : public UTcsSkillEntrySelector
{
	GENERATED_BODY()

public:
	virtual TArray<UTcsSkillEntry*> ResolveTargets(
		const FInstancedStruct& Config,
		UTcsSkillComponent* SkillComp) const override;
};
