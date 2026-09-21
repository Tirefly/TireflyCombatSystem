// Copyright Tirefly. All Rights Reserved.

#include "Flow/Steps/TcsFlowSteps.h"

#include "EventBus/TcsEventBusSubsystem.h"
#include "Flow/Steps/TcsFlowDataSteps.h"
#include "Flow/TcsDamageFlowCollectEvent.h"
#include "Flow/TcsFlowStepConditions.h"
#include "Flow/TcsFlowStepExecutor.h"
#include "Parameter/TcsParamSource_Literal.h"
#include "TcsDamageLogChannel.h"
#include "TcsDamageSubsystem.h"



// ===== 其余六步 + 两个数据步骤的执行器（默认模板不组装；宿主/项目按需启用）=====
namespace
{
	// 黑板契约键（标准步骤库认识；**本文件与 Core 文件在 unity build 下可能同 TU——符号带本文件前缀**）
	const FName TcsFlowRestKey_HitRate(TEXT("HitRate"));
	const FName TcsFlowRestKey_CritRate(TEXT("CritRate"));

	UTcsDamageSubsystem* ResolveFlowRestOwner(const FTcsDamageFlowContext& Context)
	{
		return Context.Owner.Get();
	}

	// 以 **Add** 提交输入/基值类；以 **Override** 提交结果类（见 TcsFlowStepsCore.cpp 的语义说明）
	void SubmitFlowRestAdd(FTcsFlowAttributes& Blackboard, FName Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Add, Operand);
	}

	void SubmitFlowRestOverride(FTcsFlowAttributes& Blackboard, FName Key, double Value)
	{
		FTcsParamValue Operand;
		Operand.Source.GetMutable<FTcsParamSource_Literal>().Value = Value;
		Blackboard.Submit(Key, ETcsAttributeOp::TAO_Override, Operand);
	}

	// 广播一个收集事件（步骤边界——响应方在返回前提交，收集 ≠ 消费）
	void BroadcastFlowRestCollect(const FTcsDamageFlowContext& Context, FGameplayTag Tag)
	{
		if (UTcsDamageSubsystem* Owner = ResolveFlowRestOwner(Context))
		{
			Owner->PublishCollectEvent(Tag, const_cast<FTcsDamageFlowContext&>(Context));
		}
	}

	// —— ② PreHit ——
	bool ExecuteFlowPreHit(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowPreHit* Step = StepData.GetPtr<FTcsFlowPreHit>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_PreHit);
		return true;
	}

	// —— ③ Hit：基础命中率 → 修改器 → 判定 ——
	bool ExecuteFlowHit(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowHit* Step = StepData.GetPtr<FTcsFlowHit>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		double HitRate = 1.0;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			const FTcsCombatEntityHandle Target = Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle();
			HitRate = Step->Delegate->GetBaseHitRate(Context.Attacker, Target, Context);
		}

		// 修改器可在收集事件后改写 `HitRate` 键（宿主挂点）
		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_Hit);
		const double Modified = Context.Blackboard.Read(TEXT("HitRate")) > 0.0
			? Context.Blackboard.Read(TEXT("HitRate"))
			: HitRate;

		SubmitFlowRestOverride(Context.Blackboard, FName(TEXT("Hit")), Modified >= 1.0 ? 1.0 : 0.0);
		return true;
	}

	// —— ④ Crit ——
	bool ExecuteFlowCrit(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowCrit* Step = StepData.GetPtr<FTcsFlowCrit>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		double CritRate = 0.0;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			const FTcsCombatEntityHandle Target = Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle();
			CritRate = Step->Delegate->GetBaseCritRate(Context.Attacker, Target, Context);
		}

		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_Crit);
		const double Modified = Context.Blackboard.Read(TEXT("CritRate")) > 0.0
			? Context.Blackboard.Read(TEXT("CritRate"))
			: CritRate;

		SubmitFlowRestOverride(Context.Blackboard, FName(TEXT("Crit")), Modified > 0.0 ? 1.0 : 0.0);
		return true;
	}

	// —— ⑤ Element：delegate 解析元素 → 写分类 Tag 集 ——
	bool ExecuteFlowElement(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowElement* Step = StepData.GetPtr<FTcsFlowElement>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		if (Step->Delegate && Step->Delegate.GetObject())
		{
			const FTcsCombatEntityHandle Target = Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle();
			const FGameplayTag Element = Step->Delegate->ResolveElement(Context.Attacker, Target, Context);
			if (Element.IsValid())
			{
				Context.ClassificationTags.AddUnique(Element);
			}
		}

		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_Element);
		return true;
	}

	// —— ⑦ AfterDamage ——
	bool ExecuteFlowAfterDamage(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowAfterDamage* Step = StepData.GetPtr<FTcsFlowAfterDamage>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_AfterDamage);
		return true;
	}

	// —— ⑧ PreExecute：发收集事件（只收集不消费）——
	bool ExecuteFlowPreExecute(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowPreExecute* Step = StepData.GetPtr<FTcsFlowPreExecute>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		// 候选（免疫/减伤）落进 `Step->CandidateKey`——⑨ 按 SortKey 裁决、成功才消费
		BroadcastFlowRestCollect(Context, Tag_Tcs_Event_Damage_PreExecute);
		return true;
	}

	// —— 数据步骤：FlowModify ——
	bool ExecuteFlowModify(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowModify* Step = StepData.GetPtr<FTcsFlowModify>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		if (Step->TargetKey.IsNone())
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("FlowModify: TargetKey 为空——跳过（不静默写错键）"));
			return true;
		}

		Context.Blackboard.Submit(Step->TargetKey, Step->Op, Step->Operand);
		return true;
	}

	// —— 数据步骤：FlowDelegate ——
	bool ExecuteFlowDelegate(const FInstancedStruct& StepData, FTcsDamageFlowContext& Context)
	{
		const FTcsFlowDelegate* Step = StepData.GetPtr<FTcsFlowDelegate>();
		if (!Step || !ShouldRunFlowStep(Step->Conditions, Context))
		{
			return true;
		}

		if (Step->TargetKey.IsNone())
		{
			UE_LOG(LogTcsDamage, Warning, TEXT("FlowDelegate: TargetKey 为空——跳过"));
			return true;
		}

		double Value = 0.0;
		if (Step->Delegate && Step->Delegate.GetObject())
		{
			const FTcsCombatEntityHandle Target = Context.Targets.Num() > 0 ? Context.Targets[0] : FTcsCombatEntityHandle();
			Value = Step->Delegate->CalculateBaseDamage(Context.Blackboard.Read(Step->TargetKey), Context.Attacker, Target, Context);
		}

		SubmitFlowRestOverride(Context.Blackboard, Step->TargetKey, Value);
		return true;
	}
}

UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowPreHit, ExecuteFlowPreHit)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowHit, ExecuteFlowHit)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowCrit, ExecuteFlowCrit)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowElement, ExecuteFlowElement)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowAfterDamage, ExecuteFlowAfterDamage)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowPreExecute, ExecuteFlowPreExecute)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowModify, ExecuteFlowModify)
UE_DEFINE_FLOW_STEP_EXECUTOR(FTcsFlowDelegate, ExecuteFlowDelegate)
