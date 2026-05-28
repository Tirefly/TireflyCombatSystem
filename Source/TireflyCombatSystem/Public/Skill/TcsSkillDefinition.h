// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TcsStateDefinition.h"
#include "TcsSkillDefinition.generated.h"



class UTcsSkillEntry;
class UTcsSkillInstance;



/**
 * Skill 定义资产。
 *
 * 负责统一声明 Skill 的 learned 数据对象类型与激活执行态类型。
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsSkillDefinition : public UTcsStateDefinition
{
	GENERATED_BODY()

#pragma region Runtime

public:
	/** Skill 激活时创建的运行时实例类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSubclassOf<UTcsSkillInstance> SkillInstanceClass;

	/** SkillComponent 持有的 learned-skill 数据对象类。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Runtime")
	TSubclassOf<UTcsSkillEntry> SkillEntryClass;

	/** @return 当前 Skill 定义解析出的 learned-skill 数据对象类。 */
	virtual UClass* ResolveSkillEntryClass() const;

	/** @return 当前 Skill 定义解析出的激活执行态类。 */
	virtual UClass* ResolveStateInstanceClass() const override;

#pragma endregion
};