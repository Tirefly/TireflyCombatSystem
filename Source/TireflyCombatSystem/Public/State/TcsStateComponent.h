// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Containers/ArrayView.h"
#include "TcsStateInstance.h"
#include "TcsStateContainer.h"
#include "TcsStateSlot.h"
#include "TcsStateComponent.generated.h"



class UTcsStateComponent;
class UTcsStateInstance;
class UTcsStateManagerSubsystem;
class UTcsAttributeManagerSubsystem;
class UTcsBuffComponent;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;



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
// (状态组件, 状态实例, 移除原因: 共享原因通常包括 Expired/Removed/Cancelled；模块扩展可附加专属原因)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnStateRemovedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	FName, RemovalReason);

// 状态等级变化事件签名
// (状态组件, 状态实例, 旧等级, 新等级)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateLevelChangedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	int32, OldLevel,
	int32, NewLevel);
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

// 槽位激活刷新前置扩展点签名
// (状态组件, 槽位标签, 槽位运行时数据)
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnPrepareStateSlotActivationSignature,
	UTcsStateComponent*,
	FGameplayTag,
	FTcsStateSlot*);

// 状态调试叠加层扩展点签名
// (状态组件, 状态实例, 输出叠层数, 输出时长文本)
DECLARE_MULTICAST_DELEGATE_FourParams(
	FTcsOnBuildStateDebugOverlaySignature,
	UTcsStateComponent*,
	const UTcsStateInstance*,
	int32&,
	FString&);



UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tcs State Component"))
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
	friend class UTcsBuffComponent;

public:
	// 供状态实例调用的接口，移除状态实例到状态树 Tick 调度器
	void AddToStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Add(StateInstance); }
	// 供状态实例调用的接口，移除状态实例到状态树 Tick 调度器
	void RemoveFromStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Remove(StateInstance); }

	/**
	 * 获取共享 StateManager。
	 *
	 * @return 共享 StateManager 子系统；失败时返回 nullptr
	 */
	UTcsStateManagerSubsystem* GetStateManager() const { return const_cast<UTcsStateComponent*>(this)->ResolveStateManager(); }

#pragma endregion


#pragma region StateEvents

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
	// RemovalReason: 统一透传移除原因字符串；Buff-only 语义请优先通过 Buff 模块事件面理解
	void NotifyStateRemoved(UTcsStateInstance* StateInstance, FName RemovalReason);

	// 通知状态等级变化
	void NotifyStateLevelChanged(UTcsStateInstance* StateInstance, int32 OldLevel, int32 NewLevel);

	// 通知槽位Gate状态变化
	void NotifySlotGateStateChanged(FGameplayTag SlotTag, bool bIsOpen);

	// 通知状态参数变化
	void NotifyStateParameterChanged(
		UTcsStateInstance* StateInstance,
		ETcsStateParameterKeyType KeyType,
		FName ParameterName,
		FGameplayTag ParameterTag,
		ETcsStateParameterType ParameterType);

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
	 * 状态等级变化事件
	 * 当状态的等级发生变化时广播
	 */
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateLevelChangedSignature OnStateLevelChanged;

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

#pragma endregion


#pragma region StateExtensibility

public:
	/**
	 * 获取槽位激活刷新前置扩展点。
	 *
	 * @return 槽位激活刷新前置扩展事件引用
	 */
	FTcsOnPrepareStateSlotActivationSignature& OnPrepareStateSlotActivation() { return PrepareStateSlotActivationEvent; }

	/**
	 * 获取状态调试叠加层扩展点。
	 *
	 * @return 状态调试叠加层扩展事件引用
	 */
	FTcsOnBuildStateDebugOverlaySignature& OnBuildStateDebugOverlay() { return StateDebugOverlayEvent; }

	/**
	 * 构建状态调试叠加层。
	 *
	 * @param StateInstance 目标状态实例
	 * @param OutStackCount 输出叠层数
	 * @param OutDurationText 输出时长文本
	 */
	void BuildStateDebugOverlay(const UTcsStateInstance* StateInstance, int32& OutStackCount, FString& OutDurationText) const;

protected:
	// 槽位激活刷新前置扩展事件。
	FTcsOnPrepareStateSlotActivationSignature PrepareStateSlotActivationEvent;

	// 状态调试叠加层扩展事件。
	mutable FTcsOnBuildStateDebugOverlaySignature StateDebugOverlayEvent;

#pragma endregion


#pragma region StateReferences

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

#pragma endregion


#pragma region StateInstance

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
	virtual bool TryApplyStateInstance(UTcsStateInstance* StateInstance);

protected:
	/**
	 * 在当前组件拥有者上创建状态实例。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param Instigator 状态发起者
	 * @param InLevel 状态等级
	 * @param ParentSourceHandle 父级来源句柄
	 * @param OutFailureReason 可选输出参数，用于返回创建失败原因
	 * @param OutFailureMessage 可选输出参数，用于返回创建失败描述
	 * @param bOutFailureLogged 可选输出参数，用于标记失败是否已在内部记录日志
	 * @return 如果创建成功则返回状态实例，否则返回 nullptr
	 */
	virtual UTcsStateInstance* CreateStateInstance(
		FName StateDefId,
		AActor* Instigator,
		int32 InLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle(),
		ETcsStateApplyFailReason* OutFailureReason = nullptr,
		FString* OutFailureMessage = nullptr,
		bool* bOutFailureLogged = nullptr);

	/**
	 * 评估并写入状态参数。
	 *
	 * @param StateDef 状态定义资产
	 * @param Instigator 状态发起者
	 * @param StateInstance 状态实例
	 * @param OutFailedParams 输出失败的参数名列表
	 * @return 如果所有参数评估成功则返回 true，否则返回 false
	 */
	virtual bool EvaluateAndApplyStateParameters(
		const UTcsStateDefinition* StateDef,
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

public:

	/**
	 * 请求移除指定状态实例。
	 *
	 * @param StateInstance 要移除的状态实例
	 * @param RemovalReason 移除原因
	 * @return 是否成功收敛到移除流程
	 */
	virtual bool RequestStateRemoval(UTcsStateInstance* StateInstance, FName RemovalReason);

	/**
	 * 移除指定状态实例。
	 *
	 * @param StateInstance 要移除的状态实例
	 * @return 是否成功移除
	 */
	virtual bool RemoveState(UTcsStateInstance* StateInstance);

	/**
	 * 按状态定义 ID 移除状态。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param bRemoveAll 是否移除全部匹配实例
	 * @return 成功移除的状态数量
	 */
	virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true);

	/**
	 * 清空指定槽位的所有状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 成功移除的状态数量
	 */
	virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag);

	/**
	 * 清空当前组件中的全部状态。
	 *
	 * @return 成功移除的状态数量
	 */
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

protected:
	// 状态实例索引：按Id/DefId/Slot查询
	UPROPERTY()
	FTcsStateInstanceIndex StateInstanceIndex;

	// StateTree Tick调度器：只保存正在Running的实例
	UPROPERTY()
	FTcsStateTreeTickScheduler StateTreeTickScheduler;

#pragma endregion


#pragma region StateSlot_References

protected:
	// 映射集合：StateSlot 到当前 StateTree 中成功绑定的状态名
	UPROPERTY()
	TMap<FGameplayTag, FName> Mapping_StateSlotToStateTreeStateName;

	// StateSlot 运行时状态数据容器
	UPROPERTY()
	TMap<FGameplayTag, FTcsStateSlot> RuntimeStateSlots;

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
	bool GetStatesInSlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取指定定义 ID 的全部状态实例。
	 *
	 * @param StateDefId 状态定义 ID
	 * @param OutStates 输出状态实例列表
	 * @return 如果找到状态则返回 true，否则返回 false
	 */
	bool GetStatesByDefId(FName StateDefId, TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 获取当前组件中的全部激活状态。
	 *
	 * @param OutStates 输出激活状态列表
	 * @return 如果找到激活状态则返回 true，否则返回 false
	 */
	bool GetAllActiveStates(TArray<UTcsStateInstance*>& OutStates) const;

	/**
	 * 检查是否存在指定定义 ID 的状态。
	 *
	 * @param StateDefId 状态定义 ID
	 * @return 如果存在则返回 true，否则返回 false
	 */
	bool HasStateWithDefId(FName StateDefId) const;

	/**
	 * 检查指定槽位中是否存在激活状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 如果存在激活状态则返回 true，否则返回 false
	 */
	bool HasActiveStateInSlot(FGameplayTag SlotTag) const;

public:
	// 调试输出
	UFUNCTION(BlueprintPure, Category = "State Slot|Debug", meta = (AutoCreateRefTerm = "SlotFilter"))
	FString GetSlotDebugSnapshot(FGameplayTag SlotFilter = FGameplayTag()) const;

	// 状态实例调试输出（按实例枚举，便于定位 Duration/Tick 等字段）
	UFUNCTION(BlueprintPure, Category = "State|Debug")
	FString GetStateDebugSnapshot(FName StateDefIdFilter = NAME_None) const;

	// 槽位Gate开关
    void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

	// 槽位Gate开关状态
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsSlotGateOpen(FGameplayTag SlotTag) const;

protected:
	// 重建当前组件的 StateSlot 运行时容器，并建立与当前 StateTree 的绑定关系。
	virtual void InitStateSlotMappings();

	/**
	 * 根据当前 StateSlotDefinition 重建运行时槽位容器。
	 *
	 * 该步骤会重建 RuntimeStateSlots，并在可复用时保留已有槽位的运行时数据。
	 */
	void RebuildStateSlotRuntimeData();

	/**
	 * 根据当前 StateTree 重建槽位到状态名的绑定表。
	 *
	 * 该步骤只重建 Mapping_StateSlotToStateTreeStateName，
	 * 不修改 RuntimeStateSlots 中已经建立的运行时槽位数据。
	 */
	void RebuildStateTreeSlotBindings();

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

	// 获取指定槽位的运行时槽位数据。
	FTcsStateSlot* FindRuntimeStateSlot(FGameplayTag SlotTag);

	// 获取指定槽位的只读运行时槽位数据。
	const FTcsStateSlot* FindRuntimeStateSlot(FGameplayTag SlotTag) const;

	// 请求刷新指定槽位的激活与扩展逻辑。
	void RequestStateSlotRefresh(FGameplayTag SlotTag);

	// Gate 一致性：当槽位关闭时，强制收敛阶段。
	virtual void EnforceSlotGateConsistency(FGameplayTag SlotTag);

	// 清理槽位中已经过期的状态实例。
	void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot);

	// 按优先级排序槽位中的状态。
	virtual void SortStatesByPriority(TArray<UTcsStateInstance*>& States);

	// 按槽位激活模式处理状态。
	virtual void ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag);

	// 优先级模式：只保留最高优先级状态激活。
	void ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinition* SlotDef);

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
	FStateTreeReference GetStateTreeReference() const;

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

protected:
	// Tick 所有运行中的 StateTree（调度、执行、清理停止的实例）
	void TickStateTrees(float DeltaTime);
};
