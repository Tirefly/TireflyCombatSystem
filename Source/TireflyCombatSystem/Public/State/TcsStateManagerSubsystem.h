// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsStateSlot.h"
#include "State/TcsStateInstance.h"
#include "TcsSourceHandle.h"
#include "TcsStateManagerSubsystem.generated.h"



class UTcsStateInstance;
class UTcsStateComponent;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;
class UTcsDefinitionRegistrySubsystem;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	/** 初始化状态定义缓存与编辑器联动入口。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 释放编辑器联动句柄并清理子系统生命周期资源。 */
	virtual void Deinitialize() override;

#pragma endregion


#pragma region RuntimeState

public:
	/**
	 * 查询当前 StateManager 是否已进入 runtime-ready。
	 *
	 * @return 若当前子系统已完成初始化并可供 runtime bootstrap 使用，则返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool IsRuntimeReady() const { return bIsRuntimeReady; }

protected:
	/** 当前 StateManager 是否已完成 runtime-ready 初始化。 */
	UPROPERTY(Transient)
	bool bIsRuntimeReady = false;

#pragma endregion


#pragma region DefinitionCaches

protected:
	/** 缓存的状态定义集合。 */
	TMap<FName, const UTcsStateDefinition*> StateDefinitions;

	/** StateTag 到 StateDefId 的运行时映射表。 */
	TMap<FGameplayTag, FName> StateTagToDefId;

	/** 缓存的状态槽定义集合。 */
	TMap<FName, const UTcsStateSlotDefinition*> StateSlotDefinitions;

#if WITH_EDITOR
	/** 获取编辑器阶段的 DefinitionRegistry 子系统。 */
	UTcsDefinitionRegistrySubsystem* GetDefinitionRegistry() const;

	/** 响应 DefinitionRegistry 刷新并重建本地缓存。 */
	void HandleDefinitionRegistryRefreshed(const UTcsDefinitionRegistrySubsystem* Registry);

	/** 获取状态定义的编辑器侧来源缓存。 */
	const TMap<FName, TSoftObjectPtr<UTcsStateDefinition>>* GetStateDefinitionSourceCache() const;

	/** 获取状态槽定义的编辑器侧来源缓存。 */
	const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>>* GetStateSlotDefinitionSourceCache() const;

	/** DefinitionRegistry 刷新事件句柄。 */
	FDelegateHandle DefinitionRegistryRefreshedHandle;
#endif

#pragma endregion


#pragma region DefinitionLoading

protected:

	/**
	 * 从 DeveloperSettings 缓存加载定义（编辑器模式）
	 */
	void LoadFromDeveloperSettings();

	/**
	 * 从 DefinitionRegistry 快照加载定义（编辑器模式）
	 */
	void LoadFromDefinitionRegistry();

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
	const UTcsStateDefinition* LoadStateOnDemand(FName StateDefId);

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

#pragma endregion


#pragma region DefinitionQueries

public:
	/**
	 * 获取状态定义资产
	 * 支持按需加载（当 StateLoadingStrategy 为 OnDemand 或 Hybrid 时）
	 *
	 * @param DefId 状态定义 ID
	 * @return 状态定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateDefinition* GetStateDefinition(FName DefId);

	/**
	 * 通过 StateTag 获取状态定义资产
	 * 支持按需加载（当 StateLoadingStrategy 为 OnDemand 或 Hybrid 时）
	 *
	 * @param StateTag 状态标签
	 * @return 状态定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateDefinition* GetStateDefinitionByTag(FGameplayTag StateTag);

	/**
	 * 获取状态槽定义资产
	 *
	 * @param DefId 状态槽定义 ID
	 * @return 状态槽定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateSlotDefinition* GetStateSlotDefinition(FName DefId);

	/**
	 * 通过槽位标签获取状态槽定义资产
	 *
	 * @param SlotTag 状态槽标签
	 * @return 状态槽定义资产指针，如果未找到则返回 nullptr
	 */
	const UTcsStateSlotDefinition* GetStateSlotDefinitionByTag(FGameplayTag SlotTag);

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
	

#pragma region RuntimeIds

public:
	/** 分配全局唯一的状态实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }

protected:
	/** 全局状态实例 ID 计数器。 */
	UPROPERTY()
	int32 GlobalStateInstanceIdMgr = 0;

#pragma endregion


#pragma region CrossActorApplyFacade

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
