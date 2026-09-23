// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerCondition.h"

#include "TcsEffectLogChannel.h"



// ===== 注册表 =====

FTcsTriggerConditionRegistrar::FTcsTriggerConditionRegistrar(
	UScriptStruct* (*InConditionStructGetter)(),
	FTcsTriggerConditionTest InTest)
{
	FTcsTriggerConditionEntry Entry;
	Entry.GetConditionStruct = InConditionStructGetter;
	Entry.Test = MoveTemp(InTest);

	FTcsTriggerConditionRegistry::Get().AddPending(MoveTemp(Entry));
}

FTcsTriggerConditionRegistry& FTcsTriggerConditionRegistry::Get()
{
	static FTcsTriggerConditionRegistry Registry;
	return Registry;
}

void FTcsTriggerConditionRegistry::AddPending(FTcsTriggerConditionEntry Entry)
{
	// 静态初始化期调用：只入待解析表，**不调用 getter**（那时 UObject 设施未就绪）
	PendingEntries.Add(MoveTemp(Entry));
}

void FTcsTriggerConditionRegistry::Register(const UScriptStruct* ConditionStruct, FTcsTriggerConditionTest Test)
{
	ensureMsgf(ConditionStruct != nullptr, TEXT("FTcsTriggerConditionRegistry::Register: 条件类型为空——拒绝登记"));

	if (!ConditionStruct)
	{
		return;
	}

	if (Tests.Contains(ConditionStruct))
	{
		ensureMsgf(false, TEXT("FTcsTriggerConditionRegistry::Register: 条件类型 %s 重复登记——保留首个登记"),
			*ConditionStruct->GetName());
		return;
	}

	Tests.Add(ConditionStruct, MoveTemp(Test));
}

const FTcsTriggerConditionTest* FTcsTriggerConditionRegistry::Find(const UScriptStruct* ConditionStruct)
{
	if (!bPendingResolved)
	{
		ResolvePending();
	}

	return ConditionStruct ? Tests.Find(ConditionStruct) : nullptr;
}

void FTcsTriggerConditionRegistry::ResolvePending()
{
	// 幂等：只跑一次（此后 Register 直接进 Tests）
	bPendingResolved = true;

	for (FTcsTriggerConditionEntry& Entry : PendingEntries)
	{
		// 此刻才调用 getter（静态初始化期已过，UObject 设施就绪）
		UScriptStruct* ConditionStruct = Entry.GetConditionStruct ? Entry.GetConditionStruct() : nullptr;
		if (!ConditionStruct)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("FTcsTriggerConditionRegistry: 待解析项的类型 getter 返回空——跳过"));
			continue;
		}

		if (Tests.Contains(ConditionStruct))
		{
			ensureMsgf(false, TEXT("FTcsTriggerConditionRegistry: 条件类型 %s 重复登记（静态自注册）——保留首个"),
				*ConditionStruct->GetName());
			continue;
		}

		Tests.Add(ConditionStruct, MoveTemp(Entry.Test));
	}

	PendingEntries.Reset();
}



// ===== 内置条件求值器 =====

namespace
{
	// HasAllTags：上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）
	bool TestTriggerConditionHasAllTags(const FInstancedStruct& ConditionData, const FTcsTriggerContext& Context, double /*RandomValue*/)
	{
		const FTcsTriggerCondition_HasAllTags* Condition = ConditionData.GetPtr<FTcsTriggerCondition_HasAllTags>();
		if (!Condition)
		{
			return false;
		}

		for (const FGameplayTag& RequiredTag : Condition->Tags)
		{
			if (!Context.ClassificationTags.Contains(RequiredTag))
			{
				return false;
			}
		}

		return true;
	}

	// Chance：随机值**由调用方注入**（D0-1——本函数 MUST NOT 取随机数）
	bool TestTriggerConditionChance(const FInstancedStruct& ConditionData, const FTcsTriggerContext& /*Context*/, double RandomValue)
	{
		const FTcsTriggerCondition_Chance* Condition = ConditionData.GetPtr<FTcsTriggerCondition_Chance>();
		if (!Condition)
		{
			return false;
		}

		return RandomValue < Condition->Probability;
	}
}

UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_HasAllTags, TestTriggerConditionHasAllTags)
UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_Chance, TestTriggerConditionChance)



// ===== 求值助手 =====

bool EvaluateTriggerConditions(
	const TArray<FInstancedStruct>& Conditions,
	const FTcsTriggerContext& Context,
	double RandomValue)
{
	for (const FInstancedStruct& ConditionStruct : Conditions)
	{
		// 空项跳过（数组里可能有未初始化的槽位——不算"条件未过"，也不是错误）
		if (!ConditionStruct.IsValid())
		{
			continue;
		}

		const UScriptStruct* ConditionType = ConditionStruct.GetScriptStruct();
		const FTcsTriggerConditionTest* Test = FTcsTriggerConditionRegistry::Get().Find(ConditionType);

		// 未注册类型：不静默通过（静默会让"条件类型未注册/写错"表现成"条件通过"）
		if (!Test)
		{
			UE_LOG(LogTcsEffect, Warning, TEXT("EvaluateTriggerConditions: 条件类型 %s 未注册求值器——按不通过处理"),
				*GetNameSafe(ConditionType));
			return false;
		}

		if (!(*Test)(ConditionStruct, Context, RandomValue))
		{
			return false;
		}
	}

	return true;
}
