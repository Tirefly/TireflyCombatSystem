// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "StateTreeConditionBase.h"
#include "GameplayTagContainer.h"
#include "TireflyCombatStateTreeTasks.generated.h"

class UTireflyStateInstance;
class UTireflyStateComponent;
struct FTireflyStateDefinition;

//////////////////////////////////////////////////////////////////////////
// StateTree状态槽Task节点
//////////////////////////////////////////////////////////////////////////

/**
 * StateTree互斥槽节点实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyStateSlot_ExclusiveInstanceData
{
	GENERATED_BODY()

	// 接受的状态类型标签
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	FGameplayTagContainer AcceptedStateTags;

	// 优先级
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	int32 Priority = 1;

	// 槽位标识
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	FGameplayTag SlotTag;
};

/**
 * StateTree互斥槽节点
 * 互斥槽：只能有一个状态，新状态会替换旧状态
 */
USTRUCT(meta = (DisplayName = "Tirefly State Slot: Exclusive"))
struct TIREFLYCOMBATSYSTEM_API FTireflyStateSlot_Exclusive : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyStateSlot_ExclusiveInstanceData;

	FTireflyStateSlot_Exclusive();

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyStateSlot_ExclusiveInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

/**
 * StateTree并行槽节点实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyStateSlot_ParallelInstanceData
{
	GENERATED_BODY()

	// 最大堆叠数量 (-1表示无限制)
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	int32 MaxStackCount = -1;

	// 接受的状态类型标签
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	FGameplayTagContainer AcceptedStateTags;

	// 槽位标识
	UPROPERTY(EditAnywhere, Category = "Slot Config")
	FGameplayTag SlotTag;
};

/**
 * StateTree并行槽节点
 * 并行槽：可以有多个状态，支持叠加
 */
USTRUCT(meta = (DisplayName = "Tirefly State Slot: Parallel"))
struct TIREFLYCOMBATSYSTEM_API FTireflyStateSlot_Parallel : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyStateSlot_ParallelInstanceData;

	FTireflyStateSlot_Parallel();

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyStateSlot_ParallelInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

//////////////////////////////////////////////////////////////////////////
// StateTree状态应用Condition节点
//////////////////////////////////////////////////////////////////////////

/**
 * StateTree状态槽可用条件实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyStateCondition_SlotAvailableInstanceData
{
	GENERATED_BODY()

	// 目标槽位
	UPROPERTY(EditAnywhere, Category = "Input")
	FGameplayTag TargetSlot;

	// 要应用的状态定义ID
	UPROPERTY(EditAnywhere, Category = "Input")
	FName StateDefId;
};

/**
 * StateTree状态槽可用条件
 * 检查指定槽位是否可以应用新状态
 */
USTRUCT(meta = (DisplayName = "Tirefly State Condition: Slot Available"))
struct TIREFLYCOMBATSYSTEM_API FTireflyStateCondition_SlotAvailable : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyStateCondition_SlotAvailableInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyStateCondition_SlotAvailableInstanceData::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

//////////////////////////////////////////////////////////////////////////
// StateTree战斗Task节点
//////////////////////////////////////////////////////////////////////////

/**
 * StateTree状态应用Task实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatTask_ApplyStateInstanceData
{
	GENERATED_BODY()

	// 状态定义ID
	UPROPERTY(EditAnywhere, Category = "Input")
	FName StateDefId;

	// 状态目标Actor
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> StateTarget = nullptr;

	// 状态来源Actor
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> StateSource = nullptr;

	// 状态参数
	UPROPERTY(EditAnywhere, Category = "Input")
	FInstancedStruct StateParameters;
};

/**
 * StateTree状态应用Task
 * 在StateTree中应用状态到目标Actor
 */
USTRUCT(meta = (DisplayName = "Tirefly Combat Task: Apply State"))
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatTask_ApplyState : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyCombatTask_ApplyStateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyCombatTask_ApplyStateInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

/**
 * StateTree属性修改Task实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatTask_ModifyAttributeInstanceData
{
	GENERATED_BODY()

	// 属性名称
	UPROPERTY(EditAnywhere, Category = "Input")
	FName AttributeName;

	// 修改值
	UPROPERTY(EditAnywhere, Category = "Input")
	float ModificationValue = 0.0f;

	// 是否为百分比修改
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bIsPercentageModification = false;

	// 修改目标Actor
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> ModificationTarget = nullptr;
};

/**
 * StateTree属性修改Task
 * 在StateTree中修改目标Actor的属性
 */
USTRUCT(meta = (DisplayName = "Tirefly Combat Task: Modify Attribute"))
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatTask_ModifyAttribute : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyCombatTask_ModifyAttributeInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyCombatTask_ModifyAttributeInstanceData::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

//////////////////////////////////////////////////////////////////////////
// StateTree战斗Condition节点
//////////////////////////////////////////////////////////////////////////

/**
 * StateTree属性比较Condition实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatCondition_AttributeComparisonInstanceData
{
	GENERATED_BODY()

	// 属性名称
	UPROPERTY(EditAnywhere, Category = "Input")
	FName AttributeName;

	// 比较值
	UPROPERTY(EditAnywhere, Category = "Input")
	float ComparisonValue = 0.0f;

	// 比较操作类型
	UPROPERTY(EditAnywhere, Category = "Input")
	TEnumAsByte<EArithmeticKeyOperation::Type> ComparisonOperation = EArithmeticKeyOperation::Greater;

	// 比较目标Actor
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> ComparisonTarget = nullptr;
};

/**
 * StateTree属性比较Condition
 * 比较目标Actor的属性值
 */
USTRUCT(meta = (DisplayName = "Tirefly Combat Condition: Attribute Comparison"))
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatCondition_AttributeComparison : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyCombatCondition_AttributeComparisonInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyCombatCondition_AttributeComparisonInstanceData::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};

/**
 * StateTree状态检查Condition实例数据
 */
USTRUCT()
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatCondition_HasStateInstanceData
{
	GENERATED_BODY()

	// 状态定义ID
	UPROPERTY(EditAnywhere, Category = "Input")
	FName StateDefId;

	// 是否检查特定状态
	UPROPERTY(EditAnywhere, Category = "Input")
	bool bCheckSpecificState = true;

	// 要检查的状态标签
	UPROPERTY(EditAnywhere, Category = "Input", meta = (EditCondition = "!bCheckSpecificState"))
	FGameplayTagContainer StateTagsToCheck;

	// 最小堆叠数量
	UPROPERTY(EditAnywhere, Category = "Input")
	int32 MinimumStacks = 1;

	// 检查目标Actor
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<AActor> CheckTarget = nullptr;
};

/**
 * StateTree状态检查Condition
 * 检查目标Actor是否有指定状态
 */
USTRUCT(meta = (DisplayName = "Tirefly Combat Condition: Has State"))
struct TIREFLYCOMBATSYSTEM_API FTireflyCombatCondition_HasState : public FStateTreeConditionBase
{
	GENERATED_BODY()

	using FInstanceDataType = FTireflyCombatCondition_HasStateInstanceData;

	virtual const UStruct* GetInstanceDataType() const override { return FTireflyCombatCondition_HasStateInstanceData::StaticStruct(); }
	virtual bool TestCondition(FStateTreeExecutionContext& Context) const override;

#if WITH_EDITOR
	virtual FText GetDescription(const FGuid& ID, FStateTreeDataView InstanceDataView, const IStateTreeBindingLookup& BindingLookup, EStateTreeNodeFormatting Formatting) const override;
#endif
};