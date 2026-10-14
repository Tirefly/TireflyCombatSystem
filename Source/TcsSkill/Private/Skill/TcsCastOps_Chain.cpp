// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Chain/TcsEffectContext.h"
#include "TcsEffectSubsystem.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsCastEvents.h"
#include "Skill/TcsCastRun.h"
#include "Skill/TcsChainRedirect.h"
#include "Skill/TcsSkillRegistry.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



// 链身份解析
FGameplayTag FTcsCastOps::ResolveCastChainId(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Entity,
	FGameplayTag GlobalChainId)
{
	// ── 第 2 级：链重定向栈（数据行，策划零代码） ──
	//
	// **未命中 ⇒ 返回 `GlobalChainId`**（第 1 级 = 全局定义）：这是**正常路径**，
	// **MUST NOT** 报"未实现"——报它会让"零意外红字"这条验收信号被自己的正常路径淹没。
	// 解析函数自己保证这一点（`FTcsChainRedirectRegistry::Resolve` 未命中即原样返回）。
	const FGameplayTag AfterStack = Subsystem.ChainRedirects.Resolve(Entity, GlobalChainId);

	// ── 第 3 级：Custom 契约（**本轮具名未实现**，归 `R6.5-g`） ──
	//
	// **未注入 ⇒ 不报**：没有东西被拒绝求值，"未实现"这几个字反而会污染常规命令的输出
	// （同 `SetEntityQuery` 的"未注入 = 配置状态不是错误"口径）。
	// **已注入 ⇒ 具名 Warning + 跳过**：MUST NOT 静默忽略——那会让宿主以为它生效了
	// （本仓反复拦的静默错误）。**跳过而非调用**：调用一个"本轮不保证语义"的宿主实现，
	// 会得到无法解释的结果；如实报出才是可诊断的。
	if (Subsystem.ChainRedirectResolver.GetObject())
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("链重定向：自定义契约（ITcsChainRedirectResolver）的求值未实现（归 R6.5-g）——")
			TEXT("已注入实现但不予调用；解析结果取数据栈档（%s）"),
			*AfterStack.ToString());
	}

	return AfterStack;
}



// 起链
bool FTcsCastOps::ExecuteMainChain(
	UTcsSkillSubsystem& Subsystem,
	FTcsCastRunHandle RunHandle,
	FGameplayTag ChainId)
{
	if (!ChainId.IsValid())
	{
		// 作者期已由 `IsDataValid` 拦（`skill-def-asset` 的既有规则）——运行期遇到**不阻断激活**：
		// 技能已经放出去了，只是没有链。故 Warning 后正常返回（激活结果仍是成功）。
		UE_LOG(LogTcsSkill, Warning, TEXT("主链未起：链身份无效（run=%d/%d）"),
			RunHandle.Index, RunHandle.Generation);
		return false;
	}

	// **按句柄取运行态，且立刻把需要的一切拷成值**（理由见下方"起链前 MUST 取出"）
	const FTcsCastRun* Run = ResolveRun(Subsystem, RunHandle);
	if (!Run)
	{
		// 运行态在解析期间被终结（时序竞态）⇒ 静默 false，不 ensure
		return false;
	}

	UTcsEffectSubsystem* Effect = Subsystem.GetWorld()
		? Subsystem.GetWorld()->GetSubsystem<UTcsEffectSubsystem>() : nullptr;
	if (!Effect)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("主链未起：效果链门面不可得（实体=%lld）"), Run->Entity.Id);
		return false;
	}

	// 黑板装配：`RunSource` MUST = 本 run 的来源锚点——**台账 `CHAIN-7` 的闭合依据**：
	// 施法运行态是链运行态的**第一个长生命周期持有者**，链内 `ModifyAttribute` 挂的账本条目
	// 即以该 Source 为锚点 ⇒ 施法终结时可按它回收（回收例程住 `FinishCastRun`）。
	// 本轮无独立的施法上下文形参 ⇒ 主体与发起者同为施法实体、目标集留空
	// （链侧 `SelectTargets` 自填；`Instigator` 缺省 = 施法者）。
	FTcsEffectContext Context;
	Context.Caster = Run->Entity;
	Context.Instigator = Run->Entity;
	Context.RunSource = Run->RunSource;

	// **★ 起链前 MUST 把结果以外的一切先取出来，且 MUST NOT 在 `ExecuteChain` 之后再写 `Run`**
	// （2026-10-08 静态自查抓到的一处真实悬空引用）：
	// `ExecuteChain` 会**同步执行**链步骤，而链步骤可以**再起一次技能激活**
	// （技能起链 → 链里一个步骤又激活另一个技能）⇒ 那会让施法池 `TArray` **扩容搬移** ⇒
	// 传进来的 `Run` 引用**在返回时已悬空**（本仓明文纪律：池元素"扩容即搬移，MUST NOT 跨可能
	// 触发扩容的调用缓存指针/引用"，`TcsInstancePool.h` 与 `TcsChainRun.h` 两处都写着）。
	const FTcsCombatEntityHandle CasterEntity = Run->Entity;
	const FTcsSourceHandle RunSource = Run->RunSource;

	const FTcsChainRunHandle ChainRun = Effect->ExecuteChain(ChainId, MoveTemp(Context));

	UE_LOG(LogTcsSkill, Log, TEXT("主链已起：实体=%lld 链=%s 运行态=%d/%d 来源=%llu"),
		CasterEntity.Id, *ChainId.ToString(),
		ChainRun.Index, ChainRun.Generation, RunSource.Id);

	// **按句柄重新解析后回写**（`Run` 引用此刻可能已悬空）：
	// ① `ChainRunHandle`——"这次施法起了哪条链"；
	// ② `ActiveChainId`——**实际起来的链身份**（重定向的验收读数取它）。
	// **② 是重定向能被真正验证的关键**：链运行态不持链身份、`FindChain` 也只答"某条链是否登记"
	// ⇒ 从 `ChainRunHandle` **反查不到**这次起的是哪条链。若装置改取"解析函数返回了什么"，
	// 那是在测**解析**而不是测**起链**——一个"解析对了但起链仍用旧 id"的实现会全绿。
	if (FTcsCastRun* LiveRun = ResolveRun(Subsystem, RunHandle))
	{
		LiveRun->ChainRunHandle = ChainRun;
		LiveRun->ActiveChainId = ChainId;
	}

	// 结果经**输出形参**交回调用方（调用方按句柄重新解析后再写——见 `.h` 的接缝注）
	return ChainRun.IsValid();
}

void FTcsCastOps::StartMainChain(
	UTcsSkillSubsystem& Subsystem,
	FTcsCastRunHandle RunHandle,
	const FTcsCastRun& Run,
	const FTcsSkillDefData& Def,
	const FTcsLearnedSkillEntry& Entry,
	ECastChainTrigger Trigger,
	FTcsChainRunHandle& OutChainRun)
{
	// ── 档位判定（**单点**：三个调用点只报"我是谁"，判"该不该起"只在本函数） ──
	//
	// **`Run.MainChainStart` 是激活时冻结的值**（不是 `Def.MainChainStart`）——理由见
	// `FTcsCastRun::MainChainStart` 的注释：定义可在施法期间被注销，回查会拿到 nullptr。
	// 本函数仍收 `Def` 是因为**链身份与时段标识**要从它取（调用方已保证它此刻有效）。
	const EMainChainStart StartMode = Run.MainChainStart;

	// 档位与触发点不匹配 ⇒ **静默返回**（正常路径：配 `OnCastCompleted` 的技能在激活时
	// 本来就不该起链，报日志会让常规命令被正常路径刷满）
	switch (StartMode)
	{
	case EMainChainStart::MCS_OnCastStarted:
		if (Trigger != ECastChainTrigger::ECT_OnCastStarted)
		{
			return;
		}
		break;

	case EMainChainStart::MCS_OnPhaseEnter:
	{
		if (Trigger != ECastChainTrigger::ECT_OnPhaseEnter)
		{
			return;
		}

		// **匹配"当前时段"而不是某个记忆中的时段**：对"时段进入"这个触发点而言，
		// 当前段就是刚进入的那段（`AdvancePhase` 先 `++PhaseIndex` 再调本函数）⇒ 判据是
		// "当前段的 `Tag` == 冻结的 `MainChainStartPhaseTag`"，不需要额外的"上一次是哪段"状态。
		//
		// **标识无效 ⇒ 不命中**（本档没配目标时段就没有触发条件）；**段数越界 ⇒ 不命中**
		// （防御性：调用方已保证有效，但本函数不因此越界读定义内容）。
		const bool bPhaseTagMatches =
			Run.MainChainStartPhaseTag.IsValid()
			&& Def.Phases.IsValidIndex(Run.PhaseIndex)
			&& Def.Phases[Run.PhaseIndex].Tag == Run.MainChainStartPhaseTag;
		if (!bPhaseTagMatches)
		{
			return;
		}
		break;
	}

	case EMainChainStart::MCS_OnCastCompleted:
		if (Trigger != ECastChainTrigger::ECT_OnCastCompleted)
		{
			return;
		}
		break;

	case EMainChainStart::MCS_Custom:
	default:
		// **`MCS_Custom` 保持具名未实现**（归宿主编排，归 `R6.5-g`）：
		// **MUST NOT 静默跳过**——那会让配了该档的技能"不报错也不生效"（静默错误，本仓反复拦的形态）。
		// 本档与触发点无关（它压根不由框架决定时点）⇒ 在档位判定处即报出并返回。
		UE_LOG(LogTcsSkill, Warning,
			TEXT("主链未起：MainChainStart=%d 档（自定义）未实现——主链启动时点归宿主编排（R6.5-g）。")
			TEXT("实体=%lld 定义=%s 链=%s"),
			static_cast<int32>(StartMode), Run.Entity.Id, *Entry.DefTag.ToString(),
			*Def.CastChainId.ToString());
		return;
	}

	if (!Def.CastChainId.IsValid())
	{
		// 作者期已由 `IsDataValid` 拦（`skill-def asset` 的既有规则）——运行期遇到**不阻断激活**。
		// 本处只报"全局定义缺失"；**不由本函数去猜替代链**（重定向是"替代已声明的链"，
		// 没有全局定义时它没有输入）。
		UE_LOG(LogTcsSkill, Warning, TEXT("主链未起：CastChainId 无效（实体=%lld 定义=%s）"),
			Run.Entity.Id, *Entry.DefTag.ToString());
		return;
	}

	// ── 链身份解析（三级顺序） ──
	//
	// **★ 位置在这里不是随便选的**：解析 MUST 排在**全部早退之前**吗？不——它排在
	// "档位匹配 + 全局定义有效"**之后**。理由：解析本身会**报日志**（`Custom` 档的具名未实现
	// `Warning`），若排在早退之前，"档位根本不匹配（正常路径）"的每次调用都会带出一条
	// "重定向未实现"的 Warning ⇒ **常规命令被正常路径刷满红字**，正是本仓反复拦的信号污染。
	// ⇒ 顺序 = **先判"这次该不该起链"，再判"起了哪条链"**。
	//
	// **MUST NOT 修改共享 Def**：解析是**读**（全局定义 → 数据栈 → Custom），
	// 结果只用于本次 `ExecuteChain`——设计原文明文"绝不修改共享 Def"。
	const FGameplayTag ResolvedChainId = ResolveCastChainId(Subsystem, Run.Entity, Def.CastChainId);

	// **起链**（结果与"实际链身份"都在 `ExecuteMainChain` 内落定）。
	// **句柄显式传入**（见头文件的形参纪律：**MUST NOT** 从账本条目反查——在
	// `CI_InstancePerExecution` 下同一条目可有多个在飞 run，反查会取错那一发）。
	if (ExecuteMainChain(Subsystem, RunHandle, ResolvedChainId))
	{
		if (const FTcsCastRun* LiveRun = ResolveRun(Subsystem, RunHandle))
		{
			OutChainRun = LiveRun->ChainRunHandle;
		}
	}
}
