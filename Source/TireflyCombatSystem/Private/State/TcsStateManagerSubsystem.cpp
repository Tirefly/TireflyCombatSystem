// Copyright Tirefly. All Rights Reserved.


#include "State/TcsStateManagerSubsystem.h"

#include "State/TcsState.h"
#include "TcsDeveloperSettings.h"
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
        StateInstance->Initialize(StateDef, Owner, Instigator);
        StateInstance->SetStateDefId(StateDefRowId);
        // 标记应用时间戳（用于合并器等逻辑）
        StateInstance->SetApplyTimestamp(FDateTime::UtcNow().GetTicks());
    }
    
    return StateInstance;
}