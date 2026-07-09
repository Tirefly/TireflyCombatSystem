// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsDefinitionManagerSubsystem.generated.h"



class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;
class UTcsBuffDefinition;
class UTcsSkillDefinition;
class UTcsSkillModifierDefinition;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;



/**
 * 运行时 Definition 管理子系统。
 *
 * 作为 TCS 运行时 Definition loaded cache、同步查询与后续异步加载策略的统一归口。
 * 所有缓存持有硬引用（TObjectPtr），确保已加载资产不会被 GC 回收。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsDefinitionManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

// GameInstanceSubsystem 生命周期
#pragma region GameInstanceSubsystem

public:
	/** 初始化运行时 Definition loaded cache。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 清理运行时 Definition cache。 */
	virtual void Deinitialize() override;

#pragma endregion


// Runtime-ready 状态
#pragma region RuntimeState

public:
	/**
	 * 查询当前 DefinitionManager 是否已进入 runtime-ready。
	 *
	 * @return 若当前子系统已完成初始化并可供运行时查询，则返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	bool IsRuntimeReady() const { return bIsRuntimeReady; }

protected:
	/** 当前 DefinitionManager 是否已完成 runtime-ready 初始化。 */
	UPROPERTY(Transient)
	bool bIsRuntimeReady = false;

#pragma endregion


// State-like Definition 查询（Buff / Skill / 合并 StateDef）
#pragma region StateLikeDefinitionQueries

public:
	/**
	 * 获取 Buff 定义资产。
	 *
	 * @param BuffDefId Buff 定义 ID。
	 * @return Buff 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsBuffDefinition* GetBuffDefinition(FName BuffDefId) const;

	/**
	 * 通过 GameplayTag 获取 Buff 定义资产。
	 *
	 * @param BuffTag Buff 定义语义标签。
	 * @return Buff 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsBuffDefinition* GetBuffDefinitionByTag(FGameplayTag BuffTag) const;

	/**
	 * 获取 Skill 定义资产。
	 *
	 * @param SkillDefId Skill 定义 ID。
	 * @return Skill 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsSkillDefinition* GetSkillDefinition(FName SkillDefId) const;

	/**
	 * 获取 State-like 定义资产（面向 State 模块内部的桥接查询）。
	 *
	 * 从合并的 StateDef 缓存中一次查找命中，无需逐个 dispatch。
	 * 该入口仅为 State 模块运行时便捷使用，不重建独立的抽象 StateDef 加载族或扫描中心。
	 *
	 * @param StateDefId 状态定义 ID。
	 * @return 状态定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsStateDefinition* GetStateDefinition(FName StateDefId) const;

	/** @return 当前 loaded cache 中全部 State-like Definition 定义 ID。 */
	TArray<FName> GetAllStateLikeDefIds() const;

#pragma endregion


// StateSlot Definition 查询
#pragma region StateSlotDefinitionQueries

public:
	/**
	 * 获取 StateSlot 定义资产。
	 *
	 * @param StateSlotDefId StateSlot 定义 ID。
	 * @return StateSlot 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsStateSlotDefinition* GetStateSlotDefinition(FName StateSlotDefId) const;

	/**
	 * 通过 GameplayTag 获取 StateSlot 定义资产。
	 *
	 * @param StateSlotTag StateSlot 语义标签。
	 * @return StateSlot 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsStateSlotDefinition* GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag) const;

	/** @return 当前 loaded cache 中全部 StateSlot 定义 ID。 */
	TArray<FName> GetAllStateSlotDefIds() const;

#pragma endregion


// Attribute Definition 查询
#pragma region AttributeDefinitionQueries

public:
	/**
	 * 获取 Attribute 定义资产。
	 *
	 * @param AttributeDefId Attribute 定义 ID。
	 * @return Attribute 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsAttributeDefinition* GetAttributeDefinition(FName AttributeDefId) const;

	/**
	 * 获取 AttributeModifier 定义资产。
	 *
	 * @param AttributeModifierDefId AttributeModifier 定义 ID。
	 * @return AttributeModifier 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsAttributeModifierDefinition* GetAttributeModifierDefinition(FName AttributeModifierDefId) const;

	/** @return 当前 loaded cache 中全部 Attribute 定义 ID。 */
	TArray<FName> GetAllAttributeDefIds() const;

	/** @return 当前 loaded cache 中全部 AttributeModifier 定义 ID。 */
	TArray<FName> GetAllAttributeModifierDefIds() const;

#pragma endregion


// SkillModifier Definition 查询
#pragma region SkillModifierDefinitionQueries

public:
	/**
	 * 获取 SkillModifier 定义资产。
	 *
	 * @param SkillModifierDefId SkillModifier 定义 ID。
	 * @return SkillModifier 定义资产指针；未找到时返回 nullptr。
	 */
	const UTcsSkillModifierDefinition* GetSkillModifierDefinition(FName SkillModifierDefId) const;

#pragma endregion


// Definition loaded cache 与内部维护
#pragma region DefinitionCache

protected:
	/** 从 AssetManager 重建所有具体 DefAsset loaded cache。 */
	void RebuildLoadedCache();

	/** 重建 BuffTag 与 StateSlotTag 查询索引。 */
	void RebuildTagIndexes();

	/** 记录 Definition 查询失败诊断。 */
	void LogDefinitionQueryFailure(FName QueryKey, const TCHAR* EntryName, const TCHAR* FailureCategory) const;

public:
	/** BuffDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsBuffDefinition>> BuffDefinitions;

	/** SkillDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsSkillDefinition>> SkillDefinitions;

	/**
	 * State-like Definition 合并 loaded cache（Buff + Skill）。
	 * 供 State 模块高频查询路径（如 CreateStateInstance）一次命中。
	 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsStateDefinition>> StateDefinitions;

	/** StateSlotDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsStateSlotDefinition>> StateSlotDefinitions;

	/** AttributeDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsAttributeDefinition>> AttributeDefinitions;

	/** AttributeModifierDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsAttributeModifierDefinition>> AttributeModifierDefinitions;

	/** SkillModifierDefinition loaded cache。 */
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTcsSkillModifierDefinition>> SkillModifierDefinitions;

	/** BuffTag 到 BuffDefId 的运行时查询索引。 */
	TMap<FGameplayTag, FName> BuffTagToDefId;

	/** StateSlotTag 到 StateSlotDefId 的运行时查询索引。 */
	TMap<FGameplayTag, FName> StateSlotTagToDefId;

#pragma endregion
};
