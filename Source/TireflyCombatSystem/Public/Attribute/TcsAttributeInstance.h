// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsAttributeInstance.generated.h"


class UTcsAttributeDefinition;



// 属性实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeInstance
{
	GENERATED_BODY()

public:
	// 属性定义 DataAsset 硬引用
	UPROPERTY(BlueprintReadOnly)
	const UTcsAttributeDefinition* AttributeDef = nullptr;

	// 属性定义 ID（冗余字段，用于快速查询和调试）
	UPROPERTY(BlueprintReadOnly)
	FName AttributeDefId = NAME_None;

	// 属性实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 AttributeInstId = -1;

	//  属性拥有者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Owner;

	// 基础值
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.0f;

	// 属性值
	UPROPERTY(BlueprintReadOnly)
	float CurrentValue = 0.0f;

public:
	FTcsAttributeInstance() {}

	FTcsAttributeInstance(
		const UTcsAttributeDefinition* InAttrDefAsset,
		FName InAttrDefId,
		int32 InstId,
		AActor* InOwner)
		: AttributeDef(InAttrDefAsset)
		, AttributeDefId(InAttrDefId)
		, AttributeInstId(InstId)
		, Owner(InOwner)
		, BaseValue(0.f)
		, CurrentValue(0.f)
	{}
};
