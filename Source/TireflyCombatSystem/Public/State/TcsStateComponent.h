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

// 状态参数变化事件签名（统一为 GameplayTag Key）
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnStateParameterChangedSignature,
	UTcsStateInstance*, StateInstance,
	FGameplayTag, ParameterTag,
	ETcsStateParameterType, ParameterType);

// 状态模块内部联动使用的原生阶段变更事件签名。
DECLARE_MULTICAST_DELEGATE_FourParams(
	FTcsOnInternalStateStageChangedSignature,
	UTcsStateComponent*,
	UTcsStateInstance*,
	ETcsStateStage,
	ETcsStateStage);

// 状态模块内部联动使用的原生应用成功事件签名。
DECLARE_MULTICAST_DELEGATE_FiveParams(
	FTcsOnInternalStateApplySuccessSignature,
	AActor*,
	FName,
	UTcsStateInstance*,
	FGameplayTag,
	ETcsStateStage);

// 状态模块内部联动使用的原生应用失败事件签名。
DECLARE_MULTICAST_DELEGATE_FourParams(
	FTcsOnInternalStateApplyFailedSignature,
	AActor*,
	FName,
	ETcsStateApplyFailReason,
	const FString&);

// 状态模块内部联动使用的原生移除事件签名。
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnInternalStateRemovedSignature,
	UTcsStateComponent*,
	UTcsStateInstance*,
	FName);

// 状态模块内部联动使用的原生槽位 Gate 事件签名。
DECLARE_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnInternalSlotGateStateChangedSignature,
	UTcsStateComponent*,
	FGameplayTag,
	bool);

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
	/** 构造状态组件并初始化基础 StateTree 组件参数。 */
	UTcsStateComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 在 BeginPlay 时预热依赖子系统并初始化槽位映射。 */
	virtual void BeginPlay() override;

	/** 在 Tick 中推进运行中的 StateTree 实例。 */
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

public:
	friend class UTcsStateInstance;
	friend class UTcsBuffComponent;

#pragma endregion


#pragma region EventSurface

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

	/**
	 * 获取供模块内部联动使用的原生阶段变更事件。
	 *
	 * @return 原生阶段变更事件引用；优先供 Buff 等运行时模块绑定
	 */
	FTcsOnInternalStateStageChangedSignature& OnInternalStateStageChanged() { return InternalStateStageChangedEvent; }

	/**
	 * 获取供模块内部联动使用的原生状态应用成功事件。
	 *
	 * @return 原生状态应用成功事件引用；优先供 Buff 等运行时模块绑定
	 */
	FTcsOnInternalStateApplySuccessSignature& OnInternalStateApplySuccess() { return InternalStateApplySuccessEvent; }

	/**
	 * 获取供模块内部联动使用的原生状态应用失败事件。
	 *
	 * @return 原生状态应用失败事件引用；优先供运行时模块绑定
	 */
	FTcsOnInternalStateApplyFailedSignature& OnInternalStateApplyFailed() { return InternalStateApplyFailedEvent; }

	/**
	 * 获取供模块内部联动使用的原生状态移除事件。
	 *
	 * @return 原生状态移除事件引用；优先供 Buff 等运行时模块绑定
	 */
	FTcsOnInternalStateRemovedSignature& OnInternalStateRemoved() { return InternalStateRemovedEvent; }

	/**
	 * 获取供模块内部联动使用的原生槽位 Gate 事件。
	 *
	 * @return 原生槽位 Gate 事件引用；优先供 Buff 等运行时模块绑定
	 */
	FTcsOnInternalSlotGateStateChangedSignature& OnInternalSlotGateStateChanged() { return InternalSlotGateStateChangedEvent; }

protected:
	// 供模块内部联动使用的原生阶段变更事件。
	FTcsOnInternalStateStageChangedSignature InternalStateStageChangedEvent;

	// 供模块内部联动使用的原生状态应用成功事件。
	FTcsOnInternalStateApplySuccessSignature InternalStateApplySuccessEvent;

	// 供模块内部联动使用的原生状态应用失败事件。
	FTcsOnInternalStateApplyFailedSignature InternalStateApplyFailedEvent;

	// 供模块内部联动使用的原生状态移除事件。
	FTcsOnInternalStateRemovedSignature InternalStateRemovedEvent;

	// 供模块内部联动使用的原生槽位 Gate 事件。
	FTcsOnInternalSlotGateStateChangedSignature InternalSlotGateStateChangedEvent;

#pragma endregion


#pragma region ExtensibilityHooks

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


#pragma region ManagerReferences

public:
	/**
	 * 获取共享 StateManager。
	 *
	 * @return 共享 StateManager 子系统；失败时返回 nullptr
	 */
	UTcsStateManagerSubsystem* GetStateManager() const { return const_cast<UTcsStateComponent*>(this)->ResolveStateManager(); }

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


#pragma region ApplyAndCreation

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
	 * 检查状态实例是否满足应用条件。
	 *
	 * @param StateInstance 要检查的状态实例
	 * @return 如果满足应用条件则返回 true，否则返回 false
	 */
	virtual bool CheckStateApplyConditions(UTcsStateInstance* StateInstance);

#pragma endregion


#pragma region LifecycleAndRemoval

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
	UFUNCTION(BlueprintCallable, Category = "State")
	virtual int32 RemoveStatesByDefId(FName StateDefId, bool bRemoveAll = true);

	/**
	 * 清空指定槽位的所有状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State Slot", meta = (AutoCreateRefTerm = "SlotTag"))
	virtual int32 RemoveAllStatesInSlot(FGameplayTag SlotTag);

	/**
	 * 清空当前组件中的全部状态。
	 *
	 * @return 成功移除的状态数量
	 */
	UFUNCTION(BlueprintCallable, Category = "State")
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
	/** 状态实例索引：按 Id / DefId / Slot 查询。 */
	UPROPERTY()
	FTcsStateInstanceIndex StateInstanceIndex;

#pragma endregion


#pragma region SlotRuntimeData

protected:
	// 映射集合：StateSlot 到当前 StateTree 中成功绑定的状态名
	UPROPERTY()
	TMap<FGameplayTag, FName> Mapping_StateSlotToStateTreeStateName;

	// 反向映射缓存：StateTree 状态名到所有受其驱动的槽位。
	// 这是从正向绑定表派生出来的本地索引，只服务于差量刷新路径。
	TMultiMap<FName, FGameplayTag> Mapping_StateTreeStateNameToStateSlotTags;

	// StateSlot 运行时状态数据容器
	UPROPERTY()
	TMap<FGameplayTag, FTcsStateSlot> RuntimeStateSlots;

#pragma endregion


#pragma region QueryAndDebug

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
	UFUNCTION(BlueprintPure, Category = "State")
	bool HasStateWithDefId(FName StateDefId) const;

	/**
	 * 检查指定槽位中是否存在激活状态。
	 *
	 * @param SlotTag 状态槽标签
	 * @return 如果存在激活状态则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintPure, Category = "State Slot", meta = (AutoCreateRefTerm = "SlotTag"))
	bool HasActiveStateInSlot(FGameplayTag SlotTag) const;

public:
	/**
	 * 获取槽位调试快照。
	 *
	 * @param SlotFilter 可选槽位过滤条件
	 * @return 当前槽位运行时的调试文本
	 */
	UFUNCTION(BlueprintPure, Category = "State Slot|Debug", meta = (AutoCreateRefTerm = "SlotFilter"))
	FString GetSlotDebugSnapshot(FGameplayTag SlotFilter = FGameplayTag()) const;

	/**
	 * 获取状态实例调试快照。
	 *
	 * @param StateDefIdFilter 可选状态定义过滤条件
	 * @return 当前状态实例运行时的调试文本
	 */
	UFUNCTION(BlueprintPure, Category = "State|Debug")
	FString GetStateDebugSnapshot(FName StateDefIdFilter = NAME_None) const;

#pragma endregion


#pragma region SlotActivation

	public:
	/**
	 * 设置槽位 Gate 开关状态。
	 *
	 * @param SlotTag 目标槽位标签
	 * @param bOpen 新的 Gate 开关状态
	 */
	UFUNCTION(BlueprintCallable, Category = "StateTree Integration", meta = (AutoCreateRefTerm = "SlotTag"))
	void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

	/**
	 * 查询槽位 Gate 当前是否打开。
	 *
	 * @param SlotTag 目标槽位标签
	 * @return 如果 Gate 打开则返回 true，否则返回 false
	 */
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

	/**
	 * 计算 StateTree 激活状态集合的差异。
	 *
	 * AddedStates 表示本轮新出现的状态名，RemovedStates 表示本轮消失的状态名。
	 * 这里把输入视为集合语义，只关心是否存在，不关心顺序。
	 */
	void DiffStateTreeStateNames(
		const TArray<FName>& NewStates,
		const TArray<FName>& OldStates,
		TSet<FName>& AddedStates,
		TSet<FName>& RemovedStates) const;

	/**
	 * 根据状态名变化集合收集受影响的槽位。
	 *
	 * 优先使用反向绑定缓存；若缓存为空，则保守回退到正向绑定表扫描。
	 */
	void CollectAffectedSlotTagsForStateChanges(
		const TSet<FName>& AddedStates,
		const TSet<FName>& RemovedStates,
		TSet<FGameplayTag>& OutAffectedSlotTags) const;

	// 响应 StateTree 激活状态变更并刷新相关槽位。
	virtual void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates);

	/**
	 * 请求刷新指定槽位的激活结果。
	 *
	 * 这是槽位刷新链路的统一入口：外部逻辑不直接判断“现在该不该立刻重算”，
	 * 而是统一把请求交给这里做分流。
	 *
	 * 分流规则如下：
	 * 1. 如果当前没有重入刷新，也不处于批处理作用域，则立即执行 `UpdateStateSlotActivation()`，
	 *    保持单次变更路径的同步结算语义。
	 * 2. 如果当前正在刷新，或正处于 `BeginStateSlotActivationBatch()` / `EndStateSlotActivationBatch()`
	 *    包裹的批处理作用域内，则本次请求只会写入 `PendingSlotActivationUpdates`。
	 *
	 * 这套批处理的核心作用不是“延后到下一帧”，而是把同一帧、同一批次内的多次槽位语义变化
	 * 压缩成“每个槽位最多一次最终结算”，避免重复执行排序、阶段收敛、广播和清理链路。
	 * 因为待处理请求会在最外层批处理结束时立即排空，所以仍然保持当前帧内完成最终收敛。
	 */
	void RequestUpdateStateSlotActivation(FGameplayTag SlotTag);

	/**
	 * 开始一段槽位激活刷新批处理作用域。
	 *
	 * 调用方应在“会连续触发多个槽位刷新请求，但最终只需要一次稳定结算”的路径外层调用它，
	 * 例如 StateTree Gate 批量开关、批量状态移除等。
	 *
	 * 进入批处理后，`RequestUpdateStateSlotActivation()` 不再立即执行刷新，而是只按槽位去重入队，
	 * 等待最外层批次结束后统一排空。
	 */
	void BeginStateSlotActivationBatch();

	/**
	 * 结束一段槽位激活刷新批处理作用域。
	 *
	 * 只有最外层批处理结束时才会真正触发 `DrainPendingSlotActivationUpdates()`，
	 * 因此嵌套批处理不会过早打断“先集中改语义、再统一结算”的节奏。
	 */
	void EndStateSlotActivationBatch();

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

	/** 清理槽位中已经过期的状态实例。 */
	void ClearStateSlotExpiredStates(FTcsStateSlot* StateSlot);

	/** 按优先级排序槽位中的状态。 */
	virtual void SortStatesByPriority(TArray<UTcsStateInstance*>& States);

	/** 按槽位激活模式处理状态。 */
	virtual void ProcessStateSlotByActivationMode(FTcsStateSlot* StateSlot, FGameplayTag SlotTag);

	/** 优先级模式：只保留最高优先级状态激活。 */
	void ProcessPriorityOnlyMode(FTcsStateSlot* StateSlot, const UTcsStateSlotDefinition* SlotDef);

	/** 全激活模式：槽位中所有状态都保持激活。 */
	void ProcessAllActiveMode(FTcsStateSlot* StateSlot);

	/** 按抢占策略处理低优先级状态。 */
	virtual void ApplyPreemptionPolicyToState(UTcsStateInstance* State, ETcsStatePreemptionPolicy Policy);

	/** 清理槽位中的无效实例。 */
	void CleanupInvalidStates(FTcsStateSlot* StateSlot);

	// 从槽位中移除指定状态实例。
	void RemoveStateFromSlot(FTcsStateSlot* StateSlot, UTcsStateInstance* State, bool bDeactivateIfNeeded = true);

	/** @return 当前激活的 StateTree 状态名列表。 */
	TArray<FName> GetCurrentActiveStateTreeStates() const;

	/** 缓存上一帧的 StateTree 激活状态名，用于检测变化。 */
	TArray<FName> CachedActiveStateNames;

	/** 当前组件是否正在执行槽位激活刷新；用于同帧防重入。 */
	bool bIsUpdatingSlotActivation = false;

	/** 当前组件待排空的槽位激活请求集合；按槽位去重，保证同批次同槽位最多只结算一次。 */
	TSet<FGameplayTag> PendingSlotActivationUpdates;

	/** 当前嵌套的槽位刷新批处理深度。 */
	int32 StateSlotActivationBatchDepth = 0;

#pragma endregion


#pragma region StateTreeIntegration

public:
	/** @return 当前组件持有的 StateTree 引用包装。 */
	FStateTreeReference GetStateTreeReference() const;

	/** @return 当前组件绑定的底层 StateTree 资产。 */
	const UStateTree* GetStateTree() const;

	/**
	 * 由 TcsSTTask_StateChangeNotify 调用，通知 StateTree 状态变更。
	 *
	 * @param Context 执行上下文，包含当前激活状态信息
	 */
	void OnStateTreeStateChanged(const FStateTreeExecutionContext& Context);

protected:
	/**
	 * 比较两组状态名列表是否相等。
	 *
	 * @param A 第一组状态名列表
	 * @param B 第二组状态名列表
	 * @return 如果两组状态名完全相等则返回 true，否则返回 false
	 */
	bool AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const;

	/**
	 * 响应单个槽位变化后的后续联动处理。
	 *
	 * @param SlotTag 发生变化的槽位标签
	 */
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

#pragma endregion


#pragma region RuntimeFlagsAndScheduler

public:
	/**
	 * 将状态实例加入 StateTree Tick 调度器。
	 *
	 * @param StateInstance 要加入调度的状态实例
	 */
	void AddToStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Add(StateInstance); }

	/**
	 * 将状态实例移出 StateTree Tick 调度器。
	 *
	 * @param StateInstance 要移出调度的状态实例
	 */
	void RemoveFromStateTreeTickScheduler(UTcsStateInstance* StateInstance) { StateTreeTickScheduler.Remove(StateInstance); }

protected:
	/** Tick 所有运行中的 StateTree（调度、执行、清理停止的实例）。 */
	void TickStateTrees(float DeltaTime);

	/** StateTree Tick 调度器：只保存正在 Running 的实例。 */
	UPROPERTY()
	FTcsStateTreeTickScheduler StateTreeTickScheduler;

	/** 当前是否处于 StateTree Tick/回调上下文；仅用于移除链路的 ensure 诊断。 */
	bool bIsInStateTreeCallback = false;

	/** @return 当前是否处于 StateTree 更新上下文。 */
	bool IsInStateTreeUpdateContext() const { return bIsInStateTreeCallback; }

#pragma endregion
};
