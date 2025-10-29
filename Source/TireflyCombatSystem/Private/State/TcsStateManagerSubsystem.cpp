// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"

#include "State/TcsState.h"
#include "TcsDeveloperSettings.h"
#include "TcsGenericLibrary.h"
#include "TcsLogChannels.h"
#include "Engine/DataTable.h"



bool UTcsStateManagerSubsystem::GetStateDefinition(FName StateDefId, FTcsStateDefinition& OutStateDef)
{
    if (StateDefId.IsNone())
    {
        return false;
    }

    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (!Settings || !Settings->StateDefTable.IsValid())
    {
        return false;
    }

    if (UDataTable* Table = Settings->StateDefTable.LoadSynchronous())
    {
        if (const FTcsStateDefinition* Row = Table->FindRow<FTcsStateDefinition>(StateDefId, TEXT("TCS GetStateDefinition"), true))
        {
            OutStateDef = *Row;
        	return true;
        }
    }

	return false;
}

UTcsStateInstance* UTcsStateManagerSubsystem::CreateStateInstance(FName StateDefRowId, AActor* Owner, AActor* Instigator)
{
    if (!Owner)
    {
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
    if (StateInstance)
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
    AActor* Instigator,
    FTcsStateApplyResult& OutResult)
{
    if (!Target || !Instigator)
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
    if (!TargetStateCmp)
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] Target does not have state component: %s"),
            *FString(__FUNCTION__),
            *Target->GetName());
        return false;
    }

    return TryApplyStateInstance(CreateStateInstance(StateDefId, Target, Instigator), OutResult);
}

bool UTcsStateManagerSubsystem::TryApplyStateToTargetWithParams(
    AActor* Target,
    FName StateDefId,
    AActor* Instigator,
    const FInstancedStruct& Params,
    FTcsStateApplyResult& OutResult)
{
    if (!Target || !Instigator)
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
    if (!TargetStateCmp)
    {
        UE_LOG(LogTcsState, Error, TEXT("[%s] Target does not have state component: %s"),
            *FString(__FUNCTION__),
            *Target->GetName());
        return false;
    }

    UTcsStateInstance* StateInstance = CreateStateInstance(StateDefId, Target, Instigator);
    // TODO: 添加参数
    
    return TryApplyStateInstance(StateInstance, OutResult);
}

bool UTcsStateManagerSubsystem::TryApplyStateInstance(UTcsStateInstance* StateInstance, FTcsStateApplyResult& OutResult)
{
    // TODO: 未来实现CheckImmunity

    // TODO：创建状态实例

    // TODO: 判定StateDef中的Conditions

    // TODO：尝试附加到状态槽中

    return true;
}
