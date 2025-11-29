// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TcsAttribute.h"
#include "TcsAttributeChangeEventPayload.h"
#include "TcsAttributeModifier.h"
#include "TcsAttributeComponent.generated.h"



// 属性值改变事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsAttributeChangeDelegate,
	const TArray<FTcsAttributeChangeEventPayload>&, Payloads);

// 属性修改器添加事件委托声明
// (修改器实例)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsOnAttributeModifierAddedSignature,
	const FTcsAttributeModifierInstance&, ModifierInstance);

// 属性修改器移除事件委托声明
// (修改器实例)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsOnAttributeModifierRemovedSignature,
	const FTcsAttributeModifierInstance&, ModifierInstance);

// 属性达到边界值事件委托声明
// (属性名称, 边界类型: true=最大值, false=最小值, 当前值)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnAttributeReachedBoundarySignature,
	FName, AttributeName,
	bool, bIsMaxBoundary,
	float, CurrentValue);



// 属性组件，保存战斗实体的属性相关数据
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Cmp"))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	UTcsAttributeComponent();

protected:
	virtual void BeginPlay() override;

#pragma endregion


#pragma region Attribute

public:
	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	// 获取所有属性的当前值
	TMap<FName, float> GetAttributeValues() const;
	// 获取所有属性的基础值
	TMap<FName, float> GetAttributeBaseValues() const;

	// 广播属性当前值改变事件
	void BroadcastAttributeValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;
	// 广播属性基础值改变事件
	void BroadcastAttributeBaseValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;
	// 广播属性修改器添加事件
	void BroadcastAttributeModifierAddedEvent(const FTcsAttributeModifierInstance& ModifierInstance) const;
	// 广播属性修改器移除事件
	void BroadcastAttributeModifierRemovedEvent(const FTcsAttributeModifierInstance& ModifierInstance) const;
	// 广播属性达到边界值事件
	void BroadcastAttributeReachedBoundaryEvent(FName AttributeName, bool bIsMaxBoundary, float CurrentValue) const;

public:
	// 战斗实体的所有属性实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTcsAttributeInstance> Attributes;

	// 战斗实体的所有属性修改器实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TArray<FTcsAttributeModifierInstance> AttributeModifiers;

	// 属性当前值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeValueChanged;

	// 属性基础值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeBaseValueChanged;

	// 属性修改器添加事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeModifierAddedSignature OnAttributeModifierAdded;

	// 属性修改器移除事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeModifierRemovedSignature OnAttributeModifierRemoved;

	/**
	 * 属性达到边界值事件
	 * 当属性值达到最大值或最小值时广播
	 * bIsMaxBoundary: true表示达到最大值，false表示达到最小值（如HP归零）
	 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeReachedBoundarySignature OnAttributeReachedBoundary;

#pragma endregion
};
