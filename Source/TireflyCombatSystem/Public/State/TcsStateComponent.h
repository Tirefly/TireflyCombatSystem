// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Containers/ArrayView.h"
#include "TcsState.h"
#include "TcsStateContainer.h"
#include "TcsStateSlot.h"
#include "TcsStateComponent.generated.h"



class UTcsStateInstance;
class UTcsStateManagerSubsystem;
class UTcsAttributeManagerSubsystem;
class UTcsStateDefinitionAsset;
class UTcsStateSlotDefinitionAsset;
struct FStateTreeStateHandle;



// 状态阶段变更事件签名
// 状态阶段: SS_Inactive(未激活), SS_Active(已激活), SS_HangUp(挂起), SS_Pause(暂停), SS_Expired(已过期)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateStageChangedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	ETcsStateStage, PreviousStage,
	ETcsStateStage, NewStage);

// 状态停用事件签名（状态停止执行逻辑，但未必被移除）
// (状态组件, 状态实例, 新阶段, 停用原因)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateDeactivatedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	ETcsStateStage, NewStage,
	FName, DeactivateReason);

// 状态应用成功事件签名
// (应用到的Actor, 状态定义ID, 创建的状态实例, 目标槽位, 应用后的状态阶段)
// 应用后的状态阶段通常是 SS_Active, 也可能是 SS_HangUp 或 SS_Pause (根据槽位Gate状态)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FTcsOnStateApplySuccessSignature,
	AActor*, TargetActor,
	FName, StateDefId,
	UTcsStateInstance*, CreatedStateInstance,
	FGameplayTag, TargetSlot,
	ETcsStateStage, AppliedStage);

// 状态应用失败事件签名
// (应用到的Actor, 状态定义ID, 失败原因枚举, 失败详情消息)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateApplyFailedSignature,
	AActor*, TargetActor,
	FName, StateDefId,
	ETcsStateApplyFailReason, FailureReason,
	FString, FailureMessage
);

// 状态移除事件签名
// (状态组件, 状态实例, 移除原因: Expired=自然过期, Removed=主动移除, Cancelled=被取消, MergedOut=合并淘汰, StackDepleted=叠层耗尽)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnStateRemovedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	FName, RemovalReason);

// 状态叠层变化事件签名
// (状态组件, 状态实例, 旧叠层数, 新叠层数)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateStackChangedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	int32, OldStackCount,
	int32, NewStackCount);

// 状态等级变化事件签名
// (状态组件, 状态实例, 旧等级, 新等级)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateLevelChangedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	int32, OldLevel,
	int32, NewLevel);

// 状态持续时间刷新事件签名
// (状态组件, 状态实例, 新的持续时间)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnStateDurationRefreshedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	float, NewDuration);

// 槽位Gate状态变化事件签名
// (状态组件, 槽位标签, 是否开启)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnSlotGateStateChangedSignature,
	UTcsStateComponent*, StateComponent,
	FGameplayTag, SlotTag,
	bool, bIsOpen);

// 状态参数变化事件签名
// (状态实例, 参数键类型, 参数名称, 参数Tag, 参数类型)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FTcsOnStateParameterChangedSignature,
	UTcsStateInstance*, StateInstance,
	ETcsStateParameterKeyType, KeyType,
	FName, ParameterName,
	FGameplayTag, ParameterTag,
	ETcsStateParameterType, ParameterType);

// 状态合并事件签名
// (状态组件, 目标状态实例, 源状态实例, 合并后的叠层数)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateMergedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, TargetStateInstance,
	UTcsStateInstance*, SourceStateInstance,
	int32, ResultStackCount);



UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly State Cmp"))
class TIREFLYCOMBATSYSTEM_API UTcsStateComponent : public UStateTreeComponent
{
    GENERATED_BODY()

#pragma region ActorComponent

public:
	// Sets default values for this component's properties
	UTcsStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

public:
	friend class UTcsStateInstance;

public:
	void AddToStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Add(StateInstance); }
	void RemoveFromStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Remove(StateInstance); }

#pragma endregion


#pragma region StateInstance

public:
	// 通知阶段发生变化（内部与外部均可触发）
	// 状态阶段: SS_Inactive, SS_Active, SS_HangUp, SS_Pause, SS_Expired
	void NotifyStateStageChanged(
		UTcsStateInstance* StateInstance,
		ETcsStateStage PreviousStage,
		ETcsStateStage NewStage);

	// 通知状态停用（停止执行逻辑）
	void NotifyStateDeactivated(
		UTcsStateInstance* StateInstance,
		ETcsStateStage NewStage,
		FName DeactivateReason);

	// 通知状态被移除
	// RemovalReason: "Expired"=自然过期, "Removed"=主动移除, "Cancelled"=被取消, "MergedOut"=合并淘汰, "StackDepleted"=叠层耗尽
	void NotifyStateRemoved(UTcsStateInstance* StateInstance, FName RemovalReason);

	// 通知状态叠层变化
	void NotifyStateStackChanged(UTcsStateInstance* StateInstance, int32 OldStackCount, int32 NewStackCount);

	// 通知状态等级变化
	void NotifyStateLevelChanged(UTcsStateInstance* StateInstance, int32 OldLevel, int32 NewLevel);

	// 通知状态持续时间刷新
	void NotifyStateDurationRefreshed(UTcsStateInstance* StateInstance, float NewDuration);

	// 通知槽位Gate状态变化
	void NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen);

	// 通知状态参数变化
	void NotifyStateParameterChanged(
		UTcsStateInstance* StateInstance,
		ETcsStateParameterKeyType KeyType,
		FName ParameterName,
		FGameplayTag ParameterTag,
		ETcsStateParameterType ParameterType);

	// 通知状态合并
	void NotifyStateMerged(UTcsStateInstance* TargetStateInstance, UTcsStateInstance* SourceStateInstance, int32 ResultStackCount);

	// 通知状态应用成功
	void NotifyStateApplySuccess(
		AActor* TargetActor,
		FName StateDefId,
		UTcsStateInstance* CreatedStateInstance,
		FGameplayTag TargetSlot,
		ETcsStateStage AppliedStage);

	// 通知状态应用失败
	void NotifyStateApplyFailed(
		AActor* TargetActor,
		FName StateDefId,
		ETcsStateApplyFailReason FailureReason,
		const FString& FailureMessage);

public:
	// 状态阶段变更事件（槽位联动）
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateStageChangedSignature OnStateStageChanged;

	// 状态停用事件（停止执行逻辑，但未必被移除）
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateDeactivatedSignature OnStateDeactivated;

	/**
	 * 状态应用成功事件
	 * 当状态成功应用到槽位时广播
	 * AppliedStage 表示状态应用后的实际阶段（可能是Active或HangUp等）
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateApplySuccessSignature OnStateApplySuccess;

	/**
	 * 状态应用失败事件
	 * 当状态应用失败时广播，包含失败原因枚举
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateApplyFailedSignature OnStateApplyFailed;

	/**
	 * 状态移除事件
	 * 当状态被移除时广播（包括自然过期、主动移除、被取消等情况）
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateRemovedSignature OnStateRemoved;

	/**
	 * 状态叠层变化事件
	 * 当状态的叠层数发生变化时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateStackChangedSignature OnStateStackChanged;

	/**
	 * 状态等级变化事件
	 * 当状态的等级发生变化时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateLevelChangedSignature OnStateLevelChanged;

	/**
	 * 状态持续时间刷新事件
	 * 当状态的持续时间被刷新时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateDurationRefreshedSignature OnStateDurationRefreshed;

	/**
	 * 槽位Gate状态变化事件
	 * 当槽位的Gate开关状态变化时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnSlotGateStateChangedSignature OnSlotGateStateChanged;

	/**
	 * 状态参数变化事件
	 * 当状态的参数被修改时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateParameterChangedSignature OnStateParameterChanged;

	/**
	 * 状态合并事件
	 * 当两个同类型状态合并时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateMergedSignature OnStateMerged;

protected:
	// 状态管理器子系统
	UPROPERTY()
	TObjectPtr<UTcsStateManagerSubsystem> StateMgr;

	// 属性管理器子系统（迁移期缓存，供 Phase D/E 下沉的生命周期/清理逻辑直接访问）
	UPROPERTY()
	TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;

	/**
	 * 懒加载获取 StateManager
	 * BeginPlay 已预热；业务方法中若首访为空，会在此补拉取并 ensureMsgf 诊断
	 *
	 * @return StateManager 指针；失败时返回 nullptr 并触发 ensureMsgf
	 */
	UTcsStateManagerSubsystem* ResolveStateManager();

	/**
	 * 懒加载获取 AttributeManager
	 *
	 * @return AttributeManager 指针；失败时返回 nullptr 并触发 ensureMsgf
	 */
	UTcsAttributeManagerSubsystem* ResolveAttributeManager();

	// 状态实例索引：按Id/DefId/Slot查询
	UPROPERTY()
	FTcsStateInstanceIndex StateInstanceIndex;

	// StateTree Tick调度器：只保存正在Running的实例
	UPROPERTY()
	FTcsStateTreeTickScheduler StateTreeTickScheduler;

public:
	/**
	 * 尝试在当前组件拥有者上应用指定状态定义。
	 *
	 * @param StateDefId 要应用的状态定义 ID
	 * @param Instigator 状态发起者
	 * @param StateLevel 状态等级
	 * @param ParentSourceHandle 父级来源句柄
	 * @return 是否应用成功
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	virtual bool TryApplyState(
		FName StateDefId,
		AActor* Instigator,
		int32 StateLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

	/**
	 * 尝试将已初始化的状态实例应用到当前组件。
	 *
	 * @param StateInstance 要应用的状态实例
	 * @return 如果应用成功则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
	virtual bool TryApplyStateInstance(UTcsStateInstance* StateInstance);

protected:
	/**
	 * 在当前组件拥有者上创建状态实例。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param Instigator 状态发起者
	 * @param InLevel 状态等级
	 * @param ParentSourceHandle 父级来源句柄
	 * @return 如果创建成功则返回状态实例，否则返回 nullptr
	 */
	virtual UTcsStateInstance* CreateStateInstance(
		FName StateDefId,
		AActor* Instigator,
		int32 InLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

	/**
	 * 评估并写入状态参数。
	 *
	 * @param StateDefAsset 状态定义资产
	 * @param Instigator 状态发起者
	 * @param StateInstance 状态实例
	 * @param OutFailedParams 输出失败的参数名列表
	 * @return 如果所有参数评估成功则返回 true，否则返回 false
	 */
	virtual bool EvaluateAndApplyStateParameters(
		const UTcsStateDefinitionAsset* StateDefAsset,
		AActor* Instigator,
		UTcsStateInstance* StateInstance,
		TArray<FName>& OutFailedParams);

	/**
	 * 检查状态实例是否满足应用条件。
	 *
	 * @param StateInstance 要检查的状态实例
	 * @return 如果满足应用条件则返回 true，否则返回 false
	 */
	virtual bool CheckStateApplyConditions(UTcsStateInstance* StateInstance);

	/**
	 * 请求移除指定状态实例。
	 *
	 * @param StateInstance 要移除的状态实例
	 * @param RemovalReason 移除原因
	 * @return 是否成功收敛到移除流程
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Removal")
	virtual bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);

	/**
	 * 移除指定状态实例。
	 *
	 * @param StateInstance 要移除的状态实例
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Removal")
	virtual bool RemoveState(UTcsStateInstance* StateInstance);

	/**
	 * 按状态定义 ID 移除状态。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param bRemoveAll 是否移除全部匹配实例
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Removal")
	virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true);

	/**
	 * 清空指定槽位的所有状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Removal")
	virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag);

	/**
	 * 清空当前组件中的全部状态。
	 *
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Removal")
	virtual int32 RemoveAllStates();

	// 取消状态实例（非 virtual 包装器）
	void CancelState(UTcsStateInstance* StateInstance);

	// 标记状态实例为自然过期（非 virtual 包装器）
	void ExpireState(UTcsStateInstance* StateInstance);

protected:
	// 激活状态实例
	virtual void ActivateState(UTcsStateInstance* StateInstance);

	// 停用状态实例
	virtual void DeactivateState(UTcsStateInstance* StateInstance);

	// 挂起状态实例
	virtual void HangUpState(UTcsStateInstance* StateInstance);

	// 恢复状态实例
	virtual void ResumeState(UTcsStateInstance* StateInstance);

	// 暂停状态实例
	virtual void PauseState(UTcsStateInstance* StateInstance);

	// 检查状态是否仍然属于当前组件且未过期
	virtual bool IsStateStillValid(UTcsStateInstance* StateInstance) const;

	// 最终化移除流程：停止逻辑、清理容器、广播事件并标记 GC
	virtual void FinalizeStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);

#pragma endregion


#pragma region StateSlot_References

protected:
	// 映射集合：StateSlot 到 StateTreeState
	UPROPERTY()
	TMap<FGameplayTag, FStateTreeStateHandle> Mapping_StateSlotToStateHandle;
	
	// 映射集合：StateTreeState 到 StateSlot
	UPROPERTY()
	TMap<FStateTreeStateHandle, FGameplayTag> Mapping_StateHandleToStateSlot;

	// StateTree状态槽映射 (运行时状态数据)
	UPROPERTY()
	TMap<FGameplayTag, FTcsStateSlot> StateSlotsX;

#pragma endregion


#pragma region StateSlot_Gate

public:
	/**
	 * 获取指定槽位中的全部状态实例。
	 *
	 * @param SlotTag 状态槽标签
	 * @param OutStates 输出状态实例列表
	 * @return 如果找到状态则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Query")
	bool GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取指定定义 ID 的全部状态实例。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param OutStates 输出状态实例列表
	 * @return 如果找到状态则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Query")
	bool GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取当前组件中的全部激活状态。
	 *
	 * @param OutStates 输出激活状态列表
	 * @return 如果找到激活状态则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Query")
	bool GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 检查是否存在指定定义 ID 的状态。
	 *
	 * @param StateDefId 状态定义 ID
	 * @return 如果存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Query")
	bool HasStateWithDefId(FName StateDefId) const;

	/**
	 * 检查指定槽位中是否存在激活状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 如果存在激活状态则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Query")
	bool HasActiveStateInSlot(FGameplayTag SlotTag) const;

public:
	// 调试输出
	UFUNCTION(BlueprintPure, Category = "State Slot|Debug", meta = (AutoCreateRefTerm = "SlotFilter"))
	FString GetSlotDebugSnapshot(FGameplayTag SlotFilter = FGameplayTag()) const;

	// 状态实例调试输出（按实例枚举，便于定位 Duration/Tick 等字段）
	UFUNCTION(BlueprintPure, Category = "State|Debug")
	FString GetStateDebugSnapshot(FName StateDefIdFilter = NAME_None) const;

	// 槽位Gate开关
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

	// 槽位Gate开关状态
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsSlotGateOpen(FGameplayTag SlotTag) const;

protected:
	// 初始化当前组件的 StateSlot 与 StateTreeState 映射。
	virtual void InitStateSlotMappings();

	// 尝试把状态实例放入目标槽位并驱动后续激活流程。
	virtual bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance);

	// 响应 StateTree 激活状态变更并刷新相关槽位。
	virtual void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates);

	// 请求刷新指定槽位的激活结果。
	void RequestUpdateStateSlotActivation(FGameplayTag SlotTag);

	// 排空同帧累积的槽位激活请求。
	void DrainPendingSlotActivationUpdates();

	// 更新指定槽位的激活结果。
	virtual void UpdateStateSlotActivation(FGameplayTag SlotTag);

	// Gate 一致性：当槽位关闭时，强制收敛阶段。
	virtual void EnforceSlotGateConsistency(FGameplayTag SlotTag);

	// 清理槽位中已经过期的状态实例。
	void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot);

	// 按优先级排序槽位中的状态。
	virtual void SortStatesByPriority(TArray<UTcsStateInstance*>& States);

	// 处理槽位中的状态合并。
	virtual void ProcessStateSlotMerging(FTcsStateSlot* StateSlot);

	// 对同一组状态执行合并策略。
	void MergeStateGroup(TArray<UTcsStateInstance*>& StatesToMerge, TArray<UTcsStateInstance*>& OutMergedStates);

	// 移除在合并后未被保留的状态实例。
	void RemoveUnmergedStates(
		FTcsStateSlot* StateSlot,
		const TArray<UTcsStateInstance*>& MergedStates,
		const TMap<FName, UTcsStateInstance*>& MergePrimaryByDefId);

	// 按槽位激活模式处理状态。
	virtual void ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag);

	// 优先级模式：只保留最高优先级状态激活。
	void ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinitionAsset* SlotDef);

	// 全激活模式：槽位中所有状态都保持激活。
	void ProcessAllActiveMode(FTcsStateSlot* StateSlot);

	// 按抢占策略处理低优先级状态。
	virtual void ApplyPreemptionPolicyToState(UTcsStateInstance* State, ETcsStatePreemptionPolicy Policy);

	// 清理槽位中的无效实例。
	void CleanupInvalidStates(FTcsStateSlot* StateSlot);

	// 从槽位中移除指定状态实例。
	void RemoveStateFromSlot(FTcsStateSlot* StateSlot, UTcsStateInstance* State, bool bDeactivateIfNeeded = true);

    // 获取当前激活的StateTree状态名列表
    TArray<FName> GetCurrentActiveStateTreeStates() const;

    // 缓存上一帧的StateTree激活状态名,用于检测变化
    TArray<FName> CachedActiveStateNames;

	// 当前组件是否正在执行槽位激活刷新；用于同帧防重入。
	bool bIsUpdatingSlotActivation = false;

	// 当前组件待排空的槽位激活请求集合。
	TSet<FGameplayTag> PendingSlotActivationUpdates;

	// 当前是否处于 StateTree Tick/回调上下文；仅用于移除链路的 ensure 诊断。
	bool bIsInStateTreeCallback = false;

	// 判断当前是否处于 StateTree 更新上下文。
	bool IsInStateTreeUpdateContext() const { return bIsInStateTreeCallback; }

#pragma endregion


#pragma region StateTree_Reference

public:
	UFUNCTION(BlueprintCallable, Category = "StateTree")
	FStateTreeReference GetStateTreeReference() const;

	UFUNCTION(BlueprintCallable, Category = "StateTree")
	const UStateTree* GetStateTree() const;

#pragma endregion


#pragma region StateTree_State

public:
	/**
	 * 由TcsStateChangeNotifyTask调用，通知StateTree状态变更
	 * @param Context 执行上下文，包含当前激活状态信息
	 */
	void OnStateTreeStateChanged(const FStateTreeExecutionContext& Context);

protected:
	// 比较两个状态列表是否相等
	bool AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const;

	// 状态槽变化事件处理
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

#pragma endregion

	
#pragma region StateDuration

public:
	// 获取状态实例的剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	float GetStateRemainingDuration(const UTcsStateInstance* StateInstance) const;

	// 刷新状态实例的剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	void RefreshStateRemainingDuration(UTcsStateInstance* StateInstance);

	// 设置状态实例的剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	void SetStateRemainingDuration(UTcsStateInstance* StateInstance, float InDurationRemaining);

protected:
	// 更新所有持续存在的状态实例的持续时间
	void UpdateActiveStateDurations(float DeltaTime);

	// Tick 所有运行中的 StateTree（调度、执行、清理停止的实例）
	void TickStateTrees(float DeltaTime);

protected:
	// 状态实例持续时间追踪器（仅SDT_Duration）
	UPROPERTY()
	FTcsStateDurationTracker DurationTracker;

#pragma endregion
};
