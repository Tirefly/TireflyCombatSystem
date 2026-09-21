// Copyright Tirefly. All Rights Reserved.

#include "Targeting/TcsSelSelf.h"

#include "TcsTargetingLogChannel.h"



void FTcsSelSelf::Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* /*EntityQuery*/, TArray<FTcsCombatEntityHandle>& OutTargets) const
{
	if (!Context.Caster.IsValid())
	{
		// 装配缺失（不是"选中空集"）：留痕但不产出——与"空集是合法结果"可区分
		UE_LOG(LogTcsTargeting, Warning, TEXT("FTcsSelSelf::Resolve: Context.Caster 无效——不产出目标"));
		return;
	}

	// 只填充不清空（基类契约）：调用方负责清空 OutTargets
	OutTargets.Add(Context.Caster);
}
