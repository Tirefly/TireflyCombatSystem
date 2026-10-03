// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepSelectTargets.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "Targeting/TcsTargetScorerStrategy.h"
#include "TcsEffectSubsystem.h"
#include "TcsTargetingLogChannel.h"



namespace
{
	/**
	 * 单个候选的**预计算评分行**（P-A 排序相位）。
	 *
	 * 预计算的理由：评分频次是"逐候选 × 逐排序项"，而比较排序会比较 `O(n log n)` 次——
	 * 逐次比较都现算会把每个评分器调用放大到对数倍。故每个评分器对每个候选**只求值一次**。
	 *
	 * 命名带文件前缀：UBT 的 unity 合并会把同模块多个 `.cpp` 并进同一翻译单元，
	 * 匿名命名空间的通用名会跨文件相撞（先例见 `unreal-development-workflow` 的引擎机制事实）。
	 */
	struct FSelectTargetsScoredCandidate
	{
		// 候选句柄（稳定键决胜也取它）
		FTcsCombatEntityHandle Handle;

		// 逐排序项的评分（顺序与 SortItems 一致；含 NaN 的候选不会进入本数组）
		TArray<double> Scores;
	};

	/**
	 * 求一个排序项对该候选的评分。
	 *
	 * 载体为空 / 类型不符时按**中性分 0.0** 处理（与过滤器"空槽位不淘汰候选"同款口径：
	 * 配置残缺由作者侧校验兜底，执行器不做隐式淘汰）。
	 */
	double EvaluateSelectTargetsSortItem(
		const FTcsTargetSortItem& SortItem,
		FTcsCombatEntityHandle Candidate,
		const FTcsEffectContext& Context,
		ITcsEntityQuery* EntityQuery)
	{
		const FTcsTargetScorerStrategy* Scorer = SortItem.Scorer.GetPtr<FTcsTargetScorerStrategy>();
		if (!Scorer)
		{
			return 0.0;
		}

		return Scorer->Score(Candidate, Context, EntityQuery);
	}

	/**
	 * SelectTargets 步骤执行器（即时步骤——目标选择无异步语义，恒返回 TSR_Completed）。
	 *
	 * 执行序：清空目标集（写回时整体替换）→ `Selector->Resolve` → 过滤（AND + 短路）→
	 * **去重**（键 = 句柄身份）→ **排序**（严格字典序 + 每项独立方向 + 稳定键决胜）→
	 * **取前 K** → 写回 `Context.Targets`。
	 *
	 * 跨模块自注册（D4-14 的首个实证）：本文件用一行宏把自己的执行器喂进 TcsEffect 的注册表，
	 * 机制层不认识本类型、也零改动。
	 */
	ETcsStepResult ExecuteStepSelectTargets(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepSelectTargets* Step = StepData.GetPtr<FTcsStepSelectTargets>();
		if (!Step)
		{
			UE_LOG(LogTcsTargeting, Error, TEXT("SelectTargets[%s]: 步骤载荷类型不符——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		const FTcsTargetSelectorStrategy* Selector = Step->Selector.GetPtr<FTcsTargetSelectorStrategy>();
		if (!Selector)
		{
			// 未配选择器：目标集保持原样（与"选择器选中空集"可区分）——不静默清空、不 ensure
			UE_LOG(LogTcsTargeting, Warning, TEXT("SelectTargets[%s]: 未配置 Selector——目标集保持原样（%d 个）"),
				*Run.ChainId.ToString(), Context.Targets.Num());
			return ETcsStepResult::TSR_Completed;
		}

		// 宿主能力注入点（未注入为 nullptr——契约允许，策略自行降级）
		ITcsEntityQuery* EntityQuery = nullptr;
		if (const UTcsEffectSubsystem* Subsystem = Run.Owner.Get())
		{
			EntityQuery = Subsystem->GetEntityQuery();
		}

		// 候选：本步骤（调用方）负责清空，选择器只负责填充（实体句柄流转——无 Actor 依赖）
		TArray<FTcsCombatEntityHandle> Candidates;
		Selector->Resolve(Context, EntityQuery, Candidates);

		// 过滤：AND 全过 + 短路 + 保序（确定性纪律 D0-1）
		// **不做存活过滤**：候选有效性是宿主语义（由 Filter 表达）——框架不认"存活"，
		// 无 Filter 时句柄原样传递（消费方自行用 IsAlive 核对）
		TArray<FTcsCombatEntityHandle> PassedTargets;
		PassedTargets.Reserve(Candidates.Num());
		for (const FTcsCombatEntityHandle& Candidate : Candidates)
		{
			bool bPassedAllFilters = true;
			for (const FInstancedStruct& FilterStruct : Step->Filters)
			{
				const FTcsTargetFilterStrategy* Filter = FilterStruct.GetPtr<FTcsTargetFilterStrategy>();
				if (!Filter)
				{
					// 空槽位不淘汰候选（配置残缺由作者侧校验兜底，执行器不做隐式过滤）
					continue;
				}

				if (!Filter->Pass(Candidate, Context))
				{
					bPassedAllFilters = false;
					break;
				}
			}

			if (bPassedAllFilters)
			{
				PassedTargets.Add(Candidate);
			}
		}

		// 去重（键 = 实体句柄身份）：宿主来源重叠时下游会对同一目标动手两次——本步把该缺口堵住。
		// 位置 MUST 在取前 K **之前**（放到之后会让结果数少于 K）。重复项彼此不可区分，
		// 故"保留先出现的还是后出现的"无观测差异。
		TArray<FTcsCombatEntityHandle> ResultTargets;
		ResultTargets.Reserve(PassedTargets.Num());
		{
			TSet<FTcsCombatEntityHandle> SeenHandles;
			SeenHandles.Reserve(PassedTargets.Num());
			for (const FTcsCombatEntityHandle& Candidate : PassedTargets)
			{
				bool bAlreadyInSet = false;
				SeenHandles.Add(Candidate, &bAlreadyInSet);
				if (!bAlreadyInSet)
				{
					ResultTargets.Add(Candidate);
				}
			}
		}

		const int32 DuplicateCount = PassedTargets.Num() - ResultTargets.Num();

		// 排序（**空 SortItems 时不做任何重排**——既有"保序"行为逐位不变）
		if (Step->SortItems.Num() > 0 && ResultTargets.Num() > 1)
		{
			// 预计算评分矩阵：任一排序项返回 NaN ⇒ 该候选**被排除**（MUST NOT 排到末尾）；
			// ±Inf 保留参与排序（比较用 `==` 判等，±Inf 与自身相等，故不会误进稳定键分支）
			TArray<FSelectTargetsScoredCandidate> ScoredCandidates;
			ScoredCandidates.Reserve(ResultTargets.Num());

			for (const FTcsCombatEntityHandle& Candidate : ResultTargets)
			{
				FSelectTargetsScoredCandidate Row;
				Row.Handle = Candidate;
				Row.Scores.Reserve(Step->SortItems.Num());

				bool bScorable = true;
				for (const FTcsTargetSortItem& SortItem : Step->SortItems)
				{
					const double Score = EvaluateSelectTargetsSortItem(SortItem, Candidate, Context, EntityQuery);
					if (FMath::IsNaN(Score))
					{
						bScorable = false;
						break;
					}

					Row.Scores.Add(Score);
				}

				if (bScorable)
				{
					ScoredCandidates.Add(MoveTemp(Row));
				}
			}

			// 严格字典序（第 N 项只在第 1..N-1 项全等时才参与）+ 每项独立方向；
			// 全部相等后按**稳定键决胜**：实体句柄 `Id` **升序**（唯一裁定者——保证同进程可复现）
			const TArray<FTcsTargetSortItem>& SortItems = Step->SortItems;
			ScoredCandidates.Sort([&SortItems](const FSelectTargetsScoredCandidate& A, const FSelectTargetsScoredCandidate& B)
			{
				for (int32 SortIndex = 0; SortIndex < SortItems.Num(); ++SortIndex)
				{
					const double ScoreA = A.Scores[SortIndex];
					const double ScoreB = B.Scores[SortIndex];
					if (ScoreA == ScoreB)
					{
						continue;
					}

					const bool bAscending = SortItems[SortIndex].Direction == ETcsTargetSortDirection::ETSD_Ascending;
					return bAscending ? (ScoreA < ScoreB) : (ScoreA > ScoreB);
				}

				return A.Handle.Id < B.Handle.Id;
			});

			ResultTargets.Reset(ScoredCandidates.Num());
			for (const FSelectTargetsScoredCandidate& Row : ScoredCandidates)
			{
				ResultTargets.Add(Row.Handle);
			}
		}

		const int32 BeforeTruncateCount = ResultTargets.Num();

		// 取前 K（按**完整字典序**——排完序再截取；0 / 负值 = 不限）
		if (Step->MaxCount > 0 && ResultTargets.Num() > Step->MaxCount)
		{
			ResultTargets.SetNum(Step->MaxCount, EAllowShrinking::No);
		}

		Context.Targets = MoveTemp(ResultTargets);

		UE_LOG(LogTcsTargeting, Log,
			TEXT("SelectTargets[%s]: 候选 %d → 通过 %d（Filter 数 %d）→ 去重 %d → 排序项 %d → 取前 %d → 目标集 %d"),
			*Run.ChainId.ToString(), Candidates.Num(), PassedTargets.Num(), Step->Filters.Num(),
			DuplicateCount, Step->SortItems.Num(), BeforeTruncateCount, Context.Targets.Num());

		return ETcsStepResult::TSR_Completed;
	}
}

// 跨模块自注册（TcsTargeting → TcsEffect 注册表；模块静态初始化期登记）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepSelectTargets, ExecuteStepSelectTargets)
