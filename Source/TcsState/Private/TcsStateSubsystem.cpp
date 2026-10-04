// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "Clock/TcsClockSubsystem.h"
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
	// **先逐条撤销时间条目**（时值 + 周期）：条目持句柄，桶清空后回调仍会到达——
	// 靠代际校验兜底是"能成立"，但每次世界切换都会往堆里留一批无谓条目。
	// 顺序 = 撤条目 → 清桶 → 复位登记表。
	// **发号器不复位**：`Id` 的契约是"进程内唯一且永不复用"（`instance-handle-pool` 规格），
	// 复位会让新世界的来源号与上一世界撞车——那正是级联摘除会误伤别人的缺陷形态（`WAIT-6`）。
	Registry.ForEachBucket([this](FTcsCombatEntityHandle Unit, FStateBucket& Bucket)
	{
		Bucket.ForEach([this](FTcsStateInstance& Instance)
		{
			FTcsStateOps::CancelTimeEntries(*this, Instance);
			return true;
		});

		return true;
	});

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

	// 在册实例的参数快照**同款地雷**：实例记录住桶（非 UPROPERTY 容器），而其 `ParamSnapshot`
	// 的 `SourceRef` 位可含对象引用（数值来源的副本，如域侧 `AttributeScaled` 一类源持读口）
	// ⇒ 不补引用即"那个对象被静默回收"（表现为读快照时来源变空，不是崩溃）。
	//
	// **为什么用 `FInstancedStruct::AddStructReferencedObjects` 而不是 `AddPropertyReferencesWithStructARO`**：
	// 后者要求一个 `UScriptStruct*`（会连带收集结构体自身的引用），而 `FTcsParamSnapshotEntry`
	// **不是反射结构体**（快照只在 C++ 侧流转）⇒ 没有 `StaticStruct()` 可给。逐条走 `SourceRef`
	// 自己的 ARO 反而更准：它只补真正持对象的那一层，且递归进内层结构体的属性
	// （引擎 `FInstancedStruct::AddStructReferencedObjects` 已实现该递归）。
	This->Registry.ForEachBucket([&Collector](FTcsCombatEntityHandle Unit, FStateBucket& Bucket)
	{
		Bucket.ForEach([&Collector](FTcsStateInstance& Instance)
		{
			for (FTcsParamSnapshotEntry& Entry : Instance.ParamSnapshot.Entries)
			{
				if (Entry.SourceRef.IsValid())
				{
					// 收集只枚举引用、不改内容——`const_cast` 仅为满足 ARO 的非 const 签名
					const_cast<FInstancedStruct&>(Entry.SourceRef).AddStructReferencedObjects(Collector);
				}
			}

			return true;
		});

		return true;
	});

	Super::AddReferencedObjects(InThis, Collector);
}



// 施加与查询
EApplyResult UTcsStateSubsystem::ApplyState(
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsCombatEntityHandle Instigator,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	const TMap<FGameplayTag, double>& Overrides)
{
	return FTcsStateOps::Apply(*this, Target, DefTag, Source, Instigator, ParamTable, Overrides);
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

	// 到期堆同步：撤销旧条目 + 按新余量重入堆（句柄配对清理）
	FTcsStateOps::RescheduleExpiry(*this, *Instance);

	UE_LOG(LogTcsState, Log, TEXT("状态时长延长：句柄=%d/%d 增量=%.3f 剩余=%.3f"),
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

	// 到期堆同步同上；剩余 0 时条目按"当前时刻"入堆 ⇒ 在**下一个泵点**到期
	FTcsStateOps::RescheduleExpiry(*this, *Instance);

	UE_LOG(LogTcsState, Log, TEXT("状态剩余时长设置：句柄=%d/%d 剩余=%.3f"),
		Handle.Index, Handle.Generation, Instance->DurationRemaining);

	return true;
}



// 宿主等级读口
void UTcsStateSubsystem::SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider> InProvider)
{
	EntityLevelProvider = InProvider;

	UE_LOG(LogTcsState, Log, TEXT("状态门面注入等级读口：%s"),
		InProvider.GetObject() ? *InProvider.GetObject()->GetName() : TEXT("（已清空）"));
}



// 内核
UTcsEventBusSubsystem* UTcsStateSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}

UTcsClockSubsystem* UTcsStateSubsystem::GetClockSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsClockSubsystem>() : nullptr;
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
