// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsStateChangeNotifyTask.generated.h"

class UTcsStateComponent;

/**
 * 通知TcsStateComponent状态变更的Task实例数据
 */
USTRUCT()
struct FTcsStateChangeNotifyTaskInstanceData
{
	GENERATED_BODY()

	/** Optional reference to TcsStateComponent (will auto-resolve from Owner) */
	UPROPERTY(EditAnywhere, Category = "Parameter", meta = (Optional))
	TObjectPtr<UTcsStateComponent> StateComponent = nullptr;
};

/**
 * 通知TcsStateComponent状态变更的Task
 * 该Task不需要Tick，仅在State进入/退出时通知
 *
 * 工作原理:
 * - 在State激活时调用EnterState()，通知TcsStateComponent状态变更
 * - 在State退出时调用ExitState()，再次通知以检测状态变更
 * - 禁用Tick以实现零开销的通知机制
 * - 如果未配置StateComponent参数，会自动从Owner获取
 */
USTRUCT(meta = (DisplayName = "Tcs State Change Notify Task"))
struct TIREFLYCOMBATSYSTEM_API FTcsStateChangeNotifyTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsStateChangeNotifyTaskInstanceData;

	FTcsStateChangeNotifyTask();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	/**
	 * 当State激活时调用
	 */
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context,
										   const FStateTreeTransitionResult& Transition) const override;

	/**
	 * 当State退出时调用
	 */
	virtual void ExitState(FStateTreeExecutionContext& Context,
						  const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID,
								FStateTreeDataView InstanceDataView,
								const IStateTreeBindingLookup& BindingLookup,
								EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif
};
