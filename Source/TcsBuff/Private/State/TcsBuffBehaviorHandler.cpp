// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffBehaviorHandler.h"

#include "TcsBuffSubsystem.h"
#include "UObject/StrongObjectPtr.h"



void UTcsBuffBehaviorHandler::Initialize(UTcsBuffSubsystem* InOwner)
{
	Owner = InOwner;
}

void UTcsBuffBehaviorHandler::HandleEvent_Implementation(
	FGameplayTag EventTag,
	const FInstancedStruct& Payload)
{
	// 回调可显式反初始化门面并触发 GC；门面释放 UPROPERTY 后，当前 Handler 仍须活到转发返回。
	const TStrongObjectPtr<UTcsBuffBehaviorHandler> CallbackHandler(this);
	if (UTcsBuffSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->DispatchBehaviorEvent(EventTag, Payload);
	}
}
