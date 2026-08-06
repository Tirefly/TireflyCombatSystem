// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TcsAttributeInstance.h"
#include "TcsAttributeChangeEventPayload.h"
#include "TcsAttributeEvaluationSnapshot.h"
#include "TcsAttributeModifier.h"
#include "TcsAttributeModifierApplication.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeComponent.generated.h"



class UTcsDefinitionManagerSubsystem;
class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;
class UTcsRuntimeBootstrapSubsystem;
class UTcsStateInstance;
class UTcsSkillEntry;



// 属性值改变事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsAttributeChangeDelegate,
	const TArray<FTcsAttributeChangeEventPayload>&, Payloads);

// 属性修改器批量事件委托声明
// (事件载荷列表)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsAttributeModifierBatchDelegate,
	const TArray<FTcsAttributeModifierEventPayload>&, Payloads);

// 属性边界批量事件委托声明
// (事件载荷列表)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsAttributeBoundaryBatchDelegate,
	const TArray<FTcsAttributeBoundaryEventPayload>&, Payloads);



// 属性组件，保存战斗实体的属性相关数据
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Cmp"))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

#pragma region FriendClasses

	friend class UTcsRuntimeBootstrapSubsystem;

#pragma endregion


#pragma region ActorComponent

public:
	/** 构造属性组件并初始化默认 Tick 策略。 */
	UTcsAttributeComponent();

protected:
	/** 在组件初始化时接入 runtime bootstrap。 */
	virtual void InitializeComponent() override;

	/** 在组件反初始化时退出 runtime bootstrap。 */
	virtual void UninitializeComponent() override;

	/** 在 BeginPlay 时预热 AttributeManager 缓存。 */
	virtual void BeginPlay() override;

#pragma endregion


#pragma region RuntimeBootstrap

public:
	/**
	 * 查询当前 AttributeComponent 是否已完成 runtime prepare。
	 *
	 * @return 若当前组件已完成 runtime prepare，则返回 true
	 */
	UFUNCTION(BlueprintPure, Category = "Attribute|Runtime")
	bool IsRuntimePrepared() const { return bRuntimePrepared; }

protected:
	/** 缓存的 RuntimeBootstrapSubsystem 指针。 */
	UPROPERTY(Transient)
	TObjectPtr<UTcsRuntimeBootstrapSubsystem> RuntimeBootstrapSubsystem;

	/** 当前 AttributeComponent 是否已完成 runtime prepare。 */
	UPROPERTY(Transient)
	bool bRuntimePrepared = false;

	/**
	 * 显式执行 Attribute runtime prepare。
	 *
	 * @return 若 prepare 成功，则返回 true
	 */
	bool PrepareAttributeRuntime();

	/**
	 * 懒加载获取 RuntimeBootstrapSubsystem。
	 *
	 * @return RuntimeBootstrapSubsystem 指针；失败时返回 nullptr
	 */
	UTcsRuntimeBootstrapSubsystem* ResolveRuntimeBootstrapSubsystem();

#pragma endregion


// 全局 ID 工厂与 DefinitionManager 引用
#pragma region ManagerReference

protected:
	/** 全局自增的 AttributeInstance ID 计数器（进程级唯一）。 */
	static int32 NextAttributeInstanceId;

	/** 全局自增的 ModifierInstance ID 计数器（进程级唯一）。 */
	static int32 NextModifierInstanceId;

	/** 缓存的 DefinitionManager 指针。 */
	UPROPERTY()
	TObjectPtr<UTcsDefinitionManagerSubsystem> DefinitionMgr;

public:
	/** 分配全局唯一的 AttributeInstance ID。 */
	static int32 AllocateAttributeInstanceId() { return ++NextAttributeInstanceId; }

	/** 分配全局唯一的 ModifierInstance ID。 */
	static int32 AllocateModifierInstanceId() { return ++NextModifierInstanceId; }

protected:
	/**
	 * 懒加载获取 DefinitionManager。
	 *
	 * @return DefinitionManager 指针；失败时返回 nullptr 并触发 ensureMsgf。
	 */
	UTcsDefinitionManagerSubsystem* ResolveDefinitionManager();

#pragma endregion


#pragma region QueryAndSnapshot

public:
	/**
	 * 获取特定属性的当前值。
	 *
	 * @param AttributeName 属性名称
	 * @param OutValue 输出属性当前值
	 * @return 如果属性存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	/**
	 * 通过 GameplayTag 检查属性是否存在。
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @return 如果 Tag 能解析且组件中已存在该属性则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool HasAttributeByTag(const FGameplayTag& AttributeTag) const;

	/**
	 * 通过 GameplayTag 获取特定属性的当前值。
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param OutValue 输出属性当前值
	 * @return 如果 Tag 能解析且属性存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const;

	/**
	 * 获取特定属性的基础值。
	 *
	 * @param AttributeName 属性名称
	 * @param OutValue 输出属性基础值
	 * @return 如果属性存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	/**
	 * 通过 GameplayTag 获取特定属性的基础值。
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param OutValue 输出属性基础值
	 * @return 如果 Tag 能解析且属性存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValueByTag(const FGameplayTag& AttributeTag, float& OutValue) const;

	/** @return 当前组件中全部属性的 CurrentValue 快照。 */
	TMap<FName, float> GetAttributeValues() const;

	/** @return 当前组件中全部属性的 BaseValue 快照。 */
	TMap<FName, float> GetAttributeBaseValues() const;

#pragma endregion


#pragma region EventBroadcast

public:

	/**
	 * 广播属性当前值变化事件。
	 *
	 * @param Payloads 本次变化事件载荷列表
	 */
	void BroadcastAttributeValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;

	/**
	 * 广播属性基础值变化事件。
	 *
	 * @param Payloads 本次变化事件载荷列表
	 */
	void BroadcastAttributeBaseValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;

	/**
	 * 广播属性修改器批量添加事件。
	 *
	 * @param Payloads 本次修改器添加事件载荷列表
	 */
	void BroadcastAttributeModifierAddedBatchEvent(const TArray<FTcsAttributeModifierEventPayload>& Payloads) const;

	/**
	 * 广播属性修改器批量移除事件。
	 *
	 * @param Payloads 本次修改器移除事件载荷列表
	 */
	void BroadcastAttributeModifierRemovedBatchEvent(const TArray<FTcsAttributeModifierEventPayload>& Payloads) const;

	/**
	 * 广播属性修改器批量更新事件。
	 *
	 * @param Payloads 本次修改器更新事件载荷列表
	 */
	void BroadcastAttributeModifierUpdatedBatchEvent(const TArray<FTcsAttributeModifierEventPayload>& Payloads) const;

	/**
	 * 广播属性达到边界值事件。
	 *
	 * @param Payloads 本次边界事件载荷列表
	 */
	void BroadcastAttributeReachedBoundaryBatchEvent(const TArray<FTcsAttributeBoundaryEventPayload>& Payloads) const;

	/** 属性当前值改变事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeValueChanged;

	/** 属性基础值改变事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeBaseValueChanged;

	/** 属性修改器批量添加事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeModifierBatchDelegate OnAttributeModifiersAdded;

	/** 属性修改器批量移除事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeModifierBatchDelegate OnAttributeModifiersRemoved;

	/** 属性修改器批量更新事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeModifierBatchDelegate OnAttributeModifiersUpdated;

	/** 属性达到边界值批量事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeBoundaryBatchDelegate OnAttributesReachedBoundary;

#pragma endregion


#pragma region AttributeInstanceLifecycle

public:
	/**
	 * 给当前战斗实体添加属性。
	 *
	 * @param AttributeName 属性名称
	 * @return 是否成功添加
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool AddAttribute(UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

	/**
	 * 批量给当前战斗实体添加属性。
	 *
	 * @param AttributeNames 要添加的属性名称列表
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void AddAttributes(const TArray<FName>& AttributeNames);

	/**
	 * 通过 GameplayTag 给战斗实体添加属性（非 virtual，通过 Tag 解析后调用 AddAttribute）
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @return 是否成功添加（Tag 有效、在映射中注册、且属性不存在时返回 true）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool AddAttributeByTag(const FGameplayTag& AttributeTag);

	/**
	 * 直接设置属性的 Base 值
	 *
	 * @param AttributeName 属性名称
	 * @param NewValue 新的 Base 值
	 * @param bTriggerEvents 是否触发事件（默认 true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool SetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * 移除属性和所有属性相关的修改器
	 *
	 * @param AttributeName 属性名称
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool RemoveAttribute(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

#pragma endregion


#pragma region ModifierApplication

public:
	/**
	 * 唯一 AttributeModifier Application 入口。
	 *
	 * Instant 原子写入 BaseValue；Ongoing 创建由本地 StateInstance 持有的可撤销父实例。
	 * @param Request Application 输入与可选 Operand 覆写。
	 * @param OutResult 输出逐 Operation 审计结果。
	 * @return 全部 Operation 成功提交时返回 true。
	 */
	virtual bool ApplyAttributeModifier(
		const FTcsAttributeModifierApplicationRequest& Request,
		FTcsAttributeModifierApplicationResult& OutResult);

	/**
	 * 按 SourceHandle 移除全部 Ongoing AttributeModifier 父实例。
	 *
	 * @param SourceHandle 要清理的有效来源句柄。
	 * @return 至少移除一个父实例时返回 true。
	 */
	virtual bool RemoveOngoingModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);

#pragma endregion


#pragma region AttributeCalculation

protected:
	/**
	 * 以给定 BaseValue 工作集及 Ongoing 父实例构建不可变 Snapshot。
	 *
	 * @param BaseValues 当前事务中的候选 BaseValue。
	 * @param ModifierInstances 当前参与聚合的 Ongoing 父实例。
	 * @param ExcludedModifierInstId 要虚拟排除的父实例 ID；负值表示不排除。
	 * @param OutSnapshot 输出只读数值 Snapshot。
	 * @return 全部已应用 Operation 可安全重放时返回 true。
	 */
	bool BuildAttributeEvaluationSnapshot(
		const TMap<FName, float>& BaseValues,
		const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		int32 ExcludedModifierInstId,
		FTcsAttributeEvaluationSnapshot& OutSnapshot,
		FTcsAttributeModifierApplicationResult* InOutResult = nullptr);

	/**
	 * 解析并稳定排序单个 Definition 的全部 Operation。
	 *
	 * @param ModifierDefinition 要解析的 AttributeModifier Definition。
	 * @param OperationOverrides 本轮允许的 Evaluator / Payload 覆写。
	 * @param SourceHandle 本轮有效来源句柄。
	 * @param SourceStateInstance 可选来源 StateInstance。
	 * @param SourceSkillEntry 可选来源 SkillEntry。
	 * @param Snapshot 本轮共享的只读 Attribute Snapshot。
	 * @param OutOperations 输出按 OperationId 排序的已求值 Operation。
	 * @param InOutResult 输出失败和逐 Operation 审计结果。
	 * @return 全部 Operation 都成功求值时返回 true。
	 */
	bool BuildEvaluatedAttributeOperations(
		const UTcsAttributeModifierDefinition& ModifierDefinition,
		const TMap<FName, FTcsAttributeModifierOperationOverride>& OperationOverrides,
		const FTcsSourceHandle& SourceHandle,
		UTcsStateInstance* SourceStateInstance,
		UTcsSkillEntry* SourceSkillEntry,
		const FTcsAttributeEvaluationSnapshot& Snapshot,
		TArray<FTcsEvaluatedAttributeOperation>& OutOperations,
		FTcsAttributeModifierApplicationResult* InOutResult = nullptr) const;

	/**
	 * 以候选 BaseValue 和 Ongoing 父实例重算全部 CurrentValue 及动态 Operand。
	 *
	 * @param BaseValues 当前事务的候选 BaseValue。
	 * @param ModifierInstances 参与本轮重算的 Ongoing 父实例。
	 * @param OutUpdatedModifierInstances 输出带最新 EvaluatedOperation 的父实例。
	 * @param OutCurrentValues 输出候选 CurrentValue。
	 * @param AuditedModifierInstId 需要写入 OutResult 的父实例 ID；负值表示无需审计。
	 * @param InOutResult 可选逐 Operation 审计输出。
	 * @return 全部父实例都能以自排除 Snapshot 重算时返回 true。
	 */
	bool BuildOngoingAttributeValues(
		const TMap<FName, float>& BaseValues,
		const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		TArray<FTcsAttributeModifierInstance>& OutUpdatedModifierInstances,
		TMap<FName, float>& OutCurrentValues,
		int32 AuditedModifierInstId = INDEX_NONE,
		FTcsAttributeModifierApplicationResult* InOutResult = nullptr);

	/**
	 * 校验 AttributeModifier Definition 的 Operator / Merger 兼容性。
	 *
	 * @param ModifierDefinition 待校验 Definition。
	 * @param InOutResult 可选失败原因输出。
	 * @return 兼容时返回 true。
	 */
	bool ValidateAttributeModifierDefinitionCompatibility(
		const UTcsAttributeModifierDefinition& ModifierDefinition,
		FTcsAttributeModifierApplicationResult* InOutResult = nullptr) const;

	/**
	 * 按 ModifierDefId 分组并对同组 Ongoing 父实例执行 Merger。
	 *
	 * @param ModifierInstances 已求值的 Ongoing 父实例。
	 * @param OutMergedModifierInstances 输出 Merger 后的有效父实例集合。
	 * @param InOutResult 可选失败原因输出。
	 * @return 全部组合均可安全合并时返回 true。
	 */
	bool MergeOngoingModifierInstances(
		const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		TArray<FTcsAttributeModifierInstance>& OutMergedModifierInstances,
		FTcsAttributeModifierApplicationResult* InOutResult = nullptr) const;

	/**
	 * 将已求值 Operation 按稳定顺序施加到候选 Attribute 值集合。
	 *
	 * @param Operations 待施加的 Operation 集合。
	 * @param InOutValues 候选 Attribute 值集合。
	 * @param InOutResult 可选逐 Operation 审计输出。
	 * @return 全部 Operator 成功且结果有限时返回 true。
	 */
	bool ApplyEvaluatedOperationsToValues(
		const TArray<FTcsEvaluatedAttributeOperation>& Operations,
		TMap<FName, float>& InOutValues,
		FTcsAttributeModifierApplicationResult* InOutResult) const;

	/**
	 * 对候选 Attribute 值工作集执行独立的范围约束 fixpoint。
	 *
	 * @param InOutValues 待收敛的 BaseValue 或 CurrentValue 候选集合。
	 * @return 在最大迭代次数内收敛时返回 true。
	 */
	bool ClampCandidateAttributeValues(TMap<FName, float>& InOutValues);

	/**
	 * 将同一事务中的 BaseValue、CurrentValue 与 Ongoing 父实例一次性提交，并广播最终稳定态。
	 *
	 * @param BaseValues 成功求得的候选 BaseValue。
	 * @param CurrentValues 成功求得的候选 CurrentValue。
	 * @param ModifierInstances 成功重算后的 Ongoing 父实例。
	 * @param bCommitModifierInstances 是否提交 Ongoing 父实例变更。
	 * @param bBroadcastEvents 是否在 Clamp 和范围传播收敛后广播 Attribute 事件。
	 */
	void CommitAttributeModifierTransaction(
		const TMap<FName, float>& BaseValues,
		const TMap<FName, float>& CurrentValues,
		const TArray<FTcsAttributeModifierInstance>& ModifierInstances,
		bool bCommitModifierInstances,
		bool bBroadcastEvents = true);

	/**
	 * 按当前 AttributeModifiers 内容重建 Modifier 运行时缓存。
	 */
	void RebuildModifierRuntimeCaches();

	/**
	 * 基于 ClampStrategy 的声明式依赖，构建“源属性 -> 受其范围约束影响的属性”映射。
	 *
	 * 这里只有在所有策略都声明了完整依赖时才返回 true；
	 * 只要混入一个未知策略，调用方就必须回退到保守的全局传播。
	 *
	 * @param OutDependents 输出的依赖映射，Key 为依赖源属性，Value 为受其影响的属性集合
	 * 返回 false 表示当前至少有一个策略未声明完整依赖，调用方应回退到全局传播路径。
	 */
	bool TryBuildDeclaredRangeConstraintDependents(TMap<FName, TSet<FName>>& OutDependents) const;

	/**
	 * 仅针对给定脏属性集合执行范围约束传播。
	 *
	 * 这个入口只决定传播起点，真正的传播策略仍由
	 * EnforceAttributeRangeConstraintsInternal 统一处理。
	 *
	 * @param DirtyAttributes 本轮已确认发生变化的属性集合
	 * 如果当前策略集合没有声明完整依赖，或局部传播未在上限轮次内收敛，则会自动回退到全局传播。
	 */
	void EnforceAttributeRangeConstraints(const TSet<FName>& DirtyAttributes, bool bBroadcastEvents = true);

	/**
	 * 属性范围约束传播内部实现。
	 * DirtyAttributes 为空时执行保守的全局传播；否则优先尝试局部传播。
	 *
	 * @param DirtyAttributes 可选的脏属性种子；为 nullptr 时直接执行全局 fixpoint
	 */
	void EnforceAttributeRangeConstraintsInternal(const TSet<FName>* DirtyAttributes, bool bBroadcastEvents);

	/**
	 * 对比前后快照，统一补发 Base/Current/Boundary 三类公共事件。
	 *
	 * @param PreviousBaseValues 广播前的 BaseValue 快照
	 * @param PreviousCurrentValues 广播前的 CurrentValue 快照
	 */
	void BroadcastAttributeStateDiffs(
		const TMap<FName, float>& PreviousBaseValues,
		const TMap<FName, float>& PreviousCurrentValues);

	// 属性夹值计算：所有动态范围依赖（ART_Dynamic）仅在本 Component 上解析。
	// 不支持跨 Actor 属性引用。自定义 ClampStrategy 接收的 Context 也绑定到本 Component。
	// 若未来需要跨 Actor 依赖，应扩展 FTcsAttributeClampContextBase 或引入跨 Component Resolver。

	/**
	 * 从 BaseValue 与全部 Ongoing 父实例受控重建 CurrentValue。
	 *
	 * @param bBroadcastEvents 是否在最终稳定态后广播 Attribute 事件。
	 */
	void RecalculateAttributeCurrentValues(bool bBroadcastEvents = true);

	/**
	 * 将指定属性值约束到其定义的范围内。
	 *
	 * @param AttributeName 要执行约束的属性名
	 * @param NewValue 输入时为待约束值，输出时为约束后的结果
	 * @param OutMinValue 可选输出，返回本次解析得到的最小边界
	 * @param OutMaxValue 可选输出，返回本次解析得到的最大边界
	 * @param WorkingValues 可选工作集，用于在传播过程中读取尚未提交到组件的动态范围值
	 */
	virtual void ClampAttributeValueInRange(
		const FName& AttributeName,
		float& NewValue,
		float* OutMinValue = nullptr,
		float* OutMaxValue = nullptr,
		const TMap<FName, float>* WorkingValues = nullptr);

	/**
	 * 执行保守的全局范围约束传播。
	 *
	 * 确保所有属性的 BaseValue 和 CurrentValue 都在其定义范围内，
	 * 并支持多跳依赖场景（例如 HP <= MaxHP，MaxHP 又依赖 Level）。
	 */
	virtual void EnforceAttributeRangeConstraints(bool bBroadcastEvents = true);

#pragma endregion


#pragma region RuntimeStorage

public:
	/** 当前战斗实体持有的全部属性实例。 */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTcsAttributeInstance> Attributes;

	/** 当前战斗实体持有的全部属性修改器实例。 */
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TArray<FTcsAttributeModifierInstance> AttributeModifiers;

	// SourceHandle ID 到 Modifier 实例 ID 的映射 (性能优化 - 稳定索引)
	// Key: SourceHandle.Id, Value: ModifierInstId 列表
	// 注: 使用稳定的 ModifierInstId 而非数组下标，避免删除操作导致的索引漂移问题
	// 不使用 UPROPERTY 的原因:
	//   1. 仅存储值类型 (int32)，不涉及 UObject 指针，无需 GC 追踪
	//   2. 运行时优化数据，可从 AttributeModifiers 重建，无需序列化
	//   3. 本地缓存，无需网络复制 (每个客户端独立维护)
	//   4. 内部实现细节，无需暴露给蓝图或编辑器
	//   5. 生命周期跟随组件，C++ 析构函数自动释放内存
	// Value 使用 TSet<int32>，避免批量按 SourceHandle 移除时在桶内做线性删除。
	TMap<int32, TSet<int32>> SourceHandleIdToModifierInstIds;

	// Modifier 实例 ID 到当前数组下标的映射 (性能优化 - 快速定位)
	// Key: ModifierInstId, Value: AttributeModifiers 数组中的当前索引
	// 注: 此映射在每次数组变更时更新，提供 O(1) 的 ID->Index 查询
	// TODO(Perf): 当前 RemoveAtSwap 路径已是 O(1) 维护；若未来新增 "多元素批量移除" 场景，
	//   避免对每个元素独立 Swap+Map 更新，可改为 "先收集所有待删索引 → 一次性重排 → 整体重建 Index Map"，
	//   将 K 次移除的总成本从 O(K) 次 Map 写入降为一次性 O(N) 扫描（当 K 接近 N 时更优）。
	TMap<int32, int32> ModifierInstIdToIndex;

#pragma endregion
};
