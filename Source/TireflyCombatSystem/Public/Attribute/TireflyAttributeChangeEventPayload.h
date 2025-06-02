// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyAttributeChangeEventPayload.generated.h"



// 属性变化事件数据
USTRUCT(BlueprintType)
struct FTireflyAttributeChangeEventPayload
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

	// 变化来源记录
	UPROPERTY(BlueprintReadOnly)
	TMap<FName, float> ChangeSourceRecord;

public:
	FTireflyAttributeChangeEventPayload() {}

	FTireflyAttributeChangeEventPayload(
		FName InAttrName,
		float InNewVal,
		float InOldVal,
		const TMap<FName, float>& InChangeSourceRecord)
	{
		AttributeName = InAttrName;
		NewValue = InNewVal;
		OldValue = InOldVal;
		ChangeSourceRecord = InChangeSourceRecord;
	}
};
