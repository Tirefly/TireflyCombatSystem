// Copyright Tirefly. All Rights Reserved.

#include "TcsEffectSubsystem.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectLogChannel.h"



// 内部常量与设施
namespace
{
	/**
	 * 链**嵌套深度硬上限**（**调用栈护栏，非行为语义**——MUST NOT 做成链定义字段）。
	 *
	 * 取值依据：远高于任何合理的内容嵌套（几层子链），又远低于会耗尽调用栈的深度。
	 * 拦住自激的主力是**共享步数预算**（见 `FTcsChainEntryScope`），本常量只是
	 * "预算被作者配到很大"时的兜底——`MaxStepsPerFrame` 由链定义给出、作者可配到 4096。
	 */
	constexpr int32 TcsMaxChainNestingDepth = 16;

	/**
	 * 链进入作用域守卫：`RunFrom` 入口 `++`、出口 `--`（**含全部 return 路径**——该函数有四条
	 * `return`，手工增减必漏）。
	 *
	 * **最外层进入**时重置共享预算与本次上限 ⇒ 嵌套起链（`RunSubChain` / `Branch` 的 `bWait=true`
	 * 同步起链）**共用父链预算**：子链步数计入父链，MUST NOT 各自获得新预算。
	 */
	struct FTcsChainEntryScope
	{
		FTcsChainEntryScope(int32& InDepth, int32& InSteps, int32& InLimit, int32 InStepLimit)
			: Depth(InDepth), Steps(InSteps), Limit(InLimit)
		{
			if (Depth == 0)
			{
				Steps = 0;
				Limit = InStepLimit;
			}

			++Depth;
		}

		~FTcsChainEntryScope()
		{
			--Depth;
			if (Depth == 0)
			{
				Steps = 0;
				Limit = 0;
			}
		}

		int32& Depth;
		int32& Steps;
		int32& Limit;
	};
}



// 执行

FTcsChainRunHandle UTcsEffectSubsystem::ExecuteChain(FGameplayTag ChainId, FTcsEffectContext Context)
{
	ensure(IsInGameThread());

	const FTcsEffectChain* Chain = FindChain(ChainId);
	if (!Chain)
	{
		UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::ExecuteChain: 链 %s 未登记——拒绝起链"), *ChainId.ToString());
		return FTcsChainRunHandle();
	}

	FTcsChainRunHandle Handle;
	Handle.SetInner(RunPool.Allocate());

	FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
	Run->ChainId = ChainId;
	Run->PC = 0;
	Run->Context = MoveTemp(Context);
	Run->Owner = this;
	Run->Self = Handle;
	Run->PendingExpiry = FTcsTimeEntryHandle();
	Run->bHasPendingExpiry = false;
	Run->PendingEventTag = FGameplayTag();
	Run->bPendingEventHit = false;
	Run->PendingChildRun = FTcsChainRunHandle();
	Run->ParentRun = FTcsChainRunHandle();

	UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 起（步数=%d 单次上限=%d）"),
		*ChainId.ToString(), Chain->Steps.Num(), Chain->MaxStepsPerFrame);

	RunFrom(Handle);

	// 全即时链在返回前即走完（运行态已释放）——句柄活性由 IsRunActive 判定
	return Handle;
}

FTcsChainRunHandle UTcsEffectSubsystem::ExecuteChainForCaster(FGameplayTag ChainId, FTcsCombatEntityHandle Caster)
{
	// 上下文装配：Caster = Instigator = 传入句柄，Targets = 仅该句柄
	// （D4-4 v2"默认目标 = 事件目标"；脚本层入口无事件载荷，故以 Caster 为默认目标）
	FTcsEffectContext Context;
	Context.Caster = Caster;
	Context.Instigator = Caster;
	Context.Targets.Add(Caster);

	return ExecuteChain(ChainId, MoveTemp(Context));
}

bool UTcsEffectSubsystem::ResumeRun(FTcsChainRunHandle Handle)
{
	ensure(IsInGameThread());

	// 代际校验：句柄悬空 / 运行态已释放 / 世界将拆 = 正常竞态，静默返回（不 ensure）
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return false;
	}

	if (FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner()))
	{
		UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 唤醒（PC=%d）"), *Run->ChainId.ToString(), Run->PC);
	}

	RunFrom(Handle);
	return true;
}

bool UTcsEffectSubsystem::IsRunActive(FTcsChainRunHandle Handle) const
{
	return RunPool.IsValid(Handle.GetInner());
}



// 内核

void UTcsEffectSubsystem::RunFrom(FTcsChainRunHandle Handle)
{
	// 进入作用域守卫：最外层进入时重置**共享**预算与本次上限；退出（含全部 return 路径）归还深度。
	// 上限取自**最外层**链定义——若再拿内层链自己的上限裁一次，会出现"子链单独跑没事、
	// 被父起就熔断"这类难解释的行为（取舍记录见提案 design.md D-2）
	const FTcsChainRun* EntryRun = RunPool.Resolve(Handle.GetInner());
	const FTcsEffectChain* EntryChain = EntryRun ? FindChain(EntryRun->ChainId) : nullptr;
	const FTcsChainEntryScope EntryScope(ChainEntryDepth, StepsInChainEntry, ActiveEntryStepLimit,
		EntryChain ? FMath::Max(1, EntryChain->MaxStepsPerFrame) : 1);

	while (true)
	{
		// 池元素住连续缓冲、扩容即搬移（引擎事实 2026-09-17）——每步入器前按句柄重解析
		FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());
		if (!Run)
		{
			return;
		}

		const FTcsEffectChain* Chain = FindChain(Run->ChainId);
		if (!Chain)
		{
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 定义不可解析——运行态释放"),
				*Run->ChainId.ToString());
			ReleaseRun(Handle);
			return;
		}

		// 走完：正常路径（零诊断噪音）
		if (Run->PC >= Chain->Steps.Num())
		{
			UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 完成（共 %d 步）"),
				*Chain->ChainId.ToString(), Chain->Steps.Num());
			ReleaseRun(Handle);
			return;
		}

		// 熔断①：嵌套深度硬上限（**调用栈护栏**——共享预算才是拦自激的主力，本常量是
		// "预算被作者配到很大"时的兜底；故它是固定常量、MUST NOT 做成链定义字段）
		if (ChainEntryDepth > TcsMaxChainNestingDepth)
		{
			ensureMsgf(false, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 嵌套深度超上限（%d）——熔断断链"),
				*Chain->ChainId.ToString(), TcsMaxChainNestingDepth);
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 嵌套深度超上限（%d）——断链"),
				*Chain->ChainId.ToString(), TcsMaxChainNestingDepth);
			ReleaseRun(Handle);
			return;
		}

		// 熔断②：步数预算（**按最外层进入计**——嵌套起链共用父链预算；上界取最外层链定义）
		if (StepsInChainEntry >= ActiveEntryStepLimit)
		{
			ensureMsgf(false, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 单次进入步数超上限（共享预算 %d）——熔断断链"),
				*Chain->ChainId.ToString(), ActiveEntryStepLimit);
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 单次进入步数超上限（共享预算 %d）——断链"),
				*Chain->ChainId.ToString(), ActiveEntryStepLimit);
			ReleaseRun(Handle);
			return;
		}

		const FInstancedStruct& Step = Chain->Steps[Run->PC];
		const UScriptStruct* StepStruct = Step.GetScriptStruct();
		const FTcsStepExecute* Executor = FTcsEffectStepExecutorRegistry::Get().Find(StepStruct, GetWorld());
		if (!Executor)
		{
			// 未知步骤类型：断链（不静默跳过——静默会让"配置写错"表现成"效果没生效"）
			UE_LOG(LogTcsEffect, Error, TEXT("UTcsEffectSubsystem::RunFrom: 链 %s 第 %d 步类型 %s 无执行器——断链"),
				*Chain->ChainId.ToString(), Run->PC, StepStruct ? *StepStruct->GetName() : TEXT("<空>"));
			ReleaseRun(Handle);
			return;
		}

		const int32 StepIndex = Run->PC;
		const ETcsStepResult Result = (*Executor)(Step, Run->Context, *Run);
		++StepsInChainEntry;

		if (Result == ETcsStepResult::TSR_Running)
		{
			// 步内挂起：PC 停驻原步、运行态保持活动——等唤醒源按句柄重入（零每帧成本）
			UE_LOG(LogTcsEffect, Log, TEXT("UTcsEffectSubsystem: 链 %s 步 %d 挂起（PC 停驻）"),
				*Chain->ChainId.ToString(), StepIndex);
			return;
		}

		// 步骤执行期间可能新增运行态（池扩容即搬移）——推进 PC 前重新解析
		FTcsChainRun* AdvancedRun = RunPool.Resolve(Handle.GetInner());
		if (!AdvancedRun)
		{
			return;
		}
		AdvancedRun->PC = StepIndex + 1;
	}
}

void UTcsEffectSubsystem::ReleaseRun(FTcsChainRunHandle Handle)
{
	if (!RunPool.IsValid(Handle.GetInner()))
	{
		return;
	}

	FTcsChainRun* Run = RunPool.Resolve(Handle.GetInner());

	// 等待方句柄先拷出：下面的解锚会覆盖它，而通知必须在**释放之后**发（见文末）
	const FTcsChainRunHandle ParentRun = Run->ParentRun;

	// 解事件等待锚（**必须在此处、且早于下面的清锚**——`DropEventWait` 靠 `PendingEventTag` 认领登记）。
	// 这是退订三路径的最后一条（命中 / 超时 / 释放）：漏了它 = 总线按 tag 派发时打到一个已回收的运行态
	// ——到期锚有代际兜底，**订阅锚没有**（总线不看运行态代际），泄漏是静默的
	DropEventWait(Handle);

	// 清黑板与三个挂起锚：槽位内容不跨生命周期残留（池 Free 不清零——池零策略纪律）。
	// 本清单是**唯一无条件解锚点**——步骤侧配平只在正常唤醒路径上发生，
	// "挂起中被释放"（取消 / 世界拆解 / 熔断连带）只能在这里清（D-1 三条退订路径的最后一条）
	Run->ChainId = FGameplayTag();
	Run->PC = 0;
	Run->Context = FTcsEffectContext();
	Run->Owner = nullptr;
	Run->Self = FTcsChainRunHandle();
	Run->PendingExpiry = FTcsTimeEntryHandle();
	Run->bHasPendingExpiry = false;
	Run->PendingEventTag = FGameplayTag();
	Run->bPendingEventHit = false;
	Run->PendingChildRun = FTcsChainRunHandle();
	Run->ParentRun = FTcsChainRunHandle();

	RunPool.Free(Handle.GetInner());

	// 通知等待方（**异常结束同样通知**，D-3）：四条结束路径全汇聚于本函数——走完 /
	// 嵌套深度熔断 / 步数预算熔断 / 链定义不可解析 / 未知步骤类型。不通知 = 父永久挂起 = **死链**；
	// "父错误地继续 + 一条日志"可观测、可排查，比静默死锁便宜得多。
	//
	// **必须在 Free 之后发**：父侧 `RunSubChain` / `Branch` 被唤醒时会用
	// `IsRunActive(PendingChildRun)` 复核（挡"脚本层乱调 ResumeRun"的假唤醒）——Free 未做则
	// 复核答"子链还在跑" ⇒ 父重新挂起，而本次通知已用掉、不会再来 ⇒ 死链。
	// 父已先被释放的情形由 ResumeRun 的代际校验静默丢弃（陈旧句柄是正常竞态）。
	if (ParentRun.IsValid())
	{
		ResumeRun(ParentRun);
	}
}
