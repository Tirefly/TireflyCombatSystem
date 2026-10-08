// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Handle/TcsCombatEntityHandle.h"

#include "Skill/TcsCastRunHandle.h"
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
 * **为什么四枚共用一个形状**：订阅者拿到的永远是"哪个技能的哪一次施法动作"——运行身份、所属单位、
 * 定义身份、条目身份与等级在四枚上口径一致；各写一个 struct 会让订阅侧复制四份解析代码。
 * 先例 = 状态侧六枚生命周期事件共用 `FTcsStateEventPayload`。
 *
 * **`Run` 是句柄不是指针**：订阅者若要读运行态细节，在回调内用句柄复核——
 * **"先广播后归还槽位"的纪律**保证那一刻 run 仍在册（同状态侧"先广播后释放槽位"）。
 *
 * **`Run` / `EntryHandle` 的反射性已具备**：两者都是 `USTRUCT(BlueprintType)`
 * （`FTcsCastRunHandle` 于 Task 2 落地、`FTcsSkillEntryHandle` 同批）⇒ 载荷可整块过反射面，
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
	FTcsCastRunHandle Run;

	// 施法单位（本 run 的施法者）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FTcsCombatEntityHandle Unit;

	// 技能定义身份（`FTcsSkillDefData` 的内容身份）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|Skill|Cast Event")
	FGameplayTag DefTag;

	// 账本条目身份（该单位"会这个技能"的那条记录）
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
};
