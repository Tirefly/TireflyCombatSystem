// Copyright Tirefly. All Rights Reserved.

#include "Skill/TcsCooldownOps.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "StructUtils/InstancedStruct.h"
#include "Parameter/TcsParamValue.h"
#include "Parameter/TcsParamValueSource.h"

#include "Def/TcsCooldownTypes.h"
#include "Def/TcsSkillDefData.h"
#include "Param/TcsSkillParamKeys.h"
#include "Skill/TcsCooldownEvents.h"
#include "Skill/TcsParamChain.h"
#include "Skill/TcsSkillRegistry.h"

#include "TcsSkillLogChannel.h"
#include "TcsSkillSubsystem.h"



namespace
{
	/**
	 * 取该单位的组槽表（**不代做任何清理**——纯查表）。
	 *
	 * **⚠ 一处注释订正（fresh-context 复核抓到的 Minor）**：本函数初版的注释写着它
	 * "顺手清掉已过期或簿记悬空的槽"，**而它一行清理都不做**（就是 `TMap::Find` 的转发）
	 * ——注释告诉下一个读者的是**反话**。已删。
	 * **陈旧槽的实际处置点**有三处，各司其职：① `StartCooldownOnEntry` 的组轨分支
	 * （复用前判活、不活的**原地重建**）；② `ExpireGroup`（到期即**复位 + 移除**，
	 * 并在内层表空时**连外层键一起删**）；③ `ReleaseUnitCooldowns`（单位级整体清）。
	 * **判据**：过期判定需要时钟读数，而"查表"不该悄悄做副作用——把它留给真正需要判活的那几处。
	 */
	TMap<FGameplayTag, FTcsCooldownGroupState>* FindLiveGroupSlots(
		UTcsSkillSubsystem& Subsystem,
		FTcsCombatEntityHandle Unit)
	{
		return FTcsCooldownOps::FindGroupSlots(Subsystem, Unit);
	}

	/**
	 * 广播一枚冷却事件（**统一出口**——载荷装配只有一处，四个广播点共用）。
	 *
	 * **为什么必须有它**：四个产生者（起冷 / 调轨 / 清轨 / 到期）要发的载荷形状完全相同，
	 * 各装一份必然漂移；而载荷里的字段（`Total` / `Remaining` / `EndTimeSeconds`）**跨三处
	 * 取值口径不同**（起冷时 `Remaining == Total`；调整后二者不等；到期时 `Remaining == 0`）
	 * ⇒ 口径 MUST 集中。
	 *
	 * **世界无总线 ⇒ 静默跳过**（世界拆解期）：不 ensure、不红字（时序而非契约违规）。
	 */
	void BroadcastCooldownEvent(
		UTcsSkillSubsystem& Subsystem,
		FGameplayTag EventTag,
		const FTcsLearnedSkillEntry& Entry,
		int32 TrackIndex,
		FGameplayTag GroupTag,
		double Total,
		double Remaining,
		double EndTimeSeconds)
	{
		UTcsEventBusSubsystem* Bus = Subsystem.GetEventBus();
		if (!Bus)
		{
			// **世界无总线（拆解期）⇒ 静默跳过**：不 ensure、不红字——时序而非契约违规。
			return;
		}

		FTcsCooldownEventPayload Payload;
		Payload.EntryHandle = Entry.Handle;
		Payload.Unit = Entry.Unit;
		Payload.DefTag = Entry.DefTag;
		Payload.TrackIndex = TrackIndex;
		Payload.GroupTag = GroupTag;
		Payload.Total = Total;
		Payload.Remaining = Remaining;
		Payload.EndTimeSeconds = EndTimeSeconds;

		// **立即通道**：冷却状态是"查询面可验证的事实"（`IsOnCooldown` 立刻能读到），
		// 发事件若走帧末，订阅者会在"事件还没到、但查询已经说在冷却"的窗口里看到不自洽的读数。
		Bus->PublishImmediate(EventTag, FInstancedStruct::Make(Payload));
	}
}



// 起轨
int32 FTcsCooldownOps::StartCooldownOnEntry(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle)
{
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		// 悬空句柄 = 时序竞态（调用方手上的句柄过期），不是契约错误 ⇒ Log 级，不 ensure
		UE_LOG(LogTcsSkill, Log, TEXT("起冷却跳过：账本条目悬空（单位=%lld 条目=%d/%d）"),
			Unit.Id, EntryHandle.Index, EntryHandle.Generation);
		return 0;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		// 定义在施法期间被注销 ⇒ 说不出"要起几条轨" ⇒ 如实报出并零动作
		UE_LOG(LogTcsSkill, Warning, TEXT("起冷却中止：定义取不到（单位=%lld 定义=%s）"),
			Unit.Id, *Entry->DefTag.ToString());
		return 0;
	}

	const int32 TrackCount = Def->Cooldown.Tracks.Num();
	if (TrackCount == 0)
	{
		// **空 Tracks = 永远可放**（`D5-15`"默认无冷却"）⇒ 正常路径，静默返回 0（零红字）
		return 0;
	}

	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		// 时钟不可得（世界拆解 / 单测无时钟）⇒ Log 级、零红字、**不 ensure**（时序而非配置错误）。
		// **不留"待起"状态**：冷却是纯时轴设施，没有时钟就没有"剩余多久"这回事 ⇒ 零动作退化为无冷却。
		UE_LOG(LogTcsSkill, Log, TEXT("起冷却跳过：时钟子系统不可得（单位=%lld）——退化为无冷却"), Unit.Id);
		return 0;
	}

	// **★ CDR 只在此刻取一次**（`D5-15`："快照于冷却触发时刻，中途变化不追溯"）。
	// 取多次会让同一批轨道拿到不同系数——那是"同一次冷却内部不自洽"，最难解释的一类读数。
	const double CooldownPct = EvaluateCooldownPct(Subsystem, *Entry, Def);
	const double Now = Clock->GetClock().Elapsed;

	// **★ 时长载体的求值上下文（2026-10-12：`Track.Duration` 换型为 `FTcsParamValue`）** ──
	//
	// **为什么在循环外装配一次**：全部轨道共用同一份上下文（等级、单位、参数表），
	// 逐轨各配一次是白做，且会让"同一次冷却是几档"这件事出现多个装配点（同 `MakeContext` 的理由）。
	//
	// **求值上下文与参数链其余各点逐字同款**：`Subject`/`Instigator` = 单位、
	// `EffectiveLevel` = **条目等级**——**这是"按时长等级表配冷却"能成立的唯一原因**
	// （`FTcsParamSource_StateLevelArray` 读基类的 `Context.EffectiveLevel`，
	// 见 `TcsParamSource_StateLevel.h:70`）。
	//
	// **参数表恒空**（同 `FTcsCastOps::MakeContext` 的既有边界）：引用类源（`ParamRef`）
	// 在这个时点没有更外层参数表可查 ⇒ 落各自兜底（不报错、不崩溃）。
	FTcsParamEvaluateContext DurationCtx;
	DurationCtx.Subject = Unit;
	DurationCtx.Instigator = Unit;
	DurationCtx.EffectiveLevel = Entry->Level;
	DurationCtx.ParamTable = TScriptInterface<ITcsParamTableReader>();

	// 轨道状态数组与定义**同长同序**（查询口按 `TrackIndex` 下标寻址就靠这条）。
	// **整体重建**（而非逐字段赋值）：定义可能被热改过（轨道数变了），重建保证下标对齐。
	//
	// **★ 重建前 MUST 先撤掉旧锚点（本仓"外部资源成对"纪律；R6.5-1 自审抓到的一处真实泄漏）**：
	// `Reset()` 会把旧句柄**直接丢掉**，而每个自身轨都持一枚到期堆条目
	// ⇒ 不撤就重建 = 那些条目**失去唯一的取消锚点**、只能等各自到期后空跑一次回调
	// （回调里 `EntryHandle`/`TrackIndex` 仍能命中，于是它会**清掉新起的冷**——
	// 症状极具误导性：**"刚起冷就立刻被清"**，且日志上只有一条正常的"冷却到期"）。
	// **判据**：凡"丢弃一个已入堆的句柄"之前，MUST 先 `CancelExpiry`（`ClearTrack` /
	// `ReleaseEntryCooldowns` 都遵守，本处是同一纪律的第三处落点）。
	if (UTcsClockSubsystem* LiveClock = Clock)
	{
		for (FTcsCooldownTrackState& OldState : Entry->CooldownTracks)
		{
			if (OldState.ExpiryEntry.IsValid())
			{
				LiveClock->CancelExpiry(OldState.ExpiryEntry);
			}
		}
	}

	Entry->CooldownTracks.Reset();
	Entry->CooldownTracks.SetNum(TrackCount);

	TWeakObjectPtr<UTcsSkillSubsystem> WeakSubsystem(&Subsystem);
	int32 StartedCount = 0;

	for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
	{
		const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[TrackIndex];

		// **时长 = 载体求值**（2026-10-12 换型：静态 `double` → `FTcsParamValue`）。
		//
		// **求值结果钳下界到 0**：负数时长没有语义（"立刻到期"应当由 `Duration = 0` 或
		// `CDR = 1` 表达）——与 `EffectiveDuration` 的既有钳制同款。
		//
		// **CDR 快照语义不变**：`CooldownPct` 仍是**循环外取一次**（见上），作用的仍是
		// "本轨时长的求值结果" ⇒ "中途变化不追溯"逐字成立。
		const double BaseDuration = FMath::Max(0.0, Track.Duration.Evaluate(DurationCtx));

		// 生效时长 = 时长 × (1 - CDR)。
		const double EffectiveDuration = FMath::Max(0.0, BaseDuration * (1.0 - CooldownPct));
		const double EndTimeSeconds = Now + EffectiveDuration;

		if (Track.GroupTag.IsValid())
		{
			// ── 组轨：**单位级共享槽** ──
			TMap<FGameplayTag, FTcsCooldownGroupState>& Slots =
				Subsystem.CooldownGroups.FindOrAdd(Unit);

			if (FTcsCooldownGroupState* Existing = Slots.Find(Track.GroupTag))
			{
				// **同组已有活槽 ⇒ 复用，不重起不叠加**（这是公共冷却的定义：重起会让
				// "放完 A 立刻放 B" 把 GCD 刷满，而 GCD 存在的意义恰恰是不许这样做）。
				// 判据用**时间**而非句柄：时间才是权威（见 `IsEntryOnCooldown` 的同条判据）。
				//
				// **★ 判据 MUST 只取时间（fresh-context 复核抓到的 Minor）**：初版写成
				// `EndTimeSeconds > Now && ExpiryEntry.IsValid()`——**多加的那个句柄条件与注释自相矛盾**，
				// 且后果正是上面刚说的禁忌形态：一个"时间还活着、句柄却丢了"的槽会**掉进重建分支**
				// ⇒ **用新时长把 GCD 重起一遍**（"放完 A 立刻放 B 刷满 GCD"）。
				// 今天没有哪条路径会造出"时间活、句柄丢"（各清点都同时清两者），故它是**潜伏**的；
				// 但判据必须自洽。⇒ 时间活 ⇒ **一律复用**；句柄丢了则**按剩余时长补挂**（见下），
				// **MUST NOT 按新时长重起**。
				if (Existing->EndTimeSeconds > Now)
				{
					// 句柄丢失 ⇒ 该槽本会**永不解除**（没有人会来触发到期）⇒ 按**剩余时长**补挂一枚，
					// 使"复用"与"终会解除"同时成立（**MUST NOT** 用 `EffectiveDuration` 重起——
					// 那是把 GCD 刷满）。
					if (!Existing->ExpiryEntry.IsValid())
					{
						const double Remaining = Existing->EndTimeSeconds - Now;
						const FGameplayTag RepairGroupTag = Existing->GroupTag;
						TWeakObjectPtr<UTcsSkillSubsystem> WeakForRepair(&Subsystem);
						Existing->ExpiryEntry = Clock->PushExpiry(Existing->EndTimeSeconds, Unit.Id,
							[WeakForRepair, Unit, RepairGroupTag](uint64 /*OwnerId*/)
							{
								if (UTcsSkillSubsystem* Live = WeakForRepair.Get())
								{
									FTcsCooldownOps::ExpireGroup(*Live, Unit, RepairGroupTag);
								}
							});

						UE_LOG(LogTcsSkill, Warning,
							TEXT("组冷却槽补挂到期条目：单位=%lld 组=%s（时间仍活但句柄丢失，剩余=%.3f）"),
							Unit.Id, *Track.GroupTag.ToString(), Remaining);
					}

					++StartedCount;
					continue;
				}

				// 已过期的槽：撤掉残留到期条目（可能是别的路径遗留）再原地重写。
				// **不静默保留**——它会让该组"永久堵死"（症状 = 该组技能再也放不出来、零日志）。
				if (Existing->ExpiryEntry.IsValid())
				{
					Clock->CancelExpiry(Existing->ExpiryEntry);
				}

				UE_LOG(LogTcsSkill, Log,
					TEXT("组冷却槽重建：单位=%lld 组=%s（旧槽已过期）"),
					Unit.Id, *Track.GroupTag.ToString());
			}

			FTcsCooldownGroupState& Slot = Slots.FindOrAdd(Track.GroupTag);
			Slot.Total = EffectiveDuration;
			Slot.EndTimeSeconds = EndTimeSeconds;
			Slot.GroupTag = Track.GroupTag;

			// **`OwnerId` 只填归属读数**（堆零领域词汇）；**定位靠 lambda 捕获**——
			// MUST NOT 把定位职责压给 `OwnerId`（那会与 R6 已确立的堆使用约定造出第二真相：
			// 同一个堆、两种回调约定）。先例原文 = `TcsCastOps_Phase.cpp:198-206`。
			Slot.ExpiryEntry = Clock->PushExpiry(EndTimeSeconds, Unit.Id,
				[WeakSubsystem, Unit, GroupTag = Track.GroupTag](uint64 /*OwnerId*/)
				{
					if (UTcsSkillSubsystem* Live = WeakSubsystem.Get())
					{
						FTcsCooldownOps::ExpireGroup(*Live, Unit, GroupTag);
					}
				});

			++StartedCount;
			continue;
		}

		// ── 自身轨：真值住条目 ──
		if (EffectiveDuration <= 0.0)
		{
			// 零时长轨道 = "起了但立刻清" ⇒ 状态保持默认（未在跑），零到期条目
			// （入一个立刻到期的条目毫无收益，只会在下一帧泵点空跑一次）
			continue;
		}

		FTcsCooldownTrackState& State = Entry->CooldownTracks[TrackIndex];

		// 重复起冷 = **重启**：先撤旧锚点，避免旧条目到期后误清新冷
		// （旧回调用的是同一 `(Unit, EntryHandle, TrackIndex)` ⇒ 它认不出自己已过期）
		if (State.ExpiryEntry.IsValid())
		{
			Clock->CancelExpiry(State.ExpiryEntry);
		}

		State.Total = EffectiveDuration;
		State.EndTimeSeconds = EndTimeSeconds;
		State.ExpiryEntry = Clock->PushExpiry(EndTimeSeconds, Unit.Id,
			[WeakSubsystem, Unit, EntryHandle, TrackIndex](uint64 /*OwnerId*/)
			{
				if (UTcsSkillSubsystem* Live = WeakSubsystem.Get())
				{
					FTcsCooldownOps::ExpireTrack(*Live, Unit, EntryHandle, TrackIndex);
				}
			});

		++StartedCount;
	}

	UE_LOG(LogTcsSkill, Log,
		TEXT("冷却已起：单位=%lld 定义=%s 轨道 %d/%d 起冷 CDR=%.3f 时刻=%.3f"),
		Unit.Id, *Entry->DefTag.ToString(), StartedCount, TrackCount, CooldownPct, Now);

	// **事件排在全部轨道写完槽之后**（`Task 4 Step 4`）：订阅者在回调里查 `IsOnCooldown`
	// 时应当能看到**完整**的冷却状态，而不是"第一条轨已起、后两条还没起"的中间态。
	// **起冷事件是"一条事件对应一次起冷"**：即便一次起了多轨，也只发一枚
	// （判据：订阅者的语义是"这个技能进冷却了"，不是"第 N 条轨进冷却了"；
	// 逐轨发 N 枚会让最简单的用例收到 N 次，而它们的信息量完全相同）。
	if (StartedCount > 0)
	{
		// 载荷的轨道读数取**第一条实际起的轨**（订阅者若要逐轨读数，MUST 经查询口按 `TrackIndex` 取
		// ——`D5-15` 的"真值在 Entry，UI 轮询"正是这个意思）
		const int32 FirstIndex = [&]() -> int32
		{
			for (int32 i = 0; i < TrackCount; ++i)
			{
				const FTcsCooldownTrack& T = Def->Cooldown.Tracks[i];
				if (T.GroupTag.IsValid())
				{
					if (const TMap<FGameplayTag, FTcsCooldownGroupState>* Slots =
						FindLiveGroupSlots(Subsystem, Unit))
					{
						if (const FTcsCooldownGroupState* S = Slots->Find(T.GroupTag))
						{
							if (S->EndTimeSeconds > Now) { return i; }
						}
					}
				}
				else if (Entry->CooldownTracks.IsValidIndex(i)
					&& Entry->CooldownTracks[i].EndTimeSeconds > Now)
				{
					return i;
				}
			}

			return -1;
		}();

		if (FirstIndex >= 0)
		{
			const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[FirstIndex];
			double Total = 0.0;
			double End = 0.0;
			if (Track.GroupTag.IsValid())
			{
				if (const TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit))
				{
					if (const FTcsCooldownGroupState* S = Slots->Find(Track.GroupTag))
					{
						Total = S->Total;
						End = S->EndTimeSeconds;
					}
				}
			}
			else if (Entry->CooldownTracks.IsValidIndex(FirstIndex))
			{
				Total = Entry->CooldownTracks[FirstIndex].Total;
				End = Entry->CooldownTracks[FirstIndex].EndTimeSeconds;
			}

			BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownStarted, *Entry,
				FirstIndex, Track.GroupTag, Total, FMath::Max(0.0, End - Now), End);
		}
	}

	return StartedCount;
}



// 判可放
bool FTcsCooldownOps::IsEntryOnCooldown(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData& Def,
	FString* OutHeldBy)
{
	const int32 TrackCount = Def.Cooldown.Tracks.Num();
	if (TrackCount == 0)
	{
		// **空 Tracks ⇒ 恒可放**（`D5-15`："默认无冷却"）——本条是"不消耗/无冷却"档的判据落点
		return false;
	}

	const UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		// 时钟不可得 ⇒ 没有"剩余多久"这回事 ⇒ 判为**不阻拦**（与"未注入实体查询 ⇒ 降级放行"同口径：
		// 设施缺席 MUST NOT 把一切拦死）。Log 级、零红字。
		return false;
	}

	const double Now = Clock->GetClock().Elapsed;

	// 单位级组槽（可能为 nullptr = 该单位没有活组槽）
	const TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Entry.Unit);

	for (int32 TrackIndex = 0; TrackIndex < TrackCount; ++TrackIndex)
	{
		const FTcsCooldownTrack& Track = Def.Cooldown.Tracks[TrackIndex];

		if (Track.GroupTag.IsValid())
		{
			// ── 组轨：查单位级共享槽（**这就是真 GCD 的判据落点**） ──
			if (Slots)
			{
				if (const FTcsCooldownGroupState* Slot = Slots->Find(Track.GroupTag))
				{
					if (Slot->EndTimeSeconds > Now)
					{
						if (OutHeldBy)
						{
							*OutHeldBy = FString::Printf(TEXT("组轨[%s] 剩余=%.3f"),
								*Track.GroupTag.ToString(), Slot->EndTimeSeconds - Now);
						}

						return true;
					}
				}
			}

			continue;
		}

		// ── 自身轨：查条目状态（**下标可能越界**——定义被热改成更多轨道） ──
		if (!Entry.CooldownTracks.IsValidIndex(TrackIndex))
		{
			// 缺的轨道按"未在跑"算（**MUST NOT 越界读**）：定义加了轨道但本轮还没起过冷
			continue;
		}

		const FTcsCooldownTrackState& State = Entry.CooldownTracks[TrackIndex];
		if (State.EndTimeSeconds > Now)
		{
			if (OutHeldBy)
			{
				*OutHeldBy = FString::Printf(TEXT("自身轨[%d] 剩余=%.3f"),
					TrackIndex, State.EndTimeSeconds - Now);
			}

			return true;
		}
	}

	return false;
}

bool FTcsCooldownOps::IsEntryOnCooldownByHandle(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	FString* OutHeldBy)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		// 悬空句柄 ⇒ 判为**不阻拦**（交给上游按"未学"具名拒绝，那是更精确的原因）
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def)
	{
		// 定义取不到 ⇒ 不阻拦（上游门禁⑥ 会给出 `SAR_DefInvalid` 这个更精确的答案）
		return false;
	}

	return IsEntryOnCooldown(Subsystem, *Entry, *Def, OutHeldBy);
}

double FTcsCooldownOps::GetTrackRemaining(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	int32 TrackIndex)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		return 0.0;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def || !Def->Cooldown.Tracks.IsValidIndex(TrackIndex))
	{
		return 0.0;
	}

	const UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		return 0.0;
	}

	const double Now = Clock->GetClock().Elapsed;
	const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[TrackIndex];

	if (Track.GroupTag.IsValid())
	{
		// **组轨的读数 MUST 取自共享槽**——按条目里那份（本来就不写的）状态读会恒得 0，
		// 表现为"UI 显示没冷却、但技能放不出来"。
		const TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit);
		const FTcsCooldownGroupState* Slot = Slots ? Slots->Find(Track.GroupTag) : nullptr;
		return (Slot && Slot->EndTimeSeconds > Now) ? (Slot->EndTimeSeconds - Now) : 0.0;
	}

	if (!Entry->CooldownTracks.IsValidIndex(TrackIndex))
	{
		return 0.0;
	}

	const FTcsCooldownTrackState& State = Entry->CooldownTracks[TrackIndex];
	return (State.EndTimeSeconds > Now) ? (State.EndTimeSeconds - Now) : 0.0;
}

double FTcsCooldownOps::GetTrackTotal(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	int32 TrackIndex)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		return 0.0;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def || !Def->Cooldown.Tracks.IsValidIndex(TrackIndex))
	{
		return 0.0;
	}

	const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[TrackIndex];

	if (Track.GroupTag.IsValid())
	{
		const TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit);
		const FTcsCooldownGroupState* Slot = Slots ? Slots->Find(Track.GroupTag) : nullptr;
		return Slot ? Slot->Total : 0.0;
	}

	return Entry->CooldownTracks.IsValidIndex(TrackIndex)
		? Entry->CooldownTracks[TrackIndex].Total : 0.0;
}

int32 FTcsCooldownOps::GetTrackCount(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle)
{
	const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	const FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		return 0;
	}

	// **读定义而不是读运行态**：该读数的用途是"给 UI 画几根条"，而"这个技能有几条轨道"是**定义**的
	// 属性；数运行态会让"冷却都跑完了"与"这个技能根本没有冷却"给出同一个 0（两者含义完全不同）。
	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	return Def ? Def->Cooldown.Tracks.Num() : 0;
}



// 改轨与清轨
bool FTcsCooldownOps::AdjustTrack(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	int32 TrackIndex,
	double Delta)
{
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("调整冷却被拒：账本条目悬空（单位=%lld 条目=%d/%d）"),
			Unit.Id, EntryHandle.Index, EntryHandle.Generation);
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def || !Def->Cooldown.Tracks.IsValidIndex(TrackIndex))
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("调整冷却被拒：轨道下标越界（单位=%lld 下标=%d 轨道数=%d）"),
			Unit.Id, TrackIndex, Def ? Def->Cooldown.Tracks.Num() : 0);
		return false;
	}

	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		return false;
	}

	const double Now = Clock->GetClock().Elapsed;
	const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[TrackIndex];

	// 目标 = 组槽 或 自身轨（两者共用同一段"改剩余 + 重挂到期堆"逻辑，故取引用后统一处理）
	FTcsCooldownTrackState* SelfState = nullptr;
	FTcsCooldownGroupState* GroupState = nullptr;

	if (Track.GroupTag.IsValid())
	{
		TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit);
		GroupState = Slots ? Slots->Find(Track.GroupTag) : nullptr;
	}
	else if (Entry->CooldownTracks.IsValidIndex(TrackIndex))
	{
		SelfState = &Entry->CooldownTracks[TrackIndex];
	}

	FTcsTimeEntryHandle* ExpiryEntry = GroupState ? &GroupState->ExpiryEntry
		: (SelfState ? &SelfState->ExpiryEntry : nullptr);
	double* StateTotal = GroupState ? &GroupState->Total : (SelfState ? &SelfState->Total : nullptr);
	double* StateEnd = GroupState ? &GroupState->EndTimeSeconds
		: (SelfState ? &SelfState->EndTimeSeconds : nullptr);

	if (!ExpiryEntry || !StateTotal || !StateEnd)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("调整冷却跳过：该轨当前未在跑（单位=%lld 下标=%d）"), Unit.Id, TrackIndex);
		return false;
	}

	// 时间判据才是权威（同 `IsEntryOnCooldown`）：已过期的轨不构成"在跑"
	if (*StateEnd <= Now)
	{
		UE_LOG(LogTcsSkill, Log, TEXT("调整冷却跳过：该轨已到期（单位=%lld 下标=%d）"), Unit.Id, TrackIndex);
		return false;
	}

	const double NewEnd = *StateEnd + Delta;

	if (NewEnd <= Now)
	{
		// 缩短到不剩 ⇒ **等于清轨**（提前结束）。复用清轨路径而不是"入一个立刻到期的条目"：
		// 后者会在下一帧泵点空跑一次，且期间 `IsEntryOnCooldown` 的读数依赖"时间判据"才正确。
		return ClearTrack(Subsystem, Unit, EntryHandle, TrackIndex);
	}

	// **`Total` 只增不减**（维持 `Remaining <= Total` 的不变量——否则 UI 径向会画出超过 100% 的弧）。
	// 缩短不让分母变小："这条冷却是多久"是**已经发生过的事实**。
	*StateTotal = FMath::Max(*StateTotal, NewEnd - Now);
	*StateEnd = NewEnd;

	// **★ 改完 MUST 重挂到期堆**：只改 `EndTimeSeconds` 而不重挂，堆里的旧到期时刻会继续生效
	// ⇒ 症状是"改了却不生效"或"提前到期"（两个方向都可能，取决于调整方向）。
	if (ExpiryEntry->IsValid())
	{
		Clock->CancelExpiry(*ExpiryEntry);
	}

	if (GroupState)
	{
		const FGameplayTag GroupTag = GroupState->GroupTag;
		TWeakObjectPtr<UTcsSkillSubsystem> WeakSubsystem(&Subsystem);
		*ExpiryEntry = Clock->PushExpiry(NewEnd, Unit.Id,
			[WeakSubsystem, Unit, GroupTag](uint64 /*OwnerId*/)
			{
				if (UTcsSkillSubsystem* Live = WeakSubsystem.Get())
				{
					FTcsCooldownOps::ExpireGroup(*Live, Unit, GroupTag);
				}
			});
	}
	else
	{
		TWeakObjectPtr<UTcsSkillSubsystem> WeakSubsystem(&Subsystem);
		*ExpiryEntry = Clock->PushExpiry(NewEnd, Unit.Id,
			[WeakSubsystem, Unit, EntryHandle, TrackIndex](uint64 /*OwnerId*/)
			{
				if (UTcsSkillSubsystem* Live = WeakSubsystem.Get())
				{
					FTcsCooldownOps::ExpireTrack(*Live, Unit, EntryHandle, TrackIndex);
				}
			});
	}

	UE_LOG(LogTcsSkill, Log, TEXT("冷却已调整：单位=%lld 下标=%d 增量=%.3f 新到期=%.3f"),
		Unit.Id, TrackIndex, Delta, NewEnd);

	// **调轨事件**（`AdjustCooldown` 已承诺发它——`D5-16`）：载荷的 `Remaining` 是**新值**
	// （订阅者据此刷新 UI 径向；`Total` 可能因"只增不减"而大于原值，那是刻意的）。
	BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownUpdated, *Entry,
		TrackIndex, Track.GroupTag, *StateTotal, FMath::Max(0.0, NewEnd - Now), NewEnd);

	return true;
}

bool FTcsCooldownOps::ClearTrack(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	int32 TrackIndex)
{
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry)
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("清冷却被拒：账本条目悬空（单位=%lld 条目=%d/%d）"),
			Unit.Id, EntryHandle.Index, EntryHandle.Generation);
		return false;
	}

	const FTcsSkillDefData* Def = Subsystem.GetRegisteredSkillDef(Entry->DefTag);
	if (!Def || !Def->Cooldown.Tracks.IsValidIndex(TrackIndex))
	{
		UE_LOG(LogTcsSkill, Warning, TEXT("清冷却被拒：轨道下标越界（单位=%lld 下标=%d）"), Unit.Id, TrackIndex);
		return false;
	}

	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();
	if (!Clock)
	{
		return false;
	}

	const double Now = Clock->GetClock().Elapsed;
	const FTcsCooldownTrack& Track = Def->Cooldown.Tracks[TrackIndex];

	if (Track.GroupTag.IsValid())
	{
		TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit);
		FTcsCooldownGroupState* Slot = Slots ? Slots->Find(Track.GroupTag) : nullptr;
		if (!Slot || Slot->EndTimeSeconds <= Now)
		{
			return false;
		}

		// **读数 MUST 在复位之前拷出来**（下面整体复位会把它们清成 0，而事件载荷要用原值）
		const double ClearedTotal = Slot->Total;
		const double ClearedEnd = Slot->EndTimeSeconds;

		// **撤锚点再整体复位**（顺序有理由）：`CancelExpiry` 对悬空句柄会 `ensureMsgf`，
		// 故先判有效性；复位用整体赋值而非逐字段（池槽复用纪律的同一条）。
		if (Slot->ExpiryEntry.IsValid())
		{
			Clock->CancelExpiry(Slot->ExpiryEntry);
		}

		*Slot = FTcsCooldownGroupState();

		// 组槽解除 = 该组整条线消失 ⇒ 直接移除（不需要逐条目遍历——这正是"组槽住单位"的收益）
		Slots->Remove(Track.GroupTag);

		// **★ 内层表空了 MUST 连外层键一起删（fresh-context 复核抓到的 Minor 泄漏）**：
		// 只摘内层条目会让 `CooldownGroups` 里留下**永不使用的空键**，随"清过组的单位数"线性增长
		// （`ExpireGroup` 早已这么做——本处是同一纪律的第二处落点，初版漏了）。
		if (Slots->Num() == 0)
		{
			Subsystem.CooldownGroups.Remove(Unit);
		}

		UE_LOG(LogTcsSkill, Log, TEXT("冷却已清：单位=%lld 下标=%d 组=%s"),
			Unit.Id, TrackIndex, *Track.GroupTag.ToString());

		BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownUpdated, *Entry,
			TrackIndex, Track.GroupTag, ClearedTotal, 0.0, ClearedEnd);
	}
	else
	{
		if (!Entry->CooldownTracks.IsValidIndex(TrackIndex))
		{
			return false;
		}

		FTcsCooldownTrackState& State = Entry->CooldownTracks[TrackIndex];
		if (State.EndTimeSeconds <= Now)
		{
			return false;
		}

		const double ClearedTotal = State.Total;
		const double ClearedEnd = State.EndTimeSeconds;

		if (State.ExpiryEntry.IsValid())
		{
			Clock->CancelExpiry(State.ExpiryEntry);
		}

		State = FTcsCooldownTrackState();

		UE_LOG(LogTcsSkill, Log, TEXT("冷却已清：单位=%lld 下标=%d 组=%s"),
			Unit.Id, TrackIndex, *Track.GroupTag.ToString());

		BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownUpdated, *Entry,
			TrackIndex, Track.GroupTag, ClearedTotal, 0.0, ClearedEnd);
	}

	return true;
}



// 生命周期配对
void FTcsCooldownOps::ReleaseEntryCooldowns(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsLearnedSkillEntry& Entry)
{
	UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem();

	for (FTcsCooldownTrackState& State : Entry.CooldownTracks)
	{
		// **先判有效性再撤**（`CancelExpiry` 对悬空句柄是 `ensureMsgf`）
		if (Clock && State.ExpiryEntry.IsValid())
		{
			Clock->CancelExpiry(State.ExpiryEntry);
		}
	}

	Entry.CooldownTracks.Reset();

	// **组槽刻意不动**（本函数只清"属于这个条目"的东西）：组槽是**单位级**的，它有自己的到期条目，
	// 会自行到期解除。此处若把它一并移除，会**替同组的其它技能解除公共冷却**
	// （症状：撤销 A 之后，同组的 B 立刻能放——而 GCD 本该还在）。
	// ⇒ 组槽的生命周期归 `ExpireGroup` 与 `ReleaseUnitCooldowns`，**MUST NOT 在此越界清理**。
	UE_LOG(LogTcsSkill, Log, TEXT("条目冷却已摘：单位=%lld 条目=%d/%d（组槽留待自行到期）"),
		Unit.Id, Entry.Handle.Index, Entry.Handle.Generation);
}

void FTcsCooldownOps::ReleaseUnitCooldowns(UTcsSkillSubsystem& Subsystem, FTcsCombatEntityHandle Unit)
{
	// 组槽全部撤锚点后整表删除（单位都没了，没有"留给谁"的问题）
	if (TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit))
	{
		if (UTcsClockSubsystem* Clock = Subsystem.GetClockSubsystem())
		{
			for (const TPair<FGameplayTag, FTcsCooldownGroupState>& Pair : *Slots)
			{
				if (Pair.Value.ExpiryEntry.IsValid())
				{
					Clock->CancelExpiry(Pair.Value.ExpiryEntry);
				}
			}
		}

		Subsystem.CooldownGroups.Remove(Unit);
	}
}

double FTcsCooldownOps::EvaluateCooldownPct(
	UTcsSkillSubsystem& Subsystem,
	const FTcsLearnedSkillEntry& Entry,
	const FTcsSkillDefData* Def)
{
	double Value = 0.0;
	if (!FTcsParamChainOps::EvaluateParamKey(Subsystem, Entry, Def, Tag_TcsSkillParam_CooldownPct, Value))
	{
		// **未命中 ⇒ 无缩减**（正常路径、零日志）：多数技能不会有 CDR 修正
		return 0.0;
	}

	if (!FMath::IsFinite(Value))
	{
		// 非有限值 = 配置/求值出了真问题（NaN / Inf）⇒ **报出来**（MUST NOT 静默按 0 比较）
		UE_LOG(LogTcsSkill, Warning,
			TEXT("冷却缩减取值非有限（单位=%lld 值=%f）——按无缩减处理"), Entry.Unit.Id, Value);
		return 0.0;
	}

	// **钳制到 `[0, 1]`（确切的上下界就写在这里，不许"合理处理"）**：
	// - 下界 0：**负的 CDR 不得变成冷却延长**（"配了个负减免反而被罚"是静默怪相）；
	// - 上界 1：**不得产生负时长**（1 已表示"冷却为 0"）。
	return FMath::Clamp(Value, 0.0, 1.0);
}



// 到期回调
void FTcsCooldownOps::ExpireTrack(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FTcsSkillEntryHandle EntryHandle,
	int32 TrackIndex)
{
	FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	FTcsLearnedSkillEntry* Entry = Bucket ? Bucket->Find(EntryHandle) : nullptr;
	if (!Entry || !Entry->CooldownTracks.IsValidIndex(TrackIndex))
	{
		// 代际失配 / 条目已撤销 / 定义被热改成更少轨道 = 时序竞态 ⇒ 静默丢弃
		// （条目本该已被 `ReleaseEntryCooldowns` 撤锚点；走到这里说明该路径没撤成，属边界）
		return;
	}

	// **★ 第一条语句即整体复位**（防 ensure 的关键，理由见头文件的两条纪律）：
	// 复位后 `ExpiryEntry.IsValid()` 为假 ⇒ 后续任何清理路径（含终结路径）天然跳过本句柄，
	// 而"到期回调正在跑"那一刻它自己的条目**仍然有效**（`FireDueItem` 在回调返回后才释放）。
	//
	// **读数 MUST 在复位之前拷出来**（下面整体复位会把它们清成 0，而事件载荷要用原值）。
	const double ExpiredTotal = Entry->CooldownTracks[TrackIndex].Total;
	const double ExpiredEnd = Entry->CooldownTracks[TrackIndex].EndTimeSeconds;

	Entry->CooldownTracks[TrackIndex] = FTcsCooldownTrackState();

	UE_LOG(LogTcsSkill, Log, TEXT("冷却到期：单位=%lld 条目=%d/%d 轨道=%d"),
		Unit.Id, EntryHandle.Index, EntryHandle.Generation, TrackIndex);

	// **到期事件是权威侧**：`Remaining` 恒 0（"剩余时间到点了"就是它的定义）。
	BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownEnded, *Entry,
		TrackIndex, FGameplayTag(), ExpiredTotal, 0.0, ExpiredEnd);

	// **无需额外的"允许再次激活"状态**：门禁③ 每帧现算（`IsEntryOnCooldown` 按时间判据），
	// 不存在缓存态要清——这正是"不缓存可放性"的收益。
}

void FTcsCooldownOps::ExpireGroup(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit,
	FGameplayTag GroupTag)
{
	TMap<FGameplayTag, FTcsCooldownGroupState>* Slots = FindLiveGroupSlots(Subsystem, Unit);
	if (!Slots)
	{
		// 单位已注销 / 该组已被清 ⇒ 时序竞态，静默丢弃
		return;
	}

	FTcsCooldownGroupState* Slot = Slots->Find(GroupTag);
	if (!Slot)
	{
		return;
	}

	// **读数 MUST 在复位之前拷出来**（下面整体复位会把它们清成 0，而事件载荷要用原值）
	const double ExpiredTotal = Slot->Total;
	const double ExpiredEnd = Slot->EndTimeSeconds;

	// **整体复位 + 移除**（同 `ClearTrack` 的组轨分支）：组槽到期 = 该组整条线解除。
	// 复位在前可保证"万一后面还有路径看这一槽"时拿到的是干净状态（本仓"先整体重置再用/再弃"纪律）。
	*Slot = FTcsCooldownGroupState();
	Slots->Remove(GroupTag);

	// 空表顺手删掉，免得单位级表里留下永不使用的空键
	if (Slots->Num() == 0)
	{
		Subsystem.CooldownGroups.Remove(Unit);
	}

	UE_LOG(LogTcsSkill, Log, TEXT("组冷却到期：单位=%lld 组=%s"), Unit.Id, *GroupTag.ToString());

	// **组轨到期事件的载荷要能定位到"哪个条目"**：而组槽是**单位级**的、不记"谁起的它"
	// ⇒ 广播时遍历该单位全部条目，找**配了本组轨道**的那些，各发一枚
	// （判据：订阅者的语义是"我这条技能的公共冷却结束了"——它需要自己的条目身份才能刷新 UI；
	// 只发一枚无身份的组事件会让订阅者无从知道该更新哪一行）。
	//
	// **★ 两段式遍历（MUST；fresh-context 复核抓到的 Important 缺陷）**：`BroadcastCooldownEvent`
	// 走**同步派发** ⇒ 订阅者可在回调里 `UnregisterUnit`（`RemoveBucket` 释放桶）或
	// `GrantSkill`（`AllocateSlot` 的 `Entries.Emplace()` **搬移**）
	// ⇒ **在 `ForEach` 里广播 = 一边遍历一边被拆掉遍历对象**（heap-use-after-free）。
	// **判据 = 本文件与全仓既有纪律**："遍历期间 MUST NOT 增删条目"（`FSkillBucket::ForEach` 的注释原文）、
	// 两段式先例 = `FTcsSkillOps::RevokeBySource`（先收集命中句柄，遍历结束后再动）。
	// ⇒ 本处**先收集**（句柄 + 该发哪条轨），**再广播**。
	struct FR65GroupEndedTarget
	{
		FTcsSkillEntryHandle EntryHandle;
		int32 TrackIndex = -1;
	};

	TArray<FR65GroupEndedTarget> Targets;
	if (const FSkillBucket* Bucket = Subsystem.Registry.FindBucket(Unit))
	{
		Bucket->ForEach([&Subsystem, &GroupTag, &Targets](const FTcsLearnedSkillEntry& Candidate)
		{
			// **本段只读、零广播**（收集期不得有任何可能触发增删的调用）
			const FTcsSkillDefData* CandidateDef = Subsystem.GetRegisteredSkillDef(Candidate.DefTag);
			if (!CandidateDef)
			{
				return true;
			}

			for (int32 TrackIndex = 0; TrackIndex < CandidateDef->Cooldown.Tracks.Num(); ++TrackIndex)
			{
				if (CandidateDef->Cooldown.Tracks[TrackIndex].GroupTag == GroupTag)
				{
					Targets.Add(FR65GroupEndedTarget{Candidate.Handle, TrackIndex});
					break;
				}
			}

			return true;
		});
	}

	// **第二段：逐个广播**（每次广播前**重新解析条目**——前一次的广播可能已把它撤销）
	for (const FR65GroupEndedTarget& Target : Targets)
	{
		FSkillBucket* LiveBucket = Subsystem.Registry.FindBucket(Unit);
		FTcsLearnedSkillEntry* LiveEntry = LiveBucket ? LiveBucket->Find(Target.EntryHandle) : nullptr;
		if (!LiveEntry)
		{
			// 已被前一次广播的订阅者撤销 ⇒ 静默跳过（它不在账本上了，没有"谁的冷却结束"要通知）
			continue;
		}

		BroadcastCooldownEvent(Subsystem, Tag_TcsEvent_Cast_CooldownEnded, *LiveEntry,
			Target.TrackIndex, GroupTag, ExpiredTotal, 0.0, ExpiredEnd);
	}
}



// 内核
TMap<FGameplayTag, FTcsCooldownGroupState>* FTcsCooldownOps::FindGroupSlots(
	UTcsSkillSubsystem& Subsystem,
	FTcsCombatEntityHandle Unit)
{
	return Subsystem.CooldownGroups.Find(Unit);
}
