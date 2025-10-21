// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TcsAttribute.generated.h"



// 属性范围类型
UENUM(BlueprintType)
enum class ETcsAttributeRangeType : uint8
{
	ART_None  = 0		UMETA(DisplayName = "无", ToolTip = "属性值范围的一侧（最小值或最大值）没有限制"),
	ART_Static = 1		UMETA(DisplayName = "静态", ToolTip = "属性值范围的一侧（最小值或最大值）是一个恒定的数值"),
	ART_Dynamic = 2		UMETA(DisplayName = "动态", ToolTip = "属性值范围的一侧（最小值或最大值）是动态的，受另一个属性值的影响"),
};



// 属性范围
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeRange
{
	GENERATED_BODY()

#pragma region MinValue
	
public:
	// 最小值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	ETcsAttributeRangeType MinValueType = ETcsAttributeRangeType::ART_None;

	// 最小值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETcsAttributeRangeType::ART_Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	float MinValue = 0.f;

	// 最小值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MinValueType == ETcsAttributeRangeType::ART_Dynamic",  EditConditionHides,
		GetOptions = "TcsGenericLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Min Value")
	FName MinValueAttribute = NAME_None;

#pragma endregion


#pragma region MaxValue
	
public:
	// 最大值类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	ETcsAttributeRangeType MaxValueType = ETcsAttributeRangeType::ART_None;

	// 最大值（静态：常数）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETcsAttributeRangeType::ART_Static",  EditConditionHides),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	float MaxValue = 0.f;

	// 最大值（动态：属性）
	UPROPERTY(Meta = (EditCondition = "MaxValueType == ETcsAttributeRangeType::ART_Dynamic",  EditConditionHides,
		GetOptions = "TcsGenericLibrary.GetAttributeNames"),
		EditAnywhere, BlueprintReadOnly, Category = "Max Value")
	FName MaxValueAttribute = NAME_None;

#pragma endregion
};



// 属性定义表
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 属性数值范围
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FTcsAttributeRange AttributeRange;

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
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeInstance
{
	GENERATED_BODY()

public:
	// 属性定义
	UPROPERTY(BlueprintReadOnly)
	FTcsAttributeDefinition AttributeDef;

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

	FTcsAttributeInstance(const FTcsAttributeDefinition& AttrDef, int32 InstId, AActor* Owner)
		: AttributeDef(AttrDef), AttributeInstId(InstId), Owner(Owner), BaseValue(0.f), CurrentValue(0.f)
	{}

	FTcsAttributeInstance(const FTcsAttributeDefinition& AttrDef, int32 InstId, AActor* Owner, float InitValue)
		: AttributeDef(AttrDef), AttributeInstId(InstId), Owner(Owner), BaseValue(InitValue), CurrentValue(InitValue)
	{}
};
