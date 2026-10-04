// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateEvents.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



// 施加
EApplyResult FTcsStateOps::Apply(
	UTcsStateSubsystem& Subsystem,
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsCombatEntityHandle Instigator,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	const TMap<FGameplayTag, double>& Overrides)
{
	// 目标无效 = 调用方错误（空实体句柄）——拒绝而不 ensure（同门面其余拒绝面口径）
	if (!Target.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态施加被拒：目标实体句柄无效（DefTag=%s）"), *DefTag.ToString());
		return EApplyResult::EAR_Rejected;
	}

	// 定义必须已在本世界登记（定义库逐世界登记进来；未登记 = 内容缺口，不 ensure）
	const FTcsBuffDef* Def = Subsystem.GetRegisteredStateDef(DefTag);
	if (!Def)
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态施加被拒：定义未在本世界登记（DefTag=%s）"), *DefTag.ToString());
		return EApplyResult::EAR_Rejected;
	}

	FStateBucket& Bucket = Subsystem.Registry.FindOrAddBucket(Target);

	// 本轮共存判据 = 同单位 + 同 DefTag（`GroupBy = None` 语义）。
	// **这里是 R5 Task 5 五轴共存决策的替换点**：Task 5 换成"组键 + 上限 + 溢出 + 数值叠加 + 时长刷新"
	// 的确定性决策，事件面与返回值语义不变。两段式（先收句柄再动）——`ForEach` 期间不得增删实例。
	FTcsStateHandle ExistingHandle;
	Bucket.ForEach([&DefTag, &ExistingHandle](const FTcsStateInstance& Instance)
	{
		if (Instance.DefTag == DefTag)
		{
			ExistingHandle = Instance.Handle;
			return false;
		}

		return true;
	});

	if (FTcsStateInstance* Existing = Bucket.Find(ExistingHandle))
	{
		// 刷新：更新可确定的字段。**`Source` 刻意不动**——它是级联撤销锚点，
		// 已被触发行/修正器等下游按原值登记过；换来源会让"同一个状态 = 同一个来源"失效。
		// `Instigator` 反过来：它答"最近一次施加是谁发起的"，故刷新时更新。
		Existing->Level = Def->LevelBase;
		Existing->Instigator = Instigator.IsValid() ? Instigator : Target;

		// 快照重建（D3-12："修改 = 重新施加，新 payload 覆盖旧的"）
		FTcsStateEvaluateContext Ctx;
		MakeContext(Ctx, Subsystem, Target, Existing->Instigator, Def->LevelBase, ParamTable);
		BuildSnapshot(Existing->ParamSnapshot, *Def, Ctx, Overrides);

		// 修正器重物化：**先按来源摘旧、再按新快照挂新**（同一批内完成）。
		// 不这么做就会出现"快照是新值、账本还是旧值"的静默不一致（刷新 = 重新施加的账本面）。
		MountModifiers(Subsystem, *Def, *Existing, /*bStripFirst=*/true);

		// 挂载提交会重算 + 广播（属性变更事件）⇒ 订阅者可重入状态操作（移除本实例 / 注销单位），
		// 那会让上面那个实例指针与桶引用双双失效（槽位释放后还可能被复用）⇒ MUST 重新定位。
		FStateBucket* LiveBucket = Subsystem.Registry.FindBucket(Target);
		Existing = LiveBucket ? LiveBucket->Find(ExistingHandle) : nullptr;
		if (!Existing)
		{
			// 重入移除的极端路径：刷新动作本身已完成（快照 + 修正器都按本次施加更新过），
			// 实例却已不在册 ⇒ 不再补播 `Refreshed`（对已不在册的实例广播是错语义）。
			UE_LOG(LogTcsState, Warning,
				TEXT("状态刷新后实例已不在册（挂载提交期间被重入移除）：单位=%lld 定义=%s 句柄=%d/%d"),
				Target.Id, *DefTag.ToString(), ExistingHandle.Index, ExistingHandle.Generation);
			return EApplyResult::EAR_Refreshed;
		}

		Transit(Subsystem, Existing, EStatePhase::ESP_Expiring);

		// 时间条目重挂放在**广播之前**：订阅者在 `Refreshed` 回调里读到的时值/周期已是本次施加的结果
		// （放在广播之后会让回调读到上一轮的剩余时长——一处只有靠时序才能发现的错值）
		ScheduleTime(Subsystem, *Existing, *Def, Existing->ParamSnapshot, ParamTable, /*bRefresh=*/true);

		Broadcast(Subsystem, Tag_TcsEvent_State_Refreshed, *Existing, EStateRemoveCause::ESRC_Removed);
		Transit(Subsystem, Existing, EStatePhase::ESP_Active);

		UE_LOG(LogTcsState, Log, TEXT("状态刷新：单位=%lld 定义=%s 句柄=%d/%d 层数=%d 快照=%d 条 剩余=%.3f"),
			Target.Id, *DefTag.ToString(), Existing->Handle.Index, Existing->Handle.Generation,
			Existing->Stacks, Existing->ParamSnapshot.Num(), Existing->DurationRemaining);

		return EApplyResult::EAR_Refreshed;
	}

	// 新施加：分配槽位 → 填实例 → 快照 → 时间条目 → 广播。句柄在 `AllocateSlot` 后取得，
	// 其 `Index` 只在本函数内按值随句柄流转——`TArray` 扩容不使**值**失效（这是"传句柄不传指针"的纪律）。
	const FTcsStateHandle NewHandle = Bucket.AllocateSlot();
	FTcsStateInstance* NewInstance = Bucket.Find(NewHandle);
	check(NewInstance != nullptr);

	NewInstance->DefTag = DefTag;
	NewInstance->Handle = NewHandle;
	NewInstance->Unit = Target;
	NewInstance->Source = Source.IsValid() ? Source : Subsystem.SourceRegistry.Allocate();
	NewInstance->Instigator = Instigator.IsValid() ? Instigator : Target;
	NewInstance->Stacks = 1;
	NewInstance->Level = Def->LevelBase;
	NewInstance->Phase = EStatePhase::ESP_Active;
	NewInstance->DurationRemaining = 0.0;
	NewInstance->PeriodRemaining = 0.0;

	// 快照构建（D3-12）：覆盖优先、Def 兜底、值约定在写入点转规范值
	FTcsStateEvaluateContext Ctx;
	MakeContext(Ctx, Subsystem, Target, NewInstance->Instigator, Def->LevelBase, ParamTable);
	BuildSnapshot(NewInstance->ParamSnapshot, *Def, Ctx, Overrides);

	// 修正器物化 + 挂载（施加路径无需先摘：来源句柄是本次施加才发放的，账本上不可能有同来源条目）。
	// 位置在广播之前——订阅者在 `Applied` 回调里读到的属性已是挂载后的数值。
	MountModifiers(Subsystem, *Def, *NewInstance, /*bStripFirst=*/false);

	// 挂载提交会重算 + 广播 ⇒ 订阅者可重入状态操作 ⇒ MUST 重新定位实例与桶（理由同刷新路径）
	FStateBucket* LiveBucket = Subsystem.Registry.FindBucket(Target);
	FTcsStateInstance* LiveInstance = LiveBucket ? LiveBucket->Find(NewHandle) : nullptr;
	if (!LiveInstance)
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("状态施加后实例已不在册（挂载提交期间被重入移除）：单位=%lld 定义=%s 句柄=%d/%d"),
			Target.Id, *DefTag.ToString(), NewHandle.Index, NewHandle.Generation);
		return EApplyResult::EAR_Applied;
	}

	// 时间条目（放在广播之前——订阅者读到的是完整的实例）
	ScheduleTime(Subsystem, *LiveInstance, *Def, LiveInstance->ParamSnapshot, ParamTable, /*bRefresh=*/false);

	Broadcast(Subsystem, Tag_TcsEvent_State_Applied, *LiveInstance, EStateRemoveCause::ESRC_Removed);

	UE_LOG(LogTcsState, Log, TEXT("状态施加：单位=%lld 定义=%s 句柄=%d/%d 来源=%llu 等级=%d 快照=%d 条 剩余=%.3f 周期=%.3f"),
		Target.Id, *DefTag.ToString(), NewHandle.Index, NewHandle.Generation,
		LiveInstance->Source.Id, LiveInstance->Level, LiveInstance->ParamSnapshot.Num(),
		LiveInstance->DurationRemaining, LiveInstance->PeriodRemaining);

	return EApplyResult::EAR_Applied;
}



// 移除
bool FTcsStateOps::Remove(
	UTcsStateSubsystem& Subsystem,
	FTcsStateHandle Handle,
	EStateRemoveCause Cause)
{
	if (!Handle.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态移除被拒：句柄无效（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	const FTcsStateInstance* Found = Find(Subsystem, Handle);
	if (!Found)
	{
		// 代际失配 / 单位已注销 = 时序竞态，不是契约违规（不 ensure）
		UE_LOG(LogTcsState, Warning, TEXT("状态移除被拒：句柄悬空（%d/%d）"), Handle.Index, Handle.Generation);
		return false;
	}

	// 先把单位与槽位下标取出来（**按值**）：广播之后本函数不再持实例指针
	// （订阅者可在回调里增删状态，槽位可能被复用）。
	const FTcsCombatEntityHandle Unit = Found->Unit;
	const uint32 SlotIndex = static_cast<uint32>(Handle.Index);
	const FGameplayTag DefTag = Found->DefTag;
	const FTcsSourceHandle Source = Found->Source;

	// **修正器摘除排在"取实例指针"之前**（顺序有理由，不是随手排的）：`StripModifiers` 内的批提交
	// 会重算 + 广播（属性变更事件），订阅者可在其中重入状态操作（移除本实例 / 注销单位）——
	// 先摘除、再重新定位桶与实例，下面两道既有守卫（桶不存在 / 实例查不到）就同时接住了重入造成的失效。
	// 完整撤销顺序：**摘除修正器 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**。
	// 两处撤销（修正器 / 时间条目）都排在广播之前 ⇒ 订阅者读到的是"已经还清"的属性值与实例。
	StripModifiers(Subsystem, Unit, Source);

	FStateBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态移除被拒：单位桶不存在（单位=%lld）"), Unit.Id);
		return false;
	}

	// 撤销顺序（**硬约束**）：**撤时间条目** → `Expiring` → 广播 → 归还槽位。
	// 先广播后释放，订阅者才能在回调里用载荷句柄读到实例（否则"收到移除事件却查不到实例"）；
	// 撤条目排在最前，是因为条目持句柄——槽位复用后回调仍会到达，靠代际校验兜底是"能成立"，
	// 但先撤条目才是"不产生无谓回调"。
	FTcsStateInstance* Instance = Bucket->Find(Handle);
	if (!Instance)
	{
		// 摘除提交期间被重入者移除（或单位已注销）——按已移除处理，不重复归还槽位
		return false;
	}

	CancelTimeEntries(Subsystem, *Instance);
	Transit(Subsystem, Instance, EStatePhase::ESP_Expiring);

	// 广播前把实例复制一份：广播会让订阅者改桶（槽位可能被复用），故不用引用跨过广播。
	// **复制的是广播当时的快照**（阶段 = `Expiring`，正是"移除中"的语义）。
	const FTcsStateInstance Snapshot = *Instance;
	Broadcast(Subsystem, Cause == EStateRemoveCause::ESRC_Expired ? Tag_TcsEvent_State_Expired : Tag_TcsEvent_State_Removed,
		Snapshot, Cause);

	// 归还槽位（`ReleaseSlot` 清槽内容 + 代际 +1 使旧句柄悬空；销毁路径不需过渡守卫——
	// 槽位内容立刻被清零，"Stage 矩阵"对本实例到此为止）
	Bucket->ReleaseSlot(SlotIndex);

	UE_LOG(LogTcsState, Log, TEXT("状态移除：单位=%lld 定义=%s 句柄=%d/%d 原因=%d 来源=%llu"),
		Unit.Id, *DefTag.ToString(), Handle.Index, Handle.Generation, static_cast<int32>(Cause), Source.Id);

	return true;
}

int32 FTcsStateOps::UnregisterUnit(UTcsStateSubsystem& Subsystem, FTcsCombatEntityHandle Unit)
{
	const FStateBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		return 0;
	}

	// 两段式：先收集句柄（`ForEach` 期间不得增删实例），再逐个走同一条移除链。
	// MUST NOT 先删桶再广播——那时订阅者已经查不到实例。
	TArray<FTcsStateHandle> Targets;
	Bucket->ForEach([&Targets](const FTcsStateInstance& Instance)
	{
		Targets.Add(Instance.Handle);
		return true;
	});

	int32 RemovedCount = 0;
	for (const FTcsStateHandle& Target : Targets)
	{
		if (Remove(Subsystem, Target, EStateRemoveCause::ESRC_Removed))
		{
			++RemovedCount;
		}
	}

	Subsystem.Registry.RemoveBucket(Unit);

	UE_LOG(LogTcsState, Log, TEXT("单位状态注销：单位=%lld 移除=%d 条"), Unit.Id, RemovedCount);

	return RemovedCount;
}
