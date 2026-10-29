// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "Attribute/TcsAttributeDef.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::AssembleAttributeDef(UObject* InAsset, const FString& InAssetPath)
{
	const UTcsAttributeDef* AttributeDefAsset = Cast<UTcsAttributeDef>(InAsset);
	if (!AttributeDefAsset)
	{
		FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *InAssetPath));
		return;
	}

	// 校验一：身份非空（空身份无法按 [PrimaryAssetType, DefTag] 解析，也无从按 tag 寻址——
	// `RegisterAttributeDef` 的第一道 ensure 也是它，但那时已经算"进入了域门面"，早拦一层）
	if (!AttributeDefAsset->DefTag.IsValid())
	{
		FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *InAssetPath));
		return;
	}

	// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发 / 状态三条）。
	// **只拦"本类缓存内"的重复**：跨世界门面里的同身份登记由 `SeedWorld` 的幂等守卫负责，
	// 两者不是同一层判据（前者的域是整个 GameInstance，后者只针对单个世界）。
	if (AttributeDefs.Contains(AttributeDefAsset->DefTag))
	{
		FailureList.Add(FString::Printf(TEXT("%s: 属性定义身份 %s 已被另一个资产占用（重复定义）"),
			*InAssetPath, *AttributeDefAsset->DefTag.ToString()));
		return;
	}

	// 内容按值拷进 `TUniquePtr`（与其余四份缓存同形；`TMap` 值按值置入会要求完整类型，
	// 而本类的公开头只前向声明 `FTcsAttributeDefData`）——资产对象同时进保活数组
	AttributeDefs.Add(AttributeDefAsset->DefTag, MakeUnique<FTcsAttributeDefData>(AttributeDefAsset->Def));
	AttributeDefAssets.Add(const_cast<UTcsAttributeDef*>(AttributeDefAsset));
}
