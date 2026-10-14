// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "EventBus/TcsEventBusSubsystem.h"

#include "Skill/TcsCastOps.h"
#include "Skill/TcsCooldownOps.h"
#include "Skill/TcsCostOps.h"
#include "Skill/TcsSkillOps.h"
#include "TcsSkillLogChannel.h"



// 生命期
void UTcsSkillSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 本门面无需预建每世界共享对象：账本、定义登记表与运行态池都按需增长
	// （与状态门面不同——那里有一个快照参数表壳要在初始化期建，见其 `Initialize` 的说明）。
	// 发号器是出线的进程级设施，构造即可用。
}

bool UTcsSkillSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 仅游戏世界（Game/PIE/GamePreview）——技能账本是运行时设施，与状态/时钟/总线/属性/效果链门面同口径
	return WorldType == EWorldType::Game ||
		WorldType == EWorldType::PIE ||
		WorldType == EWorldType::GamePreview;
}

void UTcsSkillSubsystem::Deinitialize()
{
	// **七者都要清**：运行态池 / 登记表 / 账本 / 注入的实体查询接口 /
	// 链重定向栈（含宿主契约）/ 在飞终结标记集 / **实体级组冷却槽**。
	// 不清注入的接口会让"未注入 ⇒ 降级放行"这条路径在跨世界后**再也测不到**
	// （上一世界注入的实现一直生效，表现为门禁第一道永远走宿主判定）。
	//
	// **在飞终结标记集 MUST 清**：它只在"某次终结还没走完"的窗口内有意义；跨世界残留会让
	// 新世界里同 `(Index, Generation)` 的 run **再也终结不掉**（返回 false 且静默）——
	// 症状是"技能放不出去也结束不了"，远离根因。状态侧同款（`InRemovalBroadcastHandles.Reset()`）。
	//
	// **组冷却槽 MUST 清（且不能只 `Reset` 表）**：每个槽持一枚到期堆句柄，而堆挂在**时钟子系统**上
	// ——不撤锚点就删表，那些条目会**活到各自到期**并回调一个已不存在的槽
	// （症状：世界拆解后仍有回调空跑；而槽表已空 ⇒ 回调静默丢弃，属"静默无效但占着池槽"）。
	//
	// **★ 撤锚点 MUST 先确认时钟仍活着（fresh-context 复核抓到的 Important 缺陷）**：
	// 本类的 `Deinitialize` 与时钟子系统的拆解顺序**不保证**（两者都是世界子系统）。
	// `FTcsExpiryHeap::Reset()` 清条目与回调、但**不失效代际** ⇒ 时钟已拆完时再 `CancelExpiry`
	// 会**二次递减** `Tcs.Core.ExpiryHeapDepth` 并释放一个陈旧槽位（症状 = 拆解期堆深度计数变负）。
	// **判据 = 本仓既有先例**：`FTcsStateBehaviorRegistry::Reset` 对总线用的正是
	// `if (Bus && Bus->IsInitialized())`——`IsInitialized()` 的语义是"**Initialize 已调用、
	// Deinitialize 尚未调用**"（`WorldSubsystem.h:102`），正好回答"这个时钟现在还能撤锚点吗"。
	int32 GroupSlotsBefore = 0;
	for (const TPair<FTcsCombatEntityHandle, TMap<FGameplayTag, FTcsCooldownGroupState>>& Pair : CooldownGroups)
	{
		GroupSlotsBefore += Pair.Value.Num();
		if (UTcsClockSubsystem* Clock = GetClockSubsystem(); Clock && Clock->IsInitialized())
		{
			for (const TPair<FGameplayTag, FTcsCooldownGroupState>& SlotPair : Pair.Value)
			{
				if (SlotPair.Value.ExpiryEntry.IsValid())
				{
					Clock->CancelExpiry(SlotPair.Value.ExpiryEntry);
				}
			}
		}
	}

	const int32 EntriesBefore = Registry.NumItems();
	const int32 DefsBefore = RegisteredDefs.Num();
	const int32 RunsBefore = FTcsCastOps::ResetAll(*this);
	const int32 RedirectsBefore = ChainRedirects.Num();

	Registry.Reset();
	RegisteredDefs.Reset();
	ChainRedirects.Reset();
	FinishBroadcastGuard.Reset();
	CooldownGroups.Reset();

	EntityQuery = TScriptInterface<ITcsEntityQuery>();
	ChainRedirectResolver = TScriptInterface<ITcsChainRedirectResolver>();

	UE_LOG(LogTcsSkill, Log,
		TEXT("技能门面反初始化：账本条目 %d→%d 已登记定义 %d→%d 施法运行态 %d→%d 链重定向 %d→%d 组冷却槽 %d→0，注入接口已清空"),
		EntriesBefore, Registry.NumItems(), DefsBefore, RegisteredDefs.Num(),
		RunsBefore, GetSkillInstanceCount(), RedirectsBefore, ChainRedirects.Num(), GroupSlotsBefore);

	Super::Deinitialize();
}



// 冷却（薄壳转发——流程逻辑全在 `FTcsCooldownOps`，本类只保持签名稳定）
bool UTcsSkillSubsystem::IsOnCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry)
{
	return FTcsCooldownOps::IsEntryOnCooldownByHandle(*this, Entity, Entry, nullptr);
}

double UTcsSkillSubsystem::GetCooldownRemaining(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex)
{
	return FTcsCooldownOps::GetTrackRemaining(*this, Entity, Entry, TrackIndex);
}

double UTcsSkillSubsystem::GetCooldownTotal(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex)
{
	return FTcsCooldownOps::GetTrackTotal(*this, Entity, Entry, TrackIndex);
}

int32 UTcsSkillSubsystem::GetCooldownTrackCount(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry)
{
	return FTcsCooldownOps::GetTrackCount(*this, Entity, Entry);
}

void UTcsSkillSubsystem::AdjustCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex, double Delta)
{
	FTcsCooldownOps::AdjustTrack(*this, Entity, Entry, TrackIndex, Delta);
}

void UTcsSkillSubsystem::ResetCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry, int32 TrackIndex)
{
	FTcsCooldownOps::ClearTrack(*this, Entity, Entry, TrackIndex);
}

int32 UTcsSkillSubsystem::StartCooldown(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry)
{
	return FTcsCooldownOps::StartCooldownOnEntry(*this, Entity, Entry);
}



// 消耗（薄壳转发——流程逻辑全在 `FTcsCostOps`）
bool UTcsSkillSubsystem::PayCost(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry)
{
	return FTcsCostOps::PayCostForEntry(*this, Entity, Entry);
}

bool UTcsSkillSubsystem::CanAfford(FTcsCombatEntityHandle Entity, FTcsSkillEntryHandle Entry)
{
	const FTcsLearnedSkillEntry* Learned = FTcsSkillOps::Find(*this, Entity, Entry);
	if (!Learned)
	{
		// 悬空 / 未学 ⇒ false（不 ensure——正常查询路径）
		return false;
	}

	const FTcsSkillDefData* Def = GetRegisteredSkillDef(Learned->DefTag);
	if (!Def)
	{
		return false;
	}

	// **★ 传 `Learned`（2026-10-12 B 形状）**：本口是宿主"预先灰掉按钮"用的查询，
	// 而 `CostAmount` 键上的**减免修正器住在条目上** ⇒ 不传就会报**未减免**的基数，
	// 宿主据此灰按钮就会与实际提交点的判定**不一致**
	// （"UI 说不付得起、真按下去却付得起"或反之——正是本仓反复拦的自相矛盾）。
	return FTcsCostOps::CanAffordCost(*this, Entity, Def->Cost, nullptr, Learned);
}

UTcsEventBusSubsystem* UTcsSkillSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}

void UTcsSkillSubsystem::ForEachSkillInstanceForGC(TFunctionRef<void(const FTcsSkillInstance&)> Visitor) const
{
	// 转调池的 **GC 专用只读遍历**（`TTcsInstancePool::ForEachForGC`）。
	//
	// **★ 为什么不能直接用 `SkillInstances.ForEach`（2026-10-08 首轮 PIE 实测暴露的真实缺陷）**：
	// 池的**每一个正常进入点**都无条件 `ensure(IsInGameThread())`（`TcsInstancePool.h` 的
	// `Allocate`/`Free`/`Resolve`/`IsValid`/`ForEach`/`Reset`，D0-4 单游戏线程假设），而
	// **GC 的引用收集不在游戏线程上** ⇒ 实测 callstack：
	// `FRealtimeGC::CollectReferencesForGC` → `AddReferencedObjects` → `TTcsInstancePool::ForEach`
	// → `Ensure condition failed: IsInGameThread()`（`TcsInstancePool.h:91`）。
	// **症状极具隐蔽性**：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"、且**与池空不空无关**
	// （首轮实测的两次命中都发生在探针运行之前 2 分 21 秒，即 GC 一跑就报）。
	// ⇒ ARO 内 MUST NOT 走池的公开进入点，改用 `ForEachForGC`（纯读、无断言、判据同 `ForEach`）。
	SkillInstances.ForEachForGC(Visitor);
}

void UTcsSkillSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTcsSkillSubsystem* This = CastChecked<UTcsSkillSubsystem>(InThis);

	// 技能定义登记表非 UPROPERTY（TUniquePtr 容器）——GC 只走 RefLink 看不见它，故在此手动补引用。
	// 定义内容里的 FInstancedStruct 内层可放宿主自定义 struct 的 UPROPERTY 对象引用
	// ⇒ 不补引用即静默回收（表现为"参数行/查询片段里那个对象变成空引用"，不是崩溃）。
	// 判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关
	// （引擎 UDataTable::AddReferencedObjects 对 RowMap 用的同一招）。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsSkillDefData>>& Pair : This->RegisteredDefs)
	{
		if (FTcsSkillDefData* Def = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsSkillDefData::StaticStruct(), Def, This);
		}
	}

	// **施法运行态池**同款地雷：池元素住 `TArray`（非 UPROPERTY 容器），而每个 run 的
	// `ParamSnapshot` 条目可含对象引用（数值来源的副本）⇒ 不补引用即"那个对象被静默回收"。
	//
	// **⚠ 遍历口 MUST 用 `ForEachSkillInstanceForGC`，MUST NOT 用 `SkillInstances.ForEach`**
	// （2026-10-08 首轮 PIE 实测暴露：池的公开入口都断言游戏线程，而 GC 收集不在游戏线程
	// ⇒ 每次 GC 留一条 `Ensure condition failed: IsInGameThread()` 红字）。详见该函数的注释。
	//
	// **为什么用 `FInstancedStruct::AddStructReferencedObjects` 而不是
	// `AddPropertyReferencesWithStructARO`**：后者要求一个 `UScriptStruct*`（会连带收集结构体
	// 自身的引用），而 `FTcsParamSnapshotEntry` **不是反射结构体**（快照只在 C++ 侧流转）
	// ⇒ 没有 `StaticStruct()` 可给。逐条走 `SourceRef` 自己的 ARO 反而更准。
	This->ForEachSkillInstanceForGC([&Collector](const FTcsSkillInstance& Run)
	{
		for (const FTcsParamSnapshotEntry& Entry : Run.ParamSnapshot.Entries)
		{
			if (Entry.SourceRef.IsValid())
			{
				// 收集只枚举引用、不改内容——`const_cast` 仅为满足 ARO 的非 const 签名
				const_cast<FInstancedStruct&>(Entry.SourceRef).AddStructReferencedObjects(Collector);
			}
		}
	});

	// 账本条目不在本函数范围内：它按纪律 MUST NOT 持任何 UObject 引用（见 TcsSkillRegistry.h）。

	Super::AddReferencedObjects(InThis, Collector);
}
