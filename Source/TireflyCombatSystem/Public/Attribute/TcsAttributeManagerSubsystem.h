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

#pragma endregion
	

#pragma region AttributeInstance

public:
	// 给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void AddAttributes(
		AActor* CombatEntity,
		const TArray<FName>& AttributeNames);

	/**
	 * 通过 GameplayTag 给战斗实体添加属性
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param InitValue 初始值
	 * @return 是否成功添加（Tag 有效、在映射中注册、且属性不存在时返回 true）
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity", Categories = "TCS.Attribute"))
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
	 * 直接设置属性的Base值
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @param NewValue 新的Base值
	 * @param bTriggerEvents 是否触发事件（默认true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool SetAttributeBaseValue(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * 直接设置属性的Current值
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @param NewValue 新的Current值
	 * @param bTriggerEvents 是否触发事件（默认true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool SetAttributeCurrentValue(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * 重置属性到定义的初始值
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @return 是否成功重置
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool ResetAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

	/**
	 * 移除属性
	 *
	 * @param CombatEntity 战斗实体
	 * @param AttributeName 属性名称
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool RemoveAttribute(
		AActor* CombatEntity,
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

protected:
	// 获取战斗实体的属性组件
	static class UTcsAttributeComponent* GetAttributeComponent(const AActor* CombatEntity);

protected:
	// 全局属性实例ID管理器
	UPROPERTY()
	int32 GlobalAttributeInstanceIdMgr = -1;

#pragma endregion
	

#pragma region AttributeModifier

public:
	/**
	 * 创建属性修改器实例
	 * 
	 * @param ModifierId 属性修改器Id，通过TcsGenericLibrary.GetAttributeModifierIds获取
	 * @param SourceName 属性修改器来源，可以是技能Id、效果Id等
	 * @param Instigator 属性修改器发起者，应该是一个战斗实体
	 * @param Target 属性修改器目标，应该也是一个战斗实体
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool CreateAttributeModifier(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		FName SourceName,
		AActor* Instigator,
		AActor* Target,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	/**
	 * 创建属性修改器实例，并且设置属性修改器的操作数
	 * 
	 * @param ModifierId 属性修改器Id，通过TcsGenericLibrary.GetAttributeModifierIds获取
	 * @param SourceName 属性修改器来源，可以是技能Id、效果Id等
	 * @param Instigator 属性修改器发起者，应该是一个战斗实体
	 * @param Target 属性修改器目标，应该也是一个战斗实体
	 * @param Operands 属性修改器操作数
	 * @param OutModifierInst 最终创建的属性修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
	bool CreateAttributeModifierWithOperands(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		FName SourceName,
		AActor* Instigator,
		AActor* Target,
		const TMap<FName, float>& Operands,
		FTcsAttributeModifierInstance& OutModifierInst);
	
	// 给战斗实体应用多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void ApplyModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * 使用 SourceHandle 应用属性修改器
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @param ModifierIds 要应用的修改器ID列表
	 * @param OutModifiers 输出创建的修改器实例列表
	 * @return 是否成功应用
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool ApplyModifierWithSourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle,
		const TArray<FName>& ModifierIds,
		TArray<FTcsAttributeModifierInstance>& OutModifiers);

	// 从战斗实体身上移除多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void RemoveModifier(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * 按 SourceHandle 移除属性修改器
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool RemoveModifiersBySourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle);

	/**
	 * 按 SourceHandle 查询属性修改器
	 *
	 * @param CombatEntity 战斗实体
	 * @param SourceHandle 来源句柄
	 * @param OutModifiers 输出查询到的修改器实例列表
	 * @return 是否查询到修改器
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	bool GetModifiersBySourceHandle(
		AActor* CombatEntity,
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsAttributeModifierInstance>& OutModifiers) const;

	// 处理战斗实体的属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute", Meta = (DefaultToSelf = "CombatEntity"))
	void HandleModifierUpdated(AActor* CombatEntity, UPARAM(ref)TArray<FTcsAttributeModifierInstance>& Modifiers);

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
	 * 创建 SourceHandle (完整版本)
	 *
	 * @param SourceDefinition Source定义的DataTable引用
	 * @param SourceName Source名称
	 * @param SourceTags Source类型标签
	 * @param Instigator 施加者Actor
	 * @return 创建的SourceHandle
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|SourceHandle")
	FTcsSourceHandle CreateSourceHandle(
		const FDataTableRowHandle& SourceDefinition,
		FName SourceName,
		const FGameplayTagContainer& SourceTags,
		AActor* Instigator);

	/**
	 * 创建 SourceHandle (简化版本, 用于用户自定义效果)
	 *
	 * @param SourceName Source名称
	 * @param Instigator 施加者Actor
	 * @return 创建的SourceHandle
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|SourceHandle")
	FTcsSourceHandle CreateSourceHandleSimple(
		FName SourceName,
		AActor* Instigator);

protected:
	// 全局 SourceHandle ID 管理器
	UPROPERTY()
	int32 GlobalSourceHandleIdMgr = -1;

#pragma endregion


#pragma region AttributeCalculation

protected:
	// 重新计算战斗实体的属性基值
	static void RecalculateAttributeBaseValues(const AActor* CombatEntity, const TArray<FTcsAttributeModifierInstance>& Modifiers);

	// 重新计算战斗实体的属性当前值
	static void RecalculateAttributeCurrentValues(const AActor* CombatEntity, int64 ChangeBatchId = -1);

	// 属性修改器合并
	static void MergeAttributeModifiers(
		const AActor* CombatEntity,
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers);

	// 将属性的给定值限制在指定范围内
	// WorkingValues: 可选的工作集，用于从工作集读取动态范围属性值（两段式 Clamp）
	static void ClampAttributeValueInRange(
		UTcsAttributeComponent* AttributeComponent,
		const FName& AttributeName,
		float& NewValue,
		float* OutMinValue = nullptr,
		float* OutMaxValue = nullptr,
		const TMap<FName, float>* WorkingValues = nullptr);

	// 执行属性范围约束传播
	// 确保所有属性的 BaseValue 和 CurrentValue 都在其定义的范围内
	// 支持多跳依赖（如 HP <= MaxHP，MaxHP 依赖 Level）
	static void EnforceAttributeRangeConstraints(UTcsAttributeComponent* AttributeComponent);

#pragma endregion
};
