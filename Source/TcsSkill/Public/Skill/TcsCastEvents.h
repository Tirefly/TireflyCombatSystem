// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Skill/TcsCastFinishReason.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Skill/TcsSkillInstanceHandle.h"
#include "Skill/TcsSkillEntryHandle.h"

#include "TcsCastEvents.generated.h"



// 施法**开始**事件 Tag（激活成功后广播——本 Task 的产生者之一）
// 命名公约：`TcsEvent.<域>.<事件名>`——域单数、事件名 PascalCase 动词短语；
// 属"框架协议"而非宿主词汇（事件广播面的契约词若由宿主配置，宿主漏配即静默破坏广播面），
// 故由事件所属模块**原生声明**。本族**落既有的 `TcsEvent` 根**（域段 = `Cast`），
// MUST NOT 为此新增根（`gameplay-tag-governance` 的"一角色一根"与"增根不增层"）。
// 总线现状提示：**原生订阅为精确匹配**（非层级）⇒ 订阅方 MUST 逐叶子具名，不靠父 tag 收全。
//
// **导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**
// （`NativeGameplayTags.h`，无 `__declspec(dllexport)`）⇒ 宿主/其它模块引用本变量会**链接失败**
// （`LNK2001`）。框架事件 Tag 的设计意图正是"供宿主订阅"，故本模块的 Tag 声明 MUST 带模块导出宏。
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_Started;

// 施法**时段变化**事件 Tag（**产生者归 Task 5**——时段推进；本 Task 只落词与载荷）
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_PhaseChanged;

// 施法**完成**事件 Tag（**产生者归 Task 5**——全时段走完的自然终结；本 Task 只落词与载荷）
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_Completed;

// 施法**被打断**事件 Tag（顶替路径在本 Task 即产生；打断的完整结算归 Task 5）
extern TCSSKILL_API FNativeGameplayTag Tag_TcsEvent_Cast_Interrupted;



/**
 * 施法生命周期事件载荷（四枚共用一种形状）。
 *
 * **为什么四枚共用一个形状**：订阅者拿到的永远是"哪个技能的哪一次施法动作"——运行身份、所属实体、
 * 定义身份、条目身份与等级在四枚上口径一致；各写一个 struct 会让订阅侧复制四份解析代码。
 * 先例 = 状态侧六枚生命周期事件共用 `FTcsBuffEventPayload`。
 *
 * **`Run` 是句柄不是指针**：订阅者若要读运行态细节，在回调内用句柄复核——
 * **"先广播后归还槽位"的纪律**保证那一刻 run 仍在册（同状态侧"先广播后释放槽位"）。
 *
 * **`Run` / `EntryHandle` 的反射性已具备**：两者都是 `USTRUCT(BlueprintType)`
 * （`FTcsSkillInstanceHandle` 于 Task 2 落地、`FTcsSkillEntryHandle` 同批）⇒ 载荷可整块过反射面，
 * **不需要** SCRIPT-8 那种"传句柄 + 门面访问器"的展平绕行。
 */
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCastEventPayload
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 施法运行态句柄（回调内可据此复核运行态）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FTcsSkillInstanceHandle Run;

	// 施法实体（本 run 的施法者）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FTcsCombatEntityHandle Entity;

	// 技能定义身份（`FTcsSkillDefData` 的内容身份）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FGameplayTag DefTag;

	// 账本条目身份（该实体"会这个技能"的那条记录）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FTcsSkillEntryHandle EntryHandle;

#pragma endregion


// 数值
#pragma region Value

public:
	// 本次施法运行态冻结的生效等级
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	int32 Level = 0;

#pragma endregion


// 终结原因
#pragma region Reason

public:
	/**
	 * **本次终结的原因**（2026-10-16 `R5.5-e1` 新增）。
	 *
	 * **为什么载荷必须有它**：`Interrupted` 与 `Replaced` **发同一枚事件**
	 * （`TcsEvent.Cast.Interrupted`），而 `ECastFinishReason` 自己的注释写明其存在理由是
	 * 「两者对订阅者语义不同」（一个是外部打断、一个是同类技能抢位）。**没有本字段时**，
	 * 订阅者只能靠「哪枚事件」反推 ⇒ 那个区分**只在枚举注释里成立、在载荷面上不成立**。
	 *
	 * **四枚共用同一形状 ⇒ 本字段恒有值**：`Started` / `PhaseChanged` 两枚按「尚未终结」
	 * 填各自档位（`ECFR_Completed` 是中性默认）。MUST NOT 让它变成「只有终结事件才填」的
	 * 第二真相——那会逼订阅方记住「每枚事件各自哪些字段有效」。
	 */
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	ECastFinishReason Reason = ECastFinishReason::ECFR_Completed;

#pragma endregion
};
