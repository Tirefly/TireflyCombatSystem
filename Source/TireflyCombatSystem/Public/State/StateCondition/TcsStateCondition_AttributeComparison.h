// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "State/StateCondition/TcsStateCondition.h"
#include "TcsGenericEnum.h"
#include "TcsStateCondition_AttributeComparison.generated.h"



// 属性比较条件Payload
USTRUCT(BlueprintType)
struct FTcsStateConditionPayload_AttributeComparison
{
	GENERATED_BODY()

public:
	// 要比较的属性名称（FName 版本，权威 ID）
	// 如果 AttributeTag 有效，则优先使用 Tag 解析为 Name
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison",
		Meta = (GetOptions = "TcsGenericLibrary.GetAttributeNames"))
	FName AttributeName = NAME_None;

	// 要比较的属性标签（GameplayTag 版本，可选）
	// 如果有效，则优先使用此字段解析为 AttributeName
	// 推荐使用 Tag 以获得更好的层级语义和重构支持
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison",
		Meta = (Categories = "TCS.Attribute"))
	FGameplayTag AttributeTag;

	// 检查目标（Owner或Instigator）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	ETcsAttributeCheckTarget CheckTarget = ETcsAttributeCheckTarget::Owner;

	// 比较操作
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	ETcsNumericComparison ComparisonType = ETcsNumericComparison::Equal;

	// 比较值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	float CompareValue = 0.0f;
};



// 基于属性比较的状态条件
UCLASS(BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTcsStateCondition_AttributeComparison : public UTcsStateCondition
{
	GENERATED_BODY()

public:
	// 检查条件实现
	virtual bool CheckCondition_Implementation(
		UTcsStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime) override;
}; 