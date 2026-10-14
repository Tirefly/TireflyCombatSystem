// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffOps.h"

#include "Clock/TcsClockSubsystem.h"
#include "Def/TcsBuffDef.h"
#include "State/TcsBuffEvents.h"
#include "TcsBuffLogChannel.h"
#include "TcsBuffSubsystem.h"


namespace
{
	// 撤销一个条目锚点（惰性取消：即时标无效，堆内残留条目出队时跳过）；
	// 撤销后置空，使"可重复调用"成立（世界拆解期与移除路径都会走到）
	void CancelEntry(UTcsBuffSubsystem& Subsystem, FTcsTimeEntryHandle& Entry)
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
void FTcsBuffOps::PushExpiry(UTcsBuffSubsystem& Subsystem, FTcsBuffInstance& Instance)
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
	TWeakObjectPtr<UTcsBuffSubsystem> WeakSubsystem(&Subsystem);
	const FTcsBuffInstanceHandle Handle = Instance.Handle;
	Instance.ExpiryEntry = Clock->PushExpiry(DueTime, Instance.Source.Id,
		[WeakSubsystem, Handle](uint64 /*OwnerId*/)
		{
			if (UTcsBuffSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->ExpireState(Handle);
			}
		});
}

void FTcsBuffOps::PushPeriod(UTcsBuffSubsystem& Subsystem, FTcsBuffInstance& Instance)
{
	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		return;
	}

	const double DueTime = Clock->GetClock().Elapsed + FMath::Max(0.0, Instance.PeriodRemaining);

	TWeakObjectPtr<UTcsBuffSubsystem> WeakSubsystem(&Subsystem);
	const FTcsBuffInstanceHandle Handle = Instance.Handle;

	// 重复到期条目：回调广播 `Periodic` 后**重新入堆**（条目是一次性的）
	Instance.PeriodEntry = Clock->PushExpiry(DueTime, Instance.Source.Id,
		[WeakSubsystem, Handle](uint64 /*OwnerId*/)
		{
			UTcsBuffSubsystem* Subsystem = WeakSubsystem.Get();
			if (!Subsystem)
			{
				return;
			}

			// 先复位锚点：条目已一次性失效，若下面中途 return 也不留悬空句柄
			FTcsBuffInstance* Instance = const_cast<FTcsBuffInstance*>(FTcsBuffOps::Find(*Subsystem, Handle));
			if (!Instance)
			{
				// 代际失配 / 实例已移除 = 时序竞态：静默丢弃（条目本该已被移除路径撤销）
				return;
			}

			Instance->PeriodEntry = FTcsTimeEntryHandle();
			Instance->PeriodRemaining = 0.0;

			const FTcsBuffInstance PeriodSnapshot = *Instance;
			FTcsBuffOps::Broadcast(*Subsystem, Tag_TcsEvent_State_Periodic, PeriodSnapshot, EBuffRemoveCause::EBRC_Removed);

			// 广播会让订阅者增删状态 ⇒ 实例指针不可跨广播使用，重新取一次
			FTcsBuffInstance* AfterBroadcast = const_cast<FTcsBuffInstance*>(FTcsBuffOps::Find(*Subsystem, Handle));
			if (!AfterBroadcast || AfterBroadcast->PeriodEntry.IsValid())
			{
				// 自移除时不重挂；回调已刷新重挂时不重复挂第二枚条目。
				return;
			}

			const FTcsBuffDef* Def = FTcsBuffOps::GetDef(*Subsystem, AfterBroadcast->DefTag);
			if (!Def || Def->Period <= 0.0)
			{
				return;
			}

			AfterBroadcast->PeriodRemaining = Def->Period;
			FTcsBuffOps::PushPeriod(*Subsystem, *AfterBroadcast);

			UE_LOG(LogTcsBuff, Log, TEXT("状态周期到点：句柄=%d/%d 定义=%s 层数=%d 周期=%.3f"),
				Handle.Index, Handle.Generation, *AfterBroadcast->DefTag.ToString(),
				AfterBroadcast->Stacks, Def->Period);
		});
}

void FTcsBuffOps::CancelTimeEntries(UTcsBuffSubsystem& Subsystem, FTcsBuffInstance& Instance)
{
	CancelEntry(Subsystem, Instance.ExpiryEntry);
	CancelEntry(Subsystem, Instance.PeriodEntry);
	Instance.PeriodRemaining = 0.0;
}



// 时间调度
void FTcsBuffOps::ScheduleTime(
	UTcsBuffSubsystem& Subsystem,
	FTcsBuffInstance& Instance,
	const FTcsBuffDef& Def,
	const FTcsParamSnapshot& Snapshot,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	bool bRefresh)
{
	// 时值：先撤旧条目（刷新路径上旧条目还在堆里）
	CancelEntry(Subsystem, Instance.ExpiryEntry);

	if (Def.DurationPolicy == EBuffDurationPolicy::EBDP_Infinite)
	{
		// 无限时值：不入堆、字段不使用（"≤0 = 永久"的魔法值语义不成立）
		Instance.DurationRemaining = 0.0;
	}
	else
	{
		// 刷新路径的剩余时长按 `StackDurationPolicy` 定（R5 Task 5）：`ESD_None` = **保留剩余**
		// （只重挂条目、不动计时），`ESD_RefreshRemainingToTotal` = 回满额；**首挂（`bRefresh == false`）
		// 恒为满额**。保留的那一支靠"不覆写 `DurationRemaining`"实现——`PushExpiry` 按字段值排到期时刻。
		const bool bKeepDurationRemaining = bRefresh
			&& Def.StackPolicy.StackDurationPolicy == EStackDurationPolicy::ESD_None
			&& Instance.DurationRemaining > 0.0;

		if (!bKeepDurationRemaining)
		{
			Instance.DurationRemaining = EvaluateTotalDuration(
				Subsystem, Def, Snapshot, Instance.Entity, Instance.Instigator, ParamTable);
		}

		PushExpiry(Subsystem, Instance);
	}

	// 周期：撤旧条目后按 `PeriodRefresh` 重建
	CancelEntry(Subsystem, Instance.PeriodEntry);

	if (Def.Period <= 0.0)
	{
		Instance.PeriodRemaining = 0.0;
		return;
	}

	// `EBPR_Immediate` 只在刷新路径上成立，且**同步执行一次**（不是"排一个 0 延迟条目"）：
	// "立即"的语义就是本次调用内发生，排 0 延迟会把它变成"下一泵点"（与语义不符）
	const bool bImmediate = bRefresh && Def.PeriodRefresh == ETcsBuffPeriodRefresh::EBPR_Immediate;
	const bool bKeepRemaining = bRefresh
		&& Def.PeriodRefresh == ETcsBuffPeriodRefresh::EBPR_Keep
		&& Instance.PeriodRemaining > 0.0;

	// 先定下一次的计时（Immediate 的那一次已经"发生"了，故满额重建）
	Instance.PeriodRemaining = bKeepRemaining ? Instance.PeriodRemaining : Def.Period;

	if (bImmediate)
	{
		const FTcsBuffInstanceHandle Handle = Instance.Handle;
		const FTcsBuffInstance PeriodSnapshot = Instance;
		Broadcast(Subsystem, Tag_TcsEvent_State_Periodic, PeriodSnapshot, EBuffRemoveCause::EBRC_Removed);

		// 回调可自移除、注销实体或嵌套刷新；不可继续使用原 Instance 引用。
		FTcsBuffInstance* AfterBroadcast = const_cast<FTcsBuffInstance*>(Find(Subsystem, Handle));
		if (AfterBroadcast && !AfterBroadcast->PeriodEntry.IsValid())
		{
			PushPeriod(Subsystem, *AfterBroadcast);
		}
		return;
	}

	PushPeriod(Subsystem, Instance);
}



// 生命周期操作
void FTcsBuffOps::RescheduleExpiry(UTcsBuffSubsystem& Subsystem, FTcsBuffInstance& Instance)
{
	// 句柄配对清理：旧条目先撤，再按新余量重入堆（"延长 / 缩短"都走这一条）
	CancelEntry(Subsystem, Instance.ExpiryEntry);
	PushExpiry(Subsystem, Instance);
}
