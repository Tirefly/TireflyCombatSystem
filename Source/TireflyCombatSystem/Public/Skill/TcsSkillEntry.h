// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsSkillEntry.generated.h"



class UTcsSkillDefinition;



/**
 * 已学会技能的数据对象。
 *
 * 负责表达 SkillComponent 持有的 learned-skill 身份，并记录其绑定的 Skill 定义资产。
 * 当前阶段不承载等级、冷却等 Skill-only 逻辑，也不承载一次技能激活的运行时执行态。
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillEntry : public UObject
{
	GENERATED_BODY()

#pragma region UObject

public:
	/** 构造默认的 learned-skill 数据对象。 */
	UTcsSkillEntry();

	/** @return 当前对象所在的世界；无有效外层时返回 nullptr。 */
	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region Skill

public:
	/** @return 当前 learned-skill 绑定的 Skill 定义资产。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	UTcsSkillDefinition* GetSkillDefinition() const { return SkillDefinition.Get(); }

	/** 设置当前 learned-skill 绑定的 Skill 定义资产。 */
	UFUNCTION(BlueprintCallable, Category = "Skill|Entry")
	void SetSkillDefinition(UTcsSkillDefinition* InSkillDefinition) { SkillDefinition = InSkillDefinition; }

protected:
	/** 当前 learned-skill 绑定的 Skill 定义资产。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Entry", Meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UTcsSkillDefinition> SkillDefinition = nullptr;

#pragma endregion
};