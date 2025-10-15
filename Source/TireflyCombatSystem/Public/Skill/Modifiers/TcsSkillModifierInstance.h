#pragma once

#include "CoreMinimal.h"
#include "Skill/Modifiers/TcsSkillModifierDefinition.h"
#include "TcsSkillModifierInstance.generated.h"

class UTcsSkillInstance;

/**
 * 技能修改器运行时实例
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierInstance
{
	GENERATED_BODY()

public:
	/** 静态定义 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillModifier")
	FTcsSkillModifierDefinition ModifierDef;

	/** 实例 Id（全局唯一） */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillModifier")
	int32 SkillModInstanceId = -1;

	/** 指向技能实例，可为空（如按 Filter 生成） */
	UPROPERTY()
	TObjectPtr<UTcsSkillInstance> SkillInstance = nullptr;

	/** 应用时间戳 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillModifier")
	int64 ApplyTime = 0;

	/** 最近一次更新的时间戳 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillModifier")
	int64 UpdateTime = 0;

public:
	bool IsValid() const
	{
		return ModifierDef.ModifierName != NAME_None && SkillModInstanceId >= 0;
	}
};
