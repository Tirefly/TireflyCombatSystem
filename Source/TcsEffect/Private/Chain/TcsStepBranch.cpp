// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepBranch.h"

#include "Chain/TcsEffectStepExecutor.h"
#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"
#include "Trigger/TcsTriggerCondition.h"



namespace
{
	/**
	 * 分支原语执行器：条件求值（触发行同一套注册表）→ 选支 → 交给门面侧的**共用实现**起链
	 * （与 `RunSubChain` 同一入口——子链完成唤醒只有一份实现）。
	 *
	 * **步内起链纪律（D-4 的落点）**：条件求值与取值全在起链**之前**完成，之后 MUST NOT 再触碰
	 * `StepData` / `Context` / `Run`——`RunChildChainStep` 只收值类型（句柄 / tag / bool）。
	 */
	ETcsStepResult ExecuteStepBranch(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepBranch* Step = StepData.GetPtr<FTcsStepBranch>();
		UTcsEffectSubsystem* Facade = Run.Owner.Get();
		if (!Step || !Facade)
		{
			// 载荷类型不符 / 门面已回收（世界拆解期）：按完成处理（不挂起——挂起需要可靠的唤醒源）
			UE_LOG(LogTcsEffect, Warning, TEXT("Branch[%s]: 步骤载荷类型不符或门面不可得——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 链侧上下文**最小映射**：本批 `Caster` 与 `World` 有来源（`EventTag` / `ClassificationTags`
		// 在链侧无来源 ⇒ 依赖它们的条件恒不通过——明示接受的限制，见头文件与台账）。
		// `World`（2026-10-05 补）：`AttributeCompare` 要经它解析属性门面；漏填会让该条件**静默恒不通过**。
		FTcsTriggerContext TriggerContext;
		TriggerContext.Caster = Context.Caster;
		TriggerContext.World = Facade->GetWorld();

		// 条件求值：空数组 = 无条件通过（`EvaluateTriggerConditions` 的既有语义）；
		// 随机值取自门面种子流——与触发行同款可复现口径（求值内部 MUST NOT 自行取随机数）
		const bool bThen = EvaluateTriggerConditions(
			Step->Conditions, TriggerContext, Facade->NextTriggerRandomValue(), Facade->GetWorld());

		// 取齐数据（值拷贝）：此后不得再用 `Step` / `Context` / `Run`
		const FGameplayTag ChainId = bThen ? Step->ThenChainId : Step->ElseChainId;
		const bool bWait = Step->bWait;
		const FTcsChainRunHandle Self = Run.Self;
		const FString SelfChainName = Run.ChainId.ToString();

		// 空支 = 正常配置（不是漏配）：不起链、不记 Warning，本步仍算完成
		if (!ChainId.IsValid())
		{
			UE_LOG(LogTcsEffect, Verbose, TEXT("Branch[%s]: 走向 %s 支为空——不起链（本步按完成处理）"),
				*SelfChainName, bThen ? TEXT("Then") : TEXT("Else"));
			return ETcsStepResult::TSR_Completed;
		}

		return Facade->RunChildChainStep(Self, ChainId, bWait);
	}
}

// 自注册（本模块内；机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepBranch, ExecuteStepBranch)
