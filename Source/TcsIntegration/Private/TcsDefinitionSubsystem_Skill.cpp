// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Def/TcsSkillDef.h"
#include "Def/TcsSkillDefData.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::AssembleSkillDef(UObject* InAsset, const FString& InAssetPath)
{
	UTcsSkillDef* SkillDefAsset = Cast<UTcsSkillDef>(InAsset);
	if (!SkillDefAsset)
	{
		FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *InAssetPath));
		return;
	}

	// 校验一：身份有效（无效身份无法按 DefTag 寻址；内容级规则归资产 IsDataValid）
	if (!SkillDefAsset->DefTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *InAssetPath));
		return;
	}

	// 校验二：身份重复（同身份两个技能定义资产 = 内容冲突，不得静默覆写）
	if (SkillDefs.Contains(SkillDefAsset->DefTag))
	{
		FailureList.Add(FString::Printf(TEXT("%s: 技能定义身份 %s 已被另一个资产占用（重复定义）"),
			*InAssetPath, *SkillDefAsset->DefTag.ToString()));
		return;
	}

	SkillDefs.Add(SkillDefAsset->DefTag, MakeUnique<FTcsSkillDefData>(SkillDefAsset->SkillDef));
	SkillDefAssets.Add(SkillDefAsset);
}



const FTcsSkillDefData* UTcsDefinitionSubsystem::ResolveSkillDef(
	FGameplayTag DefTag, ETcsResolveMode Mode, FStreamableDelegate OnLoaded)
{
	// 快路径：已在缓存 ⇒ 直接返回并同步触发代理
	if (const TUniquePtr<FTcsSkillDefData>* Found = SkillDefs.Find(DefTag))
	{
		if (Found->IsValid())
		{
			if (OnLoaded.IsBound())
			{
				OnLoaded.Execute();
			}
			return Found->Get();
		}
	}

	const_cast<UTcsDefinitionSubsystem*>(this)->EnsureDefinitionLoaded(
		FPrimaryAssetType(UTcsSkillDef::StaticClass()->GetFName()), DefTag, Mode, OnLoaded);

	return nullptr;
}
