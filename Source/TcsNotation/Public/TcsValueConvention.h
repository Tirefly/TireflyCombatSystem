// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TcsValueConvention.generated.h"



// 值约定标志位（EnumFlags；枚举值前缀 VCF_ 是 ValueConventionFlag 的缩写；固定组合顺序 Percent→OneMinus→Negate）
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETcsValueConventionFlag : uint8
{
	VCF_None = 0			UMETA(DisplayName = "无", ToolTip = "不做任何值约定变换，原值即规范值（默认，值 0）"),
	VCF_Percent = 1 << 0	UMETA(DisplayName = "百分比", ToolTip = "策划语言值除以 100 写入规范值（策划写 85 → 规范 0.85）"),
	VCF_OneMinus = 1 << 1	UMETA(DisplayName = "一减", ToolTip = "在当前值基础上取 1−v，与百分比组合（策划写 25 减少25% → 规范 0.75）"),
	VCF_Negate = 1 << 2		UMETA(DisplayName = "取负", ToolTip = "对当前值取负（策划写 1000 减少1000生命 → 规范 -1000）"),
};



/**
 * 值约定转换助手（D5-18）：策划语言值 ↔ 规范值的通用词汇。
 * 变换改写逻辑消费的值本身（不只是显示格式）；无状态静态助手，转换只发生在物化/注册边界。
 */
struct TCSNOTATION_API FTcsValueConvention
{
	/**
	 * 将策划书写的原始值转换为运行时消费的规范值（写入点调用）。
	 * 固定组合顺序：Percent（÷100）→ OneMinus（1−v）→ Negate（取负）。
	 *
	 * @param RawValue 策划书写的原始值。
	 * @param ConventionFlags 值约定标志位掩码（ETcsValueConventionFlag 组合）。
	 * @return 返回变换后的规范值。
	 */
	static FORCEINLINE double ConvertToCanonical(double RawValue, int32 ConventionFlags)
	{
		double CanonicalValue = RawValue;

		if (ConventionFlags & static_cast<int32>(ETcsValueConventionFlag::VCF_Percent))
		{
			CanonicalValue /= 100.0;
		}

		if (ConventionFlags & static_cast<int32>(ETcsValueConventionFlag::VCF_OneMinus))
		{
			CanonicalValue = 1.0 - CanonicalValue;
		}

		if (ConventionFlags & static_cast<int32>(ETcsValueConventionFlag::VCF_Negate))
		{
			CanonicalValue = -CanonicalValue;
		}

		return CanonicalValue;
	}
};
