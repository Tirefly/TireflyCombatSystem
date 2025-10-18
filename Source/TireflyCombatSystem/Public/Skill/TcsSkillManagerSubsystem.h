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

	/**
	 * 从数据表加载修改器定义
	 * @param ModifierId 修改器行ID
	 * @param OutDef 输出的修改器定义
	 * @return 是否成功加载
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool LoadModifierDefinition(FName ModifierId, FTcsSkillModifierDefinition& OutDef);

	/**
	 * 通过ID应用技能修改器(数据表驱动版本)
	 * @param TargetActor 目标Actor
	 * @param ModifierIds 修改器行ID列表
	 * @param OutInstanceIds 输出的实例ID列表
	 * @return 是否成功应用
	 */
	UFUNCTION(BlueprintCallable, Category = "Skill Manager")
	bool ApplySkillModifierByIds(AActor* TargetActor, const TArray<FName>& ModifierIds, TArray<int32>& OutInstanceIds);

protected:
	/**
	 * 校验修改器定义的有效性
	 * @param Definition 待校验的定义
	 * @param ModifierId 修改器ID(用于日志)
	 * @return 是否有效
	 */
	bool ValidateModifierDefinition(const FTcsSkillModifierDefinition& Definition, FName ModifierId) const;
};
