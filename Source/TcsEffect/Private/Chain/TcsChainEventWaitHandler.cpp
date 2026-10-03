// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsChainEventWaitHandler.h"

#include "TcsEffectSubsystem.h"



// 装配
void UTcsChainEventWaitHandler::Initialize(UTcsEffectSubsystem* InOwner)
{
	Owner = InOwner;
}



// 派发入口
void UTcsChainEventWaitHandler::HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload)
{
	UTcsEffectSubsystem* Facade = Owner.Get();
	if (!Facade)
	{
		// 门面已回收（世界拆解期）——订阅的惰性摘除由总线负责（Handler 弱引用失效即僵尸订阅），此处静默返回
		return;
	}

	Facade->WakeEventWaiters(EventTag, Payload);
}
