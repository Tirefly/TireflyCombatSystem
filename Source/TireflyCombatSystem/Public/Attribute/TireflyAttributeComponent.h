// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TireflyAttribute.h"
#include "TireflyAttributeChangeEventPayload.h"
#include "TireflyAttributeModifier.h"
#include "TireflyAttributeComponent.generated.h"



// 属性值改变事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTireflyAttributeChangeDelegate,
	const TArray<FTireflyAttributeChangeEventPayload>&, Payloads);



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
	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const;

	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TireflyCombatSystemLibrary.GetAttribtueNames"))FName AttributeName,
		float& OutValue) const;

	// 获取所有属性的当前值
	TMap<FName, float> GetAttributeValues() const;
	// 获取所有属性的基础值
	TMap<FName, float> GetAttributeBaseValues() const;

	// 广播属性当前值改变事件
	void BroadcastAttributeValueChangeEvent(const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const;
	// 广播属性基础值改变事件
	void BroadcastAttributeBaseValueChangeEvent(const TArray<FTireflyAttributeChangeEventPayload>& Payloads) const;

public:
	// 战斗实体的所有属性实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTireflyAttributeInstance> Attributes;

	// 战斗实体的所有属性修改器实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TArray<FTireflyAttributeModifierInstance> AttributeModifiers;

	// 属性当前值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute")
	FTireflyAttributeChangeDelegate OnAttributeValueChanged;

	// 属性基础值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute")
	FTireflyAttributeChangeDelegate OnAttributeBaseValueChanged;

#pragma endregion
};
