// Copyright Tirefly. All Rights Reserved.

#include "TcsSkillSubsystem.h"

#include "TcsSkillLogChannel.h"



// 生命期
void UTcsSkillSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 本门面无需预建每世界共享对象：账本与定义登记表都按需增长
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
	// **三者都要清**：登记表 / 账本 / 注入的实体查询接口。
	// 不清注入的接口会让"未注入 ⇒ 降级放行"这条路径在跨世界后**再也测不到**
	// （上一世界注入的实现一直生效，表现为门禁第一道永远走宿主判定）。
	const int32 EntriesBefore = Registry.NumEntries();
	const int32 DefsBefore = RegisteredDefs.Num();

	Registry.Reset();
	RegisteredDefs.Reset();
	EntityQuery = TScriptInterface<ITcsEntityQuery>();

	UE_LOG(LogTcsSkill, Log, TEXT("技能门面反初始化：账本条目 %d→%d 已登记定义 %d→%d，实体查询已清空"),
		EntriesBefore, Registry.NumEntries(), DefsBefore, RegisteredDefs.Num());

	Super::Deinitialize();
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

	// 账本条目不在本函数范围内：它按纪律 MUST NOT 持任何 UObject 引用（见 TcsSkillRegistry.h）。

	Super::AddReferencedObjects(InThis, Collector);
}
