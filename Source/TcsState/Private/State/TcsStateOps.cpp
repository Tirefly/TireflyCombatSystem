// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateEvents.h"
#include "State/TcsStateStackFragment.h"
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

	// 组键与共存决策（**策略驱动**——R5 Task 5 落地，原"同单位 + 同 `DefTag` 即刷新"的暂用判据已替换）：
	// 组键 = `(单位, DefTag)` + 轴附加项，命中后按容量与溢出政策决定刷新 / 叠层 / 替换 / 拒绝。
	// 决策与后续更新流水住 `TcsStateOps_Stack.cpp`；本函数只做校验、决策分派与"新建"一支。
	FStateBucket* Bucket = &Subsystem.Registry.FindOrAddBucket(Target);

	FTcsStateStackRequest StackRequest;
	StackRequest.DefTag = DefTag;
	StackRequest.Unit = Target;
	StackRequest.Source = Source;
	StackRequest.Instigator = Instigator.IsValid() ? Instigator : Target;

	const FTcsStateStackDecision Decision = ResolveStackDecision(StackRequest, *Bucket, Def->StackPolicy);
	const FTcsStateHandle MemberHandle = Decision.ExistingHandle;

	// 满仓拒绝 / 自定义不接受：**业务结果不是错误**（`Log` 而非 `Warning`；实例不动、无任何广播）
	if (Decision.Kind == ETcsStateStackDecisionKind::Reject)
	{
		const FTcsStateInstance* Member = Bucket->Find(MemberHandle);
		UE_LOG(LogTcsState, Log, TEXT("状态施加被共存策略拒绝：单位=%lld 定义=%s 组内层数=%d 层数上限=%d 溢出政策=%d"),
			Target.Id, *DefTag.ToString(), Member ? Member->Stacks : 0,
			Def->StackPolicy.MaxStacks, static_cast<int32>(Def->StackPolicy.Overflow));
		return EApplyResult::EAR_Rejected;
	}

	// 替换：先走**同一条移除链**摘掉旧条（撤时间条目 + 摘修正器 + `Removed` 广播 + 归还槽位），
	// 再落到下面的新建路径——旧句柄从此悬空，这正是"替换"与"原地刷新"的可观测差别。
	if (Decision.Kind == ETcsStateStackDecisionKind::Replace)
	{
		Remove(Subsystem, Decision.ExistingHandle, EStateRemoveCause::ESRC_Removed);

		// 移除会广播 ⇒ 订阅者可重入状态操作（注销该单位）⇒ MUST 重新取桶。桶对象经 `TUniquePtr` 持有、
		// 地址稳定，但**对象本身可能已被销毁** ⇒ 必须重取而不是复用旧指针（Task 4 的重入纪律）。
		Bucket = Subsystem.Registry.FindBucket(Target);
		if (!Bucket)
		{
			UE_LOG(LogTcsState, Warning, TEXT("状态替换的移除阶段期间单位桶已注销：单位=%lld 定义=%s"),
				Target.Id, *DefTag.ToString());
			return EApplyResult::EAR_Rejected;
		}
	}

	// 刷新 / 叠层：同一处更新流水（层数 → 快照重建 → 修正器重挂 → 时长重挂 → 广播），
	// 回执按层数有无变化分档（`Refreshed` = 续杯 / `Stacked` = 叠了一层）。
	if (Decision.Kind == ETcsStateStackDecisionKind::Refresh || Decision.Kind == ETcsStateStackDecisionKind::Stack)
	{
		FTcsStateInstance* Existing = Bucket->Find(Decision.ExistingHandle);
		if (!Existing)
		{
			UE_LOG(LogTcsState, Warning, TEXT("状态共存决策后实例已不在册：单位=%lld 定义=%s 句柄=%d/%d"),
				Target.Id, *DefTag.ToString(), Decision.ExistingHandle.Index, Decision.ExistingHandle.Generation);
			return EApplyResult::EAR_Rejected;
		}

		return RefreshStacked(Subsystem, *Def, *Existing, StackRequest, Decision, ParamTable, Overrides);
	}

	// 新施加：分配槽位 → 填实例 → 快照 → 时间条目 → 广播。句柄在 `AllocateSlot` 后取得，
	// 其 `Index` 只在本函数内按值随句柄流转——`TArray` 扩容不使**值**失效（这是"传句柄不传指针"的纪律）。
	const FTcsStateHandle NewHandle = Bucket->AllocateSlot();
	FTcsStateInstance* NewInstance = Bucket->Find(NewHandle);
	check(NewInstance != nullptr);

	NewInstance->DefTag = DefTag;
	NewInstance->Handle = NewHandle;
	NewInstance->Unit = Target;
	NewInstance->Source = Source.IsValid() ? Source : Subsystem.SourceRegistry.Allocate();

	// 级联锚点：**每实例恒发新号**（与 `Source` 解耦——一个来源可施加多个不同定义，
	// 按来源撤销会互相误摘；锚点保证"撤销只摘本实例"）。
	NewInstance->CascadeAnchor = Subsystem.SourceRegistry.Allocate();
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

	// 修正器物化 + 挂载（施加路径无需先摘：级联锚点是本次施加才发放的，账本上不可能有同锚点条目）。
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

	// 内联触发行接线（`TRIG-4` 的行为半）：**排在 `Applied` 广播之前**——新登记的行要能看见自己的
	// `Applied`（"状态在，行为就在"的起点）；行以实例的**级联锚点**为来源 ⇒ 撤销时一次摘净。
	WireTriggerRows(Subsystem, *LiveInstance, *Def);

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

	// 撤销锚点 = **级联锚点**（不是来源句柄：来源是"谁施加的"，一个来源可挂多个定义 ⇒ 按它摘会误摘）
	const FTcsSourceHandle CascadeAnchor = Found->CascadeAnchor;

	// **修正器摘除排在"取实例指针"之前**（顺序有理由，不是随手排的）：`StripModifiers` 内的批提交
	// 会重算 + 广播（属性变更事件），订阅者可在其中重入状态操作（移除本实例 / 注销单位）——
	// 先摘除、再重新定位桶与实例，下面两道既有守卫（桶不存在 / 实例查不到）就同时接住了重入造成的失效。
	// 完整撤销顺序：**摘除修正器 → 退订内联触发行 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**。
	// 三处撤销都排在广播之前 ⇒ 订阅者读到的是"已经还清"的属性值、行与实例；其中退订排在广播之前
	// 还有一层意义：**该实例自己的触发行 MUST NOT 因自己的死亡事件起链**（"状态走了，行为先走"）。
	StripModifiers(Subsystem, Unit, CascadeAnchor);

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

	// 退订内联触发行（撤销顺序第二段）：不广播（纯登记表操作）⇒ 不受"取实例指针之前"那条约束，
	// 但必须排在广播之前——理由见上面的顺序注释。
	UnwireTriggerRows(Subsystem, *Instance);

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
