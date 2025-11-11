// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"

#include "Engine/DataTable.h"
#include "StateTree.h"
#include "TcsEntityInterface.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "State/TcsState.h"
#include "State/TcsStateComponent.h"
#include "State/StateMerger/TcsStateMerger.h"


void UTcsStateManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 加载状态定义表
    StateDefTable = UTcsGenericLibrary::GetStateDefTable();
    if (!IsValid(StateDefTable))
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[%s] StateDefTable in TcsDevSettings is not valid"),
            *FString(__FUNCTION__));
        return;
    }

    // 加载状态槽定义表
    StateSlotDefTable = UTcsGenericLibrary::GetStateSlotDefTable();
    if (!IsValid(StateSlotDefTable))
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[%s] StateSlotDefTable in TcsDevSettings is not valid"),
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
    return true;
}

UTcsStateInstance* UTcsStateManagerSubsystem::CreateStateInstance(
    FName StateDefRowId,
    AActor* Owner,
    AActor* Instigator)
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
        StateInstance->Initialize(StateDef, Owner, Instigator);
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
        return false;
    }

    return TryApplyStateInstance(CreateStateInstance(StateDefId, Target, Instigator));
}

bool UTcsStateManagerSubsystem::TryApplyStateInstance(UTcsStateInstance* StateInstance)
{
    if (!IsValid(StateInstance))
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateInstance is invalid."),
            *FString(__FUNCTION__));;
        return false;
    }
    
    // TODO: 未来实现CheckImmunity

    // 检查应用条件
    if (!CheckStateApplyConditions(StateInstance))
    {
        return false;
    }

    // 尝试附加到状态槽中
    if (!TryAssignStateToStateSlot(StateInstance))
    {
        return false;
    }

    return true;
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
                StateComponent->StateSlotsX.Add(StateSlotTag);
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
        return false;
    }

    const FTcsStateSlotDefinition* StateSlotDef = StateSlotDefs.Find(StateDef.StateSlotType);
    if (!StateSlotDef)
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] StateSlotDef %s not found."),
            *FString(__FUNCTION__),
            *StateDef.StateSlotType.ToString());
        return false;
    }

    // 清除槽位中过期的状态
    ClearStateSlotExpiredStates(OwnerStateCmp, StateSlot);

    // 定义一个局部函数，用于将状态实例加入槽位并等待后续激活判定
    auto AssignToSlotAndWaitForApplication = [&]()
    {
        // 加入到槽位中，等待后续激活判定
        StateSlot->States.Push(StateInstance);
        // 如果是持续性状态，加入持久化列表
        if (StateInstance->GetStateDef().DurationType != ETcsStateDurationType::SDT_None)
        {
            OwnerStateCmp->PersistentStateInstances.AddInstance(StateInstance);
            // 如果是有持续时间的状态，加入持续时间映射
            if (StateInstance->GetStateDef().DurationType == ETcsStateDurationType::SDT_Duration)
            {
                auto DurationData = FTcsStateDurationData(StateInstance, StateInstance->GetTotalDuration());
                OwnerStateCmp->StateDurationMap.Add(StateInstance, DurationData);
            }
        }
    };

    // 如果状态槽当前为开启状态
    if (StateSlot->bIsGateOpen)
    {
        // 根据状态槽定义的激活模式处理
        switch (StateSlotDef->ActivationMode)
        {
        case ETcsStateSlotActivationMode::SSAM_PriorityOnly:
            {
                // 获取状态槽当前优先级最高的状态实例，默认为数组栈顶元素
                if (const UTcsStateInstance* HighestPriorityState = StateSlot->States.Top())
                {
                    // 如果新状态优先级更高，或者状态槽的抢占策略不是取消低优先级，啧加入槽位并等待激活判定
                    if (StateInstance->GetStateDef().Priority > HighestPriorityState->GetStateDef().Priority
                        || StateSlotDef->PreemptionPolicy != ETcsStatePreemptionPolicy::SPP_CancelLowerPriority)
                    {
                        AssignToSlotAndWaitForApplication();
                    }
                    else
                    {
                        // 否则，取消新状态的应用
                        UE_LOG(LogTcsState, Log, TEXT("[%s] State %s application canceled due to lower priority than existing state %s in slot %s."),
                            *FString(__FUNCTION__),
                            *StateInstance->GetStateDefId().ToString(),
                            *HighestPriorityState->GetStateDefId().ToString(),
                            *StateDef.StateSlotType.ToString());
                        return false;
                    }
                }

                // 如果槽位里当前没有状态实例，直接加入
                AssignToSlotAndWaitForApplication();
                break;
            }
        case ETcsStateSlotActivationMode::SSAM_AllActive:
            {
                // 如果状态槽的激活策略是全部激活，直接加入槽位
                AssignToSlotAndWaitForApplication();
                break;
            }
        }
    }
    // 否则，状态槽为关闭状态
    else
    {
        // 如果状态槽的阀门关闭策略是取消，直接取消状态应用
        if (StateSlotDef->GateCloseBehavior == ETcsStateSlotGateClosePolicy::SSGCP_Cancel)
        {
            UE_LOG(LogTcsState, Log, TEXT("[%s] State %s application canceled because slot %s gate is closed."),
                *FString(__FUNCTION__),
                *StateInstance->GetStateDefId().ToString(),
                *StateDef.StateSlotType.ToString());
            return false;
        }

        // 否则，加入槽位并等待激活判定
        AssignToSlotAndWaitForApplication();
    }

    // 更新槽位中所有状态的激活流程
    UpdateStateSlotActivation(OwnerStateCmp, StateDef.StateSlotType);
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
            StateComponent->PersistentStateInstances.RemoveInstance(State);
            return true;
        }
        return false;
    });
    StateComponent->PersistentStateInstances.RefreshInstances();
}

void UTcsStateManagerSubsystem::UpdateStateSlotActivation(
    UTcsStateComponent* StateComponent,
    FGameplayTag StateSlotTag)
{
}
