// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "GameplayTagContainer.h"
#include "Attribute/TcsAttrModInstance.h"
#include "Flow/TcsFlowAttributes.h"
#include "Parameter/TcsParamValue.h"

#include "TcsStepModifyFlow.generated.h"



/**
 * ModifyFlow 链原语（09 §2.3 / D7-6"伤害修改器唯一通道"的**提交侧**）：
 * 效果链上"向流程黑板提交一笔修正"的步骤。
 *
 * **创作糖的落点**：一条伤害修改器 = **触发行 + 单步链 `[ModifyFlow]`**——状态/装备/被动不改属性、
 * 不改流程，而是订阅流程收集事件，命中后起一条单步链向黑板提交修正（"破甲"就是这么来的）。
 *
 * **流程零计算纪律**（同 `FTcsStepDamage`）：本原语 MUST NOT 自行推导基础伤害；MUST NOT 内嵌目标集
 * 或流程模板 id——"打谁"由链上 `SelectTargets` 或上下文默认目标表达，"跑哪条流程"不属本步骤。
 *
 * **只存不裁**：`Consume` 只是随提交携带的消耗策略——裁决与消费（扣次数 / 起冷却 / 发消费事件）
 * 归 `FTcsFlowExecute`（D7-4"收集 ≠ 消费"），本原语不消费任何东西、也不读回任何东西。
 *
 * **跨模块形态**（D4-14）：执行器住 TcsDamage 并经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 自注册进
 * TcsEffect 的执行器注册表——`TcsEffect` 全程不认识任何 TcsDamage 类型；流程上下文经
 * `FTcsEffectContext::EventPayload`（**已就位的中立字段**）抵达。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsStepModifyFlow
{
	GENERATED_BODY()

// 提交配置
#pragma region Submit

public:
	// 目标黑板键（**无效 tag = 落契约键 `Tcs.Flow.Key.BaseDamage`**——兜底在执行器内做：
	// `FGameplayTag` 字段的默认值不能是 tag，"靠字段默认值兜底"会静默落进无效键）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FGameplayTag TargetKey;

	// 运算带（带序唯一真相在 `Op`——本原语不另立带序）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 操作数（PV 载体；`Literal` 可直配，引用型源随其来源策略轮）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FTcsParamValue Operand;

	// 消耗策略（**只存不裁**；裁决与消费归 `FTcsFlowExecute`/台账 DAMAGE-4）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FTcsDamageModifierConsumePolicy Consume;

#pragma endregion
};
