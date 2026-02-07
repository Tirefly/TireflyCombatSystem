// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"

#include "Engine/DataTable.h"
#include "StateTree.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsEntityInterface.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "State/StateMerger/TcsStateMerger.h"
#include "State/SamePriorityPolicy/TcsStateSamePriorityPolicy.h"
#include "TcsGameplayTags.h"


void UTcsStateManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 加载状态定义表
    StateDefTable = UTcsGenericLibrary::GetStateDefTable();
    if (!IsValid(StateDefTable))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateDefTable in TcsDevSettings is not valid"),
            *FString(__FUNCTION__));
        return;
    }

    // 加载状态槽定义表
    StateSlotDefTable = UTcsGenericLibrary::GetStateSlotDefTable();
    if (!IsValid(StateSlotDefTable))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDefTable in TcsDevSettings is not valid"),
            *FString(__FUNCTION__));
    }
    InitStateSlotDefs();
}

bool UTcsStateManagerSubsystem::GetStateDefinition(FName StateDefId, FTcsStateDefinition& OutStateDef)
{
    if (StateDefId.IsNone())
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid state definition id"),
            *FString(__FUNCTION__));
        return false;
    }

    if (!IsValid(StateDefTable))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateDefTable in TcsDevSettings is not valid"),
            *FString(__FUNCTION__));
        return false;
    }

    const FTcsStateDefinition* StateDefRow = StateDefTable->FindRow<FTcsStateDefinition>(
        StateDefId,
        TEXT("TCS GetStateDefinition"),
        true);
    if (!StateDefRow)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid state definition: %s"),
            *FString(__FUNCTION__),
            *StateDefId.ToString());
        return false;
    }

    OutStateDef = *StateDefRow;

	// Priority: larger is higher; clamp invalid negative values.
	if (OutStateDef.Priority < 0)
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] StateDef %s has invalid Priority %d, clamped to 0."),
			*FString(__FUNCTION__),
			*StateDefId.ToString(),
			OutStateDef.Priority);
		OutStateDef.Priority = 0;
	}
    return true;
}

UTcsStateInstance* UTcsStateManagerSubsystem::CreateStateInstance(
    FName StateDefRowId,
    AActor* Owner,
    AActor* Instigator,
	int32 InLevel)
{
    if (!IsValid(Owner) || !IsValid(Instigator))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid owner or instigator to create state instance %s"),
            *FString(__FUNCTION__),
            *StateDefRowId.ToString());
        return nullptr;
    }

    // 获取状态定义
    FTcsStateDefinition StateDef;
	if (!Owner->Implements<UTcsEntityInterface>() || !Instigator->Implements<UTcsEntityInterface>())
	{
		UE_LOG(LogTcsState, Error,
			TEXT("[%s] Owner or Instigator does not implement TcsEntityInterface. StateDef=%s Owner=%s Instigator=%s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString(),
			*Owner->GetName(),
			*Instigator->GetName());
		return nullptr;
	}

	if (!GetStateDefinition(StateDefRowId, StateDef))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid state definition: %s"),
			*FString(__FUNCTION__),
			*StateDefRowId.ToString());
		return nullptr;
	}
    
    // 创建状态实例
    UTcsStateInstance* StateInstance = NewObject<UTcsStateInstance>(Owner);
    if (IsValid(StateInstance))
    {
        // 初始化状态实例
        StateInstance->SetStateDefId(StateDefRowId);
        StateInstance->Initialize(StateDef, Owner, Instigator, ++GlobalStateInstanceIdMgr, InLevel);

		if (!StateInstance->IsInitialized())
		{
			UE_LOG(LogTcsState, Error,
				TEXT("[%s] Failed to initialize StateInstance. StateDef=%s Owner=%s Instigator=%s"),
				*FString(__FUNCTION__),
				*StateDefRowId.ToString(),
				*Owner->GetName(),
				*Instigator->GetName());
			StateInstance->MarkPendingGC();
			return nullptr;
		}
        // 标记应用时间戳（用于合并器等逻辑）
        StateInstance->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());
    }
    
    return StateInstance;
}

bool UTcsStateManagerSubsystem::TryApplyStateToTarget(
    AActor* Target,
    FName StateDefId,
    AActor* Instigator)
{
    if (!IsValid(Target) || !IsValid(Instigator))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid target or instigator to apply state %s"),
            *FString(__FUNCTION__),
            *StateDefId.ToString());
        return false;
    }

    FTcsStateDefinition StateDef;
    if (!GetStateDefinition(StateDefId, StateDef))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid state definition: %s"),
            *FString(__FUNCTION__),
            *StateDefId.ToString());
        return false;
    }

	UTcsStateComponent* TargetStateCmp = UTcsGenericLibrary::GetStateComponent(Target);
	if (!IsValid(TargetStateCmp))
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] Target does not have state component: %s"),
			*FString(__FUNCTION__),
			*Target->GetName());
		if (Target)
		{
			// 无法获取StateComponent时，只能依赖日志提示
		}
		return false;
	}

	UTcsStateInstance* NewStateInstance = CreateStateInstance(StateDefId, Target, Instigator, 1);
	if (!IsValid(NewStateInstance))
	{
		TargetStateCmp->NotifyStateApplyFailed(
			Target,
			StateDefId,
			ETcsStateApplyFailReason::CreateInstanceFailed,
			TEXT("Failed to create StateInstance."));
		return false;
	}

    return TryApplyStateInstance(NewStateInstance);
}

bool UTcsStateManagerSubsystem::TryApplyStateInstance(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."),
            *FString(__FUNCTION__));;
        return false;
    }

	if (!StateInstance->IsInitialized())
	{
		UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is not initialized. StateDef=%s"),
			*FString(__FUNCTION__),
			*StateInstance->GetStateDefId().ToString());

		if (UTcsStateComponent* OwnerStateCmp = StateInstance->GetOwnerStateComponent())
		{
			OwnerStateCmp->NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::InvalidInput,
				TEXT("StateInstance is not initialized."));
		}
		return false;
	}
    
    // TODO: 未来实现CheckImmunity

    // 检查应用条件
    if (!CheckStateApplyConditions(StateInstance))
    {
		if (UTcsStateComponent* OwnerStateCmp = StateInstance->GetOwnerStateComponent())
		{
			OwnerStateCmp->NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::ApplyConditionsFailed,
				TEXT("State apply conditions check failed."));
		}
        return false;
    }

    // 尝试附加到状态槽中
    return TryAssignStateToStateSlot(StateInstance);
}

bool UTcsStateManagerSubsystem::CheckStateApplyConditions(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."),
            *FString(__FUNCTION__));;
        return false;
    }
    
    const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
    // 检查所有激活条件
    for (int i = 0; i < StateDef.ActiveConditions.Num(); ++i)
    {
        if (!StateDef.ActiveConditions[i].IsValid())
        {
            UE_LOG(LogTcsState, Error, TEXT("[%s] Invalid state condition of index %d in StateDef %s"),
                *FString(__FUNCTION__),
                i,
                *StateInstance->GetStateDefId().ToString());
            continue;
        }

        if (!StateDef.ActiveConditions[i].bCheckWhenApplying)
        {
            continue;
        }

        // 获取条件实例并检查，有一个不满足则返回false
        UTcsStateCondition* Condition = StateDef.ActiveConditions[i].ConditionClass.GetDefaultObject();
        if (!Condition->CheckCondition(StateInstance, StateDef.ActiveConditions[i].Payload))
        {
            return false;
        }
    }

    return true;
}

bool UTcsStateManagerSubsystem::TryGetStateSlotDefinition(
    FGameplayTag StateSlotTag,
    FTcsStateSlotDefinition& OutStateSlotDef) const
{
    if (StateSlotDefs.Contains(StateSlotTag))
    {
        OutStateSlotDef = StateSlotDefs[StateSlotTag];
        return true;
    }

    return false;
}

void UTcsStateManagerSubsystem::InitStateSlotDefs()
{
    if (!IsValid(StateSlotDefTable))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDefTable in TcsDevSettings is not valid"),
            *FString(__FUNCTION__));
        return;
    }

    // 读取所有配置行
    TArray<FTcsStateSlotDefinition*> StateSlotDefRows;
    StateSlotDefTable->GetAllRows<FTcsStateSlotDefinition>(*FString(__FUNCTION__), StateSlotDefRows);
    for (const FTcsStateSlotDefinition* Row : StateSlotDefRows)
    {
        if (Row && Row->SlotTag.IsValid())
        {
            StateSlotDefs.FindOrAdd(Row->SlotTag, *Row);
            UE_LOG(LogTcsState, Log, TEXT("[%s], Loaded state slot def: [%s] -> Mode: %s"), 
                *FString(__FUNCTION__),
                *Row->SlotTag.ToString(),
                *StaticEnum<ETcsStateSlotActivationMode>()->GetNameStringByValue(
                    static_cast<int64>(Row->ActivationMode)));
        }
    }
}

void UTcsStateManagerSubsystem::InitStateSlotMappings(AActor* CombatEntity)
{
    UTcsStateComponent* StateComponent = UTcsGenericLibrary::GetStateComponent(CombatEntity);
    if (!IsValid(StateComponent))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] CombatEntity %s does not have StateComponent"),
            *FString(__FUNCTION__),
            *CombatEntity->GetName());
        return;
    }

    // 清空现有映射
    StateComponent->Mapping_StateSlotToStateHandle.Empty();
    StateComponent->Mapping_StateHandleToStateSlot.Empty();

	// Ensure all StateSlots exist based on StateSlotDefs, regardless of optional StateTree visual mapping.
	// (StateTreeStateName mapping should only affect Gate automation, not whether a Slot can accept states.)
	for (const auto& Pair : StateSlotDefs)
	{
		const FGameplayTag StateSlotTag = Pair.Key;
		if (StateSlotTag.IsValid())
		{
			StateComponent->StateSlotsX.FindOrAdd(StateSlotTag);
		}
	}

    // 获取 StateTree
    const UStateTree* StateTree = StateComponent->GetStateTree();
    if (!IsValid(StateTree))
    {
        UE_LOG(LogTcsState, Verbose, TEXT("[%s] No StateTree assigned on StateComponent of %s"),
            *FString(__FUNCTION__),
            *CombatEntity->GetName());
        return;
    }

    // 遍历配置中包含 StateTreeStateName 的槽位定义，尝试匹配到 StateTree 状态
    for (const auto& Pair : StateSlotDefs)
    {
        const FGameplayTag StateSlotTag = Pair.Key;
        const FTcsStateSlotDefinition& StateSlotDef = Pair.Value;
        // 若未指定映射，跳过
        if (StateSlotDef.StateTreeStateName.IsNone())
        {
            continue;
        }

        // 迭代StateTree的运行时状态，匹配名字
        bool bMapped = false;
        // 注意：GetStates() 在5.6可用；若未来API变化，可改为其它查找方案
        const TArrayView<const FCompactStateTreeState> States = StateTree->GetStates();
        for (int32 Index = 0; Index < States.Num(); ++Index)
        {
            const FCompactStateTreeState& State = States[Index];
            if (State.Name == StateSlotDef.StateTreeStateName)
            {
                FStateTreeStateHandle Handle(Index);
                StateComponent->Mapping_StateSlotToStateHandle.Add(StateSlotTag, Handle);
                StateComponent->Mapping_StateHandleToStateSlot.Add(Handle, StateSlotTag);
				StateComponent->StateSlotsX.FindOrAdd(StateSlotTag);
                bMapped = true;
                break;
            }
        }

        UE_LOG(LogTcsState, Log, TEXT("[%s] State Slot [%s] -> StateTree State [%s] %s of %s"),
            *FString(__FUNCTION__),
            *StateSlotTag.ToString(),
            *StateSlotDef.StateTreeStateName.ToString(),
            bMapped ? TEXT("mapped") : TEXT("not found"),
            *CombatEntity->GetName());
    }
}

bool UTcsStateManagerSubsystem::TryAssignStateToStateSlot(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."),
            *FString(__FUNCTION__));;
        return false;
    }

    const FTcsStateDefinition& StateDef = StateInstance->GetStateDef();
    if (!StateDef.StateSlotType.IsValid())
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateDef %s does not specify StateSlotType."),
            *FString(__FUNCTION__),
            *StateInstance->GetStateDefId().ToString());
		if (UTcsStateComponent* OwnerStateCmp = StateInstance->GetOwnerStateComponent())
		{
			OwnerStateCmp->NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::InvalidStateDefinition,
				TEXT("StateDef does not specify a valid StateSlotType."));
		}
        return false;
    }

    UTcsStateComponent* OwnerStateCmp = StateInstance->GetOwnerStateComponent();
    if (!IsValid(OwnerStateCmp))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance's owner does not have StateComponent."),
            *FString(__FUNCTION__));;
        return false;
    }

    FTcsStateSlot* StateSlot = OwnerStateCmp->StateSlotsX.Find(StateDef.StateSlotType);
    if (!StateSlot)
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlot %s not found in owner StateComponent."),
            *FString(__FUNCTION__),
            *StateDef.StateSlotType.ToString());
		OwnerStateCmp->NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlot,
			FString::Printf(TEXT("StateSlot %s not found."), *StateDef.StateSlotType.ToString()));
        return false;
    }

    const FTcsStateSlotDefinition* StateSlotDef = StateSlotDefs.Find(StateDef.StateSlotType);
    if (!StateSlotDef)
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDef %s not found."),
            *FString(__FUNCTION__),
            *StateDef.StateSlotType.ToString());
		OwnerStateCmp->NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::NoStateSlotDefinition,
			FString::Printf(TEXT("StateSlotDef %s not found."), *StateDef.StateSlotType.ToString()));
        return false;
    }

    // 清除槽位中过期的状态
    ClearStateSlotExpiredStates(OwnerStateCmp, StateSlot);

	// 若槽位已存在同一指针，直接拒绝（避免重复入槽）
	if (StateSlot->States.Contains(StateInstance))
	{
		OwnerStateCmp->NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::AlreadyInSlot,
			TEXT("StateInstance already exists in target slot."));
		return false;
	}

	// Gate关闭时，若策略为Cancel，则直接取消应用
	if (!StateSlot->bIsGateOpen && StateSlotDef->GateCloseBehavior == ETcsStateSlotGateClosePolicy::SSGCP_Cancel)
	{
		OwnerStateCmp->NotifyStateApplyFailed(
			StateInstance->GetOwner(),
			StateInstance->GetStateDefId(),
			ETcsStateApplyFailReason::SlotGateClosed_CancelPolicy,
			FString::Printf(TEXT("StateSlot %s gate is closed (Cancel policy)."), *StateDef.StateSlotType.ToString()));
		return false;
	}

	// PriorityOnly + CancelLowerPriority：低优先级直接拒绝入槽
	if (StateSlot->bIsGateOpen
		&& StateSlotDef->ActivationMode == ETcsStateSlotActivationMode::SSAM_PriorityOnly
		&& StateSlotDef->PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
	{
		bool bHasExisting = false;
		int32 BestPriority = TNumericLimits<int32>::Lowest();
		for (const UTcsStateInstance* Existing : StateSlot->States)
		{
			if (!IsValid(Existing) || Existing->HasPendingRemovalRequest() || Existing->GetCurrentStage() == ETcsStateStage::SS_Expired)
			{
				continue;
			}
			bHasExisting = true;
			BestPriority = FMath::Max(BestPriority, Existing->GetStateDef().Priority);
		}

		// Priority值越大越高；如果新状态Priority更小，则更低优先级，拒绝入槽。
		if (bHasExisting && StateInstance->GetStateDef().Priority < BestPriority)
		{
			OwnerStateCmp->NotifyStateApplyFailed(
				StateInstance->GetOwner(),
				StateInstance->GetStateDefId(),
				ETcsStateApplyFailReason::LowerPriorityRejected,
				TEXT("State application rejected: lower priority than existing state in PriorityOnly slot."));
			return false;
		}
	}

	// 加入到槽位中，等待后续激活判定
	StateSlot->States.Add(StateInstance);

	// 如果是持续性状态，加入持久化列表
	if (StateInstance->GetStateDef().DurationType != ETcsStateDurationType::SDT_None)
	{
		// 如果是有持续时间的状态，加入持续时间映射
		if (StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Duration)
		{
			OwnerStateCmp->DurationTracker.Add(StateInstance, StateInstance->GetTotalDuration());
		}
	}

	// 若Gate关闭但允许入槽，设置应用后阶段（不启动StateTree）
	if (!StateSlot->bIsGateOpen)
	{
		const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
		switch (StateSlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp);
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
		default:
			StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause);
			break;
		}
		OwnerStateCmp->NotifyStateStageChanged(StateInstance, PreviousStage, StateInstance->GetCurrentStage());
	}

    // 更新槽位中所有状态的激活流程（使用延迟请求机制）
    RequestUpdateStateSlotActivation(OwnerStateCmp, StateDef.StateSlotType);

	// 发送应用成功事件（AppliedStage 以更新后的阶段为准）
	if (StateSlot->States.Contains(StateInstance) && StateInstance->GetCurrentStage() != ETcsStateStage::SS_Expired)
	{
		OwnerStateCmp->StateInstanceIndex.AddInstance(StateInstance);
	}

	OwnerStateCmp->NotifyStateApplySuccess(
		StateInstance->GetOwner(),
		StateInstance->GetStateDefId(),
		StateInstance,
		StateDef.StateSlotType,
		StateInstance->GetCurrentStage());
    return true;
}

void UTcsStateManagerSubsystem::ClearStateSlotExpiredStates(
    UTcsStateComponent* StateComponent,
    FTcsStateSlot* StateSlot)
{
    if (!IsValid(StateComponent))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateComponent is null."),
            *FString(__FUNCTION__));
        return;
    }
    
    if (!StateSlot)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot is null."),
            *FString(__FUNCTION__));
        return;
    }

    StateSlot->States.RemoveAll([StateComponent](UTcsStateInstance* State) {
        if (!IsValid(State))
        {
            return true;
        }
        if (State->GetCurrentStage() == ETcsStateStage::SS_Expired)
        {
			StateComponent->StateTreeTickScheduler.Remove(State);
			StateComponent->DurationTracker.Remove(State);
			StateComponent->StateInstanceIndex.RemoveInstance(State);
            return true;
        }
        return false;
    });
	StateComponent->StateTreeTickScheduler.RefreshInstances();
	StateComponent->DurationTracker.RefreshInstances();
	StateComponent->StateInstanceIndex.RefreshInstances();
}

void UTcsStateManagerSubsystem::RefreshSlotsForStateChange(
	UTcsStateComponent* StateComponent,
	const TArray<FName>& NewStates,
	const TArray<FName>& OldStates)
{
	if (!IsValid(StateComponent))
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateComponent"),
			*FString(__FUNCTION__));
		return;
	}

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
	for (const auto& Pair : StateComponent->Mapping_StateSlotToStateHandle)
	{
		const FGameplayTag SlotTag = Pair.Key;
		FTcsStateSlotDefinition SlotDef;

		if (!TryGetStateSlotDefinition(SlotTag, SlotDef))
		{
			continue;
		}

		const FName& MappedStateName = SlotDef.StateTreeStateName;
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

		const bool bWasOpen = StateComponent->IsSlotGateOpen(SlotTag);
		if (bShouldOpen != bWasOpen)
		{
			StateComponent->SetSlotGateOpen(SlotTag, bShouldOpen);

			UE_LOG(LogTcsState, Log,
				   TEXT("[StateTree Event] Slot [%s] gate %s due to StateTree state '%s'"),
				   *SlotTag.ToString(),
				   bShouldOpen ? TEXT("opened") : TEXT("closed"),
				   *MappedStateName.ToString());
		}
	}
}

void UTcsStateManagerSubsystem::RequestUpdateStateSlotActivation(
	UTcsStateComponent* StateComponent,
	FGameplayTag StateSlotTag)
{
	if (!IsValid(StateComponent) || !StateSlotTag.IsValid())
	{
		UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateComponent or StateSlotTag"),
			*FString(__FUNCTION__));
		return;
	}

	if (bIsUpdatingSlotActivation)
	{
		// 正在更新，延迟请求
		PendingSlotActivationUpdates.FindOrAdd(StateComponent).Add(StateSlotTag);
		UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deferred slot activation update: Component=%s Slot=%s"),
			*FString(__FUNCTION__),
			*StateComponent->GetOwner()->GetName(),
			*StateSlotTag.ToString());
	}
	else
	{
		// 不在更新，立即执行
		UpdateStateSlotActivation(StateComponent, StateSlotTag);
	}
}

void UTcsStateManagerSubsystem::DrainPendingSlotActivationUpdates()
{
	const int32 MaxIterations = 10;
	int32 Iteration = 0;

	while (!PendingSlotActivationUpdates.IsEmpty() && Iteration < MaxIterations)
	{
		// 拷贝队列（避免遍历时修改）
		TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> ToProcess = PendingSlotActivationUpdates;
		PendingSlotActivationUpdates.Empty();

		for (const auto& Pair : ToProcess)
		{
			UTcsStateComponent* StateComponent = Pair.Key.Get();
			const TSet<FGameplayTag>& SlotTags = Pair.Value;

			if (IsValid(StateComponent))
			{
				for (const FGameplayTag& SlotTag : SlotTags)
				{
					UpdateStateSlotActivation(StateComponent, SlotTag);
				}
			}
		}

		Iteration++;
	}

	if (Iteration >= MaxIterations)
	{
		UE_LOG(LogTcsState, Warning,
			TEXT("[%s] Max iterations (%d) reached, possible infinite loop. Remaining pending updates: %d"),
			*FString(__FUNCTION__),
			MaxIterations,
			PendingSlotActivationUpdates.Num());
		// 清空剩余请求，避免下次继续循环
		PendingSlotActivationUpdates.Empty();
	}
}

void UTcsStateManagerSubsystem::UpdateStateSlotActivation(
    UTcsStateComponent* StateComponent,
    FGameplayTag StateSlotTag)
{
    // 设置更新标志，防止递归调用
    bIsUpdatingSlotActivation = true;

    // 1. 参数验证和数据准备
    if (!IsValid(StateComponent) || !StateSlotTag.IsValid())
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Invalid StateComponent or StateSlotTag"),
            *FString(__FUNCTION__));
        bIsUpdatingSlotActivation = false;
        DrainPendingSlotActivationUpdates();
        return;
    }

    FTcsStateSlot* StateSlot = StateComponent->StateSlotsX.Find(StateSlotTag);
    if (!StateSlot)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
            *FString(__FUNCTION__),
            *StateSlotTag.ToString());
        bIsUpdatingSlotActivation = false;
        DrainPendingSlotActivationUpdates();
        return;
    }

    // 2. 清理过期状态
    ClearStateSlotExpiredStates(StateComponent, StateSlot);

    // 3. 优先级排序
    SortStatesByPriority(StateSlot->States);

    // 4. 执行合并
    ProcessStateSlotMerging(StateSlot);

    // 5. 强制Gate一致性（确保Gate关闭时槽内无Active状态）
	EnforceSlotGateConsistency(StateComponent, StateSlotTag);

	// Gate关闭时，不允许激活任何状态（避免后续ActivationMode将状态误激活）
	if (!StateSlot->bIsGateOpen)
	{
		CleanupInvalidStates(StateSlot);
        bIsUpdatingSlotActivation = false;
        DrainPendingSlotActivationUpdates();
		return;
	}

    // 6. 处理激活模式
    ProcessStateSlotByActivationMode(StateComponent, StateSlot, StateSlotTag);

    // 7. 最终清理
    CleanupInvalidStates(StateSlot);

    // 清除更新标志并排空待处理队列
    bIsUpdatingSlotActivation = false;
    DrainPendingSlotActivationUpdates();
}

void UTcsStateManagerSubsystem::SortStatesByPriority(TArray<UTcsStateInstance*>& States)
{
    States.Sort([](const UTcsStateInstance& A, const UTcsStateInstance& B)
    {
        return A.GetStateDef().Priority > B.GetStateDef().Priority;
    });
}

void UTcsStateManagerSubsystem::ProcessStateSlotMerging(FTcsStateSlot* StateSlot)
{
    if (!StateSlot)
    {
        return;
    }

    // 按DefId分组
    TMap<FName, TArray<UTcsStateInstance*>> StatesByDefId;
	TArray<UTcsStateInstance*> AlwaysKeepStates;
    for (UTcsStateInstance* State : StateSlot->States)
    {
		if (!IsValid(State))
		{
			continue;
		}

		// Do not merge/remove instances that are handling a pending removal request.
		if (State->HasPendingRemovalRequest())
		{
			AlwaysKeepStates.Add(State);
			continue;
		}

		StatesByDefId.FindOrAdd(State->GetStateDefId()).Add(State);
    }

	// 对每组执行合并
    TArray<UTcsStateInstance*> AllMergedStates;
	TMap<FName, UTcsStateInstance*> MergePrimaryByDefId;
    for (auto& Pair : StatesByDefId)
    {
        TArray<UTcsStateInstance*> MergedGroup;
        MergeStateGroup(Pair.Value, MergedGroup);
        AllMergedStates.Append(MergedGroup);
		if (MergedGroup.Num() > 0 && IsValid(MergedGroup[0]))
		{
			MergePrimaryByDefId.Add(Pair.Key, MergedGroup[0]);
		}
    }

	AllMergedStates.Append(AlwaysKeepStates);

    // 移除未被保留的状态
	// 注意：合并后的移除需要事件与容器清理，因此要求调用方传入StateComponent
	// 这里由调用方在UpdateStateSlotActivation阶段保证可用
	// (StateComponent 从状态实例Owner组件侧获取)
	UTcsStateComponent* StateComponent = nullptr;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State))
		{
			StateComponent = State->GetOwnerStateComponent();
			break;
		}
	}
	RemoveUnmergedStates(StateComponent, StateSlot, AllMergedStates, MergePrimaryByDefId);
}

void UTcsStateManagerSubsystem::EnforceSlotGateConsistency(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag)
{
	if (!IsValid(StateComponent) || !StateSlotTag.IsValid())
	{
		return;
	}

	FTcsStateSlot* StateSlot = StateComponent->StateSlotsX.Find(StateSlotTag);
	if (!StateSlot)
	{
		return;
	}

	// Gate开启时无需处理
	if (StateSlot->bIsGateOpen)
	{
		return;
	}

	const FTcsStateSlotDefinition* SlotDef = StateSlotDefs.Find(StateSlotTag);
	if (!SlotDef)
	{
		return;
	}

	TArray<UTcsStateInstance*> StatesToCancel;
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (!IsValid(State))
		{
			continue;
		}

		const ETcsStateStage Stage = State->GetCurrentStage();
		if (Stage == ETcsStateStage::SS_Expired)
		{
			continue;
		}
		if (State->HasPendingRemovalRequest())
		{
			continue;
		}

		switch (SlotDef->GateCloseBehavior)
		{
		case ETcsStateSlotGateClosePolicy::SSGCP_HangUp:
			// Gate关闭：不允许Active；HangUp策略只收敛Active -> HangUp。
			if (Stage == ETcsStateStage::SS_Active)
			{
				HangUpState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Pause:
			// Pause策略：收敛 Active/HangUp -> Pause，确保完全冻结。
			if (Stage == ETcsStateStage::SS_Active || Stage == ETcsStateStage::SS_HangUp)
			{
				PauseState(State);
			}
			break;
		case ETcsStateSlotGateClosePolicy::SSGCP_Cancel:
			// Cancel策略：所有未过期状态均应被取消并移除。
			StatesToCancel.Add(State);
			break;
		}
	}

	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (!IsValid(State))
		{
			continue;
		}
		CancelState(State);
	}

	// 最终保障：Gate关闭时不允许Active残留（若出现，强制HangUp）
	for (UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
		{
			HangUpState(State);
		}
	}

	// 开发模式下的不变量检查：Gate关闭后，槽内不应存在Active状态
	#if !UE_BUILD_SHIPPING
	for (const UTcsStateInstance* State : StateSlot->States)
	{
		if (IsValid(State) && !State->HasPendingRemovalRequest())
		{
			checkf(State->GetCurrentStage() != ETcsStateStage::SS_Active,
				TEXT("[EnforceSlotGateConsistency] Invariant violation: Active state found in closed gate slot. Slot=%s State=%s Stage=%s"),
				*StateSlotTag.ToString(),
				*State->GetStateDefId().ToString(),
				*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(State->GetCurrentStage())));
		}
	}
	#endif
}

void UTcsStateManagerSubsystem::MergeStateGroup(
    TArray<UTcsStateInstance*>& StatesToMerge,
    TArray<UTcsStateInstance*>& OutMergedStates)
{
    if (StatesToMerge.Num() == 0)
    {
        return;
    }

    FTcsStateDefinition StateDef;
    if (!GetStateDefinition(StatesToMerge[0]->GetStateDefId(), StateDef))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get state definition for %s"),
            *FString(__FUNCTION__),
            *StatesToMerge[0]->GetStateDefId().ToString());
        OutMergedStates = StatesToMerge;
        return;
    }

    TSubclassOf<UTcsStateMerger> MergerClass = StateDef.MergerType;
    if (!MergerClass)
    {
        // 没有合并器，保留所有状态
        OutMergedStates = StatesToMerge;
        return;
    }

    UTcsStateMerger* Merger = MergerClass->GetDefaultObject<UTcsStateMerger>();
    if (!IsValid(Merger))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] Failed to get merger instance for %s"),
            *FString(__FUNCTION__),
            *MergerClass->GetName());
        OutMergedStates = StatesToMerge;
        return;
    }

    Merger->Merge(StatesToMerge, OutMergedStates);
}

void UTcsStateManagerSubsystem::RemoveUnmergedStates(
	UTcsStateComponent* StateComponent,
    FTcsStateSlot* StateSlot,
    const TArray<UTcsStateInstance*>& MergedStates,
	const TMap<FName, UTcsStateInstance*>& MergePrimaryByDefId)
{
    if (!StateSlot)
    {
        return;
    }

    TArray<UTcsStateInstance*> StatesToRemove;
    for (UTcsStateInstance* State : StateSlot->States)
    {
		if (IsValid(State) && State->HasPendingRemovalRequest())
		{
			continue;
		}
        if (!MergedStates.Contains(State))
        {
            StatesToRemove.Add(State);
        }
    }

	for (UTcsStateInstance* State : StatesToRemove)
	{
		if (IsValid(StateComponent))
		{
			UTcsStateInstance* MergeTarget = nullptr;
			for (UTcsStateInstance* Candidate : MergedStates)
			{
				if (!IsValid(Candidate) || Candidate->GetStateDefId() != State->GetStateDefId())
				{
					continue;
				}

				// 优先选择相同Instigator的合并目标（支持StackByInstigator）
				if (Candidate->GetInstigator() == State->GetInstigator())
				{
					MergeTarget = Candidate;
					break;
				}

				if (!MergeTarget)
				{
					MergeTarget = Candidate;
				}
			}

			// 回退：使用预缓存的主合并实例
			if (!IsValid(MergeTarget))
			{
				if (UTcsStateInstance* const* Primary = MergePrimaryByDefId.Find(State->GetStateDefId()))
				{
					MergeTarget = IsValid(*Primary) ? *Primary : nullptr;
				}
			}

			if (IsValid(MergeTarget))
			{
				StateComponent->NotifyStateMerged(MergeTarget, State, MergeTarget->GetStackCount());
			}
		}

		// 使用 RequestStateRemoval 统一移除路径，确保 StateTree 退场逻辑有机会执行
		FTcsStateRemovalRequest Request;
		Request.Reason = ETcsStateRemovalRequestReason::Custom;
		Request.CustomReason = FName("MergedOut");
		RequestStateRemoval(State, Request);
		// TODO(TCS): Merge-removed instances should be returned to pool when TireflyObjectPool refactor is complete.
    }
}

void UTcsStateManagerSubsystem::ProcessStateSlotByActivationMode(
    const UTcsStateComponent* StateComponent,
    FTcsStateSlot* StateSlot,
    FGameplayTag SlotTag)
{
    if (!IsValid(StateComponent) || !StateSlot)
    {
        return;
    }

    const FTcsStateSlotDefinition* SlotDef = StateSlotDefs.Find(SlotTag);
    if (!SlotDef)
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDef %s not found"),
            *FString(__FUNCTION__),
            *SlotTag.ToString());
        return;
    }

    switch (SlotDef->ActivationMode)
    {
    case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
        {
            ProcessPriorityOnlyMode(StateSlot, *SlotDef);
            break;
        }
    case ETcsStateSlotActivationMode::SSAM_AllActive:
        {
            ProcessAllActiveMode(StateSlot);
            break;
        }
    }
}

void UTcsStateManagerSubsystem::ProcessPriorityOnlyMode(
    FTcsStateSlot* StateSlot,
    const FTcsStateSlotDefinition& SlotDef)
{
    if (!StateSlot || StateSlot->States.Num() == 0)
    {
        return;
    }

    // 找到最高优先级
    int32 HighestPriority = TNumericLimits<int32>::Lowest();
    for (UTcsStateInstance* Candidate : StateSlot->States)
    {
        if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest())
        {
            HighestPriority = FMath::Max(HighestPriority, Candidate->GetStateDef().Priority);
        }
    }

    // 收集所有最高优先级的状态
    TArray<UTcsStateInstance*> HighestPriorityStates;
    for (UTcsStateInstance* Candidate : StateSlot->States)
    {
        if (IsValid(Candidate) && !Candidate->HasPendingRemovalRequest()
            && Candidate->GetStateDef().Priority == HighestPriority)
        {
            HighestPriorityStates.Add(Candidate);
        }
    }

    if (HighestPriorityStates.Num() == 0)
    {
        return;
    }

    // 如果有多个同优先级状态，应用策略排序
    UTcsStateInstance* HighestPriorityState = nullptr;
    if (HighestPriorityStates.Num() > 1 && SlotDef.SamePriorityPolicy)
    {
        const UTcsStateSamePriorityPolicy* Policy = SlotDef.SamePriorityPolicy->GetDefaultObject<UTcsStateSamePriorityPolicy>();
        if (IsValid(Policy))
        {
            // 按策略排序（降序，越大越靠前）
            HighestPriorityStates.Sort([Policy](const UTcsStateInstance& A, const UTcsStateInstance& B)
            {
                int64 KeyA = Policy->GetOrderKey(&A);
                int64 KeyB = Policy->GetOrderKey(&B);
                return KeyA > KeyB;
            });
        }
    }

    // 选择第一个作为激活状态
    HighestPriorityState = HighestPriorityStates[0];

    if (!IsValid(HighestPriorityState))
    {
        return;
    }

    // 激活最高优先级状态
    if (HighestPriorityState->GetCurrentStage() != ETcsStateStage::SS_Active)
    {
        ActivateState(HighestPriorityState);
    }

	TArray<UTcsStateInstance*> StatesToCancel;

    // 按照低优先级抢占策略，处理其他低优先级状态
    for (int32 i = 0; i < StateSlot->States.Num(); ++i)
    {
        UTcsStateInstance* State = StateSlot->States[i];
        if (!IsValid(State))
        {
            continue;
        }
		if (State->HasPendingRemovalRequest())
		{
			continue;
		}
		if (State == HighestPriorityState)
		{
			continue;
		}

		if (SlotDef.PreemptionPolicy == ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
		{
			StatesToCancel.Add(State);
			continue;
		}

		ApplyPreemptionPolicyToState(State, SlotDef.PreemptionPolicy);
    }

	// 统一处理Cancel策略，避免遍历时修改容器
	for (UTcsStateInstance* State : StatesToCancel)
	{
		if (!IsValid(State))
		{
			continue;
		}
		CancelState(State);
	}
}

void UTcsStateManagerSubsystem::ProcessAllActiveMode(FTcsStateSlot* StateSlot)
{
    if (!StateSlot)
    {
        return;
    }

    // 激活所有状态
    for (UTcsStateInstance* State : StateSlot->States)
    {
        if (IsValid(State) && !State->HasPendingRemovalRequest() && State->GetCurrentStage() != ETcsStateStage::SS_Active)
        {
            ActivateState(State);
        }
    }
}

void UTcsStateManagerSubsystem::ApplyPreemptionPolicyToState(
    UTcsStateInstance* State,
    ETcsStatePreemptionPolicy Policy)
{
    if (!IsValid(State))
    {
        return;
    }

    switch (Policy)
    {
    case ETcsStatePreemptionPolicy::SPP_HangUpLowerPriority:
        if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
        {
            HangUpState(State);
        }
        break;

    case ETcsStatePreemptionPolicy::SPP_PauseLowerPriority:
        if (State->GetCurrentStage() == ETcsStateStage::SS_Active)
        {
            PauseState(State);
        }
        break;

    case ETcsStatePreemptionPolicy::SPP_CancelLowerPriority:
		// Cancel策略需要由调用方负责移除（避免遍历时修改容器）
		break;
    }
}

void UTcsStateManagerSubsystem::CleanupInvalidStates(FTcsStateSlot* StateSlot)
{
    if (!StateSlot)
    {
        return;
    }

    StateSlot->States.RemoveAll([](const UTcsStateInstance* State)
    {
        return !IsValid(State);
    });
}

void UTcsStateManagerSubsystem::RemoveStateFromSlot(
    FTcsStateSlot* StateSlot,
    UTcsStateInstance* State,
	bool bDeactivateIfNeeded)
{
    if (!StateSlot || !IsValid(State))
    {
        return;
    }

    // 从槽位中移除
    StateSlot->States.Remove(State);

    // 如果状态不是Inactive阶段，先停用它（可选）
    if (bDeactivateIfNeeded && State->GetCurrentStage() != ETcsStateStage::SS_Inactive)
    {
        DeactivateState(State);
    }
}

void UTcsStateManagerSubsystem::ActivateState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    // 如果已经是激活状态,不重复激活
    const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
    if (PreviousStage == ETcsStateStage::SS_Active)
    {
        return;
    }

    UE_LOG(LogTcsState, Verbose, TEXT("[%s] Activating state: %s"),
        *FString(__FUNCTION__),
        *StateInstance->GetStateDefId().ToString());

    // 设置为激活阶段
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
    // 启动StateTree
	UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent();
	const ETcsStateTreeTickPolicy TickPolicy = StateInstance->GetStateDef().TickPolicy;
	switch (TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->StartStateTree();
		StateInstance->TickStateTree(0.f);
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		if (IsValid(StateComponent))
		{
			StateComponent->StateTreeTickScheduler.Remove(StateInstance);
		}
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->StartStateTree();
		if (IsValid(StateComponent))
		{
			StateComponent->StateTreeTickScheduler.Remove(StateInstance);
		}
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->StartStateTree();
		if (IsValid(StateComponent) && StateInstance->IsStateTreeRunning())
		{
			StateComponent->StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

    // 广播状态阶段变更事件
    if (IsValid(StateComponent))
    {
        StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
    }
}

void UTcsStateManagerSubsystem::DeactivateState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    // 如果已经是未激活状态,不重复停用
    const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();
    if (PreviousStage == ETcsStateStage::SS_Inactive)
    {
        return;
    }

    UE_LOG(LogTcsState, Verbose, TEXT("[%s] Deactivating state: %s"),
        *FString(__FUNCTION__),
        *StateInstance->GetStateDefId().ToString());

    // 设置为未激活阶段
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Inactive);
    // Stop ticking StateTree (do not stop/lose its internal data)
    StateInstance->PauseStateTree();

    // 广播状态阶段变更事件
    if (UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent())
    {
		StateComponent->StateTreeTickScheduler.Remove(StateInstance);
        StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Inactive);
		if (PreviousStage == ETcsStateStage::SS_Active)
		{
			StateComponent->NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Inactive, FName("Deactivated"));
		}
    }
}

void UTcsStateManagerSubsystem::HangUpState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

    UE_LOG(LogTcsState, Verbose, TEXT("[%s] Hanging up state: %s"),
        *FString(__FUNCTION__),
        *StateInstance->GetStateDefId().ToString());

    // 设置为挂起阶段
    StateInstance->SetCurrentStage(ETcsStateStage::SS_HangUp);
    // 暂停StateTree执行 (但保持实例存活)
    StateInstance->PauseStateTree();

    // 广播状态阶段变更事件
    if (UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent())
    {
		StateComponent->StateTreeTickScheduler.Remove(StateInstance);
        StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_HangUp);
		if (PreviousStage == ETcsStateStage::SS_Active)
		{
			StateComponent->NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_HangUp, FName("HangUp"));
		}
    }
}

void UTcsStateManagerSubsystem::ResumeState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

    UE_LOG(LogTcsState, Verbose, TEXT("[%s] Resuming state: %s"),
        *FString(__FUNCTION__),
        *StateInstance->GetStateDefId().ToString());

    // 恢复到激活阶段
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Active);
    // 恢复StateTree执行
	UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent();
	const ETcsStateTreeTickPolicy TickPolicy = StateInstance->GetStateDef().TickPolicy;
	switch (TickPolicy)
	{
	case ETcsStateTreeTickPolicy::RunOnce:
		StateInstance->ResumeStateTree();
		StateInstance->TickStateTree(0.f);
		if (StateInstance->IsStateTreeRunning())
		{
			UE_LOG(LogTcsState, Warning, TEXT("[%s] StateTree TickPolicy=RunOnce but it is still running: %s, force stopping."),
				*FString(__FUNCTION__),
				*StateInstance->GetStateDefId().ToString());
			StateInstance->StopStateTree();
		}
		if (IsValid(StateComponent))
		{
			StateComponent->StateTreeTickScheduler.Remove(StateInstance);
		}
		break;
	case ETcsStateTreeTickPolicy::ManualOnly:
		StateInstance->ResumeStateTree();
		if (IsValid(StateComponent))
		{
			StateComponent->StateTreeTickScheduler.Remove(StateInstance);
		}
		break;
	case ETcsStateTreeTickPolicy::WhileActive:
	default:
		StateInstance->ResumeStateTree();
		if (IsValid(StateComponent) && StateInstance->IsStateTreeRunning())
		{
			StateComponent->StateTreeTickScheduler.Add(StateInstance);
		}
		break;
	}

    // 广播状态阶段变更事件
    if (IsValid(StateComponent))
    {
        StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Active);
    }
}

void UTcsStateManagerSubsystem::PauseState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

    const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

    UE_LOG(LogTcsState, Verbose, TEXT("[%s] Pausing state: %s"),
        *FString(__FUNCTION__),
        *StateInstance->GetStateDefId().ToString());

    // 设置为完全暂停阶段
    // 注意: Pause与HangUp的区别是,Pause会完全冻结状态(包括持续时间计算),
    // 而HangUp只是暂停逻辑执行但仍计算持续时间
    StateInstance->SetCurrentStage(ETcsStateStage::SS_Pause);
    // 完全暂停StateTree
    StateInstance->PauseStateTree();

    // 广播状态阶段变更事件
    if (UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent())
    {
		StateComponent->StateTreeTickScheduler.Remove(StateInstance);
        StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Pause);
		if (PreviousStage == ETcsStateStage::SS_Active)
		{
			StateComponent->NotifyStateDeactivated(StateInstance, ETcsStateStage::SS_Pause, FName("Pause"));
		}
    }
}

void UTcsStateManagerSubsystem::CancelState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

	FTcsStateRemovalRequest Request;
	Request.Reason = ETcsStateRemovalRequestReason::Cancelled;
	RequestStateRemoval(StateInstance, Request);
}

void UTcsStateManagerSubsystem::ExpireState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return;
    }

	FTcsStateRemovalRequest Request;
	Request.Reason = ETcsStateRemovalRequestReason::Expired;
	RequestStateRemoval(StateInstance, Request);

	// 从槽位中移除并刷新槽位（自然过期应立刻反映到槽位激活结果）
}

bool UTcsStateManagerSubsystem::RequestStateRemoval(UTcsStateInstance* StateInstance, FTcsStateRemovalRequest Request)
{
	if (!IsValid(StateInstance))
	{
		return false;
	}

	UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent();
	if (!IsValid(StateComponent))
	{
		return false;
	}

	if (StateInstance->GetCurrentStage() == ETcsStateStage::SS_Expired)
	{
		return true;
	}

	StateComponent->StateInstanceIndex.AddInstance(StateInstance);
	StateInstance->SetPendingRemovalRequest(Request);

	if (!StateInstance->IsStateTreeRunning())
	{
		StateInstance->StartStateTree();
	}

	if (!StateInstance->IsStateTreeRunning())
	{
		const FName ReasonName = Request.ToRemovalReasonName();
		StateInstance->ClearPendingRemovalRequest();
		FinalizeStateRemoval(StateInstance, ReasonName);
		return true;
	}

	FInstancedStruct Payload;
	Payload.InitializeAs(FTcsStateRemovalRequest::StaticStruct());
	*Payload.GetMutablePtr<FTcsStateRemovalRequest>() = Request;

	StateInstance->SendStateTreeEvent(TcsGameplayTags::Event_RemovalRequested, Payload);
	StateComponent->StateTreeTickScheduler.Add(StateInstance);

	StateInstance->TickStateTree(0.f);
	if (!StateInstance->IsStateTreeRunning() && StateInstance->HasPendingRemovalRequest())
	{
		FinalizePendingRemovalRequest(StateInstance);
	}

	return true;
}

void UTcsStateManagerSubsystem::FinalizePendingRemovalRequest(UTcsStateInstance* StateInstance)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	if (!StateInstance->HasPendingRemovalRequest())
	{
		return;
	}

	if (StateInstance->IsStateTreeRunning())
	{
		return;
	}

	const FName ReasonName = StateInstance->GetPendingRemovalRequest().ToRemovalReasonName();
	StateInstance->ClearPendingRemovalRequest();
	FinalizeStateRemoval(StateInstance, ReasonName);
}

bool UTcsStateManagerSubsystem::GetStatesInSlot(
    UTcsStateComponent* StateComponent,
    FGameplayTag SlotTag,
    TArray<UTcsStateInstance*>& OutStates) const
{
    OutStates.Empty();

    if (!IsValid(StateComponent) || !SlotTag.IsValid())
    {
        return false;
    }

    const FTcsStateSlot* StateSlot = StateComponent->StateSlotsX.Find(SlotTag);
    if (!StateSlot)
    {
        return false;
    }

    for (UTcsStateInstance* State : StateSlot->States)
    {
        if (IsValid(State))
        {
            OutStates.Add(State);
        }
    }

    return true;
}

bool UTcsStateManagerSubsystem::GetStatesByDefId(
    UTcsStateComponent* StateComponent,
    FName StateDefId,
    TArray<UTcsStateInstance*>& OutStates) const
{
    OutStates.Empty();

    if (!IsValid(StateComponent) || StateDefId.IsNone())
    {
        return false;
    }

    // 遍历所有槽位查找匹配的状态
    for (auto& Pair : StateComponent->StateSlotsX)
    {
        const FTcsStateSlot& StateSlot = Pair.Value;
        for (UTcsStateInstance* State : StateSlot.States)
        {
            if (IsValid(State) && State->GetStateDefId() == StateDefId)
            {
                OutStates.Add(State);
            }
        }
    }

    return OutStates.Num() > 0;
}

bool UTcsStateManagerSubsystem::GetAllActiveStates(
    UTcsStateComponent* StateComponent,
    TArray<UTcsStateInstance*>& OutStates) const
{
    OutStates.Empty();

    if (!IsValid(StateComponent))
    {
        return false;
    }

    // 遍历所有槽位查找活跃状态
    for (auto& Pair : StateComponent->StateSlotsX)
    {
        const FTcsStateSlot& StateSlot = Pair.Value;
        for (UTcsStateInstance* State : StateSlot.States)
        {
            if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
            {
                OutStates.Add(State);
            }
        }
    }

    return OutStates.Num() > 0;
}

bool UTcsStateManagerSubsystem::HasStateWithDefId(
    UTcsStateComponent* StateComponent,
    FName StateDefId) const
{
    if (!IsValid(StateComponent) || StateDefId.IsNone())
    {
        return false;
    }

    // 遍历所有槽位检查是否存在匹配的状态
    for (const auto& Pair : StateComponent->StateSlotsX)
    {
        const FTcsStateSlot& StateSlot = Pair.Value;
        for (const UTcsStateInstance* State : StateSlot.States)
        {
            if (IsValid(State) && State->GetStateDefId() == StateDefId)
            {
                return true;
            }
        }
    }

    return false;
}

bool UTcsStateManagerSubsystem::HasActiveStateInSlot(
    UTcsStateComponent* StateComponent,
    FGameplayTag SlotTag) const
{
    if (!IsValid(StateComponent) || !SlotTag.IsValid())
    {
        return false;
    }

    const FTcsStateSlot* StateSlot = StateComponent->StateSlotsX.Find(SlotTag);
    if (!StateSlot)
    {
        return false;
    }

    for (const UTcsStateInstance* State : StateSlot->States)
    {
        if (IsValid(State) && State->GetCurrentStage() == ETcsStateStage::SS_Active)
        {
            return true;
        }
    }

    return false;
}

bool UTcsStateManagerSubsystem::RemoveState(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateInstance is invalid"),
            *FString(__FUNCTION__));
        return false;
    }

    UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent();
    if (!IsValid(StateComponent))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateComponent is invalid"),
            *FString(__FUNCTION__));
        return false;
    }

    // 查找状态所在的槽位
    const FGameplayTag SlotTag = StateInstance->GetStateDef().StateSlotType;
    if (!StateComponent->StateSlotsX.Contains(SlotTag))
    {
        UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlot %s not found"),
            *FString(__FUNCTION__),
            *SlotTag.ToString());
        return false;
    }

	// 移除状态并从槽位中移除
	FTcsStateRemovalRequest Request;
	Request.Reason = ETcsStateRemovalRequestReason::Removed;
	return RequestStateRemoval(StateInstance, Request);
}

int32 UTcsStateManagerSubsystem::RemoveStatesByDefId(
    UTcsStateComponent* StateComponent,
    FName StateDefId,
    bool bRemoveAll)
{
    if (!IsValid(StateComponent) || StateDefId.IsNone())
    {
        return 0;
    }

    int32 RemovedCount = 0;

    // 遍历所有槽位查找匹配的状态
    for (auto& Pair : StateComponent->StateSlotsX)
    {
        FTcsStateSlot& StateSlot = Pair.Value;

        TArray<UTcsStateInstance*> StatesToRemove;
        for (UTcsStateInstance* State : StateSlot.States)
        {
            if (IsValid(State) && State->GetStateDefId() == StateDefId)
            {
                StatesToRemove.Add(State);
                if (!bRemoveAll)
                {
                    break;
                }
            }
        }

		for (UTcsStateInstance* State : StatesToRemove)
        {
			FTcsStateRemovalRequest Request;
			Request.Reason = ETcsStateRemovalRequestReason::Removed;
			RequestStateRemoval(State, Request);
            RemovedCount++;
        }

        if (!bRemoveAll && RemovedCount > 0)
        {
            break;
        }
    }

    return RemovedCount;
}

int32 UTcsStateManagerSubsystem::RemoveAllStatesInSlot(
    UTcsStateComponent* StateComponent,
    FGameplayTag SlotTag)
{
    if (!IsValid(StateComponent) || !SlotTag.IsValid())
    {
        return 0;
    }

    FTcsStateSlot* StateSlot = StateComponent->StateSlotsX.Find(SlotTag);
    if (!StateSlot)
    {
        return 0;
    }

    int32 RemovedCount = 0;
    TArray<UTcsStateInstance*> StatesToRemove = StateSlot->States;

    for (UTcsStateInstance* State : StatesToRemove)
    {
        if (IsValid(State))
        {
			FTcsStateRemovalRequest Request;
			Request.Reason = ETcsStateRemovalRequestReason::Removed;
			RequestStateRemoval(State, Request);
            RemovedCount++;
        }
    }

    return RemovedCount;
}

int32 UTcsStateManagerSubsystem::RemoveAllStates(UTcsStateComponent* StateComponent)
{
    if (!IsValid(StateComponent))
    {
        return 0;
    }

    int32 TotalRemoved = 0;

    // 遍历所有槽位
    for (auto& Pair : StateComponent->StateSlotsX)
    {
        FTcsStateSlot& StateSlot = Pair.Value;

        for (UTcsStateInstance* State : StateSlot.States)
        {
            if (IsValid(State))
            {
				FTcsStateRemovalRequest Request;
				Request.Reason = ETcsStateRemovalRequestReason::Removed;
				RequestStateRemoval(State, Request);
                TotalRemoved++;
            }
        }

    }

    return TotalRemoved;
}

void UTcsStateManagerSubsystem::FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason)
{
	if (!IsValid(StateInstance))
	{
		return;
	}

	const AActor* OwnerActor = StateInstance->GetOwner();
	const FGameplayTag SlotTag = StateInstance->GetStateDef().StateSlotType;
	UE_LOG(LogTcsState, Verbose, TEXT("[%s] FinalizeRemoval: State=%s Id=%d Reason=%s Owner=%s Slot=%s Stage=%s"),
		*FString(__FUNCTION__),
		*StateInstance->GetStateDefId().ToString(),
		StateInstance->GetInstanceId(),
		*RemovalReason.ToString(),
		OwnerActor ? *OwnerActor->GetName() : TEXT("None"),
		*SlotTag.ToString(),
		*StaticEnum<ETcsStateStage>()->GetNameStringByValue(static_cast<int64>(StateInstance->GetCurrentStage())));

	const ETcsStateStage PreviousStage = StateInstance->GetCurrentStage();

	// Stop StateTree logic if running.
	if (StateInstance->IsStateTreeRunning())
	{
		StateInstance->StopStateTree();
	}

	// Mark expired.
	StateInstance->SetCurrentStage(ETcsStateStage::SS_Expired);

	UTcsStateComponent* StateComponent = StateInstance->GetOwnerStateComponent();
	if (IsValid(StateComponent))
	{
		// Remove from containers.
		StateComponent->StateTreeTickScheduler.Remove(StateInstance);
		StateComponent->DurationTracker.Remove(StateInstance);
		StateComponent->StateInstanceIndex.RemoveInstance(StateInstance);

		// Broadcast stage changed and removal.
		StateComponent->NotifyStateStageChanged(StateInstance, PreviousStage, ETcsStateStage::SS_Expired);
		StateComponent->NotifyStateRemoved(StateInstance, RemovalReason);

		if (SlotTag.IsValid())
		{
			if (FTcsStateSlot* Slot = StateComponent->StateSlotsX.Find(SlotTag))
			{
				RemoveStateFromSlot(Slot, StateInstance, /*bDeactivateIfNeeded*/ false);
				RequestUpdateStateSlotActivation(StateComponent, SlotTag);
			}
		}
	}

	StateInstance->MarkPendingGC();

	// TODO(TCS): Return StateInstance to TireflyObjectPool when pool API is finalized (planned refactor).
}
