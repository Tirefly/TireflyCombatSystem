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
	const TMap<FGameplayTag, double>& Overrides)
{
	(void)Overrides;

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
		// **时值字段本轮刻意不动**（`DurationRemaining` / `PeriodRemaining`）：它们的真值来自
		// `DurationTime` 参数源求值 ⇒ 归 R5 Task 3（快照构建）——本轮没有快照，写死一个数就是编造。
		Existing->Level = Def->LevelBase;
		Existing->Instigator = Target;

		Transit(Subsystem, Existing, EStatePhase::ESP_Expiring);
		Broadcast(Subsystem, Tag_TcsEvent_State_Refreshed, *Existing, EStateRemoveCause::ESRC_Removed);
		Transit(Subsystem, Existing, EStatePhase::ESP_Active);

		UE_LOG(LogTcsState, Log, TEXT("状态刷新：单位=%lld 定义=%s 句柄=%d/%d 层数=%d"),
			Target.Id, *DefTag.ToString(), Existing->Handle.Index, Existing->Handle.Generation, Existing->Stacks);

		return EApplyResult::EAR_Refreshed;
	}

	// 新施加：分配槽位 → 填实例 → 广播。句柄在 `AllocateSlot` 后取得，
	// 其 `Index` 只在本函数内按值随句柄流转——`TArray` 扩容不使**值**失效（这是"传句柄不传指针"的纪律）。
	const FTcsStateHandle NewHandle = Bucket.AllocateSlot();
	FTcsStateInstance* NewInstance = Bucket.Find(NewHandle);
	check(NewInstance != nullptr);

	NewInstance->DefTag = DefTag;
	NewInstance->Handle = NewHandle;
	NewInstance->Unit = Target;
	NewInstance->Source = Source.IsValid() ? Source : Subsystem.SourceRegistry.Allocate();
	NewInstance->Instigator = Target;
	NewInstance->Stacks = 1;
	NewInstance->Level = Def->LevelBase;
	NewInstance->Phase = EStatePhase::ESP_Active;
	NewInstance->DurationRemaining = 0.0;
	NewInstance->PeriodRemaining = 0.0;

	Broadcast(Subsystem, Tag_TcsEvent_State_Applied, *NewInstance, EStateRemoveCause::ESRC_Removed);

	UE_LOG(LogTcsState, Log, TEXT("状态施加：单位=%lld 定义=%s 句柄=%d/%d 来源=%llu 等级=%d"),
		Target.Id, *DefTag.ToString(), NewHandle.Index, NewHandle.Generation,
		NewInstance->Source.Id, NewInstance->Level);

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

	FStateBucket* Bucket = Subsystem.Registry.FindBucket(Unit);
	if (!Bucket)
	{
		UE_LOG(LogTcsState, Warning, TEXT("状态移除被拒：单位桶不存在（单位=%lld）"), Unit.Id);
		return false;
	}

	// 撤销顺序（**硬约束**）：`Expiring` → 广播 → 归还槽位。
	// 先广播后释放，订阅者才能在回调里用载荷句柄读到实例（否则"收到移除事件却查不到实例"）。
	FTcsStateInstance* Instance = Bucket->Find(Handle);
	if (!Instance)
	{
		return false;
	}

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
