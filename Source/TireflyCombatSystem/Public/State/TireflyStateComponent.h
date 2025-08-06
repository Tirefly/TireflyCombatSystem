// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TireflyStateComponent.generated.h"



class UTireflyStateInstance;
class UTireflyStateManagerSubsystem;



// 状态实例持续时间数据
USTRUCT()
struct FTireflyStateDurationData
{
	GENERATED_BODY()

public:
	// 状态实例
	UPROPERTY()
	TObjectPtr<UTireflyStateInstance> StateInstance;

	// 剩余持续时间
	float RemainingDuration;

	// 持续时间类型
	uint8 DurationType;

	FTireflyStateDurationData()
		: StateInstance(nullptr)
		, RemainingDuration(0.0f)
		, DurationType(0)
	{
	}

	FTireflyStateDurationData(UTireflyStateInstance* InStateInstance, float InRemainingDuration, uint8 InDurationType)
		: StateInstance(InStateInstance)
		, RemainingDuration(InRemainingDuration)
		, DurationType(InDurationType)
	{
	}
};



UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly State Comp"))
class TIREFLYCOMBATSYSTEM_API UTireflyStateComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region ActorComponent

public:
	// Sets default values for this component's properties
	UTireflyStateComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion


#pragma region StateInstance

public:
	// 添加状态实例到管理
	void AddStateInstance(UTireflyStateInstance* StateInstance);

	// 从管理中移除状态实例
	void RemoveStateInstance(const UTireflyStateInstance* StateInstance);

	// 获取状态实例通过定义ID
	UFUNCTION(BlueprintPure, Category = "State Management")
	UTireflyStateInstance* GetStateInstance(FName StateDefId) const;

	// 获取所有激活的状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTireflyStateInstance*> GetAllActiveStates() const;

	// 按类型获取状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTireflyStateInstance*> GetStatesByType(uint8 StateType) const;

	// 按标签获取状态实例
	UFUNCTION(BlueprintPure, Category = "State Management")
	TArray<UTireflyStateInstance*> GetStatesByTags(const FGameplayTagContainer& Tags) const;

protected:
	// 活跃状态实例列表
	UPROPERTY()
	TArray<UTireflyStateInstance*> ActiveStateInstances;

	// 状态实例索引映射（通过ID）
	UPROPERTY()
	TMap<int32, UTireflyStateInstance*> StateInstancesById;

	// 状态实例索引映射（通过定义ID）
	UPROPERTY()
	TMap<FName, UTireflyStateInstance*> StateInstancesByDefId;

	// 状态实例索引映射（通过类型）
	UPROPERTY()
	TMap<uint8, TArray<UTireflyStateInstance*>> StateInstancesByType;

#pragma endregion


#pragma region StateTreeSlots

public:
	// StateTree状态槽管理接口
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	bool IsSlotOccupied(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	UTireflyStateInstance* GetSlotState(FGameplayTag SlotTag) const;

	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	TArray<UTireflyStateInstance*> GetActiveStatesInSlot(FGameplayTag SlotTag) const;

	// 状态槽管理
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	bool TryAssignStateToSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "State|StateTree") 
	void RemoveStateFromSlot(UTireflyStateInstance* StateInstance, FGameplayTag SlotTag);

	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void ClearSlot(FGameplayTag SlotTag);

protected:
	// StateTree状态槽映射
	UPROPERTY()
	TMap<FGameplayTag, TArray<UTireflyStateInstance*>> StateSlots;

	// 状态槽变化事件处理
	virtual void OnStateSlotChanged(FGameplayTag SlotTag);

	// 状态实例与槽位同步
	void SyncStateInstanceToSlot(UTireflyStateInstance* StateInstance);
	void RemoveStateInstanceFromSlot(UTireflyStateInstance* StateInstance);

	// 索引管理
	void UpdateStateInstanceIndices(UTireflyStateInstance* StateInstance);
	void CleanupStateInstanceIndices(UTireflyStateInstance* StateInstance);

#pragma endregion
	// 状态管理器子系统
	UPROPERTY()
	TObjectPtr<UTireflyStateManagerSubsystem> StateManagerSubsystem;

	
#pragma region StateDuration

public:
	// 获取状态实例的剩余时间
	float GetStateInstanceDurationRemaining(const UTireflyStateInstance* StateInstance) const;

	// 刷新状态实例的剩余时间
	void RefreshStateInstanceDurationRemaining(UTireflyStateInstance* StateInstance);

	// 设置状态实例的剩余时间
	void SetStateInstanceDurationRemaining(UTireflyStateInstance* StateInstance, float InDurationRemaining);

protected:
	// 状态实例持续时间映射表
	UPROPERTY()
	TMap<UTireflyStateInstance*, FTireflyStateDurationData> StateDurationMap;

#pragma endregion
};
