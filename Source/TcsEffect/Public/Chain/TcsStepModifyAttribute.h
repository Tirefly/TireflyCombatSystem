// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Parameter/TcsParamValue.h"

#include "TcsStepModifyAttribute.generated.h"



/**
 * 属性修改步骤（D4-16 十五原语之一，**属 TcsEffect**；04 §2.1）。
 *
 * **做什么**：把一条**常驻修正器条目**（`FTcsAttrModInstance`）按本步骤的运算带与运算数写进目标单位的
 * 属性账本——`Operand` 经统一数值载体求值后作**字面量**挂在条目上；条目的 `Source` = **本次运行的
 * 来源锚点**（`FTcsEffectContext::RunSource`），故"同一次运行挂上的条目"可按该锚点回收。
 *
 * **MUST NOT 新造词表**：运算带复用 M2 的 `ETcsAttributeOp`、运算数复用 `FTcsParamValue`
 * （全插件统一数值配置载体）——链侧只负责"何时挂"，"怎么折叠"归属性管线。
 *
 * **语义**：**即时步骤**（恒 `TSR_Completed`，不挂起）。要"临时"改数值那是状态层的活（施加态 +
 * 到期/移除时摘除）；本原语表达的是"链直接改一次账本"（D7-6：属性只有修正器一条写入口）。
 *
 * **失败面（都不 ensure、都不断链）**：步骤载荷类型不符 / `Attribute` 无效 / 目标无效且黑板目标集为空
 * ⇒ `Warning` + 按完成处理；单位无属性账本 ⇒ `Log` + 按完成处理（配置状态，零红字）。
 * `ETcsStepResult` 只有 `Completed` / `Running`，**步骤无法中断链**——软失败接管归 `OnError`（R5.5-a）。
 *
 * **依赖边界**：本步是 `TcsEffect` 对**下层**领域模块 `TcsAttribute` 的首次真实使用——允许依赖下层
 * （`TcsAttribute` / `TcsCore` / `TcsNotation`），**MUST NOT** 依赖上层（`TcsDamage` / `TcsTargeting` /
 * `TcsState` / `TcsSkill`）。属性访问一律经 `Private/Attribute/TcsEffectAttributeAccess.h`
 * （本模块唯一的属性门面取用处）。
 *
 * 执行器住 `Private/Chain/TcsStepModifyAttribute.cpp`（+ 一行宏自注册）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepModifyAttribute
{
	GENERATED_BODY()

// 属性修改
#pragma region Attribute

public:
	// 目标实体（无效则取黑板 `Targets[0]`，仍无效 ⇒ 软失败）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|ModifyAttribute")
	FTcsCombatEntityHandle Target;

	// 被修改的属性键（宿主声明的属性词；无效 ⇒ 软失败）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|ModifyAttribute")
	FGameplayTag Attribute;

	// 运算带（复用 M2 枚举——不新造词表）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|ModifyAttribute")
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 运算数（统一数值载体：字面量 / 参数源；链侧无参数表 ⇒ 引用类源落兜底）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|ModifyAttribute")
	FTcsParamValue Operand;

#pragma endregion
};
