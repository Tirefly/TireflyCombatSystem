// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepModifyFlow.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsFlowKeys.h"
#include "TcsDamageLogChannel.h"



namespace
{
	/**
	 * ModifyFlow 链原语执行器（09 §2.3）：从链上下文的 `EventPayload` 解出流程上下文 →
	 * 向黑板提交一笔修正 → 恒 `TSR_Completed`（即时步骤，无挂起语义）。
	 *
	 * **跨模块自注册**（D4-14）：本文件用一行宏把执行器喂进 TcsEffect 的注册表——TcsEffect 不认识
	 * 伤害语义，流程上下文也不经具名类型穿过模块边界（走 `EventPayload` 这一中立字段；
	 * 载荷类型与解包知识全住本模块）。
	 */
	ETcsStepResult ExecuteStepModifyFlow(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepModifyFlow* Step = StepData.GetPtr<FTcsStepModifyFlow>();
		if (!Step)
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("ModifyFlow[%s]: 步骤载荷类型不符——本步按完成处理"), *Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 流程上下文经中立通道取（`Context.EventPayload` 由触发求值器在起链时装配）
		const FTcsDamageFlowCollectEvent* Event = Context.EventPayload.GetPtr<FTcsDamageFlowCollectEvent>();
		FTcsDamageFlowContext* FlowContext = Event ? Event->Context : nullptr;
		if (!FlowContext)
		{
			// 载荷缺失或类型不符：不崩溃、不静默通过、**不产生半笔提交**
			UE_LOG(LogTcsDamage, Warning,
				TEXT("ModifyFlow[%s]: 链上下文无流程收集载荷（EventPayload 为空或类型不符）——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 键无效 = 未配置 → 落契约键（同 `FTcsFlowBaseDamage` / `FTcsFlowExecute` 的兜底口径：
		// 兜底 MUST 在执行器内做——`FGameplayTag` 字段默认值不能是 tag）
		const FGameplayTag Key = Step->TargetKey.IsValid()
			? Step->TargetKey
			: FGameplayTag(Tag_DamageFlowKey_BaseDamage);

		if (!FlowContext->Blackboard.Submit(Key, Step->Op, Step->Operand, Step->Consume))
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("ModifyFlow[%s]: 提交键 %s 无效——本笔修正未落账"),
				*Run.ChainId.ToString(), *Key.ToString());
		}

		return ETcsStepResult::TSR_Completed;
	}
}

// 跨模块自注册（TcsDamage → TcsEffect 注册表；模块静态初始化期登记）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepModifyFlow, ExecuteStepModifyFlow)
