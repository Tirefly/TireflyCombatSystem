// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "State/TireflyState.h"
#include "TireflySkillManagerSubsystem.generated.h"

class UTireflyStateInstance;

UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflySkillManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 获取技能定义
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	FTireflyStateDefinition GetSkillDefinition(FName SkillDefId);

	// 创建技能状态实例
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	UTireflyStateInstance* CreateSkillStateInstance(AActor* Owner, FName SkillDefId, AActor* Instigator);
};
