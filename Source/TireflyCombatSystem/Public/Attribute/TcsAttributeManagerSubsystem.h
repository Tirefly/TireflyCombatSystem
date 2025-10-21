// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TcsAttributeModifier.h"
#include "TcsAttributeManagerSubsystem.generated.h"



// 属性管理器子系统，所有战斗实体执行属性相关逻辑的入口
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

#pragma region Attribute

public:
	// 给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttributes(
		AActor* CombatEntity,
		const TArray<FName>& AttributeNames);

protected:
	// 获取战斗实体的属性组件
	static class UTcsAttributeComponent* GetAttributeComponent(const AActor* CombatEntity);

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

#pragma endregion
	

#pragma region AttributeModifier

public:
	/**
	 * 创建属性修改器实例
	 * 
	 * @param ModifierId 属性修改器Id，通过TcsGenericLibrary.GetAttributeModifierIds获取
	 * @param SourceName 属性修改器来源，可以是技能Id、效果Id等
	 * @param Instigator 属性修改器发起者，应该是一个战斗实体
	 * @param Target 属性修改器目标，应该也是一个战斗实体
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool CreateAttributeModifier(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		FName SourceName,
		AActor* Instigator,
		AActor* Target,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	/**
	 * 创建属性修改器实例，并且设置属性修改器的操作数
	 * 
	 * @param ModifierId 属性修改器Id，通过TcsGenericLibrary.GetAttributeModifierIds获取
	 * @param SourceName 属性修改器来源，可以是技能Id、效果Id等
	 * @param Instigator 属性修改器发起者，应该是一个战斗实体
	 * @param Target 属性修改器目标，应该也是一个战斗实体
	 * @param Operands 属性修改器操作数
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool CreateAttributeModifierWithOperands(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		FName SourceName,
		AActor* Instigator,
		AActor* Target,
		const TMap<FName, float>& Operands,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	// 给战斗实体应用多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	// 从战斗实体身上移除多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	// 处理战斗实体的属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

protected:
	// 全局属性修改器实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeModifierInstanceIdMgr = -1;

#pragma endregion


#pragma region AttributeCalculation

protected:
	// 重新计算战斗实体的属性基值
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTcsAttributeModifierInstance>& Modifiers);

	// 重新计算战斗实体的属性当前值
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity);

	// 属性修改器合并
	static void MergeAttributeModifiers(
		const AActor* CombatEntity,
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers);

	// 将属性的给定值限制在指定范围内
	static void ClampAttributeValueInRange(UTcsAttributeComponent* AttributeComponent, const FName& AttributeName, float& NewValue);

#pragma endregion
};
