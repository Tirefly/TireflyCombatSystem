// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepDamage.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "Flow/TcsFlowKeys.h"
#include "Flow/TcsDamageFlowContext.h"
#include "Parameter/TcsParamSource_Literal.h"
#include "TcsDamageLogChannel.h"
#include "TcsDamageSubsystem.h"
#include "TcsEffectSubsystem.h"



namespace
{
	// 黑板契约键（与标准步骤库一致：`BaseDamage` 是"本次基础伤害"的契约键）
	const FName TcsChainDamage_BaseKey(TEXT("BaseDamage"));

	/**
	 * 伤害链原语执行器（09 §2.4 / D7-2）：构流程上下文 → 写基础值 → `RunTemplate`。
	 * **跨模块自注册**（D4-14）：本文件用一行宏把执行器喂进 TcsEffect 的注册表——TcsEffect 不认识
	 * 伤害语义、TcsDamage 也不依赖 TcsTargeting（目标集只流动在链上下文里）。
	 */
	ETcsStepResult ExecuteStepDamage(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepDamage* Step = StepData.GetPtr<FTcsStepDamage>();
		if (!Step)
		{
			UE_LOG(LogTcsDamage, Error, TEXT("Damage[%s]: 步骤载荷类型不符——本步按完成处理"), *Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 门面（流程宿主；经链运行态取——句柄化后没有 Actor 可借道）
		UTcsDamageSubsystem* Owner = nullptr;
		if (UTcsEffectSubsystem* EffectSubsystem = Run.Owner.Get())
		{
			UWorld* World = EffectSubsystem->GetWorld();
			Owner = World ? World->GetSubsystem<UTcsDamageSubsystem>() : nullptr;
		}
		if (!Owner)
		{
			UE_LOG(LogTcsDamage, Error, TEXT("Damage[%s]: 伤害流程门面不可得——本步按完成处理"), *Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		FTcsDamageFlowContext FlowContext;
		FlowContext.Attacker = Context.Caster;
		FlowContext.Instigator = Context.Instigator;
		FlowContext.Targets = Context.Targets;
		FlowContext.FormulaParams = Step->FormulaParams;

		// 基础伤害值 = **参数账本解算结果**（PV-7；R3 用 Literal 直配）——流程零计算：
		// 本步骤只搬运，不做任何推导。以**上下文请求字段**传入（不是黑板键——黑板会被 CollectStart 重置）
		FlowContext.BaseDamageInput = Step->DamageBase.Evaluate(FTcsParamEvaluateContext());

		// 扣血目标属性键（请求方指定——插件组装的默认模板不可能知道项目词表）
		FlowContext.TargetAttrKey = Step->TargetAttrKey;

		// 起流程（无效模板 tag → 官方默认模板；流程单帧同步完成，无挂起）
		// 默认模板 tag 是**框架词汇**（插件自带），由本模块原生声明（`TcsFlowKeys.h`）——零项目配置依赖。
		const FGameplayTag TemplateId = Step->FlowTemplateId.IsValid()
			? Step->FlowTemplateId
			: FGameplayTag(Tag_Tcs_Flow_Template_Default);
		if (!Owner->RunTemplate(TemplateId, FlowContext))
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("Damage[%s]: 流程 %s 未走完（中止路径见上文日志）"),
				*Run.ChainId.ToString(), *TemplateId.ToString());
		}

		return ETcsStepResult::TSR_Completed;
	}
}

// 跨模块自注册（TcsDamage → TcsEffect 注册表；模块静态初始化期登记）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepDamage, ExecuteStepDamage)
