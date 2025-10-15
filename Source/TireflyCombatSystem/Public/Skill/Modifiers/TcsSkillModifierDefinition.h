#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "State/TcsState.h"
#include "TcsSkillModifierDefinition.generated.h"

class UTcsSkillFilter;
class UTcsSkillModifierCondition;
class UTcsSkillModifierExecution;
class UTcsSkillModifierMerger;

/**
 * 技能修改器定义：描述修改器的筛选、条件、执行与合并策略
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	/** 修改器标识 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FName ModifierName = NAME_None;

	/** 修改器执行优先级（值越小优先级越高） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	int32 Priority = 0;

	/** 技能筛选器 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	TSubclassOf<UTcsSkillFilter> SkillFilter;

	/** 激活条件（全部满足才生效） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	TArray<TSubclassOf<UTcsSkillModifierCondition>> ActiveConditions;

	/** 修正参数载荷（沿用状态参数结构） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	FTcsStateParameter ModifierParameter;

	/** 执行器类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	TSubclassOf<UTcsSkillModifierExecution> ExecutionType;

	/** 合并器类型 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	TSubclassOf<UTcsSkillModifierMerger> MergeType;
};
