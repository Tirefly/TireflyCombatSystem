// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsStateSlot.h"
#include "State/TcsStateInstance.h"
#include "TcsSourceHandle.h"
#include "TcsStateManagerSubsystem.generated.h"



class UTcsStateInstance;
class UTcsStateComponent;



UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

#pragma region GameInstanceSubsystem

public:
	/** 初始化状态定义缓存与编辑器联动入口。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 释放编辑器联动句柄并清理子系统生命周期资源。 */
	virtual void Deinitialize() override;

#pragma endregion


#pragma region RuntimeState

public:
	/**
	 * 查询当前 StateManager 是否已进入 runtime-ready。
	 *
	 * @return 若当前子系统已完成初始化并可供 runtime bootstrap 使用，则返回 true
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool IsRuntimeReady() const { return bIsRuntimeReady; }

protected:
	/** 当前 StateManager 是否已完成 runtime-ready 初始化。 */
	UPROPERTY(Transient)
	bool bIsRuntimeReady = false;

#pragma endregion


#pragma region RuntimeIds

public:
	/** 分配全局唯一的状态实例 ID（迁移期供 Component 调用的 ID 工厂入口） */
	int32 AllocateStateInstanceId() { return ++GlobalStateInstanceIdMgr; }

protected:
	/** 全局状态实例 ID 计数器。 */
	UPROPERTY()
	int32 GlobalStateInstanceIdMgr = 0;

#pragma endregion


#pragma region CrossActorApplyFacade

public:
	/**
	 * 尝试向目标应用状态
	 *
	 * @param Target 目标，状态将应用到此目标
	 * @param StateDefId 状态定义名，可通过TcsGenericLibrary.GetStateDefIds获取
	 * @param Instigator 状态的发起者
	 * @param StateLevel 状态等级（默认为 1）
	 * @param ParentSourceHandle 父级来源句柄 (用于因果链传递, 默认为空)
	 * @return 如果应用状态成功，则返回true，否则返回false
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool TryApplyStateToTarget(
		AActor* Target,
		FName StateDefId,
		AActor* Instigator,
		int32 StateLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

#pragma endregion
};
