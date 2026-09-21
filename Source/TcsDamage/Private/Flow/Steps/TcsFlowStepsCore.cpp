// Copyright Tirefly. All Rights Reserved.

#include "Flow/Steps/TcsFlowSteps.h"

#include "Clock/TcsClockSubsystem.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsDamageRecord.h"
#include "Flow/TcsFlowStepConditions.h"
#include "Flow/TcsFlowStepExecutor.h"
#include "Parameter/TcsParamSource_Literal.h"
#include "TcsAttributeSubsystem.h"
#include "TcsDamageLogChannel.h"
#include "TcsDamageSubsystem.h"



// ===== 核心四步执行器（默认模板所用；其余六步与两个数据步骤见 TcsFlowStepsRest.cpp）=====
namespace
{
	// 黑板保留键（标准步骤库的契约键名——M8 校验/Explain 认识；项目自定义键自由 FName）
	// **伤害量只有一个契约键 `BaseDamage`**：基值以 Add 落在它上面、收集到的 PercentAdd/Mul 也叠在它上面
	// （不再有 `FinalDamage` 键——记录里的"最终值"直接读该键的折叠结果）
	const FName TcsFlowKey_BaseDamage(TEXT("BaseDamage"));
	const FName TcsFlowKey_Executed(TEXT("Executed"));
	const FName TcsFlowKey_Absorbed(TEXT("Absorbed"));
	const FName TcsFlowKey_Kill(TEXT("Kill"));

	// 取宿主门面（步骤取世界/子系统的唯一通路——句柄化后上下文里没有 Actor 可借道）
	UTcsDamageSubsystem* ResolveFlowOwner(const FTcsDamageFlowContext& Context)
	{
		return Context.Owner.Get();
	}

	// 取属性门面（M2）
	UTcsAttributeSubsystem* ResolveAttributeSubsystem(const FTcsDamageFlowContext& Context)
	{
		UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context);
		UWorld* World = Owner ? Owner->GetWorld() : nullptr;
		return World ? World->GetSubsystem<UTcsAttributeSubsystem>() : nullptr;
	}

	// 黑板写入两个口（语义分离）：
	// - **输入/基值类走 `Add`**：折叠式 `(0 + ΣAdd) × (1 + ΣPercentAdd) × ΠMul + ΣFlatAdd` 里
	//   "基值"天然就是 `ΣAdd` 的一员（与 M2 属性"基础值 + Add 带"同式同值）。
	//   **MUST NOT 用覆盖带**——覆盖带盖掉一切，会把收集到的修正（如"受火伤 +20%"）整笔抹掉。
	// - **结果类走 `Override`**：`Executed`/`Absorbed`/`Kill` 是本次执行的事实值，不含修正带语义。
	void SubmitAddValue(FTcsFlowAttributes& Blackboard, FName Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Add, Operand);
	}

	void SubmitOverrideValue(FTcsFlowAttributes& Blackboard, FName Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Override, Operand);
	}

	// —— ① CollectStart：发流程开始事件 + 重置收集 ——
	bool ExecuteFlowCollectStart(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowCollectStart* Step = StepData.GetPtr<FTcsFlowCollectStart>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		Context.Blackboard.Reset();

		if (UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context))
		{
			Owner->PublishCollectEvent(Tag_TcsEvent_Damage_FlowStarted, Context);
		}
		return true;
	}

	// —— ⑥ BaseDamage：接收输入值 → delegate 逃生口 → 写黑板（流程零计算）——
	bool ExecuteFlowBaseDamage(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowBaseDamage* Step = StepData.GetPtr<FTcsFlowBaseDamage>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		// 输入值来自**上下文请求字段**（链步骤解算结果）——不是黑板键：
		// 黑板会被 `CollectStart` 重置（收集语义），而输入是"请求"的一部分、不随收集清除；
		// 若从黑板读输入，链侧预写 + 本步提交会造成**重复计数**（实测：10 + 20 = 30）。
		const double IncomingBase = Context.BaseDamageInput;

		double BaseDamage = IncomingBase;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			// 降级逃生口：宿主特殊公式（默认实现即原样返回）
			BaseDamage = Step->Delegate->CalculateBaseDamage(IncomingBase, Context.Attacker, Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle(), Context);
		}

		// 基值以 **Add** 提交（见上：基值 = ΣAdd 的一员；后续收集到的 PercentAdd/Mul 自然叠在它上面）
		SubmitAddValue(Context.Blackboard, Step->OutputKey, BaseDamage);
		return true;
	}

	// —— ⑨ Execute：裁决 → 护盾 hook → M2 事务扣血 → 成功才消费 ——
	bool ExecuteFlowExecute(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowExecute* Step = StepData.GetPtr<FTcsFlowExecute>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		const double Candidate = Context.Blackboard.Read(Step->DamageKey);

		// 裁决：候选中按消耗策略的 SortKey 选一（收集 ≠ 消费——未被选中者完全不动）
		int32 BestIndex = INDEX_NONE;
		int32 BestSortKey = TNumericLimits<int32>::Lowest();
		if (const TArray<FTcsFlowAttributeSubmit>* Submits = Context.Blackboard.FindSubmits(Step->CandidateKey))
		{
			for (int32 Index = 0; Index < Submits->Num(); ++Index)
			{
				if ((*Submits)[Index].Consume.SortKey > BestSortKey)
				{
					BestSortKey = (*Submits)[Index].Consume.SortKey;
					BestIndex = Index;
				}
			}
		}

		// 护盾 hook（宿主；默认 0 = 无护盾）
		double Absorbed = 0.0;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			for (const FTcsCombatEntityHandle& Target : Context.Targets)
			{
				Absorbed += Step->Delegate->ModifyShield(Target, Candidate, Context);
			}
		}

		const double Executed = FMath::Max(0.0, Candidate - Absorbed);

		// M2 事务扣血：**改基值**（伤害是"生命被削减"，不是可被来源级联撤销的修正器——见 plan2 Task 4 注记）
		// 属性键解析：**步骤级 AttrKey 优先**（模板可覆盖）；否则用调用方在上下文里指定的请求键
		// （插件组装的官方默认模板不可能知道项目词表——`Health` 是项目侧的）
		const FName ResolvedAttrKey = !Step->AttrKey.IsNone() ? Step->AttrKey : Context.TargetAttrKey.Name;

		bool bApplied = false;
		if (UTcsAttributeSubsystem* AttributeSubsystem = ResolveAttributeSubsystem(Context))
		{
			if (!ResolvedAttrKey.IsNone())
			{
				for (const FTcsCombatEntityHandle& Target : Context.Targets)
				{
					const FTcsAttributeName Attribute(ResolvedAttrKey);

					AttributeSubsystem->BeginBatch(Target);
					const double Current = AttributeSubsystem->EvaluateCurrent(Target, Attribute);
					const double NewValue = FMath::Max(0.0, Current - Executed);
					AttributeSubsystem->SetBaseValue(Target, Attribute, NewValue);
					AttributeSubsystem->Commit(Target);
					bApplied = true;
				}
			}
			else
			{
				UE_LOG(LogTcsDamage, Warning, TEXT("FlowExecute: 步骤未配 AttrKey 且上下文未指定 TargetAttrKey——本次不扣血（仅记录）"));
			}
		}

		SubmitOverrideValue(Context.Blackboard, TcsFlowKey_Executed, bApplied ? Executed : 0.0);
		SubmitOverrideValue(Context.Blackboard, TcsFlowKey_Absorbed, Absorbed);

		// 记账语义：本次事务提交后目标生命 ≤ 0（死亡规则仍归宿主）
		double bKill = 0.0;
		if (UTcsAttributeSubsystem* AttributeSubsystem = ResolveAttributeSubsystem(Context))
		{
			if (!ResolvedAttrKey.IsNone())
			{
				for (const FTcsCombatEntityHandle& Target : Context.Targets)
				{
					if (AttributeSubsystem->EvaluateCurrent(Target, FTcsAttributeName(ResolvedAttrKey)) <= 0.0)
					{
						bKill = 1.0;
						break;
					}
				}
			}
		}
		SubmitOverrideValue(Context.Blackboard, TcsFlowKey_Kill, bKill);

		return true;
	}

	// —— ⑩ Completed：组装并发布伤害记录（每目标一条）——
	bool ExecuteFlowCompleted(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowCompleted* Step = StepData.GetPtr<FTcsFlowCompleted>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		UTcsDamageSubsystem* Owner = ResolveFlowOwner(Context);
		if (!Owner)
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("FlowCompleted: 门面不可得——记录未产出"));
			return true;
		}

		const UTcsClockSubsystem* Clock = Owner->GetWorld() ? Owner->GetWorld()->GetSubsystem<UTcsClockSubsystem>() : nullptr;
		const double Now = Clock ? Clock->GetClock().Elapsed : 0.0;

		for (const FTcsCombatEntityHandle& Target : Context.Targets)
		{
			FTcsDamageRecord Record;
			Record.FlowId = Context.FlowSource.Id;
			Record.Source = Context.Attacker;
			Record.Target = Target;
			Record.Element = FGameplayTag();
			Record.bHit = true;
			Record.bCrit = Context.Blackboard.Read(TEXT("Crit")) > 0.0;
			// Base = 调用方输入（原始解算结果）；Final = 该键折叠后的最终值（基值 + 收集到的修正）
			Record.Base = Context.BaseDamageInput;
			Record.Final = Context.Blackboard.Read(TcsFlowKey_BaseDamage);
			Record.Executed = Context.Blackboard.Read(TcsFlowKey_Executed);
			Record.Absorbed = Context.Blackboard.Read(TcsFlowKey_Absorbed);
			Record.bKill = Context.Blackboard.Read(TcsFlowKey_Kill) > 0.0;
			Record.Timestamp = Now;

			Owner->AppendRecord(Record);
		}

		// 完成收集事件（宿主挂点：结算后表现/统计——记录事件在 AppendRecord 内已发）
		if (UTcsDamageSubsystem* CompletedOwner = ResolveFlowOwner(Context))
		{
			CompletedOwner->PublishCollectEvent(Tag_TcsEvent_Damage_Completed, Context);
		}

		return true;
	}
}

UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowCollectStart, ExecuteFlowCollectStart)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowBaseDamage, ExecuteFlowBaseDamage)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowExecute, ExecuteFlowExecute)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowCompleted, ExecuteFlowCompleted)
