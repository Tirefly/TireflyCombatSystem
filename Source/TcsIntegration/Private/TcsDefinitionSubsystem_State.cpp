// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Def/TcsBuffDefAsset.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::AssembleStateDef(UObject* InAsset, const FString& InAssetPath)
{
	UTcsBuffDefAsset* BuffDefAsset = Cast<UTcsBuffDefAsset>(InAsset);
	if (!BuffDefAsset)
	{
		FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *InAssetPath));
		return;
	}

	// 校验一：身份非空（空身份无法按 [PrimaryAssetType, DefTag] 解析，也无从按 tag 寻址）
	if (!BuffDefAsset->DefTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *InAssetPath));
		return;
	}

	// **原校验二已删除（2026-10-15）**：它拦的是 `Def.StatusTag` 非空。
	// `StatusTag` 已并入 `CategoryTags`（容器），而容器是**筛选依据而非身份** ⇒
	// 为空是合法配置（有消费者筛选时自然不命中，属明确结果而非缺省故障）。
	// 拦它属**误报**——发现期只判"这条资产能不能被按 tag 寻址"，与类别是否填了无关。

	// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发定义的"重复定义"）
	if (StateDefs.Contains(BuffDefAsset->DefTag))
	{
		FailureList.Add(FString::Printf(TEXT("%s: 状态定义身份 %s 已被另一个资产占用（重复定义）"),
			*InAssetPath, *BuffDefAsset->DefTag.ToString()));
		return;
	}

	StateDefs.Add(BuffDefAsset->DefTag, MakeUnique<FTcsBuffDef>(BuffDefAsset->BuffDef));
	StateDefAssets.Add(BuffDefAsset);
}



const FTcsBuffDef* UTcsDefinitionSubsystem::ResolveStateDef(
	FGameplayTag DefTag, ETcsResolveMode Mode, FStreamableDelegate OnLoaded)
{
	// 快路径：已在缓存 ⇒ 直接返回并**同步**触发代理（MUST NOT 制造无谓的一帧延迟）
	if (const TUniquePtr<FTcsBuffDef>* Found = StateDefs.Find(DefTag))
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

	// 不在缓存 ⇒ 按需加载（OnDemand / 未被选中的资产只有这条路径能进内存）。
	// 加载完成后的装配收口在 EnsureDefinitionLoaded 里，本处不重复。
	const_cast<UTcsDefinitionSubsystem*>(this)->EnsureDefinitionLoaded(
		FPrimaryAssetType(UTcsBuffDefAsset::StaticClass()->GetFName()), DefTag, Mode, OnLoaded);

	return nullptr;
}
