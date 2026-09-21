// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "Flow/TcsDamageFlowContext.h"
#include "TcsDamageLogChannel.h"

#include "TcsFlowStepConditions.generated.h"



/**
 * 条件：上下文分类 Tag 集须含全部给定 Tag（HasAllTags）。
 * **纯数据谓词**（D4-5 条件最小集的数据面）——不是策略对象，故不用策略基类形态。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsConditionHasAllTags
{
	GENERATED_BODY()

	// 要求的 Tag（全部命中才通过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow")
	TArray<FGameplayTag> Tags;
};

/**
 * 条件：概率通过（Chance）。
 *
 * **确定性纪律的显式例外（D0-1）**：概率型条件天然依赖随机源——故本条件的随机数**由调用方注入**
 * （`ShouldRunFlowStep` 的 `RandomValue` 入参，取值 [0,1)）；宿主未注入随机源时传固定值（如 0.0），
 * 判定即退化为确定性可复现。**MUST NOT 在求值内部直接取随机数**（否则同输入不同输出、回放失效）。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsConditionChance
{
	GENERATED_BODY()

	// 通过概率 [0,1]（`RandomValue < Probability` 即通过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Flow", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Probability = 0.0;
};



/**
 * 步骤条件求值助手（09 §2.2"每步骤可带 Conditions"）。
 *
 * 步骤类型之间**无公共基类**（D4-16）→ 没有基类虚函数可挂条件求值，故用"**同名字段 + 各执行器首行调用本助手**"
 * 的纪律替代（规格 `damage-step-library` 明文）。
 *
 * 语义：条件全过 → true（执行本步）；任一不过 → false（**跳过本步并继续流程**——不是中止）；
 * **未知条件类型 → 视为不过 + Warning 日志**（不静默通过：静默会让"条件写错"表现成"步骤照跑"）。
 *
 * @param Conditions 步骤携带的条件数组（可空 = 无条件）。
 * @param Context 流程上下文（条件按此求值）。
 * @param RandomValue [0,1) 随机值（仅 Chance 用；未注入随机源时传固定值以保证可复现）。
 * @return 返回是否应当执行本步骤。
 */
inline bool ShouldRunFlowStep(
	const TArray<FInstancedStruct>& Conditions,
	const FTcsDamageFlowContext& Context,
	double RandomValue = 0.0)
{
	for (const FInstancedStruct& ConditionStruct : Conditions)
	{
		if (!ConditionStruct.IsValid())
		{
			continue;
		}

		if (const FTcsConditionHasAllTags* HasAllTags = ConditionStruct.GetPtr<FTcsConditionHasAllTags>())
		{
			// 上下文须含**全部**给定 Tag（逐个核对——空数组视为无条件通过）
			for (const FGameplayTag& RequiredTag : HasAllTags->Tags)
			{
				if (!Context.ClassificationTags.Contains(RequiredTag))
				{
					return false;
				}
			}
			continue;
		}

		if (const FTcsConditionChance* Chance = ConditionStruct.GetPtr<FTcsConditionChance>())
		{
			if (RandomValue >= Chance->Probability)
			{
				return false;
			}
			continue;
		}

		// 未知名条件：不静默通过（记日志、按不过处理）
		UE_LOG(LogTcsDamage, Warning, TEXT("ShouldRunFlowStep: 条件类型 %s 无求值器——按不通过处理（跳过该步）"),
			*GetNameSafe(ConditionStruct.GetScriptStruct()));
		return false;
	}

	return true;
}
