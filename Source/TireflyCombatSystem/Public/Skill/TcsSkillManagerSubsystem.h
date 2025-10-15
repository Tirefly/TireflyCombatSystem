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

	// 应用技能修改器定义
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool ApplySkillModifier(AActor* TargetActor, const TArray<struct FTcsSkillModifierDefinition>& Modifiers, TArray<int32>& OutInstanceIds);

	// 移除技能修改器实例
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool RemoveSkillModifierById(AActor* TargetActor, int32 InstanceId);

	// 更新技能修改器状态（例如定期重算）
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	void UpdateCombatEntity(AActor* TargetActor);
};
