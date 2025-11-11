// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
	TArray<UTcsStateInstance*> StateInstances;
	
};



// 活跃状态实例容器
USTRUCT()
struct FTcsPersistentStateInstanceContainer
{
	GENERATED_BODY()
	
public:
	// 活跃状态实例列表
	UPROPERTY()
	TArray<UTcsStateInstance*> Instances;

	// 通过Id索引的活跃状态实例映射
	UPROPERTY()
	TMap<int32, UTcsStateInstance*> InstancesById;

	// 通过定义Id索引的活跃状态实例映射
	UPROPERTY()
	TMap<FName, FTcsStateInstanceArray> InstancesByName;

public:
	void AddInstance(UTcsStateInstance* StateInstance);
	
	void RemoveInstance(UTcsStateInstance* StateInstance);

	void RefreshInstances();

	UTcsStateInstance* GetInstanceById(int32 InstanceId) const;

	bool GetInstancesByName(FName StateDefId, TArray<UTcsStateInstance*>& OutInstances) const;
};