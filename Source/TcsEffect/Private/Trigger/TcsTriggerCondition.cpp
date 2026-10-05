// Copyright Tirefly. All Rights Reserved.

#include "Trigger/TcsTriggerCondition.h"

#include "Attribute/TcsEffectAttributeAccess.h"
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

void FTcsTriggerConditionRegistry::Register(
	const UScriptStruct* ConditionStruct,
	FTcsTriggerConditionTest Test,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	ensureMsgf(ConditionStruct != nullptr, TEXT("FTcsTriggerConditionRegistry::Register: 条件类型为空——拒绝登记"));

	if (!ConditionStruct)
	{
		return;
	}

	// 拒绝门 = "同世界活对象重复"：既有条目**已失效**时可替换；**有效**（或静态自注册项）时拒绝保留首个
	if (Tests.Contains(ConditionStruct))
	{
		if (DiscardIfStale(ConditionStruct, LifetimeWorld))
		{
			UE_LOG(LogTcsEffect, Log,
				TEXT("FTcsTriggerConditionRegistry: 条件类型 %s 的既有动态登记已失效（旧世界残留）——替换为新登记"),
				*ConditionStruct->GetName());
		}
		else
		{
			ensureMsgf(false, TEXT("FTcsTriggerConditionRegistry::Register: 条件类型 %s 重复登记——保留首个登记"),
				*ConditionStruct->GetName());
			return;
		}
	}

	Tests.Add(ConditionStruct, MoveTemp(Test));
	RecordLifetime(ConditionStruct, LifetimeObject, LifetimeWorld);
}

bool FTcsTriggerConditionRegistry::Unregister(const UScriptStruct* ConditionStruct)
{
	if (!ConditionStruct)
	{
		return false;
	}

	// 静态自注册项是**代码**而非登记——MUST NOT 可移除（本入口只服务动态登记）
	if (!Lifetimes.Contains(ConditionStruct))
	{
		return false;
	}

	Lifetimes.Remove(ConditionStruct);
	Tests.Remove(ConditionStruct);
	return true;
}

TArray<const UScriptStruct*> FTcsTriggerConditionRegistry::GetDynamicKeys() const
{
	TArray<const UScriptStruct*> Keys;
	Keys.Reserve(Lifetimes.Num());

	for (const TPair<const UScriptStruct*, FTcsTriggerConditionLifetime>& Pair : Lifetimes)
	{
		Keys.Add(Pair.Key);
	}

	return Keys;
}

const FTcsTriggerConditionTest* FTcsTriggerConditionRegistry::Find(const UScriptStruct* ConditionStruct, const UWorld* World)
{
	if (!bPendingResolved)
	{
		ResolvePending();
	}

	if (!ConditionStruct)
	{
		return nullptr;
	}

	const FTcsTriggerConditionTest* Found = Tests.Find(ConditionStruct);
	if (!Found)
	{
		return nullptr;
	}

	// 寿命校验（只对动态登记项生效；内置条件全走静态自注册 ⇒ 不在 Lifetimes 表 ⇒ 永不过期）
	const FTcsTriggerConditionLifetime* Lifetime = Lifetimes.Find(ConditionStruct);
	if (Lifetime)
	{
		if (!Lifetime->Object.IsValid())
		{
			// 对象已被 GC：该条登记自然失效（MUST NOT 解引用）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsTriggerConditionRegistry: 条件类型 %s 的宿主求值器已被回收——该条登记视为失效"),
				*ConditionStruct->GetName());
			Lifetimes.Remove(ConditionStruct);
			Tests.Remove(ConditionStruct);
			return nullptr;
		}

		if (Lifetime->World.IsValid() && World && Lifetime->World.Get() != World)
		{
			// 跨世界：**可诊断**（MUST NOT 静默按"未登记"处理——那会把"世界已更换"表现成"条件类型写漏"）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("FTcsTriggerConditionRegistry: 条件类型 %s 的动态登记属另一世界（登记世界 %s ≠ 查询世界 %s）——该条登记视为失效"),
				*ConditionStruct->GetName(),
				*Lifetime->World->GetName(),
				*World->GetName());
			Lifetimes.Remove(ConditionStruct);
			Tests.Remove(ConditionStruct);
			return nullptr;
		}
	}

	return Found;
}

void FTcsTriggerConditionRegistry::RecordLifetime(
	const UScriptStruct* ConditionStruct,
	UObject* LifetimeObject,
	const UWorld* LifetimeWorld)
{
	if (!LifetimeObject && !LifetimeWorld)
	{
		// 纯 C++ 路径（无 UObject 寿命约束）——不建条目，该登记永不过期
		Lifetimes.Remove(ConditionStruct);
		return;
	}

	FTcsTriggerConditionLifetime& Lifetime = Lifetimes.FindOrAdd(ConditionStruct);
	Lifetime.Object = LifetimeObject;
	Lifetime.World = LifetimeWorld;
}

bool FTcsTriggerConditionRegistry::DiscardIfStale(const UScriptStruct* ConditionStruct, const UWorld* World)
{
	// 静态自注册项（不在 Lifetimes 表）不是"失效"——它是代码，MUST NOT 被替换
	const FTcsTriggerConditionLifetime* Lifetime = Lifetimes.Find(ConditionStruct);
	if (!Lifetime)
	{
		return false;
	}

	const bool bObjectCollected = !Lifetime->Object.IsValid();
	const bool bWorldGone = Lifetime->World.IsValid() == false;
	const bool bWorldMismatch = Lifetime->World.IsValid() && World && Lifetime->World.Get() != World;

	if (!bObjectCollected && !bWorldGone && !bWorldMismatch)
	{
		return false;
	}

	Lifetimes.Remove(ConditionStruct);
	Tests.Remove(ConditionStruct);
	return true;
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

	/**
	 * AttributeCompare（2026-10-05 R5 Task 6）：读上下文 `Caster` 的属性**当前值**后按方向比较。
	 *
	 * 两档失败面（见 `FTcsTriggerCondition_AttributeCompare` 头注释）：不可求值 ⇒ 静默不通过；
	 * **键在账本上不存在 ⇒ 不通过 + Warning**（`EvaluateCurrent` 对"键不存在"与"值为 0"同读数，
	 * 不区分就会让"键写错"表现成"按 0 比较"、可能与阈值比较后静默通过）。
	 *
	 * **依赖边界**：经 `FTcsEffectAttributeAccess`（TcsEffect 唯一属性门面取用处）——本条件是
	 * TcsEffect 使用**下层**领域模块 `TcsAttribute` 的第二个真实消费者（第一个是 `ModifyAttribute` 步骤）。
	 */
	bool TestTriggerConditionAttributeCompare(const FInstancedStruct& ConditionData, const FTcsTriggerContext& Context, double /*RandomValue*/)
	{
		const FTcsTriggerCondition_AttributeCompare* Condition = ConditionData.GetPtr<FTcsTriggerCondition_AttributeCompare>();
		if (!Condition || !Condition->Attribute.IsValid() || !Context.Caster.IsValid())
		{
			// 未配置 / 主体未知 = 不可求值：不通过、零红字（"条件未过不是故障"口径）
			return false;
		}

		const FTcsEffectAttributeAccess Access = FTcsEffectAttributeAccess::Resolve(Context.World);
		if (!Access.IsValid() || !Access.IsLedgerReady(Context.Caster))
		{
			// 无世界读数 / 该单位没有属性账本：同上——不通过、零红字
			return false;
		}

		if (!Access.HasAttribute(Context.Caster, Condition->Attribute))
		{
			// 键写错 = 配置错误：**不静默按 0 比较**（与"未注册条件类型 ⇒ 不过 + Warning"同款 fail-closed）
			UE_LOG(LogTcsEffect, Warning,
				TEXT("触发条件 AttributeCompare：属性 %s 在该单位账本上不存在——按不通过处理（单位=%lld）"),
				*Condition->Attribute.ToString(), Context.Caster.Id);
			return false;
		}

		const double Current = Access.EvaluateCurrent(Context.Caster, Condition->Attribute);

		switch (Condition->Comparison)
		{
		case ETcsAttributeComparison::EAC_Greater:
			return Current > Condition->Threshold;

		case ETcsAttributeComparison::EAC_Less:
			return Current < Condition->Threshold;

		case ETcsAttributeComparison::EAC_LessOrEqual:
			return Current <= Condition->Threshold;

		case ETcsAttributeComparison::EAC_GreaterOrEqual:
		default:
			return Current >= Condition->Threshold;
		}
	}
}

UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_HasAllTags, TestTriggerConditionHasAllTags)
UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_Chance, TestTriggerConditionChance)
UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(FTcsTriggerCondition_AttributeCompare, TestTriggerConditionAttributeCompare)



// ===== 求值助手 =====

bool EvaluateTriggerConditions(
	const TArray<FInstancedStruct>& Conditions,
	const FTcsTriggerContext& Context,
	double RandomValue,
	const UWorld* World)
{
	for (const FInstancedStruct& ConditionStruct : Conditions)
	{
		// 空项跳过（数组里可能有未初始化的槽位——不算"条件未过"，也不是错误）
		if (!ConditionStruct.IsValid())
		{
			continue;
		}

		const UScriptStruct* ConditionType = ConditionStruct.GetScriptStruct();
		const FTcsTriggerConditionTest* Test = FTcsTriggerConditionRegistry::Get().Find(ConditionType, World);

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
