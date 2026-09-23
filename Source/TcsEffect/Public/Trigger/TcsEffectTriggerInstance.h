// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Handle/TcsInstanceHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "Trigger/TcsEffectTrigger.h"

#include "TcsEffectTriggerInstance.generated.h"



// 触发实例句柄的类型区分标签（防与链运行态句柄等互换——`TTcsInstanceHandle` 的 TTagType 用法）
struct FTcsEffectTriggerTag {};



/**
 * 触发实例句柄（登记表内定位；见 `TcsEffectTriggerInstance` 的持有形态说明）。
 * 句柄**无池上下文**（登记表是 `TArray` 非池），故只判"是否曾被赋值"——代际校验不适用。
 */
USTRUCT()
struct TCSEFFECT_API FTcsEffectTriggerHandle
{
	GENERATED_BODY()

	// 登记表内下标句柄
	TTcsInstanceHandle<FTcsEffectTriggerTag> Inner;

	// 句柄有效性
	bool IsValid() const
	{
		return Inner.IsValid();
	}
};



/**
 * 触发实例（**运行期形态**）——定义 + 运行期簿记。
 *
 * **分层（2026-09-23 用户拍板）**：定义 `FTcsEffectTriggerDef` 是纯配置（可进资产/可内联），
 * 本类型在它之上加**运行期簿记**。两者分开的核心理由：`Source` 是运行期发号的句柄
 * （跨会话/跨机不同、MUST NOT 进内容资产），与"能存进资产的配置"是两类字段。
 *
 * **`Source` 的语义（它取决于行"怎么被登记"）**：
 * | 登记时机 | `Source` = | 典型场景 |
 * |---|---|---|
 * | 定义加载期（全局常驻） | 系统/DefLibrary 来源句柄 | 系统级规则（"任何单位死亡时…"） |
 * | **施加状态时**（per-instance） | **该状态实例句柄** | **Buff 行为**——"状态在，行为就在" |
 *
 * 第二行是 `09 §2.3` 原文"订阅随 Source 生命周期自动退订——'状态在，修改器就在'天然成立"
 * 的机制落点：BuffDef 内联的触发行在状态施加时注册（Source = 状态实例句柄）、
 * 状态移除时按 Source 级联退订。
 *
 * **持有形态（`TArray<FTcsEffectTriggerInstance>` 值语义，非池）**：触发行无高频增删、
 * 无挂起语义，池的代际校验在此是零收益的复杂度。**代价**：`TArray` 扩容会搬移元素地址，
 * 故**MUST NOT 跨帧持有实例指针**——求值器每次从登记表按下标重解析
 * （与链解释器"每步入器前重解析"同款纪律）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsEffectTriggerInstance
{
	GENERATED_BODY()

// 定义
#pragma region Definition

public:
	// 定义（纯配置——来自资产 / 内联 / 运行期登记）
	UPROPERTY()
	FTcsEffectTriggerDef Def;

#pragma endregion


// 运行期簿记
#pragma region Runtime

public:
	/**
	 * 来源（**级联退订锚点**）：D4-1 明文"触发行注册于定义加载期，运行句柄与订阅配对；
	 * **来源注销自动退订**"——本字段即该机制的锚点，与 M2 `RemoveBySource` 同款语义。
	 *
	 * **非 `UPROPERTY`（2026-09-23 编译实证）**：`FTcsSourceHandle` 是非反射纯 C++ struct
	 * （`TcsCore/Handle/TcsSourceHandle.h:21`），UHT 报 `Unable to find 'class', ... with name
	 * 'FTcsSourceHandle'`。同款先例 = `FTcsAttrModInstance.Source`。
	 * **不序列化是正确语义**：句柄是运行期发号的，本就不该进内容资产。
	 */
	FTcsSourceHandle Source;

	// 自身句柄（退订 / 点灯 / 求值器回填上下文时用）
	UPROPERTY()
	FTcsEffectTriggerHandle Self;

#pragma endregion
};
