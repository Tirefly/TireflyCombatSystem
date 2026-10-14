// Copyright Tirefly. All Rights Reserved.

#include "TcsStateSubsystem.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsStateOps.h"
#include "TcsStateLogChannel.h"



// 生命期
void UTcsStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 快照参数表反射壳：门面持有单例 ⇒ 物化路径零 per-apply 分配。
	// 初始**不绑定**任何快照（未绑定 = 一切查询落 miss）——绑定只在物化作用域内发生。
	ParamTableReader = NewObject<UTcsStateParamTableReader>(this, TEXT("TcsStateParamTableReader"));
}

bool UTcsStateSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界（Game/PIE/GamePreview）——状态注册表是运行时设施，与时钟/总线/属性/效果链门面同口径
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsStateSubsystem::Deinitialize()
{
	// 行为先退订：清桶/清 Def 之前完成配对，并打印前后计数以支持跨 PIE 残留复核。
	const int32 BehaviorSubscriptionsBefore = BehaviorRegistry.GetSubscriptionCount();
	const int32 BehaviorInstancesBefore = BehaviorRegistry.GetInstanceCount();
	BehaviorRegistry.Reset(GetEventBus());
	BehaviorHandler = nullptr;
	UE_LOG(LogTcsState, Log,
		TEXT("状态行为反初始化：订阅 %d→%d 实例 %d→%d"),
		BehaviorSubscriptionsBefore, BehaviorRegistry.GetSubscriptionCount(),
		BehaviorInstancesBefore, BehaviorRegistry.GetInstanceCount());

	// **先逐条撤销时间条目**（时值 + 周期）：条目持句柄，桶清空后回调仍会到达——
	// 靠代际校验兜底是"能成立"，但每次世界切换都会往堆里留一批无谓条目。
	// 顺序 = 撤条目 → 清桶 → 复位登记表。
	// **发号器不复位**：`Id` 的契约是"进程内唯一且永不复用"（`instance-handle-pool` 规格），
	// 复位会让新世界的来源号与上一世界撞车——那正是级联摘除会误伤别人的缺陷形态（`WAIT-6`）。
	Registry.ForEachBucket([this](FTcsCombatEntityHandle Entity, FStateBucket& Bucket)
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
	RemovalBroadcastGuard.Reset();

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
	// **⚠ 遍历口 MUST 用 `ForEachForGC`，MUST NOT 用 `Bucket.ForEach`**（2026-10-14 补齐）：
	// 状态桶自本变更起**改由共享层的 `TTcsInstancePool` 承载**，而池的**每一个正常进入点**都
	// 无条件 `ensure(IsInGameThread())`（D0-4 单游戏线程假设），**GC 的引用收集不在游戏线程上**
	// ⇒ 在 ARO 里走 `ForEach` 会**每次 GC 触发一次** `Ensure condition failed: IsInGameThread()`。
	// 该症状极具隐蔽性：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"、且与池空不空无关。
	// （技能侧 2026-10-08 首轮 PIE 实测已踩过同一个坑，其 ARO 早已改用 `ForEachForGC`；
	// 状态侧原为手写桶、没有这条断言，故**本变更把它引入**——必须同批接上 GC 口。）
	This->Registry.ForEachBucket([&Collector](FTcsCombatEntityHandle Entity, const FStateBucket& Bucket)
	{
		Bucket.ForEachForGC([&Collector](const FTcsStateInstance& Instance)
		{
			for (const FTcsParamSnapshotEntry& Entry : Instance.ParamSnapshot.Entries)
			{
				if (Entry.SourceRef.IsValid())
				{
					// 收集只枚举引用、不改内容——`const_cast` 仅为满足 ARO 的非 const 签名
					const_cast<FInstancedStruct&>(Entry.SourceRef).AddStructReferencedObjects(Collector);
				}
			}
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
	FTcsCombatEntityHandle Entity,
	TFunctionRef<bool(const FTcsStateInstance&)> Visitor) const
{
	if (const FStateBucket* Bucket = Registry.FindBucket(Entity))
	{
		Bucket->ForEach(Visitor);
	}
}

int32 UTcsStateSubsystem::GetStateCount(FTcsCombatEntityHandle Entity) const
{
	return FTcsStateOps::CountStates(*const_cast<UTcsStateSubsystem*>(this), Entity);
}

int32 UTcsStateSubsystem::GetTotalStateCount() const
{
	// 共享层模板的口名是 `NumItems`（它对两域都叫"条目/实例"——沿用域侧旧名会逼模板长两张脸）
	return Registry.NumItems();
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

int32 UTcsStateSubsystem::UnregisterEntity(FTcsCombatEntityHandle Entity)
{
	return FTcsStateOps::UnregisterEntity(*this, Entity);
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

UTcsStateParamTableReader* UTcsStateSubsystem::GetParamTableReader()
{
	return ParamTableReader;
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
