// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "StateTreeExecutionContext.h"
#include "StateTreeExecutionTypes.h"
#include "TireflyStateCondition.h"
#include "TireflyState.generated.h"



class UTireflyStateMerger;
class UTireflyStateCondition;
class UTireflyStateParamExtractor;
class UTireflyStateComponent;



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
	SS_Inactive = 0		UMETA(DisplayName = "Inactive", ToolTip = "未激活"),
	SS_Active			UMETA(DisplayName = "Active", ToolTip = "已激活"),
	SS_HangUp			UMETA(DisplayName = "Hanging", ToolTip = "挂起"),
	SS_Expired			UMETA(DisplayName = "Expired", ToolTip = "已过期"),
};



// 状态参数类型枚举
UENUM(BlueprintType)
enum class ETireflyStateParameterType : uint8
{
	Numeric = 0		UMETA(DisplayName = "Numeric", ToolTip = "数值类型参数(Float)，需要使用参数解析器计算"),
	Bool			UMETA(DisplayName = "Bool", ToolTip = "布尔类型参数，直接存储使用"),
	Vector			UMETA(DisplayName = "Vector", ToolTip = "向量类型参数，直接存储使用"),
};



// 状态参数数据
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyStateParameter
{
	GENERATED_BODY()

public:
	// 参数类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Type")
	ETireflyStateParameterType ParameterType = ETireflyStateParameterType::Numeric;

	// 参数值提取类 (仅Numeric类型使用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Parameter", 
		meta = (EditCondition = "ParameterType == ETireflyStateParameterType::Numeric", EditConditionHides))
	TSubclassOf<class UTireflyStateParamParser> ParamResolverClass;

	// 参数值容器
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Value")
	FInstancedStruct ParamValueContainer;

	// 快照配置 (仅技能使用，其他状态类型忽略此设置)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill Parameter", 
		meta = (ToolTip = "是否为快照参数：快照参数在技能激活时计算一次；非快照参数会实时同步变化"))
	bool bIsSnapshot = true;
};



// 状态定义表
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTireflyStateDefinition : public FTableRowBase
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

	// 状态树资产引用，作为状态的运行时脚本
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	FStateTreeReference StateTreeRef;

	// 状态的激活条件配置
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<FTireflyStateConditionConfig> ActiveConditions;

	// 状态的参数集
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	TMap<FName, FTireflyStateParameter> Parameters;
};



// 状态实例
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTireflyStateInstance : public UObject
{
	GENERATED_BODY()

#pragma region GameplayObject

public:
	UTireflyStateInstance();

	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region Meta

public:
	// 初始化状态实例
	void Initialize(
		const FTireflyStateDefinition& InStateDef,
		AActor* InOwner,
		AActor* InInstigator,
		int32 InInstanceId = -1,
		int32 InLevel = -1);

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
	ETireflyStateStage Stage = ETireflyStateStage::SS_Inactive;

	// 是否待GC
	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	bool bPendingGC = false;

#pragma endregion


#pragma region Runtime

public:
	// 获取状态实例的拥有者
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AActor* GetOwner() const { return Owner.Get(); }

	// 获取状态实例的发起者
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AActor* GetInstigator() const { return Instigator.Get(); }

	// 获取状态等级
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	int32 GetLevel() const { return Level; }

	// 设置状态等级
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	void SetLevel(int32 InLevel);

protected:
	// 状态实例拥有者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AActor> Owner;

	// 状态实例的发起者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AActor> Instigator;

	// 状态等级
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	int32 Level = -1;

#pragma endregion


#pragma region Parameters

public:
	// 数值类型参数操作接口
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	float GetParamValue(FName ParameterName) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetParamValue(FName ParameterName, float Value);

	// 布尔类型参数操作接口
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetBoolParam(FName ParameterName, bool DefaultValue = false) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetBoolParam(FName ParameterName, bool Value);

	// 向量类型参数操作接口
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	FVector GetVectorParam(FName ParameterName, const FVector& DefaultValue = FVector::ZeroVector) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetVectorParam(FName ParameterName, const FVector& Value);

	// 参数检查接口
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	bool HasNumericParam(FName ParameterName) const;

	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	bool HasBoolParam(FName ParameterName) const;

	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	bool HasVectorParam(FName ParameterName) const;

	// 获取所有参数名称
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllNumericParamNames() const;

	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllBoolParamNames() const;

	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllVectorParamNames() const;

protected:
	// 数值类型参数 (原Parameters重命名)
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FName, float> NumericParameters;

	// 布尔类型参数
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")  
	TMap<FName, bool> BoolParameters;

	// 向量类型参数
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FName, FVector> VectorParameters;

#pragma endregion


#pragma region Duration

public:
	// 获取状态剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	float GetDurationRemaining() const;

	// 刷新状态剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	void RefreshDurationRemaining();

	// 设置状态剩余时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	void SetDurationRemaining(float InDurationRemaining);

	// 获取状态总持续时间
	UFUNCTION(BlueprintCallable, Category = "State|Duration")
	float GetTotalDuration() const;

#pragma endregion


#pragma region Stack

public:
	// 检查状态是否可以叠加
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	bool CanStack() const;

	// 获取状态叠层数
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	int32 GetStackCount() const { return StackCount; }

	// 获取状态最大叠层数
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	int32 GetMaxStackCount() const;

	// 设置状态叠层数
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	void SetStackCount(int32 InStackCount);

	// 增加状态叠层数
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	void AddStack(int32 Count = 1);

	// 减少状态叠层数
	UFUNCTION(BlueprintCallable, Category = "State|Stack")
	void RemoveStack(int32 Count = 1);

protected:
	// 状态叠层数
	UPROPERTY(BlueprintReadOnly, Category = "State|Stack")
	int32 StackCount = 1;

#pragma endregion


#pragma region StateTree

public:
	// StateTree生命周期管理
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	bool InitializeStateTree();
	
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void StartStateTree();
	
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void TickStateTree(float DeltaTime);
	
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void StopStateTree();
	
	// StateTree状态查询
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	bool IsStateTreeRunning() const { return bStateTreeRunning; }
	
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	EStateTreeRunStatus GetStateTreeRunStatus() const;
	
	// StateTree事件发送
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void SendStateTreeEvent(FGameplayTag EventTag, const FInstancedStruct& EventPayload);

protected:
	// StateTree上下文设置
	virtual bool SetupStateTreeContext(FStateTreeExecutionContext& Context);
	virtual bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews);
	
	// StateTree运行状态
	bool bStateTreeRunning = false;
	EStateTreeRunStatus CurrentStateTreeStatus = EStateTreeRunStatus::Unset;
	
	// 状态树数据
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

#pragma endregion
};
