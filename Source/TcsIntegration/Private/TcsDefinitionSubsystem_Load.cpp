// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "Asset/TcsAssetButlerSettings.h"
#include "Asset/TcsPrimaryAssetRegistry.h"
#include "Attribute/TcsAttrModDef.h"
#include "Attribute/TcsAttributeDef.h"
#include "Chain/TcsEffectChainDef.h"
#include "Def/TcsBuffDefAsset.h"
#include "Def/TcsSkillDef.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Trigger/TcsEffectTriggerDefAsset.h"

#include "TcsIntegrationLogChannel.h"



bool UTcsDefinitionSubsystem::IsSeedingType(FPrimaryAssetType Type) const
{
	// 装配型 = `SeedWorld` 会逐世界登记它的定义族的类型。
	// **判据 MUST 与 `SeedWorld` 的装配段同步增删**：新增一个装配族却忘了扩这里，
	// 那个族就能被配成 OnDemand 而静默装配为空。
	// 修正器模板 `UTcsAttrModDef` **不在其中**——它的消费面是资产直引用
	// （`TSoftObjectPtr<UTcsAttrModDef>`），不建每世界结构，故它是唯一合法可用 OnDemand 的 Def 类型。
	return Type == FPrimaryAssetType(UTcsEffectChainDef::StaticClass()->GetFName())
		|| Type == FPrimaryAssetType(UTcsEffectTriggerDefAsset::StaticClass()->GetFName())
		|| Type == FPrimaryAssetType(UTcsBuffDefAsset::StaticClass()->GetFName())
		|| Type == FPrimaryAssetType(UTcsSkillDef::StaticClass()->GetFName())
		|| Type == FPrimaryAssetType(UTcsAttributeDef::StaticClass()->GetFName());
}



bool UTcsDefinitionSubsystem::CheckDuplicates(const FDiscoverResult& InResult)
{
	// 键 = PrimaryAssetName（= DefTag.GetTagName()——各 Def 类的 GetPrimaryAssetId 都是纯函数）
	TMap<FName, FString> SeenPaths;
	bool bClean = true;

	for (const FAssetData& AssetData : InResult.Assets)
	{
		const FPrimaryAssetId PrimaryAssetId = AssetData.GetPrimaryAssetId();
		if (!PrimaryAssetId.IsValid())
		{
			// 身份无效不是重复问题，留给装配段按身份校验处理
			continue;
		}

		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();
		if (const FString* Existing = SeenPaths.Find(PrimaryAssetId.PrimaryAssetName))
		{
			// 同一路径出现两次不是重复（扫描幂等），只有路径不同才是真重复
			if (*Existing != AssetPath)
			{
				FailureList.Add(FString::Printf(
					TEXT("%s: 主资产身份 %s 重复——%s 与 %s 是同名不同路径的两个资产。"
						 "拒绝登记：引擎对同名是覆盖且扫描顺序无保证，会表现为跨启动随机生效一个，比整体失败更难查"),
					*InResult.Type.ToString(), *PrimaryAssetId.ToString(), **Existing, *AssetPath));
				bClean = false;
			}
			continue;
		}

		SeenPaths.Add(PrimaryAssetId.PrimaryAssetName, AssetPath);
	}

	return bClean;
}



bool UTcsDefinitionSubsystem::PreloadPrimaryAssetIds(
	UAssetManager* InAssetManager,
	const TArray<FPrimaryAssetId>& InIds,
	const FDiscoverResult& InResult,
	FString& OutError)
{
	if (!InAssetManager)
	{
		OutError = TEXT("<AssetManager>: 为空");
		return false;
	}

	// **未注册自查（MUST 在调用前做，实测 R-2）**：未注册类型会让 LoadPrimaryAssetsWithType
	// 拿到空数组 ⇒ CreateCombinedHandle 对空子句柄**同步执行 OnComplete** 并返回 nullptr ⇒
	// 「代理立刻被调用 + 句柄 nullptr + 零 Error 零 Warning」。只挂 OnComplete 的调用方会读成
	// "加载完成、共 0 个资产" ⇒ 计入成功 ⇒ 一路走到就绪。
	FPrimaryAssetTypeInfo TypeInfo;
	const bool bRegisteredInEngine = InAssetManager->GetPrimaryAssetTypeInfo(InResult.Type, TypeInfo);
	const bool bRegisteredInOurs = FTcsPrimaryAssetRegistry::Get().Find(InResult.Type) != nullptr;

	if (!bRegisteredInEngine || !bRegisteredInOurs)
	{
		OutError = FString::Printf(
			TEXT("主资产类型未注册（引擎 GetPrimaryAssetTypeInfo=%s / 本仓注册表=%s）——拒绝发起加载。"
				 "MUST NOT 靠《加载返回空》反推未注册，那会把未注册误读成《成功加载 0 个》"),
			bRegisteredInEngine ? TEXT("true") : TEXT("false"),
			bRegisteredInOurs ? TEXT("true") : TEXT("false"));
		return false;
	}

	TWeakObjectPtr<UTcsDefinitionSubsystem> WeakThis(this);

	const FStreamableDelegate OnComplete = FStreamableDelegate::CreateLambda(
		[WeakThis, InResult]()
		{
			UTcsDefinitionSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			// 装配本批已加载的资产（失败项进清单），再走单出口
			Self->AssembleLoadedType(InResult);
			Self->OnPreloadFinished(Self->FailureList.IsEmpty());
		});

	const TSharedPtr<FStreamableHandle> Handle =
		InAssetManager->LoadPrimaryAssets(InIds, TArray<FName>(), OnComplete);

	if (!Handle.IsValid())
	{
		// 注册已自查，故这里的真实原因是资产自身问题（路径失效等）
		OutError = FString::Printf(
			TEXT("异步预载未取得有效句柄（%d 个身份）——资产可能不存在或路径已失效"), InIds.Num());
		return false;
	}

	// **句柄必须由本类持有至反初始化**（裁定第 9 条）：句柄释放即触发卸载，
	// 已"预载"的资产会变回未加载。
	PreloadHandles.Add(Handle);
	return true;
}



void UTcsDefinitionSubsystem::AssembleLoadedType(const FDiscoverResult& InResult)
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();

	for (const FAssetData& AssetData : InResult.Assets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		// **这里才取对象**：发现阶段没加载，加载阶段已批量预载，此刻取对象应命中内存。
		//
		// **为什么用 `GetPrimaryAssetObject` 而不是 `FastGetAsset`**：后者按 `PackageName`
		// 找包（`FindObjectFast<UPackage>`），而引擎注释明写这个字段**可能与该资产加载后的
		// 实际路径不同**（"external assets may be saved in a different package to their loaded
		// path"）⇒ 用它当主判据会在那种资产上**静默返 null**，表现为"预载完成后仍取不到"。
		// `GetPrimaryAssetObject` 走引擎自己维护的 `[类型, 名] → FSoftObjectPtr` 映射，
		// 与 `LoadPrimaryAssets` 用的是同一份数据 ⇒ 才是可靠判据。
		//
		// **MUST NOT 用 `AssetData.GetAsset()` 兜底**：那会在预载未命中时**同步补加载**，
		// 正好是本策略要消灭的事；取不到就按失败登记，不让单个资产把整批拖成同步。
		const FPrimaryAssetId PrimaryAssetId = AssetData.GetPrimaryAssetId();
		UObject* Asset = AssetManager ? AssetManager->GetPrimaryAssetObject(PrimaryAssetId) : nullptr;

		if (!Asset)
		{
			// 退一步再试"只查不加载"：两者都落空才是真失败
			Asset = AssetData.FastGetAsset(/*bLoad=*/false);
		}

		if (!Asset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 预载完成后仍取不到资产对象"), *AssetPath));
			continue;
		}

		AssembleAsset(InResult.Type, Asset, AssetPath);
	}
}



void UTcsDefinitionSubsystem::AssembleAsset(
	FPrimaryAssetType Type, UObject* InAsset, const FString& InAssetPath)
{
	const FName TypeName = Type.GetName();

	if (TypeName == UTcsEffectChainDef::StaticClass()->GetFName())
	{
		AssembleChainDef(InAsset, InAssetPath);
	}
	else if (TypeName == UTcsEffectTriggerDefAsset::StaticClass()->GetFName())
	{
		AssembleTriggerDef(InAsset, InAssetPath);
	}
	else if (TypeName == UTcsBuffDefAsset::StaticClass()->GetFName())
	{
		AssembleStateDef(InAsset, InAssetPath);
	}
	else if (TypeName == UTcsAttrModDef::StaticClass()->GetFName())
	{
		AssembleAttrModDef(InAsset, InAssetPath);
	}
	else if (TypeName == UTcsSkillDef::StaticClass()->GetFName())
	{
		AssembleSkillDef(InAsset, InAssetPath);
	}
	else if (TypeName == UTcsAttributeDef::StaticClass()->GetFName())
	{
		AssembleAttributeDef(InAsset, InAssetPath);
	}
}



void UTcsDefinitionSubsystem::OnPreloadFinished(bool bInSucceeded)
{
	// 批次未全到时只递减计数并返回（"全部完成"才走单出口）
	if (PendingPreloadBatches > 0)
	{
		--PendingPreloadBatches;
	}

	if (PendingPreloadBatches > 0)
	{
		return;
	}

	// **失败即永不就绪**（裁定第 10 条）：只要出现过一次失败，本生命周期内不再置位。
	const bool bFinalFailure = !bInSucceeded || !FailureList.IsEmpty() || bLoadFailed;

	if (bFinalFailure)
	{
		bLoadFailed = true;
		bRuntimeReady = false;

		// **一次性、Error 级、含完整失败清单**（不是逐条刷屏）：这是"失败即永不就绪"的必要配套。
		// 没有它，现象是"组件全部不注册"，看起来像 TCS 没工作，实际是内容里有一个坏文件。
		UE_LOG(LogTcsIntegration, Error,
			TEXT("UTcsDefinitionSubsystem: 定义库**永不就绪**——失败 %d 条（这就是《整体停摆》的原因，逐条见下）"),
			FailureList.Num());

		for (const FString& Failure : FailureList)
		{
			UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: 失败——%s"), *Failure);
		}

		if (PendingWorlds.Num() > 0)
		{
			UE_LOG(LogTcsIntegration, Warning,
				TEXT("UTcsDefinitionSubsystem: 仍有 %d 个世界在等待装配——它们永远不会被装配（定义库未就绪）"),
				PendingWorlds.Num());
		}

		OnDefinitionsFailed.Broadcast();
		return;
	}

	// ── 就绪单出口（06 §4 硬规则① / M6 双层引导的最小版） ──
	//
	// **次序是契约，MUST NOT 调换**：置位 → 广播 → 装配世界。三条理由：
	// ① 置位先于广播：订阅者在回调里 MUST 能查到 IsRuntimeReady() == true——否则它只能
	//    "收到通知但门禁仍不放行"，通知就等于没发。
	// ② 广播先于装配世界：装配会逐世界登记并可能报 Warning，那些是装配面的读数；
	//    就绪信号承诺的是"定义已齐备"，不是"已装配到某个世界"。
	// ③ 幂等：本路径只在批次归零时走一次，Deinitialize 置回 false。
	bRuntimeReady = true;

	UE_LOG(LogTcsIntegration, Log,
		TEXT("UTcsDefinitionSubsystem: 定义库就绪——链定义 %d 条，触发定义 %d 条，状态定义 %d 条，"
			 "修正器模板 %d 条，技能定义 %d 条，属性定义 %d 条（OnDemand 目录 %d 个类型）"),
		ChainDefs.Num(), TriggerDefs.Num(), StateDefs.Num(), AttrModDefs.Num(),
		SkillDefs.Num(), AttributeDefs.Num(), OnDemandCatalog.Num());

	OnDefinitionsReady.Broadcast();

	// 世界可能已存在——立即装配
	SeedWorld(GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr);

	// **待装配世界的补装**（裁定第 8/15 条）：就绪前初始化过的世界在等这一刻。
	// 顺手剔除失效条目（弱引用悬空 ⇒ 留存只会让集合单调增长）。
	for (int32 Index = PendingWorlds.Num() - 1; Index >= 0; --Index)
	{
		UWorld* Pending = PendingWorlds[Index].Get();
		PendingWorlds.RemoveAtSwap(Index);

		if (!Pending)
		{
			continue;
		}

		SeedWorld(Pending);
	}

	PendingWorlds.Empty();
}



const TArray<FPrimaryAssetId>& UTcsDefinitionSubsystem::GetOnDemandCatalog(FPrimaryAssetType Type) const
{
	static const TArray<FPrimaryAssetId> EmptyCatalog;

	const TArray<FPrimaryAssetId>* Found = OnDemandCatalog.Find(Type);
	return Found ? *Found : EmptyCatalog;
}



int32 UTcsDefinitionSubsystem::GetOnDemandCatalogCount(FPrimaryAssetType Type) const
{
	const TArray<FPrimaryAssetId>* Found = OnDemandCatalog.Find(Type);
	return Found ? Found->Num() : 0;
}



bool UTcsDefinitionSubsystem::FindCatalogId(
	FPrimaryAssetType Type, FGameplayTag Tag, FPrimaryAssetId& OutId) const
{
	if (!Tag.IsValid())
	{
		return false;
	}

	const TArray<FPrimaryAssetId>* Catalog = OnDemandCatalog.Find(Type);
	if (!Catalog)
	{
		return false;
	}

	// 名位就是 DefTag.GetTagName()（各 Def 类的 GetPrimaryAssetId 都是纯函数）
	const FName Wanted = Tag.GetTagName();
	for (const FPrimaryAssetId& Id : *Catalog)
	{
		if (Id.PrimaryAssetName == Wanted)
		{
			OutId = Id;
			return true;
		}
	}

	return false;
}



UObject* UTcsDefinitionSubsystem::EnsureDefinitionLoaded(
	FPrimaryAssetType Type,
	FGameplayTag Tag,
	ETcsResolveMode Mode,
	FStreamableDelegate OnLoaded)
{
	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		return nullptr;
	}

	FPrimaryAssetId Id;
	if (!FindCatalogId(Type, Tag, Id))
	{
		// 目录里没有该身份：不是"没加载"，是"没这个定义"——正常查询路径，不 ensure、不 Error
		return nullptr;
	}

	// **已在内存的快路径**：先查对象，命中就直接返回并可同步触发代理。
	// 不做这一步会强行给已在内存的资产加一帧延迟，那是纯粹的损失。
	if (UObject* AlreadyLoaded = AssetManager->GetPrimaryAssetObject(Id))
	{
		if (OnLoaded.IsBound())
		{
			OnLoaded.Execute();
		}
		return AlreadyLoaded;
	}

	if (Mode == ETcsResolveMode::Sync)
	{
		// 逃生口：阻塞当帧（调用点 MUST 各自说明代价）
		UObject* Loaded = AssetManager->GetPrimaryAssetObject(Id);
		if (Loaded)
		{
			if (OnLoaded.IsBound())
			{
				OnLoaded.Execute();
			}
			return Loaded;
		}

		const TArray<FSoftObjectPath> Paths = { FSoftObjectPath(AssetManager->GetPrimaryAssetPath(Id)) };
		AssetManager->GetStreamableManager().RequestSyncLoad(Paths);

		if (OnLoaded.IsBound())
		{
			OnLoaded.Execute();
		}

		return AssetManager->GetPrimaryAssetObject(Id);
	}

	// Async：登记一次性加载，句柄由本类持有（否则句柄释放即卸载）。
	//
	// **加载完成后的装配收口在这里**，MUST NOT 让 6 个 Resolve* 各写一遍 lambda：
	// 六份重复 = 六套真相，改一处漏五处。
	TWeakObjectPtr<UTcsDefinitionSubsystem> WeakThis(this);
	const FString AssetPath = AssetManager->GetPrimaryAssetPath(Id).ToString();

	const FStreamableDelegate AssembleThenNotify = FStreamableDelegate::CreateLambda(
		[WeakThis, Type, Id, AssetPath, OnLoaded]()
		{
			UTcsDefinitionSubsystem* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}

			UAssetManager* Manager = UAssetManager::GetIfInitialized();
			UObject* Asset = Manager ? Manager->GetPrimaryAssetObject(Id) : nullptr;
			if (Asset)
			{
				// 装配是幂等的（重复身份会被身份校验拦下并记失败），但这里不去重——
				// 缓存命中时根本走不到本分支。
				Self->AssembleAsset(Type, Asset, AssetPath);
			}
			else
			{
				Self->FailureList.Add(FString::Printf(
					TEXT("%s: 按需加载完成后仍取不到资产对象（DefTag 解析失败）"), *AssetPath));
			}

			if (OnLoaded.IsBound())
			{
				OnLoaded.Execute();
			}
		});

	const TSharedPtr<FStreamableHandle> Handle =
		AssetManager->LoadPrimaryAsset(Id, TArray<FName>(), AssembleThenNotify);

	if (Handle.IsValid())
	{
		PreloadHandles.Add(Handle);
	}

	return nullptr;
}
