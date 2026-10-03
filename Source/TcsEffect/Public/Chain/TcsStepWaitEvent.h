// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

#include "TcsStepWaitEvent.generated.h"



/**
 * 事件等待步骤（`04 §2.4` 唤醒源之"事件匹配"的首个载体）。执行器住
 * `Private/Chain/TcsStepWaitEvent.cpp`（+ 一行宏自注册）。
 *
 * **语义**：首入即订阅 `EventTag`（**立即通道**——等待者要在事件发布返回之前就被唤醒，帧末通道会晚一拍）
 * 并返回 `TSR_Running`（PC 停驻本步、零每帧成本）；`TimeoutSeconds > 0` 时**同时**入到期堆
 * （基准 = 战斗时钟 `Elapsed`，与 `WaitDelay` 同一时轴）。两条唤醒路择先到者兑现，且**互相取消**：
 * 事件先到则撤掉到期条目、超时先到则退订——否则先到者兑现后另一路还会产生第二次唤醒。
 *
 * **两路的结果不同**（这是本原语最要紧的一条）：
 * - **命中**：事件载荷被写进该运行态的 `FTcsEffectContext::EventPayload`（门面在派发当场写），
 *   后续步骤据此读本次事件；
 * - **超时**：`EventPayload` **保持原样**——超时不是事件，MUST NOT 伪造成载荷（"没等到"必须与
 *   "等到了一个空载荷"可区分）。
 *
 * **订阅形态（本原语的核心设计决定）**：**同一 `EventTag` 的多个等待者共用一条总线订阅**
 * （计数配对：首个等待者订阅、最后一个离开时退订）——运行态**不持订阅句柄**，只持"在等哪个 tag"
 * 这一可配平锚；订阅句柄归门面侧的等待表（`FTcsChainEventWaitRegistry`）。依据两条硬事实：总线
 * 派发只传 `(EventTag, Payload)`、**不传订阅句柄**（每条等待各订一次既无收益、又让回调无法自辨身份），
 * 以及既有明文纪律"订阅表不得随数量膨胀"。退订覆盖三条路径：命中 / 超时 / 运行态释放。
 *
 * **降级（一律不挂起、不崩溃）**：`EventTag` 无效 → Warning + 本步按完成处理；总线设施不可得 →
 * Error + 本步按完成处理（挂起 MUST 有可靠的唤醒源——口径同 `WaitDelay` 的缺设施路径）；
 * 作者配了超时但时钟设施不可得 → Error + **撤掉刚装的等待**并按完成处理（作者配超时即表示他不接受
 * "永远等"，故此处不接受"只等事件"的半程挂起）。
 */
USTRUCT()
struct TCSEFFECT_API FTcsStepWaitEvent
{
	GENERATED_BODY()

// 等待
#pragma region Wait

public:
	// 等的事件 Tag（无效 → 不挂起：Warning + 本步按完成处理）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|WaitEvent")
	FGameplayTag EventTag;

	// 超时秒数（战斗时间秒；**0 = 不设超时**，只等事件）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|WaitEvent", meta = (ClampMin = "0.0"))
	double TimeoutSeconds = 0.0;

#pragma endregion
};
