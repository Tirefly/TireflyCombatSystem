// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Chain/TcsEffectContext.h"
#include "TcsEffectSubsystem.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsCastEvents.h"
#include "Skill/TcsCastRun.h"
#include "Skill/TcsSkillRegistry.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



// 起链
void FTcsCastOps::StartMainChain(
	UTcsSkillSubsystem& Subsystem,
	const FTcsCastRun& Run,
	const FTcsSkillDefData& Def,
	const FTcsLearnedSkillEntry& Entry,
	FTcsChainRunHandle& OutChainRun)
{
	// **档位处置（MUST 逐档处理，不得留白）**：本 Task **只实现 `MCS_OnCastStarted`**。
	// `OnPhaseEnter` 需时段推进、`OnCastCompleted` 需自然终结（两者都归 Task 5），
	// `Custom` 需宿主编排 ⇒ 三档命中时 MUST **具名报出"该档未实现、主链未起"**，
	// **MUST NOT** 静默跳过——那会让配了 `OnCastCompleted` 的技能"不报错也不生效"（静默错误）。
	if (Def.MainChainStart != EMainChainStart::MCS_OnCastStarted)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("主链未起：MainChainStart=%d 档在本 Task 未实现（当前只支持 MCS_OnCastStarted=0）——")
			TEXT("单位=%lld 定义=%s 链=%s"),
			static_cast<int32>(Def.MainChainStart), Run.Unit.Id, *Entry.DefTag.ToString(),
			*Def.CastChainId.ToString());
		return;
	}

	if (!Def.CastChainId.IsValid())
	{
		// 作者期已由 `IsDataValid` 拦（`skill-def-asset` 的既有规则）——运行期遇到**不阻断激活**：
		// 技能已经放出去了，只是没有链。故 Warning 后正常返回（激活结果仍是成功）。
		UE_LOG(LogTcsSkill, Warning, TEXT("主链未起：CastChainId 无效（单位=%lld 定义=%s）"),
			Run.Unit.Id, *Entry.DefTag.ToString());
		return;
	}

	const UWorld* World = Subsystem.GetWorld();
	UTcsEffectSubsystem* Effect = World ? World->GetSubsystem<UTcsEffectSubsystem>() : nullptr;
	if (!Effect)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("主链未起：效果链门面不可得（单位=%lld 定义=%s）"),
			Run.Unit.Id, *Entry.DefTag.ToString());
		return;
	}

	// 黑板装配：`RunSource` MUST = 本 run 的来源锚点——**台账 `CHAIN-7` 的闭合依据**：
	// 施法运行态是链运行态的**第一个长生命周期持有者**，链内 `ModifyAttribute` 挂的账本条目
	// 即以该 Source 为锚点 ⇒ 施法终结时可按它回收（回收例程归 Task 5 Step 3）。
	// 本轮无独立的施法上下文形参 ⇒ 主体与发起者同为施法单位、目标集留空
	// （链侧 `SelectTargets` 自填；`Instigator` 缺省 = 施法者）。
	FTcsEffectContext Context;
	Context.Caster = Run.Unit;
	Context.Instigator = Run.Unit;
	Context.RunSource = Run.RunSource;

	// **★ 起链前 MUST 把结果以外的一切先取出来，且 MUST NOT 在 `ExecuteChain` 之后再写 `Run`**
	// （2026-10-08 静态自查抓到的一处真实悬空引用）：
	// `ExecuteChain` 会**同步执行**链步骤，而链步骤可以**再起一次技能激活**
	// （技能起链 → 链里一个步骤又激活另一个技能）⇒ 那会让施法池 `TArray` **扩容搬移** ⇒
	// 传进来的 `Run` 引用**在返回时已悬空**（本仓明文纪律：池元素"扩容即搬移，MUST NOT 跨可能
	// 触发扩容的调用缓存指针/引用"，`TcsInstancePool.h` 与 `TcsChainRun.h` 两处都写着）。
	// ⇒ 纪律：**`Run` 只读、MUST NOT 在起链后经它写字段**；需要落盘的结果由**调用方按句柄回写**。
	const FTcsCombatEntityHandle CasterUnit = Run.Unit;
	const FTcsSourceHandle RunSource = Run.RunSource;

	const FTcsChainRunHandle ChainRun = Effect->ExecuteChain(Def.CastChainId, MoveTemp(Context));

	UE_LOG(LogTcsSkill, Log, TEXT("主链已起：单位=%lld 定义=%s 链=%s 运行态=%d/%d 来源=%llu"),
		CasterUnit.Id, *Entry.DefTag.ToString(), *Def.CastChainId.ToString(),
		ChainRun.Index, ChainRun.Generation, RunSource.Id);

	// 结果经**输出形参**交回调用方（调用方按句柄重新解析后再写——见 `.cpp` 的接缝注）
	OutChainRun = ChainRun;
}
