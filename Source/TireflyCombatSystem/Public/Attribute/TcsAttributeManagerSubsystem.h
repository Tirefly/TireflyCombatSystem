// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsAttributeModifier.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeManagerSubsystem.generated.h"


class UTcsAttributeComponent;
class UTcsAttributeDefinitionAsset;
class UTcsAttributeModifierDefinitionAsset;


// 属性管理器子系统，所有战斗实体执行属性相关逻辑的入口
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsAttributeManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma endregion


#pragma region AttributeDefinitions

protected:
	// 缓存的属性定义（从 DeveloperSettings 加载）
	TMap<FName, const UTcsAttributeDefinitionAsset*> AttributeDefinitions;

	// 缓存的属性修改器定义（从 DeveloperSettings 加载）
	TMap<FName, const UTcsAttributeModifierDefinitionAsset*> AttributeModifierDefinitions;

	// AttributeTag -> AttributeName 映射（运行时构建，用于 Tag 入口 API）
	TMap<FGameplayTag, FName> AttributeTagToName;

	// AttributeName -> AttributeTag 映射（可选，用于反查和调试）
	TMap<FName, FGameplayTag> AttributeNameToTag;

	/**
	 * 从 DeveloperSettings 缓存加载定义（编辑器模式）
	 */
	void LoadFromDeveloperSettings();

	/**
	 * 从 AssetManager 加载定义（Runtime 模式）
	 */
	void LoadFromAssetManager();

public:
	/**
	 * 获取属性定义资产（迁移期供 Component 查询的 public 入口）
	 *
	 * @param AttributeName 属性名
	 * @return 属性定义资产指针；未找到返回 nullptr
	 */
	const UTcsAttributeDefinitionAsset* GetAttributeDefinitionAsset(FName AttributeName) const;

	/**
	 * 获取属性修改器定义资产（迁移期供 Component 查询的 public 入口）
	 *
	 * @param ModifierId 修改器定义 ID
	 * @return 修改器定义资产指针；未找到返回 nullptr
	 */
	const UTcsAttributeModifierDefinitionAsset* GetModifierDefinitionAsset(FName ModifierId) const;

#pragma endregion
	

#pragma region AttributeInstance

public:
	/**
	 * 通过 GameplayTag 解析属性名称
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param OutAttributeName 输出解析得到的属性名称（FName）
	 * @return 是否成功解析（Tag 是否在映射中注册）
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool TryResolveAttributeNameByTag(
		const FGameplayTag& AttributeTag,
		FName& OutAttributeName) const;

	/**
	 * 通过属性名称获取对应的 GameplayTag（反查，用于调试）
	 *
	 * @param AttributeName 属性名称（FName）
	 * @param OutAttributeTag 输出对应的 GameplayTag
	 * @return 是否成功获取（Name 是否在映射中注册）
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool TryGetAttributeTagByName(
		FName AttributeName,
		FGameplayTag& OutAttributeTag) const;

public:
	/** 分配全局唯一的属性实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateAttributeInstanceId() { return ++GlobalAttributeInstanceIdMgr; }

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

public:
	/** 分配全局唯一的修改器实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateModifierInstanceId() { return ++GlobalAttributeModifierInstanceIdMgr; }

	/** 分配全局唯一的修改器变更批次 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int64 AllocateModifierChangeBatchId() { return ++GlobalAttributeModifierChangeBatchIdMgr; }

protected:
	// 全局属性修改器实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeModifierInstanceIdMgr = -1;

	// 全局属性修改器“变更批次”管理器（用于本地归因/排序，不用于网络同步）
	UPROPERTY()
	int64 GlobalAttributeModifierChangeBatchIdMgr = -1;

#pragma endregion


#pragma region SourceHandle

public:
	/**
	 * 创建 SourceHandle (唯一的创建入口)
	 *
	 * @param CausalityChain 因果链 (从根源到直接父级的 PrimaryAssetId 有序链)
	 * @param Instigator 施加者Actor
	 * @param SourceTags Source类型标签 (可选)
	 * @return 创建的SourceHandle
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|SourceHandle")
	FTcsSourceHandle CreateSourceHandle(
		const TArray<FPrimaryAssetId>& CausalityChain,
		AActor* Instigator,
		const FGameplayTagContainer& SourceTags = FGameplayTagContainer());

protected:
	// 全局 SourceHandle ID 管理器
	UPROPERTY()
	int32 GlobalSourceHandleIdMgr = -1;

#pragma endregion
};
