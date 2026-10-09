// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "Parameter/TcsParamValueSource.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsCastEvents.h"
#include "Skill/TcsParamChain.h"
#include "Skill/TcsSkillRegistry.h"
#include "State/TcsStateParamTableReader.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"
#include "TcsStateSubsystem.h"



// 内核
void FTcsCastOps::MakeContext(
	FTcsParamEvaluateContext& OutContext,
	FTcsCombatEntityHandle Unit,
	int32 Level)
{
	// 本轮无独立的发起者参数 ⇒ 主体与发起者同为施法单位（同状态侧"无效时取被施加方"的缺省口径）
	OutContext.Subject = Unit;
	OutContext.Instigator = Unit;
	OutContext.EffectiveLevel = Level;

	// 本轮参数表恒空：技能侧引用类参数源（`ParamRef`）在快照构建期间没有"更外层参数表"
	// （链侧参数表载体不存在，同 `TcsStepApplyState` 的既有边界）⇒ 落兜底
	OutContext.ParamTable = TScriptInterface<ITcsParamTableReader>();
}

FTcsCastRun* FTcsCastOps::ResolveRun(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	// **先判有效性再取指针**：池的 `Resolve` 对悬空句柄会 **ensure**（它是"调用方给错句柄"的
	// 契约违规口径），而本族的查询面 MUST 走"时序竞态静默返回 nullptr"的语义 ⇒ 必须用非确保的
	// `IsValid` 先挡一道。
	const TTcsInstanceHandle<FTcsCastRunTag> Inner = RunHandle.GetInner();
	if (!Subsystem.CastRuns.IsValid(Inner))
	{
		return nullptr;
	}

	return Subsystem.CastRuns.Resolve(Inner);
}

FTcsCastRunHandle FTcsCastOps::FindActiveRun(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry)
{
	for (const FTcsCastRunHandle& Candidate : Entry.RunHandles)
	{
		// 账本可能残留已归还的陈旧句柄 ⇒ 逐个经池复核（失败 = 视为无在飞，**不 ensure**）
		if (ResolveRun(Subsystem, Candidate))
		{
			return Candidate;
		}
	}

	return FTcsCastRunHandle();
}

void FTcsCastOps::Broadcast(
	UTcsSkillSubsystem& Subsystem,
	FGameplayTag EventTag,
	FTcsCastRunHandle RunHandle,
	const FTcsCastRun& Run,
	const FTcsLearnedSkillEntry& Entry)
{
	UTcsEventBusSubsystem* Bus = Subsystem.GetEventBus();
	if (!Bus)
	{
		// 世界拆解期（总线已回收）——时序而非配置错误，故 Warning 不 ensure
		UE_LOG(LogTcsSkill, Warning, TEXT("施法事件丢弃：事件总线不可得（Tag=%s 定义=%s）"),
			*EventTag.ToString(), *Entry.DefTag.ToString());
		return;
	}

	// 载荷装配（**唯一出口**——全部广播点共用本函数，MUST NOT 各配一份）
	FTcsCastEventPayload Payload;
	Payload.Run = RunHandle;
	Payload.Unit = Run.Unit;
	Payload.DefTag = Entry.DefTag;
	Payload.EntryHandle = Entry.Handle;
	Payload.Level = Run.Level;

	// **★ 广播前把日志要用的读数先拷成值**（2026-10-08 静态自查抓到的一处真实悬空引用）：
	// `PublishImmediate` 是**当场同步派发** ⇒ 订阅者可在其中注销单位（桶被删 ⇒ `Entry` 悬空）
	// 或**再起一次激活**（施法池扩容搬移 ⇒ `Run` 悬空）。
	// ⇒ 本函数在 `PublishImmediate` **之后** MUST NOT 再经 `Entry` / `Run` 取任何东西。
	// （载荷本身已在上面装配完毕、是值拷贝，**不受影响**——这正是"先装配后发布"的价值）
	//
	// **命名注（编译实测订正）**：这些局部量 MUST NOT 叫 `LogLevel` —— 它会与 UE 的全局枚举值
	// `ELogVerbosity::LogLevel` 撞名，MSVC 报 **C4459: declaration of 'LogLevel' hides global
	// declaration**（本仓零 warning 门槛 ⇒ 直接算失败）。故一律带 `Broadcast` 前缀区分。
	const FGameplayTag BroadcastDefTag = Entry.DefTag;
	const int64 BroadcastUnitId = Run.Unit.Id;
	const int32 BroadcastRunIndex = RunHandle.Index;
	const int32 BroadcastRunGeneration = RunHandle.Generation;
	const int32 BroadcastLevel = Run.Level;

	FInstancedStruct PayloadStruct;
	PayloadStruct.InitializeAs<FTcsCastEventPayload>(Payload);

	// 立即通道：同一提交内到达——订阅者可在本次调用返回前读到运行态
	// （**"先广播后归还槽位"的纪律**：`TerminateRun` 的广播排在 `Free` 之前，正因如此载荷里的
	// 运行句柄在回调内仍然有效）
	Bus->PublishImmediate(EventTag, PayloadStruct);

	// 日志只用上面拷好的值（**MUST NOT** 在这里碰 `Entry` / `Run`）
	UE_LOG(LogTcsSkill, Verbose, TEXT("施法事件派发：Tag=%s run=%d/%d 单位=%lld 定义=%s 等级=%d"),
		*EventTag.ToString(), BroadcastRunIndex, BroadcastRunGeneration,
		BroadcastUnitId, *BroadcastDefTag.ToString(), BroadcastLevel);
}



// 激活
ESkillActivateResult FTcsCastOps::Activate(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FGameplayTag DefTag,
	FTcsCastRunHandle* OutRun)
{
	// ── 门禁① 实体 Ready ────────────────────────────────────────────────
	// 判据 = 注入的 `ITcsEntityQuery::IsEntityReady`（**不是** `IsAlive`——两者正交：
	// 前者答"框架能不能在它身上操作"、后者答"宿主认为它活着吗"；拿后者当门禁会双向误判）。
	// 未注入实现时门面已按既有语义**降级放行**（只判句柄有效）。
	if (!Subsystem.IsEntityReady(Unit))
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[实体未就绪]：单位=%lld 定义=%s"),
			Unit.Id, *DefTag.ToString());
		return ESkillActivateResult::SAR_EntityNotReady;
	}

	// ── 门禁② 已学 ──────────────────────────────────────────────────────
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->FindByDefTag(DefTag) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[未学该技能]：单位=%lld 定义=%s"),
			Unit.Id, *DefTag.ToString());
		return ESkillActivateResult::SAR_NotLearned;
	}

	// ── 门禁⑥ 定义可解析（**评估可达性约束**：第 ③④⑤ 道都要读定义内容 ⇒ 必须在它们之前判） ──
	// 若排在后面，第 ③④⑤ 道会拿到"取不到定义"的空数据并返回**看似合理但错误**的枚举值（假读数）。
	// **指针可变**：顶替路径的终结例程会广播，订阅者可在其中注销定义 ⇒ 之后 MUST 重新解析
	// （见下方顶替分支的"三样都要重新定位"注）。
	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsSkill, Warning,
			TEXT("技能激活被拒[定义无效]：单位=%lld 定义=%s 未在本世界登记（条目存在但定义已注销）"),
			Unit.Id, *DefTag.ToString());
		return ESkillActivateResult::SAR_DefInvalid;
	}

	// ── 门禁③ 冷却（**R6.5 占位**：本轮判据 = 空轨道恒可放） ──────────────
	// **MUST NOT** 引用任何尚未定名的枚举值（R6.5 的定义尚在概念期）——按语义描述即可。

	// ── 门禁④ Instancing 四路分流 ──────────────────────────────────────
	const FTcsCastRunHandle ActiveRun = FindActiveRun(Subsystem, *Entry);
	if (ActiveRun.IsValid() && Def->Instancing == ECastInstancing::CI_InstancePerEntity)
	{
		if (!Def->bRetriggerOnActive)
		{
			// 分流 a：不顶替 ⇒ 驳回（**纯查询语义：不建 run、不改任何状态**）
			UE_LOG(LogTcsSkill, Log,
				TEXT("技能激活被拒[已在飞行中]：单位=%lld 定义=%s 在飞 run=%d/%d（顶替位为假）"),
				Unit.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
			return ESkillActivateResult::SAR_AlreadyActive;
		}

		// 分流 b：顶替 —— 先查旧 run 当前时段的可打断性。
		// **Task 3 的判据 = `bInterruptibleDefault`**（`ECastQueryMode::DefSwitches` 档，查询契约的
		// **默认档**，Task 1 已交付 ⇒ 非临时实现）。Task 5 落地 `IsInterruptibleNow()` 后改调它，
		// 而 DefSwitches 档读的就是该字段 ⇒ **行为逐字不变、零回归**。
		// **MUST NOT** 在此预建三档分派（`PhaseTable` 需时段推进、`Custom` 需 Fragment 求值）。
		if (!Def->bInterruptibleDefault)
		{
			UE_LOG(LogTcsSkill, Log,
				TEXT("技能激活被拒[已在飞行中]：单位=%lld 定义=%s 顶替被拒（当前时段不可打断，run=%d/%d）"),
				Unit.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
			return ESkillActivateResult::SAR_AlreadyActive;
		}

		// 顶替：终止旧 run——**统一走终结例程**（发打断事件、摘账本句柄、归还池槽位）
		UE_LOG(LogTcsSkill, Log, TEXT("技能顶替：单位=%lld 定义=%s 终止旧 run=%d/%d"),
			Unit.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
		TerminateRun(Subsystem, ActiveRun);

		// **重入纪律（三样都要重新定位，2026-10-08 静态自查补齐）**：终结例程会广播
		// （`PublishImmediate` 当场同步派发）⇒ 订阅者可在其中：
		// ① 撤销条目 / 注销单位 ⇒ 上面的桶与条目指针**失效**；
		// ② **注销技能定义**（`UnregisterSkillDef`）⇒ 上面拿到的 `Def` 指针**悬空**（登记表是
		//    `TUniquePtr` 持有 ⇒ 指针在**在册期间**稳定，但注销即析构 —— 本条是**use-after-free**）。
		// ⇒ 两者都 MUST 重新解析后再继续（**"任何可能广播的调用之后，手里的指针都可能失效"**）。
		Bucket = Subsystem.Registry.FindBucket(Unit);
		Entry = Bucket ? Bucket->FindByDefTag(DefTag) : nullptr;
		if (!Entry)
		{
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：顶替期间条目被撤销（单位=%lld 定义=%s）"), Unit.Id, *DefTag.ToString());
			return ESkillActivateResult::SAR_NotLearned;
		}

		Def = Subsystem.GetRegisteredSkillDef(DefTag);
		if (!Def)
		{
			// 定义在顶替广播期间被注销 ⇒ 与门禁⑥ 同一语义（取不到定义内容即不激活）
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：顶替期间定义被注销（单位=%lld 定义=%s）"),
				Unit.Id, *DefTag.ToString());
			return ESkillActivateResult::SAR_DefInvalid;
		}
	}

	// ── 门禁⑤ CanAfford（**R6.5 占位**：本轮判据 = 不消耗档恒可付） ────────

	// ── 门禁全过：建运行态 ──────────────────────────────────────────────
	const TTcsInstanceHandle<FTcsCastRunTag> Inner = Subsystem.CastRuns.Allocate();
	FTcsCastRun* Run = Subsystem.CastRuns.Resolve(Inner);
	if (!Run)
	{
		// 分配后必然可解析（同一次调用内无释放）——防御性返回
		UE_LOG(LogTcsSkill, Warning, TEXT("技能激活异常：新分配运行态无法解析"));
		return ESkillActivateResult::SAR_DefInvalid;
	}

	FTcsCastRunHandle RunHandle;
	RunHandle.SetInner(Inner);

	// **先整体重置再用**（2026-10-08 静态自查抓到的一处真实潜伏缺陷）：
	// `TTcsInstancePool::Allocate` 复用空闲槽时**只改代际、不重建元素**（池的零策略纪律明文
	// "Free 不清零槽位内容"）⇒ 若逐个字段赋值，**任何未显式赋值的字段都会带上"上一任占用者"的值**。
	// 本轮就有一个：`PhaseExpiryEntry`（Task 3 零写入者）会留着上一个 run 的到期句柄——今天无害
	// （无读取者），但 **Task 5 一旦加上读取者，就会把"上一任的陈旧到期条目"当成自己的**，
	// 症状是"莫名取消了别人的到期回调"或"自己的时段永不推进"（远离根因的一类）。
	// ⇒ 纪律：**复用池槽后 MUST 先整体重置**（而非逐字段赋值）——这样**将来给 `FTcsCastRun`
	// 加字段时自动是干净的**，不依赖"加字段的人记得来补一行"。
	*Run = FTcsCastRun();

	Run->EntryHandle = Entry->Handle;
	Run->Unit = Unit;
	Run->Level = Entry->Level;
	Run->PhaseIndex = 0;
	Run->ChainRunHandle = FTcsChainRunHandle();

	// 来源锚点：每次运行一枚（`FTcsSourceHandleRegistry` 进程唯一、永不复用）
	Run->RunSource = Subsystem.SourceRegistry.Allocate();

	// **回填与摘除成对**：建 run 成功后回填账本条目；终结时（`TerminateRun`）摘除。
	// 只写不摘 ⇒ 账本永久持有已归还的句柄（表现为"技能显示在飞但实际放完了"，静默错误）。
	Entry->RunHandles.Add(RunHandle);

	// ── 激活期参数快照（**时序：门禁全过之后、任何后续读取之前**） ──────────
	// **MUST NOT 在门禁之前构建**（门禁失败即白算一份快照）。
	//
	// **快照的用途（2026-10-09 用户裁定 A′，本注是防误读的唯一说明）**：它是**求值期的参数表载体**
	// ——物化 `ParamChainRows` 时，引用类操作数（`ParamRef`）即从它取键值（先例 = 状态侧的
	// `FTcsStateModifierMaterializer` 把 `Instance.ParamSnapshot` 经 `FTcsStateSnapshotScope`
	// 装进 `Ctx.ParamTable`）。**它不是"公共读口的冻结来源"**：`GetNumericParam` 走实时通道
	// （参数行初值 + 账本槽位折叠），MUST NOT 被读成"读它拿冻结值"。
	{
		FTcsParamEvaluateContext Ctx;
		MakeContext(Ctx, Unit, Entry->Level);

		// 本轮无覆盖值来源（`TryActivate` 不收施法上下文形参）⇒ 传空表
		static const TMap<FGameplayTag, double> EmptyOverrides;
		BuildSkillSnapshot(Run->ParamSnapshot, *Def, Ctx, EmptyOverrides);
	}

	// ── 激活期物化定义自带的参数链行（PV-9） ─────────────────────────────
	//
	// **业务上这一步在做什么**：策划在技能资产上写的那些"算式行"（比如"攻击力 × 倍率"），
	// 是在**这里**第一次真正被算出来、并落到这个技能的账本上。在此之前它们只是配置数据。
	// 算出来的值在本次施法期间**固定**（放第二次技能会重算，且上一条痕迹已清干净）。
	//
	// **为什么必须先绑参数表**：算式里凡是用「参数引用」取值的行，都要去**本技能参数表**里
	// 查键（比如"倍率"这一项）。而这张表就是**本次施法快照**。不绑 ⇒ 引用取不到值、
	// **静默落各自兜底**（不报错、不崩溃，只是数值悄悄不对）。
	//
	// 实现要点：`Source` 一律 = **本 run 的 `RunSource`** ⇒ 随施法终结按来源一次摘净。
	// 作用域覆盖整段物化（退出时按栈恢复上一份绑定——嵌套天然正确，同状态侧先例）。
	// 状态门面不可得（世界拆解 / 单测无状态门面）时退化为"无参数表"：引用类源落各自兜底，
	// **不 ensure、不红字**（与 `UTcsStateParamTableReader` 可空的口径一致）。
	{
		UTcsStateSubsystem* StateSubsystem = Subsystem.GetWorld()
			? Subsystem.GetWorld()->GetSubsystem<UTcsStateSubsystem>() : nullptr;

		FTcsStateSnapshotScope Scope(
			StateSubsystem ? StateSubsystem->GetParamTableReader() : nullptr,
			&Run->ParamSnapshot);

		FTcsParamChainOps::MaterializeParamChainRows(
			Subsystem, *Def, *Entry, Scope.GetTable(), Run->RunSource);
	}

	// ── 生效等级（`Level` 键的接线：重算并写回 run） ──────────────────────
	//
	// **业务上这一步在做什么**：策划可以在参数链行里填 `Level` 键来改**技能生效等级**
	// （"某天赋让这个技能等级 +2"）。上一步刚把那些行落进账本，所以**必须排在它之后**算，
	// 否则会漏掉它们。等级定了之后本次施法不变（术语："运行中升级不追溯"——中途升级
	// 只会改下次施法，不会改正在飞的那一发）。
	//
	// 实现要点：`EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`。
	// **如实登记的边界**：本步是**单趟**——`Level` 键自身若配"按等级取档"的数值源，它读到的
	// 是折叠前的等级（`Entry->Level`）；等级修正链套等级链不存在于设计语料，不为其预建多趟收敛。
	if (FTcsCastRun* LevelRun = ResolveRun(Subsystem, RunHandle))
	{
		LevelRun->Level = FTcsParamChainOps::EvaluateEffectiveLevel(Subsystem, *Entry, Def);
	}

	// **起链前的读数先拷出来**：起链会同步执行链步骤，而链步骤可改状态/属性并广播 ⇒
	// 之后 MUST NOT 再假定 `Run` 与 `Entry` 指针有效（池扩容即搬移；广播可重入撤销条目或注销单位）。
	// **账本条目是纯数据**（无 UObject、无自引用）⇒ 直接**按值拷一份**即可安全跨过起链。
	const int32 SnapshotLevel = Run->Level;
	const FTcsSourceHandle SnapshotRunSource = Run->RunSource;
	const FTcsLearnedSkillEntry EntryCopy = *Entry;

	// ── 主链起链（按 `MainChainStart`；本 Task 只实现 `OnCastStarted` 档） ──
	// **`Run` 传 const 引用**：起链可能经"链步骤再激活"让池扩容搬移 ⇒ 本函数内 MUST NOT 写它。
	// 结果经 `ChainRun` 交回，下面**按句柄重新解析后**再回写（`Run` 此刻可能已失效）。
	FTcsChainRunHandle ChainRun;
	StartMainChain(Subsystem, *Run, *Def, *Entry, ChainRun);

	// **按句柄回写链句柄**（`Run` 指针在起链后可能已失效 ⇒ 必须重新解析；解析不到即条目/运行态
	// 已在起链期间被终结，此时不回写——那是正确的，run 都不在了）
	if (FTcsCastRun* LiveRun = ResolveRun(Subsystem, RunHandle))
	{
		LiveRun->ChainRunHandle = ChainRun;
	}

	UE_LOG(LogTcsSkill, Log, TEXT("技能激活成功：单位=%lld 定义=%s 条目=%d/%d run=%d/%d 等级=%d 来源=%llu"),
		Unit.Id, *DefTag.ToString(), EntryCopy.Handle.Index, EntryCopy.Handle.Generation,
		RunHandle.Index, RunHandle.Generation, SnapshotLevel, SnapshotRunSource.Id);

	if (OutRun)
	{
		*OutRun = RunHandle;
	}

	// ── 发 `OnCastStarted`（**最后一步**：起链可能已产生副作用与广播，事件排在它们之后） ──
	// `Run` 指针在起链后可能已失效 ⇒ **按句柄重新解析**；载荷的账本面用上面那份**按值拷贝**
	// （条目若在起链期间被撤销，本次激活的事实仍然成立——run 已建、事件该发，只是身份取自拷贝）。
	if (const FTcsCastRun* LiveRun = ResolveRun(Subsystem, RunHandle))
	{
		Broadcast(Subsystem, Tag_TcsEvent_Cast_Started, RunHandle, *LiveRun, EntryCopy);
	}

	return ESkillActivateResult::SAR_Success;
}



// 运行态生命周期
bool FTcsCastOps::TerminateRun(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	FTcsCastRun* Run = ResolveRun(Subsystem, RunHandle);
	if (!Run)
	{
		// 悬空句柄 = 时序竞态（调用方手上的句柄过期），不是契约违规 ⇒ 静默 false，不 ensure
		return false;
	}

	// **先把终结所需的数据拷出来再动池**：下面要归还槽位，而池操作可能搬移元素
	const FTcsCastRun RunCopy = *Run;
	const FTcsSkillEntryHandle EntryHandle = RunCopy.EntryHandle;
	const FTcsCombatEntityHandle Unit = RunCopy.Unit;

	// ① 摘账本条目里的在飞句柄（**与回填成对**）。条目可能已被撤销 ⇒ 静默跳过，不 ensure。
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (Entry)
	{
		Entry->RunHandles.Remove(RunHandle);

		// ②' **按 `RunSource` 级联摘除本 run 物化的参数链行**（`add-skill-param-chain` 的明文条款：
		// "物化时 `Source` MUST = 本施法运行态来源句柄 ⇒ 随施法终结**级联摘除**，生命周期语义与基类
		// `ModifierRows` 同款"）。
		//
		// **业务上不摘会怎样（这是玩家能直接感觉到的错）**：物化是"每次激活都追加"（同一技能
		// 可先后放多次，各带自己的 `RunSource`）；终结时不摘 ⇒ 加成**逐次累积**，表现为
		// **"放两次技能，参数翻倍"**。而且它是**静默**的——只放一次技能时读数完全正常，
		// 所以"只验一次激活"的检查必漏（本处正是我 Task 3 漏读规格原文留下的洞，Task 4 补上）。
		// 摘除按来源全量命中 ⇒ 只摘本次 run 那一批，其它 run 的条目原样保留。
		//
		// **与 Task 5 的分工（MUST NOT 混淆）**：这里摘的是**技能自己定义侧物化的行**
		// （`SkillDef.ParamChainRows`，落账本时来源就是 `RunSource`）。而台账 `CHAIN-7` 的
		// "链挂条目回收"（链步骤经 `Context.RunSource` 挂到 M2 属性账本的那些修正器）仍归
		// **Task 5 Step 3 的施法终结三路统一回收例程**——两者来源句柄相同、但**账本不同**
		// （一个在 M5 技能账本、一个在 M2 属性账本），互不代劳。
		FTcsParamChainOps::RemoveParamModifiersBySource(Subsystem, Unit, RunCopy.RunSource);

		// ② 广播**打断**事件（**MUST NOT** 广播完成事件——顶替不是完成：照抄 GAS 会让配
		// `OnCastCompleted` 起链的旧技能在被顶替的瞬间真的打出主链，即玩家连点两下、
		// 第一下的效果仍兑现）。**广播排在归还槽位之前** ⇒ 载荷里的运行时句柄在回调内仍有效。
		Broadcast(Subsystem, Tag_TcsEvent_Cast_Interrupted, RunHandle, RunCopy, *Entry);
	}

	// ③ 归还池槽位（此后旧句柄凭代际失配判悬空）
	//
	// **★ 归还前 MUST 复核句柄仍有效（重入窗口，2026-10-08 静态自查补入）**：
	// 上面的广播是**当场同步派发** ⇒ 订阅者**可以在回调里重入 `TerminateCastRun(同一句柄)`**。
	// 若不复核，第二次 `Free` 会命中池的 `ensureMsgf`（悬空句柄）⇒ **留红字**，破坏
	// "零 ensure / 零意外红字"这条验收信号。
	// ⇒ 本处用**非确保**的 `IsValid` 先挡一道（复核失败 = 已在广播期间被重入终结 ⇒ 静默跳过）。
	//
	// **★ 同形先例与它今天的真实状态（2026-10-08 订正；本注原稿有误，见下）**：
	// 状态侧曾有**完全同形**的缺陷 —— **`STAT-5`**："`Removed` 广播与归还槽位之间重入移除同一句柄
	// ⇒ 双次归还槽位"。**该缺陷已于 R5 Task 6 闭合**，其修法比我这里做的**更彻底**：
	// `UTcsStateSubsystem::InRemovalBroadcastHandles`（`TcsStateSubsystem.h:414`，进出广播窗口成对
	// 标记/解除，见 `TcsStateOps.cpp:188/244/247`）⇒ 递归 `Remove` 同句柄**返回 false 并静默**，
	// **既不重复广播、也不重复归还槽位**。**⇒ 本处只挡住了"双次归还"这一半，MUST NOT 被读成
	// "与状态侧同等彻底"。**
	//
	// **如实登记的残留边界（MUST NOT 读成已彻底解决）**：本复核只消除**双次归还**；**重入导致的
	// "打断事件发两次"仍然存在**（外层发一次、重入的内层再发一次）。
	// **它的归属 = `PLN-R6` Task 5 Step 3 的"施法终结三路统一回收例程"**（届时一并裁重入语义，
	// 并参考状态侧 `InRemovalBroadcastHandles` 的体例——那是本仓已验证过的同一问题解法）。
	// **★ 本注原稿的两处失真已订正，留痕以免后人再抄错**：① 原写"状态侧已有**已登记**缺陷 `STAT-5`"
	// ——`STAT-5` 当时**已闭合**，"已登记/在册"是错的；② 原写"**已随本 Task 在台账登记**"——
	// **台账里并无此条**（65 条全为旧条目，"打断事件发两次"/"TcsCastOps" 在台账零命中），
	// 且按台账《入册判据》第 3 条"**不在任何现有计划的 Task 里**"，本条**有 Task 5 归属 ⇒ 本就不该入册**。
	// ⇒ **准确说法 = 登记在计划的 Task 5 Step 3，不在台账。**
	const TTcsInstanceHandle<FTcsCastRunTag> FreeInner = RunHandle.GetInner();
	if (Subsystem.CastRuns.IsValid(FreeInner))
	{
		Subsystem.CastRuns.Free(FreeInner);
	}
	else
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("施法终结：归还槽位跳过（run=%d/%d 已在广播期间被重入终结）"),
			RunHandle.Index, RunHandle.Generation);
	}

	// ④ 链挂条目的回收（**台账 `CHAIN-7` 的闭合动作**）归 **Task 5 Step 3** 的"施法终结三路统一回收例程"
	// ——本 Task 只落 `RunSource` 锚点与它的传递（`Context.RunSource`）。**MUST NOT** 在此提前实现，
	// 否则 Task 5 会面对"两个回收点"（三路统一回收的前提是终结点唯一）。

	UE_LOG(LogTcsSkill, Log, TEXT("施法终结：单位=%lld 条目=%d/%d run=%d/%d 来源=%llu（已发打断事件、已归还槽位）"),
		Unit.Id, EntryHandle.Index, EntryHandle.Generation,
		RunHandle.Index, RunHandle.Generation, RunCopy.RunSource.Id);

	return true;
}

int32 FTcsCastOps::ResetAll(UTcsSkillSubsystem& Subsystem)
{
	const int32 CountBefore = CountRuns(Subsystem);

	// 池的零策略纪律：不逐槽清理实例内容（`Reset` 使全部旧句柄代际失配）
	Subsystem.CastRuns.Reset();

	return CountBefore;
}

FTcsCastRun* FTcsCastOps::Find(UTcsSkillSubsystem& Subsystem, FTcsCastRunHandle RunHandle)
{
	return ResolveRun(Subsystem, RunHandle);
}

int32 FTcsCastOps::CountRuns(UTcsSkillSubsystem& Subsystem)
{
	int32 Count = 0;
	Subsystem.CastRuns.ForEach([&Count](FTcsCastRun&)
	{
		++Count;
	});

	return Count;
}
