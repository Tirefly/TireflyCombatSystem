#pragma once

#include "CoreMinimal.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeNodeBase.h"
#include "StateTreeLinker.h"
#include "StateTreeExecutionTypes.h"
#include "GameplayTagContainer.h"
#include "TcsStateSlotDebugEvaluator.generated.h"

class UTcsStateComponent;

/**
 * 调试槽位信息的Evaluator实例数据
 */
USTRUCT()
struct FTcsStateSlotDebugEvaluatorInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Output")
	FString Snapshot;

	UPROPERTY()
	float TimeSinceLastUpdate = 0.0f;
};

/**
 * 将 UTcsStateComponent 槽位状态输出为字符串，便于在StateTree调试器中查看
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTcsStateSlotDebugEvaluator : public FStateTreeEvaluatorCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTcsStateSlotDebugEvaluatorInstanceData;

	FTcsStateSlotDebugEvaluator();

	virtual bool Link(FStateTreeLinker& Linker) override;
	virtual void Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;

	// 刷新间隔
	UPROPERTY(EditAnywhere, Category = "Config", meta = (ClampMin = "0.0"))
	float UpdateInterval;

	// 可选：只输出指定槽位
	UPROPERTY(EditAnywhere, Category = "Config")
	FGameplayTag SlotFilter;

protected:
	TStateTreeExternalDataHandle<UTcsStateComponent> StateComponentHandle;
};
