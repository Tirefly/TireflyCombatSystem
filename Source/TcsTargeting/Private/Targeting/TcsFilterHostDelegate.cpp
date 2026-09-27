// Copyright Tirefly. All Rights Reserved.

#include "Targeting/TcsFilterHostDelegate.h"



bool FTcsFilterHostDelegate::Pass(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context) const
{
	// 转发到宿主脚本实现（**反射分派**——脚本层可达）
	UObject* HostObject = Host.GetObject();
	if (!HostObject)
	{
		// 未配置宿主实现：判"通过"（与基类中性默认实现同口径——见头注释的降级理由）
		return true;
	}

	return ITcsTargetFilterHost::Execute_PassTarget(
		HostObject, Candidate, Context.Caster, Context.Instigator);
}
