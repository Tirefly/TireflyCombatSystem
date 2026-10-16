// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Asset/TcsPrimaryAssetQuery.h"
#include "Def/TcsBuffDefAsset.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverStateDefs()
{
	// **发现手段（2026-10-15 Task 6 改）**：该类于 Task 4 自 `TcsState` 搬入 `TcsBuff`，
	// `GetAssetsByClass` 按当前类路径查索引会**静默返 0**（实测 PIE 装置 `Tcs.Test.State.Run`
	// 的 S2「buff 定义 未命中」及其下游 S5~S12 连锁 FAIL，由 26/0 退为 6/9）。
	TArray<FAssetData> FoundAssets;
	FString Diagnostic;
	if (!FTcsPrimaryAssetQuery::GetAssetsForDefClass(
			UTcsBuffDefAsset::StaticClass(), FoundAssets, &Diagnostic))
	{
		if (!Diagnostic.IsEmpty())
		{
			FailureList.Add(FString::Printf(TEXT("状态定义资产: %s"), *Diagnostic));
		}

		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何状态定义资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现状态定义资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		UTcsBuffDefAsset* BuffDefAsset = Cast<UTcsBuffDefAsset>(AssetData.GetAsset());
		if (!BuffDefAsset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份非空（空身份无法按 [PrimaryAssetType, DefTag] 解析，也无从按 tag 寻址）
		if (!BuffDefAsset->DefTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *AssetPath));
			continue;
		}

		// **原校验二已删除（2026-10-15）**：它拦的是 `Def.StatusTag` 非空。
		// `StatusTag` 已并入 `CategoryTags`（容器），而容器是**筛选依据而非身份** ⇒
		// 为空是合法配置（有消费者筛选时自然不命中，属明确结果而非缺省故障）。
		// 拦它属**误报**——发现期只判"这条资产能不能被按 tag 寻址"，与类别是否填了无关。

		// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发定义的"重复定义"）
		if (StateDefs.Contains(BuffDefAsset->DefTag))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 状态定义身份 %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *BuffDefAsset->DefTag.ToString()));
			continue;
		}

		StateDefs.Add(BuffDefAsset->DefTag, MakeUnique<FTcsBuffDef>(BuffDefAsset->BuffDef));
		StateDefAssets.Add(BuffDefAsset);
	}
}



const FTcsBuffDef* UTcsDefinitionSubsystem::ResolveStateDef(FGameplayTag DefTag) const
{
	const TUniquePtr<FTcsBuffDef>* Found = StateDefs.Find(DefTag);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}
