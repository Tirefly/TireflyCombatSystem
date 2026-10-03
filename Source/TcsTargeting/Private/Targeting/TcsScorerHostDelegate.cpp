// Copyright Tirefly. All Rights Reserved.

#include "Targeting/TcsScorerHostDelegate.h"

#include "TcsTargetingLogChannel.h"



double FTcsScorerHostDelegate::Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* /*EntityQuery*/) const
{
	// 转发到宿主脚本实现（**反射分派**——脚本层可达；虚分派在此止步）
	UObject* HostObject = Host.GetObject();
	if (!HostObject)
	{
		// 未配置宿主实现：中性分（**不淘汰候选**——"这一项不区分候选"比"打不到人"更易排查）
		UE_LOG(LogTcsTargeting, Warning,
			TEXT("FTcsScorerHostDelegate::Score: Host 未配置——本排序项按中性分处理（不淘汰候选）"));
		return 0.0;
	}

	// 脚本返回 NaN ⇒ 该候选被排除（同一条浮点边界，调用点不特殊处理）
	// 注：本函数按"逐候选 × 逐排序项"调用，故不加逐次日志（热路径纪律）
	return ITcsTargetScorerHost::Execute_ScoreTarget(HostObject, Candidate, Context.Caster, Context.Instigator);
}
