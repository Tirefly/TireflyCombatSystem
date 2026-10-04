// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateOps.h"
#include "TcsStateLogChannel.h"



// 生命期
bool UTcsStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界（Game/PIE/GamePreview）——状态注册表是运行时设施，与时钟/总线/属性/效果链门面同口径
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsStateSubsystem::Deinitialize()
{
	// 确定性清空：全部桶逐槽归还（旧句柄凭代际失配失效）+ 登记表复位。
	// **本轮无到期条目待撤销**（入堆/撤堆归 R5 Task 3——见头文件说明）。
	// **发号器不复位**：`Id` 的契约是"进程内唯一且永不复用"（`instance-handle-pool` 规格），
	// 复位会让新世界的来源号与上一世界撞车——那正是级联摘除会误伤别人的缺陷形态（`WAIT-6`）。
	Registry.Reset();
	RegisteredDefs.Reset();

	Super::Deinitialize();
}

void UTcsStateSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTcsStateSubsystem* This = CastChecked<UTcsStateSubsystem>(InThis);

	// 状态定义登记表非 UPROPERTY（TUniquePtr 容器）——GC 只走 RefLink 看不见它，故在此手动补引用。
	// 定义内容里的 FInstancedStruct 内层可放宿主自定义 struct 的 UPROPERTY 对象引用
	// （引擎 UDataTable::AddReferencedObjects 对 RowMap 用的同一招，见头文件说明）。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsBuffDef>>& Pair : This->RegisteredDefs)
	{
		if (FTcsBuffDef* Def = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsBuffDef::StaticStruct(), Def, This);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}



// 施加与查询
EApplyResult UTcsStateSubsystem::ApplyState(
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	const TMap<FGameplayTag, double>& Overrides)
{
	return FTcsStateOps::Apply(*this, Target, DefTag, Source, Overrides);
}

const FTcsStateInstance* UTcsStateSubsystem::GetState(FTcsStateHandle Handle) const
{
	// 逻辑在引擎函数里（它按句柄做代际校验式定位）——门面保持薄壳
	return FTcsStateOps::Find(*const_cast<UTcsStateSubsystem*>(this), Handle);
}

bool UTcsStateSubsystem::IsStateActive(FTcsStateHandle Handle) const
{
	return GetState(Handle) != nullptr;
}

void UTcsStateSubsystem::ForEachState(
	FTcsCombatEntityHandle Unit,
	TFunctionRef<bool(const FTcsStateInstance&)> Visitor) const
{
	if (const FStateBucket* Bucket = Registry.FindBucket(Unit))
	{
		Bucket->ForEach(Visitor);
	}
}

int32 UTcsStateSubsystem::GetStateCount(FTcsCombatEntityHandle Unit) const
{
	return FTcsStateOps::CountStates(*const_cast<UTcsStateSubsystem*>(this), Unit);
}

int32 UTcsStateSubsystem::GetTotalStateCount() const
{
	return Registry.NumInstances();
}



// 移除与生命周期操作
bool UTcsStateSubsystem::RemoveState(FTcsStateHandle Handle, EStateRemoveCause Cause)
{
	// `Expired` 是到期路径的专属原因——调用方走错口时**以到期语义执行**（不静默改写原因，
	// 但也不拦：原因值最终由 Cause 参数决定，两条口只是给调用点一个自解释的名字）
	return FTcsStateOps::Remove(*this, Handle, Cause);
}

bool UTcsStateSubsystem::ExpireState(FTcsStateHandle Handle)
{
	return FTcsStateOps::Remove(*this, Handle, EStateRemoveCause::ESRC_Expired);
}

int32 UTcsStateSubsystem::UnregisterUnit(FTcsCombatEntityHandle Unit)
{
	return FTcsStateOps::UnregisterUnit(*this, Unit);
}

bool UTcsStateSubsystem::ExtendDuration(FTcsStateHandle Handle, double Delta)
{
	FTcsStateInstance* Instance = nullptr;
	if (!FTcsStateOps::PrepareDurationOp(*this, Handle, Instance))
	{
		return false;
	}

	Instance->DurationRemaining = FMath::Max(0.0, Instance->DurationRemaining + Delta);

	// 到期堆同步（撤销旧条目 + 按新余量重入堆）归 R5 Task 3——本轮只更新字段
	UE_LOG(LogTcsState, Log, TEXT("状态时长延长：句柄=%d/%d 增量=%.3f 剩余=%.3f（堆同步归 Task 3）"),
		Handle.Index, Handle.Generation, Delta, Instance->DurationRemaining);

	return true;
}

bool UTcsStateSubsystem::SetRemaining(FTcsStateHandle Handle, double Seconds)
{
	FTcsStateInstance* Instance = nullptr;
	if (!FTcsStateOps::PrepareDurationOp(*this, Handle, Instance))
	{
		return false;
	}

	Instance->DurationRemaining = FMath::Max(0.0, Seconds);

	// 到期堆同步归 R5 Task 3——本轮只更新字段
	UE_LOG(LogTcsState, Log, TEXT("状态剩余时长设置：句柄=%d/%d 剩余=%.3f（堆同步归 Task 3）"),
		Handle.Index, Handle.Generation, Instance->DurationRemaining);

	return true;
}



// 内核
UTcsEventBusSubsystem* UTcsStateSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}

void UTcsStateSubsystem::TransitionPhase(FTcsStateInstance* Instance, EStatePhase NewPhase)
{
	if (!Instance)
	{
		return;
	}

	// 阶段迁移矩阵（设计文档 §5："阶段×允许迁移 = 数据表声明，可查不可改；非法迁移 = ensure"）。
	// 本轮矩阵以代码表达（相位仍只有三态，落表属于关系表族那一批的形态统一）：
	//   Inactive → Active（施加成功）
	//   Active   → Expiring（进入移除流程）
	//   Expiring → Inactive（槽位归还）
	//   Expiring → Active（**刷新路径合法**：刷新是一次"过渡"，撤下再挂回）
	const EStatePhase Current = Instance->Phase;
	const bool bLegal =
		(Current == EStatePhase::ESP_Inactive && NewPhase == EStatePhase::ESP_Active) ||
		(Current == EStatePhase::ESP_Active && NewPhase == EStatePhase::ESP_Expiring) ||
		(Current == EStatePhase::ESP_Expiring && NewPhase == EStatePhase::ESP_Inactive) ||
		(Current == EStatePhase::ESP_Expiring && NewPhase == EStatePhase::ESP_Active);

	// 非法迁移 = **配置错误**语义（与脏句柄的时序竞态刻意分开：那个只记 Warning）
	if (!ensureMsgf(bLegal, TEXT("UTcsStateSubsystem::TransitionPhase: 非法阶段迁移（%d → %d）"),
		static_cast<int32>(Current), static_cast<int32>(NewPhase)))
	{
		return;
	}

	Instance->Phase = NewPhase;
}
