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

	// 指定槽位应用状态（数据表行）
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool ApplyStateToSpecificSlot(AActor* TargetActor, FName StateDefId, AActor* SourceActor, FGameplayTag SlotTag, const FInstancedStruct& Parameters);

	/**
	 * 应用状态(返回详细结果版本)
	 * @param TargetActor 目标Actor
	 * @param StateDefId 状态定义ID
	 * @param SourceActor 状态来源Actor
	 * @param Parameters 参数
	 * @return 应用结果结构体，包含成功/失败状态和详细信息
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager", meta = (DisplayName = "Apply State (With Details)"))
	FTcsStateApplyResult ApplyStateWithDetails(AActor* TargetActor, FName StateDefId, AActor* SourceActor, const FInstancedStruct& Parameters);

	/**
	 * 指定槽位应用状态(返回详细结果版本)
	 * @param TargetActor 目标Actor
	 * @param StateDefId 状态定义ID
	 * @param SourceActor 状态来源Actor
	 * @param SlotTag 目标槽位标签
	 * @param Parameters 参数
	 * @return 应用结果结构体，包含成功/失败状态和详细信息
	 */
	UFUNCTION(BlueprintCallable, Category = "State Manager", meta = (DisplayName = "Apply State To Specific Slot (With Details)"))
	FTcsStateApplyResult ApplyStateToSpecificSlotWithDetails(AActor* TargetActor, FName StateDefId, AActor* SourceActor, FGameplayTag SlotTag, const FInstancedStruct& Parameters);

	// 将已有状态实例应用到指定槽位
	UFUNCTION(BlueprintCallable, Category = "State Manager")
	bool ApplyStateInstanceToSlot(AActor* TargetActor, UTcsStateInstance* StateInstance, FGameplayTag SlotTag, bool bAllowFallback = true);

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
