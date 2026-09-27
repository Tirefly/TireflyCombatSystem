// Copyright Tirefly. All Rights Reserved.

#include "Targeting/TcsSelHostDelegate.h"

#include "TcsTargetingLogChannel.h"



void FTcsSelHostDelegate::Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<FTcsCombatEntityHandle>& OutTargets) const
{
	// 转发到宿主脚本实现（**反射分派**——脚本层可达；虚分派在此止步）
	// 纪律：调用方负责清空 OutTargets，本方法只填充（同基类契约）
	UObject* HostObject = Host.GetObject();
	if (!HostObject)
	{
		// 未配置宿主实现：产出空集（**不静默通过**——空集在链日志里与"选不中"可区分）
		UE_LOG(LogTcsTargeting, Warning, TEXT("FTcsSelHostDelegate: Host 未配置——产出空集（选择器无可用实现）"));
		return;
	}

	ITcsTargetSelectorHost::Execute_ResolveTargets(
		HostObject, Context.Caster, Context.Instigator, OutTargets);

	UE_LOG(LogTcsTargeting, Verbose, TEXT("FTcsSelHostDelegate: 宿主脚本选择器产出 %d 个目标"), OutTargets.Num());
}
