// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsSourceHandle.generated.h"



/**
 * SourceHandle 用于标识和追踪效果的来源
 *
 * 核心概念:
 * - Id: 全局唯一标识符（单调递增）
 * - Instigator: 实际造成效果的实体 (运行时 Actor)
 * - SourceTags: 来源标签（可选，用于分类/过滤）
 * - CausalityChain: 因果链，从根源到直接父级的 PrimaryAssetId 有序链（不包含自身）
 *
 * 示例:
 * - 根源 State (玩家释放技能): CausalityChain 为空
 * - 派生 State (技能 → Buff): CausalityChain = [技能StateDefAsset.PrimaryAssetId]
 * - 多层派生 (技能 → Buff → 持续伤害): CausalityChain = [技能Id, BuffId]
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSourceHandle
{
	GENERATED_BODY()

public:
	// 全局唯一来源 ID (单调递增, -1 表示无效)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	int32 Id = -1;

	// Source 类型标签 (可选, 用于分类和过滤)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	FGameplayTagContainer SourceTags;

	// 施加者 (实际造成效果的实体, 使用弱指针避免 GC 问题)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	TWeakObjectPtr<AActor> Instigator;

	// 因果链: 从根源到直接父级的 PrimaryAssetId 有序链 (不包含自身)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	TArray<FPrimaryAssetId> CausalityChain;

public:
	// 默认构造函数
	FTcsSourceHandle() = default;

	/**
	 * 完整构造函数
	 * @param InId 全局唯一ID
	 * @param InCausalityChain 因果链
	 * @param InInstigator 施加者Actor
	 * @param InSourceTags Source类型标签
	 */
	FTcsSourceHandle(int32 InId, const TArray<FPrimaryAssetId>& InCausalityChain,
		AActor* InInstigator, const FGameplayTagContainer& InSourceTags = FGameplayTagContainer())
		: Id(InId)
		, SourceTags(InSourceTags)
		, Instigator(InInstigator)
		, CausalityChain(InCausalityChain)
	{
	}

	/**
	 * 检查 SourceHandle 是否有效
	 * @return true 当且仅当 ID >= 0
	 */
	bool IsValid() const
	{
		return Id >= 0;
	}

	/**
	 * 生成调试字符串
	 * @return 格式: "[SH:ID] Instigator=ActorName Chain=[...]" 或 "[SH:ID]"
	 */
	FString ToDebugString() const
	{
		FString ChainStr;
		for (int32 i = 0; i < CausalityChain.Num(); ++i)
		{
			if (i > 0) ChainStr += TEXT("->");
			ChainStr += CausalityChain[i].ToString();
		}

		if (Instigator.IsValid())
		{
			return FString::Printf(TEXT("[SH:%d] Instigator=%s Chain=[%s]"),
				Id, *Instigator->GetName(), *ChainStr);
		}
		else
		{
			return FString::Printf(TEXT("[SH:%d] Chain=[%s]"), Id, *ChainStr);
		}
	}

	/**
	 * 网络序列化
	 * 实现自定义网络同步, 支持条件复制 (Instigator 有效时才复制)
	 * @param Ar 序列化归档
	 * @param Map 网络包映射
	 * @param bOutSuccess 输出是否成功
	 * @return 是否成功序列化
	 */
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);

	/**
	 * 相等性比较 (基于 ID)
	 * @param Other 另一个SourceHandle
	 * @return 是否相等
	 */
	bool operator==(const FTcsSourceHandle& Other) const
	{
		return Id == Other.Id;
	}

	/**
	 * 不等性比较 (基于 ID)
	 * @param Other 另一个SourceHandle
	 * @return 是否不等
	 */
	bool operator!=(const FTcsSourceHandle& Other) const
	{
		return Id != Other.Id;
	}

	/**
	 * 哈希函数 (支持作为 TMap 的 key)
	 * @param Handle SourceHandle
	 * @return 哈希值
	 */
	friend uint32 GetTypeHash(const FTcsSourceHandle& Handle)
	{
		return GetTypeHash(Handle.Id);
	}
};



// 启用网络序列化
template<>
struct TStructOpsTypeTraits<FTcsSourceHandle> : public TStructOpsTypeTraitsBase2<FTcsSourceHandle>
{
	enum
	{
		WithNetSerializer = true,
	};
};
