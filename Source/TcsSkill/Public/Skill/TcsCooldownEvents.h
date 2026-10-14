// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Clock/TcsExpiryHeap.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "Skill/TcsSkillEntryHandle.h"

#include "TcsCooldownEvents.generated.h"



// 冷却**已开始**事件 Tag（产生者 = `FTcsCooldownOps::StartCooldownOnEntry`——自动档与 Manual 档
// **同一条路径**，故两档都发它）。
//
// 命名公约：`TcsEvent.<域>.<事件名>`——域单数、事件名 PascalCase。
// **落地名去掉 `On` 前缀**（`D5-16` 原文写的是 `OnCooldownStarted`）：`TcsEvent.Cast` 根下既有的
// 四枚是 `Started` / `PhaseChanged` / `Completed` / `Interrupted`（**零 `On` 前缀**）⇒ 按**同族对称**
// 取本名。**MUST NOT 照抄 `On*` 进 tag 名**（那会让同一根下两种命名风格并存）。
// 新增 "Cooldown" 段把三枚冷却事件与四枚施法事件区分开。
//
// **根数不变**：落既有 `TcsEvent` 根（域段 = `Cast`），`gameplay-tag-governance` 的 12 个根**不增**
// （"增根不增层"）。
//
// **导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为裸 `extern`（无 `__declspec(dllexport)`）
// ⇒ 宿主/其它模块引用本变量会 `LNK2001`。框架事件 Tag 的设计意图正是"供宿主订阅" ⇒ MUST 带模块导出宏。
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_CooldownStarted;

// 冷却**被改变**事件 Tag（产生者 = `AdjustCooldown` / `ResetCooldown`）
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_CooldownUpdated;

// 冷却**已结束**事件 Tag（产生者 = 到期堆回调——**权威侧**：到期是时轴上的事实，
// 不是"某个调用者说结束了"）
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_CooldownEnded;



/**
 * 冷却事件载荷（**三枚共用一种形状**，沿 `FTcsCastEventPayload` 的先例）。
 *
 * **为什么三枚共用一个形状**：订阅者拿到的永远是"哪个实体的哪条技能的哪条轨道"——
 * 身份与读数在三枚上口径一致；各写一个 struct 会让订阅侧复制三份解析代码。
 *
 * **`D5-16` 的 `SkillId` 字段用 `DefTag` 取代（MUST 记录）**：`D5-16` 原文的 payload 清单里有
 * `SkillId`，而**今天没有这个类型**——`D5-9` 修订后 `EffectiveDefId` 已整体移除，
 * "这个条目用哪份定义"的唯一答案就是 `DefTag`（`TcsSkillRegistry.h` 明文）。
 * **MUST NOT 为它发明一个类型**：那是"拟定未建"类名字的正确处置 = **用现有的正确载体替代**。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCooldownEventPayload
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 账本条目身份（该实体"会这个技能"的那条记录）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	FTcsSkillEntryHandle EntryHandle;

	// 所属实体（冷却的作用域——**组轨的共享槽住实体级**，故它是身份的一部分）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	FTcsCombatEntityHandle Entity;

	// 技能定义身份（`FTcsSkillDefData` 的内容身份；取代 `D5-16` 里那个不存在的 `SkillId`）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	FGameplayTag DefTag;

#pragma endregion


// 轨道与读数
#pragma region Track

public:
	// 轨道下标（对应 `FTcsCooldownPolicy::Tracks` 的序；**组轨事件带的是触发它的那条轨的下标**）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	int32 TrackIndex = -1;

	// 组标签（自身轨 = 无效 tag；**有效即"这条冷却属于某个公共冷却组"**）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	FGameplayTag GroupTag;

	// 本次冷却的展示总时长（秒；CDR 已作用——起冷时刻冻结的那个值）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	double Total = 0.0;

	// 事件发生时刻的**剩余**秒数（`CooldownEnded` 时为 0）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	double Remaining = 0.0;

	// 到期绝对时刻（`FTcsClock::Elapsed` 时轴——与全部到期锚点同一根时轴）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cooldown Event")
	double EndTimeSeconds = 0.0;

#pragma endregion
};
