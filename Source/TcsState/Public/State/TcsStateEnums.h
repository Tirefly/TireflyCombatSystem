// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsStateEnums.generated.h"



// 时值策略（D3-13）：把"时长 ≤0 即永久"的魔法值拆成显式两态
// 枚举值前缀 EDP_ 是 DurationPolicy 的缩写
UENUM(BlueprintType)
enum class EDurationPolicy : uint8
{
	EDP_Finite = 0		UMETA(DisplayName = "有限", ToolTip = "按 DurationTime 到期：注册到期条目，到点发 Expired"),
	EDP_Infinite = 1	UMETA(DisplayName = "无限", ToolTip = "无到期条目、永不过期——由 Remove / Cancel 显式结束"),
};



// 周期刷新策略（D3-13）：堆叠刷新对周期计时器的作用
// 枚举值前缀 EPR_ 是 PeriodRefresh 的缩写
UENUM(BlueprintType)
enum class ETcsPeriodRefresh : uint8
{
	EPR_Keep = 0		UMETA(DisplayName = "保持", ToolTip = "刷新不动周期计时（默认）——下一个周期仍按原剩余时间到点"),
	EPR_Reset = 1		UMETA(DisplayName = "重置", ToolTip = "刷新时把周期计时重置为满额（下一个周期从零开始计）"),
	EPR_Immediate = 2	UMETA(DisplayName = "立即", ToolTip = "刷新时立刻执行一次周期效果并重置计时"),
};
