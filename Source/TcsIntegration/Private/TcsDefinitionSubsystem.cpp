// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Asset/TcsPrimaryAssetQuery.h"
#include "Attribute/TcsAttrModDef.h"
#include "Attribute/TcsAttributeDef.h"
#include "Chain/TcsEffectChain.h"
#include "Chain/TcsEffectChainDef.h"
#include "Def/TcsBuffDef.h"
#include "Def/TcsSkillDefData.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Trigger/TcsEffectTriggerDefAsset.h"
#include "Trigger/TcsEffectTriggerInstance.h"

#include "TcsAttributeSubsystem.h"
#include "TcsBuffSubsystem.h"
#include "TcsEffectSubsystem.h"
#include "TcsIntegrationLogChannel.h"
#include "TcsSkillSubsystem.h"



void UTcsDefinitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DiscoverChainDefs();
	DiscoverTriggerDefs();
	DiscoverStateDefs();
	DiscoverAttrModDefs();
	DiscoverSkillDefs();
	DiscoverAttributeDefs();

	// 触发行来源句柄：本类在世界装配期登记触发行时作 `Source`（"定义库来源"这一级联退订锚点，发放一次）
	TriggerSeedSource = TriggerSourceRegistry.Allocate();

	// ── 就绪单出口（06 §4 硬规则① / M6 双层引导的最小版） ──
	//
	// **次序是契约，MUST NOT 调换**：置位 → 广播 → 装配世界。三条理由：
	// ① **置位先于广播**：订阅者在本委托回调里 MUST 能查到 `IsRuntimeReady() == true`——否则它只能
	//    "收到通知但门禁仍不放行"，通知就等于没发。
	// ② **广播先于装配世界**：装配会逐世界登记链/触发行/状态定义并可能报 Warning，那些是**装配面**
	//    的读数；就绪信号承诺的是"**定义已齐备**"，不是"**已装配到某个世界**"（后者是 `SeedWorld` 的事，
	//    且每个世界各来一次）。把装配排在广播前会让订阅者在一个尚未装配的世界里被唤醒。
	// ③ **幂等**：本函数只在 `Initialize` 里跑一次，`Deinitialize` 置回 false ⇒ 无需防重入判据。
	bRuntimeReady = true;

	// 广播**恰好一次**（这是"单出口"的全部含义：一个信号点，不是每处调用点各广播一份）
	OnDefinitionsReady.Broadcast();

	// 世界可能已存在（PIE 里 GameInstance 与世界创建顺序、或宿主先手动 LoadMap）——立即装配
	SeedWorld(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);

	// 未来世界：订阅世界初始化（跨级方向：GameInstance 级持有 World 级引用，长寿→短寿合法）
	WorldInitDelegateHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UTcsDefinitionSubsystem::HandlePostWorldInitialization);

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 定义库就绪——链定义 %d 条，触发定义 %d 条，状态定义 %d 条，修正器模板 %d 条，技能定义 %d 条，属性定义 %d 条，失败 %d 条"),
		ChainDefs.Num(), TriggerDefs.Num(), StateDefs.Num(), AttrModDefs.Num(), SkillDefs.Num(), AttributeDefs.Num(), FailureList.Num());

	for (const FString& Failure : FailureList)
	{
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: 登记失败——%s"), *Failure);
	}
}

void UTcsDefinitionSubsystem::Deinitialize()
{
	if (WorldInitDelegateHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(WorldInitDelegateHandle);
		WorldInitDelegateHandle.Reset();
	}

	// ── 触发行来源的**级联撤销**（`ARCH-11` D-2：这是 `TriggerSeedSource` 唯一的撤销点） ──
	//
	// **为什么必须在这里做**：本类分配了 `TriggerSeedSource` 并在**每个世界**用它登记该世界的全部触发行
	// （"定义库来源"= 全局常驻规则）。本类反初始化时（GameInstance 销毁），那些世界**可能还活着**
	// （编辑器换 GameInstance、或世界比 GameInstance 活得久）——此时行还在册、来源句柄却已无主
	// （`FTcsSourceHandle` 是**纯编号、无存活性登记**，`TcsSourceHandle.h:42` 明写"只发号，不做注销登记"）
	// ⇒ 不给撤销点，"定义库来源"与"永不过期"就等价，这个句柄白发。
	//
	// **世界级容器的兜底不替代本步**：`UTcsEffectSubsystem::Deinitialize` 会 `TriggerRegistry.Reset(...)`
	// 全清 ⇒ **世界先死**的情况下确实不会泄漏。但那是**世界侧**的清理，成立的条件是"世界会被销毁"；
	// 而本函数可能在**世界仍存活**时被调用 ⇒ 两件事都要做，不能互相替代。
	for (const TWeakObjectPtr<UWorld>& SeededWorld : SeededWorlds)
	{
		UWorld* LiveWorld = SeededWorld.Get();
		if (!LiveWorld)
		{
			// 世界已销毁 ⇒ 它的门面也随之销毁，行随世界走，无需撤销
			continue;
		}

		UTcsEffectSubsystem* EffectSubsystem = LiveWorld->GetSubsystem<UTcsEffectSubsystem>();
		if (!EffectSubsystem)
		{
			continue;
		}

		const int32 RemovedRows = EffectSubsystem->UnregisterTriggerRowsBySource(TriggerSeedSource);
		UE_LOG(LogTcsIntegration, Log,
			TEXT("UTcsDefinitionSubsystem: 反初始化——世界 %s 按来源撤销触发行 %d 条"),
			*LiveWorld->GetName(), RemovedRows);
	}

	SeededWorlds.Empty();
	ChainDefs.Empty();
	ChainDefAssets.Empty();
	TriggerDefs.Empty();
	TriggerDefAssets.Empty();
	StateDefs.Empty();
	StateDefAssets.Empty();
	AttrModDefs.Empty();
	AttrModDefAssets.Empty();
	SkillDefs.Empty();
	SkillDefAssets.Empty();
	AttributeDefs.Empty();
	AttributeDefAssets.Empty();
	TriggerSeedSource = FTcsSourceHandle();
	FailureList.Empty();
	bRuntimeReady = false;

	Super::Deinitialize();
}



void UTcsDefinitionSubsystem::DiscoverChainDefs()
{
	// **发现手段（2026-10-15 Task 6 改）**：走加载层统一查询口，不再直接 `GetAssetsByClass`。
	// 判据与实测见 `Asset/TcsPrimaryAssetQuery.h` 的头注释：类换过模块后 `GetAssetsByClass`
	// 按**当前类路径**查索引会**静默返回 0**（本类的 `UTcsEffectChainDef` 于 Task 8 自
	// `TcsIntegration` 搬入 `TcsEffect` ⇒ 实测链资产发现由 12 个退为 0 个）。
	TArray<FAssetData> FoundAssets;
	FString Diagnostic;
	if (!FTcsPrimaryAssetQuery::GetAssetsForDefClass(
			UTcsEffectChainDef::StaticClass(), FoundAssets, &Diagnostic))
	{
		// "未登记"是**退化**（需补 UE_DEFINE_PRIMARY_ASSET_TYPE），进失败清单；
		// "已登记但零资产"是正常状态，只留日志。
		if (!Diagnostic.IsEmpty())
		{
			FailureList.Add(FString::Printf(TEXT("链资产: %s"), *Diagnostic));
		}

		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何链资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现链资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		UTcsEffectChainDef* ChainDef = Cast<UTcsEffectChainDef>(AssetData.GetAsset());
		if (!ChainDef)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份非空（空 id 无法按 [PrimaryAssetType, ChainId] 解析，且登记必被拒）
		if (!ChainDef->ChainId.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: ChainId 为空"), *AssetPath));
			continue;
		}

		// 校验二：双真相禁令（Chain.ChainId 与 ChainId 必须一致——不静默取其一）
		if (ChainDef->Chain.ChainId != ChainDef->ChainId)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 双真相——资产 ChainId(%s) 与 Chain.ChainId(%s) 不一致"),
				*AssetPath, *ChainDef->ChainId.ToString(), *ChainDef->Chain.ChainId.ToString()));
			continue;
		}

		// 校验三：重复登记（同 id 两个资产 = 内容冲突，不得静默覆写）
		if (ChainDefs.Contains(ChainDef->ChainId))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 链 id %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *ChainDef->ChainId.ToString()));
			continue;
		}

		ChainDefs.Add(ChainDef->ChainId, MakeUnique<FTcsEffectChain>(ChainDef->Chain));
		ChainDefAssets.Add(ChainDef);
	}
}



void UTcsDefinitionSubsystem::SeedWorld(UWorld* World)
{
	// 无内容即返回的判据 = 五类**可装配**定义全空。
	//
	// **判据 MUST 与「哪些定义族要装配」同步增删**：若新增一个装配族却忘了扩这一行，则
	// 「世界里只有该族定义」的工程会**整场不装配**，而失败面是静默的——本函数直接 return、
	// 连日志都没有（对照：四段登记各自都有分子/分母日志）。⇒ 新增装配族 MUST 同批改这一行。
	// 修正器模板不在其中：它的消费面是**资产直引用**（`TSoftObjectPtr<UTcsAttrModDef>`），不建每世界结构。
	if (!World || (ChainDefs.Num() == 0 && TriggerDefs.Num() == 0 && StateDefs.Num() == 0
		&& SkillDefs.Num() == 0 && AttributeDefs.Num() == 0))
	{
		return;
	}

	// 幂等：同一世界不重复装配（世界初始化可能触发多次回调路径）。
	// 顺手剔除已销毁世界（`TWeakObjectPtr` 悬空 ⇒ 留存只会让集合单调增长）。
	for (int32 Index = SeededWorlds.Num() - 1; Index >= 0; --Index)
	{
		UWorld* Recorded = SeededWorlds[Index].Get();
		if (!Recorded)
		{
			SeededWorlds.RemoveAtSwap(Index);
			continue;
		}

		if (Recorded == World)
		{
			return;
		}
	}

	UTcsEffectSubsystem* EffectSubsystem = World->GetSubsystem<UTcsEffectSubsystem>();
	if (!EffectSubsystem)
	{
		// 非游戏型世界（编辑器预览等）——该子系统自身按 DoesSupportWorldType 过滤，此处静默跳过
		return;
	}

	// 状态门面（R5 Task 2）：状态定义逐世界登记进它。存在性由世界子系统机制保证
	// （与 EffectSubsystem 同批实例化），故此处不做门禁——但取不到就静默跳过该批（非游戏世界）
	UTcsBuffSubsystem* StateSubsystem = World->GetSubsystem<UTcsBuffSubsystem>();

	// 技能门面（R6 Task 2）：技能定义逐世界登记进它（**改判**——Task 1 时"只进缓存"的前提是
	// 运行期消费者为零，而账本正是那个消费者：它的读取面全都要定义内容，且 TcsSkill 不能反向
	// 依赖本类（成环）⇒ 定义 MUST 经登记口单向写进门面）
	UTcsSkillSubsystem* SkillSubsystem = World->GetSubsystem<UTcsSkillSubsystem>();

	// 属性门面（2026-10-17 `ARCH-04` U-1）：属性定义逐世界登记进它。缺它就跳过该批（非游戏世界）
	UTcsAttributeSubsystem* AttributeSubsystem = World->GetSubsystem<UTcsAttributeSubsystem>();

	int32 Registered = 0;
	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectChain>>& Pair : ChainDefs)
	{
		if (Pair.Value.IsValid() && EffectSubsystem->RegisterChain(*Pair.Value))
		{
			++Registered;
		}
	}

	// 触发定义 → 该世界的触发行（**在链之后**：引用链预检要查得到链）。
	// `Source` = 定义库来源句柄——"定义加载期登记（全局常驻规则）"这条规格由此成立。
	int32 RegisteredRows = 0;
	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectTriggerDef>>& Pair : TriggerDefs)
	{
		const FTcsEffectTriggerDef* TriggerDef = Pair.Value.Get();
		if (!TriggerDef)
		{
			continue;
		}

		// 引用链预检：不可解析 → Warning + **仍登记**（宿主可在世界运行期自行 `RegisterChain`，
		// "先起世界后登记链"是合法时序；跳过登记会让该规则永久静默失效）。
		// 不 ensure / 不 Error：内容缺口不是契约违规，且 `ExecuteChain` 已有自己的拒绝面。
		if (!EffectSubsystem->IsChainRegistered(TriggerDef->EffectChainId))
		{
			UE_LOG(LogTcsIntegration, Warning,
				TEXT("UTcsDefinitionSubsystem: 触发定义 %s 引用的链 %s 在该世界未登记——仍登记该行（宿主可在运行期登记该链）"),
				*Pair.Key.ToString(), *TriggerDef->EffectChainId.ToString());
		}

		FTcsEffectTriggerInstance Instance;
		Instance.Def = *TriggerDef;
		Instance.Source = TriggerSeedSource;

		if (EffectSubsystem->RegisterTriggerRow(Instance).IsValid())
		{
			++RegisteredRows;
		}
	}

	// 状态定义 → 该世界的状态门面（**在链与触发行之后**：顺序只要求"不早于消费方存在"，
	// 状态定义与链/触发行之间无引用关系）。**不与本类共享生命周期**：门面按值拷一份，
	// 故本类缓存被清（世界切换 / 反初始化）不会让门面手里的定义悬空。
	// 身份取自缓存键（= 资产侧 `UTcsStateDef::DefTag`）——身份归资产，内容归数据 struct。
	int32 RegisteredStateDefs = 0;
	if (StateSubsystem)
	{
		for (const TPair<FGameplayTag, TUniquePtr<FTcsBuffDef>>& Pair : StateDefs)
		{
			if (Pair.Value.IsValid() && StateSubsystem->RegisterBuffDef(Pair.Key, *Pair.Value))
			{
				++RegisteredStateDefs;
			}
		}
	}

	// 技能定义 → 该世界的技能门面（**排在链 / 触发行 / 状态定义之后**——与状态定义同档：
	// 顺序只要求"不早于消费方存在"，技能定义与链/触发行/状态定义之间无引用关系）。
	// **不与本类共享生命周期**：门面按值拷一份，故本类缓存被清（世界切换 / 反初始化）
	// 不会让门面手里的定义悬空。
	// 身份取自缓存键（= 资产侧 `UTcsSkillDef::DefTag`）——身份归资产，内容归数据 struct。
	int32 RegisteredSkillDefs = 0;
	if (SkillSubsystem)
	{
		for (const TPair<FGameplayTag, TUniquePtr<FTcsSkillDefData>>& Pair : SkillDefs)
		{
			if (Pair.Value.IsValid() && SkillSubsystem->RegisterSkillDef(Pair.Key, *Pair.Value))
			{
				++RegisteredSkillDefs;
			}
		}
	}

	// 属性定义 → 该世界的属性门面（**排在最后**：与其他定义族之间无引用关系，位置只为次序一致）。
	//
	// **幂等守卫是硬要求，不是防御性编程**：`RegisterAttributeDef` 对重复身份走 `ensureMsgf`
	// （`TcsAttributeSubsystem.cpp:88`，**会刷红字**），而本类补齐之前，"宿主自己扫定义并登记"是
	// **唯一合法用法**（`TcsDevBootstrap::LoadAttributeDefs` 就是它）。⇒ 宿主若先登记过，本段 MUST
	// 跳过而不是让 ensure 响——否则本类的补齐会**把原本正确的宿主用法变成错误**。
	// 这与 `RegisterBuffDef` / `RegisterSkillDef` 的差别是**对方的口语义不同**（那两个返回具名失败），
	// 不是本类可以省的一层。
	int32 RegisteredAttributeDefs = 0;
	if (AttributeSubsystem)
	{
		for (const TPair<FGameplayTag, TUniquePtr<FTcsAttributeDefData>>& Pair : AttributeDefs)
		{
			if (!Pair.Value.IsValid())
			{
				continue;
			}

			// 已登记 ⇒ 视为已就位（不重复登记、不刷 ensure）
			if (AttributeSubsystem->FindAttributeDef(Pair.Key))
			{
				++RegisteredAttributeDefs;
				continue;
			}

			if (AttributeSubsystem->RegisterAttributeDef(Pair.Key, *Pair.Value))
			{
				++RegisteredAttributeDefs;
			}
		}
	}

	SeededWorlds.AddUnique(World);

	UE_LOG(LogTcsIntegration, Log,
		TEXT("UTcsDefinitionSubsystem: 已装配到世界 %s——链定义 %d/%d 条，触发行 %d/%d 条，状态定义 %d/%d 条，技能定义 %d/%d 条，属性定义 %d/%d 条"),
		*World->GetName(), Registered, ChainDefs.Num(), RegisteredRows, TriggerDefs.Num(),
		RegisteredStateDefs, StateDefs.Num(), RegisteredSkillDefs, SkillDefs.Num(),
		RegisteredAttributeDefs, AttributeDefs.Num());
}

void UTcsDefinitionSubsystem::HandlePostWorldInitialization(UWorld* World, const UWorld::InitializationValues IVS)
{
	SeedWorld(World);
}



const FTcsEffectChain* UTcsDefinitionSubsystem::ResolveChain(FGameplayTag ChainId) const
{
	const TUniquePtr<FTcsEffectChain>* Found = ChainDefs.Find(ChainId);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}
