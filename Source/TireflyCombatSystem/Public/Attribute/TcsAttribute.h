// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
	FTcsAttributeRange AttributeRange;

	// Clamp 策略类（默认使用线性 Clamp 策略）
	// 用于定义属性值的约束方式，可以选择内置策略或自定义策略
	// 默认值在构造函数中设置为 UTcsAttrClampStrategy_Linear::StaticClass()
	// 示例：可以实现循环 Clamp（角度）、阶梯 Clamp（整数等级）等自定义策略
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range",
		Meta = (ToolTip = "属性值的约束策略。默认使用线性约束（FMath::Clamp）。可以选择其他内置策略或自定义策略（C++ 或蓝图）。"))
	TSubclassOf<class UTcsAttributeClampStrategy> ClampStrategyClass;

	// Clamp 策略配置（可选，使用 FInstancedStruct 存储）
	// 用户可以定义任意结构体作为配置，不需要继承任何父类
	// 只有当策略需要额外配置时才使用此字段
	// 示例：条件 Clamp 可以配置触发条件，阶梯 Clamp 可以配置阶梯值列表
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range",
		Meta = (ToolTip = "Clamp 策略的配置（可选）。可以是任意用户定义的结构体，不需要继承父类。"))
	FInstancedStruct ClampStrategyConfig;

	// 属性类别
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeCategory = FString("");

	// 属性缩写（用于公式）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	FString AttributeAbbreviation = FString("");

	// 属性的语义标识（可选，但推荐）
	// 用于父子 Tag 匹配、分类筛选、跨系统对齐
	// 仍然以 Attribute DataTable RowName(FName) 作为权威唯一 ID
	// 推荐命名约定：TCS.Attribute.<RowName>
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta", Meta = (Categories = "TCS.Attribute"))
	FGameplayTag AttributeTag;

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

public:
	// 构造函数：设置默认 Clamp 策略
	FTcsAttributeDefinition();
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
