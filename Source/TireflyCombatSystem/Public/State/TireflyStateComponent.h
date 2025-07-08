// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

protected:

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
