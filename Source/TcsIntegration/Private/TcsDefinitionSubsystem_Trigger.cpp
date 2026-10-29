// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Trigger/TcsEffectTriggerDefAsset.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::AssembleTriggerDef(UObject* InAsset, const FString& InAssetPath)
{
	UTcsEffectTriggerDefAsset* TriggerDefAsset = Cast<UTcsEffectTriggerDefAsset>(InAsset);
	if (!TriggerDefAsset)
	{
		FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *InAssetPath));
		return;
	}

	// 校验一：身份非空（空身份无法按 [PrimaryAssetType, TriggerTag] 解析，且登记必被拒）
	if (!TriggerDefAsset->TriggerTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: TriggerTag 为空"), *InAssetPath));
		return;
	}

	// 校验二/三：定义必填项——`RegisterTriggerRow` 对这两项走 ensure 拒绝面，
	// 故在此提前拦下（不让编辑器启动时刷 ensure 红字，也不等到装配期才发现）
	if (!TriggerDefAsset->Def.EventTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: 定义不完整——Def.EventTag 为空（触发行无从订阅任何事件）"), *InAssetPath));
		return;
	}

	if (!TriggerDefAsset->Def.EffectChainId.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: 定义不完整——Def.EffectChainId 为空（命中后无链可执行）"), *InAssetPath));
		return;
	}

	// 校验四：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链的"重复登记"）
	if (TriggerDefs.Contains(TriggerDefAsset->TriggerTag))
	{
		FailureList.Add(FString::Printf(TEXT("%s: 触发身份 %s 已被另一个资产占用（重复定义）"),
			*InAssetPath, *TriggerDefAsset->TriggerTag.ToString()));
		return;
	}

	TriggerDefs.Add(TriggerDefAsset->TriggerTag, MakeUnique<FTcsEffectTriggerDef>(TriggerDefAsset->Def));
	TriggerDefAssets.Add(TriggerDefAsset);
}



const FTcsEffectTriggerDef* UTcsDefinitionSubsystem::ResolveTriggerDef(
	FGameplayTag TriggerTag, ETcsResolveMode Mode, FStreamableDelegate OnLoaded)
{
	// 快路径：已在缓存 ⇒ 直接返回并同步触发代理
	if (const TUniquePtr<FTcsEffectTriggerDef>* Found = TriggerDefs.Find(TriggerTag))
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
		FPrimaryAssetType(UTcsEffectTriggerDefAsset::StaticClass()->GetFName()), TriggerTag, Mode, OnLoaded);

	return nullptr;
}
