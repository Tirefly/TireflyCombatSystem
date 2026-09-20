// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "Targeting/TcsTargetFilterStrategy.h"
#include "Targeting/TcsTargetSelectorStrategy.h"

#include "TcsStepSelectTargets.generated.h"



/**
 * 目标选择链步骤（D4-4 v2 / 04 §2.3）：**选择 → 过滤 → 写回 `Context.Targets`** 的即时步骤。
 *
 * 执行序（见 `Private/Chain/TcsStepSelectTargets.cpp`）：清空 `Context.Targets` → `Selector->Resolve(...)`
 * → 逐候选过 `Filters`（AND + 短路，保序）→ 通过者写回。下游战斗步骤只消费 `Context.Targets`
 * （步骤不内嵌选择器是 D4-4 v2 的改口：目标集 = 链上显式数据流）。
 *
 * 边界语义：**未配 `Selector` 时目标集保持原样 + Warning**——"没配选择器"与"选择器选中空集"必须可区分
 * （后者是合法结果，前者是配置缺失）。
 */
USTRUCT()
struct TCSTARGETING_API FTcsStepSelectTargets
{
	GENERATED_BODY()

// 策略配置
#pragma region Strategy

public:
	// 选择器（未配 → 目标集原样 + Warning；编辑器类型 picker 只列 FTcsTargetSelectorStrategy 的 C++ 子类）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting")
	TInstancedStruct<FTcsTargetSelectorStrategy> Selector;

	// 过滤器（AND 全过 + 短路；**空数组 = 全过**——框架不施加任何隐式默认过滤）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting")
	TArray<TInstancedStruct<FTcsTargetFilterStrategy>> Filters;

#pragma endregion
};
