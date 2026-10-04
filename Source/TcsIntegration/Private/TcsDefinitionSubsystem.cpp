// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Chain/TcsEffectChain.h"
#include "Chain/TcsEffectChainDef.h"
#include "Def/TcsBuffDef.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Trigger/TcsEffectTriggerDefAsset.h"
#include "Trigger/TcsEffectTriggerInstance.h"

#include "TcsEffectSubsystem.h"
#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DiscoverChainDefs();
	DiscoverTriggerDefs();
	DiscoverStateDefs();

	// 触发行来源句柄：本类在世界装配期登记触发行时作 `Source`（"定义库来源"这一级联退订锚点，发放一次）
	TriggerSeedSource = TriggerSourceRegistry.Allocate();

	// 就绪单出口：发现/校验完成即派发（幂等——只在此处置位并广播一次；06 §4 硬规则①）
	bRuntimeReady = true;

	// 世界可能已存在（PIE 里 GameInstance 与世界创建顺序、或宿主先手动 LoadMap）——立即装配
	SeedWorld(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);

	// 未来世界：订阅世界初始化（跨级方向：GameInstance 级持有 World 级引用，长寿→短寿合法）
	WorldInitDelegateHandle = FWorldDelegates::OnPostWorldInitialization.AddUObject(
		this, &UTcsDefinitionSubsystem::HandlePostWorldInitialization);

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 定义库就绪——链定义 %d 条，触发定义 %d 条，状态定义 %d 条，失败 %d 条"),
		ChainDefs.Num(), TriggerDefs.Num(), StateDefs.Num(), FailureList.Num());

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

	SeededWorld.Reset();
	ChainDefs.Empty();
	ChainDefAssets.Empty();
	TriggerDefs.Empty();
	TriggerDefAssets.Empty();
	StateDefs.Empty();
	StateDefAssets.Empty();
	TriggerSeedSource = FTcsSourceHandle();
	FailureList.Empty();
	bRuntimeReady = false;

	Super::Deinitialize();
}



void UTcsDefinitionSubsystem::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	UTcsDefinitionSubsystem* This = CastChecked<UTcsDefinitionSubsystem>(InThis);

	// 三份定义缓存都非 UPROPERTY（TUniquePtr 容器）——GC 只走 RefLink 看不见它们，故在此手动补引用。
	// 缓存内容里的 FInstancedStruct（链的 Steps / 触发定义的 Conditions / 状态定义的参数行与描述视图）
	// 内层可放宿主自定义 struct 的 UPROPERTY 对象引用（D4-16 类型不设限），须靠本函数保活
	// （引擎 UDataTable::AddReferencedObjects 对 RowMap 用的同一招，见头文件说明）。
	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectChain>>& Pair : This->ChainDefs)
	{
		if (const FTcsEffectChain* Chain = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsEffectChain::StaticStruct(), const_cast<FTcsEffectChain*>(Chain), This);
		}
	}

	for (const TPair<FGameplayTag, TUniquePtr<FTcsEffectTriggerDef>>& Pair : This->TriggerDefs)
	{
		if (const FTcsEffectTriggerDef* TriggerDef = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsEffectTriggerDef::StaticStruct(), const_cast<FTcsEffectTriggerDef*>(TriggerDef), This);
		}
	}

	// 状态定义缓存：参数行的数值来源与描述的视图载荷都是 FInstancedStruct（内层可放对象引用）
	for (const TPair<FGameplayTag, TUniquePtr<FTcsBuffDef>>& Pair : This->StateDefs)
	{
		if (const FTcsBuffDef* BuffDef = Pair.Value.Get())
		{
			Collector.AddPropertyReferencesWithStructARO(FTcsBuffDef::StaticStruct(), const_cast<FTcsBuffDef*>(BuffDef), This);
		}
	}

	Super::AddReferencedObjects(InThis, Collector);
}



void UTcsDefinitionSubsystem::DiscoverChainDefs()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		FailureList.Add(TEXT("<AssetRegistry>: 不可得——链资产发现被跳过"));
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: AssetRegistry 不可得——链资产发现被跳过"));
		return;
	}

	// 异步扫描就绪门（编辑器初始扫描是异步的：未完成时查询会**静默返回不完整结果**，不报错）
	if (AssetRegistry->IsLoadingAssets())
	{
		AssetRegistry->WaitForCompletion();
	}

	TArray<FAssetData> FoundAssets;
	const FTopLevelAssetPath ClassPath = UTcsEffectChainDef::StaticClass()->GetClassPathName();
	if (!AssetRegistry->GetAssetsByClass(ClassPath, FoundAssets, /*bSearchSubClasses=*/false))
	{
		// 返回 false = 一个都没扫到（不是错误——空项目/未创建链资产是正常状态）
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
	if (!World || (ChainDefs.Num() == 0 && TriggerDefs.Num() == 0))
	{
		return;
	}

	// 幂等：同一世界不重复装配（世界初始化可能触发多次回调路径）
	if (SeededWorld.Get() == World)
	{
		return;
	}

	UTcsEffectSubsystem* EffectSubsystem = World->GetSubsystem<UTcsEffectSubsystem>();
	if (!EffectSubsystem)
	{
		// 非游戏型世界（编辑器预览等）——该子系统自身按 DoesSupportWorldType 过滤，此处静默跳过
		return;
	}

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

	SeededWorld = World;

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 已装配到世界 %s——链定义 %d/%d 条，触发行 %d/%d 条"),
		*World->GetName(), Registered, ChainDefs.Num(), RegisteredRows, TriggerDefs.Num());
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
