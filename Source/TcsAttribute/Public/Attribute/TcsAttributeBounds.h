// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"

#include "TcsAttributeBounds.generated.h"



/**
 * 边界模式（D2-4 三态）：Min 与 Max 各自独立取态（如 Min 静态 0 配 Max 动态 MaxHealth）。
 * 无 Custom 位——自定义值域语义走值域模式（`ETcsAttributeValueDomain`，同文件末），两者职责不重叠。
 * 枚举值前缀 ABM_ 是 AttributeBoundMode 的缩写。
 */
UENUM(BlueprintType)
enum class ETcsAttributeBoundMode : uint8
{
	ABM_None = 0	UMETA(DisplayName = "无边界", ToolTip = "该侧不设边界（默认，值 0）"),
	ABM_Static = 1	UMETA(DisplayName = "静态", ToolTip = "该侧边界为恒定数值（StaticValue）"),
	ABM_Dynamic = 2	UMETA(DisplayName = "动态", ToolTip = "该侧边界取自另一属性的当前值（先按聚合管线求值——HP ≤ MaxHP 形态）；自引用禁止"),
};



/**
 * 单侧边界（Min/Max 各持一个）。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeBound
{
	GENERATED_BODY()

// 模式
#pragma region Mode

public:
	// 边界模式（三态）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bound")
	ETcsAttributeBoundMode Mode = ETcsAttributeBoundMode::ABM_None;

#pragma endregion


// 静态边界
#pragma region StaticValue

public:
	// 静态边界值（ABM_Static 时生效）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bound",
		Meta = (EditCondition = "Mode == ETcsAttributeBoundMode::ABM_Static", EditConditionHides))
	double StaticValue = 0.0;

#pragma endregion


// 动态边界
#pragma region DynamicAttribute

public:
	/**
	 * 动态边界属性（ABM_Dynamic 时生效）：先按聚合管线求该属性的当前值，再作本属性的边界。
	 * 自引用禁止（本属性以自身为边界 = 循环依赖）——属性添加期 ensure 拒绝。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bound",
		Meta = (EditCondition = "Mode == ETcsAttributeBoundMode::ABM_Dynamic", EditConditionHides))
	FGameplayTag DynamicAttribute;

#pragma endregion
};



/**
 * 值域边界（D2-4）：末端统一收口的 Min/Max 两端。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeBounds
{
	GENERATED_BODY()

// 下边界
#pragma region Min

public:
	// 下边界
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FTcsAttributeBound Min;

#pragma endregion


// 上边界
#pragma region Max

public:
	// 上边界
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bounds")
	FTcsAttributeBound Max;

#pragma endregion
};



/**
 * 值域模式（D2-6）：末端收口的语义选择——与上面的边界三态合起来构成"值域"的两面
 * （边界给范围、模式给越界语义），故同住本文件。
 * Custom 固定值 1（全插件"逃逸位取 1"规约），语义 = "自定义值域策略只接管值域函数，
 * 时序/级联/事务仍由引擎守护"——值域策略接口（`IValueDomainPolicy`）不在 R3 范围，
 * 使用 Custom 时的收口行为归聚合管线任务并有显式提示；策略接口落地时按需另立文件。
 * 枚举值前缀 AVD_ 是 AttributeValueDomain 的缩写。
 */
UENUM(BlueprintType)
enum class ETcsAttributeValueDomain : uint8
{
	AVD_Clamp = 0	UMETA(DisplayName = "钳制", ToolTip = "越界钳到边界值——默认，值 0"),
	AVD_Custom = 1	UMETA(DisplayName = "自定义", ToolTip = "逃逸位（固定值 1）：自定义值域策略只接管值域函数，时序/级联/事务仍由引擎守护"),
	AVD_Wrap = 2	UMETA(DisplayName = "循环", ToolTip = "越界按值域跨度循环回卷"),
};
