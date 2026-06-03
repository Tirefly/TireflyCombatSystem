// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "TcsSTTask_BuffPeriodDriver.generated.h"



class UTcsBuffInstance;
struct FStateTreeLinker;



/**
 * Buff PeriodTick 事件负载。
 */
USTRUCT(BlueprintType)
struct FTcsBuffPeriodTickEventPayload
{
	GENERATED_BODY()

	/** 本次 Tick 使用的实际周期长度。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buff|Period")
	float ResolvedPeriod = 0.f;
};



/**
 * Buff 周期驱动任务实例数据。
 */
USTRUCT()
struct FTcsSTTask_BuffPeriodDriverInstanceData
{
	GENERATED_BODY()

	/** 可选的周期覆盖值；大于 0 时优先使用该值。 */
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float PeriodOverride = 0.f;

	/** 当前累计的已过去时间。 */
	UPROPERTY(Transient)
	float ElapsedTime = 0.f;
};



/**
 * 为 BuffStateTree 提供周期节拍驱动的 Task。
 *
 * 用途：按 Buff 定义上的 Period 或本地 override 值累计时间，到点后发出中性的 PeriodTick 事件。
 */
USTRUCT(meta = (DisplayName = "TcsSTTask_BuffPeriodDriver"))
struct TIREFLYCOMBATSYSTEM_API FTcsSTTask_BuffPeriodDriver : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsSTTask_BuffPeriodDriverInstanceData;

	/** 构造默认 Task 配置。 */
	FTcsSTTask_BuffPeriodDriver();

	/**
	 * 绑定 StateTree 外部数据。
	 *
	 * @param Linker 当前 Linker
	 * @return 绑定成功时返回 true
	 */
	virtual bool Link(FStateTreeLinker& Linker) override;

	/** @return 当前任务使用的实例数据类型。 */
	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	/**
	 * 进入状态时重置本地累计时间。
	 *
	 * @param Context 当前执行上下文
	 * @param Transition 当前状态切换结果
	 * @return 当前 Task 的运行状态
	 */
	virtual EStateTreeRunStatus EnterState(
		FStateTreeExecutionContext& Context,
		const FStateTreeTransitionResult& Transition) const override;

	/**
	 * 每帧推进周期，并在到点时发出 PeriodTick 事件。
	 *
	 * @param Context 当前执行上下文
	 * @param DeltaTime 本帧时间增量
	 * @return 当前 Task 的运行状态
	 */
	virtual EStateTreeRunStatus Tick(
		FStateTreeExecutionContext& Context,
		const float DeltaTime) const override;

#if WITH_EDITOR
	/**
	 * 获取编辑器中显示的节点描述。
	 *
	 * @param ID 当前节点 ID
	 * @param InstanceDataView 当前实例数据视图
	 * @param BindingLookup 当前绑定查询器
	 * @param Formatting 当前节点格式化模式
	 * @return 节点描述文本
	 */
	virtual FText GetDescription(
		const FGuid& ID,
		FStateTreeDataView InstanceDataView,
		const IStateTreeBindingLookup& BindingLookup,
		EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
	/** BuffStateTree 使用的 BuffInstance 外部数据句柄。 */
	TStateTreeExternalDataHandle<UTcsBuffInstance> BuffInstanceHandle;
};