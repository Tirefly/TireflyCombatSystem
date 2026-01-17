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

public:
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
	 * @param InLevel 状态等级（默认1）
	 * @return 如果创建状态实例成功，则返回状态实例指针，否则返回nullptr
	 */
	UTcsStateInstance* CreateStateInstance(FName StateDefId, AActor* Owner, AActor* Instigator, int32 InLevel = 1);

protected:
	// 全局状态实例ID管理器
	UPROPERTY()
	int32 GlobalStateInstanceIdMgr = 0;

#pragma endregion


#pragma region StateApplying

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

public:
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryGetStateSlotDefinition(FGameplayTag StateSlotTag, FTcsStateSlotDefinition& OutStateSlotDef) const;

	// 初始化状态槽定义
	void InitStateSlotDefs();

protected:
	// 状态槽定义缓存
	TMap<FGameplayTag, FTcsStateSlotDefinition> StateSlotDefs;

#pragma endregion


#pragma region StateSlot

public:
	// 初始化战斗实体的状态树状态与状态槽的映射关系
	// 注意：StateSlot 在初始化后就固定，不允许动态新增
	void InitStateSlotMappings(AActor* CombatEntity);

	// 尝试分配状态到状态槽位
	bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance);

	// 更新状态槽激活状态（主函数）
	void UpdateStateSlotActivation(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag);

	// Gate一致性：当槽位Gate关闭时，强制确保槽内无Active状态，并按策略收敛阶段
	void EnforceSlotGateConsistency(UTcsStateComponent* StateComponent, FGameplayTag StateSlotTag);

	/**
	 * 响应 StateTree 状态变化，刷新相关槽位的 Gate 状态
	 *
	 * @param StateComponent 状态组件
	 * @param NewStates 当前激活的 StateTree 状态名列表
	 * @param OldStates 上一帧激活的 StateTree 状态名列表
	 */
	void RefreshSlotsForStateChange(
		UTcsStateComponent* StateComponent,
		const TArray<FName>& NewStates,
		const TArray<FName>& OldStates);

protected:
	// 清除状态槽位中过期的状态
	static void ClearStateSlotExpiredStates(UTcsStateComponent* StateComponent, FTcsStateSlot* StateSlot);

	// 优先级排序
	static void SortStatesByPriority(TArray<UTcsStateInstance*>& States);

	// 处理状态槽内状态的合并
	void ProcessStateSlotMerging(FTcsStateSlot* StateSlot);
	// 对特定组内的状态进行合并
	void MergeStateGroup(TArray<UTcsStateInstance*>& StatesToMerge, TArray<UTcsStateInstance*>& OutMergedStates);
	// 将状态槽中未被合并的状态实例移除
	void RemoveUnmergedStates(
		UTcsStateComponent* StateComponent,
		FTcsStateSlot* StateSlot,
		const TArray<UTcsStateInstance*>& MergedStates,
		const TMap<FName, UTcsStateInstance*>& MergePrimaryByDefId);

	// 状态槽位阀门关闭的处理
	void ProcessStateSlotOnGateClosed(const UTcsStateComponent* StateComponent, FTcsStateSlot* StateSlot, FGameplayTag SlotTag);

	// 按激活模式处理状态槽内的状态
	void ProcessStateSlotByActivationMode(const UTcsStateComponent* StateComponent, FTcsStateSlot* StateSlot, FGameplayTag SlotTag);
	// 按激活模式处理状态槽内的状态：优先级模式
	void ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, ETcsStatePreemptionPolicy PreemptionPolicy);
	// 按激活模式处理状态槽内的状态：全激活模式
	void ProcessAllActiveMode(FTcsStateSlot* StateSlot);
	// 按照低优先级抢占策略，处理状态实例
	void ApplyPreemptionPolicyToState(UTcsStateInstance* State, ETcsStatePreemptionPolicy Policy);

	// 辅助函数：清理状态槽位中无效的状态
	static void CleanupInvalidStates(FTcsStateSlot* StateSlot);
	// 辅助函数：将状态从状态槽中移除
	void RemoveStateFromSlot(FTcsStateSlot* StateSlot, UTcsStateInstance* State, bool bDeactivateIfNeeded = true);

#pragma endregion


#pragma region StateQuery

public:
	/**
	 * 获取指定组件中指定槽位的所有状态实例
	 * @param StateComponent 状态组件
	 * @param SlotTag 状态槽标签
	 * @param OutStates 输出状态列表
	 * @return 是否成功获取
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Query")
	bool GetStatesInSlot(
		UTcsStateComponent* StateComponent,
		FGameplayTag SlotTag,
		TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取指定组件中指定定义ID的所有状态实例
	 * @param StateComponent 状态组件
	 * @param StateDefId 状态定义ID
	 * @param OutStates 输出状态列表
	 * @return 是否成功获取
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Query")
	bool GetStatesByDefId(
		UTcsStateComponent* StateComponent,
		FName StateDefId,
		TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取指定组件的所有活跃状态实例
	 * @param StateComponent 状态组件
	 * @param OutStates 输出状态列表
	 * @return 是否成功获取
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Query")
	bool GetAllActiveStates(
		UTcsStateComponent* StateComponent,
		TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 检查指定组件是否存在指定定义ID的状态
	 * @param StateComponent 状态组件
	 * @param StateDefId 状态定义ID
	 * @return 是否存在
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Query")
	bool HasStateWithDefId(
		UTcsStateComponent* StateComponent,
		FName StateDefId) const;

	/**
	 * 检查指定组件是否存在指定槽位中的活跃状态
	 * @param StateComponent 状态组件
	 * @param SlotTag 状态槽标签
	 * @return 是否存在活跃状态
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Query")
	bool HasActiveStateInSlot(
		UTcsStateComponent* StateComponent,
		FGameplayTag SlotTag) const;

#pragma endregion


#pragma region StateRemoval

public:
	/**
	 * 移除指定状态实例
	 * @param StateInstance 要移除的状态实例
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
	bool RemoveState(UTcsStateInstance* StateInstance);

	/**
	 * 按状态定义ID移除状态
	 * @param StateComponent 状态组件
	 * @param StateDefId 状态定义ID
	 * @param bRemoveAll 是否移除所有匹配的状态，false则只移除第一个
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
	int32 RemoveStatesByDefId(
		UTcsStateComponent* StateComponent,
		FName StateDefId,
		bool bRemoveAll = true);

	/**
	 * 清空指定槽位的所有状态
	 * @param StateComponent 状态组件
	 * @param SlotTag 状态槽标签
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
	int32 RemoveAllStatesInSlot(
		UTcsStateComponent* StateComponent,
		FGameplayTag SlotTag);

	/**
	 * 清空指定组件的所有状态
	 * @param StateComponent 状态组件
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
	int32 RemoveAllStates(UTcsStateComponent* StateComponent);

#pragma endregion


#pragma region StateInstance

public:
	// 激活状态实例
	void ActivateState(UTcsStateInstance* StateInstance);

	// 停用状态实例
	void DeactivateState(UTcsStateInstance* StateInstance);

	// 挂起状态实例
	void HangUpState(UTcsStateInstance* StateInstance);

	// 恢复状态实例
	void ResumeState(UTcsStateInstance* StateInstance);

	// 暂停状态实例
	void PauseState(UTcsStateInstance* StateInstance);

	// 取消状态实例
	void CancelState(UTcsStateInstance* StateInstance);

	// 标记状态为过期
	void ExpireState(UTcsStateInstance* StateInstance);

	UFUNCTION(BlueprintCallable, Category = "State Manager|Removal")
	bool RequestStateRemoval(UTcsStateInstance* StateInstance, FTcsStateRemovalRequest Request);

	void FinalizePendingRemovalRequest(UTcsStateInstance* StateInstance);

#pragma endregion


#pragma region StateRemoval_Internal

protected:
	// 最终化移除流程：停止逻辑、标记过期、清理容器并广播事件
	void FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);

#pragma endregion
};
