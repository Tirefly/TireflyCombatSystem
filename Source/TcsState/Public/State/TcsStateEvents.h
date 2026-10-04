// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Handle/TcsCombatEntityHandle.h"
#include "Handle/TcsSourceHandle.h"

#include "State/TcsStateEnums.h"
#include "State/TcsStateHandle.h"

#include "TcsStateEvents.generated.h"



// 状态**施加**事件 Tag（原生 Tag——订阅方按此过滤；跨模块可直接引用本声明）
// 命名公约（2026-10-01 换根后）：`TcsEvent.<域>.<事件名>`——域单数、事件名 PascalCase 动词短语；
// 属"框架协议"而非宿主词汇（事件广播面的契约词若由宿主配置，宿主漏配即静默破坏广播面），
// 故由事件所属模块**原生声明**。同域兄弟事件挂在本标签的父节点 `TcsEvent.State` 下。
// 总线现状提示：**原生订阅为精确匹配**（父标签订阅需等原生层级匹配落地）；BP/CS 动态层已支持部分匹配。
//
// **导出宏（2026-09-21 跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无
// `__declspec(dllexport)`，`NativeGameplayTags.h:31`）——宿主/其他模块引用本变量会**链接失败**
// （实测 `LNK2001`）。框架事件 Tag 的设计意图正是"供宿主订阅"，故本模块的 Tag 声明 MUST 带模块导出宏
// （同模块内的定义 TU 见到 dllexport 声明即导出符号，定义处零改动）。
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_Applied;

// 状态**刷新**事件 Tag（重复施加命中同组成活实例——本轮判据 = 同单位 + 同 DefTag）
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_Refreshed;

// 状态**层数变化**事件 Tag（组内层数 +1 / 替换；同组判据归 R5 Task 5）
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_StackChanged;

// 状态**到期**事件 Tag（时值走完的自然结束——载荷 `Cause` = `ESRC_Expired`）
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_Expired;

// 状态**移除**事件 Tag（显式驱散 / 取消 / 单位注销批量移除——载荷 `Cause` 由调用方给定）
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_Removed;

// 状态**周期到点**事件 Tag（载荷带当前层数与等级；**产生者归 R5 Task 3**——本轮只声明词与载荷）
extern TCSSTATE_API FNativeGameplayTag Tag_TcsEvent_State_Periodic;



/**
 * 状态生命周期事件载荷（D3-7 生命周期事件全集）：六枚事件共用一种载荷形状。
 *
 * **为什么六枚共用一个形状**：订阅者拿到的永远是"哪个状态的哪一次生命周期动作"——
 * 六个字段（实例身份 + 定义身份 + 来源 + 发起者 + 层数 + 等级）在六枚上口径一致，
 * 各写一个 struct 会让订阅侧复制六份解析代码。差异只有 `Cause`：它**仅 `Expired` / `Removed`
 * 有意义**（其余四枚里无意义，读它属调用方责任）。
 *
 * **`Handle` 是句柄不是指针**：订阅者若要读实例细节，在回调内用句柄调
 * `UTcsStateSubsystem::GetState`——"先广播后释放槽位"这条纪律保证那一刻实例仍在册。
 *
 * **`Source` 为何不是 UPROPERTY**：`FTcsSourceHandle` 是**非反射**纯 C++ struct
 * （`Handle/TcsSourceHandle.h`，无 `USTRUCT` 宏——"纯内联类型不得带导出宏"那条纪律的另一面）。
 * 需要脚本层可达时须先把它反射化（属台账 SCRIPT 系列的连带项，本轮不做）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsStateEventPayload
{
	GENERATED_BODY()

// 身份
#pragma region Identity

public:
	// 实例句柄（回调内可据此调 `GetState` 读实例）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	FTcsStateHandle Handle;

	// 定义身份（状态 Def 的 `DefTag`）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	FGameplayTag DefTag;

	// 归属来源句柄（级联撤销锚点——触发行退订与属性修正器摘除按它清）
	FTcsSourceHandle Source;

	// 发起者实体（可与被施加方不同）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	FTcsCombatEntityHandle Instigator;

#pragma endregion


// 数值
#pragma region Value

public:
	// 广播时的层数
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	int32 Stacks = 1;

	// 广播时的生效等级
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	int32 Level = 0;

	// 移除原因（**仅 `Expired` / `Removed` 有意义**——其余四枚读它无意义）
	UPROPERTY(BlueprintReadOnly, Category = "Tcs|State|Event")
	EStateRemoveCause Cause = EStateRemoveCause::ESRC_Removed;

#pragma endregion
};
