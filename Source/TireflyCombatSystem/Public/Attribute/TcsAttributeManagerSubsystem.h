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
	 * @deprecated 请使用 UTcsAttributeComponent::AddAttribute
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::AddAttribute instead."))
	bool AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::AddAttributes
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::AddAttributes instead."))
	void AddAttributes(
		AActor* CombatEntity,
		const TArray<FName>& AttributeNames);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::AddAttributeByTag
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param InitValue 初始值
	 * @return 是否成功添加
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", Categories = "TCS.Attribute", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::AddAttributeByTag instead."))
	bool AddAttributeByTag(
		AActor* CombatEntity,
		const FGameplayTag& AttributeTag,
		float InitValue = 0.f);

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

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::SetAttributeBaseValue
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @param NewValue 新的Base值
	 * @param bTriggerEvents 是否触发事件（默认true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::SetAttributeBaseValue instead."))
	bool SetAttributeBaseValue(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::SetAttributeCurrentValue
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @param NewValue 新的Current值
	 * @param bTriggerEvents 是否触发事件（默认true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::SetAttributeCurrentValue instead."))
	bool SetAttributeCurrentValue(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::ResetAttribute
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @return 是否成功重置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::ResetAttribute instead."))
	bool ResetAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::RemoveAttribute
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::RemoveAttribute instead."))
	bool RemoveAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

protected:
	// 获取战斗实体的属性组件（迁移期内部辅助，Phase G 删除）
	static class UTcsAttributeComponent* GetAttributeComponent(const AActor* CombatEntity);

public:
	/** 分配全局唯一的属性实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateAttributeInstanceId() { return ++GlobalAttributeInstanceIdMgr; }

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

#pragma endregion
	

#pragma region AttributeModifier

public:
	/**
	 * @deprecated 请使用 UTcsAttributeComponent::CreateAttributeModifier
	 *
	 * @param ModifierId 属性修改器Id
	 * @param Instigator 属性修改器发起者
	 * @param Target 属性修改器目标
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::CreateAttributeModifier instead."))
	bool CreateAttributeModifier(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		AActor* Instigator,
		AActor* Target,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	/**
	 * @deprecated 请使用 UTcsAttributeComponent::CreateAttributeModifierWithOperands
	 *
	 * @param ModifierId 属性修改器Id
	 * @param Instigator 属性修改器发起者
	 * @param Target 属性修改器目标
	 * @param Operands 属性修改器操作数
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::CreateAttributeModifierWithOperands instead."))
	bool CreateAttributeModifierWithOperands(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		AActor* Instigator,
		AActor* Target,
		const TMap<FName, float>& Operands,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	/**
	 * @deprecated 请使用 UTcsAttributeComponent::ApplyModifier
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::ApplyModifier instead."))
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::ApplyModifierWithSourceHandle
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @param ModifierIds 要应用的修改器ID列表
	 * @param OutModifiers 输出创建的修改器实例列表
	 * @return 是否成功应用
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::ApplyModifierWithSourceHandle instead."))
	bool ApplyModifierWithSourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle,
		const TArray<FName>& ModifierIds,
		TArray<FTcsAttributeModifierInstance>& OutModifiers);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::RemoveModifier
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::RemoveModifier instead."))
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::RemoveModifiersBySourceHandle
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::RemoveModifiersBySourceHandle instead."))
	bool RemoveModifiersBySourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle);

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::GetModifiersBySourceHandle
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @param OutModifiers 输出查询到的修改器实例列表
	 * @return 是否查询到修改器
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::GetModifiersBySourceHandle instead."))
	bool GetModifiersBySourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsAttributeModifierInstance>& OutModifiers) const;

	/**
	 * @deprecated 请使用 UTcsAttributeComponent::HandleModifierUpdated
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", DeprecatedFunction, DeprecationMessage = "Use UTcsAttributeComponent::HandleModifierUpdated instead."))
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

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


#pragma region AttributeCalculation

protected:
	/** @deprecated Use UTcsAttributeComponent::RecalculateAttributeBaseValues */
	UE_DEPRECATED(5.6, "Use UTcsAttributeComponent::RecalculateAttributeBaseValues instead.")
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTcsAttributeModifierInstance>& Modifiers);

	/** @deprecated Use UTcsAttributeComponent::RecalculateAttributeCurrentValues */
	UE_DEPRECATED(5.6, "Use UTcsAttributeComponent::RecalculateAttributeCurrentValues instead.")
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId = -1);

	/** @deprecated Use UTcsAttributeComponent::MergeAttributeModifiers */
	UE_DEPRECATED(5.6, "Use UTcsAttributeComponent::MergeAttributeModifiers instead.")
	static void MergeAttributeModifiers(
		const AActor* CombatEntity,
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers);

	/** @deprecated Use UTcsAttributeComponent::ClampAttributeValueInRange */
	UE_DEPRECATED(5.6, "Use UTcsAttributeComponent::ClampAttributeValueInRange instead.")
	static void ClampAttributeValueInRange(
		UTcsAttributeComponent* AttributeComponent,
		const FName& AttributeName,
		float& NewValue,
		float* OutMinValue = nullptr,
		float* OutMaxValue = nullptr,
		const TMap<FName, float>* WorkingValues = nullptr);

	/** @deprecated Use UTcsAttributeComponent::EnforceAttributeRangeConstraints */
	UE_DEPRECATED(5.6, "Use UTcsAttributeComponent::EnforceAttributeRangeConstraints instead.")
	static void EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent);

#pragma endregion
};
