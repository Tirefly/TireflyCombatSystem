// Copyright Tirefly. All Rights Reserved.

#include "TcsBuffSubsystem.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "State/TcsBuffOps.h"
#include "TcsBuffLogChannel.h"
#include "TcsStateSubsystem.h"



// 生命期
void UTcsBuffSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// **向下注册在飞查询契约（R5.5-e1）**：共享层 `UTcsStateSubsystem` 是两域唯一共用设施持有者，
	// 技能侧经它读本域在飞实例（`TcsSkill` MUST NOT 依赖 `TcsBuff`）。未注册时那边走降级路径。
	if (UTcsStateSubsystem* StateSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTcsStateSubsystem>() : nullptr)
	{
		StateSubsystem->SetLiveQuery(this);
	}
	else
	{
		UE_LOG(LogTcsBuff, Warning, TEXT("状态门面初始化：共享层门面不可得，在飞查询契约未注册（技能侧关系门将走降级路径）"));
	}

	// **不在这里建快照参数表壳**：它是两域共用设施（绑定是壳上的单例状态），住共享层
	// `UTcsStateSubsystem`。本门面只在需要时经 `GetParamTableReader()` 转发取用。
}

bool UTcsBuffSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界（Game/PIE/GamePreview）——状态注册表是运行时设施，与时钟/总线/属性/效果链门面同口径
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsBuffSubsystem::Deinitialize()
{
	// 行为先退订：清桶/清 Def 之前完成配对，并打印前后计数以支持跨 PIE 残留复核。
	const int32 BehaviorSubscriptionsBefore = BehaviorRegistry.GetSubscriptionCount();
	const int32 BehaviorInstancesBefore = BehaviorRegistry.GetInstanceCount();
	BehaviorRegistry.Reset(GetEventBus());
	BehaviorHandler = nullptr;
	UE_LOG(LogTcsBuff, Log,
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
		Bucket.ForEach([this](FTcsBuffInstance& Instance)
		{
			FTcsBuffOps::CancelTimeEntries(*this, Instance);
			return true;
		});

		return true;
	});

	// 注销契约（防悬空：共享层门面可能比本域活得久）
	if (UTcsStateSubsystem* StateSubsystem = GetWorld() ? GetWorld()->GetSubsystem<UTcsStateSubsystem>() : nullptr)
	{
		StateSubsystem->SetLiveQuery(nullptr);
	}

	Registry.Reset();
	RegisteredDefs.Reset();
	RemovalBroadcastGuard.Reset();

	Super::Deinitialize();
}

void UTcsBuffSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTcsBuffSubsystem* This = CastChecked<UTcsBuffSubsystem>(InThis);

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
		Bucket.ForEachForGC([&Collector](const FTcsBuffInstance& Instance)
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
EBuffApplyResult UTcsBuffSubsystem::ApplyState(
	FTcsCombatEntityHandle Target,
	FGameplayTag DefTag,
	FTcsSourceHandle Source,
	FTcsCombatEntityHandle Instigator,
	const TScriptInterface<ITcsParamTableReader>& ParamTable,
	const TMap<FGameplayTag, double>& Overrides)
{
	return FTcsBuffOps::Apply(*this, Target, DefTag, Source, Instigator, ParamTable, Overrides);
}

const FTcsBuffInstance* UTcsBuffSubsystem::GetState(FTcsBuffInstanceHandle Handle) const
{
	// 逻辑在引擎函数里（它按句柄做代际校验式定位）——门面保持薄壳
	return FTcsBuffOps::Find(*const_cast<UTcsBuffSubsystem*>(this), Handle);
}

bool UTcsBuffSubsystem::IsStateActive(FTcsBuffInstanceHandle Handle) const
{
	return GetState(Handle) != nullptr;
}

void UTcsBuffSubsystem::ForEachState(
	FTcsCombatEntityHandle Entity,
	TFunctionRef<bool(const FTcsBuffInstance&)> Visitor) const
{
	if (const FStateBucket* Bucket = Registry.FindBucket(Entity))
	{
		Bucket->ForEach(Visitor);
	}
}

int32 UTcsBuffSubsystem::GetStateCount(FTcsCombatEntityHandle Entity) const
{
	return FTcsBuffOps::CountStates(*const_cast<UTcsBuffSubsystem*>(this), Entity);
}

int32 UTcsBuffSubsystem::GetTotalStateCount() const
{
	// 共享层模板的口名是 `NumItems`（它对两域都叫"条目/实例"——沿用域侧旧名会逼模板长两张脸）
	return Registry.NumItems();
}



// 移除与生命周期操作
bool UTcsBuffSubsystem::RemoveState(FTcsBuffInstanceHandle Handle, EBuffRemoveCause Cause)
{
	// `Expired` 是到期路径的专属原因——调用方走错口时**以到期语义执行**（不静默改写原因，
	// 但也不拦：原因值最终由 Cause 参数决定，两条口只是给调用点一个自解释的名字）
	return FTcsBuffOps::Remove(*this, Handle, Cause);
}

bool UTcsBuffSubsystem::ExpireState(FTcsBuffInstanceHandle Handle)
{
	return FTcsBuffOps::Remove(*this, Handle, EBuffRemoveCause::EBRC_Expired);
}

int32 UTcsBuffSubsystem::UnregisterEntity(FTcsCombatEntityHandle Entity)
{
	return FTcsBuffOps::UnregisterEntity(*this, Entity);
}

bool UTcsBuffSubsystem::ExtendDuration(FTcsBuffInstanceHandle Handle, double Delta)
{
	FTcsBuffInstance* Instance = nullptr;
	if (!FTcsBuffOps::PrepareDurationOp(*this, Handle, Instance))
	{
		return false;
	}

	Instance->DurationRemaining = FMath::Max(0.0, Instance->DurationRemaining + Delta);

	// 到期堆同步：撤销旧条目 + 按新余量重入堆（句柄配对清理）
	FTcsBuffOps::RescheduleExpiry(*this, *Instance);

	UE_LOG(LogTcsBuff, Log, TEXT("状态时长延长：句柄=%d/%d 增量=%.3f 剩余=%.3f"),
		Handle.Index, Handle.Generation, Delta, Instance->DurationRemaining);

	return true;
}

bool UTcsBuffSubsystem::SetRemaining(FTcsBuffInstanceHandle Handle, double Seconds)
{
	FTcsBuffInstance* Instance = nullptr;
	if (!FTcsBuffOps::PrepareDurationOp(*this, Handle, Instance))
	{
		return false;
	}

	Instance->DurationRemaining = FMath::Max(0.0, Seconds);

	// 到期堆同步同上；剩余 0 时条目按"当前时刻"入堆 ⇒ 在**下一个泵点**到期
	FTcsBuffOps::RescheduleExpiry(*this, *Instance);

	UE_LOG(LogTcsBuff, Log, TEXT("状态剩余时长设置：句柄=%d/%d 剩余=%.3f"),
		Handle.Index, Handle.Generation, Instance->DurationRemaining);

	return true;
}



// 宿主等级读口
void UTcsBuffSubsystem::SetEntityLevelProvider(TScriptInterface<ITcsEntityLevelProvider> InProvider)
{
	EntityLevelProvider = InProvider;

	UE_LOG(LogTcsBuff, Log, TEXT("状态门面注入等级读口：%s"),
		InProvider.GetObject() ? *InProvider.GetObject()->GetName() : TEXT("（已清空）"));
}



// 内核
UTcsEventBusSubsystem* UTcsBuffSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}

UTcsClockSubsystem* UTcsBuffSubsystem::GetClockSubsystem() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsClockSubsystem>() : nullptr;
}

UTcsStateParamTableReader* UTcsBuffSubsystem::GetParamTableReader()
{
	// **转发到共享层**：壳是两域共用设施（`FTcsStateSnapshotScope` 的绑定是壳上的单例状态），
	// 故它住 `UTcsStateSubsystem`。本门面自建第二份壳会让"当前绑定"出现两处真相
	// ⇒ 嵌套物化时的恢复顺序将取决于调用方先取到哪一份（本仓反复拦的"第二处查找语义"）。
	// 共享门面不可得（世界拆解 / 无游戏世界）⇒ 返回 nullptr，调用方按"无参数表"降级。
	const UWorld* World = GetWorld();
	UTcsStateSubsystem* SharedSubsystem = World ? World->GetSubsystem<UTcsStateSubsystem>() : nullptr;
	return SharedSubsystem ? SharedSubsystem->GetParamTableReader() : nullptr;
}

void UTcsBuffSubsystem::TransitionPhase(FTcsBuffInstance* Instance, EBuffPhase NewPhase)
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
	const EBuffPhase Current = Instance->Phase;
	const bool bLegal =
		(Current == EBuffPhase::EBP_Inactive && NewPhase == EBuffPhase::EBP_Active) ||
		(Current == EBuffPhase::EBP_Active && NewPhase == EBuffPhase::EBP_Expiring) ||
		(Current == EBuffPhase::EBP_Expiring && NewPhase == EBuffPhase::EBP_Inactive) ||
		(Current == EBuffPhase::EBP_Expiring && NewPhase == EBuffPhase::EBP_Active);

	// 非法迁移 = **配置错误**语义（与脏句柄的时序竞态刻意分开：那个只记 Warning）
	if (!ensureMsgf(bLegal, TEXT("UTcsBuffSubsystem::TransitionPhase: 非法阶段迁移（%d → %d）"),
		static_cast<int32>(Current), static_cast<int32>(NewPhase)))
	{
		return;
	}

	Instance->Phase = NewPhase;
}


// 在飞运行态查询契约实现（R5.5-e1）
void UTcsBuffSubsystem::ForEachLiveState(
	FTcsCombatEntityHandle Entity,
	TFunctionRef<bool(const FTcsLiveStateView&)> Visitor)
{
	ForEachState(Entity, [this, &Visitor](const FTcsBuffInstance& Instance)
	{
		// 定义已注销 ⇒ 行面不可知，跳过（比「当作空行」更诚实：空行也不命中，但那是巧合而非判据）
		const FTcsBuffDef* Def = GetRegisteredBuffDef(Instance.DefTag);
		if (!Def)
		{
			return true;
		}

		// 合成完整视图：**事实半**（类别词集）+ **声明半**（关系行，主体 = 本定义自己）
		FTcsLiveStateView View;
		View.CategoryTags = Def->CategoryTags;
		View.Row.AppliesToTag = Instance.DefTag;
		View.Row.RequiredTags = FGameplayTagContainer::CreateFromArray(Def->RequiredTags);
		View.Row.ForbiddenTags = FGameplayTagContainer::CreateFromArray(Def->ForbiddenTags);
		View.Row.TagsToBlock = FGameplayTagContainer::CreateFromArray(Def->TagsToBlock);
		View.Row.TagsToCancel = FGameplayTagContainer::CreateFromArray(Def->TagsToCancel);

		return Visitor(View);
	});
}

int32 UTcsBuffSubsystem::GetLiveCount(FTcsCombatEntityHandle Entity) const
{
	return GetStateCount(Entity);
}
