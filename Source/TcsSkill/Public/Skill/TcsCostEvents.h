// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Skill/TcsSkillEntryHandle.h"

#include "TcsCostEvents.generated.h"



// 消耗**已提交**事件 Tag（产生者 = 策略 `CommitCost` 的成功路径；内建 = `FTcsSkillCostPolicy_ResourceAttr`）。
//
// **`CanAfford` 失败 MUST NOT 发本事件**（`D5-16` 明文："CanAfford 失败不发——`TryActivate` 返回值覆盖"）：
// 门禁的拒绝面已经有 `SAR_CannotAfford` 这个具名读数，再发一枚"没扣成"的事件纯属噪音
// （订阅者还得自己判"这枚事件是真的扣了还是没扣"）。
//
// **落地名去掉 `On` 前缀**（`D5-16` 原文写 `OnCostApplied`）：理由同冷却三枚——
// `TcsEvent.Cast` 根下既有四枚**零 `On` 前缀**，按同族对称取名。
//
// **导出宏**：同冷却三枚（裸 `extern` 会 `LNK2001`）。
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_CostApplied;



/**
 * 消耗事件载荷（`D5-16` 原文清单 + 身份三件套）。
 *
 * **为什么带 `DefTag` 而不带 `D5-16` 原文的 `SkillId`**：同 `FTcsCooldownEventPayload` 的理由
 * （`EffectiveDefId` 已整体移除，"用哪份定义"的唯一答案是 `DefTag`；MUST NOT 为它发明类型）。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCostEventPayload
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 账本条目身份（该单位"会这个技能"的那条记录）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cost Event")
	FTcsSkillEntryHandle EntryHandle;

	// 所属单位（被扣资源的那一方）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cost Event")
	FTcsCombatEntityHandle Unit;

	// 技能定义身份
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cost Event")
	FGameplayTag DefTag;

#pragma endregion


// 消耗内容
#pragma region Cost

public:
	// 被扣的属性（**内建属性资源策略**才有；宿主自定义策略扣什么框架不认识 ⇒ 留无效）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cost Event")
	FGameplayTag ResourceAttr;

	// 实际扣减的数量（> 0；零动作档不发本事件）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cost Event")
	double Amount = 0.0;

#pragma endregion
};
