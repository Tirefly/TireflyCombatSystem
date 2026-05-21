// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "TcsSourceHandle.h"
#include "TcsStateInstance.generated.h"


class UTcsAttributeComponent;
class UTcsSkillComponent;
class UTcsStateComponent;
class UTcsStateCondition;
class UTcsStateParamExtractor;
class UTcsStateDefinition;



// 状态移除原因常量命名空间
// 用于在 State 生命周期管线中统一移除原因的 FName 值，替代散落的字符串字面量。
namespace TcsStateRemovalReasons
{
	// 自然过期（由运行时生命周期推进后触发）
	static const FName Expired(TEXT("Expired"));
	// 主动移除（由 RemoveState / RemoveStatesByDefId / RemoveAllStatesInSlot / RemoveAllStates 触发）
	static const FName Removed(TEXT("Removed"));
	// 被取消（由 CancelState 触发）
	static const FName Cancelled(TEXT("Cancelled"));
}



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



// 状态应用失败原因
UENUM(BlueprintType)
enum class ETcsStateApplyFailReason : uint8
{
	None = 0						UMETA(DisplayName = "None"),
	InvalidInput					UMETA(DisplayName = "Invalid Input"),
	NoStateComponent				UMETA(DisplayName = "No State Component"),
	InvalidStateDefinition			UMETA(DisplayName = "Invalid State Definition"),
	NoStateSlot						UMETA(DisplayName = "No State Slot"),
	NoStateSlotDefinition			UMETA(DisplayName = "No State Slot Definition"),
	SlotGateClosed_CancelPolicy		UMETA(DisplayName = "Slot Gate Closed (Cancel Policy)"),
	LowerPriorityRejected			UMETA(DisplayName = "Lower Priority Rejected"),
	ApplyConditionsFailed			UMETA(DisplayName = "Apply Conditions Failed"),
	CreateInstanceFailed			UMETA(DisplayName = "Create Instance Failed"),
	AlreadyInSlot					UMETA(DisplayName = "Already In Slot"),
};



// 状态参数键类型
UENUM(BlueprintType)
enum class ETcsStateParameterKeyType : uint8
{
	Name = 0	UMETA(DisplayName = "Name"),
	Tag			UMETA(DisplayName = "Tag"),
};



// StateTree Tick 策略
UENUM(BlueprintType)
enum class ETcsStateTreeTickPolicy : uint8
{
	WhileActive = 0		UMETA(DisplayName = "While Active", ToolTip = "处于Active阶段时加入TickScheduler，按帧推进StateTree"),
	RunOnce				UMETA(DisplayName = "Run Once", ToolTip = "激活时启动并Tick一次(DeltaTime=0)，不加入调度器；若仍在运行则强制Stop并警告"),
	ManualOnly			UMETA(DisplayName = "Manual Only", ToolTip = "只启动，不自动Tick；由外部事件手动Tick推进"),
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

	// 快照配置（共享参数求值时机策略）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Parameter Policy",
	          meta = (ToolTip = "是否为快照参数：快照参数在技能激活时计算一次；非快照参数会实时同步变化"))
	bool bIsSnapshot = true;

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
	virtual void Initialize(
		const UTcsStateDefinition* InStateDef,
		FName InStateDefId,
		AActor* InOwner,
		AActor* InInstigator,
		int32 InInstanceId,
		int32 InLevel = 1);

	// Initialize() succeeds only when Owner/Instigator are valid combat entities and required component refs are resolved.
	bool IsInitialized() const { return bInitialized; }
	
    // 获取状态的定义Id
    FName GetStateDefId() const { return StateDefId; }

	// 获取状态定义 DataAsset 硬引用
	const UTcsStateDefinition* GetStateDef() const { return StateDef; }

	// 获取状态实例Id
	int32 GetInstanceId() const { return StateInstanceId; }

	// 获取状态实例的来源句柄
	const FTcsSourceHandle& GetSourceHandle() const { return SourceHandle; }

	// 设置状态实例的来源句柄（由管理器在 CreateStateInstance 中填充）
	void SetSourceHandle(const FTcsSourceHandle& InSourceHandle) { SourceHandle = InSourceHandle; }

protected:
	// 初始化派生运行态的专属参数缓存。
	virtual void InitializeRuntimeParameters();

	// 状态定义 DataAsset 硬引用
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	const UTcsStateDefinition* StateDef = nullptr;

	// 状态定义Id（冗余字段，用于快速查询和调试）
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FName StateDefId;

	// 状态实例Id
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	int32 StateInstanceId = -1;

	// Internal init guard (not exposed to BP/serialization).
	bool bInitialized = false;

	// 状态实例的来源句柄 (用于追踪 Modifier 来源和因果链)
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	FTcsSourceHandle SourceHandle;

#pragma endregion


#pragma region Lifecycle

public:
	// 获取状态实例应用时间戳
	int64 GetApplyTimestamp() const { return ApplyTimestamp; }

	// 设置状态实例的应用时间戳（创建后不一定立刻应用）
	void SetApplyTimestamp(int64 InTimestamp) { ApplyTimestamp = InTimestamp; }

	// 获取状态实例的当前阶段
	ETcsStateStage GetCurrentStage() const { return Stage; }

	// 设置状态实例的当前阶段；返回 true 表示转换成功，false 表示非法转换（阶段未改变）
	bool SetCurrentStage(ETcsStateStage InStage);

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


#pragma region Parameter_Init

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


#pragma region StateTree

public:
	// 初始化StateTree


	// 热恢复StateTree：保留InstanceData并启动（用于从Pause/HangUp恢复）
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void StartStateTree();

	// 冷启动StateTree：重置InstanceData并启动（用于全新应用状态）
	UFUNCTION(BlueprintCallable, Category = "State|StateTree")
	void RestartStateTree();

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
		return (Stage == ETcsStateStage::SS_HangUp || Stage == ETcsStateStage::SS_Pause);
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

private:
	// 内部实现：启动StateTree
	// @param bResetInstanceData 是否重置InstanceData
	void StartStateTreeInternal(bool bResetInstanceData);

	// StateTree是否在运行
	bool bStateTreeRunning = false;

	// StateTree运行状态
	EStateTreeRunStatus CurrentStateTreeStatus = EStateTreeRunStatus::Unset;
	
	// 状态树实例数据
	UPROPERTY()
	FStateTreeInstanceData StateTreeInstanceData;

#pragma endregion
};
