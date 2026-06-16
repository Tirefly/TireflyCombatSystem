// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "TcsSTEvaluator_BuffPeriod.generated.h"


class UTcsBuffInstance;



// BuffPeriod Evaluator 实例数据
USTRUCT()
struct FTcsSTEvaluator_BuffPeriodInstanceData
{
	GENERATED_BODY()

	// 周期到点信号（绑定到 Transition 条件使用）
	UPROPERTY(EditAnywhere, Category = "Output")
	bool bIsPeriodBoundary = false;

	// 累计已过去时间
	UPROPERTY(Transient)
	float ElapsedTime = 0.f;
};


/**
 * 按 Buff 定义的 Period 输出周期到点信号。
 *
 * 每 Tick 累计时间，到点后将 bIsPeriodBoundary 置为 true（仅一帧）。
 * 开发者将 Transition 绑定到 bIsPeriodBoundary 即可驱动周期性动作（如 ApplyAttributeModifier）。
 *
 * 作为 Global Evaluator 使用，对同一 StateTree 内所有状态可见。
 */
USTRUCT(meta = (DisplayName = "TcsSTEvaluator_BuffPeriod"))
struct TIREFLYCOMBATSYSTEM_API FTcsSTEvaluator_BuffPeriod : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsSTEvaluator_BuffPeriodInstanceData;

	FTcsSTEvaluator_BuffPeriod();

	virtual const UStruct* GetInstanceDataType() const override
	{
		return FInstanceDataType::StaticStruct();
	}

	virtual bool Link(FStateTreeLinker& Linker) override;

	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 可选周期覆盖值（>0 时优先使用，忽略 Buff 定义上的 Period）
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float PeriodOverride = 0.f;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID,
	                             FStateTreeDataView InstanceDataView,
	                             const IStateTreeBindingLookup& BindingLookup,
	                             EStateTreeNodeFormatting Formatting = EStateTreeNodeFormatting::Text) const override;
#endif

protected:
	TStateTreeExternalDataHandle<UTcsBuffInstance> BuffInstanceHandle;
};
