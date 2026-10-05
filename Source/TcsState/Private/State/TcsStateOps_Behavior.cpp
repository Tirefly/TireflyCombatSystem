// Copyright Tirefly. All Rights Reserved.

#include "State/TcsStateOps.h"

#include "Def/TcsBuffDef.h"
#include "State/TcsStateBehaviorHandler.h"
#include "TcsStateSubsystem.h"



void FTcsStateOps::WireBehaviors(
	UTcsStateSubsystem& Subsystem,
	const FTcsStateInstance& Instance,
	const FTcsBuffDef& Def)
{
	if (Def.Fragments.IsEmpty())
	{
		// 默认空载体不创建 UObject，也不占订阅或实例登记。
		return;
	}

	if (!Subsystem.BehaviorHandler)
	{
		Subsystem.BehaviorHandler = NewObject<UTcsStateBehaviorHandler>(&Subsystem);
		Subsystem.BehaviorHandler->Initialize(&Subsystem);
	}

	Subsystem.BehaviorRegistry.SetHandler(Subsystem.BehaviorHandler.Get());
	Subsystem.BehaviorRegistry.AddInstance(Instance.Handle, Def, Subsystem.GetEventBus());
}

void FTcsStateOps::UnwireBehaviors(
	UTcsStateSubsystem& Subsystem,
	const FTcsStateInstance& Instance)
{
	Subsystem.BehaviorRegistry.RemoveInstance(Instance.Handle, Subsystem.GetEventBus());
}
