// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "GameFramework/Actor.h"
#include "State/TcsStateDefinition.h"
#include "State/TcsStateSlotDefinition.h"


bool UTcsStateComponent::GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const
{
	if (!SlotTag.IsValid())
	{
		OutStates.Empty();
		return false;
	}

	return StateInstanceIndex.GetInstancesBySlot(SlotTag, OutStates);
}

bool UTcsStateComponent::GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const
{
	if (StateDefId.IsNone())
	{
		OutStates.Empty();
		return false;
	}

	return StateInstanceIndex.GetInstancesByName(StateDefId, OutStates);
}

bool UTcsStateComponent::GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const
{
	OutStates.Empty();
	for (UTcsStateInstance* State : StateInstanceIndex.Instances)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			OutStates.Add(State);
		}
	}
	return OutStates.Num() > 0;
}

bool UTcsStateComponent::HasStateWithDefId(FName StateDefId) const
{
	TArray<UTcsStateInstance*> States;
	return GetStatesByDefId(StateDefId, States);
}

bool UTcsStateComponent::HasActiveStateInSlot(FGameplayTag SlotTag) const
{
	TArray<UTcsStateInstance*> States;
	if (!GetStatesInSlot(SlotTag, States))
	{
		return false;
	}

	for (const UTcsStateInstance* State : States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			return true;
		}
	}

	return false;
}

FTcsStateSlot* UTcsStateComponent::FindRuntimeStateSlot(FGameplayTag SlotTag)
{
	if (!SlotTag.IsValid())
	{
		return nullptr;
	}

	return RuntimeStateSlots.Find(SlotTag);
}

const FTcsStateSlot* UTcsStateComponent::FindRuntimeStateSlot(FGameplayTag SlotTag) const
{
	if (!SlotTag.IsValid())
	{
		return nullptr;
	}

	return RuntimeStateSlots.Find(SlotTag);
}

FString UTcsStateComponent::GetSlotDebugSnapshot(FGameplayTag SlotFilter) const
{
	auto BuildLine = [this](const FGameplayTag& SlotTag, const FTcsStateSlot& Slot) -> FString
	{
		FString Line = FString::Printf(TEXT("[%s] Gate=%s"),
			*SlotTag.ToString(),
			Slot.bIsGateOpen ? TEXT("Open") : TEXT("Closed"));

		if (const UTcsStateSlotDefinition* SlotDef = Slot.GetStateSlotDef())
		{
			Line += FString::Printf(TEXT(" Mode=%s Preempt=%s"),
				*StaticEnum<ETcsStateSlotActivationMode>()->GetNameStringByValue(static_cast<int64>(SlotDef->ActivationMode)),
				*StaticEnum<ETcsStatePreemptionPolicy>()->GetNameStringByValue(static_cast<int64>(SlotDef->PreemptionPolicy)));
		}

		auto FormatState = [this](UTcsStateInstance* State) -> FString
		{
			if (!IsValid(State))
			{
				return TEXT("<invalid>");
			}

			const FString StateId = State->GetStateDefId().ToString();
			const int32 InstanceId = State->GetInstanceId();
			const UTcsStateDefinition* StateDef = State->GetStateDef();
			const int32 Priority = StateDef ? StateDef->Priority : 0;
			const int32 Level = State->GetLevel();
			const ETcsStateTreeTickPolicy TickPolicy = StateDef ? StateDef->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
			const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));
			int32 StackCount = -1;
			FString DurStr = TEXT("0.00");
			BuildStateDebugOverlay(State, StackCount, DurStr);

			const AActor* Instigator = State->GetInstigator();
			const FString InstigatorName = Instigator ? Instigator->GetName() : TEXT("None");

			return FString::Printf(TEXT("%s#%d(P=%d,Stack=%d,Lv=%d,Dur=%s,Tick=%s,Inst=%s)"),
				*StateId,
				InstanceId,
				Priority,
				StackCount,
				Level,
				*DurStr,
				*TickPolicyStr,
				*InstigatorName);
		};

		auto SortStates = [](TArray<UTcsStateInstance*>& States)
		{
			States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
			{
				const UTcsStateDefinition* AStateDef = A.GetStateDef();
				const UTcsStateDefinition* BStateDef = B.GetStateDef();
				const int32 AP = AStateDef ? AStateDef->Priority : 0;
				const int32 BP = BStateDef ? BStateDef->Priority : 0;
				if (AP != BP)
				{
					return AP > BP;
				}
				return A.GetInstanceId() < B.GetInstanceId();
			});
		};

		TArray<FString> ActiveStates;
		TArray<FString> HangUpStates;
		TArray<FString> PauseStates;
		TArray<FString> StoredStates;

		int32 ActiveCount = 0;
		int32 HangUpCount = 0;
		int32 PauseCount = 0;
		int32 InactiveCount = 0;

		TArray<UTcsStateInstance*> ActiveToSort;
		TArray<UTcsStateInstance*> HangUpToSort;
		TArray<UTcsStateInstance*> PauseToSort;
		TArray<UTcsStateInstance*> StoredToSort;

		for (UTcsStateInstance* State : Slot.States)
		{
			if (!IsValid(State))
			{
				continue;
			}

			switch (State->GetCurrentStage())
			{
			case ETcsStateStage::SS_Active:
				ActiveToSort.Add(State);
				ActiveCount++;
				break;
			case ETcsStateStage::SS_HangUp:
				HangUpToSort.Add(State);
				HangUpCount++;
				break;
			case ETcsStateStage::SS_Pause:
				PauseToSort.Add(State);
				PauseCount++;
				break;
			case ETcsStateStage::SS_Inactive:
				StoredToSort.Add(State);
				InactiveCount++;
				break;
			default:
				StoredToSort.Add(State);
				break;
			}
		}

		SortStates(ActiveToSort);
		SortStates(HangUpToSort);
		SortStates(PauseToSort);
		SortStates(StoredToSort);

		for (UTcsStateInstance* State : ActiveToSort)
		{
			ActiveStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : HangUpToSort)
		{
			HangUpStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : PauseToSort)
		{
			PauseStates.Add(FormatState(State));
		}
		for (UTcsStateInstance* State : StoredToSort)
		{
			if (!IsValid(State))
			{
				continue;
			}

			const FString StateDesc = FormatState(State);
			if (State->GetCurrentStage() == ETcsStateStage::SS_Inactive)
			{
				StoredStates.Add(StateDesc);
			}
			else
			{
				StoredStates.Add(FString::Printf(TEXT("%s(%s)"),
					*StateDesc,
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage()))));
			}
		}

		Line += FString::Printf(TEXT(" N=%d(A=%d,H=%d,P=%d,I=%d)"),
			Slot.States.Num(),
			ActiveCount,
			HangUpCount,
			PauseCount,
			InactiveCount);

		auto AppendList = [&Line](const TCHAR* Label, const TArray<FString>& Names)
		{
			if (Names.Num() > 0)
			{
				Line += FString::Printf(TEXT(" %s={%s}"), Label, *FString::Join(Names, TEXT(", ")));
			}
		};

		AppendList(TEXT("Active"), ActiveStates);
		AppendList(TEXT("HangUp"), HangUpStates);
		AppendList(TEXT("Pause"), PauseStates);
		AppendList(TEXT("Stored"), StoredStates);

		return Line;
	};

	if (SlotFilter.IsValid())
	{
		if (const FTcsStateSlot* Slot = RuntimeStateSlots.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	TArray<FGameplayTag> SlotTags;
	RuntimeStateSlots.GetKeys(SlotTags);
	SlotTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
	{
		return A.ToString() < B.ToString();
	});

	for (const FGameplayTag& Tag : SlotTags)
	{
		const FTcsStateSlot& Slot = RuntimeStateSlots[Tag];
		if (!Accumulator.IsEmpty())
		{
			Accumulator += TEXT("\n");
		}
		Accumulator += BuildLine(Tag, Slot);
	}

	if (Accumulator.IsEmpty())
	{
		Accumulator = TEXT("<no slots>");
	}

	return Accumulator;
}

FString UTcsStateComponent::GetStateDebugSnapshot(FName StateDefIdFilter) const
{
	auto FormatStateLine = [this](const UTcsStateInstance* State) -> FString
	{
		if (!IsValid(State))
		{
			return TEXT("<invalid>");
		}

		const UTcsStateDefinition* StateDef = State->GetStateDef();
		const FGameplayTag SlotTag = StateDef ? StateDef->StateSlotType : FGameplayTag();
		const FTcsStateSlot* Slot = SlotTag.IsValid() ? RuntimeStateSlots.Find(SlotTag) : nullptr;
		const bool bGateOpen = SlotTag.IsValid() ? (Slot && Slot->bIsGateOpen) : true;

		const ETcsStateTreeTickPolicy TickPolicy = StateDef ? StateDef->TickPolicy : ETcsStateTreeTickPolicy::ManualOnly;
		const FString TickPolicyStr = StaticEnum<ETcsStateTreeTickPolicy>()->GetNameStringByValue(static_cast<int64>(TickPolicy));

		int32 StackCount = -1;
		FString DurStr = TEXT("0.00");
		BuildStateDebugOverlay(State, StackCount, DurStr);

		const AActor* OwnerActor = State->GetOwner();
		const AActor* Instigator = State->GetInstigator();

		const int32 Priority = StateDef ? StateDef->Priority : 0;

		return FString::Printf(TEXT("State=%s Id=%d Slot=%s Gate=%s Stage=%s P=%d Lv=%d Stack=%d Dur=%s Tick=%s Owner=%s Inst=%s"),
			*State->GetStateDefId().ToString(),
			State->GetInstanceId(),
			*SlotTag.ToString(),
			bGateOpen ? TEXT("Open") : TEXT("Closed"),
			*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())),
			Priority,
			State->GetLevel(),
			StackCount,
			*DurStr,
			*TickPolicyStr,
			OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
			Instigator ? *Instigator->GetName() : TEXT("None"));
	};

	TArray<UTcsStateInstance*> Instances = StateInstanceIndex.Instances;
	Instances.RemoveAll([StateDefIdFilter](const UTcsStateInstance* State)
	{
		if (!IsValid(State))
		{
			return true;
		}
		if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
		{
			return true;
		}
		if (!StateDefIdFilter.IsNone() && State->GetStateDefId() != StateDefIdFilter)
		{
			return true;
		}
		return false;
	});

	Instances.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
	{
		const UTcsStateDefinition* AStateDef = A.GetStateDef();
		const UTcsStateDefinition* BStateDef = B.GetStateDef();

		const FString AS = AStateDef ? AStateDef->StateSlotType.ToString() : TEXT("");
		const FString BS = BStateDef ? BStateDef->StateSlotType.ToString() : TEXT("");
		if (AS != BS)
		{
			return AS < BS;
		}

		const int32 AP = AStateDef ? AStateDef->Priority : 0;
		const int32 BP = BStateDef ? BStateDef->Priority : 0;
		if (AP != BP)
		{
			return AP > BP;
		}

		return A.GetInstanceId() < B.GetInstanceId();
	});

	FString Accumulator = FString::Printf(TEXT("Total=%d Filter=%s"), Instances.Num(), *StateDefIdFilter.ToString());
	for (const UTcsStateInstance* State : Instances)
	{
		Accumulator += TEXT("\n");
		Accumulator += FormatStateLine(State);
	}

	return Accumulator;
}
