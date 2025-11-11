// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TcsStateSlot.h"
#include "State/TcsState.h"
#include "TcsStateManagerSubsystem.generated.h"



class UTcsStateInstance;
class UTcsStateComponent;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#pragma endregion


#pragma region StateTables

protected:
	UPROPERTY()
	UDataTable* StateDefTable;

	UPROPERTY()
	UDataTable* StateSlotDefTable;

#pragma endregion
	

#pragma region MetaData

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
	 * @return 如果应用状态成功，则返回true，否则返回false
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateToTarget(
		AActor* Target,
		FName StateDefId,
		AActor* Instigator);

	/**
	 * 
	 * @param StateInstance 要应用的状态实例
	 * @return 如果应用状态实例成功，则返回true，否则返回false
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateInstance(UTcsStateInstance* StateInstance);

protected:
	// 基于状态的定义配置内容，检查状态应用条件
	static bool CheckStateApplyConditions(UTcsStateInstance* StateInstance);

#pragma endregion


#pragma region StateSlotDef

protected:
	// 初始化状态槽定义
	void InitStateSlotDefs();

protected:
	// 状态槽定义缓存
	TMap<FGameplayTag, FTcsStateSlotDefinition> StateSlotDefs;

#pragma endregion


#pragma region StateSlot

public:
	// 初始化战斗实体的状态树状态与状态槽的映射关系
	void InitStateSlotMappings(AActor* CombatEntity);

	// 尝试分配状态到状态槽位
	bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance);

protected:
	// 清除状态槽位中过期的状态
	void ClearStateSlotExpiredStates(UTcsStateComponent* StateComponent, FTcsStateSlot* StateSlot);

	void UpdateStateSlotActivation(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag);

#pragma endregion


#pragma region StateInstance

public:
	void ActivateState(UTcsStateInstance* StateInstance);

	void DeactivateState(UTcsStateInstance* StateInstance);

	void HangUpState(UTcsStateInstance* StateInstance);

	void ResumeState(UTcsStateInstance* StateInstance);

#pragma endregion
};
