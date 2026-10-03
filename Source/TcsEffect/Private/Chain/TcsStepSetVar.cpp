// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepSetVar.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectLogChannel.h"



namespace
{
	/**
	 * SetVar 链原语执行器（D4-3 元原语）：求值 `Value` → 写 `Context.Variables` → 恒 `TSR_Completed`。
	 *
	 * 求值上下文与同族步骤同款（默认构造）：`FTcsParamEvaluateContext` 的参数表访问可空，
	 * 空时 `ParamRef` 类源落 Fallback（口径见 `param-value` 能力，本文件不重复）。
	 */
	ETcsStepResult ExecuteStepSetVar(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepSetVar* Step = StepData.GetPtr<FTcsStepSetVar>();
		if (!Step)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("SetVar[%s]: 步骤载荷类型不符——本步按完成处理"), *Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 键无效 = 未配置：跳过写入但留痕——"写不进去"与"写进去的值不对"必须可区分
		if (!Step->VarKey.IsValid())
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("SetVar[%s]: VarKey 无效——跳过写入（本步按完成处理）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		const double Evaluated = Step->Value.Evaluate(FTcsParamEvaluateContext());

		// 同键覆写：后写生效（顺序链的天然语义）
		Context.Variables.Add(Step->VarKey, Evaluated);

		UE_LOG(LogTcsEffect, Verbose, TEXT("SetVar[%s]: %s = %.6f"),
			*Run.ChainId.ToString(), *Step->VarKey.ToString(), Evaluated);

		return ETcsStepResult::TSR_Completed;
	}
}

// 自注册（本模块内；机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepSetVar, ExecuteStepSetVar)
