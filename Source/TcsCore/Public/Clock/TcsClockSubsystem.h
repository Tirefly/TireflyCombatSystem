// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"

#include "Clock/TcsClock.h"
#include "Clock/TcsExpiryHeap.h"
#include "Clock/TcsTimeSource.h"

#include "TcsClockSubsystem.generated.h"



/**
 * 时钟泵门面（M0 §2.3）：世界级子系统，持有战斗时钟与到期堆，按固定泵序驱动帧界推进。
 *
 * 泵点 = FWorldDelegates::OnWorldTickStart（UWorld::Tick 头部广播，早于全部 Actor tick 组——PrePhysics 语义）；
 * 引擎事实（2026-09-16 源码核实）：Tickable 自 tick 位于 UWorld::Tick 尾部（晚于 tick 组与 TimerManager），
 * 无法承担泵点——故本子系统自 tick 全程停用（GetTickableTickType = Never），泵全部由委托钩子驱动。
 *
 * 固定泵序：①时间源取步长（暂停帧 0）→ ②FTcsClock 推进 → ③总线帧末队列冲洗 → ④到期堆 AdvanceTo。
 * 本帧游戏逻辑期间入队的帧末事件于下一帧泵点派发（"帧界"语义——事件总线冲洗驱动权自 Task 3 起归本泵）。
 */
UCLASS()
class TCSCORE_API UTcsClockSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

// 生命期
#pragma region Lifetime

public:
	// 世界类型过滤：仅游戏世界（Game/PIE/GamePreview）实例化——时钟泵是运行时设施
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

	// 初始化：兜底默认时间源 + 绑定帧界泵点委托
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	// 反初始化：解绑委托 + 确定性清空到期堆与时间源；必须转发 Super 以正确停用 tick
	virtual void Deinitialize() override;

#pragma endregion


// 泵
#pragma region Pump

public:
	// 自 tick 全程停用（泵由 OnWorldTickStart 驱动——Tickable 自 tick 在 UWorld::Tick 尾部，晚于 tick 组）
	virtual ETickableTickType GetTickableTickType() const override
	{
		return ETickableTickType::Never;
	}

	// Tick 统计标识（基类纯虚——自 tick 停用后仍需提供）
	virtual TStatId GetStatId() const override;

#pragma endregion


// 时间源
#pragma region TimeSource

public:
	/**
	 * 注入自定义时间源（回合制宿主"回合即时间"等；空指针拒绝——保留当前源）。
	 *
	 * @param InTimeSource 自定义时间源（移动接管）。
	 */
	void SetTimeSource(TUniquePtr<ITcsTimeSource> InTimeSource);

#pragma endregion


// 取时与到期
#pragma region Access

public:
	// 唯一取时入口（帧号 / 累计 ScaledDt / 本帧步长）
	const FTcsClock& GetClock() const
	{
		return Clock;
	}

	// 入堆到期条目（转发内部到期堆；DueTime 为 Elapsed 时轴绝对时刻——消费者经本入口入堆）
	FTcsTimeEntryHandle PushExpiry(double DueTime, uint64 OwnerId, TFunction<void(uint64)> OnDue)
	{
		return ExpiryHeap.Push(DueTime, OwnerId, MoveTemp(OnDue));
	}

	// 取消到期条目（转发内部到期堆；惰性取消——句柄配对清理）
	void CancelExpiry(FTcsTimeEntryHandle Handle)
	{
		ExpiryHeap.Cancel(Handle);
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 帧界泵点（委托是进程级广播——世界标识过滤，只泵本子系统所属世界）
	void HandleWorldTickStart(UWorld* World, ELevelTick TickType, float DeltaSeconds);

	// 战斗时钟（唯一取时入口）
	FTcsClock Clock;

	// 到期堆（M3 到期 / M4 定时步骤 / M5 冷却的消费底座；AdvanceTo 保持泵私有）
	FTcsExpiryHeap ExpiryHeap;

	// 时间源（默认 FTcsTimeSource_Default；宿主可注入替换）
	TUniquePtr<ITcsTimeSource> TimeSource;

#pragma endregion
};
