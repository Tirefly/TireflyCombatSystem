// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsTargetSortItem.generated.h"



// 排序方向（值 0 = 内置默认，与全插件枚举规约一致；枚举值前缀 ETSD_ 是 ETcsTargetSortDirection 的缩写）
UENUM(BlueprintType)
enum class ETcsTargetSortDirection : uint8
{
	ETSD_Ascending  = 0	UMETA(DisplayName = "升序", ToolTip = "评分小者排前——距离评分下即「最近优先」"),
	ETSD_Descending = 1	UMETA(DisplayName = "降序", ToolTip = "评分大者排前——距离评分下即「最远优先」"),
};



/**
 * 排序项（P-A 排序相位）：一个「**评分器 + 方向**」组合。
 *
 * `FTcsStepSelectTargets::SortItems` 是它的**有序数组**——按**严格字典序**逐项裁定
 * （第 N 项只在第 1..N-1 项全部相等时才参与），**每一项的方向独立生效**。全部项相等时由
 * **稳定键决胜**：`FTcsCombatEntityHandle::Id` **升序**（见 `SelectTargets` 执行器）。
 *
 * **载体形态 = 裸 `FInstancedStruct`**（与 `Selector` / `Filters` 同款，2026-09-24 换型口径）：
 * 编译期类型限定由手写 `meta = (BaseStruct = ...)` 提供；代价是取用 MUST 走 `GetPtr<T>()` +
 * **判空**（运行期 `IsChildOf` 校验失败返回 nullptr，非野调用）。
 */
USTRUCT()
struct TCSTARGETING_API FTcsTargetSortItem
{
	GENERATED_BODY()

// 评分与方向
#pragma region Scorer

public:
	// 评分器（picker 经 BaseStruct metadata 限定到评分器族；载体为空 / 类型不符时该项按"中性分"降级）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting|Sort",
		meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetScorerStrategy"))
	FInstancedStruct Scorer;

	// 排序方向（每项独立；默认升序 = 值 0）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting|Sort")
	ETcsTargetSortDirection Direction = ETcsTargetSortDirection::ETSD_Ascending;

#pragma endregion
};
