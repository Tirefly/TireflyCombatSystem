// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StateTreeReference.h"
#include "StateTreeInstanceData.h"
#include "StateTreeExecutionTypes.h"
#include "TcsSourceHandle.h"
#include "State/TcsStateParamInstance.h"
#include "TcsStateInstance.generated.h"


class UTcsAttributeComponent;
class UTcsBuffComponent;
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



// StateTree Tick 策略
UENUM(BlueprintType)
enum class ETcsStateTreeTickPolicy : uint8
{
	WhileActive = 0		UMETA(DisplayName = "While Active", ToolTip = "处于Active阶段时加入TickScheduler，按帧推进StateTree"),
	RunOnce				UMETA(DisplayName = "Run Once", ToolTip = "激活时启动并Tick一次(DeltaTime=0)，不加入调度器；若仍在运行则强制Stop并警告"),
	ManualOnly			UMETA(DisplayName = "Manual Only", ToolTip = "只启动，不自动Tick；由外部事件手动Tick推进"),
};



// 状态实例
UCLASS(Abstract, BlueprintType, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsStateInstance : public UObject
{
	GENERATED_BODY()

	// State 模块内部运行时访问
	friend class UTcsStateComponent;
	friend struct FTcsStateInstanceIndex;

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
    UFUNCTION(BlueprintPure, Category = "TireflyCombatSystem|State")
    FName GetStateDefId() const { return StateDefId; }

	// 获取状态实例Id
	int32 GetInstanceId() const { return StateInstanceId; }

	// 获取状态实例的来源句柄
	const FTcsSourceHandle& GetSourceHandle() const { return SourceHandle; }

	// 设置状态实例的来源句柄（由管理器在 CreateStateInstance 中填充）
	void SetSourceHandle(const FTcsSourceHandle& InSourceHandle) { SourceHandle = InSourceHandle; }

#pragma endregion


#pragma region StateParamInstances

public:
	// --- 变量 ---
	// 运行时 StateParam 实例表：数值类
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	TMap<FGameplayTag, FTcsNumericStateParamInstance> NumericParamInstances;

	// 运行时 StateParam 实例表：布尔类
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	TMap<FGameplayTag, FTcsBoolStateParamInstance> BoolParamInstances;

	// 运行时 StateParam 实例表：向量类
	UPROPERTY(BlueprintReadOnly, Category = "Meta")
	TMap<FGameplayTag, FTcsVectorStateParamInstance> VectorParamInstances;

#pragma endregion


#pragma region StateParamPopulation

public:
	// --- 函数 ---
	/**
	 * 从 StateDef 遍历 Parameters 创建并求值 StateParam 实例。
	 *
	 * @param StateDef         状态定义资产
	 * @param Instigator       状态发起者
	 * @param Target           状态目标
	 * @param OutFailedParams  输出失败的参数名列表
	 * @return true 全部成功
	 */
	virtual bool PopulateStateParamInstances(
		const UTcsStateDefinition* StateDef,
		AActor* Instigator,
		AActor* Target,
		TArray<FName>& OutFailedParams);

	// 获取 Numeric 参数实例指针
	virtual FTcsNumericStateParamInstance* GetNumericParamInstance(FGameplayTag Tag);

	// 获取 Bool 参数实例指针
	virtual FTcsBoolStateParamInstance* GetBoolParamInstance(FGameplayTag Tag);

	// 获取 Vector 参数实例指针
	virtual FTcsVectorStateParamInstance* GetVectorParamInstance(FGameplayTag Tag);

	// 获取 Numeric 参数实例表引用（供 ResolveNumericParamInstances 使用）
	virtual TMap<FGameplayTag, FTcsNumericStateParamInstance>& GetNumericParamInstances()
	{
		return NumericParamInstances;
	}

	// 获取 Bool 参数实例表引用
	virtual TMap<FGameplayTag, FTcsBoolStateParamInstance>& GetBoolParamInstances()
	{
		return BoolParamInstances;
	}

	// 获取 Vector 参数实例表引用
	virtual TMap<FGameplayTag, FTcsVectorStateParamInstance>& GetVectorParamInstances()
	{
		return VectorParamInstances;
	}

#pragma endregion


#pragma region MetaProtected

protected:
	// --- 函数 ---
	// 初始化派生运行态的专属参数缓存。
	virtual void InitializeRuntimeParameters();

	/** @return State 模块内部使用的抽象状态定义缓存。 */
	const UTcsStateDefinition* GetStateDef() const { return StateDef; }

protected:
	// --- 变量 ---
	// 状态定义 DataAsset 硬引用
	UPROPERTY(Transient)
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
	virtual int32 GetLevel() const { return Level; }

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
	UTcsBuffComponent* GetOwnerBuffComponent() const { return OwnerBuffCmp.Get(); }

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
	UTcsBuffComponent* GetInstigatorBuffComponent() const { return InstigatorBuffCmp.Get(); }

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

	// 状态实例拥有者的 Buff 组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsBuffComponent> OwnerBuffCmp;

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

	// 状态实例发起者的 Buff 组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsBuffComponent> InstigatorBuffCmp;

	// 状态实例发起者的属性组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsAttributeComponent> InstigatorAttributeCmp;

	// 状态实例发起者的技能组件
	UPROPERTY(BlueprintReadOnly, Category = "State|Runtime")
	TWeakObjectPtr<UTcsSkillComponent> InstigatorSkillCmp;

#pragma endregion


#pragma region Parameter

public:
	/**
	 * 获取数值类型参数的 effective 值（base + 激活中 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 effective 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetNumericParamByTag(FGameplayTag ParameterTag, float& OutValue) const;

	/**
	 * 获取数值类型参数的 base 值（不含 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 base 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetNumericBaseParamByTag(FGameplayTag ParameterTag, float& OutValue) const;

	// 设置数值类型参数的 base 值（写入 NumericValue，不经过 Modifier）
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetNumericParamByTag(FGameplayTag ParameterTag, float Value);

	/**
	 * 获取布尔类型参数的 effective 值（base + 激活中 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 effective 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetBoolParamByTag(FGameplayTag ParameterTag, bool& OutValue) const;

	/**
	 * 获取布尔类型参数的 base 值（不含 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 base 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetBoolBaseParamByTag(FGameplayTag ParameterTag, bool& OutValue) const;

	// 设置布尔类型参数的 base 值
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetBoolParamByTag(FGameplayTag ParameterTag, bool Value);

	/**
	 * 获取向量类型参数的 effective 值（base + 激活中 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 effective 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetVectorParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const;

	/**
	 * 获取向量类型参数的 base 值（不含 SkillModifier）。
	 *
	 * @param ParameterTag 参数标识。
	 * @param OutValue 输出 base 值。
	 * @return 找到参数时返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	bool GetVectorBaseParamByTag(FGameplayTag ParameterTag, FVector& OutValue) const;

	// 设置向量类型参数的 base 值
	UFUNCTION(BlueprintCallable, Category = "State|Parameters")
	void SetVectorParamByTag(FGameplayTag ParameterTag, const FVector& Value);

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
