// Copyright Tirefly. All Rights Reserved.

#include "Asset/TcsPrimaryAssetQuery.h"

#include "Asset/TcsAssetButlerSettings.h"
#include "Asset/TcsPrimaryAssetEntry.h"
#include "Asset/TcsPrimaryAssetRegistry.h"
#include "TcsAssetButlerLogChannel.h"

#include "Engine/AssetManager.h"
#include "Engine/Engine.h"



bool FTcsPrimaryAssetQuery::GetAssetsForDefClass(UClass* DefClass, TArray<FAssetData>& OutAssets, FString* OutDiagnostic)
{
	OutAssets.Reset();

	const auto Fail = [OutDiagnostic](const FString& Reason) -> bool
	{
		if (OutDiagnostic)
		{
			*OutDiagnostic = Reason;
		}
		return false;
	};

	if (!DefClass)
	{
		return Fail(TEXT("<DefClass>: 为空——资产发现无法进行"));
	}

	UAssetManager* AssetManager = UAssetManager::GetIfInitialized();
	if (!AssetManager)
	{
		// AssetManagerClassName 未配或类解析失败时引擎是 Fatal（UnrealEngine.cpp:3826），
		// 故这里通常不可达；仍显式报告，避免退化成"静默 0 个"。
		return Fail(TEXT("<AssetManager>: 不可得——资产发现无法进行（AssetManagerClassName 未配置？）"));
	}

	// 类型名与 FTcsPrimaryAssetRegistry::ResolveEntry 同口径（反射名派生，零字符串面）
	const FPrimaryAssetType PrimaryAssetType(DefClass->GetFName());

	// ★ 先分辨"未登记"与"已登记但空"——这是本口相对 GetAssetsByClass 的**新增可观测性**，
	//   也是本仓 2026-10-06 对 AssetManager 路径的既有顾虑（静默返空）的消解点。
	const FTcsPrimaryAssetEntry* Entry = FTcsPrimaryAssetRegistry::Get().Find(PrimaryAssetType);
	if (!Entry)
	{
		UE_LOG(LogTcsAssetButler, Warning,
			TEXT("FTcsPrimaryAssetQuery::GetAssetsForDefClass: 主资产类型 %s 未登记——该类型的资产不会被扫到。")
			TEXT("该 Def 类需在自己的 .cpp 里 UE_DEFINE_PRIMARY_ASSET_TYPE（只登记不扫描时也 MUST 登记）"),
			*PrimaryAssetType.ToString());

		return Fail(FString::Printf(
			TEXT("<AssetManager>: 主资产类型 %s 未登记——该 Def 类尚未 UE_DEFINE_PRIMARY_ASSET_TYPE"),
			*PrimaryAssetType.ToString()));
	}

	// 初始扫描未收束时按类型查询会**返回不完整结果**（与 GetAssetsByClass 的 IsLoadingAssets 同一条纪律）
	if (!AssetManager->HasInitialScanCompleted())
	{
		UE_LOG(LogTcsAssetButler, Log,
			TEXT("FTcsPrimaryAssetQuery::GetAssetsForDefClass: 主资产类型 %s ——初始扫描尚未收束，查询结果可能不完整"),
			*PrimaryAssetType.ToString());
	}

	TArray<FPrimaryAssetId> PrimaryAssetIds;
	AssetManager->GetPrimaryAssetIdList(PrimaryAssetType, PrimaryAssetIds);

	if (PrimaryAssetIds.IsEmpty())
	{
		// 已登记但空 = 正常状态（尚无内容），不算失败
		UE_LOG(LogTcsAssetButler, Log,
			TEXT("FTcsPrimaryAssetQuery::GetAssetsForDefClass: 主资产类型 %s 已登记，但当前零资产（正常状态——尚无内容）"),
			*PrimaryAssetType.ToString());
		return false;
	}

	OutAssets.Reserve(PrimaryAssetIds.Num());
	for (const FPrimaryAssetId& PrimaryAssetId : PrimaryAssetIds)
	{
		FAssetData AssetData;
		if (AssetManager->GetPrimaryAssetData(PrimaryAssetId, AssetData))
		{
			OutAssets.Add(MoveTemp(AssetData));
			continue;
		}

		// 登记在册却取不到 FAssetData：报告而非静默丢弃
		UE_LOG(LogTcsAssetButler, Warning,
			TEXT("FTcsPrimaryAssetQuery::GetAssetsForDefClass: 主资产 %s 已登记但取不到 FAssetData——该项被跳过"),
			*PrimaryAssetId.ToString());
	}

	UE_LOG(LogTcsAssetButler, Log,
		TEXT("FTcsPrimaryAssetQuery::GetAssetsForDefClass: 主资产类型 %s ——登记 %d 项，取得 FAssetData %d 项"),
		*PrimaryAssetType.ToString(), PrimaryAssetIds.Num(), OutAssets.Num());

return !OutAssets.IsEmpty();
}
