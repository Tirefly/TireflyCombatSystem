// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EventBus/TcsEventHandler.h"

#include "TcsStateBehaviorHandler.generated.h"

class UTcsStateSubsystem;



/**
 * 状态行为的共享总线 Handler。只持门面弱引用，不持任何实例或策略。
 * 路由与句柄校验交给门面，Handler 的强引用由门面 UPROPERTY 持有。
 */
UCLASS()
class TCSSTATE_API UTcsStateBehaviorHandler : public UTcsEventHandler
{
	GENERATED_BODY()

// 装配
#pragma region Setup

public:
	// 装配门面弱引用（实现在 .cpp，赋值弱指针需要门面类型完整）
	void Initialize(UTcsStateSubsystem* InOwner);

#pragma endregion


// 派发入口
#pragma region Dispatch

public:
	/**
	 * 把事件转交门面，按注册表快照派发给在册实例。
	 *
	 * @param EventTag 命中的精确事件 Tag。
	 * @param Payload 本次事件载荷。
	 */
	virtual void HandleEvent_Implementation(
		FGameplayTag EventTag,
		const FInstancedStruct& Payload) override;

#pragma endregion


// 内核
#pragma region Core

private:
	// 世界门面弱引用；不延长其生命期
	TWeakObjectPtr<UTcsStateSubsystem> Owner;

#pragma endregion
};
