// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Flow/TcsDamageFlowContext.h"

#include "TcsDamageFlowCollectEvent.generated.h"



/**
 * 流程收集协议的事件 Tag 全集（09 §2.2/§3）——按 **`Tcs.Event.<域>.<事件名>` 命名公约**
 * （2026-09-18 用户拍板）由本模块**原生声明**（不进项目 Tag 表：项目漏配不会让事件静默丢失；
 * TcsCore 不持战斗域词汇）。**设计文档旧写法 `Combat.Damage.Collect.<Step>` 不采用**（早于该公约）。
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_FlowStarted);   // CollectStart
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_PreHit);        // PreHit
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_Hit);           // Hit（宿主可改写 `HitRate` 键）
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_Crit);          // Crit（宿主可改写 `CritRate` 键）
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_Element);       // Element
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_AfterDamage);   // AfterDamage（"伤害 +50" 挂点）
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_PreExecute);    // PreExecute（收集免疫/减伤候选）
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_Completed);     // Completed（记录已产出）
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_Recorded);      // 记录发布（载荷 = FTcsDamageRecord）



/**
 * 收集事件载荷（09 §2.3 的"payload = Context 引用包装"）：总线载荷须反射可见（`FInstancedStruct`），
 * 而 C++ 引用不可反射——故以**指针包装**承载。指针为**进程内瞬态**：立即通道同步派发期有效，
 * 消费方 MUST NOT 跨帧持有（框架不为它保活）。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsDamageFlowCollectEvent
{
	GENERATED_BODY()

	// 流程上下文（派发期有效；订阅方在回调内提交黑板修正即可——收集 ≠ 消费）
	FTcsDamageFlowContext* Context = nullptr;
};
