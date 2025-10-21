// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsSkillInstance.generated.h"



/**
 * 技能实例类
 * 代表战斗实体已学会的技能，持有技能的动态属性和运行时参数
 */
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsSkillInstance : public UObject
{
	GENERATED_BODY()

#pragma region UObject

public:
	UTcsSkillInstance();

	virtual UWorld* GetWorld() const override;

#pragma endregion
};
