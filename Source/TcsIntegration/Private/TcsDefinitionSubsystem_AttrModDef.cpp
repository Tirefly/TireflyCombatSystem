// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Attribute/TcsAttrModDef.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverAttrModDefs()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		FailureList.Add(TEXT("<AssetRegistry>: 不可得——修正器模板资产发现被跳过"));
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: AssetRegistry 不可得——修正器模板资产发现被跳过"));
		return;
	}

	// 异步扫描就绪门（编辑器初始扫描是异步的：未完成时查询会**静默返回不完整结果**，不报错）
	if (AssetRegistry->IsLoadingAssets())
	{
		AssetRegistry->WaitForCompletion();
	}

	TArray<FAssetData> FoundAssets;
	const FTopLevelAssetPath ClassPath = UTcsAttrModDef::StaticClass()->GetClassPathName();
	if (!AssetRegistry->GetAssetsByClass(ClassPath, FoundAssets, /*bSearchSubClasses=*/false))
	{
		// 返回 false = 一个都没扫到（不是错误——尚无修正器模板资产是正常状态）
		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何修正器模板资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现修正器模板资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		UTcsAttrModDef* ModDefAsset = Cast<UTcsAttrModDef>(AssetData.GetAsset());
		if (!ModDefAsset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份非空（空身份无法按 [PrimaryAssetType, TemplateTag] 解析，也无从按 tag 寻址）
		if (!ModDefAsset->TemplateTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: TemplateTag 为空"), *AssetPath));
			continue;
		}

		// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发 / 状态定义的"重复定义"）
		if (AttrModDefs.Contains(ModDefAsset->TemplateTag))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 修正器模板身份 %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *ModDefAsset->TemplateTag.ToString()));
			continue;
		}

		AttrModDefs.Add(ModDefAsset->TemplateTag, ModDefAsset);
		AttrModDefAssets.Add(ModDefAsset);
	}
}



const UTcsAttrModDef* UTcsDefinitionSubsystem::ResolveAttrModDef(FGameplayTag TemplateTag) const
{
	const TObjectPtr<UTcsAttrModDef>* Found = AttrModDefs.Find(TemplateTag);
	return Found ? Found->Get() : nullptr;
}
