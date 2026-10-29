// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Attribute/TcsAttrModDef.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::AssembleAttrModDef(UObject* InAsset, const FString& InAssetPath)
{
	UTcsAttrModDef* ModDefAsset = Cast<UTcsAttrModDef>(InAsset);
	if (!ModDefAsset)
	{
		FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *InAssetPath));
		return;
	}

	// 校验一：身份非空（空身份无法按 [PrimaryAssetType, TemplateTag] 解析，也无从按 tag 寻址）
	if (!ModDefAsset->TemplateTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: TemplateTag 为空"), *InAssetPath));
		return;
	}

	// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发 / 状态定义的"重复定义"）
	if (AttrModDefs.Contains(ModDefAsset->TemplateTag))
	{
		FailureList.Add(FString::Printf(TEXT("%s: 修正器模板身份 %s 已被另一个资产占用（重复定义）"),
			*InAssetPath, *ModDefAsset->TemplateTag.ToString()));
		return;
	}

	AttrModDefs.Add(ModDefAsset->TemplateTag, ModDefAsset);
	AttrModDefAssets.Add(ModDefAsset);
}



const UTcsAttrModDef* UTcsDefinitionSubsystem::ResolveAttrModDef(
	FGameplayTag TemplateTag, ETcsResolveMode Mode, FStreamableDelegate OnLoaded)
{
	// 快路径：已在缓存 ⇒ 直接返回并同步触发代理
	if (const TObjectPtr<UTcsAttrModDef>* Found = AttrModDefs.Find(TemplateTag))
	{
		if (*Found)
		{
			if (OnLoaded.IsBound())
			{
				OnLoaded.Execute();
			}
			return Found->Get();
		}
	}

	const_cast<UTcsDefinitionSubsystem*>(this)->EnsureDefinitionLoaded(
		FPrimaryAssetType(UTcsAttrModDef::StaticClass()->GetFName()), TemplateTag, Mode, OnLoaded);

	return nullptr;
}
