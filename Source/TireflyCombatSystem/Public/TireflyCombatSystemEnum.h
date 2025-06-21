// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TireflyCombatSystemEnum.generated.h"



// 数值比较操作
UENUM(BlueprintType)
enum class ETireflyNumericComparison : uint8
{
	Equal = 0			UMETA(DisplayName = "Equal", ToolTip = "等于"),
	NotEqual			UMETA(DisplayName = "Not Equal", ToolTip = "不等于"),
	GreaterThan			UMETA(DisplayName = "Greater Than", ToolTip = "大于"),
	GreaterThanOrEqual	UMETA(DisplayName = "Greater Than Or Equal", ToolTip = "大于等于"),
	LessThan			UMETA(DisplayName = "Less Than", ToolTip = "小于"),
	LessThanOrEqual		UMETA(DisplayName = "Less Than Or Equal", ToolTip = "小于等于"),
};

// 属性比较操作
UENUM(BlueprintType)
enum class ETireflyAttributeComparisonType : uint8
{
	Equal = 0			UMETA(DisplayName = "Equal", ToolTip = "等于"),
	NotEqual			UMETA(DisplayName = "Not Equal", ToolTip = "不等于"),
	GreaterThan			UMETA(DisplayName = "Greater Than", ToolTip = "大于"),
	GreaterThanOrEqual	UMETA(DisplayName = "Greater Than Or Equal", ToolTip = "大于等于"),
	LessThan			UMETA(DisplayName = "Less Than", ToolTip = "小于"),
	LessThanOrEqual		UMETA(DisplayName = "Less Than Or Equal", ToolTip = "小于等于"),
};

// 属性检查目标
UENUM(BlueprintType)
enum class ETireflyAttributeCheckTarget : uint8
{
	Owner = 0			UMETA(DisplayName = "Owner", ToolTip = "状态作用者"),
	Instigator = 1		UMETA(DisplayName = "Instigator", ToolTip = "状态发起者"),
}; 