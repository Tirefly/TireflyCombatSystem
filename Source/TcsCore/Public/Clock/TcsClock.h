// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 战斗时钟（D0-1/D0-5）：全插件唯一"取时间"入口。
 * 时间只经时钟泵推进（帧号 +1、累计 ScaledDt、记录本帧步长）——封装外无旁路取时；
 * 确定性纪律：封装内禁 wall-clock（FDateTime::UtcNow / FPlatformTime——review 检查点）。
 */
struct FTcsClock
{
	// 帧号（泵推进一次 +1）
	uint64 Frame = 0;

	// 累计 ScaledDt（战斗时间轴上的绝对时刻——到期堆 DueTime 的基准时轴）
	double Elapsed = 0.0;

	// 本帧步长（上次泵推进的 ScaledDt）
	double DeltaSeconds = 0.0;
};
