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


#pragma region Meta

public:
	// 获取状态定义
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool GetStateDefinition(FName StateDefId, FTcsStateDefinition& OutStateDef);

protected:
	/**
	 * 创建状态实例
	 * 
	 * @param StateDefId 状态定义名，通过TcsGenericLibrary.GetStateDefIds获取
	 * @param Owner 状态的拥有者，也是状态的应用目标
	 * @param Instigator 状态的发起者
	 * @return 如果创建状态实例成功，则返回状态实例指针，否则返回nullptr
	 */
	UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Owner, AActor* Instigator);

#pragma endregion


#pragma region Applying

public:
	/**
	 * 尝试向目标应用状态
	 * 
	 * @param Target 目标，状态将应用到此目标
	 * @param StateDefId 状态定义名，可通过TcsGenericLibrary.GetStateDefIds获取
	 * @param Instigator 状态的发起者
	 * @param OutResult 应用结果
	 * @return 如果应用状态成功，则返回true，否则返回false
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateToTarget(
		AActor* Target,
		FName StateDefId,
		AActor* Instigator,
		FTcsStateApplyResult& OutResult);

	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateToTargetWithParams(
		AActor* Target,
		FName StateDefId,
		AActor* Instigator,
		const FInstancedStruct& Params,
		FTcsStateApplyResult& OutResult);

protected:
	bool TryApplyStateInstance(
		UTcsStateInstance* StateInstance,
		FTcsStateApplyResult& OutResult);

#pragma endregion
};
