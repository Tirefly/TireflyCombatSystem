// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/ScriptInterface.h"

#include "GameplayTagContainer.h"
#include "Flow/TcsDamageFlowDelegate.h"
#include "Parameter/TcsParamValue.h"

#include "TcsStepDamage.generated.h"



/**
 * 伤害链原语（09 §2.4 / D7-2）：链上"发起一次伤害流程"的步骤。
 * 执行 = 构流程上下文 → 把基础值与公式参数写进黑板 → `RunTemplate`（**单帧同步完成，无挂起**）。
 *
 * **流程零计算纪律（PV-7/D7-2）**：`DamageBase` 即**参数账本解算结果**（M5 轮由参数链产出；
 * R3 竖切用 `FTcsParamSource_Literal` 直配测试值）——本步骤 MUST NOT 自行推导基础伤害
 * （"攻击力 × 倍率"一类复合运算由链侧参数链承载，不是流程的活）。
 *
 * 目标：**消费 `Context.Targets`**（不内嵌选择器——D4-4 v2）；目标集由前置 `SelectTargets`
 * 步骤或调用方填充。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsStepDamage
{
	GENERATED_BODY()

// 流程配置
#pragma region Flow

public:
	// 流程模板 id（**空 = 官方默认模板** `Default`）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FGameplayTag FlowTemplateId;

	// 基础伤害值输入（参数账本解算结果；R3 用 Literal 直配）——经上下文请求字段传入流程
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FTcsParamValue DamageBase;

	// 扣血目标属性键（项目词表，如 `Health`）——**必须由请求方指定**：插件组装的官方默认模板
	// 不可能知道项目词表（流程步骤的 `AttrKey` 为空时即用本键）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	FGameplayTag TargetAttrKey;

	// 公式参数初值（只读原料，键 = 项目词表；宿主公式按名取用）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")
	TMap<FGameplayTag, FTcsParamValue> FormulaParams;

	// 注：**委托配置不在链步骤**——它属**流程模板**（D7-5"流程阶段构成 = 项目知识"）：
	// 公式/护盾 hook 配在模板的 `FTcsFlowBaseDamage` / `FTcsFlowExecute` 步骤上。
	// 同一件事只有一处配置（避免双真相与"配了没人用"的死字段）；计划 sketch 曾把 `Delegate`
	// 与 `HealthAttrKey` 列在本 struct——落地收窄为 `TargetAttrKey` 一个请求字段（见 plan2 Task 4 注记）。

#pragma endregion
};
