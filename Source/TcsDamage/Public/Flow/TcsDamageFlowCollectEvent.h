// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"

#include "Flow/TcsDamageFlowContext.h"

#include "TcsDamageFlowCollectEvent.generated.h"



/**
 * 流程收集协议的事件 Tag（09 §2.2/§3）——按 **`Tcs.Event.<域>.<事件名>` 命名公约**
 * （2026-09-18 用户拍板）由本模块**原生声明**（不进项目 Tag 表：项目漏配不会让事件静默丢失；
 * TcsCore 不持战斗域词汇）。**设计文档旧写法 `Combat.Damage.Collect.<Step>` 不采用**（早于该公约）。
 *
 * 本任务只声明**流程开始事件**（标准步骤 `CollectStart` 的落点）；各步骤的收集事件
 * （PreHit / AfterDamage / PreExecute 等）随标准步骤库落地时**按 Step 名逐条声明**（Task 4）。
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(Tag_TcsEvent_Damage_FlowStarted);



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
