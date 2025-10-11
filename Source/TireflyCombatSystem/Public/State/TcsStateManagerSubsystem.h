// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "State/TcsState.h"
#include "TcsStateManagerSubsystem.generated.h"

class UTcsStateInstance;

UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 获取状态定义
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	FTcsStateDefinition GetStateDefinition(FName StateDefId);

	// 创建状态实例
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	UTcsStateInstance* CreateStateInstance(AActor* Owner, FName StateDefId, AActor* Instigator);

	// 应用状态
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool ApplyState(AActor* TargetActor, FName StateDefId, AActor* SourceActor, const FInstancedStruct& Parameters);

	// 当状态实例持续时间到期时调用
	void OnStateInstanceDurationExpired(UTcsStateInstance* StateInstance);

	// 当状态实例持续时间被更新时调用（仅当手动刷新或设置时）
	void OnStateInstanceDurationUpdated(UTcsStateInstance* StateInstance);

protected:
	// 处理状态实例移除的逻辑
	void HandleStateInstanceRemoval(UTcsStateInstance* StateInstance);

	// 处理状态实例持续时间更新的逻辑
	void HandleStateInstanceDurationUpdate(UTcsStateInstance* StateInstance);
};
