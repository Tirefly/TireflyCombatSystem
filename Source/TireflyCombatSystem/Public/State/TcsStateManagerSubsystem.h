// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "State/TcsState.h"
#include "TcsStateManagerSubsystem.generated.h"



class UTcsStateInstance;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()


#pragma region StateInstance

public:
	// 获取状态定义
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool GetStateDefinition(FName StateDefId, FTcsStateDefinition& OutStateDef);

protected:
	/**
	 * 创建状态实例
	 * 
	 * @param StateDefId 状态定义名，通过TcsGenericLibrary.GetStateDefIds获取
	 * @param Owner 状态的目标拥有者
	 * @param Instigator 状态的发起者
	 * @return 如果创建状态实例成功，则返回状态实例指针，否则返回nullptr
	 */
	UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Owner, AActor* Instigator);

#pragma endregion
};
