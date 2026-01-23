// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
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
