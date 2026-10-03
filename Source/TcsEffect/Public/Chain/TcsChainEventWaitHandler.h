// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventHandler.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"

#include "TcsChainEventWaitHandler.generated.h"

class UTcsEffectSubsystem;



/**
 * 链事件等待的**共享 Handler**（唤醒源②"事件匹配"的承接者；**共享 Handler**——裁决 2a：
 * 事件类型 → Handler 对象执行，事件 struct 上不自绑 delegate）。
 *
 * **为什么必须有这个对象、且不能复用 `UTcsTriggerEvaluator`**：
 * - 总线 `Subscribe` 只收 `UTcsEventHandler*`（UObject；传不了 lambda），且订阅表对该对象持
 *   **弱引用** ⇒ 承接者必须被 `UPROPERTY` 强持有，否则被 GC 掉、订阅静默失效（与触发求值器同款纪律）；
 * - `UTcsEffectSubsystem` **已经是 Subsystem**、不能双 UObject 基 ⇒ 承接者只能是独立对象。
 *   触发求值器虽同为 `UTcsEventHandler`，但它的回调语义是"逐行求值并起链"——与本对象
 *   "把事件路由给等待中的运行态"是两回事，合并会让两条路径的失败面互相污染。
 *
 * **本对象无实例状态**（唯一成员是指回门面的弱引用）：等待者表住门面侧的
 * `FTcsChainEventWaitRegistry`，故"共享"是天然成立的——同一 tag 的 N 条等待**只订阅一条**。
 */
UCLASS()
class TCSEFFECT_API UTcsChainEventWaitHandler : public UTcsEventHandler
{
	GENERATED_BODY()

// 装配
#pragma region Setup

public:
	/**
	 * 装配：持门面弱引用（等待者路由与唤醒重入都要经它）。由门面在创建本对象后立即调用一次。
	 *
	 * @param InOwner 门面（通常即 `this` 的 Outer）。
	 */
	void Initialize(UTcsEffectSubsystem* InOwner);

#pragma endregion


// 派发入口
#pragma region Dispatch

public:
	/**
	 * 事件入口（总线订阅回调）：把事件交给门面按 tag 路由到等待中的运行态。
	 *
	 * **不在这里做路由**：派发只传 `(EventTag, Payload)`、**不传订阅句柄** ⇒ 本对象无法自辨
	 * "是哪条运行态在等"，只能靠门面侧等待表认领（D-1）。
	 *
	 * @param EventTag 命中的事件 Tag（总线传入）。
	 * @param Payload 事件载荷（立即通道零复制透传）。
	 */
	virtual void HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload) override;

#pragma endregion


// 内核
#pragma region Core

private:
	// 门面弱引用（运行态生命周期不长于子系统）
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;

#pragma endregion
};
