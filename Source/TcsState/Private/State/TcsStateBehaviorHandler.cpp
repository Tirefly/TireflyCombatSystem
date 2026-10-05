// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateBehaviorHandler.h"

#include "TcsStateSubsystem.h"
#include "UObject/StrongObjectPtr.h"



void UTcsStateBehaviorHandler::Initialize(UTcsStateSubsystem* InOwner)
{
	Owner = InOwner;
}

void UTcsStateBehaviorHandler::HandleEvent_Implementation(
	FGameplayTag EventTag,
	const FInstancedStruct& Payload)
{
	// 回调可显式反初始化门面并触发 GC；门面释放 UPROPERTY 后，当前 Handler 仍须活到转发返回。
	const TStrongObjectPtr<UTcsStateBehaviorHandler> CallbackHandler(this);
	if (UTcsStateSubsystem* Subsystem = Owner.Get())
	{
		Subsystem->DispatchBehaviorEvent(EventTag, Payload);
	}
}
