// Copyright Tirefly. All Rights Reserved.

#include "Flow/TcsFlowAttributes.h"



bool FTcsFlowAttributes::Submit(FGameplayTag Key, ETcsAttributeOp Op, const FTcsParamValue& Operand, const FTcsConsumePolicy& Consume)
{
	if (!ensureMsgf(Key.IsValid(), TEXT("FTcsFlowAttributes::Submit: 黑板键无效——拒绝提交")))
	{
		return false;
	}

	FTcsFlowAttributeSubmit& NewSubmit = Submits.FindOrAdd(Key).AddDefaulted_GetRef();
	NewSubmit.Op = Op;
	NewSubmit.Operand = Operand;
	NewSubmit.Consume = Consume;
	return true;
}

double FTcsFlowAttributes::Read(FGameplayTag Key, const FTcsParamEvaluateContext& EvalContext) const
{
	const TArray<FTcsFlowAttributeSubmit>* KeySubmits = Submits.Find(Key);
	if (!KeySubmits)
	{
		return 0.0;
	}

	// 读时求值（操作数保持提交态）→ 摊平成共享折叠器的条目形状
	TArray<FTcsAttributeBandEntry> Entries;
	Entries.Reserve(KeySubmits->Num());
	for (const FTcsFlowAttributeSubmit& Submit : *KeySubmits)
	{
		FTcsAttributeBandEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Op = Submit.Op;
		Entry.Value = Submit.Operand.Evaluate(EvalContext);

		// 流程域**无覆盖优先级概念** → 恒 0：覆盖带同优先级时按默认策略（取最大值）裁决数值。
		// **SortKey 不参与求值顺序**（D5-5 v3 / 本模块规格）——它只服务消耗裁决（Execute 步骤）
		Entry.OverridePriority = 0;
	}

	// **唯一实现**（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器三处共用，勿私建第二份）
	// 折叠初值 = 0（流程工作值的空集语义；作用域值由调用方以 Override 提交写入）
	return FoldTcsAttributeBands(0.0, Entries);
}

double FTcsFlowAttributes::Read(FGameplayTag Key) const
{
	return Read(Key, FTcsParamEvaluateContext());
}

void FTcsFlowAttributes::Reset()
{
	Submits.Reset();
}

const TArray<FTcsFlowAttributeSubmit>* FTcsFlowAttributes::FindSubmits(FGameplayTag Key) const
{
	return Submits.Find(Key);
}
