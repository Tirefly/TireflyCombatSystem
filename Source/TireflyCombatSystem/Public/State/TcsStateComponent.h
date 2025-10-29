// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StateTreeComponent.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Containers/ArrayView.h"
#include "TcsState.h"
#include "TcsStateSlot.h"
#include "TcsStateComponent.generated.h"



class UTcsStateInstance;
class UTcsStateManagerSubsystem;
struct FStateTreeStateHandle;



// 状态阶段变更事件签名
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FTcsOnStateStageChangedSignature,
	UTcsStateComponent*, StateComponent,
	UTcsStateInstance*, StateInstance,
	ETcsStateStage, PreviousStage,
	ETcsStateStage, NewStage);

// 状态应用成功事件签名
// (应用到的Actor, 状态定义ID, 创建的状态实例, 目标槽位, 应用后的状态阶段)
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
	ETcsStateApplyFailureReason, FailureReason,
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

	// 持续时间类型
	uint8 DurationType;

	FTcsStateDurationData()
		: StateInstance(nullptr)
		, RemainingDuration(0.0f)
		, DurationType(0)
	{}

	FTcsStateDurationData(
		UTcsStateInstance* InStateInstance,
		float InRemainingDuration,
		uint8 InDurationType)
		: StateInstance(InStateInstance)
		, RemainingDuration(InRemainingDuration)
		, DurationType(InDurationType)
	{}
};



// 槽位排队数据
USTRUCT()
struct FTcsQueuedStateData
{
	GENERATED_BODY()

public:
	// 待应用的状态实例
	UPROPERTY()
	TWeakObjectPtr<UTcsStateInstance> StateInstance = nullptr;

	// 目标槽位
	UPROPERTY()
	FGameplayTag TargetSlot;

	// 入队时间
	double EnqueueTime = 0.0;

	// 存活时间（0 表示无限期）
	double TimeToLive = 0.0;

	bool IsExpired(double CurrentTime) const
	{
		return TimeToLive > 0.0 && (CurrentTime - EnqueueTime) >= TimeToLive;
	}
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

#pragma endregion


#pragma region StateInstance

public:
	// 添加状态实例到管理
	void AddStateInstance(UTcsStateInstance* StateInstance);

	// 从管理中移除状态实例
	void RemoveStateInstance(const UTcsStateInstance* StateInstance);

	// 获取状态实例通过定义ID
	UFUNCTION(BlueprintPure, Category = "State Management")
	UTcsStateInstance* GetStateInstance(FName StateDefId) const;

	// 获取所有激活的状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTcsStateInstance*> GetAllActiveStates() const;

	// 按类型获取状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTcsStateInstance*> GetStatesByType(TEnumAsByte<ETcsStateType> StateType) const;

	// 按标签获取状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTcsStateInstance*> GetStatesByTags(const FGameplayTagContainer& Tags) const;

	// 通知阶段发生变化（内部与外部均可触发）
	void NotifyStateStageChanged(
		UTcsStateInstance* StateInstance,
		ETcsStateStage PreviousStage,
		ETcsStateStage NewStage);
	
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
	// 当前处于激活阶段的状态实例列表
	UPROPERTY()
	TArray<UTcsStateInstance*> ActiveStateInstances;

	// 状态实例索引映射（通过ID）
	UPROPERTY()
	TMap<int32, UTcsStateInstance*> StateInstancesById;

	// 状态实例索引映射（通过定义ID）
	UPROPERTY()
	TMap<FName, UTcsStateInstance*> StateInstancesByDefId;

	// 状态管理器子系统
	UPROPERTY()
	TObjectPtr<UTcsStateManagerSubsystem> StateManagerSubsystem;

#pragma endregion


#pragma region StateSlot

public:
	// 状态槽是否被占用
	UFUNCTION(BlueprintPure, Category = "State Slot")
	bool IsStateSlotOccupied(FGameplayTag SlotTag) const;

	// 获取状态槽当前状态
	UFUNCTION(BlueprintPure, Category = "State Slot")
	UTcsStateInstance* GetStateSlotCurrentState(FGameplayTag SlotTag) const;

	// 获取状态槽当前所有激活状态
	UFUNCTION(BlueprintPure, Category = "State Slot")
	TArray<UTcsStateInstance*> GetActiveStatesInStateSlot(FGameplayTag SlotTag) const;

	// 尝试分配状态到状态槽位
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	bool AssignStateToStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag);

	// 从状态槽位中移除状态
	UFUNCTION(BlueprintCallable, Category = "State Slot") 
	void RemoveStateFromStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag);

	// 清空状态槽位
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	void ClearStateSlot(FGameplayTag SlotTag);
	
	// 获取槽位中最高优先级的激活状态
	UFUNCTION(BlueprintPure, Category = "State Slot")
	UTcsStateInstance* GetHighestPriorityActiveState(FGameplayTag SlotTag) const;
	
	// 获取槽位中所有存储状态（包括非激活）
	UFUNCTION(BlueprintPure, Category = "State Slot")
	TArray<UTcsStateInstance*> GetAllStatesInStateSlot(FGameplayTag SlotTag) const;

	// 调试输出
	UFUNCTION(BlueprintPure, Category = "State Slot|Debug", meta = (AutoCreateRefTerm = "SlotFilter"))
	FString GetSlotDebugSnapshot(FGameplayTag SlotFilter = FGameplayTag()) const;
	
	// 设置槽位配置
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	void SetStateSlotDefinition(const FTcsStateSlotDefinition& SlotDef);
	
	// 获取槽位配置
    UFUNCTION(BlueprintPure, Category = "State Slot")
    FTcsStateSlotDefinition GetStateSlotDefinition(FGameplayTag SlotTag) const;

#pragma endregion


#pragma region StateTreeIntegration

public:
    // 构建 槽位 <-> StateTree 状态 的映射（基于配置表 StateTreeStateName）
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void BuildStateSlotMappings();

    // 槽位对应的 StateTree 状态是否被激活（当前实现回退到槽位是否有激活状态）
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsStateTreeSlotActive(FGameplayTag SlotTag) const;

	// 槽位Gate开关
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

	// 槽位Gate开关状态
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsSlotGateOpen(FGameplayTag SlotTag) const;

	// 同步状态实例到槽位
	void SyncStateInstanceToStateSlot(UTcsStateInstance* StateInstance);
	// 从槽位中移除状态实例
	void RemoveStateInstanceFromStateSlot(UTcsStateInstance* StateInstance);

protected:
    // StateTree状态变化检测与槽位更新
    // 在Tick中调用,检测StateTree的状态变化并更新对应的槽位Gate
    void CheckAndUpdateStateTreeSlots();

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
	// 上次收到Task通知的时间
	double LastTaskNotificationTime = 0.0;

	// 是否检测到Task通知
	bool bHasTaskNotification = false;

	// 轮询间隔（兜底用，默认0.5秒）
	UPROPERTY(EditAnywhere, Category = "StateTree Integration", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float PollingFallbackInterval = 0.5f;

	// 上次轮询时间
	double LastPollingTime = 0.0;

	// 刷新槽位Gate（基于状态差集）
	void RefreshSlotsForStateChange(const TArray<FName>& NewStates, const TArray<FName>& OldStates);

	// 比较两个状态列表是否相等
	bool AreStateNamesEqual(const TArray<FName>& A, const TArray<FName>& B) const;

protected:
    // StateTree状态槽映射 (运行时状态数据)
    UPROPERTY()
    TMap<FGameplayTag, FTcsStateSlot> StateSlots;

    // 槽位配置映射（运行时配置存储,与StateSlots分离以保持配置与状态的职责分离）
    UPROPERTY()
    TMap<FGameplayTag, FTcsStateSlotDefinition> StateSlotDefinitions;

    // 槽位到 StateTree 状态句柄的映射
    TMap<FGameplayTag, struct FStateTreeStateHandle> SlotToStateHandleMap;
	// StateTree状态句柄 到 槽位 的映射
    TMap<struct FStateTreeStateHandle, FGameplayTag> StateHandleToSlotMap;

protected:
	// 状态槽变化事件处理
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

	// 索引管理
	void UpdateStateInstanceIndices(UTcsStateInstance* StateInstance);
	void CleanupStateInstanceIndices(UTcsStateInstance* StateInstance);

private:
	// === 槽位配置私有实现 ===
	
	// 从数据表初始化槽位配置
	void InitializeStateSlots();
	
	// 更新槽位中的状态激活
	void UpdateStateSlotActivation(FGameplayTag SlotTag);
	
	// 状态激活管理
	void ActivateState(UTcsStateInstance* State);
	void DeactivateState(UTcsStateInstance* State);
	
	// 辅助工具函数
	void SortSlotStatesByPriority(TArray<UTcsStateInstance*>& SlotStates);
	void CleanupExpiredStates(TArray<UTcsStateInstance*>& SlotStates);
	bool ApplyStateMergeStrategy(FGameplayTag SlotTag, UTcsStateInstance* NewState, bool bSameInstigator);

	// 排队与延迟应用
	void QueueStateForSlot(UTcsStateInstance* StateInstance, const FTcsStateSlotDefinition& SlotDefinition, FGameplayTag SlotTag);
	void ProcessQueuedStates(float DeltaTime);
	void ClearQueuedStatesForSlot(FGameplayTag SlotTag);

	// Gate 更新调度
	void RequestGateRefresh(FGameplayTag SlotTag);
	void RequestGateRefreshForAll();

	// 排队状态集合
	TMap<FGameplayTag, TArray<FTcsQueuedStateData>> QueuedStatesBySlot;

	// 待刷新 Gate 的槽位集合
	TSet<FGameplayTag> SlotsPendingGateRefresh;

	// Gate 自动刷新（事件兜底）
	UPROPERTY(EditAnywhere, Category = "State Slot")
	float GateAutoRefreshInterval = 0.25f;

	// 上一次 Gate 自动刷新的时间
	double LastGateAutoRefreshTime = 0.0;
	// 初始化时是否完成 Gate 同步
	bool bInitialGateSyncCompleted = false;
	// 是否需要对所有槽位执行一次 Gate 同步
	bool bPendingFullGateRefresh = false;

#pragma endregion

	
#pragma region StateDuration

public:
	// 获取状态实例的剩余时间
	float GetStateInstanceDurationRemaining(const UTcsStateInstance* StateInstance) const;

	// 刷新状态实例的剩余时间
	void RefreshStateInstanceDurationRemaining(UTcsStateInstance* StateInstance);

	// 设置状态实例的剩余时间
	void SetStateInstanceDurationRemaining(UTcsStateInstance* StateInstance, float InDurationRemaining);

protected:
	// 状态实例持续时间映射表
	UPROPERTY()
	TMap<UTcsStateInstance*, FTcsStateDurationData> StateDurationMap;

#pragma endregion
};
