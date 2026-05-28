// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsAttributeModifier.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeChangeEventPayload.generated.h"



// 属性变化事件数据
USTRUCT(BlueprintType)
struct FTcsAttributeChangeEventPayload
{
	GENERATED_BODY()

public:
	// 属性名
	UPROPERTY(BlueprintReadOnly)
	FName AttributeName = NAME_None;
	
	// 属性新值
	UPROPERTY(BlueprintReadOnly)
	float NewValue = 0.f;

	// 属性旧值
	UPROPERTY(BlueprintReadOnly)
	float OldValue = 0.f;

	// 变化来源记录 (SourceHandle -> 变化值)
	UPROPERTY(BlueprintReadOnly)
	TMap<FTcsSourceHandle, float> ChangeSourceRecord;

public:
	FTcsAttributeChangeEventPayload() {}

	FTcsAttributeChangeEventPayload(
		FName InAttrName,
		float InNewVal,
		float InOldVal,
		const TMap<FTcsSourceHandle, float>& InChangeSourceRecord)
	{
		AttributeName = InAttrName;
		NewValue = InNewVal;
		OldValue = InOldVal;
		ChangeSourceRecord = InChangeSourceRecord;
	}
};



// 属性修改器事件数据
USTRUCT(BlueprintType)
struct FTcsAttributeModifierEventPayload
{
	GENERATED_BODY()

public:
	// 修改器实例
	UPROPERTY(BlueprintReadOnly)
	FTcsAttributeModifierInstance ModifierInstance;

public:
	FTcsAttributeModifierEventPayload() {}

	explicit FTcsAttributeModifierEventPayload(const FTcsAttributeModifierInstance& InModifierInstance)
	{
		ModifierInstance = InModifierInstance;
	}
};



// 属性边界事件数据
USTRUCT(BlueprintType)
struct FTcsAttributeBoundaryEventPayload
{
	GENERATED_BODY()

public:
	// 属性名
	UPROPERTY(BlueprintReadOnly)
	FName AttributeName = NAME_None;

	// 是否命中最大边界
	UPROPERTY(BlueprintReadOnly)
	bool bIsMaxBoundary = false;

	// 变化前的值
	UPROPERTY(BlueprintReadOnly)
	float OldValue = 0.f;

	// 变化后的值
	UPROPERTY(BlueprintReadOnly)
	float NewValue = 0.f;

	// 命中的边界值
	UPROPERTY(BlueprintReadOnly)
	float BoundaryValue = 0.f;

public:
	FTcsAttributeBoundaryEventPayload() {}

	FTcsAttributeBoundaryEventPayload(
		FName InAttributeName,
		bool bInIsMaxBoundary,
		float InOldValue,
		float InNewValue,
		float InBoundaryValue)
	{
		AttributeName = InAttributeName;
		bIsMaxBoundary = bInIsMaxBoundary;
		OldValue = InOldValue;
		NewValue = InNewValue;
		BoundaryValue = InBoundaryValue;
	}
};
