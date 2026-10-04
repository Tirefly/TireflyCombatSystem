// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Clock/TcsClockSubsystem.h"
#include "Def/TcsBuffDef.h"
#include "State/TcsStateEvents.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"


namespace
{
	// 撤销一个条目锚点（惰性取消：即时标无效，堆内残留条目出队时跳过）；
	// 撤销后置空，使"可重复调用"成立（世界拆解期与移除路径都会走到）
	void CancelEntry(UTcsStateSubsystem& Subsystem, FTcsTimeEntryHandle& Entry)
	{
		if (!Entry.IsValid())
		{
			return;
		}

		if (UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem())
		{
			Clock->CancelExpiry(Entry);
		}

		Entry = FTcsTimeEntryHandle();
	}
}



// 时间条目
void FTcsStateOps::PushExpiry(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance)
{
	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		// 世界拆解期（时钟泵已回收）——时序而非配置错误，故不留红字
		return;
	}

	// 剩余 0 也照常入堆（DueTime = 当前时刻）——它在**下一个泵点**到期，不早于本帧游戏逻辑，
	// 也不无限拖延。刻意不在本函数里同步回调：那会让"施加"在调用栈中途变成"移除"。
	const double DueTime = Clock->GetClock().Elapsed + FMath::Max(0.0, Instance.DurationRemaining);

	// 回调捕获**世界弱引用 + 句柄**：世界已亡或句柄悬空即静默丢弃
	TWeakObjectPtr<UTcsStateSubsystem> WeakSubsystem(&Subsystem);
	const FTcsStateHandle Handle = Instance.Handle;
	Instance.ExpiryEntry = Clock->PushExpiry(DueTime, Instance.Source.Id,
		[WeakSubsystem, Handle](uint64 /*OwnerId*/)
		{
			if (UTcsStateSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->ExpireState(Handle);
			}
		});
}

void FTcsStateOps::PushPeriod(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance)
{
	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		return;
	}

	const double DueTime = Clock->GetClock().Elapsed + FMath::Max(0.0, Instance.PeriodRemaining);

	TWeakObjectPtr<UTcsStateSubsystem> WeakSubsystem(&Subsystem);
	const FTcsStateHandle Handle = Instance.Handle;

	// 重复到期条目：回调广播 `Periodic` 后**重新入堆**（条目是一次性的）
	Instance.PeriodEntry = Clock->PushExpiry(DueTime, Instance.Source.Id,
		[WeakSubsystem, Handle](uint64 /*OwnerId*/)
		{
			UTcsStateSubsystem* Subsystem = WeakSubsystem.Get();
			if (!Subsystem)
			{
				return;
			}

			// 先复位锚点：条目已一次性失效，若下面中途 return 也不留悬空句柄
			FTcsStateInstance* Instance = const_cast<FTcsStateInstance*>(FTcsStateOps::Find(*Subsystem, Handle));
			if (!Instance)
			{
				// 代际失配 / 实例已移除 = 时序竞态：静默丢弃（条目本该已被移除路径撤销）
				return;
			}

			Instance->PeriodEntry = FTcsTimeEntryHandle();
			Instance->PeriodRemaining = 0.0;

			FTcsStateOps::Broadcast(*Subsystem, Tag_TcsEvent_State_Periodic, *Instance, EStateRemoveCause::ESRC_Removed);

			// 广播会让订阅者增删状态 ⇒ 实例指针不可跨广播使用，重新取一次
			FTcsStateInstance* AfterBroadcast = const_cast<FTcsStateInstance*>(FTcsStateOps::Find(*Subsystem, Handle));
			if (!AfterBroadcast)
			{
				return;
			}

			const FTcsBuffDef* Def = FTcsStateOps::GetDef(*Subsystem, AfterBroadcast->DefTag);
			if (!Def || Def->Period <= 0.0)
			{
				return;
			}

			AfterBroadcast->PeriodRemaining = Def->Period;
			FTcsStateOps::PushPeriod(*Subsystem, *AfterBroadcast);

			UE_LOG(LogTcsState, Log, TEXT("状态周期到点：句柄=%d/%d 定义=%s 层数=%d 周期=%.3f"),
				Handle.Index, Handle.Generation, *AfterBroadcast->DefTag.ToString(),
				AfterBroadcast->Stacks, Def->Period);
		});
}

void FTcsStateOps::CancelTimeEntries(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance)
{
	CancelEntry(Subsystem, Instance.ExpiryEntry);
	CancelEntry(Subsystem, Instance.PeriodEntry);
	Instance.PeriodRemaining = 0.0;
}



// 时间调度
void FTcsStateOps::ScheduleTime(
	UTcsStateSubsystem& Subsystem,
	FTcsStateInstance& Instance,
	const FTcsBuffDef& Def,
	const FTcsParamSnapshot& Snapshot,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	bool bRefresh)
{
	// 时值：先撤旧条目（刷新路径上旧条目还在堆里）
	CancelEntry(Subsystem, Instance.ExpiryEntry);

	if (Def.DurationPolicy == EDurationPolicy::EDP_Infinite)
	{
		// 无限时值：不入堆、字段不使用（"≤0 = 永久"的魔法值语义不成立）
		Instance.DurationRemaining = 0.0;
	}
	else
	{
		Instance.DurationRemaining = EvaluateTotalDuration(
			Subsystem, Def, Snapshot, Instance.Unit, Instance.Instigator, ParamTable);
		PushExpiry(Subsystem, Instance);
	}

	// 周期：撤旧条目后按 `PeriodRefresh` 重建
	CancelEntry(Subsystem, Instance.PeriodEntry);

	if (Def.Period <= 0.0)
	{
		Instance.PeriodRemaining = 0.0;
		return;
	}

	// `EPR_Immediate` 只在刷新路径上成立，且**同步执行一次**（不是"排一个 0 延迟条目"）：
	// "立即"的语义就是本次调用内发生，排 0 延迟会把它变成"下一泵点"（与语义不符）
	const bool bImmediate = bRefresh && Def.PeriodRefresh == ETcsPeriodRefresh::EPR_Immediate;
	const bool bKeepRemaining = bRefresh
		&& Def.PeriodRefresh == ETcsPeriodRefresh::EPR_Keep
		&& Instance.PeriodRemaining > 0.0;

	// 先定下一次的计时（Immediate 的那一次已经"发生"了，故满额重建）
	Instance.PeriodRemaining = bKeepRemaining ? Instance.PeriodRemaining : Def.Period;

	if (bImmediate)
	{
		Broadcast(Subsystem, Tag_TcsEvent_State_Periodic, Instance, EStateRemoveCause::ESRC_Removed);
	}

	PushPeriod(Subsystem, Instance);
}



// 生命周期操作
void FTcsStateOps::RescheduleExpiry(UTcsStateSubsystem& Subsystem, FTcsStateInstance& Instance)
{
	// 句柄配对清理：旧条目先撤，再按新余量重入堆（"延长 / 缩短"都走这一条）
	CancelEntry(Subsystem, Instance.ExpiryEntry);
	PushExpiry(Subsystem, Instance);
}
