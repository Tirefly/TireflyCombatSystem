// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TireflyAttribute.generated.h"



// 属性范围类型
UENUM(BlueprintType)
enum class ETireflyAttributeRangeType : uint8
{
	None  = 0		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute has no limit."),
	Static = 1		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is a constant numeric value."),
	Dynamic = 2		UMETA(ToolTip = "One side (minimum or maximum) of the value range for Attribute is dynamic, which is affected by another attribute value."),
};



// 属性范围
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeRange
{
	GENERATED_BODY()

#pragma region MinValue
	
public:
	// 最小值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	ETireflyAttributeRangeType MinValueType = ETireflyAttributeRangeType::None;

	// 最小值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETireflyAttributeRangeType::Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	float MinValue = 0.f;

	// 最小值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETireflyAttributeRangeType::Dynamic",  EditConditionHides,
		GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	FName MinValueAttribute = NAME_None;

#pragma endregion


#pragma region MaxValue
	
public:
	// 最大值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	ETireflyAttributeRangeType MaxValueType = ETireflyAttributeRangeType::None;

	// 最大值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETireflyAttributeRangeType::Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	float MaxValue = 0.f;

	// 最大值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETireflyAttributeRangeType::Dynamic",  EditConditionHides,
		GetOptions = "TireflyCombatSystemLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	FName MaxValueAttribute = NAME_None;

#pragma endregion
};



// 属性定义表
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 属性数值范围
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTireflyAttributeRange AttributeRange;

	// 属性类别
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeCategory = FString("");

	// 属性缩写（用于公式）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeAbbreviation = FString("");

	// 属性名（最好使用 StringTable）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText AttributeName;

	// 属性描述（最好使用 StringTable）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display")
	FText AttributeDescription;

	// 是否在UI中显示
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bShowInUI = true;

	// 属性图标
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	TSoftObjectPtr<UTexture2D> Icon;

	// 是否显示为小数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bAsDecimal = true;

	// 是否显示为百分比
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
	bool bAsPercentage = false;
};



// 属性实例
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyAttributeInstance
{
	GENERATED_BODY()

public:
	// 属性定义
	UPROPERTY(BlueprintReadOnly)
	FTireflyAttributeDefinition AttributeDef;

	// 属性实例Id
	UPROPERTY(BlueprintReadOnly)
	int32 AttributeInstId = -1;

	// 基础值
	UPROPERTY(BlueprintReadOnly)
	float BaseValue = 0.0f;

	// 属性值
	UPROPERTY(BlueprintReadOnly)
	float CurrentValue = 0.0f;

	//  属性拥有者
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Owner;
};
