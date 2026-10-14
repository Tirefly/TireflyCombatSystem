// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Clock/TcsClockSubsystem.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsCastEvents.h"
#include "Skill/TcsSkillRegistry.h"
#include "State/TcsStateParamTableReader.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"
#include "TcsStateSubsystem.h"



namespace
{
	/**
	 * 求某段的时长（秒）——**求值上下文绑定的是本次施法的参数快照**。
	 *
	 * **为什么必须在快照作用域里求值**（不是可选的写法细节）：`Duration` 是 `FTcsParamValue`，
	 * 其源可以是**引用类**（`ParamRef`，运行时从 `Ctx.ParamTable` 取键值）。而技能侧的参数表载体
	 * **就是本 run 的 `ParamSnapshot`**——不绑它，引用类时长会**静默落各自兜底**（不报错、不崩溃，
	 * 只是时长悄悄不对）。这与 `ParamChainRows` 物化用的是**同一套绑定手法**
	 * （`UTcsStateParamTableReader` + `FTcsStateSnapshotScope`），MUST NOT 各绑一套。
	 *
	 * **绑定的是快照 ⇒ 语义自动成立**："被修正器改变时段时长 ⇒ 下一次激活生效"不需要任何专门代码：
	 * 快照在激活瞬间冻结，本次施法读到的永远是那一刻的值。
	 *
	 * **时长下界取 0**：负时长会让到期时刻落在过去（`AdvanceTo` 用 `<=` 判到期 ⇒ 立刻到期），
	 * 那不是"配置错误"而是"配错了会炸得很响"的形态；夹到 0 使之退化为"下一泵点到期"，
	 * 与状态侧 `PushExpiry` 的 `FMath::Max(0.0, …)` 逐字同款。
	 */
	double EvaluateCastPhaseDuration(
		UTcsSkillSubsystem& Subsystem,
		const FTcsPhaseSpan& Phase,
		const FTcsParamSnapshot& ParamSnapshot,
		FTcsCombatEntityHandle Entity,
		int32 Level)
	{
		// 状态门面不可得（世界拆解 / 单测无状态门面）⇒ 退化为"无参数表"：
		// 引用类源落各自兜底，**不 ensure、不红字**（与 `UTcsStateParamTableReader` 可空的口径一致）
		UTcsStateSubsystem* StateSubsystem = Subsystem.GetWorld()
			? Subsystem.GetWorld()->GetSubsystem<UTcsStateSubsystem>() : nullptr;

		FTcsStateSnapshotScope Scope(
			StateSubsystem ? StateSubsystem->GetParamTableReader() : nullptr,
			&ParamSnapshot);

		FTcsParamEvaluateContext Ctx;
		Ctx.Subject = Entity;
		Ctx.Instigator = Entity;
		Ctx.EffectiveLevel = Level;
		Ctx.ParamTable = Scope.GetTable();

		return FMath::Max(0.0, Phase.Duration.Evaluate(Ctx));
	}

	/**
	 * 取消一个到期锚点（惰性取消：即刻回收槽位；堆内残留条目出队时凭代际失配跳过）。
	 *
	 * **撤完 MUST 置空**：本条目的是让"可重复调用"成立——四路终结都可能走到它，而**先到的那个**
	 * 撤完必须让后到的那个成为空操作。体例照状态侧 `TcsStateOps_Lifetime.cpp` 的 `CancelEntry`。
	 *
	 * **⚠ 这里有一个真实陷阱，MUST 靠"回调入口先复位"而不是靠"先判有效性"来避**：
	 * `FTcsExpiryHeap::Cancel` 对**悬空**句柄是 `ensureMsgf`（留红字！）。而"到期回调正在跑"
	 * 那一刻，它自己的条目**仍然有效**（`FireDueItem` 在回调**返回之后**才释放）⇒ 若终结例程
	 * 由该回调触发（**最后一段到期 → 完成终结**正是这条路径），在这里撤它是**合法且必需**的
	 * （不撤则槽位泄漏）。真正会踩 ensure 的是另一种情形：**回调已跑完并释放了条目，而
	 * `PhaseExpiryEntry` 还留着那个已失效的句柄**——那时再撤就是悬空句柄。
	 * ⇒ 结论：**到期回调的入口 MUST 先把自己的锚点复位**（`AdvancePhase` 第一条语句即是），
	 * 使终结例程看到的永远是"要么是活的、要么是空的"，**永不会是"已死的"**。
	 */
	void CancelPhaseExpiry(UTcsSkillSubsystem& Subsystem, FTcsTimeEntryHandle& Entry)
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



// 起链触发点（**私有静态成员**：它够得着私有的 `StartMainChain` 与 `ECastChainTrigger`）
void FTcsCastOps::TryStartMainChainAt(
	UTcsSkillSubsystem& Subsystem,
	FTcsSkillInstanceHandle RunHandle,
	FTcsCombatEntityHandle Entity,
	FTcsSkillEntryHandle EntryHandle,
	ECastChainTrigger Trigger)
{
	// **每一步都重新解析**（"任何可能广播/起链的调用之后，手里的指针都可能失效"）：
	// 起链会同步执行链步骤，链步骤可再起激活（池扩容搬移）或终结本 run。
	const FTcsSkillInstance* Run = Find(Subsystem, RunHandle);
	if (!Run)
	{
		return;
	}

	// **条目经门面的公开读口取**（不必进私有区——本函数只需要读条目）
	const FTcsLearnedSkillEntry* Entry = Subsystem.GetEntry(Entity, EntryHandle);
	if (!Entry)
	{
		return;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Run->DefTag);
	if (!Def)
	{
		// 定义被注销 ⇒ 起不了链。**不在这里报 Warning**：`StartMainChain` 只对"该档未实现"
		// 具名报出，而"定义没了"是另一件事（调用方各自已有其处置）
		return;
	}

	// **`Run` 传 const 引用、`RunHandle` 显式传**（理由见 `StartMainChain` 的形参纪律）
	FTcsChainRunHandle ChainRun;
	StartMainChain(Subsystem, RunHandle, *Run, *Def, Trigger, ChainRun);

	if (!ChainRun.IsValid())
	{
		// 档位不匹配 / 链 id 无效 / 门面不可得 ⇒ 没起链，无需回写
		return;
	}

	// **按句柄重新解析后回写**（`Run` 引用在起链后可能已悬空）。
	// **链身份不过手**：`ExecuteMainChain` 已把它写进运行态——那正是"单一写入点"。
	if (FTcsSkillInstance* WriteRun = Find(Subsystem, RunHandle))
	{
		WriteRun->ChainRunHandle = ChainRun;
	}
}



// 时段推进
bool FTcsCastOps::EnterPhase(UTcsSkillSubsystem& Subsystem, FTcsSkillInstanceHandle RunHandle)
{
	FTcsSkillInstance* Run = ResolveInstance(Subsystem, RunHandle);
	if (!Run)
	{
		// 悬空句柄 = 时序竞态（调用方手上的句柄过期），不是契约违规 ⇒ 静默 false，不 ensure
		return false;
	}

	// 条目与定义：**定义取不到 ⇒ 无时段可推进**（定义在施法期间被注销是合法操作）
	const FTcsCombatEntityHandle Entity = Run->Entity;
	const int32 Level = Run->Level;
	const FTcsSourceHandle CascadeAnchor = Run->CascadeAnchor;
	const FTcsSkillEntryHandle EntryHandle = Run->EntryHandle;

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Entity);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(Run->DefTag) : nullptr;
	if (!Def)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("时段未进入：定义取不到（实体=%lld）"), Entity.Id);
		return false;
	}

	// **待推进判据 MUST 取"下标是否落在表内"**，**MUST NOT** 写成 `Phases.IsEmpty()` 这类空表判定
	// ——那会让"配了时段但已走到末尾"与"空时段表"走两条不同的路（同一件事两个入口）。
	// 瞬发（空表）因此**不需要分支**：它天然产生"下标 0 不落在空表内"这一状态，直接返回 false。
	if (!Def->Phases.IsValidIndex(Run->PhaseIndex))
	{
		// 正常路径（瞬发 / 已到末尾）⇒ Verbose 级，**零红字**
		UE_LOG(LogTcsSkill, Verbose, TEXT("时段未进入：无当前时段（实体=%lld 下标=%d 段数=%d）"),
			Entity.Id, Run->PhaseIndex, Def->Phases.Num());
		return false;
	}

	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		// **时钟不可得（世界拆解 / 单测）⇒ 静默降级**：Log 级、零红字、**不留待推进时段**
		// （退化为瞬发语义，调用方随即走完成终结）、**MUST NOT ensure**——那是时序而非配置错误。
		UE_LOG(LogTcsSkill, Log, TEXT("时段未进入：时钟子系统不可得（实体=%lld）——退化为瞬发语义"),
			Entity.Id);
		return false;
	}

	const FTcsPhaseSpan& Phase = Def->Phases[Run->PhaseIndex];
	const double Duration = EvaluateCastPhaseDuration(Subsystem, Phase, Run->ParamSnapshot, Entity, Level);

	// 到期时刻 = **Elapsed 时轴上的绝对时刻**；入堆一律经门面读口（`FTcsExpiryHeap` 是泵私有）
	const double DueTime = Clock->GetClock().Elapsed + Duration;

	// **回调捕获"世界弱引用 + 运行句柄"**（照 `TcsStateOps_Lifetime.cpp` 的既有体例）：
	// 世界已亡（世界拆解）或句柄悬空（run 已被终结）即静默丢弃 —— 不崩溃、不 ensure。
	// `OwnerId` 传来源 Id（堆零领域词汇，只作回调归属读数）
	TWeakObjectPtr<UTcsSkillSubsystem> WeakSubsystem(&Subsystem);
	const FTcsSkillInstanceHandle PhaseRunHandle = RunHandle;
	const FTcsTimeEntryHandle PushedEntry = Clock->PushExpiry(DueTime, CascadeAnchor.Id,
		[WeakSubsystem, PhaseRunHandle](uint64 /*OwnerId*/)
		{
			if (UTcsSkillSubsystem* LiveSubsystem = WeakSubsystem.Get())
			{
				FTcsCastOps::AdvancePhase(*LiveSubsystem, PhaseRunHandle);
			}
		});

	// **回写靠句柄重新解析**：上面求值调用链可能经引用类源的求值路径再起一次激活
	// （宿主自定义源是任意代码）⇒ 池可能已扩容搬移
	if (FTcsSkillInstance* LiveRun = ResolveInstance(Subsystem, RunHandle))
	{
		LiveRun->ExpiryEntry = PushedEntry;
	}
	else
	{
		// 运行态在入堆期间已被终结。终结例程撤的是它当时看到的锚点，而它**看不到这枚新条目**
		// ⇒ **本处必须自己把它撤掉**，否则这枚条目会活到到期、回调一个已终结的 run。
		// **不置空任何字段**：本 run 已不在池里（没有字段可置）。
		if (UTcsClockSubsystem* LiveClock = Subsystem.GetClockSubsystem())
		{
			LiveClock->CancelExpiry(PushedEntry);
		}

		UE_LOG(LogTcsSkill, Log, TEXT("时段到期条目已撤销：运行态在入堆期间被终结（实体=%lld）"), Entity.Id);
		return false;
	}

	UE_LOG(LogTcsSkill, Log, TEXT("时段已进入：实体=%lld 下标=%d 时长=%.3f 到期=%.3f 条目=%d/%d"),
		Entity.Id, Run->PhaseIndex, Duration, DueTime, PushedEntry.Inner.Index, PushedEntry.Inner.Generation);

	// **★ 这里再解析一次**（不是冗余）：上面的日志与回写之间没有广播，但**本函数被
	// `AdvancePhase` 调用时，调用方可能在起链之后**（起链会搬移池）⇒ `Run` 指针可能已失效，
	// 故上面那句日志读的是**回写前**的 `Run->PhaseIndex`；本行起一律只用返回值。
	return true;
}

void FTcsCastOps::AdvancePhase(UTcsSkillSubsystem& Subsystem, FTcsSkillInstanceHandle RunHandle)
{
	FTcsSkillInstance* Run = ResolveInstance(Subsystem, RunHandle);
	if (!Run)
	{
		// 代际失配（旧 run 的残留条目出队）= 时序竞态：静默丢弃（条目本该已被终结路径撤销）
		return;
	}

	// **★ 先复位自己的锚点**（本条是防 ensure 的关键，理由见 `CancelPhaseExpiry` 的注释）：
	// 条目已一次性失效（本回调正是它），若下面中途 return 也不留悬空句柄；
	// 而"最后一段到期 → 完成终结"那条路径上，终结例程看到的将是**空锚点**（而非已死的句柄）。
	Run->ExpiryEntry = FTcsTimeEntryHandle();

	const FTcsCombatEntityHandle Entity = Run->Entity;
	const int32 CurrentPhase = Run->PhaseIndex;
	const FTcsSkillEntryHandle EntryHandle = Run->EntryHandle;

	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Entity);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(Run->DefTag) : nullptr;
	if (!Def)
	{
		// 定义在施法期间被注销 ⇒ 无法判定"还有没有下一段" ⇒ 如实报出并**停止推进时段**
		// （**不在这里终结**：定义没了不等于这次施法该消失，本处只停推进，不承担终结决策）
		UE_LOG(LogTcsSkill, Warning,
			TEXT("时段推进中止：定义取不到（实体=%lld 下标=%d）——后续时段不再推进"), Entity.Id, CurrentPhase);
		return;
	}

	// ── 还有没有下一段？（判据 = 下标 + 1 是否落在表内，MUST NOT 用空表判定） ──
	const int32 NextPhase = CurrentPhase + 1;
	if (!Def->Phases.IsValidIndex(NextPhase))
	{
		// ── 最后一段到期 ⇒ **完成终结** ──
		//
		// **MUST NOT 先把 `PhaseIndex` 前进到 `Num`**：那会让它落在"已越界"的状态上，而本函数的
		// 越界分支就成了**正常路径**——"正常路径报日志"会让日志失去信号意义，也让 `PhaseTable` 档的
		// "无当前时段"降级在本该有时段时被误触发。
		// ⇒ `PhaseIndex` 停在最后一个有效下标，完成这件事由**原因**表达，不由"下标越界"表达。
		FinishSkillInstance(Subsystem, RunHandle, ECastFinishReason::ECFR_Completed);
		return;
	}

	// ① 前进 ② 发"变化"事件 ③ 起链（按档位） ④ 注册下一段
	Run->PhaseIndex = NextPhase;

	// **事件排在起链与注册之前**：注册会求值下一段的时长（可经引用类参数源，是任意代码路径），
	// 而起链会同步执行链步骤；两者都可能终结本 run 或让池搬移。
	// 事件先发还有一个语义理由：订阅者此刻读到的状态是"已进入第 N 段、尚无到期条目"——自洽。
	if (const FTcsSkillInstance* EventRun = ResolveInstance(Subsystem, RunHandle))
	{
		if (const FTcsLearnedSkillEntry* EventEntry = Bucket->Find(EntryHandle))
		{
			Broadcast(Subsystem, Tag_TcsEvent_Cast_PhaseChanged, RunHandle, *EventRun, *EventEntry);
		}
	}

	// **§5.2 `MCS_OnPhaseEnter` 档**：档位判定单点在 `StartMainChain`，本处只报"我是时段进入"。
	// **重入面（§5.6）**：配 `OnPhaseEnter` 的技能**不会**在激活时起链（档位不匹配），
	// 故与 `OnCastStarted` 不存在"同一流程里起两次"的组合——`MainChainStart` 是**单值枚举**。
	TryStartMainChainAt(
		Subsystem, RunHandle, Entity, EntryHandle, ECastChainTrigger::ECT_OnPhaseEnter);

	// 注册下一段（**排在事件与起链之后**——它写 `PhaseExpiryEntry`，而上面两步都可能终结本 run）
	if (!EnterPhase(Subsystem, RunHandle))
	{
		// 下一段排不上（定义刚被注销 / 时钟不可得 / 运行态已终结）：
		// **只有"运行态仍在册"时才补完成终结**——不这样做会留下"永不终结的在册运行态"；
		// 但若它已经不在了（上面某步已终结它），再调就是重入（会被守卫挡住，返回 false）。
		if (ResolveInstance(Subsystem, RunHandle))
		{
			FinishSkillInstance(Subsystem, RunHandle, ECastFinishReason::ECFR_Completed);
		}
	}
}
