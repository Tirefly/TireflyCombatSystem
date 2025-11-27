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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FTcsOnStateApplyFailedSignature,
	AActor*, TargetActor,
	FName, StateDefId,
	FString, FailureMessage
);



// 状态实例持续时间数据
USTRUCT()
struct FTcsStateDurationData
{
	GENERATED_BODY()

public:
	// 状态实例
	UPROPERTY()
	TObjectPtr<UTcsStateInstance> StateInstance;

	// 剩余持续时间
	float RemainingDuration;

	FTcsStateDurationData()
		: StateInstance(nullptr)
		, RemainingDuration(0.0f)
	{}

	FTcsStateDurationData(
		UTcsStateInstance* InStateInstance,
		float InRemainingDuration)
		: StateInstance(InStateInstance)
		, RemainingDuration(InRemainingDuration)
	{}
};



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

public:
	// 状态阶段变更事件（槽位联动）
	UPROPERTY(BlueprintAssignable, Category = "State|Events")
	FTcsOnStateStageChangedSignature OnStateStageChanged;

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

protected:
	// 状态管理器子系统
	UPROPERTY()
	TObjectPtr<UTcsStateManagerSubsystem> StateMgr;

	// 持久化状态实例容器
	UPROPERTY()
	FTcsPersistentStateInstanceContainer PersistentStateInstances;

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


#pragma region StateTree_Reference

public:
	UFUNCTION(BLueprintCallable, Category = "StateTree")
	FStateTreeReference GetStateTreeReference() const;

	UFUNCTION(BLueprintCallable, Category = "StateTree")
	const UStateTree* GetStateTree() const;

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


#pragma region StateTreeState

public:
	/**
	 * 由TcsStateChangeNotifyTask调用，通知StateTree状态变更
	 * @param Context 执行上下文，包含当前激活状态信息
	 */
	void OnStateTreeStateChanged(const FStateTreeExecutionContext& Context);

protected:
	// 刷新槽位Gate（基于状态差集）
	void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates);

	// 比较两个状态列表是否相等
	bool AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const;

protected:
	// 状态槽变化事件处理
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

	// 更新槽位中的状态激活
	void UpdateStateSlotActivation(FGameplayTag SlotTag);

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
	// 状态实例持续时间映射表
	UPROPERTY()
	TMap<UTcsStateInstance*, FTcsStateDurationData> StateDurationMap;

#pragma endregion
};
