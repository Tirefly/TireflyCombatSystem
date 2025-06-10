// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "TireflyState.generated.h"



// 状态类型
UENUM(BlueprintType)
enum ETireflyStateType :  uint8
{
	ST_State = 0		UMETA(DisplayName = "State", ToolTip = "状态"),
	ST_Skill			UMETA(DisplayName = "Skill", ToolTip = "技能"),
	ST_Buff				UMETA(DisplayName = "Buff", ToolTip = "BUFF效果"),
};



// 状态持续时间类型
UENUM(BlueprintType)
enum ETireflyStateDurationType : uint8
{
	SDT_None = 0		UMETA(DisplayName = "None", ToolTip = "无持续时间"),
	SDT_Duration		UMETA(DisplayName = "Duration", ToolTip = "有持续时间"),
	SDT_Infinite		UMETA(DisplayName = "Infinite", ToolTip = "无限持续时间"),
};



// 同状态合并策略（来自同一个发起者）
UENUM(BlueprintType)
enum ETireflyStateSameInstigatorMergePolicy : uint8
{
	SMP_None = 0		UMETA(DisplayName = "None", ToolTip = "新老状态并行执行，彼此间互不影响"),
	SMP_RemoveOlder		UMETA(DisplayName = "DiscardOlder", ToolTip = "丢弃旧状态，保留新状态"),
	SMP_RemoveNewer		UMETA(DisplayName = "DiscardNewer", ToolTip = "丢弃新状态，保留旧状态"),
	SMP_Stack			UMETA(DisplayName = "Stack", ToolTip = "新状态叠加到旧状态上"),
};



// 同状态合并策略（来自不同的发起者）
UENUM(BlueprintType)
enum ETireflyStateDiffInstigatorMergePolicy : uint8
{
	DMP_None = 0			UMETA(DisplayName = "None", ToolTip = "新老状态并行执行，彼此间互不影响"),
	DMP_DiscardOlder		UMETA(DisplayName = "DiscardOlder", ToolTip = "丢弃旧状态，保留新状态"),
	DMP_DiscardNewer		UMETA(DisplayName = "DiscardNewer", ToolTip = "丢弃新状态，保留旧状态"),
};



// 状态定义表
USTRUCT(BlueprintType)
struct FTireflyStateDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 状态类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	TEnumAsByte<ETireflyStateType> StateType = ETireflyStateType::ST_State;

	// 状态槽类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta", Meta = (Categories = "StateSlot"))
	FGameplayTag StateSlotType;

	// 状态优先级（值越小，优先级越高，最高优先级为0）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	int32 Priority = -1;

	// 状态类别标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag")
	FGameplayTagContainer CategoryTags;

	// 状态功能标签
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tag")
	FGameplayTagContainer FunctionTags;

	// 状态树资产引用，作为状态的运行时脚本
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	FStateTreeReference StateTreeRef;

	// // 状态的激活条件
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	// TArray<TSubclassOf<UTireflyStateCondition>> ActiveConditions;
	//
	// // 状态的参数集
	// UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	// TMap<FName, FTireflyStateParameter> Parameters;

	// 持续时间类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
	TEnumAsByte<ETireflyStateDurationType> DurationType = ETireflyStateDurationType::SDT_None;

	// 持续时间
	UPROPERTY(Meta = (EditConditionHides, EditCondition = "DurationType == ETireflyStateDurationType::SDT_Duration"),
		EditAnywhere, BlueprintReadOnly, Category = "Duration")
	float Duration = 0.f;

	// 最大叠层数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	int32 MaxStackCount = 1;

	// 同状态合并策略（来自同一个发起者）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TEnumAsByte<ETireflyStateSameInstigatorMergePolicy> SameInstigatorMergePolicy = ETireflyStateSameInstigatorMergePolicy::SMP_None;

	// 同状态合并策略（来自不同的发起者）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TEnumAsByte<ETireflyStateDiffInstigatorMergePolicy> DiffInstigatorMergePolicy = ETireflyStateDiffInstigatorMergePolicy::DMP_None;
};
