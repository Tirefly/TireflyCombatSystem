// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepWaitDelay.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "Clock/TcsClockSubsystem.h"
#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"



namespace
{
	// 从运行态取时钟设施（宿主门面 → 世界 → 时钟子系统；缺设施返回 nullptr）
	UTcsClockSubsystem* ResolveWaitDelayClock(const FTcsChainRun& Run)
	{
		const UTcsEffectSubsystem* Owner = Run.Owner.Get();
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UTcsClockSubsystem>() : nullptr;
	}

	/**
	 * 等待步骤执行器：首入入到期堆并挂起；被唤醒（挂起锚已配平）即本步完成。
	 * 时间基准 = 战斗时钟 Elapsed（ScaledDt 时轴）——slomo 减速时等待同比例拉长（不取实时秒）。
	 */
	ETcsStepResult ExecuteStepWaitDelay(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		// 唤醒重入：门面不清挂起锚，由本步配平（"首入 / 被唤醒"的唯一判据）
		if (Run.bHasPendingExpiry)
		{
			Run.bHasPendingExpiry = false;
			Run.PendingExpiry = FTcsTimeEntryHandle();

			UE_LOG(LogTcsEffect, Verbose, TEXT("WaitDelay[%s]: 到期唤醒，PC=%d 本步完成"),
				*Run.ChainId.ToString(), Run.PC);
			return ETcsStepResult::TSR_Completed;
		}

		const FTcsStepWaitDelay* Step = StepData.GetPtr<FTcsStepWaitDelay>();
		if (!Step)
		{
			UE_LOG(LogTcsEffect, Error, TEXT("WaitDelay[%s]: 步骤载荷类型不符——本步按完成处理（不挂起）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		UTcsClockSubsystem* Clock = ResolveWaitDelayClock(Run);
		UTcsEffectSubsystem* Owner = Run.Owner.Get();
		if (!Clock || !Owner)
		{
			// 降级：无时钟设施则跳过等待（不挂死、不崩溃——挂起需要一个可靠的唤醒源）
			UE_LOG(LogTcsEffect, Error, TEXT("WaitDelay[%s]: 世界/时钟子系统不可得——本步按完成处理（不挂起）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		const double Seconds = FMath::Max(0.0, Step->Seconds);
		const double DueTime = Clock->GetClock().Elapsed + Seconds;
		const FTcsChainRunHandle Self = Run.Self;
		const TWeakObjectPtr<UTcsEffectSubsystem> WeakOwner = Run.Owner;

		// 到期唤醒：按运行态句柄重入（门面做代际校验——运行态已释放则静默丢弃）
		Run.PendingExpiry = Clock->PushExpiry(DueTime, Self.Inner.Index,
			[WeakOwner, Self](uint64 /*OwnerId*/)
			{
				if (UTcsEffectSubsystem* Subsystem = WeakOwner.Get())
				{
					Subsystem->ResumeRun(Self);
				}
			});
		Run.bHasPendingExpiry = true;

		UE_LOG(LogTcsEffect, Log, TEXT("WaitDelay[%s]: PC=%d 挂起 %.3fs（现在 %.3f → 到期 %.3f）"),
			*Run.ChainId.ToString(), Run.PC, Seconds, Clock->GetClock().Elapsed, DueTime);
		return ETcsStepResult::TSR_Running;
	}
}

// 自注册（模块静态初始化期登记——机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepWaitDelay, ExecuteStepWaitDelay)
