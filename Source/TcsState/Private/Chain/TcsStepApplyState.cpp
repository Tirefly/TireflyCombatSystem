// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepApplyState.h"

#include "Chain/TcsChainRun.h"
#include "Chain/TcsEffectContext.h"
#include "Chain/TcsEffectStepExecutor.h"
#include "Engine/World.h"
#include "Parameter/TcsParamTableReader.h"
#include "TcsEffectSubsystem.h"
#include "TcsStateLogChannel.h"
#include "TcsStateSubsystem.h"



namespace
{
	/**
	 * ApplyState 链原语执行器（D4-16 十五原语之一，**属 TcsState**；04 §2.1"ApplyState 住 TcsState"）。
	 *
	 * 流水：解析目标（步骤字段优先，退黑板目标集首个）→ 取状态门面 → `ApplyState`：
	 * `Source` = 本次运行的**来源锚点**（`Context.RunSource`）、`Instigator` = 黑板发起者、
	 * 参数表 = 空（链侧参数表载体不存在 ⇒ 引用类参数源落兜底——如实边界）。
	 *
	 * **不断链**（`ETcsStepResult` 只有 `Completed` / `Running`，步骤无法中断链）：
	 * 定义未登记 / 门面不可得 / 目标无效 ⇒ `Warning` + 按完成处理；`DefTag` 无效 ⇒ `Error` + 按完成处理。
	 * **四档回执都是业务结果**（含 `Rejected`）⇒ 一律 `Log` 级，MUST NOT 因此产生红字。
	 */
	ETcsStepResult ExecuteStepApplyState(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepApplyState* Step = StepData.GetPtr<FTcsStepApplyState>();
		if (!Step)
		{
			UE_LOG(LogTcsState, Warning, TEXT("ApplyState[%s]: 步骤载荷类型不符——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 目标：步骤字段优先，无效则取黑板目标集首个（D4-4 v2 的"默认目标"语义）
		FTcsCombatEntityHandle Target = Step->Target;
		if (!Target.IsValid() && Context.Targets.Num() > 0)
		{
			Target = Context.Targets[0];
		}

		if (!Target.IsValid())
		{
			// 软失败：不 ensure、不断链
			UE_LOG(LogTcsState, Warning, TEXT("ApplyState[%s]: 目标无效且黑板目标集为空——跳过（本步按完成处理）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		if (!Step->DefTag.IsValid())
		{
			// 未配置 = 配置错误（Error 级留痕），但仍不断链——软失败接管归 `OnError`（R5.5-a）
			UE_LOG(LogTcsState, Error, TEXT("ApplyState[%s]: DefTag 无效（未配置）——跳过（本步按完成处理）"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		const UTcsEffectSubsystem* Owner = Run.Owner.Get();
		const UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		UTcsStateSubsystem* StateSubsystem = World ? World->GetSubsystem<UTcsStateSubsystem>() : nullptr;
		if (!StateSubsystem)
		{
			UE_LOG(LogTcsState, Warning, TEXT("ApplyState[%s]: 状态门面不可得——跳过（定义=%s）"),
				*Run.ChainId.ToString(), *Step->DefTag.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 来源 = 本次运行的**来源锚点**（身份：同一运行内同源 ⇒ 续杯；两次运行异源 ⇒ 可叠层）；
		// 参数表留空（链侧无参数表载体），覆盖表来自步骤配置。
		const TScriptInterface<ITcsParamTableReader> EmptyParamTable;
		const EApplyResult Result = StateSubsystem->ApplyState(
			Target, Step->DefTag, Context.RunSource, Context.Instigator, EmptyParamTable, Step->Overrides);

		// 四档回执全部是业务结果（`Rejected` 亦然）⇒ `Log` 级：常规验收命令保持零非预期红字
		UE_LOG(LogTcsState, Log, TEXT("ApplyState[%s]: 单位=%lld 定义=%s 回执=%d 来源=%llu 覆盖=%d 条"),
			*Run.ChainId.ToString(), Target.Id, *Step->DefTag.ToString(), static_cast<int32>(Result),
			Context.RunSource.Id, Step->Overrides.Num());

		return ETcsStepResult::TSR_Completed;
	}
}

// 自注册（跨模块自注册先例 = `TcsTargeting` 的 SelectTargets / `TcsDamage` 的 ModifyFlow）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepApplyState, ExecuteStepApplyState)
