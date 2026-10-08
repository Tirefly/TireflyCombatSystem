// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "EventBus/TcsEventBusSubsystem.h"

#include "Skill/TcsCastOps.h"
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
	// **四者都要清**：运行态池 / 登记表 / 账本 / 注入的实体查询接口。
	// 不清注入的接口会让"未注入 ⇒ 降级放行"这条路径在跨世界后**再也测不到**
	// （上一世界注入的实现一直生效，表现为门禁第一道永远走宿主判定）。
	const int32 EntriesBefore = Registry.NumEntries();
	const int32 DefsBefore = RegisteredDefs.Num();
	const int32 RunsBefore = FTcsCastOps::ResetAll(*this);

	Registry.Reset();
	RegisteredDefs.Reset();
	EntityQuery = TScriptInterface<ITcsEntityQuery>();

	UE_LOG(LogTcsSkill, Log,
		TEXT("技能门面反初始化：账本条目 %d→%d 已登记定义 %d→%d 施法运行态 %d→%d，实体查询已清空"),
		EntriesBefore, Registry.NumEntries(), DefsBefore, RegisteredDefs.Num(),
		RunsBefore, GetCastRunCount());

	Super::Deinitialize();
}

UTcsEventBusSubsystem* UTcsSkillSubsystem::GetEventBus() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
}

void UTcsSkillSubsystem::ForEachCastRunForGC(TFunctionRef<void(const FTcsCastRun&)> Visitor) const
{
	// 转调池的 **GC 专用只读遍历**（`TTcsInstancePool::ForEachForGC`）。
	//
	// **★ 为什么不能直接用 `CastRuns.ForEach`（2026-10-08 首轮 PIE 实测暴露的真实缺陷）**：
	// 池的**每一个正常进入点**都无条件 `ensure(IsInGameThread())`（`TcsInstancePool.h` 的
	// `Allocate`/`Free`/`Resolve`/`IsValid`/`ForEach`/`Reset`，D0-4 单游戏线程假设），而
	// **GC 的引用收集不在游戏线程上** ⇒ 实测 callstack：
	// `FRealtimeGC::CollectReferencesForGC` → `AddReferencedObjects` → `TTcsInstancePool::ForEach`
	// → `Ensure condition failed: IsInGameThread()`（`TcsInstancePool.h:91`）。
	// **症状极具隐蔽性**：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"、且**与池空不空无关**
	// （首轮实测的两次命中都发生在探针运行之前 2 分 21 秒，即 GC 一跑就报）。
	// ⇒ ARO 内 MUST NOT 走池的公开进入点，改用 `ForEachForGC`（纯读、无断言、判据同 `ForEach`）。
	CastRuns.ForEachForGC(Visitor);
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
	// **⚠ 遍历口 MUST 用 `ForEachCastRunForGC`，MUST NOT 用 `CastRuns.ForEach`**
	// （2026-10-08 首轮 PIE 实测暴露：池的公开入口都断言游戏线程，而 GC 收集不在游戏线程
	// ⇒ 每次 GC 留一条 `Ensure condition failed: IsInGameThread()` 红字）。详见该函数的注释。
	//
	// **为什么用 `FInstancedStruct::AddStructReferencedObjects` 而不是
	// `AddPropertyReferencesWithStructARO`**：后者要求一个 `UScriptStruct*`（会连带收集结构体
	// 自身的引用），而 `FTcsParamSnapshotEntry` **不是反射结构体**（快照只在 C++ 侧流转）
	// ⇒ 没有 `StaticStruct()` 可给。逐条走 `SourceRef` 自己的 ARO 反而更准。
	This->ForEachCastRunForGC([&Collector](const FTcsCastRun& Run)
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
