// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TireflyAttribute.h"
#include "TireflyAttributeModifier.h"
#include "TireflyAttributeComponent.generated.h"



// 属性组件，保存战斗实体的属性相关数据
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflyAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTireflyAttributeComponent();

protected:
	virtual void BeginPlay() override;

#pragma endregion


#pragma region Attribute

public:
	//  获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystem.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const;

	//  获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystem.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const;

	TMap<FName, float> GetAttributeValues() const;
	TMap<FName, float> GetAttributeBaseValues() const;

public:
	// 战斗实体的所有属性实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTireflyAttributeInstance> Attributes;

	// 战斗实体的所有属性修改器实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TArray<FTireflyAttributeModifierInstance> AttributeModifiers;

#pragma endregion
};
