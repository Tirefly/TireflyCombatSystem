// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateComponent.h"

#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateManagerSubsystem.h"
#include "TcsDeveloperSettings.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeExecutionContext.h"



UTcsStateComponent::UTcsStateComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bStartLogicAutomatically = true;
}

void UTcsStateComponent::BeginPlay()
{
	// 获取状态管理器子系统
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] World is invalid."), *FString(__FUNCTION__));
		return;
	}

	StateMgr = World->GetSubsystem<UTcsStateManagerSubsystem>();
	if (!StateMgr)
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Failed to get TcsStateManagerSubsystem."),
			*FString(__FUNCTION__));
		return;
	}

    // 初始化StateSlot和StateTreeState的映射
	StateMgr->InitStateSlotMappings(GetOwner());

	// 各项初始化之后，再执行状态管理StateTree
	Super::BeginPlay();
}

void UTcsStateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Tick 激活状态
	for (UTcsStateInstance* ActiveState : PersistentStateInstances.Instances)
	{
		if (IsValid(ActiveState) && ActiveState->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			ActiveState->TickStateTree(DeltaTime);
		}
	}
}

float UTcsStateComponent::GetStateRemainingDuration(const UTcsStateInstance* StateInstance) const
{
	if (const FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance))
	{
		return DurationData->RemainingDuration;
	}
	
	return 0.0f;
}

void UTcsStateComponent::RefreshStateRemainingDuration(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!StateMgr)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateMgr is not initialized."), *FString(__FUNCTION__));
		return;
	}

	FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
	if (!DurationData)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is not found in %s."),
			*FString(__FUNCTION__),
			*GetOwner()->GetName());
		return;
	}

	// 刷新为总持续时间
	DurationData->RemainingDuration = StateInstance->GetTotalDuration();
	// TODO: 实现事件通知
	// StateMgr->OnStateInstanceDurationUpdated(StateInstance);
}

void UTcsStateComponent::SetStateRemainingDuration(UTcsStateInstance* StateInstance, float InDurationRemaining)
{
	if (!IsValid(StateInstance))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid."), *FString(__FUNCTION__));
		return;
	}

	if (!StateMgr)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateMgr is not initialized."), *FString(__FUNCTION__));
		return;
	}

	FTcsStateDurationData* DurationData = StateDurationMap.Find(StateInstance);
	if (!DurationData)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is not found in %s."),
			*FString(__FUNCTION__),
			*GetOwner()->GetName());
		return;
	}

	// 设置新的剩余时间
	DurationData->RemainingDuration = InDurationRemaining;
	// TODO: 实现事件通知
	// StateMgr->OnStateInstanceDurationUpdated(StateInstance);
}

void UTcsStateComponent::UpdateActiveStateDurations(float DeltaTime)
{
	TArray<UTcsStateInstance*> ExpiredStates;

	for (auto& DurationPair : StateDurationMap)
	{
		UTcsStateInstance* StateInstance = DurationPair.Key;
		FTcsStateDurationData& DurationData = DurationPair.Value;

		if (!IsValid(StateInstance))
		{
			ExpiredStates.Add(StateInstance);
			continue;
		}

		// 确认状态当前的状态阶段
		const ETcsStateStage CurrentStage = StateInstance->GetCurrentStage();

		// SS_Pause 状态完全冻结,跳过持续时间计算
		if (CurrentStage == ETcsStateStage::SS_Pause)
		{
			continue;
		}

		// 注意: SS_HangUp 状态虽然挂起,但仍会继续计算持续时间,可以视为"激活"(根据策略)
		const bool bStateIsActive = (CurrentStage == ETcsStateStage::SS_Active);
		
		// 检查状态所属的状态槽的状态
		const FGameplayTag StateSlotTag = StateInstance->GetStateDef().StateSlotType;
		FTcsStateSlotDefinition SlotDefinition;
		if (!StateMgr->TryGetStateSlotDefinition(StateSlotTag, SlotDefinition))
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get StateSlotDefinition for %s."),
				*FString(__FUNCTION__),
				*StateSlotTag.ToString());
			continue;
		}
		FTcsStateSlot* SlotData = StateSlotTag.IsValid() ? StateSlotsX.Find(StateSlotTag) : nullptr;
		const bool bGateOpen = !StateSlotTag.IsValid() || (SlotData && SlotData->bIsGateOpen);

		bool bShouldTick = false;
		// 根据状态槽的持续时间流失策略决定是否递减剩余持续时间
		switch (SlotDefinition.DurationTickPolicy)
		{
		case ETcsDurationTickPolicy::DTP_Always:
			bShouldTick = true;
			break;
		case ETcsDurationTickPolicy::DTP_OnlyWhenGateOpen:
			bShouldTick = bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOrGateOpen:
			bShouldTick = bStateIsActive || bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveAndGateOpen:
			bShouldTick = bStateIsActive && bGateOpen;
			break;
		case ETcsDurationTickPolicy::DTP_ActiveOnly:
			bShouldTick = bStateIsActive;
			break;
		}

		if (!bShouldTick)
		{
			continue;
		}

		// 递减剩余持续时间，并检查是否到期
		DurationData.RemainingDuration -= DeltaTime;
		if (DurationData.RemainingDuration <= 0.0f)
		{
			ExpiredStates.Add(StateInstance);
			SlotData->States.Remove(StateInstance);
		}
	}

	// 处理到期状态
	for (UTcsStateInstance* ExpiredState : ExpiredStates)
	{
		if (IsValid(ExpiredState))
		{
			// TODO: 触发状态持续时间到期事件
			ExpiredState->StopStateTree();
		}

		StateDurationMap.Remove(ExpiredState);
		PersistentStateInstances.RemoveInstance(ExpiredState);
	}
	PersistentStateInstances.RefreshInstances();
}

void UTcsStateComponent::NotifyStateStageChanged(UTcsStateInstance* StateInstance, ETcsStateStage PreviousStage, ETcsStateStage NewStage)
{
	if (!IsValid(StateInstance) || PreviousStage == NewStage)
	{
		return;
	}

	if (OnStateStageChanged.IsBound())
	{
		OnStateStageChanged.Broadcast(this, StateInstance, PreviousStage, NewStage);
	}
}

FStateTreeReference UTcsStateComponent::GetStateTreeReference() const
{
	return StateTreeRef;
}

const UStateTree* UTcsStateComponent::GetStateTree() const
{
	return StateTreeRef.GetStateTree();
}

void UTcsStateComponent::OnStateSlotChanged(FGameplayTag SlotTag)
{
	// TODO: StateTree状态槽变化事件处理
	// 这里可以添加状态槽变化的响应逻辑
	// 例如：通知StateTree系统、发送游戏事件等
	
	UE_LOG(LogTcsState, VeryVerbose, TEXT("State slot [%s] changed"), *SlotTag.ToString());
}

FString UTcsStateComponent::GetSlotDebugSnapshot(FGameplayTag SlotFilter) const
{
	auto BuildLine = [this](const FGameplayTag& SlotTag, const FTcsStateSlot& Slot) -> FString
	{
		FString Line = FString::Printf(TEXT("[%s] Gate=%s"),
			*SlotTag.ToString(),
			Slot.bIsGateOpen ? TEXT("Open") : TEXT("Closed"));

		TArray<FString> ActiveStates;
		TArray<FString> HangUpStates;
		TArray<FString> StoredStates;

		for (UTcsStateInstance* State : Slot.States)
		{
			if (!IsValid(State))
			{
				continue;
			}

			const FString StateName = State->GetStateDefId().ToString();
			switch (State->GetCurrentStage())
			{
			case ETcsStateStage::SS_Active:
				ActiveStates.Add(StateName);
				break;
			case ETcsStateStage::SS_HangUp:
				HangUpStates.Add(StateName);
				break;
			case ETcsStateStage::SS_Inactive:
				StoredStates.Add(StateName);
				break;
			default:
				StoredStates.Add(FString::Printf(TEXT("%s(%s)"),
					*StateName,
					*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage()))));
				break;
			}
		}

		auto AppendList = [&Line](const TCHAR* Label, const TArray<FString>& Names)
		{
			if (Names.Num() > 0)
			{
				Line += FString::Printf(TEXT(" %s={%s}"), Label, *FString::Join(Names, TEXT(", ")));
			}
		};

		AppendList(TEXT("Active"), ActiveStates);
		AppendList(TEXT("HangUp"), HangUpStates);
		AppendList(TEXT("Stored"), StoredStates);

		return Line;
	};

	if (SlotFilter.IsValid())
	{
		if (const FTcsStateSlot* Slot = StateSlots.Find(SlotFilter))
		{
			return BuildLine(SlotFilter, *Slot);
		}
		return FString::Printf(TEXT("[%s] <slot not initialized>"), *SlotFilter.ToString());
	}

	FString Accumulator;
	for (const TPair<FGameplayTag, FTcsStateSlot>& Pair : StateSlots)
	{
		if (!Accumulator.IsEmpty())
		{
			Accumulator += TEXT("\n");
		}
		Accumulator += BuildLine(Pair.Key, Pair.Value);
	}

	if (Accumulator.IsEmpty())
	{
		Accumulator = TEXT("<no slots>");
	}

	return Accumulator;
}

void UTcsStateComponent::SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen)
{
    FTcsStateSlot* Slot = StateSlots.Find(SlotTag);
    if (!Slot)
    {
        // 如果槽位不存在,创建一个新的槽位
        FTcsStateSlot& NewSlot = StateSlots.Add(SlotTag);
        NewSlot.bIsGateOpen = bOpen;
        UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s (new slot created)"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));
        return;
    }

	if (Slot->bIsGateOpen != bOpen)
	{
		Slot->bIsGateOpen = bOpen;
		UE_LOG(LogTcsState, Verbose, TEXT("Slot gate %s -> %s"), *SlotTag.ToString(), bOpen ? TEXT("Open") : TEXT("Closed"));
		UpdateStateSlotActivation(SlotTag);
	}
}

bool UTcsStateComponent::IsSlotGateOpen(FGameplayTag SlotTag) const
{
    if (const FTcsStateSlot* Slot = StateSlots.Find(SlotTag))
    {
        return Slot->bIsGateOpen;
    }
    // 默认开启，保持向后兼容
    return true;
}

TArray<FName> UTcsStateComponent::GetCurrentActiveStateTreeStates() const
{
	TArray<FName> ActiveStateNames;

	if (!StateTreeRef.IsValid())
	{
		return ActiveStateNames;
	}

	const UStateTree* StateTree = StateTreeRef.GetStateTree();
	if (!StateTree)
	{
		return ActiveStateNames;
	}

	// StateTree API 目前只提供 const 访问接口，这里通过 const_cast 获取可写指针以创建 ExecutionContext。
	UTcsStateComponent* MutableThis = const_cast<UTcsStateComponent*>(this);
	FStateTreeInstanceData* MutableInstanceData = &MutableThis->InstanceData;
	FStateTreeExecutionContext Context(*MutableThis, *StateTree, *MutableInstanceData);
	ActiveStateNames = Context.GetActiveStateNames();

	// 如果StateTree没有激活状态，则返回缓存，避免外部逻辑误判为发生变化。
	if (ActiveStateNames.IsEmpty() && !CachedActiveStateNames.IsEmpty())
	{
		ActiveStateNames = CachedActiveStateNames;
	}

	return ActiveStateNames;
}

void UTcsStateComponent::OnStateTreeStateChanged(const FStateTreeExecutionContext& Context)
{
	// 【关键API】从ExecutionContext获取当前激活状态
	TArray<FName> CurrentActiveStates = Context.GetActiveStateNames();

	// 检测变化
	if (!AreStateNamesEqual(CurrentActiveStates, CachedActiveStateNames))
	{
		RefreshSlotsForStateChange(CurrentActiveStates, CachedActiveStateNames);
		CachedActiveStateNames = CurrentActiveStates;

		UE_LOG(LogTcsState, Log,
			   TEXT("[StateTree Event] State changed: %d active states"),
			   CurrentActiveStates.Num());
	}
}

void UTcsStateComponent::RefreshSlotsForStateChange(
	const TArray<FName>& NewStates,
	const TArray<FName>& OldStates)
{
	// 计算新增的状态
	TSet<FName> AddedStates(NewStates);
	for (const FName& OldState : OldStates)
	{
		AddedStates.Remove(OldState);
	}

	// 计算移除的状态
	TSet<FName> RemovedStates(OldStates);
	for (const FName& NewState : NewStates)
	{
		RemovedStates.Remove(NewState);
	}

	// 遍历槽位映射，更新Gate状态
	for (const auto& Pair : SlotToStateHandleMap)
	{
		const FGameplayTag SlotTag = Pair.Key;
		const FTcsStateSlotDefinition* SlotDef = StateSlotDefinitions.Find(SlotTag);

		if (!SlotDef || SlotDef->StateTreeStateName.IsNone())
		{
			continue;
		}

		const FName& MappedStateName = SlotDef->StateTreeStateName;
		bool bShouldOpen = false;

		if (AddedStates.Contains(MappedStateName))
		{
			bShouldOpen = true;
		}
		else if (RemovedStates.Contains(MappedStateName))
		{
			bShouldOpen = false;
		}
		else
		{
			bShouldOpen = NewStates.Contains(MappedStateName);
		}

		const bool bWasOpen = IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			SetSlotGateOpen(SlotTag, bShouldOpen);
			UpdateStateSlotActivation(SlotTag);

			UE_LOG(LogTcsState, Log,
				   TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				   *SlotTag.ToString(),
				   bShouldOpen ? TEXT("opened") : TEXT("closed"),
				   *MappedStateName.ToString());
		}
	}
}

bool UTcsStateComponent::AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const
{
	if (A.Num() != B.Num())
	{
		return false;
	}

	for (int32 i = 0; i < A.Num(); ++i)
	{
		if (A[i] != B[i])
		{
			return false;
		}
	}

	return true;
}

void UTcsStateComponent::UpdateStateSlotActivation(FGameplayTag SlotTag)
{
	// 委托给 Subsystem 处理
	if (IsValid(StateMgr))
	{
		StateMgr->UpdateStateSlotActivation(this, SlotTag);
	}
}