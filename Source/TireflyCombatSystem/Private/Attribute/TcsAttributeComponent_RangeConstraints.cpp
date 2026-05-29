// Copyright Tirefly. All Rights Reserved.


#include "Attribute/TcsAttributeComponent.h"

#include "TcsLogChannels.h"
#include "Attribute/TcsAttributeDefinition.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampContext.h"
#include "Attribute/AttrClampStrategy/TcsAttributeClampStrategy.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"


void UTcsAttributeComponent::ClampAttributeValueInRange(
	const FName& AttributeName,
	float& NewValue,
	float* OutMinValue,
	float* OutMaxValue,
	const TMap<FName, float>* WorkingValues)
{
	const FTcsAttributeInstance* Attribute = Attributes.Find(AttributeName);
	if (!Attribute)
	{
		return;
	}
	const FTcsAttributeRange& Range = Attribute->AttributeDef->AttributeRange;

	// 先解析最小边界。若传播过程传入了 WorkingValues，则优先读取工作集里的候选值，
	// 这样同一轮 fixpoint 可以看到“尚未提交到组件但已经在本轮推导出的新值”。
	float MinValue = TNumericLimits<float>::Lowest();
	switch (Range.MinValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MinValue = Range.MinValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MinValueAttribute))
				{
					MinValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MinValueAttribute, MinValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MinValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MinValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 最大边界与最小边界同理，也优先读取工作集，保证传播阶段的依赖解析基于最新候选状态。
	float MaxValue = TNumericLimits<float>::Max();
	switch (Range.MaxValueType)
	{
	case ETcsAttributeRangeType::ART_None:
		break;
	case ETcsAttributeRangeType::ART_Static:
		MaxValue = Range.MaxValue;
		break;
	case ETcsAttributeRangeType::ART_Dynamic:
		{
			bool bResolved = false;
			if (WorkingValues)
			{
				if (const float* Value = WorkingValues->Find(Range.MaxValueAttribute))
				{
					MaxValue = *Value;
					bResolved = true;
				}
			}

			if (!bResolved && !GetAttributeValue(Range.MaxValueAttribute, MaxValue))
			{
				UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] Owner %s has no attribute named of %s as Attribute %s MaxValueAttribute"),
					*FString(__FUNCTION__),
					GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
					*Range.MaxValueAttribute.ToString(),
					*AttributeName.ToString());
			}
			break;
		}
	}

	// 到这里 Min/Max 已经解析完毕，真正“如何在范围内修正”的策略交给 ClampStrategy。
	// 依赖声明属于 CollectDependentAttributes 的职责，不在这里推导。
	TSubclassOf<UTcsAttributeClampStrategy> StrategyClass = Attribute->AttributeDef->ClampStrategyClass;
	if (StrategyClass)
	{
		UTcsAttributeClampStrategy* StrategyCDO = StrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();

		FTcsAttributeClampContextBase Context(
			this,
			AttributeName,
			Attribute->AttributeDef,
			Attribute,
			WorkingValues
		);

		const FInstancedStruct& Config = Attribute->AttributeDef->ClampStrategyConfig;

		NewValue = StrategyCDO->Clamp(NewValue, MinValue, MaxValue, Context, Config);

		UE_LOG(LogTcsAttribute, Verbose, TEXT("[%s] Attribute %s clamped using strategy %s: Value=%f, Min=%f, Max=%f"),
			*FString(__FUNCTION__),
			*AttributeName.ToString(),
			*StrategyClass->GetName(),
			NewValue,
			MinValue,
			MaxValue);
	}
	else
	{
		UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] ClampStrategyClass is null for attribute %s, using FMath::Clamp as fallback"),
			*FString(__FUNCTION__),
			*AttributeName.ToString());
		NewValue = FMath::Clamp(NewValue, MinValue, MaxValue);
	}

	if (OutMinValue)
	{
		*OutMinValue = MinValue;
	}
	if (OutMaxValue)
	{
		*OutMaxValue = MaxValue;
	}
}

bool UTcsAttributeComponent::TryBuildDeclaredRangeConstraintDependents(
	TMap<FName, TSet<FName>>& OutDependents) const
{
	OutDependents.Reset();
	OutDependents.Reserve(Attributes.Num());

	TArray<FName> DeclaredDependencies;
	DeclaredDependencies.Reserve(4);

	// 对每个属性询问“我的 Clamp 结果依赖哪些属性”，再反向建图成“谁变化后需要重新检查我”。
	for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
	{
		const FName AttributeName = Pair.Key;
		const FTcsAttributeInstance& Attribute = Pair.Value;
		if (!Attribute.AttributeDef || !Attribute.AttributeDef->ClampStrategyClass)
		{
			return false;
		}

		UTcsAttributeClampStrategy* StrategyCDO = Attribute.AttributeDef->ClampStrategyClass->GetDefaultObject<UTcsAttributeClampStrategy>();
		if (!StrategyCDO)
		{
			return false;
		}

		DeclaredDependencies.Reset();
		if (!StrategyCDO->CollectDependentAttributes(AttributeName, Attribute.AttributeDef, Attribute.AttributeDef->ClampStrategyConfig, DeclaredDependencies))
		{
			// 只要有一个策略不给出完整声明，就不能证明局部传播闭包正确，只能回退到全局 fixpoint。
			return false;
		}

		for (const FName DependencyAttribute : DeclaredDependencies)
		{
			// 忽略空依赖、自依赖和组件内不存在的属性，避免构造出无效传播边。
			if (DependencyAttribute.IsNone() || DependencyAttribute == AttributeName || !Attributes.Contains(DependencyAttribute))
			{
				continue;
			}

			OutDependents.FindOrAdd(DependencyAttribute).Add(AttributeName);
		}
	}

	return true;
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(
	const TSet<FName>& DirtyAttributes,
	bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(&DirtyAttributes, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraints(bool bBroadcastEvents)
{
	EnforceAttributeRangeConstraintsInternal(nullptr, bBroadcastEvents);
}

void UTcsAttributeComponent::EnforceAttributeRangeConstraintsInternal(
	const TSet<FName>* DirtyAttributes,
	bool bBroadcastEvents)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(TcsAttributeComponent_EnforceAttributeRangeConstraints);

	const int32 MaxIterations = 8; // 防止无限循环
	int32 Iteration = 0;
	bool bAnyChanged = true;

	// 工作集承接整轮传播的中间态。
	// 在最终收敛前，不直接写回 Attributes，避免半更新状态污染后续依赖解析。
	TMap<FName, float> WorkingBaseValues;
	TMap<FName, float> WorkingCurrentValues;
	WorkingBaseValues.Reserve(Attributes.Num());
	WorkingCurrentValues.Reserve(Attributes.Num());

	// 初始化工作集
	for (auto& Pair : Attributes)
	{
		WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
		WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
	}

	TMap<FName, TSet<FName>> DeclaredDependents;
	const bool bTryLocalPropagation = DirtyAttributes && !DirtyAttributes->IsEmpty();
	const bool bUseDeclaredLocalPropagation = bTryLocalPropagation && TryBuildDeclaredRangeConstraintDependents(DeclaredDependents);

	if (bUseDeclaredLocalPropagation)
	{
		// 从本轮脏属性出发，只沿“已声明依赖它们的属性”继续扩散，避免无意义的全表扫描。
		TSet<FName> PendingAttributes = *DirtyAttributes;

		while (!PendingAttributes.IsEmpty() && Iteration < MaxIterations)
		{
			Iteration++;
			bool bAnyChangedThisPass = false;

			TArray<FName> AttributesToProcess;
			AttributesToProcess.Reserve(PendingAttributes.Num());
			// 先冻结本轮要处理的节点，再把新脏节点留到下一轮，避免边遍历边扩队列导致语义混乱。
			for (const FName PendingAttribute : PendingAttributes)
			{
				AttributesToProcess.Add(PendingAttribute);
			}
			PendingAttributes.Empty();

			for (const FName AttributeName : AttributesToProcess)
			{
				if (!Attributes.Contains(AttributeName))
				{
					continue;
				}

				bool bAttributeChanged = false;

				float OldBase = WorkingBaseValues.FindRef(AttributeName);
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues.Add(AttributeName, NewBase);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				float OldCurrent = WorkingCurrentValues.FindRef(AttributeName);
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues.Add(AttributeName, NewCurrent);
					bAnyChangedThisPass = true;
					bAttributeChanged = true;
				}

				// 只有当前属性在这一轮真的被 Clamp 改动了，才有必要继续唤醒它的声明式依赖下游。
				if (bAttributeChanged)
				{
					if (const TSet<FName>* Dependents = DeclaredDependents.Find(AttributeName))
					{
						for (const FName DependentAttribute : *Dependents)
						{
							PendingAttributes.Add(DependentAttribute);
						}
					}
				}
			}

			if (!bAnyChangedThisPass && PendingAttributes.IsEmpty())
			{
				break;
			}
		}

		if (!PendingAttributes.IsEmpty())
		{
			UE_LOG(LogTcsAttribute, Warning,
				TEXT("[%s] Declared local propagation did not converge for entity '%s', fallback to full fixpoint."),
				*FString(__FUNCTION__),
				GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));

			// 局部传播不收敛时，不继续沿用中间工作集，直接回到组件当前已提交状态重新跑全局 fixpoint。
			// 这样可以避免把“未证明正确的局部中间态”带进保守路径。
			WorkingBaseValues.Reset();
			WorkingCurrentValues.Reset();
			WorkingBaseValues.Reserve(Attributes.Num());
			WorkingCurrentValues.Reserve(Attributes.Num());
			for (const TPair<FName, FTcsAttributeInstance>& Pair : Attributes)
			{
				WorkingBaseValues.Add(Pair.Key, Pair.Value.BaseValue);
				WorkingCurrentValues.Add(Pair.Key, Pair.Value.CurrentValue);
			}

			Iteration = 0;
			bAnyChanged = true;
		}
		else
		{
			bAnyChanged = false;
		}
	}

	if (!bUseDeclaredLocalPropagation || bAnyChanged)
	{
		// 未提供脏属性、策略未声明依赖，或局部传播未收敛时，回退到现有全局 fixpoint。
		Iteration = 0;
		bAnyChanged = true;
		while (bAnyChanged && Iteration < MaxIterations)
		{
			bAnyChanged = false;
			Iteration++;

			// 这里故意回到全表扫描。
			// 当依赖闭包未知时，只有重复检查所有属性，才能保守地逼近稳定态。
			for (auto& Pair : Attributes)
			{
				FName AttributeName = Pair.Key;

				// 阶段1: Clamp BaseValue，使用 WorkingBaseValues 解析动态范围
				float OldBase = WorkingBaseValues[AttributeName];
				float NewBase = OldBase;
				ClampAttributeValueInRange(AttributeName, NewBase, nullptr, nullptr, &WorkingBaseValues);
				if (!FMath::IsNearlyEqual(OldBase, NewBase))
				{
					WorkingBaseValues[AttributeName] = NewBase;
					bAnyChanged = true;
				}

				// 阶段2: Clamp CurrentValue，使用 WorkingCurrentValues 解析动态范围
				float OldCurrent = WorkingCurrentValues[AttributeName];
				float NewCurrent = OldCurrent;
				ClampAttributeValueInRange(AttributeName, NewCurrent, nullptr, nullptr, &WorkingCurrentValues);
				if (!FMath::IsNearlyEqual(OldCurrent, NewCurrent))
				{
					WorkingCurrentValues[AttributeName] = NewCurrent;
					bAnyChanged = true;
				}
			}
		}
	}

	// 检查是否收敛
	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsAttribute, Warning,
			TEXT("[%s] Max iterations reached for entity '%s', possible circular dependency"),
			*FString(__FUNCTION__),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}

	UE_LOG(LogTcsAttribute, VeryVerbose,
		TEXT("[Perf][%s] Attrs=%d Iterations=%d ReachedMaxIterations=%s"),
		*FString(__FUNCTION__),
		Attributes.Num(),
		Iteration,
		Iteration >= MaxIterations ? TEXT("true") : TEXT("false"));

	// 所有 Clamp 传播完成后再统一提交，避免半更新状态污染后续依赖解析和广播结果。
	TArray<FTcsAttributeChangeEventPayload> BaseChangePayloads;
	TArray<FTcsAttributeChangeEventPayload> CurrentChangePayloads;
	TArray<FTcsAttributeBoundaryEventPayload> BoundaryPayloads;
	BaseChangePayloads.Reserve(Attributes.Num());
	CurrentChangePayloads.Reserve(Attributes.Num());
	BoundaryPayloads.Reserve(Attributes.Num());

	for (auto& Pair : Attributes)
	{
		FName AttributeName = Pair.Key;
		FTcsAttributeInstance& Attribute = Pair.Value;

		// 提交阶段只负责把收敛后的工作集写回组件，并补发变更/边界事件，不再参与依赖推导。
		// 这样“求值传播”和“对外可见状态提交”两个阶段是解耦的。
		// 提交 BaseValue
		float NewBase = WorkingBaseValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.BaseValue, NewBase))
		{
			float OldBase = Attribute.BaseValue;
			Attribute.BaseValue = NewBase;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldBase;
			Payload.NewValue = NewBase;
			BaseChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewBase;
			float RangeMax = NewBase;
			ClampAttributeValueInRange(AttributeName, NewBase, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewBase, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewBase, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldBase, NewBase, BoundaryValue);
			}
		}

		// 提交 CurrentValue
		float NewCurrent = WorkingCurrentValues[AttributeName];
		if (!FMath::IsNearlyEqual(Attribute.CurrentValue, NewCurrent))
		{
			float OldCurrent = Attribute.CurrentValue;
			Attribute.CurrentValue = NewCurrent;

			FTcsAttributeChangeEventPayload Payload;
			Payload.AttributeName = AttributeName;
			Payload.OldValue = OldCurrent;
			Payload.NewValue = NewCurrent;
			CurrentChangePayloads.Add(Payload);

			// 检测是否达到边界
			float RangeMin = NewCurrent;
			float RangeMax = NewCurrent;
			ClampAttributeValueInRange(AttributeName, NewCurrent, &RangeMin, &RangeMax);
			const bool bReachedMin = FMath::IsNearlyEqual(NewCurrent, RangeMin);
			const bool bReachedMax = FMath::IsNearlyEqual(NewCurrent, RangeMax);
			if (bReachedMin || bReachedMax)
			{
				const bool bIsMaxBoundary = bReachedMax;
				const float BoundaryValue = bReachedMax ? RangeMax : RangeMin;
				BoundaryPayloads.Emplace(AttributeName, bIsMaxBoundary, OldCurrent, NewCurrent, BoundaryValue);
			}
		}
	}

	// 广播事件
	if (bBroadcastEvents && BaseChangePayloads.Num() > 0)
	{
		BroadcastAttributeBaseValueChangeEvent(BaseChangePayloads);
	}
	if (bBroadcastEvents && CurrentChangePayloads.Num() > 0)
	{
		BroadcastAttributeValueChangeEvent(CurrentChangePayloads);
	}
	if (bBroadcastEvents)
	{
		BroadcastAttributeReachedBoundaryBatchEvent(BoundaryPayloads);
	}
}
