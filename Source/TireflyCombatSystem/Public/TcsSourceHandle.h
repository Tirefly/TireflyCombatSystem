// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "TcsSourceHandle.generated.h"



/**
 * SourceHandle 用于标识和追踪效果的来源
 *
 * 核心概念:
 * - Source: 效果的定义/配置 (通过 DataTable 引用)
 * - Instigator: 实际造成效果的实体 (运行时 Actor)
 *
 * 示例:
 * - 技能直接造成伤害: Source=技能Definition, Instigator=角色
 * - 陷阱造成伤害: Source=技能Definition(继承), Instigator=陷阱
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSourceHandle
{
	GENERATED_BODY()

public:
	// 全局唯一来源 ID (单调递增, -1 表示无效)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	int32 Id = -1;

	// Source 定义的 DataTable 引用 (可选, 用户自定义效果可以为空)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	FDataTableRowHandle SourceDefinition;

	// Source 名称 (冗余字段, 用于快速访问和调试)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	FName SourceName = NAME_None;

	// Source 类型标签 (用于分类和过滤)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	FGameplayTagContainer SourceTags;

	// 施加者 (实际造成效果的实体, 使用弱指针避免 GC 问题)
	UPROPERTY(BlueprintReadOnly, Category = "Source Handle")
	TWeakObjectPtr<AActor> Instigator;

public:
	// 默认构造函数
	FTcsSourceHandle() = default;

	/**
	 * 完整构造函数
	 * @param InId 全局唯一ID
	 * @param InSourceDefinition Source定义的DataTable引用
	 * @param InSourceName Source名称
	 * @param InSourceTags Source类型标签
	 * @param InInstigator 施加者Actor
	 */
	FTcsSourceHandle(int32 InId, const FDataTableRowHandle& InSourceDefinition, FName InSourceName,
		const FGameplayTagContainer& InSourceTags, AActor* InInstigator)
		: Id(InId)
		, SourceDefinition(InSourceDefinition)
		, SourceName(InSourceName)
		, SourceTags(InSourceTags)
		, Instigator(InInstigator)
	{
	}

	/**
	 * 简化构造函数 (用于用户自定义效果, 无 DataTable Definition)
	 * @param InId 全局唯一ID
	 * @param InSourceName Source名称
	 * @param InInstigator 施加者Actor
	 */
	FTcsSourceHandle(int32 InId, FName InSourceName, AActor* InInstigator)
		: Id(InId)
		, SourceName(InSourceName)
		, Instigator(InInstigator)
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
	 * 获取 Source Definition
	 * @return Source Definition 指针, 如果不存在则返回 nullptr
	 */
	template<typename T>
	T* GetSourceDefinition() const
	{
		if (SourceDefinition.IsNull())
		{
			return nullptr;
		}

		return SourceDefinition.GetRow<T>(TEXT("TcsSourceHandle"));
	}

	/**
	 * 生成调试字符串
	 * @return 格式: "[SourceName|ID] Instigator=ActorName" 或 "[SourceName|ID]"
	 */
	FString ToDebugString() const
	{
		if (Instigator.IsValid())
		{
			return FString::Printf(TEXT("[%s|%d] Instigator=%s"),
				*SourceName.ToString(), Id, *Instigator->GetName());
		}
		else
		{
			return FString::Printf(TEXT("[%s|%d]"), *SourceName.ToString(), Id);
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
