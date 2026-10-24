// Copyright Tirefly. All Rights Reserved.

#include "TcsDefinitionSubsystem.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Asset/TcsPrimaryAssetQuery.h"
#include "Attribute/TcsAttributeDef.h"

#include "TcsIntegrationLogChannel.h"



void UTcsDefinitionSubsystem::DiscoverAttributeDefs()
{
	// **发现手段**：走加载层统一查询口（同链 / 触发 / 状态三条），不直接 `GetAssetsByClass`。
	//
	// **为什么它必须走新口**：`FTcsPrimaryAssetQuery::GetAssetsForDefClass` 对**未登记**的主资产类型
	// 直接返回失败并给出具名诊断（`ETcsAssetLoadPolicy` 自注册注册表是它唯一的登记源）；而
	// `IAssetRegistry::GetAssetsByClass` 按**当前类路径**查索引，在类换过模块时会**静默返 0**
	// ——实测先例见 `DiscoverChainDefs` 的头注释（链资产 12 → 0）。
	// `UTcsAttributeDef` 的自注册由 `ARCH-04` D-2 于 2026-10-17 补齐（`UE_DEFINE_PRIMARY_ASSET_TYPE`），
	// 那是本函数**能返回正数**的硬前置（顺序：注册 → 发现，MUST NOT 反）。
	TArray<FAssetData> FoundAssets;
	FString Diagnostic;
	if (!FTcsPrimaryAssetQuery::GetAssetsForDefClass(
			UTcsAttributeDef::StaticClass(), FoundAssets, &Diagnostic))
	{
		// 失败清单与日志都要有：这是"加载层未登记"这条失败面的唯一出口
		if (!Diagnostic.IsEmpty())
		{
			FailureList.Add(FString::Printf(TEXT("属性定义资产: %s"), *Diagnostic));
		}

		UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 未发现任何属性定义资产（正常状态——尚无内容）"));
		return;
	}

	UE_LOG(LogTcsIntegration, Log, TEXT("UTcsDefinitionSubsystem: 发现属性定义资产 %d 个"), FoundAssets.Num());

	for (const FAssetData& AssetData : FoundAssets)
	{
		const FString AssetPath = AssetData.GetSoftObjectPath().ToString();

		const UTcsAttributeDef* AttributeDefAsset = Cast<UTcsAttributeDef>(AssetData.GetAsset());
		if (!AttributeDefAsset)
		{
			FailureList.Add(FString::Printf(TEXT("%s: 加载失败或类型不符"), *AssetPath));
			continue;
		}

		// 校验一：身份非空（空身份无法按 [PrimaryAssetType, DefTag] 解析，也无从按 tag 寻址——
		// `RegisterAttributeDef` 的第一道 ensure 也是它，但那时已经算"进入了域门面"，早拦一层）
		if (!AttributeDefAsset->DefTag.IsValid())
		{
			FailureList.Add(FString::Printf(TEXT("%s: DefTag 为空"), *AssetPath));
			continue;
		}

		// 校验二：身份重复（同身份两个资产 = 内容冲突，不得静默覆写——口径同链 / 触发 / 状态三条）。
		// **只拦"本类缓存内"的重复**：跨世界门面里的同身份登记由 `SeedWorld` 的幂等守卫负责，
		// 两者不是同一层判据（前者的域是整个 GameInstance，后者只针对单个世界）。
		if (AttributeDefs.Contains(AttributeDefAsset->DefTag))
		{
			FailureList.Add(FString::Printf(TEXT("%s: 属性定义身份 %s 已被另一个资产占用（重复定义）"),
				*AssetPath, *AttributeDefAsset->DefTag.ToString()));
			continue;
		}

		// 内容按值拷进 `TUniquePtr`（与其余四份缓存同形；`TMap` 值按值置入会要求完整类型，
		// 而本类的公开头只前向声明 `FTcsAttributeDefData`）——资产对象同时进保活数组
		AttributeDefs.Add(AttributeDefAsset->DefTag, MakeUnique<FTcsAttributeDefData>(AttributeDefAsset->Def));
		AttributeDefAssets.Add(const_cast<UTcsAttributeDef*>(AttributeDefAsset));
	}
}
