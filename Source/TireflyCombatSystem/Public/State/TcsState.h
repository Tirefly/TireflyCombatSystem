// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StateCondition/TcsStateCondition.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "TcsState.generated.h"


class UTcsAttributeComponent;
class UTcsSkillComponent;
class UTcsStateComponent;
class UTcsStateMerger;
class UTcsStateCondition;
class UTcsStateParamExtractor;



// 状态类型
UENUM(BlueprintType)
enum ETcsStateType :  uint8
{
	ST_State = 0		UMETA(DisplayName = "State", ToolTip = "状态"),
	ST_Skill			UMETA(DisplayName = "Skill", ToolTip = "技能"),
	ST_Buff				UMETA(DisplayName = "Buff", ToolTip = "BUFF效果"),
};



// 状态持续时间类型
UENUM(BlueprintType)
enum ETcsStateDurationType : uint8
{
	SDT_None = 0		UMETA(DisplayName = "None", ToolTip = "无持续时间"),
	SDT_Duration		UMETA(DisplayName = "Duration", ToolTip = "有持续时间"),
	SDT_Infinite		UMETA(DisplayName = "Infinite", ToolTip = "无限持续时间"),
};



// 状态阶段
UENUM(BlueprintType)
enum class ETcsStateStage : uint8
{
	SS_Inactive = 0		UMETA(DisplayName = "Inactive", ToolTip = "未激活"),
	SS_Active			UMETA(DisplayName = "Active", ToolTip = "已激活"),
	SS_HangUp			UMETA(DisplayName = "Hanging", ToolTip = "挂起"),
	SS_Pause			UMETA(DisplayName = "Paused", ToolTip = "暂停"),
	SS_Expired			UMETA(DisplayName = "Expired", ToolTip = "已过期"),
};



// 状态参数类型枚举
UENUM(BlueprintType)
enum class ETcsStateParameterType : uint8
{
	SPT_Numeric = 0		UMETA(DisplayName = "Numeric", ToolTip = "数值类型参数(Float)，需要使用参数解析器计算"),
	SPT_Bool			UMETA(DisplayName = "Bool", ToolTip = "布尔类型参数，直接存储使用"),
	SPT_Vector			UMETA(DisplayName = "Vector", ToolTip = "向量类型参数，直接存储使用"),
};



// 状态参数数据
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsStateParameter
{
	GENERATED_BODY()

public:
	// 参数类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Type")
	ETcsStateParameterType ParameterType = ETcsStateParameterType::SPT_Numeric;

	// 参数值提取类 (仅Numeric类型使用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Numeric Parameter", 
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Numeric", EditConditionHides))
	TSubclassOf<class UTcsStateNumericParamEvaluator> NumericParamEvaluator;

	// 参数值提取类 (仅Bool类型使用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bool Parameter", 
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Bool", EditConditionHides))
	TSubclassOf<class UTcsStateBoolParamEvaluator> BoolParamEvaluator;

	// 参数值提取类 (仅Vector类型使用)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vector Parameter", 
		meta = (EditCondition = "ParameterType == ETcsStateParameterType::SPT_Vector", EditConditionHides))
	TSubclassOf<class UTcsStateVectorParamEvaluator> VectorParamEvaluator;

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
struct TIREFLYCOMBATSYSTEM_API FTcsStateDefinition : public FTableRowBase
{
	GENERATED_BODY()

public:
	// 状态类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta")
	TEnumAsByte<ETcsStateType> StateType = ST_State;

	// 状态槽类型
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta", Meta = (Categories = "StateSlot"))
	FGameplayTag StateSlotType;

	// 状态优先级（值越小，优先级越高，越优先执行，最高优先级为0）
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
	TEnumAsByte<ETcsStateDurationType> DurationType = SDT_None;

	// 持续时间
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Duration",
		Meta = (EditConditionHides, EditCondition = "DurationType == SDT_Duration"))
	float Duration = 0.f;

	// 最大叠层数
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	int32 MaxStackCount = 1;

	// 状态合并策略
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stack")
	TSubclassOf<UTcsStateMerger> MergerType;

	// 状态树资产引用，作为状态的运行时脚本
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Tree")
	FStateTreeReference StateTreeRef;

	// 状态的激活条件配置
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Condition")
	TArray<FTcsStateConditionConfig> ActiveConditions;

	// 状态的参数集（FName 键）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	TMap<FName, FTcsStateParameter> Parameters;

	// 状态的参数集（GameplayTag 键）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter")
	TMap<FGameplayTag, FTcsStateParameter> TagParameters;
};



// 状态实例
UCLASS(BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsStateInstance : public UObject
{
	GENERATED_BODY()

#pragma region UObject

public:
	UTcsStateInstance();

	virtual UWorld* GetWorld() const override;

#pragma endregion


#pragma region Meta

public:
	// 初始化状态实例
	void Initialize(
		const FTcsStateDefinition& InStateDef,
		AActor* InOwner,
		AActor* InInstigator,
		int32 InInstanceId = -1,
		int32 InLevel = -1);

    // 获取状态的定义Id
    FName GetStateDefId() const { return StateDefId; }
	
    // 设置状态的定义Id（由管理器填充）
    void SetStateDefId(FName InStateDefId) { StateDefId = InStateDefId; }

	// 获取状态的定义数据
	FTcsStateDefinition GetStateDef() const { return StateDef; }

	// 获取状态实例Id
	int32 GetInstanceId() const { return StateInstanceId; }

protected:
	// 状态定义Id
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FName StateDefId;
	
	// 状态定义数据
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FTcsStateDefinition StateDef;

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
	ETcsStateStage GetCurrentStage() const { return Stage; }

	// 设置状态实例的当前阶段
	void SetCurrentStage(ETcsStateStage InStage);

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
	ETcsStateStage Stage = ETcsStateStage::SS_Inactive;

	// 是否待GC
	UPROPERTY(BlueprintReadOnly, Category = "Lifecycle")
	bool bPendingGC = false;

#pragma endregion


#pragma region Level

public:
	// 获取状态等级
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	int32 GetLevel() const { return Level; }

	// 设置状态等级
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	void SetLevel(int32 InLevel);

protected:
	// 状态等级
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	int32 Level = -1;

#pragma endregion


#pragma region ObjectRef

public:
	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AActor* GetOwner() const { return Owner.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AController* GetOwnerController() const { return OwnerController.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsStateComponent* GetOwnerStateComponent() const { return OwnerStateCmp.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsAttributeComponent* GetOwnerAttributeComponent() const { return OwnerAttributeCmp.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsSkillComponent* GetOwnerSkillComponent() const { return OwnerSkillCmp.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AActor* GetInstigator() const { return Instigator.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	AController* GetInstigatorController() const { return InstigatorController.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsStateComponent* GetInstigatorStateComponent() const { return InstigatorStateCmp.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsAttributeComponent* GetInstigatorAttributeComponent() const { return InstigatorAttributeCmp.Get(); }

	UFUNCTION(BlueprintCallable, Category = "State|Runtime")
	UTcsSkillComponent* GetInstigatorSkillComponent() const { return InstigatorSkillCmp.Get(); }

protected:
	// 状态实例拥有者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AActor> Owner;

	// 状态实例拥有者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AController> OwnerController;

	// 状态实例拥有者的状态组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsStateComponent> OwnerStateCmp;

	// 状态实例拥有者的属性组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsAttributeComponent> OwnerAttributeCmp;

	// 状态实例拥有者的技能组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsSkillComponent> OwnerSkillCmp;

	// 状态实例的发起者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AActor> Instigator;

	// 状态实例的发起者
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<AController> InstigatorController;

	// 状态实例发起者的状态组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsStateComponent> InstigatorStateCmp;

	// 状态实例发起者的属性组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsAttributeComponent> InstigatorAttributeCmp;

	// 状态实例发起者的技能组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsSkillComponent> InstigatorSkillCmp;

#pragma endregion


#pragma region Parameters

protected:
	void InitParameterValues();

	void InitParameterTagValues();

#pragma endregion


#pragma region Parameter_Numeric

public:
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetNumericParam(FName ParameterName, float& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetNumericParam(FName ParameterName, float Value);

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetNumericParamByTag(FGameplayTag ParameterTag, float& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetNumericParamByTag(FGameplayTag ParameterTag, float Value);

	// 获取所有数值类型参数名称
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllNumericParamNames() const;

	// 获取所有数值类型参数标签
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FGameplayTag> GetAllNumericParamTags() const;

protected:
	// 数值类型参数
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FName, float> NumericParameters;

	// 数值类型参数（Tag）
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FGameplayTag, float> NumericParametersTag;

#pragma endregion


#pragma region Parameter_Bool

public:
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetBoolParam(FName ParameterName, bool& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetBoolParam(FName ParameterName, bool Value);

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetBoolParamByTag(FGameplayTag ParameterTag, bool& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetBoolParamByTag(FGameplayTag ParameterTag, bool Value);

	// 获取所有布尔类型参数名称
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllBoolParamNames() const;

	// 获取所有布尔类型参数标签
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FGameplayTag> GetAllBoolParamTags() const;

protected:
	// 布尔类型参数
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")  
	TMap<FName, bool> BoolParameters;

	// 布尔类型参数（Tag）
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FGameplayTag, bool> BoolParametersTag;

#pragma endregion


#pragma region Parameter_Vector

public:
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetVectorParam(FName ParameterName, FVector& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetVectorParam(FName ParameterName, const FVector& Value);

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetVectorParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const;

	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetVectorParamByTag(FGameplayTag ParameterTag, const FVector& Value);

	// 获取所有向量类型参数名称
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FName> GetAllVectorParamNames() const;

	// 获取所有向量类型参数标签
	UFUNCTION(BlueprintPure, Category = "State|Parameters")
	TArray<FGameplayTag> GetAllVectorParamTags() const;

protected:
	// 向量类型参数
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FName, FVector> VectorParameters;

	// 向量类型参数（Tag）
	UPROPERTY(BlueprintReadOnly, Category = "State|Parameters")
	TMap<FGameplayTag, FVector> VectorParametersTag;

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
	int32 GetStackCount() const;

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

#pragma endregion


#pragma region StateTree

public:
	// 初始化StateTree
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	bool InitializeStateTree();

	// 开始执行StateTree
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void StartStateTree();

	// StateTree更新
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void TickStateTree(float DeltaTime);

	// 停止StateTree
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void StopStateTree();

	// 暂停StateTree
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void PauseStateTree();

	// 恢复StateTree
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void ResumeStateTree();
	
	// StateTree状态查询
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	bool IsStateTreeRunning() const { return bStateTreeRunning; }

	// StateTree暂停状态查询
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	bool IsStateTreePaused() const
	{
		return (Stage == ETcsStateStage::SS_HangUp || Stage == ETcsStateStage::SS_Pause) && !bStateTreeRunning;
	}

	// StateTree运行状态查询
	UFUNCTION(BlueprintPure, Category = "State|StateTree")
	EStateTreeRunStatus GetStateTreeRunStatus() const;
	
	// 向StateTree发送事件
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void SendStateTreeEvent(FGameplayTag EventTag, const FInstancedStruct& EventPayload);

protected:
	// 设置StateTree上下文
	virtual bool SetContextRequirements(FStateTreeExecutionContext& Context);

	// 获取StateTree外部数据
	virtual bool CollectExternalData(
		const FStateTreeExecutionContext& Context,
		const UStateTree* StateTree,
		TArrayView<const FStateTreeExternalDataDesc> ExternalDataDescs,
		TArrayView<FStateTreeDataView> OutDataViews);
	
	// StateTree是否在运行
	bool bStateTreeRunning = false;

	// StateTree运行状态
	EStateTreeRunStatus CurrentStateTreeStatus = EStateTreeRunStatus::Unset;
	
	// 状态树实例数据
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

#pragma endregion
};
