// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Def/TcsBuffDefAsset.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverStateDefs()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		FailureList.Add(TEXT("<AssetRegistry>: 不可得——状态定义资产发现被跳过"));
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: AssetRegistry 不可得——状态定义资产发现被跳过"));
		return;
	}

	// 异步扫描就绪门（编辑器初始扫描是异步的：未完成时查询会**静默返回不完整结果**，不报错）
	if (AssetRegistry->IsLoadingAssets())
	{
		AssetRegistry->WaitForCompletion();
	}

	TArray<FAssetData> FoundAssets;
	const FTopLevelAssetPath ClassPath = UTcsBuffDefAsset::StaticClass()->GetClassPathName();
	if (!AssetRegistry->GetAssetsByClass(ClassPath, FoundAssets, /*bSearchSubClasses=*/false))
	{
		// 返回 false = 一个都没扫到（不是错误——尚无状态定义资产是正常状态）
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

		// 校验二：词表身份（StatusTag 是"同名不同态"的区分依据，空值让该定义在状态词表里无从落位）
		if (!BuffDefAsset->BuffDef.StatusTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: 定义不完整——BuffDef.StatusTag 为空（状态词表里无从落位）"), *AssetPath));
			continue;
		}

		// 校验三：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发定义的"重复定义"）
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
