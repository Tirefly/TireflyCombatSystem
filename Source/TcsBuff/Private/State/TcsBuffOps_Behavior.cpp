// Copyright Tirefly. All Rights Reserved.

#include "State/TcsBuffOps.h"

#include "Def/TcsBuffDef.h"
#include "State/TcsBuffBehaviorHandler.h"
#include "TcsBuffSubsystem.h"



void FTcsBuffOps::WireBehaviors(
	UTcsBuffSubsystem& Subsystem,
	const FTcsBuffInstance& Instance,
	const FTcsBuffDef& Def)
{
	if (Def.Fragments.IsEmpty())
	{
		// 默认空载体不创建 UObject，也不占订阅或实例登记。
		return;
	}

	if (!Subsystem.BehaviorHandler)
	{
		Subsystem.BehaviorHandler = NewObject<UTcsBuffBehaviorHandler>(&Subsystem);
		Subsystem.BehaviorHandler->Initialize(&Subsystem);
	}

	Subsystem.BehaviorRegistry.SetHandler(Subsystem.BehaviorHandler.Get());
	Subsystem.BehaviorRegistry.AddInstance(Instance.Handle, Def, Subsystem.GetEventBus());
}

void FTcsBuffOps::UnwireBehaviors(
	UTcsBuffSubsystem& Subsystem,
	const FTcsBuffInstance& Instance)
{
	Subsystem.BehaviorRegistry.RemoveInstance(Instance.Handle, Subsystem.GetEventBus());
}
