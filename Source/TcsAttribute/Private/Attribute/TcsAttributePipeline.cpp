// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributePipeline.h"

#include "Attribute/TcsAttributeChangedEvent.h"
#include "Attribute/TcsAttrModInstance.h"
#include "EventBus/TcsEventBusSubsystem.h"
#include "TcsAttributeLogChannel.h"
#include "TcsAttributeSubsystem.h"

#include "Engine/World.h"
#include "StructUtils/InstancedStruct.h"



// 管线内部常量
namespace
{
	// 变化判定阈值（D2-5 已定裁决：无实质变化不广播）
	constexpr double AttributeChangeEpsilon = 1e-5;

	// 重算收敛轮数的**安全网**（MUST NOT 作为环判据——环由 RegisterDependency 的 SCC 检测负责；
	// 此处只防"传播未收敛"这类实现缺陷；轮数上限与环判定解耦，修旧 TCS 8 轮误判深链为环的缺陷）
	constexpr int32 MaxFlushPasses = 64;
}



double FTcsAttributePipeline::EvaluateCurrent(
	FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Entity);
	if (!Store)
	{
		return 0.0;
	}

	FTcsAttributeInstance* Instance = Store->FindInstance(Attribute);
	if (!Instance)
	{
		return 0.0;
	}

	// 事务期读旧值（D2-5）：批内不重算，预览走 PeekPending
	const bool bInBatch = Store->BatchDepth > 0;
	if (!bInBatch && Instance->bDirty)
	{
		if (!PushEvalStack(Attribute))
		{
			// 环兜底：用缓存值终止递归（正常路径下环已在登记期被拒）
			return Instance->CachedCurrent;
		}

		Recalculate(Entity, *Store, *Instance, /*bInBatch=*/false);
		PopEvalStack(Attribute);
	}

	return Instance->CachedCurrent;
}

double FTcsAttributePipeline::PeekPending(
	FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Entity);
	if (!Store)
	{
		return 0.0;
	}

	FTcsAttributeInstance* Instance = Store->FindInstance(Attribute);
	if (!Instance)
	{
		return 0.0;
	}

	// 无进行中的批 → 与当前值一致；批内 → 只算不写（不落账、不标脏、不广播）
	if (Store->BatchDepth == 0)
	{
		return Instance->CachedCurrent;
	}

	return ComputeCandidate(Entity, *Store, *Instance);
}

bool FTcsAttributePipeline::ApplyModifier(
	FTcsCombatEntityHandle Entity, const FTcsAttrModInstance& Modifier)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Entity);
	if (!Store)
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("FTcsAttributePipeline::ApplyModifier: 实体未注册（Id=%lld）——忽略"),
			Entity.Id);
		return false;
	}

	FTcsAttributeInstance* Instance = Store->FindInstance(Modifier.Target);
	if (!Instance)
	{
		// 目标属性无实例（宿主动态删过属性）：忽略 + 留日志，不 ensure——
		// 框架允许动态增删且不做来源追溯（D2-14）
		UE_LOG(LogTcsAttribute, Log, TEXT("FTcsAttributePipeline::ApplyModifier: 目标属性无实例（实体 %llu，属性 %s）——忽略本次挂载"),
			Entity.Id, *Modifier.Target.GetTagName().ToString());
		return false;
	}

	Instance->AttrModInstances.Add(Modifier);
	Instance->bDirty = true;

	// 未开批 = 隐式批：立即重算 + 广播
	if (Store->BatchDepth == 0)
	{
		if (PushEvalStack(Instance->Attr))
		{
			Recalculate(Entity, *Store, *Instance, /*bInBatch=*/false);
			PopEvalStack(Instance->Attr);
		}
	}

	return true;
}

void FTcsAttributePipeline::BeginBatch(FTcsCombatEntityHandle Entity)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Entity);
	if (!ensureMsgf(Store != nullptr,
		TEXT("FTcsAttributePipeline::BeginBatch: 实体未注册（Id=%lld）"), Entity.Id))
	{
		return;
	}

	++Store->BatchDepth;
}

void FTcsAttributePipeline::Commit(FTcsCombatEntityHandle Entity)
{
	FTcsAttributeStore* Store = Owner.ResolveStore(Entity);
	if (!ensureMsgf(Store != nullptr,
		TEXT("FTcsAttributePipeline::Commit: 实体未注册（Id=%lld）"), Entity.Id))
	{
		return;
	}

	if (!ensureMsgf(Store->BatchDepth > 0,
		TEXT("FTcsAttributePipeline::Commit: 无对应的 BeginBatch（Id=%lld）"), Entity.Id))
	{
		return;
	}

	--Store->BatchDepth;

	// 内层提交不 flush（嵌套计数）；最外层提交才统一重算 + 广播
	if (Store->BatchDepth > 0)
	{
		return;
	}

	FlushDirty(Entity, *Store);
}

void FTcsAttributePipeline::FlushDirty(FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store)
{
	// 多轮扫描直到无脏（依赖传播可能在本轮后再标脏；环已在登记期被拒，收敛有界）
	for (int32 Pass = 0; Pass < MaxFlushPasses; ++Pass)
	{
		TArray<FGameplayTag> DirtyAttributes;
		for (const TPair<FGameplayTag, FTcsAttributeInstance>& Pair : Store.Attributes)
		{
			if (Pair.Value.bDirty)
			{
				DirtyAttributes.Add(Pair.Key);
			}
		}

		if (DirtyAttributes.Num() == 0)
		{
			return;
		}

		for (const FGameplayTag& Attribute : DirtyAttributes)
		{
			FTcsAttributeInstance* Instance = Store.FindInstance(Attribute);
			if (Instance && Instance->bDirty && PushEvalStack(Attribute))
			{
				Recalculate(Entity, Store, *Instance, /*bInBatch=*/false);
				PopEvalStack(Attribute);
			}
		}
	}

	UE_LOG(LogTcsAttribute, Warning,
		TEXT("FTcsAttributePipeline::FlushDirty: 重算未在 %d 轮内收敛（实体 %llu）——请检查依赖登记"), MaxFlushPasses, Entity.Id);
}

bool FTcsAttributePipeline::Recalculate(
	FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store, FTcsAttributeInstance& Instance, bool bInBatch)
{
	const double OldValue = Instance.CachedCurrent;
	const double NewValue = ComputeFoldedValue(Entity, Store, Instance);

	// 唯一写入点（缓存值的唯一生产者）
	Instance.CachedCurrent = NewValue;
	Instance.bDirty = false;

	const bool bChanged = !FMath::IsNearlyEqual(OldValue, NewValue, AttributeChangeEpsilon);
	if (bChanged)
	{
		MarkDependentsDirty(Store, Instance.Attr);

		if (!bInBatch)
		{
			BroadcastChange(Entity, Instance.Attr, OldValue, NewValue);
		}
	}

	return bChanged;
}

double FTcsAttributePipeline::ComputeCandidate(
	FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance)
{
	// 只算不写：候选值基于依赖的**当前缓存**（批内依赖也脏时以缓存值为准——预览用途足够）
	return ComputeFoldedValue(Entity, Store, Instance);
}

double FTcsAttributePipeline::ComputeFoldedValue(
	FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance)
{
	TArray<FTcsAttributeBandEntry> Entries;
	Entries.Reserve(Instance.AttrModInstances.Num());

	for (const FTcsAttrModInstance& Modifier : Instance.AttrModInstances)
	{
		double Value = Modifier.Operand.Literal;

		// 属性换算操作数（D2-11/D2-3）：收集时求值 + 读即登记（主属性变化自动把本属性标脏）
		if (Modifier.Operand.Kind == ETcsOperandKind::OPK_AttributeScaled &&
			Modifier.Operand.Attribute.IsValid())
		{
			RegisterDependency(Store, Modifier.Operand.Attribute, Instance.Attr);
			Value = Modifier.Operand.Coefficient * EvaluateCurrent(Entity, Modifier.Operand.Attribute);
		}

		FTcsAttributeBandEntry Entry;
		Entry.Op = Modifier.Op;
		Entry.Value = Value;
		Entry.OverridePriority = Modifier.OverridePriority;
		Entries.Add(Entry);
	}

	// 覆盖带的同优先级策略由**属性定义**给出（实例内已展开）——修正器侧只说强弱排座次
	const double Folded = FoldTcsAttributeBands(Instance.BaseValue, Entries, Instance.OverrideTieBreak);

	return ApplyValueDomain(Entity, Store, Instance, Folded);
}

double FTcsAttributePipeline::ApplyValueDomain(
	FTcsCombatEntityHandle Entity, FTcsAttributeStore& Store, const FTcsAttributeInstance& Instance, double Value)
{
	// 边界解析（ABM_Dynamic 先按管线求值——其依赖同样"读即登记"）
	bool bHasMin = false;
	bool bHasMax = false;
	double MinValue = 0.0;
	double MaxValue = 0.0;

	ResolveBound(Entity, Store, Instance.Attr, Instance.Bounds.Min, bHasMin, MinValue);
	ResolveBound(Entity, Store, Instance.Attr, Instance.Bounds.Max, bHasMax, MaxValue);

	switch (Instance.ValueDomain)
	{
	case ETcsAttributeValueDomain::AVD_Wrap:
		{
			// 按值域跨度循环回卷（两侧边界齐备且跨度为正才可循环；否则退化为原值）
			if (bHasMin && bHasMax && MaxValue > MinValue)
			{
				const double Span = MaxValue - MinValue;
				while (Value < MinValue)
				{
					Value += Span;
				}
				while (Value > MaxValue)
				{
					Value -= Span;
				}
			}
			return Value;
		}

	case ETcsAttributeValueDomain::AVD_Custom:
		// 逃逸位：值域策略接口（IValueDomainPolicy）不在 R3 范围——显式提示（ensure 每站点每进程一次）
		// + 每次命中留 Verbose 痕迹，然后按 Clamp 收口（确定性优先于未实现策略）
		ensureMsgf(false,
			TEXT("值域模式 AVD_Custom 的值域策略接口不在 R3 范围（属性 %s）——本次按 Clamp 收口"),
			*Instance.Attr.GetTagName().ToString());
		UE_LOG(LogTcsAttribute, Verbose, TEXT("AVD_Custom 未实现策略接口，按 Clamp 收口（属性 %s）"),
			*Instance.Attr.GetTagName().ToString());
		[[fallthrough]];

	case ETcsAttributeValueDomain::AVD_Clamp:
	default:
		if (bHasMin && Value < MinValue)
		{
			Value = MinValue;
		}
		if (bHasMax && Value > MaxValue)
		{
			Value = MaxValue;
		}
		return Value;
	}
}

void FTcsAttributePipeline::BroadcastChange(
	FTcsCombatEntityHandle Entity, const FGameplayTag& Attribute, double OldValue, double NewValue) const
{
	const UWorld* World = Owner.GetWorld();
	UTcsEventBusSubsystem* BusSubsystem = World ? World->GetSubsystem<UTcsEventBusSubsystem>() : nullptr;
	if (!BusSubsystem)
	{
		return;
	}

	FTcsAttributeChangedEvent Event;
	Event.Entity = Entity;
	Event.Attribute = Attribute;
	Event.OldValue = OldValue;
	Event.NewValue = NewValue;

	BusSubsystem->PublishImmediate(Tag_TcsEvent_Attribute_ValueChanged, FInstancedStruct::Make(Event));
}

bool FTcsAttributePipeline::PushEvalStack(const FGameplayTag& Attribute)
{
	if (EvalStack.Contains(Attribute))
	{
		// 求值链上重复出现同一属性 = 依赖成环（登记期已拒边，此处为兜底）
		ensureMsgf(false, TEXT("属性求值链上重复出现同一属性（%s）——依赖成环"), *Attribute.GetTagName().ToString());
		return false;
	}

	EvalStack.Add(Attribute);
	return true;
}

void FTcsAttributePipeline::PopEvalStack(const FGameplayTag& Attribute)
{
	EvalStack.RemoveSingleSwap(Attribute, EAllowShrinking::No);
}



void FTcsAttributePipeline::ResolveBound(
	FTcsCombatEntityHandle Entity,
	FTcsAttributeStore& Store,
	const FGameplayTag& ForAttribute,
	const FTcsAttributeBound& Bound,
	bool& bOutHasValue,
	double& OutValue)
{
	bOutHasValue = false;
	OutValue = 0.0;

	if (Bound.Mode == ETcsAttributeBoundMode::ABM_Static)
	{
		bOutHasValue = true;
		OutValue = Bound.StaticValue;
	}
	else if (Bound.Mode == ETcsAttributeBoundMode::ABM_Dynamic && Bound.DynamicAttribute.IsValid())
	{
		// 动态边界：先按管线求值（读即登记——边界属性变化同样把本属性标脏）
		RegisterDependency(Store, Bound.DynamicAttribute, ForAttribute);
		bOutHasValue = true;
		OutValue = EvaluateCurrent(Entity, Bound.DynamicAttribute);
	}
}
