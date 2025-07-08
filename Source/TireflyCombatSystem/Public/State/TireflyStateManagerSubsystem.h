// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TireflyStateManagerSubsystem.generated.h"

class UTireflyStateInstance;

UCLASS()
class TIREFLYCOMBATSYSTEM_API UTireflyStateManagerSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// 当状态实例持续时间到期时调用
	void OnStateInstanceDurationExpired(UTireflyStateInstance* StateInstance);

	// 当状态实例持续时间被更新时调用（仅当手动刷新或设置时）
	void OnStateInstanceDurationUpdated(UTireflyStateInstance* StateInstance);

protected:
	// 处理状态实例移除的逻辑
	void HandleStateInstanceRemoval(UTireflyStateInstance* StateInstance);

	// 处理状态实例持续时间更新的逻辑
	void HandleStateInstanceDurationUpdate(UTireflyStateInstance* StateInstance);
};
