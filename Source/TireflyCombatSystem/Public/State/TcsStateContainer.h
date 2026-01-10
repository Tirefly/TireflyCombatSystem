// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsStateContainer.generated.h"



class UTcsStateInstance;



// 状态实例数组结构体
USTRUCT()
struct FTcsStateInstanceArray
{
	GENERATED_BODY()

public:
	// 状态实例数组
	UPROPERTY()
	TArray<TObjectPtr<UTcsStateInstance>> StateInstances;
	
};



// 状态实例索引容器：只负责查询与枚举
USTRUCT()
struct FTcsStateInstanceIndex
{
	GENERATED_BODY()
	
public:
	// 全量实例列表（便于枚举/调试）
	UPROPERTY()
	TArray<TObjectPtr<UTcsStateInstance>> Instances;

	// 通过InstanceId索引
	UPROPERTY()
	TMap<int32, TObjectPtr<UTcsStateInstance>> InstancesById;

	// 通过定义Id索引
	UPROPERTY()
	TMap<FName, FTcsStateInstanceArray> InstancesByName;

	// 通过SlotTag索引
	UPROPERTY()
	TMap<FGameplayTag, FTcsStateInstanceArray> InstancesBySlot;

public:
	void AddInstance(UTcsStateInstance* StateInstance);
	
	void RemoveInstance(UTcsStateInstance* StateInstance);

	void RefreshInstances();

	UTcsStateInstance* GetInstanceById(int32 InstanceId) const;

	bool GetInstancesByName(FName StateDefId, TArray<UTcsStateInstance*>& OutInstances) const;

	bool GetInstancesBySlot(FGameplayTag SlotTag, TArray<UTcsStateInstance*>& OutInstances) const;
};


// 状态持续时间追踪器：只负责 SDT_Duration 的剩余时间存储与更新
USTRUCT()
struct FTcsStateDurationTracker
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TMap<TObjectPtr<UTcsStateInstance>, float> RemainingByInstance;

public:
	void Add(UTcsStateInstance* StateInstance, float InitialRemaining);
	void Remove(UTcsStateInstance* StateInstance);
	bool GetRemaining(const UTcsStateInstance* StateInstance, float& OutRemaining) const;
	bool SetRemaining(UTcsStateInstance* StateInstance, float NewRemaining);
	void RefreshInstances();
};


// StateTree Tick 调度器：只保存正在 Running 的实例
USTRUCT()
struct FTcsStateTreeTickScheduler
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<TObjectPtr<UTcsStateInstance>> RunningInstances;

public:
	void Add(UTcsStateInstance* StateInstance);
	void Remove(UTcsStateInstance* StateInstance);
	void RefreshInstances();
};
