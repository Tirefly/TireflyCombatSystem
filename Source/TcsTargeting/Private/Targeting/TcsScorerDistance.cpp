// Copyright Tirefly. All Rights Reserved.

#include "Targeting/TcsScorerDistance.h"

#include <limits>

#include "TcsTargetingLogChannel.h"



double FTcsScorerDistance::Score(FTcsCombatEntityHandle Candidate, const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery) const
{
	// 零宿主语义：只消费框架自身契约"实体有位置"（ITcsEntityQuery::GetLocation）
	if (!EntityQuery)
	{
		UE_LOG(LogTcsTargeting, Warning,
			TEXT("FTcsScorerDistance::Score: 未注入 ITcsEntityQuery——无法评分（该候选被排除）"));
		return std::numeric_limits<double>::quiet_NaN();
	}

	// 施法者端取不到定位 = 本链整体无法评分：候选全部被排除（不是"距离 0 的全并列"）
	FVector CasterLocation;
	if (!Context.Caster.IsValid() || !EntityQuery->GetLocation(Context.Caster, CasterLocation))
	{
		UE_LOG(LogTcsTargeting, Warning,
			TEXT("FTcsScorerDistance::Score: 施法者无定位——无法评分（候选被排除）"));
		return std::numeric_limits<double>::quiet_NaN();
	}

	FVector CandidateLocation;
	if (!EntityQuery->GetLocation(Candidate, CandidateLocation))
	{
		UE_LOG(LogTcsTargeting, Verbose, TEXT("FTcsScorerDistance::Score: 候选无定位——该候选被排除"));
		return std::numeric_limits<double>::quiet_NaN();
	}

	// 分数 = 真实欧氏距离（非平方值）——它可观测，未声明的变换会误导诊断面
	return FVector::Dist(CasterLocation, CandidateLocation);
}
