// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TireflyAttributeModifier.h"
#include "TireflyAttributeManagerSubsystem.generated.h"



// 属性管理器子系统，所有战斗实体执行属性相关逻辑的入口
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

#pragma region Attribute

public:
	// 给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttributes(
		AActor* CombatEntity,
		const TArray<FName>& AttributeNames);

protected:
	// 获取战斗实体的属性组件
	static class UTireflyAttributeComponent* GetAttributeComponent(const AActor* CombatEntity);

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

#pragma endregion
	

#pragma region AttributeModifier

public:
	// 创建属性修改器实例
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool CreateAttributeModifier(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttributeModifierIds"))FName ModifierId,
		FName SourceName,
		AActor* Instigator,
		AActor* Target,
		const TMap<FName, float>& Operands,
		FTireflyAttributeModifierInstance& OutModifierInst);
	
	// 给战斗实体应用多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 从战斗实体身上移除多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 处理战斗实体的属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

protected:
	// 全局属性修改器实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeModifierInstanceIdMgr = -1;

#pragma endregion


#pragma region AttributeCalculation

protected:
	// 重新计算战斗实体的属性基值
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 重新计算战斗实体的属性当前值
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity);

	// 属性修改器合并
	static void MergeAttributeModifiers(
		const AActor* CombatEntity,
		const TArray<FTireflyAttributeModifierInstance>& Modifiers,
		TArray<FTireflyAttributeModifierInstance>& MergedModifiers);

	// 将属性的给定值限制在指定范围内
	static void ClampAttributeValueInRange(UTireflyAttributeComponent* AttributeComponent, const FName& AttributeName, float& NewValue);

#pragma endregion
};
