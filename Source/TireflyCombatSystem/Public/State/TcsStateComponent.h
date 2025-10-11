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



UCLASS(ClassGroup = (TcsCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly State Comp"))
class TIREFLYCOMBATSYSTEM_API UTcsStateComponent : public UStateTreeComponent
{
    GENERATED_BODY()

#pragma region ActorComponent

public:
	// Sets default values for this component's properties
	UTcsStateComponent();

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

protected:
	// 活跃状态实例列表
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
	// StateTree状态槽管理接口
	UFUNCTION(BlueprintPure, Category = "State Slot")
	bool IsStateSlotOccupied(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "State Slot")
	UTcsStateInstance* GetStateSlotCurrentState(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "State Slot")
	TArray<UTcsStateInstance*> GetActiveStatesInStateSlot(FGameplayTag SlotTag) const;

	// 状态槽管理
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	bool TryAssignStateToStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "State Slot") 
	void RemoveStateFromStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "State Slot")
	void ClearStateSlot(FGameplayTag SlotTag);

	// === 槽位配置管理接口 ===
	
	// 智能状态分配（新版本主接口）
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	bool AssignStateToStateSlot(UTcsStateInstance* StateInstance, FGameplayTag SlotTag);
	
	// 获取槽位中最高优先级的激活状态
	UFUNCTION(BlueprintPure, Category = "State Slot")
	UTcsStateInstance* GetHighestPriorityActiveState(FGameplayTag SlotTag) const;
	
	// 获取槽位中所有存储状态（包括非激活）
	UFUNCTION(BlueprintPure, Category = "State Slot")
	TArray<UTcsStateInstance*> GetAllStatesInStateSlot(FGameplayTag SlotTag) const;
	
	// 设置槽位配置
	UFUNCTION(BlueprintCallable, Category = "State Slot")
	void SetStateSlotDefinition(const FTcsStateSlotDefinition& SlotDef);
	
	// 获取槽位配置
    UFUNCTION(BlueprintPure, Category = "State Slot")
    FTcsStateSlotDefinition GetStateSlotDefinition(FGameplayTag SlotTag) const;

    // ===== StateTree Integration =====
    // 构建 槽位 <-> StateTree 状态 的映射（基于配置表 StateTreeStateName）
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void BuildStateSlotMappings();

    // 槽位对应的 StateTree 状态是否被激活（当前实现回退到槽位是否有激活状态）
    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsStateTreeSlotActive(FGameplayTag SlotTag) const;

    // 槽位 Gate 开关（用于顶层StateTree联动）
    UFUNCTION(BlueprintCallable, Category = "StateTree Integration")
    void SetSlotGateOpen(FGameplayTag SlotTag, bool bOpen);

    UFUNCTION(BlueprintPure, Category = "StateTree Integration")
    bool IsSlotGateOpen(FGameplayTag SlotTag) const;

protected:
    // StateTree状态变化检测与槽位更新
    // 在Tick中调用,检测StateTree的状态变化并更新对应的槽位Gate
    void CheckAndUpdateStateTreeSlots();

    // 获取当前激活的StateTree状态名列表
    // 注意: 需要根据UE 5.6 StateTree API实现
    TArray<FName> GetCurrentActiveStateTreeStates() const;

    // 缓存上一帧的StateTree激活状态名,用于检测变化
    TArray<FName> CachedActiveStateNames;

protected:
    // StateTree状态槽映射 (运行时状态数据)
    UPROPERTY()
    TMap<FGameplayTag, FTcsStateSlot> StateSlots;

    // 槽位配置映射（运行时配置存储,与StateSlots分离以保持配置与状态的职责分离）
    UPROPERTY()
    TMap<FGameplayTag, FTcsStateSlotDefinition> StateSlotDefinitions;

    // 槽位到 StateTree 状态句柄的映射，以及反向映射
    TMap<FGameplayTag, struct FStateTreeStateHandle> SlotToStateHandleMap;
    TMap<struct FStateTreeStateHandle, FGameplayTag> StateHandleToSlotMap;

	// 状态槽变化事件处理
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

	// 状态实例与槽位同步
	void SyncStateInstanceToStateSlot(UTcsStateInstance* StateInstance);
	void RemoveStateInstanceFromStateSlot(UTcsStateInstance* StateInstance);

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
	bool ApplyStateMergeStrategy(UTcsStateInstance* ExistingState, UTcsStateInstance* NewState, bool bSameInstigator);

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
