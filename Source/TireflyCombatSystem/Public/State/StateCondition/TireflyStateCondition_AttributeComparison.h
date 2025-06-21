// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "State/TireflyStateCondition.h"
#include "TireflyCombatSystemEnum.h"
#include "TireflyStateCondition_AttributeComparison.generated.h"



// 属性比较条件Payload
USTRUCT(BlueprintType)
struct FTireflyStateConditionPayload_AttributeComparison
{
	GENERATED_BODY()

public:
	// 要比较的属性名称
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	FName AttributeName = NAME_None;

	// 检查目标（Owner或Instigator）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	ETireflyAttributeCheckTarget CheckTarget = ETireflyAttributeCheckTarget::Owner;

	// 比较操作
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	ETireflyAttributeComparisonType ComparisonType = ETireflyAttributeComparisonType::Equal;

	// 比较值
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attribute Comparison")
	float CompareValue = 0.0f;
};



// 基于属性比较的状态条件
UCLASS(BlueprintType, Blueprintable, ClassGroup = (TireflyCombatSystem))
class TIREFLYCOMBATSYSTEM_API UTireflyStateCondition_AttributeComparison : public UTireflyStateCondition
{
	GENERATED_BODY()

public:
	// 检查条件实现
	virtual bool CheckCondition_Implementation(
		UTireflyStateInstance* StateInstance,
		const FInstancedStruct& Payload,
		float CurrentGameTime) override;
}; 