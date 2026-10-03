// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Trigger/TcsEffectTriggerDefAsset.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverTriggerDefs()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		FailureList.Add(TEXT("<AssetRegistry>: 不可得——触发定义资产发现被跳过"));
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: AssetRegistry 不可得——触发定义资产发现被跳过"));
		return;
	}

	// 异步扫描就绪门（编辑器初始扫描是异步的：未完成时查询会**静默返回不完整结果**，不报错）
	if (AssetRegistry->IsLoadingAssets())
	{
		AssetRegistry->WaitForCompletion();
	}

	TArray<FAssetData> FoundAssets;
	const FTopLevelAssetPath ClassPath = UTcsEffectTriggerDefAsset::StaticClass()->GetClassPathName();
	if (!AssetRegistry->GetAssetsByClass(ClassPath, FoundAssets, /*bSearchSubClasses=*/false))
	{
		// 返回 false = 一个都没扫到（不是错误——尚无触发定义资产是正常状态）
		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何触发定义资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现触发定义资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		UTcsEffectTriggerDefAsset* TriggerDefAsset = Cast<UTcsEffectTriggerDefAsset>(AssetData.GetAsset());
		if (!TriggerDefAsset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份非空（空身份无法按 [PrimaryAssetType, TriggerTag] 解析，且登记必被拒）
		if (!TriggerDefAsset->TriggerTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: TriggerTag 为空"), *AssetPath));
			continue;
		}

		// 校验二/三：定义必填项——`RegisterTriggerRow` 对这两项走 ensure 拒绝面，
		// 故在此提前拦下（不让编辑器启动时刷 ensure 红字，也不等到装配期才发现）
		if (!TriggerDefAsset->Def.EventTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: 定义不完整——Def.EventTag 为空（触发行无从订阅任何事件）"), *AssetPath));
			continue;
		}

		if (!TriggerDefAsset->Def.EffectChainId.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: 定义不完整——Def.EffectChainId 为空（命中后无链可执行）"), *AssetPath));
			continue;
		}

		// 校验四：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链的"重复登记"）
		if (TriggerDefs.Contains(TriggerDefAsset->TriggerTag))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 触发身份 %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *TriggerDefAsset->TriggerTag.ToString()));
			continue;
		}

		TriggerDefs.Add(TriggerDefAsset->TriggerTag, MakeUnique<FTcsEffectTriggerDef>(TriggerDefAsset->Def));
		TriggerDefAssets.Add(TriggerDefAsset);
	}
}



const FTcsEffectTriggerDef* UTcsDefinitionSubsystem::ResolveTriggerDef(FGameplayTag TriggerTag) const
{
	const TUniquePtr<FTcsEffectTriggerDef>* Found = TriggerDefs.Find(TriggerTag);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}
