// Copyright Tirefly. All Rights Reserved.

#include "Clock/UTcsClockSubsystem.h"

#include "EventBus/UTcsEventBusSubsystem.h"
#include "Stats/Stats.h"

bool UTcsClockSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界实例化：编辑器预览/检查器世界不创建（对齐总线门面）
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsClockSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 兜底默认时间源（宿主可在其后注入替换）
	if (!TimeSource.IsValid())
	{
		TimeSource = MakeUnique<FTcsTimeSource_Default>();
	}

	// 绑定帧界泵点（UWorld::Tick 头部广播——早于全部 Actor tick 组；世界标识过滤在处理函数内）
	FWorldDelegates::OnWorldTickStart.AddUObject(this, &UTcsClockSubsystem::HandleWorldTickStart);
}

void UTcsClockSubsystem::Deinitialize()
{
	// 解绑委托 + 确定性清理：到期回调捕获随 Reset 释放，时间源随子系统销毁
	FWorldDelegates::OnWorldTickStart.RemoveAll(this);
	ExpiryHeap.Reset();
	TimeSource.Reset();

	Super::Deinitialize();
}

TStatId UTcsClockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UTcsClockSubsystem, STATGROUP_Tickables);
}

void UTcsClockSubsystem::SetTimeSource(TUniquePtr<ITcsTimeSource> InTimeSource)
{
	ensureMsgf(InTimeSource.IsValid(), TEXT("UTcsClockSubsystem::SetTimeSource: 空时间源被拒绝（保留当前源）"));
	if (InTimeSource.IsValid())
	{
		TimeSource = MoveTemp(InTimeSource);
	}
}

void UTcsClockSubsystem::HandleWorldTickStart(UWorld* World, ELevelTick /*TickType*/, float DeltaSeconds)
{
	// 只泵本子系统所属世界（委托是进程级广播，PIE 多世界各自过滤）
	if (World != GetWorld())
	{
		return;
	}

	// ① 时间源取步长（暂停帧兜 0——slomo 经 TimeDilation 落到 ScaledDt、暂停经 IsPaused 冻结，检查点 7）
	const double RawDeltaSeconds = World->IsPaused() ? 0.0 : static_cast<double>(DeltaSeconds);
	const double ScaledDeltaSeconds = TimeSource.IsValid()
		? TimeSource->GetDeltaSeconds(*World, RawDeltaSeconds)
		: RawDeltaSeconds;

	// ② 时钟推进（唯一取时入口的值更新点）
	++Clock.Frame;
	Clock.DeltaSeconds = ScaledDeltaSeconds;
	Clock.Elapsed += ScaledDeltaSeconds;

	UE_LOG(LogTcsCore, Verbose, TEXT("UTcsClockSubsystem: 泵推进 Frame=%llu DeltaSeconds=%.6f Elapsed=%.6f"),
		Clock.Frame, Clock.DeltaSeconds, Clock.Elapsed);

	// ③ 总线帧末队列冲洗（固定泵序：时钟推进后、到期堆前——本帧游戏逻辑入队的事件于下一帧泵点派发）
	if (UTcsEventBusSubsystem* BusSubsystem = World->GetSubsystem<UTcsEventBusSubsystem>())
	{
		BusSubsystem->FlushFrameEndQueue();
	}

	// ④ 到期堆推进（到期回调内新入堆归下一拍——堆内核纪律）
	ExpiryHeap.AdvanceTo(Clock.Elapsed);
}
