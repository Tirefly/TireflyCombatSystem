// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsStateSlot.h"
#include "State/TcsState.h"
#include "TcsSourceHandle.h"
#include "TcsStateManagerSubsystem.generated.h"



class UTcsStateInstance;
class UTcsStateComponent;
class UTcsStateDefinitionAsset;
class UTcsStateSlotDefinitionAsset;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma endregion


#pragma region StateDefinitions

protected:
	// 缓存的状态定义（从 DeveloperSettings 加载）
	TMap<FName, const UTcsStateDefinitionAsset*> StateDefinitions;

	// StateTag -> StateDefId 映射（运行时构建，用于 Tag 入口 API）
	TMap<FGameplayTag, FName> StateTagToDefId;

	// 缓存的状态槽定义（从 DeveloperSettings 加载）
	TMap<FName, const UTcsStateSlotDefinitionAsset*> StateSlotDefinitions;

	/**
	 * 从 DeveloperSettings 缓存加载定义（编辑器模式）
	 */
	void LoadFromDeveloperSettings();

	/**
	 * 从 AssetManager 加载定义（Runtime 模式）
	 */
	void LoadFromAssetManager();

	/**
	 * 按需加载 State 定义（内部方法）
	 * 仅在 OnDemand 或 Hybrid 策略下使用
	 *
	 * @param StateDefId 状态定义 ID
	 * @return 加载的状态定义资产指针，如果加载失败则返回 nullptr
	 */
	const UTcsStateDefinitionAsset* LoadStateOnDemand(FName StateDefId);

	/**
	 * 预加载所有 State 定义（内部方法）
	 * 在 PreloadAll 策略下使用
	 */
	void PreloadAllStates();

	/**
	 * 预加载常用 State 定义（内部方法）
	 * 在 Hybrid 策略下使用
	 */
	void PreloadCommonStates();

public:
	/**
	 * 获取状态定义资产
	 * 支持按需加载（当 StateLoadingStrategy 为 OnDemand 或 Hybrid 时）
	 *
	 * @param DefId 状态定义 ID
	 * @return 状态定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateDefinitionAsset* GetStateDefinitionAsset(FName DefId);

	/**
	 * 通过 StateTag 获取状态定义资产
	 * 支持按需加载（当 StateLoadingStrategy 为 OnDemand 或 Hybrid 时）
	 *
	 * @param StateTag 状态标签
	 * @return 状态定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateDefinitionAsset* GetStateDefinitionAssetByTag(FGameplayTag StateTag);

	/**
	 * 获取状态槽定义资产
	 *
	 * @param DefId 状态槽定义 ID
	 * @return 状态槽定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateSlotDefinitionAsset* GetStateSlotDefinitionAsset(FName DefId);

	/**
	 * 通过槽位标签获取状态槽定义资产
	 *
	 * @param SlotTag 状态槽标签
	 * @return 状态槽定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateSlotDefinitionAsset* GetStateSlotDefinitionAssetByTag(FGameplayTag SlotTag);

	/**
	 * 获取所有已缓存的 State 定义名称
	 * 注意：在 OnDemand 或 Hybrid 策略下，返回的是已加载的 State 名称，不包括未加载的
	 *
	 * @return State 定义名称数组
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	TArray<FName> GetAllStateDefNames() const;

	/** 获取所有已缓存的 StateSlot 定义名称。 */
	TArray<FName> GetAllStateSlotDefNames() const;

#pragma endregion
	

#pragma region MetaData

public:
	/** 分配全局唯一的状态实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }

protected:
	// 全局状态实例ID管理器
	UPROPERTY()
	int32 GlobalStateInstanceIdMgr = 0;

#pragma endregion


#pragma region StateApplying

public:
	/**
	 * 尝试向目标应用状态
	 *
	 * @param Target 目标，状态将应用到此目标
	 * @param StateDefId 状态定义名，可通过TcsGenericLibrary.GetStateDefIds获取
	 * @param Instigator 状态的发起者
	 * @param StateLevel 状态等级（默认为 1）
	 * @param ParentSourceHandle 父级来源句柄 (用于因果链传递, 默认为空)
	 * @return 如果应用状态成功，则返回true，否则返回false
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateToTarget(
		AActor* Target,
		FName StateDefId,
		AActor* Instigator,
		int32 StateLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

#pragma endregion
};
