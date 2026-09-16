// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"

/**
 * 可注入时间源接口（D0-5）：战斗时钟的唯一步长供给口。
 * 抽象 C++ 接口（非反射）——时间源是引擎管道设施而非 Def 配置数据（PV-1 反射要求只约束参数上下文）；
 * 回合制宿主可注入"回合即时间"源替换（UTcsClockSubsystem::SetTimeSource）。
 *
 * 纯函数性纪律（D0-1）：同帧同输入同结果——源内禁随机/禁 wall-clock
 * （FDateTime::UtcNow / FPlatformTime——review 检查点）。
 */
class ITcsTimeSource
{
public:
	virtual ~ITcsTimeSource() = default;

	/**
	 * 取本帧步长。
	 *
	 * @param World 泵所属游戏世界（只读上下文，禁写入）。
	 * @param RawDeltaSeconds 本帧原始增量（暂停帧由泵兜 0）。
	 * @return 缩放后的战斗步长（ScaledDt）。
	 */
	virtual double GetDeltaSeconds(const UWorld& World, double RawDeltaSeconds) const = 0;
};



/**
 * 默认时间源：原始帧增量 × TimeDilation（WorldSettings->TimeDilation——slomo 改写目标）。
 * 无 WorldSettings 的世界按 1.0 兜底。
 */
struct FTcsTimeSource_Default : public ITcsTimeSource
{
	virtual double GetDeltaSeconds(const UWorld& World, double RawDeltaSeconds) const override
	{
		const AWorldSettings* WorldSettings = World.GetWorldSettings();
		const float TimeDilation = WorldSettings ? WorldSettings->TimeDilation : 1.0f;
		return RawDeltaSeconds * static_cast<double>(TimeDilation);
	}
};
