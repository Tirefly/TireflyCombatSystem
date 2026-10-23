// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCastOps.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "Parameter/TcsParamValueSource.h"

#include "Def/TcsSkillDefData.h"
#include "Skill/TcsCastEvents.h"
#include "Skill/TcsCooldownOps.h"
#include "Skill/TcsCostOps.h"
#include "Skill/TcsParamChain.h"
#include "Skill/TcsSkillOps_Relation.h"
#include "Skill/TcsSkillRegistry.h"
#include "Runtime/TcsEntityAdmission.h"
#include "State/TcsStateLiveQuery.h"
#include "State/TcsStateParamTableReader.h"
#include "TcsAttributeSubsystem.h"
#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"
#include "TcsStateSubsystem.h"



// 内核
void FTcsCastOps::MakeContext(
	FTcsParamEvaluateContext& OutContext,
	FTcsCombatEntityHandle Entity,
	int32 Level)
{
	// 本轮无独立的发起者参数 ⇒ 主体与发起者同为施法实体（同状态侧"无效时取被施加方"的缺省口径）
	OutContext.Subject = Entity;
	OutContext.Instigator = Entity;
	OutContext.EffectiveLevel = Level;

	// 本轮参数表恒空：技能侧引用类参数源（`ParamRef`）在快照构建期间没有"更外层参数表"
	// （链侧参数表载体不存在，同 `TcsStepApplyState` 的既有边界）⇒ 落兜底
	OutContext.ParamTable = TScriptInterface<ITcsParamTableReader>();
}

FTcsSkillInstance* FTcsCastOps::ResolveInstance(UTcsSkillSubsystem& Subsystem, FTcsSkillInstanceHandle RunHandle)
{
	// **先判有效性再取指针**：池的 `Resolve` 对悬空句柄会 **ensure**（它是"调用方给错句柄"的
	// 契约违规口径），而本族的查询面 MUST 走"时序竞态静默返回 nullptr"的语义 ⇒ 必须用非确保的
	// `IsValid` 先挡一道。
	const TTcsInstanceHandle<FTcsSkillInstanceTag> Inner = RunHandle.GetInner();
	if (!Subsystem.SkillInstances.IsValid(Inner))
	{
		return nullptr;
	}

	return Subsystem.SkillInstances.Resolve(Inner);
}

FTcsSkillInstanceHandle FTcsCastOps::FindActiveInstance(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry)
{
	for (const FTcsSkillInstanceHandle& Candidate : Entry.RunHandles)
	{
		// 账本可能残留已归还的陈旧句柄 ⇒ 逐个经池复核（失败 = 视为无在飞，**不 ensure**）
		if (ResolveInstance(Subsystem, Candidate))
		{
			return Candidate;
		}
	}

	return FTcsSkillInstanceHandle();
}

void FTcsCastOps::Broadcast(
	UTcsSkillSubsystem& Subsystem,
	FGameplayTag EventTag,
	FTcsSkillInstanceHandle RunHandle,
	const FTcsSkillInstance& Run,
	const FTcsLearnedSkillEntry& Entry,
	ECastFinishReason Reason)
{
	UTcsEventBusSubsystem* Bus = Subsystem.GetEventBus();
	if (!Bus)
	{
		// 世界拆解期（总线已回收）——时序而非配置错误，故 Warning 不 ensure
		UE_LOG(LogTcsSkill, Warning, TEXT("施法事件丢弃：事件总线不可得（Tag=%s 定义=%s）"),
			*EventTag.ToString(), *Run.DefTag.ToString());
		return;
	}

	// 载荷装配（**唯一出口**——全部广播点共用本函数，MUST NOT 各配一份）
	FTcsCastEventPayload Payload;
	Payload.Run = RunHandle;
	Payload.Entity = Run.Entity;
	Payload.DefTag = Run.DefTag;
	Payload.EntryHandle = Entry.Handle;
	Payload.Level = Run.Level;
	// 终结原因（2026-10-16 R5.5-e1）：四枚共用同一形状 ⇒ 恒有值，见载荷字段注释
	Payload.Reason = Reason;

	// **★ 广播前把日志要用的读数先拷成值**（2026-10-08 静态自查抓到的一处真实悬空引用）：
	// `PublishImmediate` 是**当场同步派发** ⇒ 订阅者可在其中注销实体（桶被删 ⇒ `Entry` 悬空）
	// 或**再起一次激活**（施法池扩容搬移 ⇒ `Run` 悬空）。
	// ⇒ 本函数在 `PublishImmediate` **之后** MUST NOT 再经 `Entry` / `Run` 取任何东西。
	// （载荷本身已在上面装配完毕、是值拷贝，**不受影响**——这正是"先装配后发布"的价值）
	//
	// **命名注（编译实测订正）**：这些局部量 MUST NOT 叫 `LogLevel` —— 它会与 UE 的全局枚举值
	// `ELogVerbosity::LogLevel` 撞名，MSVC 报 **C4459: declaration of 'LogLevel' hides global
	// declaration**（本仓零 warning 门槛 ⇒ 直接算失败）。故一律带 `Broadcast` 前缀区分。
	const FGameplayTag BroadcastDefTag = Run.DefTag;
	const int64 BroadcastEntityId = Run.Entity.Id;
	const int32 BroadcastRunIndex = RunHandle.Index;
	const int32 BroadcastRunGeneration = RunHandle.Generation;
	const int32 BroadcastLevel = Run.Level;

	FInstancedStruct PayloadStruct;
	PayloadStruct.InitializeAs<FTcsCastEventPayload>(Payload);

	// 立即通道：同一提交内到达——订阅者可在本次调用返回前读到运行态
	// （**"先广播后归还槽位"的纪律**：`TerminateInstance` 的广播排在 `Free` 之前，正因如此载荷里的
	// 运行句柄在回调内仍然有效）
	Bus->PublishImmediate(EventTag, PayloadStruct);

	// 日志只用上面拷好的值（**MUST NOT** 在这里碰 `Entry` / `Run`）
	UE_LOG(LogTcsSkill, Verbose, TEXT("施法事件派发：Tag=%s run=%d/%d 实体=%lld 定义=%s 等级=%d"),
		*EventTag.ToString(), BroadcastRunIndex, BroadcastRunGeneration,
		BroadcastEntityId, *BroadcastDefTag.ToString(), BroadcastLevel);
}



// 激活
ESkillActivateResult FTcsCastOps::Activate(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Entity,
	FGameplayTag DefTag,
	FTcsSkillInstanceHandle* OutRun)
{
	// ── 门禁① 实体 Ready ────────────────────────────────────────────────
	// 判据 = 注入的 `ITcsEntityQuery::IsEntityReady`（**不是** `IsAlive`——两者正交：
	// 前者答"框架能不能在它身上操作"、后者答"宿主认为它活着吗"；拿后者当门禁会双向误判）。
	// 未注入实现时门面已按既有语义**降级放行**（只判句柄有效）。
	if (!Subsystem.IsEntityReady(Entity))
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[实体未就绪]：实体=%lld 定义=%s"),
			Entity.Id, *DefTag.ToString());
		return ESkillActivateResult::SAR_EntityNotReady;
	}

	// ── 门禁② 已学 ──────────────────────────────────────────────────────
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Entity);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->FindByPredicate([DefTag](const FTcsLearnedSkillEntry& Item)
	{
		// "同一实体会同一个技能"至多一条（授予路径按 `DefTag` 刷新而非新建）⇒ 先命中即唯一
		return Item.DefTag == DefTag;
	}) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[未学该技能]：实体=%lld 定义=%s"),
			Entity.Id, *DefTag.ToString());
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
			TEXT("技能激活被拒[定义无效]：实体=%lld 定义=%s 未在本世界登记（条目存在但定义已注销）"),
			Entity.Id, *DefTag.ToString());
		return ESkillActivateResult::SAR_DefInvalid;
	}

	/**
	 * **本条目句柄的稳定身份（跨广播安全）**——下面三处**新增的同步广播点**（成本提交的
	 * `Attribute.ValueChanged` + `CostApplied`、起冷的 `CooldownStarted`）之后都要用它
	 * **重新解析 `Entry` / `Def`**，因为订阅者可在回调里撤销条目（释放桶）、
	 * `GrantSkill`（`Entries.Emplace()` 搬移）或注销定义（析构登记表项）。
	 *
	 * **为什么是"值"而不是指针**：句柄是 `{Index, Generation}` 两个整数 ⇒ 跨任何广播都安全；
	 * 而 `Entry` / `Def` 都是**指向可能被释放/搬移的容器元素**的指针。
	 * **一条纪律**：本函数内**凡经过可能广播的调用之后**，MUST 用本句柄重新解析，
	 * MUST NOT 沿用之前的 `Entry` / `Def`（该纪律的原文见下方"起链前的读数先拷出来"那段）。
	 */
	const FTcsSkillEntryHandle EntryHandleForResolve = Entry->Handle;

	// ── 门禁⑦ 关系门（2026-10-16 `R5.5-e1` 新增；**评估点在 ⑥ 之后**，但**枚举值序解耦**取 7） ──
	//
	// **为什么排在这里**：关系表的三道判定（`RequiredTags` / `ForbiddenTags` / 施动侧反向查找）
	// 都要读**本定义的内容**与**跨域的在跑状态** ⇒ MUST 在 ⑥（定义可解析）之后，
	// 否则会拿到空定义判出"看似合理但错误"的结果（同 ⑥ 那条"评估可达性"判据）。
	//
	// **跨域一步经共享层契约**（`ITcsStateLiveQuery`）：`TcsSkill` MUST NOT 依赖 `TcsBuff`，
	// 而本判定要读状态域的在跑实例 ⇒ 走两域共同下层。**契约未注册时降级放行 + 具名 Warning**
	// （取舍见 `ITcsStateLiveQuery` 类注释：缺注册不该变成随机的业务失败）。
	//
	// **拒绝路径零副作用**（同本条既有纪律）：不建 run、不改账本、不起链、不发事件。
	{
		ITcsStateLiveQuery* LiveQuery = nullptr;
		if (UTcsStateSubsystem* StateSubsystem = Subsystem.GetWorld() ? Subsystem.GetWorld()->GetSubsystem<UTcsStateSubsystem>() : nullptr)
		{
			LiveQuery = StateSubsystem->GetLiveQuery();
		}

		if (!FTcsSkillOps_Relation::CheckRelationGate(LiveQuery, Entity, DefTag, *Def))
		{
			return ESkillActivateResult::SAR_RelationBlocked;
		}
	}

	// ── 门禁③ 冷却（`D5-15`："可施放 = 全部自身轨道清空 + 所属组槽清空"） ──────────────
	//
	// **判据的唯一实现在 `FTcsCooldownOps::IsEntryOnCooldown`**（MUST NOT 在此内联一份判断）：
	// 同一个判据还有"到期 / 事件 / 手动"三个**提交点**要各查一次（TOCTOU 防护），
	// 各写一份必然漂移——而漂移的后果是"门禁说能放、提交点说不能"这类自相矛盾。
	//
	// **为什么门禁恒在这里、不挪去提交点**：本道答的是"**现在**放不放得出来"，
	// 而宿主从返回值里就能看出原因（R6 已为此落了具名枚举 `SAR_OnCooldown`）。
	// 提交点那次重查是**补充**（门禁通过到提交点之间资源/冷却可能已变），不是替代。
	//
	// **空 `Tracks` ⇒ 恒可放**（`D5-15`："默认无冷却"）——判据落在被调方，本处不重复判。
	{
		FString HeldBy;
		if (FTcsCooldownOps::IsEntryOnCooldown(Subsystem, *Entry, *Def, &HeldBy))
		{
			// 日志 MUST 含**滞留在哪一轨 / 哪个组**——那是宿主排查"为什么放不出来"最需要的读数，
			// 只说"在冷却"等于没说。
			UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[冷却中]：实体=%lld 定义=%s 滞留=%s"),
				Entity.Id, *DefTag.ToString(), *HeldBy);
			return ESkillActivateResult::SAR_OnCooldown;
		}
	}

	// ── 门禁④ Instancing —— 判定经**统一重申请决策接口**（MUST NOT 内联档位分流） ──
	//
	// **为什么改走接口**：本决策空间（"同组则刷新/叠层，异组则新建/拒绝/替换"）与状态侧
	// 的五轴堆叠决策**同型**——两处各写一份即漂移源。现由 `FTcsEntityAdmission`
	// 表达，四个档位各由一个**默认策略类**承载。
	//
	// **本处保留的只有"两件事"**（都不是决策本身）：
	// ① **档位 → 策略的绑定**（读 Def 字段选出策略实例——框架零具体策略，故绑定在域侧）；
	// ② **决策结果的消费**（按 `Kind` 走"驳回"或"顶替"——含可打断性查询）。
	// 决策（同组 / 接受 / 层数）全部住在策略类里，**本处 MUST NOT 再写一份**。
	const FTcsSkillInstanceHandle ActiveRun = FindActiveInstance(Subsystem, *Entry);

	// ① 档位 → 策略（三个默认策略都是无状态值类型，取址后走虚分派）
	const FTcsEntityAdmissionAlwaysNew AdmissionAlwaysNew;
	const FTcsEntityAdmissionPerEntityReject AdmissionPerEntityReject;
	const FTcsEntityAdmissionPerEntityReplace AdmissionPerEntityReplace;

	const FTcsEntityAdmission* Admission =
		Def->Instancing == ECastInstancing::CI_InstancePerExecution
			? static_cast<const FTcsEntityAdmission*>(&AdmissionAlwaysNew)
			: (Def->bRetriggerOnActive
				? static_cast<const FTcsEntityAdmission*>(&AdmissionPerEntityReplace)
				: static_cast<const FTcsEntityAdmission*>(&AdmissionPerEntityReject));

	// 装配决策输入与"既有实例视图"。技能侧无来源声明语义、无层数语义 ⇒ 这两项取中性值
	// （`IsSameGroup` 只看 `DefTag` + `Entity`，与技能侧"同实体上的该技能"口径一致）。
	FTcsEntityAdmissionRequest AdmissionRequest;
	AdmissionRequest.DefTag = DefTag;
	AdmissionRequest.Entity = Entity;
	AdmissionRequest.Instigator = Entity;

	FTcsEntityAdmissionExisting AdmissionExisting;
	AdmissionExisting.DefTag = DefTag;
	AdmissionExisting.Entity = Entity;

	// **在飞实例的判据取该条目的 `RunHandles`**（它按构造就是 per-entity，无需跨桶扫描），
	// 且**按句柄经池复核**（账本里可能有已归还的陈旧句柄 ⇒ 复核失败即视为无在飞）——
	// `FindActiveInstance` 正是这个语义。无在飞 ⇒ **不问策略**，直接新建。
	ETcsEntityAdmissionKind AdmissionKind = ETcsEntityAdmissionKind::NewInstance;
	if (ActiveRun.IsValid() && Admission->IsSameGroup(AdmissionRequest, AdmissionExisting))
	{
		AdmissionKind = Admission->ShouldAccept(AdmissionRequest, AdmissionExisting)
			? ETcsEntityAdmissionKind::Replace
			: ETcsEntityAdmissionKind::Reject;
	}

	if (AdmissionKind == ETcsEntityAdmissionKind::Reject)
	{
		// 分流 a：不顶替 ⇒ 驳回（**纯查询语义：不建 run、不改任何状态**）
		UE_LOG(LogTcsSkill, Log,
			TEXT("技能激活被拒[已在飞行中]：实体=%lld 定义=%s 在飞 run=%d/%d（顶替位为假）"),
			Entity.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
		return ESkillActivateResult::SAR_AlreadyActive;
	}

	if (AdmissionKind == ETcsEntityAdmissionKind::Replace)
	{
		// 分流 b：顶替 —— 先查旧 run 当前时段的可打断性。
		//
		// **★ 自 Task 5 起改调查询契约**（`CanBeCanceled`），不再直读 `Def->bCancellableDefault`：
		// 查询契约才是"打断结算与宿主移动系统询问的唯一合法入口"，顶替**就是**一种打断
		// ⇒ 它 MUST 走同一个判据，否则会出现"打断被拒、而顶替绕过判据成功"的第二真相。
		//
		// **零回归的证据（Task 3 的行为逐字保住）**：`DefSwitches` 档（**默认档**）读的就是
		// `bCancellableDefault` 这一个字段 ⇒ 未配 `CastQueryMode` 的定义行为**完全不变**。
		// 三档分派与两处具名降级都在被调方实现（本处 MUST NOT 再复制一份档位判定）。
		//
		// **注意判据的对象**：查的是**旧 run 的当前时段**（按旧 run 的句柄），不是"新激活这次"——
		// 顶替问的是"旧的那一发能不能被掐掉"，与新的这一发的档位无关。
		//
		// **★ 同时对照新定义的档位（如实登记的边界）**：查询契约按 `RunHandle` 定位运行态、
		// 再按该运行态的条目取**定义**——即查的是**旧 run 所依据的那份定义**。若两次激活之间
		// 定义被重新登记成另一档（`UnregisterSkillDef` + `RegisterSkillDef` 是合法序列），
		// 那么"旧 run 的可打断性"按**新定义**判定。这是**有意的**（运行态不持定义内容的副本，
		// 只持 `DefTag` 身份 ⇒ 定义是"当前生效的那一份"，与 `ParamRef` 的解析锚点同口径）。
		if (!CanBeCanceled(Subsystem, ActiveRun))
		{
			UE_LOG(LogTcsSkill, Log,
				TEXT("技能激活被拒[已在飞行中]：实体=%lld 定义=%s 顶替被拒（当前时段不可打断，run=%d/%d）"),
				Entity.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
			return ESkillActivateResult::SAR_AlreadyActive;
		}

		// 顶替：终止旧 run——**统一走终结例程**（按 `ECFR_Replaced` 档发打断事件、回收两本账、
		// 摘账本句柄、归还池槽位）。**MUST NOT** 在此自己写一份清理。
		UE_LOG(LogTcsSkill, Log, TEXT("技能顶替：实体=%lld 定义=%s 终止旧 run=%d/%d"),
			Entity.Id, *DefTag.ToString(), ActiveRun.Index, ActiveRun.Generation);
		TerminateInstance(Subsystem, ActiveRun);

		// **重入纪律（三样都要重新定位，2026-10-08 静态自查补齐）**：终结例程会广播
		// （`PublishImmediate` 当场同步派发）⇒ 订阅者可在其中：
		// ① 撤销条目 / 注销实体 ⇒ 上面的桶与条目指针**失效**；
		// ② **注销技能定义**（`UnregisterSkillDef`）⇒ 上面拿到的 `Def` 指针**悬空**（登记表是
		//    `TUniquePtr` 持有 ⇒ 指针在**在册期间**稳定，但注销即析构 —— 本条是**use-after-free**）。
		// ⇒ 两者都 MUST 重新解析后再继续（**"任何可能广播的调用之后，手里的指针都可能失效"**）。
		Bucket = Subsystem.Registry.FindBucket(Entity);
		Entry = Bucket ? Bucket->FindByPredicate([DefTag](const FTcsLearnedSkillEntry& Item)
		{
			return Item.DefTag == DefTag;
		}) : nullptr;
		if (!Entry)
		{
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：顶替期间条目被撤销（实体=%lld 定义=%s）"), Entity.Id, *DefTag.ToString());
			return ESkillActivateResult::SAR_NotLearned;
		}

		Def = Subsystem.GetRegisteredSkillDef(DefTag);
		if (!Def)
		{
			// 定义在顶替广播期间被注销 ⇒ 与门禁⑥ 同一语义（取不到定义内容即不激活）
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：顶替期间定义被注销（实体=%lld 定义=%s）"),
				Entity.Id, *DefTag.ToString());
			return ESkillActivateResult::SAR_DefInvalid;
		}
	}

	// ── 门禁⑤ CanAfford（`D5-10` 的 Cost 策略） ─────────────────────────
	//
	// **本道答"现在付得起吗"**，宿主从返回值即可看出为什么放不出来（具名 `SAR_CannotAfford`）。
	// **判据的唯一实现在 `FTcsCostOps::CanAffordCost`**（MUST NOT 在此内联一份判断）——
	// 同一个判据还有"到期 / 事件 / 手动"三个**提交点**要各重查一次（TOCTOU 防护），
	// 各写一份必然漂移，而漂移的后果正是**本 Task 要防的那类 bug**：
	// 门禁判的和真扣的数不一致。**注意这里判"基值"、绝不判"当前值"**
	// （后者含修正器，拿它比较会在有修正器时与扣减量不一致——扣减走基值）。
	{
		FString Reason;
		// **★ 传 `Entry`（2026-10-12 B 形状）**：`CostAmount` 键上的减免修正器住在条目上，
		// 不传 ⇒ 门禁判的是**未减免**的基数，而提交点扣的是**已减免**的量
		// ⇒ 两者不一致（"门禁说付不起、提交点却扣得成"或反之），正是本仓要防的自相矛盾。
		if (!FTcsCostOps::CanAffordCost(Subsystem, Entity, Def->Cost, &Reason, Entry))
		{
			UE_LOG(LogTcsSkill, Log, TEXT("技能激活被拒[代价不足]：实体=%lld 定义=%s 原因=%s"),
				Entity.Id, *DefTag.ToString(), *Reason);
			return ESkillActivateResult::SAR_CannotAfford;
		}
	}

	// ── `ECT_OnCastStarted` 档的成本提交：**门禁⑤ 之后、建 run 之前** ──────────
	//
	// **落点由用户 2026-10-11 确认**（`PLN-R6.5-1` Task 2 Step 4），判据三条：
	// ① **语义**——`OnCastStarted` = "激活事务内提交"（`D5-10` 原文），而"激活成功"这件事的
	//    分界线就是**建 run**：建了 run 才存在"在飞的施法"，也才会有终结路径去回收它；
	// ② **失败面更干净**——若在建 run 之后提交且提交失败，就得**回滚一个刚建的 run**
	//    （池槽位、账本 `RunHandles` 回填、事件广播都要撤），而建 run 前失败只需
	//    **直接返回 `SAR_CannotAfford`**，零残留；
	// ③ **顺序与判据一致**——门禁⑤ 刚判"付得起"，紧接提交是**同一事实的两次使用**（判据 + 动作），
	//    中间隔得越远 TOCTOU 窗口越大。
	//
	// **★ MUST NOT 用属性侧的 `BeginBatch` / `Commit` 造一个"事务"把它包起来**：`D5-10` 的
	// "激活事务"指的是**逻辑上的一次激活**，不是 M2 的属性事务批——包起来会让这次扣减与其它属性
	// 改动**共享一个提交点**，而那个提交点今天**不存在**（`TryActivate` 无事务边界）。
	// ⇒ 直调 `CommitCost`（其内部 `SetBaseValue` 是"未开批 = 隐式批，立即生效"）。
	//
	// **★ 提交失败 ⇒ 返回 `SAR_CannotAfford` 且 MUST NOT 建 run**（与"门禁没过"同一返回值——
	// 对调用方而言两者是同件事："现在放不出来，因为付不起"）。日志区分二者：**门禁判不足是正常
	// 业务**（上面那条 `Log`），**提交时失败是真实异常**（`CommitCost` 内部的 `Warning`）。
	if (Def->Cost.Timing == ETcsSkillCostTiming::ECT_OnCastStarted)
	{
		// **★ 传 `Entry`**（同门禁⑤：减免修正器住条目上，不传就会扣错数）
		double AppliedAmount = 0.0;
		if (!FTcsCostOps::CommitCost(Subsystem, Entity, Def->Cost, &AppliedAmount, Entry))
		{
			// 提交失败的**真实异常**已在 `CommitCost` 内留 Warning；此处补一条"因此不激活"的因果
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：`OnCastStarted` 档成本提交失败（实体=%lld 定义=%s）——不建运行态"),
				Entity.Id, *DefTag.ToString());
			return ESkillActivateResult::SAR_CannotAfford;
		}

		// **★ `CostApplied` 必须在此补发（R6.5-1 Task 5 装置实测抓到的缺陷，MUST 保留本注）**：
		// `CommitCost` 是**纯扣减**（它只拿得到 `(Entity, Config)`、拿不到条目身份，而载荷需要
		// `EntryHandle` / `DefTag`）⇒ 发事件的职责在调用方（见 `TcsCostOps.h` 的职责切分说明）。
		// **初版漏了这一行**，装置检查 F2 立刻以 `CostApplied 次数=0` 报 FAIL——
		// 症状是"资源扣了、但没有任何事件通知订阅者"，那正是 `MEM-20260902-12`
		// （"事件与可变性 API 成对"）点名的那类**静默不一致**：扣减是可观察的状态改变，
		// 没有对应事件 ⇒ 订阅者的 UI 永远不刷新。
		FTcsCostOps::BroadcastCostApplied(Subsystem, *Entry, Def->Cost, AppliedAmount);

		// **★ 重新解析（MUST；fresh-context 复核抓到的 Critical 缺陷）**：上面两条路径都会**同步广播**
		// （`CommitCost` → `SetBaseValue` → M2 管线重算 + `Attribute.ValueChanged`；
		// `BroadcastCostApplied` → `CostApplied`）⇒ 订阅者可在其中撤销条目/注销实体/登记定义
		// ⇒ `Entry` 与 `Def` 双双可能失效。
		// **判据 = 本文件下方那段明文纪律**（"任何可能广播的调用之后，手里的指针都可能失效"）。
		Bucket = Subsystem.Registry.FindBucket(Entity);
		Entry = Bucket ? Bucket->Find(EntryHandleForResolve) : nullptr;
		Def = Subsystem.GetRegisteredSkillDef(DefTag);

		if (!Entry || !Def)
		{
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：成本提交广播期间条目或定义被撤销（实体=%lld 定义=%s）"),
				Entity.Id, *DefTag.ToString());
			return Entry ? ESkillActivateResult::SAR_DefInvalid : ESkillActivateResult::SAR_NotLearned;
		}
	}
	// 其余三档（`OnCastCompleted` / `OnEvent` / `Manual`）**在此不做任何事**：
	// 前两者由终结路径与事件订阅驱动（`R6.5-1` Task 3），`Manual` 由宿主自己调 `PayCost`。
	// **MUST NOT 在此为它们补一个"默认提交"**——那会让三档的语义全部塌成 `OnCastStarted`。

	// ── 门禁全过：建运行态 ──────────────────────────────────────────────
	const TTcsInstanceHandle<FTcsSkillInstanceTag> Inner = Subsystem.SkillInstances.Allocate();
	FTcsSkillInstance* Run = Subsystem.SkillInstances.Resolve(Inner);
	if (!Run)
	{
		// 分配后必然可解析（同一次调用内无释放）——防御性返回
		UE_LOG(LogTcsSkill, Warning, TEXT("技能激活异常：新分配运行态无法解析"));
		return ESkillActivateResult::SAR_DefInvalid;
	}

	FTcsSkillInstanceHandle RunHandle;
	RunHandle.SetInner(Inner);

	// **先整体重置再用**（2026-10-08 静态自查抓到的一处真实潜伏缺陷）：
	// `TTcsInstancePool::Allocate` 复用空闲槽时**只改代际、不重建元素**（池的零策略纪律明文
	// "Free 不清零槽位内容"）⇒ 若逐个字段赋值，**任何未显式赋值的字段都会带上"上一任占用者"的值**。
	// 本轮就有一个：`PhaseExpiryEntry`（Task 3 零写入者）会留着上一个 run 的到期句柄——今天无害
	// （无读取者），但 **Task 5 一旦加上读取者，就会把"上一任的陈旧到期条目"当成自己的**，
	// 症状是"莫名取消了别人的到期回调"或"自己的时段永不推进"（远离根因的一类）。
	// ⇒ 纪律：**复用池槽后 MUST 先整体重置**（而非逐字段赋值）——这样**将来给 `FTcsSkillInstance`
	// 加字段时自动是干净的**，不依赖"加字段的人记得来补一行"。
	*Run = FTcsSkillInstance();

	Run->EntryHandle = Entry->Handle;
	Run->Entity = Entity;
	Run->DefTag = DefTag;
	Run->Level = Entry->Level;
	Run->PhaseIndex = 0;
	Run->ChainRunHandle = FTcsChainRunHandle();

	// **起链档位与时段标识一并冻结**（理由见 `FTcsSkillInstance::MainChainStart` 的注释）：
	// `StartMainChain` 在"时段进入"与"完成终结"两处被调用，而定义**可在这两处之间被注销**
	// （顶替/完成的广播会让订阅者有机会这么做）⇒ 回查会拿到 nullptr，表现为"该起链却没起、
	// 且无任何解释"。冻结它之后，**这次施法按它自己开始时的档位走完**。
	Run->MainChainStart = Def->MainChainStart;
	Run->MainChainStartPhaseTag = Def->MainChainStartPhaseTag;

	// 来源锚点：每次运行一枚（`FTcsSourceHandleRegistry` 进程唯一、永不复用）
	Run->CascadeAnchor = Subsystem.SourceRegistry.Allocate();

	// ── ★ 冷却的 `ECT_OnCastStarted` 档：**激活成功后立即起** ──────────────
	//
	// **落点 = 建 run 之后、发 `OnCastStarted` 之前**：语义上"激活成功了"这件事以建 run 为界
	// （run 在册 = 这次施法真的存在），而冷却正是"这次激活的后果"⇒ 建 run 之后起冷才对得上。
	// **排在广播之前**：订阅者收到 `OnCastStarted` 时若去查 `IsOnCooldown`，
	// 应当已经能读到"在冷却"——否则宿主会看到"事件说开始了、查询说没冷却"这个自相矛盾的瞬间。
	//
	// **`OnCastStarted` 是冷却的默认档**（`ETcsSkillCooldownTiming` 的 0 值）：多数技能配的就是它。
	// 其余三档在此**零动作**（`OnCastCompleted` / `OnCastInterrupted` 归终结路径的
	// `DispatchCooldownTiming`；`Manual` 归宿主自己调 `StartCooldown`）。
	if (Def->Cooldown.Timing == ETcsSkillCooldownTiming::ECT_OnCastStarted)
	{
		FTcsCooldownOps::StartCooldownOnEntry(Subsystem, Entity, EntryHandleForResolve);

		// **★ 重新解析（MUST；fresh-context 复核抓到的 Critical 缺陷）**：`StartCooldownOnEntry`
		// 会**同步广播** `CooldownStarted` ⇒ 订阅者可在其中 `UnregisterEntity`
		// （`RemoveBucket` 释放桶的 `TUniquePtr`）或 `GrantSkill`
		// （`AllocateSlot` 的 `Entries.Emplace()` 会**搬移**）
		// ⇒ 上面捕获的 `Entry` / `Def` **双双可能失效**。
		// **判据 = 本文件既有的明文纪律**（见下方"起链前的读数先拷出来"那段）：
		// "任何可能广播的调用之后，手里的指针都可能失效"——本处是该纪律在**新增触发点**上的落点。
		Bucket = Subsystem.Registry.FindBucket(Entity);
		Entry = Bucket ? Bucket->Find(EntryHandleForResolve) : nullptr;
		Def = Subsystem.GetRegisteredSkillDef(DefTag);

		if (!Entry || !Def)
		{
			// 条目/定义在起冷广播期间被撤销 ⇒ 与门禁②/⑥ 同语义（没条目就没法继续激活）
			UE_LOG(LogTcsSkill, Warning,
				TEXT("技能激活中止：起冷广播期间条目或定义被撤销（实体=%lld 定义=%s）"),
				Entity.Id, *DefTag.ToString());
			return Entry ? ESkillActivateResult::SAR_DefInvalid : ESkillActivateResult::SAR_NotLearned;
		}
	}

	// **回填与摘除成对**：建 run 成功后回填账本条目；终结时（`TerminateInstance`）摘除。
	// 只写不摘 ⇒ 账本永久持有已归还的句柄（表现为"技能显示在飞但实际放完了"，静默错误）。
	Entry->RunHandles.Add(RunHandle);

	// ── 激活期参数快照（**时序：门禁全过之后、任何后续读取之前**） ──────────
	// **MUST NOT 在门禁之前构建**（门禁失败即白算一份快照）。
	//
	// **快照的用途（2026-10-09 用户裁定 A′，本注是防误读的唯一说明）**：它是**求值期的参数表载体**
	// ——物化 `ParamChainRows` 时，引用类操作数（`ParamRef`）即从它取键值（先例 = 状态侧的
	// `FTcsBuffModifierMaterializer` 把 `Instance.ParamSnapshot` 经 `FTcsStateSnapshotScope`
	// 装进 `Ctx.ParamTable`）。**它不是"公共读口的冻结来源"**：`GetNumericParam` 走实时通道
	// （参数行初值 + 账本槽位折叠），MUST NOT 被读成"读它拿冻结值"。
	{
		FTcsParamEvaluateContext Ctx;
		MakeContext(Ctx, Entity, Entry->Level);

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
	// 实现要点：`Source` 一律 = **本 run 的 `CascadeAnchor`** ⇒ 随施法终结按来源一次摘净。
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
			Subsystem, *Def, *Entry, Scope.GetTable(), Run->CascadeAnchor);
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
	if (FTcsSkillInstance* LevelRun = ResolveInstance(Subsystem, RunHandle))
	{
		LevelRun->Level = FTcsParamChainOps::EvaluateEffectiveLevel(Subsystem, *Entry, Def);
	}

	// **起链前的读数先拷出来**：起链会同步执行链步骤，而链步骤可改状态/属性并广播 ⇒
	// 之后 MUST NOT 再假定 `Run` 与 `Entry` 指针有效（池扩容即搬移；广播可重入撤销条目或注销实体）。
	// **账本条目是纯数据**（无 UObject、无自引用）⇒ 直接**按值拷一份**即可安全跨过起链。
	const int32 SnapshotLevel = Run->Level;
	const FTcsSourceHandle SnapshotCascadeAnchor = Run->CascadeAnchor;
	const FTcsLearnedSkillEntry EntryCopy = *Entry;

	// ── 主链起链（档位判定单点在 `StartMainChain`；本处只报"我是激活末尾"） ──
	// **`Run` 传 const 引用**：起链可能经"链步骤再激活"让池扩容搬移 ⇒ 本函数内 MUST NOT 写它。
	// 结果经 `ChainRun` 交回，下面**按句柄重新解析后**再回写（`Run` 此刻可能已失效）。
	//
	// **§5.6 瞬时起链的重入面自检（结论：不存在双起）**：`MainChainStart` 是**单值枚举**，
	// 一个定义**只能**配一档 ⇒ "配了 `OnCastStarted` 又配 `OnCastCompleted`"这个组合在本形状下
	// **无法表达**（不是"被覆盖"或"取其一"，而是根本构造不出来）。故本处与完成终结的起链
	// **必然只有一处命中**，无需额外的"只走一次"标记。
	// （判据可证伪：若将来 `MainChainStart` 变成**数组**，本条结论即失效，届时 MUST 补去重。）
	FTcsChainRunHandle ChainRun;
	StartMainChain(
		Subsystem, RunHandle, *Run, *Def,
		ECastChainTrigger::ECT_OnCastStarted, ChainRun);

	// **按句柄回写链句柄**（`Run` 指针在起链后可能已失效 ⇒ 必须重新解析；解析不到即条目/运行态
	// 已在起链期间被终结，此时不回写——那是正确的，run 都不在了）
	if (FTcsSkillInstance* LiveRun = ResolveInstance(Subsystem, RunHandle))
	{
		LiveRun->ChainRunHandle = ChainRun;
	}

	UE_LOG(LogTcsSkill, Log, TEXT("技能激活成功：实体=%lld 定义=%s 条目=%d/%d run=%d/%d 等级=%d 来源=%llu"),
		Entity.Id, *DefTag.ToString(), EntryCopy.Handle.Index, EntryCopy.Handle.Generation,
		RunHandle.Index, RunHandle.Generation, SnapshotLevel, SnapshotCascadeAnchor.Id);

	if (OutRun)
	{
		*OutRun = RunHandle;
	}

	// ── 发 `OnCastStarted`（**排在起链之后**：起链可能已产生副作用与广播，事件排在它们之后） ──
	// `Run` 指针在起链后可能已失效 ⇒ **按句柄重新解析**；载荷的账本面用上面那份**按值拷贝**
	// （条目若在起链期间被撤销，本次激活的事实仍然成立——run 已建、事件该发，只是身份取自拷贝）。
	if (const FTcsSkillInstance* LiveRun = ResolveInstance(Subsystem, RunHandle))
	{
		Broadcast(Subsystem, Tag_TcsEvent_Cast_Started, RunHandle, *LiveRun, EntryCopy,
		/*Reason=*/ECastFinishReason::ECFR_Completed);  // 尚未终结，填中性默认
	}

	// ── ★ 时段推进的起点：**有当前时段 ⇒ 排定首段到期条目；没有 ⇒ 立即完成终结** ──
	//
	// **这个"没有 ⇒ 完成"正是用户 2026-10-09 裁定甲′的落地处**（瞬发 = 激活当刻完成）。
	// **判据 MUST 取"待推进时段数"**（这里是"排不进待推进时段"这一结果本身），
	// **MUST NOT** 写成 `if (Def->Phases.IsEmpty())` 这类空表判定——那会让"配了时段但已走到末尾"
	// 与"空时段表"走两条不同的路（同一件事两个入口）。`EnterPhase` 的实现正是按
	// `Phases.IsValidIndex(PhaseIndex)` 判的（`PhaseIndex` 此刻为 0）。
	//
	// **顺序**：完成终结排在 `OnCastStarted` **之后**——订阅者先得知"开始"，再得知"完成"；
	// 反过来会让"完成"事件到达时订阅者还没见过这次施法。
	//
	// **时钟不可得 ⇒ `EnterPhase` 返回 false ⇒ 同样落完成终结**（"不留永不终结的在册运行态"
	// 这条判据对两条路径一视同仁，这正是"静默降级为瞬发语义"的意思）。
	if (!EnterPhase(Subsystem, RunHandle))
	{
		// **只在运行态仍在册时补完成终结**：上面的起链与 `OnCastStarted` 广播都可能已经终结它
		// （订阅者可以这么做），那时再调就是重入（会被守卫挡下，但没必要发这一枪）。
		if (ResolveInstance(Subsystem, RunHandle))
		{
			FinishSkillInstance(Subsystem, RunHandle, ECastFinishReason::ECFR_Completed);
		}
	}

	return ESkillActivateResult::SAR_Success;
}



// 运行态生命周期
namespace
{
	/**
	 * 取消一个到期锚点（四路终结共用；**撤完置空**使"可重复调用"成立）。
	 *
	 * **名字带 `CastFinish` 前缀**：同模块多个 `.cpp` 会被 UBT 合并进同一 unity TU，
	 * 匿名命名空间的通用名会跨文件相撞（本仓 unity 合并撞名的既有实证——
	 * `TcsCastOps_Phase.cpp` 里另有一个同形的 `CancelPhaseExpiry`，两者**刻意不同名**）。
	 *
	 * **⚠ 为什么"先判有效性"在这里够用**：本函数由**终结例程**调用，而终结例程的入口之一是
	 * **到期回调自身**（最后一段到期 → 完成终结）。那时回调**已经复位过自己的锚点**
	 * （`AdvancePhase` 的第一条语句）⇒ 本处看到的 `Entry` 要么是"活条目"（非最后一段、提前终结）、
	 * 要么是空 ⇒ **永不会误撤一个已释放的句柄**（那会踩 `FTcsExpiryHeap::Cancel` 的 `ensureMsgf`）。
	 */
	void CancelCastFinishExpiry(UTcsSkillSubsystem& Subsystem, FTcsTimeEntryHandle& Entry)
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

bool FTcsCastOps::FinishSkillInstance(
	UTcsSkillSubsystem& Subsystem,
	FTcsSkillInstanceHandle RunHandle,
	ECastFinishReason Reason)
{
	FTcsSkillInstance* Run = ResolveInstance(Subsystem, RunHandle);
	if (!Run)
	{
		// 悬空句柄 = 时序竞态（调用方手上的句柄过期），不是契约违规 ⇒ 静默 false，不 ensure
		return false;
	}

	/**
	 * **★ 重入窗口守卫（本变更裁掉 Task 3 留下的边界）**
	 *
	 * 本函数的广播与按来源回收**都是当场同步派发** ⇒ 订阅者可在回调内**重入终结同一句柄**。
	 * 若不挡，后果有两条：① 归还槽位双次（池的 `ensureMsgf` 拦得住，但**留红字**）；
	 * ② **"打断事件发两次"**——这条 Task 3 当时**没有**挡住（其旧注释自认，归属即本轮）。
	 *
	 * **为什么标记必须"进函数就加、出函数才删"（而不是只围着广播那一小段）**：
	 * 回收属性条目的那一步**本身就会广播属性变更事件**（M2 的批提交会重算 + 广播，
	 * 订阅者可在其中重入施法操作）⇒ 守卫窗口**必须覆盖整个例程**，否则在中段重入仍会双次终结。
	 *
	 * **体例 = 共享层的统一重入守卫**（`TTcsEntityBroadcastGuard`，2026-10-14）：
	 * 它取代了状态侧的 `InRemovalBroadcastHandles` 与技能侧本处原先的 `InFinishBroadcastHandles`
	 * ——一份实现、两个实例。语义不变（进出窗口成对标记 / 解除）。
	 * **判据 = 订阅者重入一次，`Interrupted` 仍为 1、零 ensure。**
	 */
	if (Subsystem.FinishBroadcastGuard.IsBroadcasting(RunHandle))
	{
		// 递归终结同句柄 ⇒ **返回 false 且静默**（不重复广播、不重复回收、不重复归还槽位）
		return false;
	}

	Subsystem.FinishBroadcastGuard.Enter(RunHandle);

	// **先把终结所需的数据拷出来再动池**：下面要归还槽位，而池操作可能搬移元素。
	// `FTcsSkillInstance` 是**纯数据**（无 UObject、无自引用）⇒ 按值拷一份即可安全跨过全部后续调用
	// （包含会广播的回收与事件派发——那两处都可能让池扩容或让本 run 被重入终结）。
	const FTcsSkillInstance RunCopy = *Run;
	const FTcsSkillEntryHandle EntryHandle = RunCopy.EntryHandle;
	const FTcsCombatEntityHandle Entity = RunCopy.Entity;
	const FTcsSourceHandle CascadeAnchor = RunCopy.CascadeAnchor;

	// ── ⓪ **完成档的主链起链**（只在 `ECFR_Completed` 且档位匹配时真的起链） ──
	//
	// **为什么起链排在"回收"之前**：完成 ≠ 取消。完成路径的语义是"这次施法**办完了它要办的事**"
	// ——而"起链"正是它要办的事（`MCS_OnCastCompleted` 档的存在意义就是"放完了才打出效果"）。
	// 若排在回收之后，新起的链挂的条目会**当场被自己的终结例程摘掉**（同一个 `CascadeAnchor`）
	// ⇒ 表现为"配了完成档的技能永远打不出效果"，而**日志上一切正常**（这正是最难查的一类）。
	//
	// **`ECFR_Canceled` / `ECFR_Replaced` 两档不起链**（判据 = 触发点，见 `StartMainChain`）：
	// 被打断/顶替的施法**没有办完事**，此时打主链正是本能力明文要拦的形态
	// （"被顶替的旧技能在瞬间真的打出主链"）。
	//
	// **重入面**：本段会同步执行链步骤（可再起激活、可广播）⇒ 之后 MUST 重新解析运行态，
	// 且**不得**再假定 `Run` 指针有效（池可扩容搬移）——故本条之后的全部动作一律用 `RunCopy`。
	if (Reason == ECastFinishReason::ECFR_Completed)
	{
		TryStartMainChainAt(Subsystem, RunHandle, Entity, EntryHandle, ECastChainTrigger::ECT_OnCastCompleted);
	}

	// ── ① 取消时段到期锚点（**就地置空**——理由见 `CancelCastFinishExpiry` 的注释） ──
	// 取消是作废残留回调的**第一道防线**（代际校验是第二道）：不取消则本 run 的条目仍会到期、
	// 回调一个已终结的 run（表现为"什么都没发生"，属静默无效，但会占着堆与池的槽位）。
	if (FTcsSkillInstance* LiveRun = ResolveInstance(Subsystem, RunHandle))
	{
		CancelCastFinishExpiry(Subsystem, LiveRun->ExpiryEntry);
	}

	// ── ② 按 `CascadeAnchor` 回收**链挂的 M2 属性账本条目**（`CHAIN-7` 的闭合动作） ──
	//
	// **★ 为什么是两本账、同一个来源句柄（MUST NOT 只摘其一）**：
	// - **M2 属性账本**：链内 `ModifyAttribute` 挂的条目——`TcsStepModifyAttribute` 把
	//   `Context.RunSource` 写进属性修正器条目，而 Task 3 已定 `Context.RunSource` =
	//   `FTcsSkillInstance.CascadeAnchor`（`TcsCastOps_Chain.cpp` 的起链路径）⇒ 按它一次摘净。
	// - **M5 技能账本**：本次施法在激活期物化的 `ParamChainRows` 条目（Task 4 落的，
	//   `Source` 同样 = 本 run 的 `CascadeAnchor`，见下面一步）。
	// 只摘后者 ⇒ 链改的属性**常驻不回退**——**正是 `CHAIN-7` 的实测症状"常驻停在 X−5"**
	// （`EVID-2026-10-05-state-layer-pie` §1.2 检查 S15 的对照读数）。
	//
	// **★ `CHAIN-7` 的闭合依据（MUST 写清，且 MUST NOT 被读成"三条死路被修"）**：
	// 台账原文的触发条件是"出现**链改的属性必须随某生命周期回退**的真实内容需求"，
	// 而**施法运行态是链运行态的"第一个长生命周期持有者"** ⇒ 条件在 R6 成立。
	// **三条死路今天依然成立**（本变更一条都没修）：
	// ① **随链运行结束回收**：全部即时链的运行态在 `ExecuteChain` 返回前**已释放** ⇒ 无回收窗口；
	// ② **按因果边 `CausedBy` 级联**：**已被明文禁止**（`TcsEffectContext.h:54-55`）；
	// ③ **框架自动回收**：`FTcsStepModifyAttribute` **刻意不提供**回收器。
	// ⇒ 本处落地的正是台账认定的"今天唯一合法用法"（宿主自存来源锚点再自调 `RemoveBySource`）
	// **所缺的那个触发点**——不是发明新机制。
	//
	// **★ 只摘自己起的链（MUST NOT 越界）**：`CascadeAnchor` 取的是**本 run 自己的**。
	// **子链另发新号、不沿用父链的**（`TcsEffectContext.h:49`）⇒ 只摘本 run **直接**挂的条目；
	// **MUST NOT 递归下探**（递归会摘掉不属于自己的东西，且 `CausedBy` 级联已被禁止）。
	// **残留边界（如实登记，MUST NOT 假装已解）**："技能起链 → 链起子链 → 子链挂属性且子链比施法
	// 活得久"这一组合下条目**仍会常驻**。
	{
		UTcsAttributeSubsystem* Attribute = Subsystem.GetWorld()
			? Subsystem.GetWorld()->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;

		// **先判"实体在属性门面注册过吗"再调 `RemoveBySource`**（顺序有理由，不是洁癖）：
		// 属性侧的 `RemoveBySource` 对**未注册实体**留一条 `Warning`（"实体未注册——忽略"）。
		// 而施法**不要求**实体在属性侧注册（技能账本与属性账本是两套设施，宿主可只用其一）
		// ⇒ 不预判就会让**每一次施法终结**都留一条红字，而那**不是缺陷**（该 run 的链压根没改过属性）。
		// **判据 = "这条红字是不是在说真实的异常"**：它说的是"没这个实体"，而常态就是没这个实体
		// ⇒ 属**信号污染**（"零意外红字"这条验收信号会被自己的常态路径淹没，本仓反复拦的形态）。
		// `ResolveStore` 是**非确保**的解析口（读取是正常查询路径），正好承担这个预判。
		if (Attribute && Attribute->ResolveStore(Entity))
		{
			// **0 条命中 = 正常路径**（该 run 的链没改过属性 / 属性实例已被移除），不 ensure。
			const int32 RemovedAttrSlots = Attribute->RemoveBySource(Entity, CascadeAnchor);
			if (RemovedAttrSlots > 0)
			{
				UE_LOG(LogTcsSkill, Log, TEXT("施法终结：按来源回收属性账本条目 %d 条（实体=%lld 来源=%llu）"),
					RemovedAttrSlots, Entity.Id, CascadeAnchor.Id);
			}
		}
	}

	// ── ③ 按 `CascadeAnchor` 回收**本 run 物化的 M5 参数链条目**（`add-skill-param-chain` 的明文条款：
	// "物化时 `Source` MUST = 本施法运行态来源句柄 ⇒ 随施法终结**级联摘除**"） ──
	//
	// **业务上不摘会怎样（玩家能直接感觉到的错）**：物化是"每次激活都追加"（同一技能可先后放多次，
	// 各带自己的 `CascadeAnchor`）；终结时不摘 ⇒ 加成**逐次累积**，表现为**"放两次技能，参数翻倍"**。
	// 且它是**静默**的——只放一次技能时读数完全正常，所以"只验一次激活"的检查必漏。
	// 摘除按来源全量命中 ⇒ 只摘本次 run 那一批，其它 run 的条目原样保留。
	FTcsParamChainOps::RemoveParamModifiersBySource(Subsystem, Entity, CascadeAnchor);

	// ── ④ 摘账本条目里的在飞句柄（**与回填成对**） ──
	// 条目可能已被撤销 ⇒ 静默跳过，不 ensure。
	// **回收（②③）刻意排在"摘句柄"之前**：回收面**不依赖条目存在**（来源与实体都在 `RunCopy` 里
	// 自持）⇒ 即使条目已撤销，回收也必须照做（否则"撤销技能"这条路径会静默泄漏属性条目）。
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Entity);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;

	// **定义在"回收之后"才重新解析**（MUST，理由见下方 ③′ 段的次序判据）：
	// ②③ 两段回收会**广播属性变更事件** ⇒ 订阅者可在其中**注销技能定义**
	// ⇒ 之前拿到的 `Def` 指针可能已悬空（登记表是 `TUniquePtr` 持有，注销即析构
	// ——`FTcsSkillInstance` 顶替分支的既有注释已就"任何可能广播的调用之后，手里的指针都可能失效"
	// 立过同款纪律）。本处按句柄重新解析一次，**MUST NOT 沿用更早的 `Def`**。
	const FTcsSkillDefData* Def = Entry ? Subsystem.GetRegisteredSkillDef(RunCopy.DefTag) : nullptr;

	if (Entry)
	{
		Entry->RunHandles.Remove(RunHandle);
	}

	// ── ★ ③′ 冷却 / Cost 的 **Timing 触发点**（`R6.5-1` Task 3 的唯一挂点） ──
	//
	// **为什么只挂在这里、MUST NOT 去四个调用点各挂一份**：四路终结（末段走完 / 瞬发当刻 /
	// 打断 / 顶替）**全部汇聚到本函数**，而本函数的签名自带 `ECastFinishReason Reason`
	// ⇒ 一处分派即可覆盖四路。去四个调用点各挂一份就是本仓反复拦的"同一件事两个入口"。
	//
	// **次序判据（MUST，与 ⓪ 段起链的理由相反、判据相同）**：
	// - ⓪ 段（完成档起链）**必须排在回收之前**——否则新起的链挂的条目会**当场被自己的终结例程摘掉**
	//   （同一个 `CascadeAnchor`），表现为"配了完成档的技能永远打不出效果"；
	// - 本段（冷却 / Cost 触发）**必须排在回收之后**——因为回收会**广播属性变更事件**
	//   （M2 批提交重算 + 广播），而 `OnEvent` 档的 Cost 提交可能被那些广播触发；
	//   排在回收之前的话，回收尚未完成就可能提交消耗，且新起的链 / 条目会与回收动作交错。
	// ⇒ **两处次序相反、判据相同：别让自己的动作被自己的清理吃掉。**
	//
	// **为什么本段在"归还槽位之前"**：广播纪律是"**先广播、后归还槽位**"
	// （`FTcsCastEventPayload` 的类注释明文）⇒ 订阅者拿到的冷却事件里若含 run 句柄，
	// 那一刻它 MUST 仍在册。
	//
	// **全部只读 `RunCopy` 的拷贝字段**（`EntryHandle` / `Entity`）：池可扩容搬移，
	// `Run` 指针在本函数 ⓪ 段起链之后即不可信（既有纪律）。
	if (Entry && Def)
	{
		DispatchCooldownTiming(Subsystem, *Entry, *Def, Reason);
		DispatchCostTiming(Subsystem, *Entry, *Def, Reason);

		// **★ 重新解析（MUST；fresh-context 复核抓到的 Critical 缺陷）**：上两行各自**同步广播**
		// （`CooldownStarted` / `Attribute.ValueChanged` / `CostApplied`）⇒ 订阅者可在其中
		// 撤销条目 / 注销实体 / 注销定义 ⇒ 紧接着的 ⑤ 段（`Broadcast(..., *Entry)` 装配载荷）
		// 会用**已析构的桶**。**判据 = 本函数 ②③ 段之后已有的同款重新解析**
		// （其注释原文即"任何可能广播的调用之后，手里的指针都可能失效"）
		// ——本处是该纪律在新触发点上的落点，**MUST NOT 省略**。
		Bucket = Subsystem.Registry.FindBucket(Entity);
		Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
		Def = Entry ? Subsystem.GetRegisteredSkillDef(RunCopy.DefTag) : nullptr;
	}
	else if (Entry)
	{
		// 定义取不到（施法期间被注销）⇒ 冷却/Cost 配置无从读取 ⇒ 如实报出并跳过（不崩、不猜）
		UE_LOG(LogTcsSkill, Log,
			TEXT("施法终结：定义取不到 ⇒ 跳过冷却/Cost 的 Timing 触发（实体=%lld 原因=%d）"),
			Entity.Id, static_cast<int32>(Reason));
	}

	// ── ⑤ 广播终止事件（**MUST 排在归还槽位之前**） ──
	//
	// **"先广播后归还槽位"的纪律**：载荷里的运行句柄在**回调内**仍然有效——订阅者能据此
	// 复核运行态（读它的时段下标、链句柄等）。反过来（先归还）会让订阅者"收到完成事件却查不到 run"。
	//
	// **事件由 `Reason` 唯一分岔（MUST NOT 互相冒充）**：完成 → `Completed`；
	// 打断与顶替 → `Interrupted`。照抄 GAS 的 `EndAbility(bWasCancelled = false)` 会让配了
	// `OnCastCompleted` 起链的技能在**被打断**时真的打出主链（玩家连点两下、第一下的效果仍兑现）。
	//
	// **条目已被撤销时的边界（如实登记）**：载荷的定义身份取自账本条目，而 run 自身不持它
	// ⇒ 条目没了就装配不出载荷 ⇒ 本处跳过广播（Log 级报出）。**回收与归还槽位照做**（在下面），
	// 故"撤销了技能 ⇒ 施法终结时不再发事件"是**有意的**：那条技能已经不在账本上了。
	if (Entry)
	{
		const FGameplayTag EventTag = (Reason == ECastFinishReason::ECFR_Completed)
			? Tag_TcsEvent_Cast_Completed
			: Tag_TcsEvent_Cast_Interrupted;

		Broadcast(Subsystem, EventTag, RunHandle, RunCopy, *Entry, Reason);
	}
	else
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("施法终结：账本条目已撤销 ⇒ 不广播终止事件（实体=%lld 原因=%d），回收与归还槽位照做"),
			Entity.Id, static_cast<int32>(Reason));
	}

	// ── ⑥ 归还池槽位（此后旧句柄凭代际失配判悬空） ──
	//
	// **★ 归还前 MUST 复核句柄仍有效**：上面的回收与广播都是当场同步派发 ⇒ 订阅者可以在这期间
	// 让本 run 以别的路径归还（例如撤销整个实体）。若不复核，第二次 `Free` 会命中池的 `ensureMsgf`
	// ⇒ **留红字**，破坏"零 ensure / 零意外红字"这条验收信号。
	// ⇒ 本处用**非确保**的 `IsValid` 先挡一道（复核失败 = 已在回收/广播期间被别的路径归还 ⇒ 静默跳过）。
	const TTcsInstanceHandle<FTcsSkillInstanceTag> FreeInner = RunHandle.GetInner();
	if (Subsystem.SkillInstances.IsValid(FreeInner))
	{
		Subsystem.SkillInstances.Free(FreeInner);
	}
	else
	{
		UE_LOG(LogTcsSkill, Log,
			TEXT("施法终结：归还槽位跳过（run=%d/%d 已在回收/广播期间被别的路径终结）"),
			RunHandle.Index, RunHandle.Generation);
	}

	// **解除重入窗口标记**（与函数入口的 `Add` 成对——**本函数自 `Add` 之后 MUST NOT 有早退**，
	// 否则标记会永久滞留，让同一句柄的后续合法终结被永久拒绝）
	Subsystem.FinishBroadcastGuard.Exit(RunHandle);

	UE_LOG(LogTcsSkill, Log,
		TEXT("施法终结：实体=%lld 条目=%d/%d run=%d/%d 来源=%llu 原因=%d（已回收两本账、发终止事件、归还槽位）"),
		Entity.Id, EntryHandle.Index, EntryHandle.Generation,
		RunHandle.Index, RunHandle.Generation, CascadeAnchor.Id, static_cast<int32>(Reason));

	return true;
}

bool FTcsCastOps::TerminateInstance(UTcsSkillSubsystem& Subsystem, FTcsSkillInstanceHandle RunHandle)
{
	// **顶替档的便捷入口**（只转发一条、零逻辑——这正是"四路只在一个地方分岔"的形式）：
	// 回收动作与事件选择全在 `FinishSkillInstance`，本函数 MUST NOT 再写一份清理。
	return FinishSkillInstance(Subsystem, RunHandle, ECastFinishReason::ECFR_Replaced);
}

void FTcsCastOps::DispatchCooldownTiming(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData& Def,
	ECastFinishReason Reason)
{
	// **★ 分派表（`PLN-R6.5-1` Task 3 Step 3 的硬约束，MUST 守）**：
	//
	// `ECastFinishReason` 只有三档，且**瞬发没有自己的档**——瞬发走 `ECFR_Completed`
	// （`TcsCastOps_Phase.cpp` 的瞬发路径直接以它调本终结例程）。故：
	//
	// | `Reason` | 是否算"被打断" | 起哪一档 |
	// |---|---|---|
	// | `ECFR_Canceled` | **是**（真打断） | `ECT_OnCastCanceled` |
	// | `ECFR_Replaced` | **是**（`SPEC-04-skill` §11.5 裁决 1 明文"顶替 = 打断的一种终止形态"） | `ECT_OnCastCanceled` |
	// | `ECFR_Completed` | **否**（含瞬发） | `ECT_OnCastCompleted` |
	//
	// **⇒ 瞬发自动被覆盖**：瞬发以 `ECFR_Completed` 终结 ⇒ 只要本分派挂在 `FinishSkillInstance` 里，
	// 它就被 `ECT_OnCastCompleted` 档覆盖，**不需要额外挂点**。
	// **同族教训**：`SPEC-04-skill` §3.3 的实施注记已就"空 `Phases` 意味着没有任何时段到期
	// ⇒ 原句'全时段走完'对瞬发永不成立"留过同款教训；本轮 MUST NOT 复犯。
	const bool bInterruptedPath = (Reason != ECastFinishReason::ECFR_Completed);
	const ETcsSkillCooldownTiming WantedTiming = bInterruptedPath
		? ETcsSkillCooldownTiming::ECT_OnCastCanceled
		: ETcsSkillCooldownTiming::ECT_OnCastCompleted;

	// **`ECT_Manual` 档框架什么都不做**（含日志）——它的定义就是"宿主自己调 `StartCooldown`"，
	// 框架做任何事都违背它。故本函数对 `ECT_Manual` **零动作**（下面的比较自然把它排除：
	// `Manual` 既不等于 `WantedTiming`，也走不到 `OnCastStarted`）
	if (Def.Cooldown.Timing != WantedTiming)
	{
		return;
	}

	// **`OnCastStarted` 档已在激活路径起过**（`TcsCastOps.cpp` 的门禁后、建 run 前）——
	// 本终结路径 MUST NOT 为它再起一次（那会让"放一次技能起两次冷却"）。
	// 上面的比较已天然排除它（`WantedTiming` 只可能是 Completed/Interrupted 两者），此处注释示意。
	FTcsCooldownOps::StartCooldownOnEntry(Subsystem, Entry.Entity, Entry.Handle);
}

void FTcsCastOps::DispatchCostTiming(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData& Def,
	ECastFinishReason Reason)
{
	// **只有 `ECT_OnCastCompleted` 一档挂在这里**（其余两档各有归属，MUST NOT 在此代劳）：
	// - `OnCastStarted`：激活路径的"门禁后、建 run 前"（`TcsCastOps.cpp`）；
	// - `Manual`：宿主自己调 `PayCost`，**框架零动作**（"某个特定事件触发消耗"这类项目玩法
	//   归这一档——2026-10-12 用户裁定删除 `ECT_OnEvent`，判据见 `TcsCostTypes.h`）。
	//
	// **为什么 `OnCastCompleted` 档只在"完成"路触发**：`ETcsSkillCostTiming` **刻意没有**
	// `OnCastInterrupted` 档（判据：`D5-4` 已砍返还，该档两种读法一为重复一为荒谬——
	// 详见 `TcsCostTypes.h`）。⇒ 被打断 / 被顶替时**不提交**：这正是 `OnCastStarted` 档
	// 存在的意义（"施法开始时扣"已经覆盖了"被打断也扣了"）。
	if (Def.Cost.Timing != ETcsSkillCostTiming::ECT_OnCastCompleted)
	{
		return;
	}

	if (Reason != ECastFinishReason::ECFR_Completed)
	{
		// 打断 / 顶替 ⇒ 不提交（该档无"打断时提交"语义——见上）
		return;
	}

	// **提交点重查**已在 `PayCostForEntry` 内（TOCTOU 防护）——本处走它而不是直调 `CommitCost`，
	// 正是为了那次重查（`OnEvent` 档同理）。**唯一例外**是激活路径的 `OnCastStarted` 档：
	// 它紧接门禁⑤、窗口最小，直调 `CommitCost` 而不必再查一次。
	FTcsCostOps::PayCostForEntry(Subsystem, Entry.Entity, Entry.Handle);
}

int32 FTcsCastOps::ResetAll(UTcsSkillSubsystem& Subsystem)
{
	const int32 CountBefore = CountInstances(Subsystem);

	// 池的零策略纪律：不逐槽清理实例内容（`Reset` 使全部旧句柄代际失配）
	Subsystem.SkillInstances.Reset();

	return CountBefore;
}

FTcsSkillInstance* FTcsCastOps::Find(UTcsSkillSubsystem& Subsystem, FTcsSkillInstanceHandle RunHandle)
{
	return ResolveInstance(Subsystem, RunHandle);
}

int32 FTcsCastOps::CountInstances(UTcsSkillSubsystem& Subsystem)
{
	int32 Count = 0;
	Subsystem.SkillInstances.ForEach([&Count](FTcsSkillInstance&)
	{
		++Count;
	});

	return Count;
}
