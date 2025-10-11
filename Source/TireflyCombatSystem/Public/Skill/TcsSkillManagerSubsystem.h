// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "State/TcsState.h"
#include "TcsSkillManagerSubsystem.generated.h"



class UTcsStateInstance;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsSkillManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 获取技能定义
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	FTcsStateDefinition GetSkillDefinition(FName SkillDefId);

	// 创建技能状态实例
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	UTcsStateInstance* CreateSkillStateInstance(AActor* Owner, FName SkillDefId, AActor* Instigator);
};
