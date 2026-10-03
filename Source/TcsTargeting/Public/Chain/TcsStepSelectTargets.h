// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"

#include "Targeting/TcsTargetFilterStrategy.h"
#include "Targeting/TcsTargetSelectorStrategy.h"
#include "Targeting/TcsTargetSortItem.h"

#include "TcsStepSelectTargets.generated.h"



/**
 * 目标选择链步骤（D4-4 v2 / 04 §2.3）：**选择 → 过滤 → 去重 → 排序 → 取前 K → 写回
 * `Context.Targets`** 的即时步骤。
 *
 * 执行序（见 `Private/Chain/TcsStepSelectTargets.cpp`）：清空 `Context.Targets` → `Selector->Resolve(...)`
 * → 逐候选过 `Filters`（AND + 短路）→ 按句柄身份**去重** → 按 `SortItems` **严格字典序排序**
 * → 取前 `MaxCount` → 写回。下游战斗步骤只消费 `Context.Targets`
 * （步骤不内嵌选择器是 D4-4 v2 的改口：目标集 = 链上显式数据流）。
 *
 * **"保序"是有条件的（2026-10-03 加排序相位时改写）**：`SortItems` **为空**时通过者保持选择器
 * 产出顺序（既有行为，逐位不变）；**配了排序项时一律按字典序重排**——"保持选择器产出的顺序"
 * 在那时不再成立。
 *
 * **去重是执行器的责任**（2026-10-03，补掉的无人防守缺口）：`Resolve` 契约一字未改，
 * 宿主自定义选择器**不被要求**自己去重；重复句柄会让下游步骤对同一目标动手两次。
 * 去重键 = 实体句柄身份，去重发生在**过滤之后、取前 K 之前**（放到取 K 之后会让结果数少于 K）。
 *
 * 边界语义：**未配 `Selector` 时目标集保持原样 + Warning**——"没配选择器"与"选择器选中空集"必须可区分
 * （后者是合法结果，前者是配置缺失）。
 *
 * **载体形态 = 裸 `FInstancedStruct`（2026-09-24 换型，提案 `switch-strategy-carrier-to-plain-instanced-struct`）**：
 * 原为 `TInstancedStruct<T>`，在宿主脚本层（UnrealSharp/C#）**导出为空壳** ⇒ 脚本层**拼不出本步骤**
 * （原计划的三步骤链实证因此不可实现）。换裸形态后字段可读写，类型限定改由 `meta = (BaseStruct = ...)`
 * 承担（picker 只列该族）。**代价**：丢编译期 `enable_if` 限定 ⇒ 取用 MUST 走 `GetPtr<T>()` + 判空
 * （运行期 `IsChildOf` 校验失败返回 nullptr，非野调用）。
 */
USTRUCT()
struct TCSTARGETING_API FTcsStepSelectTargets
{
	GENERATED_BODY()

// 策略配置
#pragma region Strategy

public:
	// 选择器（未配 → 目标集原样 + Warning；picker 经 BaseStruct metadata 限定到选择器族）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting",
		meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetSelectorStrategy"))
	FInstancedStruct Selector;

	// 过滤器（AND 全过 + 短路；**空数组 = 全过**——框架不施加任何隐式默认过滤）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting",
		meta = (BaseStruct = "/Script/TcsTargeting.TcsTargetFilterStrategy"))
	TArray<FInstancedStruct> Filters;

#pragma endregion


// 排序与收束（P-A 排序相位）
#pragma region Sort

public:
	// 排序项（**有序比较项数组**：严格字典序逐项裁定、每项方向独立；**空数组 = 保持既有"保序"行为**）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting|Sort")
	TArray<FTcsTargetSortItem> SortItems;

	// 取前 K（按**完整字典序**取，MUST NOT 理解成"按某一个排序项截断"；0 = 不限，负值按 0 处理）
	UPROPERTY(EditAnywhere, Category = "Tcs|Targeting|Sort", meta = (ClampMin = "0"))
	int32 MaxCount = 0;

#pragma endregion
};
