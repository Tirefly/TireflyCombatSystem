// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsStepBranch.generated.h"



/**
 * 分支步骤（控制流原语）：条件**全过**走 `ThenChainId`、否则走 `ElseChainId`。执行器住
 * `Private/Chain/TcsStepBranch.cpp`（+ 一行宏自注册）。
 *
 * **条件复用触发行的条件注册表**（`EvaluateTriggerConditions`）——一套条件类型两处可用、零重复实现；
 * 语义同触发行：全过 → 通过；任一不过 → 短路（后续条件不再求值）；**未注册的条件类型 → 视为不过 +
 * Warning**（不静默通过）。求值随机值取自门面的种子流（与触发行同一个可复现口径）。
 *
 * **链侧条件上下文的来源限制（本批明示接受的限制，已登记台账）**：链侧只有 `Caster` 有来源；
 * `FTcsTriggerContext::EventTag` 与 `ClassificationTags` **在链侧无来源**，以无效 tag / 空数组构造
 * ⇒ **依赖这两者的条件在链侧恒不通过**（如 `HasAllTags`）。自然补法 = 起链时把触发事件 tag 记进
 * 运行态；该扩展不在本批。
 *
 * **与 `FTcsStepRunSubChain` 的关系**：`bWait` 语义与子链完成唤醒**完全一致**，且**共用同一套实现**
 * （`UTcsEffectSubsystem::RunChildChainStep`）——规格明文 MUST NOT 各写一套唤醒逻辑。故 `bWait`、
 * 黑板快照（不回传）与"子链不额外获得预算"三条纪律见 `TcsStepRunSubChain.h`，本文件不重复。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepBranch
{
	GENERATED_BODY()

// 分支
#pragma region Branch

public:
	// 条件数组（**全部**通过才走 Then；空数组 = 无条件通过 → 恒走 Then）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Branch")
	TArray<FInstancedStruct> Conditions;

	// 条件全过时起的链（**空 tag = 该支不执行**——属正常配置，不起链、不记 Warning，本步仍算完成）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Branch")
	FGameplayTag ThenChainId;

	// 条件有不过时起的链（空 tag 口径同上）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Branch")
	FGameplayTag ElseChainId;

	// 是否等子链走完（语义与 `FTcsStepRunSubChain::bWait` 一字不差）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Branch")
	bool bWait = true;

#pragma endregion
};
