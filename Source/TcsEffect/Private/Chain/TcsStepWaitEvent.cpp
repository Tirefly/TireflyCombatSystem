// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepWaitEvent.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "Clock/TcsClockSubsystem.h"
#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"



namespace
{
	// 从运行态取时钟设施（宿主门面 → 世界 → 时钟子系统；缺设施返回 nullptr）
	UTcsClockSubsystem* ResolveWaitEventClock(const FTcsChainRun& Run)
	{
		const UTcsEffectSubsystem* Owner = Run.Owner.Get();
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UTcsClockSubsystem>() : nullptr;
	}

	/**
	 * 事件等待步骤执行器：首入装锚（订阅 + 可选到期）并挂起；被唤醒时按**哪一路先到**配平并完成。
	 *
	 * 唤醒判据（两路 + 一路无效，见门面与 `TcsChainRun.h` 的配平纪律）：门面唤醒时**不清挂起锚**，
	 * 故锚的在场与否就是"首入 / 被唤醒"的判据；而两路各自只留自己的标记，故还需要一个区分标记
	 * （`bPendingEventHit`）才分得清是命中的还是超时的。
	 */
	ETcsStepResult ExecuteStepWaitEvent(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		// ① 命中唤醒：门面已在派发当场写载荷 + 置判据（载荷不在此处写——它不是本步的产物）
		if (Run.bPendingEventHit)
		{
			Run.bPendingEventHit = false;

			// 等待表登记已由门面在命中时摘除（它必须早于唤醒完成，故不由本步摘）；此处只配平运行态锚
			Run.PendingEventTag = FGameplayTag();

			// 两路互相取消：事件先到 ⇒ 撤掉到期条目。**从步骤侧撤是安全的**——此刻不在到期堆的派发
			// 回调里（在事件派发里），故不构成对到期堆的遍历中修改
			if (Run.bHasPendingExpiry)
			{
				if (UTcsClockSubsystem* Clock = ResolveWaitEventClock(Run))
				{
					Clock->CancelExpiry(Run.PendingExpiry);
				}

				Run.PendingExpiry = FTcsTimeEntryHandle();
				Run.bHasPendingExpiry = false;
			}

			UE_LOG(LogTcsEffect, Verbose, TEXT("WaitEvent[%s]: 事件命中唤醒，PC=%d 本步完成"),
				*Run.ChainId.ToString(), Run.PC);
			return ETcsStepResult::TSR_Completed;
		}

		// ② 超时唤醒（这条路门面没参与 ⇒ 等待表登记由本步摘）
		if (Run.bHasPendingExpiry)
		{
			Run.PendingExpiry = FTcsTimeEntryHandle();
			Run.bHasPendingExpiry = false;

			if (UTcsEffectSubsystem* Facade = Run.Owner.Get())
			{
				Facade->DropEventWait(Run.Self);
			}
			else
			{
				Run.PendingEventTag = FGameplayTag();
			}

			// 载荷保持原样：超时不是事件（"没等到"与"等到了空载荷"必须可区分）
			UE_LOG(LogTcsEffect, Log, TEXT("WaitEvent[%s]: 超时唤醒（事件未到达），PC=%d 本步完成"),
				*Run.ChainId.ToString(), Run.PC);
			return ETcsStepResult::TSR_Completed;
		}

		// ③ 无效唤醒（锚在场、两路判据都不在——如脚本层按句柄手动 `ResumeRun`）：保持挂起。
		// MUST NOT 配平、MUST NOT 前进：这条来路不明的唤醒不代表事件到达，也不代表超时
		if (Run.PendingEventTag.IsValid())
		{
			UE_LOG(LogTcsEffect, Verbose, TEXT("WaitEvent[%s]: 收到无效唤醒（仍在等 %s）——继续挂起"),
				*Run.ChainId.ToString(), *Run.PendingEventTag.ToString());
			return ETcsStepResult::TSR_Running;
		}

		// ④ 首入
		const FTcsStepWaitEvent* Step = StepData.GetPtr<FTcsStepWaitEvent>();
		UTcsEffectSubsystem* Facade = Run.Owner.Get();
		if (!Step || !Facade)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("WaitEvent[%s]: 步骤载荷类型不符或门面不可得——本步按完成处理（不挂起）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 无效 tag = 未配置：不挂起但留痕（挂起在一个永远不会命中的 tag 上等于死链）
		if (!Step->EventTag.IsValid())
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("WaitEvent[%s]: EventTag 无效——本步按完成处理（不挂起）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 装"等事件"锚（同一 tag 多等待者共用一条订阅——订阅配对归门面侧等待表）
		if (!Facade->ArmEventWait(Run.Self, Step->EventTag))
		{
			// 总线设施不可得：挂起没有可靠的唤醒源 ⇒ 降级完成（口径同 WaitDelay 的缺设施路径）
			UE_LOG(LogTcsEffect, Error, TEXT("WaitEvent[%s]: 总线/等待设施不可得——本步按完成处理（不挂起）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 可选超时：入到期堆（基准 = 战斗时钟 Elapsed 时轴，同 WaitDelay）
		if (Step->TimeoutSeconds > 0.0)
		{
			UTcsClockSubsystem* Clock = ResolveWaitEventClock(Run);
			if (!Clock)
			{
				// 作者配了超时即表示不接受"永远等" ⇒ 撤掉刚装的等待，按完成处理（不接受只等事件的半程挂起）
				Facade->DropEventWait(Run.Self);
				UE_LOG(LogTcsEffect, Error,
					TEXT("WaitEvent[%s]: 时钟子系统不可得——已配超时的等待无法成立，本步按完成处理（不挂起）"),
					*Run.ChainId.ToString());
				return ETcsStepResult::TSR_Completed;
			}

			const FTcsChainRunHandle Self = Run.Self;
			const TWeakObjectPtr<UTcsEffectSubsystem> WeakOwner = Run.Owner;
			const double DueTime = Clock->GetClock().Elapsed + Step->TimeoutSeconds;

			// 到期唤醒：按运行态句柄重入（门面做代际校验——运行态已释放则静默丢弃）
			// OwnerId 取池内槽位索引（句柄已展平为 int32 字段，经 GetInner 保位模式一致）
			Run.PendingExpiry = Clock->PushExpiry(DueTime, Self.GetInner().Index,
				[WeakOwner, Self](uint64 /*OwnerId*/)
				{
					if (UTcsEffectSubsystem* Subsystem = WeakOwner.Get())
					{
						Subsystem->ResumeRun(Self);
					}
				});
			Run.bHasPendingExpiry = true;

			UE_LOG(LogTcsEffect, Log, TEXT("WaitEvent[%s]: PC=%d 挂起等事件 %s（%.3fs 后超时）"),
				*Run.ChainId.ToString(), Run.PC, *Step->EventTag.ToString(), Step->TimeoutSeconds);
		}
		else
		{
			UE_LOG(LogTcsEffect, Log, TEXT("WaitEvent[%s]: PC=%d 挂起等事件 %s（无超时）"),
				*Run.ChainId.ToString(), Run.PC, *Step->EventTag.ToString());
		}

		// 锚已装齐（订阅 + 可选到期）：PC 停驻本步，等唤醒源按句柄重入
		return ETcsStepResult::TSR_Running;
	}
}

// 自注册（本模块内；机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepWaitEvent, ExecuteStepWaitEvent)
