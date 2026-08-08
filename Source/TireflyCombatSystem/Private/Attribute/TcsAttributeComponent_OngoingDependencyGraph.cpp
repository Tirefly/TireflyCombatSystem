// Copyright Tirefly. All Rights Reserved.

#include "Attribute/TcsAttributeComponent.h"



bool UTcsAttributeComponent::BuildOngoingDependencyEvaluationOrder(
	const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
	const TSet<int32>& SeedModifierInstIds,
	TArray<int32>& OutEvaluationOrder,
	FString& OutCycleDiagnostic,
	TArray<TArray<int32>>* OutCyclicSccs) const
{
	OutEvaluationOrder.Reset();
	OutCycleDiagnostic.Reset();
	if (OutCyclicSccs)
	{
		OutCyclicSccs->Reset();
	}

	TMap<int32, const FTcsAttributeModifierInstance*> InstancesById;
	TMap<FName, TArray<int32>> ProducersByAttribute;
	TMap<FName, TMap<int32, TArray<FName>>> ProducerOperationsByAttribute;
	for (const FTcsAttributeModifierInstance& ModifierInstance : ModifierInstances)
	{
		if (!ModifierInstance.IsValid())
		{
			OutCycleDiagnostic = FString::Printf(
				TEXT("Invalid Ongoing parent %d while building dependency graph."),
				ModifierInstance.ModifierInstId);
			return false;
		}

		InstancesById.Add(ModifierInstance.ModifierInstId, &ModifierInstance);
		for (const FTcsEvaluatedAttributeOperation& Operation : ModifierInstance.AppliedOperations)
		{
			ProducersByAttribute.FindOrAdd(Operation.TargetAttributeId).AddUnique(ModifierInstance.ModifierInstId);
			ProducerOperationsByAttribute
				.FindOrAdd(Operation.TargetAttributeId)
				.FindOrAdd(ModifierInstance.ModifierInstId)
				.AddUnique(Operation.OperationId);
		}
	}

	const TMap<FName, TArray<int32>> DirectProducersByAttribute = ProducersByAttribute;
	const TMap<FName, TMap<int32, TArray<FName>>> DirectProducerOperationsByAttribute =
		ProducerOperationsByAttribute;
	TMap<FName, TSet<FName>> RangeDependents;
	const bool bHasCompleteRangeDependencies = TryBuildDeclaredRangeConstraintDependents(RangeDependents);
	if (!bHasCompleteRangeDependencies && !ModifierInstances.IsEmpty())
	{
		OutCycleDiagnostic = TEXT("Cannot build Ongoing dependency graph because at least one ClampStrategy does not declare complete Attribute dependencies.");
		return false;
	}
	if (bHasCompleteRangeDependencies)
	{
		for (const TPair<FName, TArray<int32>>& DirectProducerPair : DirectProducersByAttribute)
		{
			TSet<FName> VisitedAttributes;
			TArray<FName> PendingAttributes;
			PendingAttributes.Add(DirectProducerPair.Key);
			while (!PendingAttributes.IsEmpty())
			{
				const FName ProducedAttribute = PendingAttributes.Pop(EAllowShrinking::No);
				if (VisitedAttributes.Contains(ProducedAttribute))
				{
					continue;
				}
				VisitedAttributes.Add(ProducedAttribute);

				const TSet<FName>* const Dependents = RangeDependents.Find(ProducedAttribute);
				if (!Dependents)
				{
					continue;
				}

				for (const FName DependentAttribute : *Dependents)
				{
					for (const int32 ProducerId : DirectProducerPair.Value)
					{
						ProducersByAttribute.FindOrAdd(DependentAttribute).AddUnique(ProducerId);
						if (const TMap<int32, TArray<FName>>* const DirectOperations =
							DirectProducerOperationsByAttribute.Find(DirectProducerPair.Key))
						{
							if (const TArray<FName>* const OperationIds = DirectOperations->Find(ProducerId))
							{
								ProducerOperationsByAttribute
									.FindOrAdd(DependentAttribute)
									.FindOrAdd(ProducerId)
									.Append(*OperationIds);
							}
						}
					}
					PendingAttributes.Add(DependentAttribute);
				}
			}
		}
	}

	TMap<int32, TSet<int32>> DependentsByProducer;
	TMap<int32, TSet<int32>> ProducersByConsumer;
	for (const FTcsAttributeModifierInstance& Consumer : ModifierInstances)
	{
		for (const FTcsAttributeModifierDependencyRecord& DependencyRecord : Consumer.DependencyRecords)
		{
			if (DependencyRecord.Key.Type != ETcsAttributeModifierDependencyType::AMDT_AttributeCurrentValue)
			{
				continue;
			}

			if (const TArray<int32>* const ProducerIds = ProducersByAttribute.Find(DependencyRecord.Key.AttributeId))
			{
				for (const int32 ProducerId : *ProducerIds)
				{
					if (ProducerId != Consumer.ModifierInstId)
					{
						DependentsByProducer.FindOrAdd(ProducerId).Add(Consumer.ModifierInstId);
						ProducersByConsumer.FindOrAdd(Consumer.ModifierInstId).Add(ProducerId);
					}
				}
			}
		}
	}

	TSet<int32> ClosureIds;
	TArray<int32> PendingIds;
	if (SeedModifierInstIds.IsEmpty())
	{
		InstancesById.GetKeys(PendingIds);
	}
	else
	{
		for (const int32 SeedId : SeedModifierInstIds)
		{
			if (InstancesById.Contains(SeedId))
			{
				PendingIds.Add(SeedId);
			}
		}
	}

	while (!PendingIds.IsEmpty())
	{
		const int32 ModifierInstId = PendingIds.Pop(EAllowShrinking::No);
		if (ClosureIds.Contains(ModifierInstId))
		{
			continue;
		}

		ClosureIds.Add(ModifierInstId);
		if (const TSet<int32>* const Dependents = DependentsByProducer.Find(ModifierInstId))
		{
			for (const int32 DependentId : *Dependents)
			{
				PendingIds.Add(DependentId);
			}
		}
	}

	TMap<int32, int32> InDegrees;
	for (const int32 ModifierInstId : ClosureIds)
	{
		InDegrees.Add(ModifierInstId, 0);
	}
	for (const TPair<int32, TSet<int32>>& Pair : DependentsByProducer)
	{
		if (!ClosureIds.Contains(Pair.Key))
		{
			continue;
		}

		for (const int32 DependentId : Pair.Value)
		{
			if (ClosureIds.Contains(DependentId))
			{
				++InDegrees.FindChecked(DependentId);
			}
		}
	}

	while (OutEvaluationOrder.Num() < ClosureIds.Num())
	{
		TArray<int32> ReadyIds;
		for (const TPair<int32, int32>& Pair : InDegrees)
		{
			if (Pair.Value == 0 && !OutEvaluationOrder.Contains(Pair.Key))
			{
				ReadyIds.Add(Pair.Key);
			}
		}
		ReadyIds.Sort();
		if (ReadyIds.IsEmpty())
		{
			break;
		}

		for (const int32 ReadyId : ReadyIds)
		{
			OutEvaluationOrder.Add(ReadyId);
			InDegrees.FindChecked(ReadyId) = INDEX_NONE;
			if (const TSet<int32>* const Dependents = DependentsByProducer.Find(ReadyId))
			{
				for (const int32 DependentId : *Dependents)
				{
					if (int32* const InDegree = InDegrees.Find(DependentId); InDegree && *InDegree > 0)
					{
						--*InDegree;
					}
				}
			}
		}
	}

	if (OutEvaluationOrder.Num() == ClosureIds.Num())
	{
		return true;
	}

	// Tarjan SCC on the closure subgraph; only report true cycles (size > 1 or self-edge).
	TArray<int32> ClosureIdList = ClosureIds.Array();
	ClosureIdList.Sort();
	TMap<int32, int32> Disc;
	TMap<int32, int32> Low;
	TSet<int32> OnStack;
	TArray<int32> Stack;
	int32 NextDisc = 0;
	TArray<TArray<int32>> CyclicSccs;

	TFunction<void(int32)> StrongConnect;
	StrongConnect = [&](int32 NodeId)
	{
		Disc.Add(NodeId, NextDisc);
		Low.Add(NodeId, NextDisc);
		++NextDisc;
		Stack.Add(NodeId);
		OnStack.Add(NodeId);

		if (const TSet<int32>* const Dependents = DependentsByProducer.Find(NodeId))
		{
			for (const int32 DependentId : *Dependents)
			{
				if (!ClosureIds.Contains(DependentId))
				{
					continue;
				}

				if (!Disc.Contains(DependentId))
				{
					StrongConnect(DependentId);
					Low.FindChecked(NodeId) = FMath::Min(Low.FindChecked(NodeId), Low.FindChecked(DependentId));
				}
				else if (OnStack.Contains(DependentId))
				{
					Low.FindChecked(NodeId) = FMath::Min(Low.FindChecked(NodeId), Disc.FindChecked(DependentId));
				}
			}
		}

		if (Low.FindChecked(NodeId) == Disc.FindChecked(NodeId))
		{
			TArray<int32> Component;
			int32 PoppedId = INDEX_NONE;
			do
			{
				PoppedId = Stack.Pop(EAllowShrinking::No);
				OnStack.Remove(PoppedId);
				Component.Add(PoppedId);
			}
			while (PoppedId != NodeId);

			bool bIsCyclic = Component.Num() > 1;
			if (!bIsCyclic)
			{
				if (const TSet<int32>* const Dependents = DependentsByProducer.Find(Component[0]))
				{
					bIsCyclic = Dependents->Contains(Component[0]);
				}
			}
			if (bIsCyclic)
			{
				Component.Sort();
				CyclicSccs.Add(MoveTemp(Component));
			}
		}
	};

	for (const int32 NodeId : ClosureIdList)
	{
		if (!Disc.Contains(NodeId))
		{
			StrongConnect(NodeId);
		}
	}
	CyclicSccs.Sort([](const TArray<int32>& Left, const TArray<int32>& Right)
	{
		const int32 LeftId = Left.Num() > 0 ? Left[0] : INDEX_NONE;
		const int32 RightId = Right.Num() > 0 ? Right[0] : INDEX_NONE;
		return LeftId < RightId;
	});
	if (OutCyclicSccs)
	{
		*OutCyclicSccs = CyclicSccs;
	}

	TArray<int32> SortedCyclicIds;
	for (const TArray<int32>& Scc : CyclicSccs)
	{
		SortedCyclicIds.Append(Scc);
	}
	SortedCyclicIds.Sort();
	OutCycleDiagnostic = FString::Printf(
		TEXT("Ongoing dependency cycle detected. ModifierInstIds=[%s]"),
		*FString::JoinBy(SortedCyclicIds, TEXT(","), [](const int32 Id)
		{
			return FString::FromInt(Id);
		}));

	TSet<int32> CyclicIdSet(SortedCyclicIds);
	for (const int32 ProducerId : SortedCyclicIds)
	{
		const TSet<int32>* const Dependents = DependentsByProducer.Find(ProducerId);
		if (!Dependents)
		{
			continue;
		}

		for (const int32 ConsumerId : *Dependents)
		{
			if (!CyclicIdSet.Contains(ConsumerId))
			{
				continue;
			}

			const FTcsAttributeModifierInstance* const Consumer = InstancesById.FindRef(ConsumerId);
			if (!Consumer)
			{
				continue;
			}

			for (const FTcsAttributeModifierDependencyRecord& DependencyRecord : Consumer->DependencyRecords)
			{
				if (DependencyRecord.Key.Type != ETcsAttributeModifierDependencyType::AMDT_AttributeCurrentValue)
				{
					continue;
				}

				const TMap<int32, TArray<FName>>* const Producers =
					ProducerOperationsByAttribute.Find(DependencyRecord.Key.AttributeId);
				const TArray<FName>* const OperationIds = Producers ? Producers->Find(ProducerId) : nullptr;
				if (OperationIds)
				{
					OutCycleDiagnostic += FString::Printf(
						TEXT(" | %d -- AttributeCurrent[%s], Operations[%s] --> %d"),
						ProducerId,
						*DependencyRecord.Key.AttributeId.ToString(),
						*FString::JoinBy(*OperationIds, TEXT(","), [](const FName OperationId)
						{
							return OperationId.ToString();
						}),
						ConsumerId);
				}
			}
		}
	}

	return false;
}
