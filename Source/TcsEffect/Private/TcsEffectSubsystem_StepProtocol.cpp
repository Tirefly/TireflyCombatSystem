// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "TcsEffectLogChannel.h"



// 步骤协议面

ETcsStepResult UTcsEffectSubsystem::RunChildChainStep(FTcsChainRunHandle Parent, FGameplayTag ChainId, bool bWait)
{
	// 空 id 的口径归调用方（`RunSubChain` 记 Warning、`Branch` 的空支是正常配置）——此处只防御性拒绝
	if (!ChainId.IsValid())
	{
		return ETcsStepResult::TSR_Completed;
	}

	// 锚在场 = 本步已在等子链 ⇒ 本次是**唤醒重入**（门面不清挂起锚，由本方法配平）。
	// 父可能已释放（挂起期间被取消）——挂无可挂，按完成处理
	FTcsChainRun* ParentRun = RunPool.IsValid(Parent.GetInner()) ? RunPool.Resolve(Parent.GetInner()) : nullptr;
	if (!ParentRun)
	{
		return ETcsStepResult::TSR_Completed;
	}

	if (ParentRun->PendingChildRun.IsValid())
	{
		const FTcsChainRunHandle Child = ParentRun->PendingChildRun;

		// **复核**（挡假唤醒）：子链仍在跑 ⇒ 这次唤醒不是"子链完成"——`ResumeRun` 是脚本层可调的
		// 门面方法，一次来路不明的唤醒 MUST NOT 让父以为子链结束了
		if (IsRunActive(Child))
		{
			UE_LOG(LogTcsEffect, Verbose, TEXT("UTcsEffectSubsystem: 链 %s 收到无效唤醒（在等的子链仍在运行）——继续挂起"),
				*ParentRun->ChainId.ToString());
			return ETcsStepResult::TSR_Running;
		}

		// 子链确已结束（正常走完或异常断链都汇聚到 ReleaseRun 的同一通知）：配平锚、本步完成
		ParentRun->PendingChildRun = FTcsChainRunHandle();
		return ETcsStepResult::TSR_Completed;
	}

	// 首入：起链前取齐黑板。`ExecuteChain` 的形参是**值语义** ⇒ 拷贝发生在进入其函数体之前（即"起链前"）。
	// 子链把父链黑板整份拿来当底稿，但**来源两件事按"每一次运行"重算**（2026-10-05 R5 Task 6）：
	// ① 身份另发新号（子链是独立的施加方，不沿用父链锚点）；② 因果边指向父链的运行锚点（逐跳成边）。
	FTcsEffectContext ChildContext = ParentRun->Context;
	ChildContext.RunSource = FTcsSourceHandleRegistry().Allocate();
	ChildContext.CausedBy = ParentRun->Context.RunSource;

	const FTcsChainRunHandle Child = ExecuteChain(ChainId, MoveTemp(ChildContext));

	// 起链之后 MUST NOT 再用任何起链前的运行态指针（池扩容即搬移）——下面一律按句柄走
	if (!Child.IsValid())
	{
		// 子链未登记：`ExecuteChain` 已留 Error，本步按完成处理（不挂起、不重复报错）
		return ETcsStepResult::TSR_Completed;
	}

	if (!bWait)
	{
		// 放支线：父立即前进，不跟踪子链结局（子链自己的结束路径照常走）
		return ETcsStepResult::TSR_Completed;
	}

	// 装锚并复核：子链已同步走完（全即时链）时 false ⇒ 本步完成（挂起则唤醒方永不出现）
	return ArmChildWait(Parent, Child) ? ETcsStepResult::TSR_Running : ETcsStepResult::TSR_Completed;
}



bool UTcsEffectSubsystem::ArmChildWait(FTcsChainRunHandle Parent, FTcsChainRunHandle Child)
{
	// 子链已走完（**全即时链在 ExecuteChain 内即释放运行态**）：装锚无意义——调用方据此直接完成本步。
	// 这一步不能省：漏了它，父会挂在一条永远不会通知它的子链上（死链）
	if (!RunPool.IsValid(Child.GetInner()))
	{
		return false;
	}

	// 两次解析之间无分配 ⇒ 两个指针同时有效（池扩容只在 Allocate 时发生）
	FTcsChainRun* ChildRun = RunPool.Resolve(Child.GetInner());
	FTcsChainRun* ParentRun = RunPool.IsValid(Parent.GetInner()) ? RunPool.Resolve(Parent.GetInner()) : nullptr;
	if (!ChildRun || !ParentRun)
	{
		return false;
	}

	// 先写子后写父：任一侧失效都不会留下"父在等一条不认父的子链"
	ChildRun->ParentRun = Parent;
	ParentRun->PendingChildRun = Child;

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 等子链 %s 完成"),
		*ParentRun->ChainId.ToString(), *ChildRun->ChainId.ToString());
	return true;
}
