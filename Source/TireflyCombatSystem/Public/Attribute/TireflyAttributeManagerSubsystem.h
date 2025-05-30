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
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystem.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "Attribute")
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
	

#pragma region AttribtueModifier

public:	
	// 给战斗实体应用多个属性修改器
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 从战斗实体身上移除多个属性修改器
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 处理战斗实体的属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable,  Category = "TireflyCombatSystem|Attribute")
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTireflyAttributeModifierInstance>& Modifiers);

protected:
	// 重新计算战斗实体的属性基值
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTireflyAttributeModifierInstance>& Modifiers);

	// 重新计算战斗实体的属性当前值
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity);

	// 将属性的给定值限制在指定范围内
	static void ClampAttributeValueInRange(UTireflyAttributeComponent* AttributeComponent, const FName& AttributeName, float& NewValue);

#pragma endregion
};
