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
// (状态组件, 状态实例, 移除原因: Expired=自然过期, Removed=主动移除, Cancelled=被取消)
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
	// 友元类声明，允许状态子系统访问私有成员
	friend class UTcsStateManagerSubsystem;

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
	// RemovalReason: "Expired"=自然过期, "Removed"=主动移除, "Cancelled"=被取消
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

	// 状态实例索引：按Id/DefId/Slot查询
	UPROPERTY()
	FTcsStateInstanceIndex StateInstanceIndex;

	// StateTree Tick调度器：只保存正在Running的实例
	UPROPERTY()
	FTcsStateTreeTickScheduler StateTreeTickScheduler;

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
	// 调试输出
	UFUNCTION(BlueprintPure, Category = "State Slot|Debug", meta = (AutoCreateRefTerm = "SlotFilter"))
	FString GetSlotDebugSnapshot(FGameplayTag SlotFilter = FGameplayTag()) const;

	// 槽位Gate开关
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

	// 槽位Gate开关状态
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsSlotGateOpen(FGameplayTag SlotTag) const;

protected:
    // 获取当前激活的StateTree状态名列表
    TArray<FName> GetCurrentActiveStateTreeStates() const;

    // 缓存上一帧的StateTree激活状态名,用于检测变化
    TArray<FName> CachedActiveStateNames;

#pragma endregion


#pragma region StateTree_Reference

public:
	UFUNCTION(BLueprintCallable, Category = "StateTree")
	FStateTreeReference GetStateTreeReference() const;

	UFUNCTION(BLueprintCallable, Category = "StateTree")
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

protected:
	// 状态实例持续时间追踪器（仅SDT_Duration）
	UPROPERTY()
	FTcsStateDurationTracker DurationTracker;

#pragma endregion
};
