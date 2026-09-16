// Copyright Tirefly. All Rights Reserved.

#include "EventBus/UTcsEventHandler.h"

void UTcsEventHandler::HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload)
{
	// 默认空实现：C++ 派生类覆写 _Implementation，蓝图子类走同签名事件覆写（共享 Handler 无状态）
}
