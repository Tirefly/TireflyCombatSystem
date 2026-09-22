// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ScriptInterface.h"

#include "Flow/TcsDamageFlowDelegate.h"

#include "TcsFlowSteps.generated.h"



/**
 * 标准步骤库（09 §2.2 的十阶段；D7-5"流程 = 数据模板 + 标准件"）。
 *
 * 共性契约（规格 `damage-step-library`）：
 * - 每个步骤 struct **持同名字段** `TArray<FInstancedStruct> Conditions`（纯数据谓词，见
 *   `TcsFlowStepConditions.h`）——步骤类型**无公共基类**（D4-16），故靠"同名字段 + 各执行器首行调
 *   `ShouldRunFlowStep`"的纪律而不是基类虚函数；
 * - 全部经 `UE_DEFINE_FLOW_STEP_EXECUTOR` 自注册（执行器住 `Private/Flow/Steps/`）；
 * - 步骤只读写**流程黑板 / 分类 Tag / 上下文**，不认识伤害公式（零公式纪律：基础值只从输入来）。
 *
 * 说明：十步 struct 同住本头（都是小数据 struct 且共享同一 Conditions 契约；执行器按"核心四步 / 其余"
 * 两文件拆分）——计划 sketch 写"各 .h"，此为落地收窄（见 plan2 Task 4 注记）。
 */

// ① 流程开始：发流程开始事件 + 重置收集
USTRUCT()
struct TCSDAMAGE_API FTcsFlowCollectStart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ② 命中前：发收集事件 → 应用（宿主挂点：改命中率 / 注册免疫候选）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowPreHit
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ③ 命中判定：基础命中率（delegate）→ 修改器 → 写黑板 `Hit`
USTRUCT()
struct TCSDAMAGE_API FTcsFlowHit
{
	GENERATED_BODY()

	// 基础命中率来源（宿主 delegate；可空 = 必中 1.0）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ④ 暴击判定：基础暴击率（delegate）→ 修改器 → 写黑板 `Crit`
USTRUCT()
struct TCSDAMAGE_API FTcsFlowCrit
{
	GENERATED_BODY()

	// 基础暴击率来源（宿主 delegate；可空 = 不暴击 0.0）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑤ 元素解析：delegate 解析元素 → 写入分类 Tag 集
USTRUCT()
struct TCSDAMAGE_API FTcsFlowElement
{
	GENERATED_BODY()

	// 元素解析来源（宿主 delegate；可空 = 不写元素 Tag）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑥ 基础伤害：**接收输入值**（`Context.BaseDamageInput`——链步骤解算结果）→ 修改器可改（流程零计算）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowBaseDamage
{
	GENERATED_BODY()

	// 输出键（输入值以 **Add** 提交到该键；收集到的 PercentAdd/Mul 修正自然叠在其上）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag OutputKey;

	// delegate 降级逃生口（宿主特殊公式；未配置则原样使用输入值）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑦ 伤害后：发收集事件 → 应用（宿主挂点："伤害 +50"）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowAfterDamage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑧ 执行前：发收集事件 → **只收集**免疫/减伤候选（收集 ≠ 消费，D7-4）
USTRUCT()
struct TCSDAMAGE_API FTcsFlowPreExecute
{
	GENERATED_BODY()

	// 候选落点键（默认 `ExecuteCandidates`——⑨ 裁决步骤按此读回）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag CandidateKey;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑨ 执行：免疫/减伤裁决（SortKey 选一）→ 护盾 hook → M2 事务扣血 → 成功才消费
USTRUCT()
struct TCSDAMAGE_API FTcsFlowExecute
{
	GENERATED_BODY()

	// 扣血目标属性键（项目词表，如 `Health`）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag AttrKey;

	// 待执行伤害量键（默认 `BaseDamage`——⑥ 以 **Add** 提交输入值，收集到的 PercentAdd/Mul 自然叠在其上）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag DamageKey;

	// 免疫/减伤候选键（默认 `ExecuteCandidates`——与 ⑧ 配对）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	FGameplayTag CandidateKey;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TScriptInterface<ITcsDamageFlowDelegate> Delegate;

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};

// ⑩ 完成：发完成事件 + 填充并发布伤害记录
USTRUCT()
struct TCSDAMAGE_API FTcsFlowCompleted
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FInstancedStruct> Conditions;
};
