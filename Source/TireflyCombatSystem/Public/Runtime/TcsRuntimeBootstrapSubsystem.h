// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/ObjectKey.h"
#include "Runtime/TcsRuntimeBootstrapTypes.h"
#include "TcsRuntimeBootstrapSubsystem.generated.h"



class AActor;
class UActorComponent;
class UTcsAttributeComponent;
class UTcsBuffComponent;
class UTcsSkillComponent;
class UTcsStateComponent;
class UTcsDefinitionManagerSubsystem;



/**
 * 等待进入 ready 的实体登记记录。
 */
USTRUCT()
struct FTcsPendingEntityRegistration
{
	GENERATED_BODY()

	/** 当前登记的目标实体。 */
	UPROPERTY()
	TWeakObjectPtr<AActor> Entity;

	/** 实体首次进入 ready 时触发的一次性回调。 */
	UPROPERTY()
	FTcsOnEntityReadyDynamicDelegate OnReady;
};



/**
 * bootstrap 内部维护的实体运行时跟踪数据。
 */
USTRUCT()
struct FTcsTrackedEntityRuntimeData
{
	GENERATED_BODY()

	/** AttributeComponent 是否已向 bootstrap 报到。 */
	UPROPERTY()
	bool bAttributeRegistered = false;

	/** AttributeComponent 是否已满足完整 runtime-ready 条件。 */
	UPROPERTY()
	bool bAttributeReady = false;

	/** StateComponent 是否已向 bootstrap 报到。 */
	UPROPERTY()
	bool bStateRegistered = false;

	/** StateComponent 是否已满足完整 runtime-ready 条件。 */
	UPROPERTY()
	bool bStateReady = false;

	/** BuffComponent 是否已向 bootstrap 报到。 */
	UPROPERTY()
	bool bBuffRegistered = false;

	/** BuffComponent 是否已满足完整 runtime-ready 条件。 */
	UPROPERTY()
	bool bBuffReady = false;

	/** SkillComponent 是否已向 bootstrap 报到。 */
	UPROPERTY()
	bool bSkillRegistered = false;

	/** SkillComponent 是否已满足完整 runtime-ready 条件。 */
	UPROPERTY()
	bool bSkillReady = false;

	/**
	 * 判断当前记录中是否仍存在任何被跟踪的组件状态。
	 *
	 * @return 若仍存在被跟踪组件则返回 true，否则返回 false
	 */
	bool HasAnyTrackedData() const;
};



/**
 * TCS 运行时 bootstrap 子系统。
 *
 * 负责维护 entity 级 waiting registration、显式 runtime 评估和最小组件协作入口。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsRuntimeBootstrapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	/** 初始化 bootstrap 子系统并显式依赖核心 ManagerSubsystem。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 释放 bootstrap 子系统持有的 waiting / tracking 记录。 */
	virtual void Deinitialize() override;

#pragma endregion


#pragma region Registration

public:
	/**
	 * 显式把一个战斗实体纳入 runtime 编排。
	 *
	 * @param Entity 要注册的目标实体
	 * @param OnReady 可选的一次性 ready 回调
	 * @return 当前注册结果
	 */
	UFUNCTION(BlueprintCallable, Category = "TCS|RuntimeBootstrap")
	ETcsRegisterEntityResult RegisterEntity(
		AActor* Entity,
		FTcsOnEntityReadyDynamicDelegate OnReady);

	/**
	 * 查询实体是否已被显式纳入 TCS runtime bootstrap。
	 *
	 * @param Entity 要查询的目标实体
	 * @return 若实体已显式注册则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintPure, Category = "TCS|RuntimeBootstrap")
	bool IsEntityRegisteredForRuntimeBootstrap(AActor* Entity) const;

	/**
	 * 评估实体当前的 runtime 状态。
	 *
	 * @param Entity 要评估的目标实体
	 * @return runtime 状态与阻塞原因
	 */
	UFUNCTION(BlueprintCallable, Category = "TCS|RuntimeBootstrap")
	FTcsEntityRuntimeStateResult EvaluateEntityRuntimeState(AActor* Entity) const;

#pragma endregion


#pragma region ComponentCoordination

public:
	/**
	 * 通知 bootstrap 某个组件已完成主注册。
	 *
	 * @param Component 已注册的组件
	 */
	void NotifyComponentRegistered(UActorComponent* Component);

	/**
	 * 通知 bootstrap 某个组件即将注销。
	 *
	 * @param Component 即将注销的组件
	 */
	void NotifyComponentUnregistered(UActorComponent* Component);

	/**
	 * 通知 bootstrap 某个组件的 runtime 相关状态已发生变化。
	 *
	 * @param Component 已发生 runtime 相关状态变化的组件
	 */
	void NotifyComponentRuntimeStateChanged(UActorComponent* Component);

	/**
	 * 通知 bootstrap `StateComponent` 的 runtime-ready 状态已变化。
	 *
	 * @param StateComponent 已发生 runtime-ready 状态变化的状态组件
	 * @param bIsRuntimeReady 当前最新的 runtime-ready 状态
	 */
	void NotifyStateRuntimeReadyChanged(UTcsStateComponent* StateComponent, bool bIsRuntimeReady);

#pragma endregion


#pragma region InternalState

protected:
	/** 缓存的 DefinitionManagerSubsystem 指针，用于 runtime-ready 门控。 */
	UPROPERTY(Transient)
	TObjectPtr<UTcsDefinitionManagerSubsystem> DefinitionManagerSubsystem;

	/** 当前 waiting 中的实体登记集合。 */
	TMap<TObjectKey<AActor>, FTcsPendingEntityRegistration> PendingEntityRegistrations;

	/** 已被开发者显式纳入 TCS runtime bootstrap 的实体集合。 */
	TSet<TObjectKey<AActor>> RegisteredEntities;

	/** 当前 bootstrap 已感知到的实体级组件运行时跟踪数据。 */
	TMap<TObjectKey<AActor>, FTcsTrackedEntityRuntimeData> TrackedEntityRuntimeData;

#pragma endregion


#pragma region Helpers

protected:
	/**
	 * 判断实体是否实现了 `ITcsEntityInterface`。
	 *
	 * @param Entity 要检查的目标实体
	 * @return 若实现接口则返回 true，否则返回 false
	 */
	bool IsRuntimeEntityValid(AActor* Entity) const;

	/**
	 * 判断实体是否已通过 `RegisterEntity` 显式纳入 bootstrap。
	 *
	 * @param Entity 要检查的目标实体
	 * @return 若实体已显式注册则返回 true，否则返回 false
	 */
	bool IsEntityRegistered(AActor* Entity) const;

	/**
	 * 解析实体上的四个 TCS 组件。
	 *
	 * @param Entity 目标实体
	 * @param OutAttributeComponent 输出 AttributeComponent
	 * @param OutStateComponent 输出 StateComponent
	 * @param OutBuffComponent 输出 BuffComponent
	 * @param OutSkillComponent 输出 SkillComponent
	 */
	void ResolveEntityComponents(
		AActor* Entity,
		UTcsAttributeComponent*& OutAttributeComponent,
		UTcsStateComponent*& OutStateComponent,
		UTcsBuffComponent*& OutBuffComponent,
		UTcsSkillComponent*& OutSkillComponent) const;

	/**
	 * 获取或创建实体的跟踪数据。
	 *
	 * @param Entity 目标实体
	 * @return 跟踪数据引用
	 */
	FTcsTrackedEntityRuntimeData& GetOrAddTrackedEntityRuntimeData(AActor* Entity);

	/**
	 * 清理当前实体的空跟踪记录。
	 *
	 * @param Entity 目标实体
	 */
	void CleanupTrackedEntityRuntimeDataIfEmpty(AActor* Entity);

	/**
	 * 主动推进实体当前可达的 runtime 阶段。
	 *
	 * @param Entity 需要推进的目标实体
	 */
	void TryAdvanceEntityRuntime(AActor* Entity);

	/**
	 * 在组件状态变化后重评估 waiting 实体。
	 *
	 * @param Entity 需要重评估的目标实体
	 */
	void ReevaluateWaitingEntity(AActor* Entity);

	/**
	 * 尝试将 waiting 实体提升为 ready，并在成功时释放 one-shot 回调。
	 *
	 * @param Entity 要尝试提升的目标实体
	 */
	void TryPromoteWaitingEntityToReady(AActor* Entity);

#pragma endregion
};
