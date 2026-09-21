// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Attribute/TcsAttributeName.h"
#include "Handle/TcsCombatEntityHandle.h"

#include "TcsAttributeChangedEvent.generated.h"



// 属性**当前值**变更事件 Tag（原生 Tag——订阅方按此过滤；跨模块可直接引用本声明）
// 命名公约（2026-09-18 用户拍板）：`Tcs.Event.<域>.<事件名>`——域单数、事件名 PascalCase 动词短语；
// 属"框架协议"而非游戏词汇，由事件所属模块原生声明（TcsCore 不持战斗域词汇，见 01 §M0-min 的 Core 边界）。
// 同域后续事件（属性上线/下线等）挂在本标签的父节点 `Tcs.Event.Attribute` 下。
// 总线现状提示：**原生订阅为精确匹配**——父标签订阅需等原生层级匹配落地（已列总线开放项）；BP/CS 动态层已支持部分匹配。
//
// **导出宏（2026-09-21 跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无
// `__declspec(dllexport)`，`NativeGameplayTags.h:31`）——宿主/其他模块引用本变量会**链接失败**
// （实测 `LNK2001 无法解析的外部符号 Tag_Tcs_Event_...`）。框架事件 Tag 的设计意图正是"供宿主订阅"，
// 故本模块的 Tag 声明 MUST 带模块导出宏（同模块内的定义 TU 见到 dllexport 声明即导出符号，定义处零改动）。
extern TCSATTRIBUTE_API FNativeGameplayTag Tag_Tcs_Event_Attribute_ValueChanged;



/**
 * 属性**当前值**变更事件（D2-5 事务事件，核心词汇 FStruct）：重算产生实质变化时经总线**立即通道**派发。
 * 变化判定阈值 epsilon = 1e-5（未变不广播）；同一提交内同一属性最多广播一次（事务能力保证）。
 * 走总线的载荷需反射可见，故本结构为 USTRUCT。
 *
 * **事件语义 = "对外可读的当前值变了"（结果事件，不是操作事件）**：任何写操作（改基值 / 挂摘修正器 /
 * 依赖属性连带变化）都只在**当前值确实动了**时产生这一条事件——所以它**不承担**"某次写操作发生过"的记账，
 * 也**不承担**"基础值变更历史"（基础值改了但被值域收口吃掉 = 对外没变 = 不广播，这是故意的：
 * "未变不广播"）。要区分"为什么变"属 `Reason` 扩展位（02 §4 预留），R3 无消费者故不预建。
 */
USTRUCT(BlueprintType)
struct TCSATTRIBUTE_API FTcsAttributeChangedEvent
{
	GENERATED_BODY()

// 变更内容
#pragma region Payload

public:
	// 变更所属单位
	UPROPERTY(BlueprintReadOnly, Category = "Attribute Event")
	FTcsCombatEntityHandle Unit;

	// 发生变更的属性名
	UPROPERTY(BlueprintReadOnly, Category = "Attribute Event")
	FTcsAttributeName Attribute;

	// 变更前当前值
	UPROPERTY(BlueprintReadOnly, Category = "Attribute Event")
	double OldValue = 0.0;

	// 变更后当前值
	UPROPERTY(BlueprintReadOnly, Category = "Attribute Event")
	double NewValue = 0.0;

#pragma endregion
};
