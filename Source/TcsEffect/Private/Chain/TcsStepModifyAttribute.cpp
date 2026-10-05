// Copyright Tirefly. All Rights Reserved.

#include "Chain/TcsStepModifyAttribute.h"

#include "Attribute/TcsEffectAttributeAccess.h"
#include "Chain/TcsEffectStepExecutor.h"
#include "Engine/World.h"
#include "TcsEffectLogChannel.h"
#include "TcsEffectSubsystem.h"



namespace
{
	/**
	 * ModifyAttribute 链原语执行器（D4-16 十五原语之一，属 TcsEffect）。
	 *
	 * 流水：解析目标（步骤字段优先，退黑板目标集首个）→ 求值运算数 → 装一条常驻账本条目
	 * （`Source` = 本次运行的来源锚点）→ 开批挂载 → 提交（属性面唯一提交点）。
	 *
	 * 全部失败面按完成处理（不断链）；`Warning` 只留给"配置写错"（键无效 / 载荷类型不符），
	 * "该单位没有属性账本"是配置状态 ⇒ `Log` 级、零红字。
	 */
	ETcsStepResult ExecuteStepModifyAttribute(const FInstancedStruct& StepData, FTcsEffectContext& Context, FTcsChainRun& Run)
	{
		const FTcsStepModifyAttribute* Step = StepData.GetPtr<FTcsStepModifyAttribute>();
		if (!Step)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("ModifyAttribute[%s]: 步骤载荷类型不符——本步按完成处理"),
				*Run.ChainId.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 目标：步骤字段优先，无效则取黑板目标集首个（D4-4 v2 的"默认目标"语义）
		FTcsCombatEntityHandle Target = Step->Target;
		if (!Target.IsValid() && Context.Targets.Num() > 0)
		{
			Target = Context.Targets[0];
		}

		if (!Step->Attribute.IsValid() || !Target.IsValid())
		{
			// 未配置：跳过写入但留痕——"写不进去"与"写进去的值不对"必须可区分
			UE_LOG(LogTcsEffect, Warning, TEXT("ModifyAttribute[%s]: %s——跳过写入（本步按完成处理）"),
				*Run.ChainId.ToString(),
				!Step->Attribute.IsValid() ? TEXT("Attribute 无效") : TEXT("目标无效且黑板目标集为空"));
			return ETcsStepResult::TSR_Completed;
		}

		const UTcsEffectSubsystem* Owner = Run.Owner.Get();
		const UWorld* World = Owner ? Owner->GetWorld() : nullptr;

		const FTcsEffectAttributeAccess Access = FTcsEffectAttributeAccess::Resolve(World);
		if (!Access.IsValid() || !Access.IsLedgerReady(Target))
		{
			// 链与属性是两套登记：链可以作用在没有属性账本的单位上（合法）⇒ 配置状态，不是错误
			UE_LOG(LogTcsEffect, Log, TEXT("ModifyAttribute[%s]: 单位无属性账本——跳过（单位=%lld 属性=%s）"),
				*Run.ChainId.ToString(), Target.Id, *Step->Attribute.ToString());
			return ETcsStepResult::TSR_Completed;
		}

		// 数值来源与提交可进入宿主/事件回调；不跨这些调用保留运行态或步骤定义引用。
		const FGameplayTag ChainId = Run.ChainId;
		const FTcsSourceHandle RunSource = Context.RunSource;
		const FTcsCombatEntityHandle Instigator = Context.Instigator;
		const FTcsStepModifyAttribute StepSnapshot = *Step;

		// 运算数求值：链侧无参数表（引用类源落兜底）；主体取目标、发起者取黑板
		FTcsParamEvaluateContext EvalContext;
		EvalContext.Subject = Target;
		EvalContext.Instigator = Instigator;
		const double Value = StepSnapshot.Operand.Evaluate(EvalContext);

		// 一条常驻账本条目（`Source` = 本次运行的来源锚点 ⇒ 同一次运行挂上的条目可按它回收）
		FTcsAttrModInstance Modifier;
		Modifier.Target = StepSnapshot.Attribute;
		Modifier.Op = StepSnapshot.Op;
		Modifier.Operand.Kind = ETcsOperandKind::OPK_Literal;
		Modifier.Operand.Literal = Value;
		Modifier.Source = RunSource;

		// 单条也开批：批内只标脏，最外层 `Commit` 才重算 + 广播（属性面的唯一提交点纪律）
		Access.BeginBatch(Target);
		const bool bApplied = Access.ApplyModifier(Target, Modifier);
		Access.Commit(Target);

		UE_LOG(LogTcsEffect, Log, TEXT("ModifyAttribute[%s]: 单位=%lld 属性=%s 带=%d 值=%.6f 来源=%llu 结果=%s"),
			*ChainId.ToString(), Target.Id, *StepSnapshot.Attribute.ToString(), static_cast<int32>(StepSnapshot.Op),
			Value, RunSource.Id, bApplied ? TEXT("已挂载") : TEXT("被拒"));

		return ETcsStepResult::TSR_Completed;
	}
}

// 自注册（本模块内；机制层不认识本类型，本文件自己把执行器喂进注册表）
UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepModifyAttribute, ExecuteStepModifyAttribute)
