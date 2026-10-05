// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TcsAttributeComparison.generated.h"



/**
 * 属性值比较方向（属性值域的**比较词汇**，2026-10-05 R5 Task 6 落地）。
 *
 * **归口判据**：比较是**属性值域**的词汇（与 `ETcsAttributeOp` 同域），不是某个消费者的私有词表——
 * 消费者（触发条件 `AttributeCompare`）从本模块取词，MUST NOT 在消费侧另立同义枚举
 * （口径见 `attribute-types` 能力的「属性值比较枚举」需求）。
 *
 * **语义边界**：比较对象是属性的**当前值**（不是基值、不是未提交候选值）；求值行为由消费者实现。
 * 等值语义由"大于等于 且 小于等于"组合表达，故本枚举**不立** `Equal` / `NotEqual` 档（零消费者不预建）。
 */
UENUM(BlueprintType)
enum class ETcsAttributeComparison : uint8
{
	EAC_Greater = 0			UMETA(DisplayName = "大于", ToolTip = "当前值 > 阈值"),
	EAC_Less = 1			UMETA(DisplayName = "小于", ToolTip = "当前值 < 阈值"),
	EAC_GreaterOrEqual = 2	UMETA(DisplayName = "大于等于", ToolTip = "当前值 ≥ 阈值"),
	EAC_LessOrEqual = 3		UMETA(DisplayName = "小于等于", ToolTip = "当前值 ≤ 阈值"),
};
