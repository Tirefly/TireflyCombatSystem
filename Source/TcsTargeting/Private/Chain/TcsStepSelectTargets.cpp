// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepSelectTargets.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectSubsystem.h"
#include "TcsTargetingLogChannel.h"



namespace
{
	/**
	 * SelectTargets 步骤执行器（即时步骤——目标选择无异步语义，恒返回 TSR_Completed）。
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

		Context.Targets = MoveTemp(PassedTargets);

		UE_LOG(LogTcsTargeting, Log, TEXT("SelectTargets[%s]: 候选 %d → 通过 %d（Filter 数 %d）"),
			*Run.ChainId.ToString(), Candidates.Num(), Context.Targets.Num(), Step->Filters.Num());
		return ETcsStepResult::TSR_Completed;
	}
}

// 跨模块自注册（TcsTargeting → TcsEffect 注册表；模块静态初始化期登记）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepSelectTargets, ExecuteStepSelectTargets)
