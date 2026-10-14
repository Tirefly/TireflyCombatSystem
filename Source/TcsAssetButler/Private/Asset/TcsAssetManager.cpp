// Copyright Tirefly. All Rights Reserved.

#include "Asset/TcsAssetManager.h"

#include "Asset/TcsAssetButlerSettings.h"
#include "Asset/TcsPrimaryAssetRegistry.h"
#include "TcsAssetButlerLogChannel.h"



void UTcsAssetManager::StartInitialLoading()
{
	// 基类：扫描 UAssetManagerSettings.PrimaryAssetTypesToScan + 开 bulk scan 窗口
	// （GInitialBulkScan.StartOnce(this)，直到 OnPostEngineInit 才收）+ 广播 OnAssetManagerCreatedDelegate
	Super::StartInitialLoading();

	UE_LOG(LogTcsAssetButler, Log,
		TEXT("UTcsAssetManager: StartInitialLoading 进入补扫段——此刻 HasInitialScanCompleted=%d（应为 0：初始扫描尚未收束）"),
		HasInitialScanCompleted() ? 1 : 0);

	// 补扫：本仓代码自注册的类型清单（裁定⑤ 绕开 ini）
	int32 ScannedTypeCount = 0;

	// 宿主可改的扫描排除前缀（编辑器/加载期规则，**不影响模拟语义**）
	const UTcsAssetButlerSettings& Settings = *GetDefault<UTcsAssetButlerSettings>();

	FTcsPrimaryAssetRegistry::Get().ForEach([this, &ScannedTypeCount, &Settings](const FTcsPrimaryAssetEntry& Entry)
	{
		// 按设定排除前缀过滤扫描路径（前缀匹配；过滤后为空则该类型退化为"不扫"）
		TArray<FString> EffectivePaths;
		EffectivePaths.Reserve(Entry.ScanPaths.Num());

		for (const FString& Path : Entry.ScanPaths)
		{
			const bool bExcluded = Settings.ExcludedScanPathPrefixes.ContainsByPredicate(
				[&Path](const FString& Prefix) { return !Prefix.IsEmpty() && Path.StartsWith(Prefix); });

			if (bExcluded)
			{
				UE_LOG(LogTcsAssetButler, Log, TEXT("UTcsAssetManager: 扫描路径 %s 命中排除前缀设定——跳过"), *Path);
				continue;
			}

			EffectivePaths.Add(Path);
		}

		if (EffectivePaths.IsEmpty())
		{
			UE_LOG(LogTcsAssetButler, Warning, TEXT("UTcsAssetManager: 主资产类型 %s 的扫描路径被排除设定清空——该类型不扫"), *Entry.Type.ToString());
			return;
		}

		// 引擎要 UClass* 而非延迟 getter——注册表已解析，此处直接取
		const int32 AddedAssetCount = ScanPathsForPrimaryAssets(
			Entry.Type,
			EffectivePaths,
			Entry.BaseClass,
			Entry.bHasBlueprintClasses,
			Entry.bIsEditorOnly);

		++ScannedTypeCount;

		UE_LOG(LogTcsAssetButler, Log,
			TEXT("UTcsAssetManager: 扫描主资产类型 %s ——路径 %d 条，命中资产 %d 个"),
			*Entry.Type.ToString(), EffectivePaths.Num(), AddedAssetCount);
	});

	// T1 时序自证（§6 T1 实测判据）：补扫结束即挂初始扫描完成钩子。
	// 若它在此后**稍后才被广播**，则说明补扫落在同一初始扫描批次内（未错过 bulk scan、未延误收束）；
	// 若它当场同步执行（HasCompletedInitialScan 已为 true），则补扫落在收束之后。
	CallOrRegister_OnCompletedInitialScan(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
	{
		UE_LOG(LogTcsAssetButler, Log,
			TEXT("UTcsAssetManager: OnCompletedInitialScan 已广播——补扫结果已落在同一初始扫描批次内"));
	}));

	UE_LOG(LogTcsAssetButler, Log,
		TEXT("UTcsAssetManager: StartInitialLoading 完成——补扫代码注册类型 %d 个（策略仅声明归属，扫描与策略无关）"),
		ScannedTypeCount);
}