// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "TireflyState.generated.h"



class UTireflyStateMerger;
class UTireflyStateCondition;
class UTireflyStateParamExtractor;



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



// 状态阶段
UENUM(BlueprintType)
enum class ETireflyStateStage : uint8
{
	Inactive = 0	UMETA(DisplayName = "Inactive", ToolTip = "未激活"),
	Active			UMETA(DisplayName = "Active", ToolTip = "已激活"),
	HangUp			UMETA(DisplayName = "Hanging", ToolTip = "挂起"),
	Expired			UMETA(DisplayName = "Expired", ToolTip = "已过期"),
};



// 状态参数数据
USTRUCT(BlueprintType)
struct FTireflyStateParameter
{
	GENERATED_BODY()

public:
	// 参数值提取类
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UObject> ParamExtractorClass;

	// 参数值容器
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FInstancedStruct ParamValueContainer;
};



// 状态定义表
USTRUCT(BlueprintType)
struct FTireflyStateDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 状态类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	TEnumAsByte<ETireflyStateType> StateType = ST_State;

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

	// 状态的激活条件
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	TArray<TSubclassOf<UTireflyStateCondition>> ActiveConditions;
	
	// 状态的参数集
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	TMap<FName, FTireflyStateParameter> Parameters;

	// 持续时间类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration")
	TEnumAsByte<ETireflyStateDurationType> DurationType = SDT_None;

	// 持续时间
	UPROPERTY(Meta = (EditConditionHides, EditCondition = "DurationType == SDT_Duration"),
		EditAnywhere, BlueprintReadOnly, Category = "Duration")
	float Duration = 0.f;

	// 最大叠层数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	int32 MaxStackCount = 1;

	// 同状态合并策略（来自同一个发起者）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TSubclassOf<UTireflyStateMerger> SameInstigatorMergerType;

	// 同状态合并策略（来自不同的发起者）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TSubclassOf<UTireflyStateMerger> DiffInstigatorMergerType;
};



// 状态实例
UCLASS(BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTireflyStateInstance : public UObject
{
	GENERATED_BODY()

#pragma region StateInstMeta

public:
	// 初始化状态实例
	void Initialize(
		const FTireflyStateDefinition& InStateDef,
		AActor* InOwner,
		int32 InInstanceId = -1,
		int32 InLevel = -1);

	// 更新状态实例
	void Update(float DeltaTime);

	// 获取状态的定义Id
	FName GetStateDefId() const { return StateDefId; }

	// 获取状态的定义数据
	FTireflyStateDefinition GetStateDef() const { return StateDef; }

	// 获取状态实例Id
	int32 GetInstanceId() const { return StateInstanceId; }

protected:
	// 状态定义Id
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FName StateDefId;
	
	// 状态定义数据
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FTireflyStateDefinition StateDef;

	// 状态实例Id
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 StateInstanceId = -1;

#pragma endregion


#pragma region Lifecycle

public:
	// 获取状态实例应用时间戳
	int64 GetApplyTimestamp() const { return ApplyTimestamp; }

	// 设置状态实例的应用时间戳（创建后不一定立刻应用）
	void SetApplyTimestamp(int64 InTimestamp) { ApplyTimestamp = InTimestamp; }

	// 获取状态实例的当前阶段
	ETireflyStateStage GetCurrentStage() const { return Stage; }

	// 设置状态实例的当前阶段
	void SetCurrentStage(ETireflyStateStage InStage);

	// 标记状态实例为待GC
	void MarkPendingGC() { bPendingGC = true; }

	// 检查状态实例是否被标记为待GC
	bool IsPendingGC() const { return bPendingGC; }

protected:
	// 应用时间戳
	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	int64 ApplyTimestamp = -1;

	// 状态阶段
	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	ETireflyStateStage Stage = ETireflyStateStage::Inactive;

	// 是否待GC
	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	bool bPendingGC = false;

#pragma endregion


#pragma region Runtime

public:
	// 获取状态实例的拥有者
	AActor* GetOwner() const { return Owner.Get(); }

	// 获取状态等级
	int32 GetLevel() const { return Level; }

	// 设置状态等级
	void SetLevel(int32 InLevel);

protected:
	// 状态实例拥有者
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	TWeakObjectPtr<AActor> Owner;

	// 状态等级
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	int32 Level = -1;

#pragma endregion


#pragma region Parameters

public:
	// 获取运行时参数
	float GetParamValue(FName ParameterName) const;

	// 设置运行时参数
	void SetParamValue(FName ParameterName, float Value);

protected:
	// 状态参数
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	TMap<FName, float> Parameters;

#pragma endregion


#pragma region Duration

public:
	// 获取状态剩余时间
	float GetRemainingTime() const;

	// 设置状态剩余时间
	void SetRemainingTime(float InRemainingTime);

	// 获取状态总持续时间
	float GetTotalDuration() const;

protected:
	// 状态持续时间
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	float DurationRemaining = 0.0f;

#pragma endregion


#pragma region Stack

public:
	// 检查状态是否可以叠加
	bool CanStack() const;

	// 获取状态叠层数
	int32 GetStackCount() const { return StackCount; }

	// 设置状态叠层数
	void SetStackCount(int32 InStackCount);

	// 增加状态叠层数
	void AddStack(int32 Count = 1);

	// 减少状态叠层数
	void RemoveStack(int32 Count = 1);

protected:
	// 状态叠层数
	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	int32 StackCount = 1;

#pragma endregion


#pragma region StateTree

protected:
	// 状态树数据
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

#pragma endregion
};
