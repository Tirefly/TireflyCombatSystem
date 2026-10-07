// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Def/TcsSkillDef.h"
#include "Def/TcsSkillDefData.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverSkillDefs()
{
	IAssetRegistry* AssetRegistry = IAssetRegistry::Get();
	if (!AssetRegistry)
	{
		FailureList.Add(TEXT("<AssetRegistry>: 不可得——技能定义资产发现被跳过"));
		UE_LOG(LogTcsIntegration, Error, TEXT("UTcsDefinitionSubsystem: AssetRegistry 不可得——技能定义资产发现被跳过"));
		return;
	}

	// 异步扫描就绪门（编辑器初始扫描未完成时，按类查询会静默返回不完整结果）
	if (AssetRegistry->IsLoadingAssets())
	{
		AssetRegistry->WaitForCompletion();
	}

	TArray<FAssetData> FoundAssets;
	const FTopLevelAssetPath ClassPath = UTcsSkillDef::StaticClass()->GetClassPathName();
	if (!AssetRegistry->GetAssetsByClass(ClassPath, FoundAssets, /*bSearchSubClasses=*/false))
	{
		// 未发现资产是正常状态，不计入失败清单。
		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何技能定义资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现技能定义资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		UTcsSkillDef* SkillDefAsset = Cast<UTcsSkillDef>(AssetData.GetAsset());
		if (!SkillDefAsset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份有效（无效身份无法按 DefTag 寻址；内容级规则归资产 IsDataValid）
		if (!SkillDefAsset->DefTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *AssetPath));
			continue;
		}

		// 校验二：身份重复（同身份两个技能定义资产 = 内容冲突，不得静默覆写）
		if (SkillDefs.Contains(SkillDefAsset->DefTag))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 技能定义身份 %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *SkillDefAsset->DefTag.ToString()));
			continue;
		}

		SkillDefs.Add(SkillDefAsset->DefTag, MakeUnique<FTcsSkillDefData>(SkillDefAsset->SkillDef));
		SkillDefAssets.Add(SkillDefAsset);
	}
}



const FTcsSkillDefData* UTcsDefinitionSubsystem::ResolveSkillDef(FGameplayTag DefTag) const
{
	const TUniquePtr<FTcsSkillDefData>* Found = SkillDefs.Find(DefTag);
	return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}
