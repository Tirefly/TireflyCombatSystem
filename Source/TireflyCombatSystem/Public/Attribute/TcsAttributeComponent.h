// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TcsAttributeInstance.h"
#include "TcsAttributeChangeEventPayload.h"
#include "TcsAttributeModifier.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeComponent.generated.h"



class UTcsAttributeManagerSubsystem;
class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;



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

	friend class UTcsAttributeManagerSubsystem;


#pragma region ActorComponent

public:
	/** 构造属性组件并初始化默认 Tick 策略。 */
	UTcsAttributeComponent();

protected:
	/** 在 BeginPlay 时预热 AttributeManager 缓存。 */
	virtual void BeginPlay() override;

#pragma endregion


#pragma region ManagerReference

protected:

	/** 缓存的 AttributeManager 指针。 */
	UPROPERTY()
	TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;

	/**
	 * 懒加载获取 AttributeManager。
	 *
	 * BeginPlay 已预热；业务方法中若首访为空，会在此补拉取并 ensureMsgf 诊断。
	 *
	 * @return AttributeManager 指针；失败时返回 nullptr 并触发 ensureMsgf
	 */
	UTcsAttributeManagerSubsystem* ResolveAttributeManager();

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
	UFUNCTION(BlueprintCallable, Category = "Attribute", Meta = (Categories = "AttributeTag"))
	bool HasAttributeByTag(const FGameplayTag& AttributeTag) const;

	/**
	 * 通过 GameplayTag 获取特定属性的当前值。
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param OutValue 输出属性当前值
	 * @return 如果 Tag 能解析且属性存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute", Meta = (Categories = "AttributeTag"))
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
	UFUNCTION(BlueprintCallable, Category = "Attribute", Meta = (Categories = "AttributeTag"))
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
	 * @param InitValue 初始值
	 * @return 是否成功添加
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool AddAttribute(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

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
	 * @param InitValue 初始值
	 * @return 是否成功添加（Tag 有效、在映射中注册、且属性不存在时返回 true）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute", Meta = (Categories = "AttributeTag"))
	bool AddAttributeByTag(const FGameplayTag& AttributeTag, float InitValue = 0.f);

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
	 * 直接设置属性的 Current 值
	 *
	 * @param AttributeName 属性名称
	 * @param NewValue 新的 Current 值
	 * @param bTriggerEvents 是否触发事件（默认 true）
	 * @return 是否成功设置
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool SetAttributeCurrentValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float NewValue,
		bool bTriggerEvents = true);

	/**
	 * 重置属性到定义的初始值
	 *
	 * @param AttributeName 属性名称
	 * @return 是否成功重置
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool ResetAttribute(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName);

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


#pragma region ModifierLifecycle

public:
	/**
	 * 创建属性修改器实例
	 * （Component 已知自身 Owner 作为 Target，移除了原有 Target 参数）
	 *
	 * @param ModifierId 属性修改器 Id
	 * @param Instigator 修改器发起者
	 * @param OutModifierInst 输出创建的修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual bool CreateAttributeModifier(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		AActor* Instigator,
		FTcsAttributeModifierInstance& OutModifierInst);

	/**
	 * 创建属性修改器实例，并设置操作数
	 *
	 * @param ModifierId 属性修改器 Id
	 * @param Instigator 修改器发起者
	 * @param Operands 属性修改器操作数
	 * @param OutModifierInst 输出创建的修改器实例
	 * @return 是否创建成功
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual bool CreateAttributeModifierWithOperands(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeModifierIds"))FName ModifierId,
		AActor* Instigator,
		const TMap<FName, float>& Operands,
		FTcsAttributeModifierInstance& OutModifierInst);

	/**
	 * 应用多个属性修改器实例。
	 *
	 * @param Modifiers 要应用的修改器实例列表
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual void ApplyModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * 使用 SourceHandle 应用属性修改器（非 virtual，调用 CreateAttributeModifier + ApplyModifier）
	 *
	 * @param SourceHandle 来源句柄
	 * @param ModifierIds 要应用的修改器 ID 列表
	 * @param OutModifiers 输出创建的修改器实例列表
	 * @return 是否成功应用
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	bool ApplyModifierWithSourceHandle(
		const FTcsSourceHandle& SourceHandle,
		const TArray<FName>& ModifierIds,
		TArray<FTcsAttributeModifierInstance>& OutModifiers);

	/**
	 * 从当前战斗实体移除多个属性修改器。
	 *
	 * @param Modifiers 要移除的修改器实例列表
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * 按 SourceHandle 移除属性修改器
	 *
	 * @param SourceHandle 来源句柄
	 * @return 是否成功移除
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual bool RemoveModifiersBySourceHandle(const FTcsSourceHandle& SourceHandle);

	/**
	 * 按 SourceHandle 查询属性修改器（非 virtual，纯读取操作）
	 *
	 * @param SourceHandle 来源句柄
	 * @param OutModifiers 输出查询到的修改器实例列表
	 * @return 是否查询到修改器
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	bool GetModifiersBySourceHandle(
		const FTcsSourceHandle& SourceHandle,
		TArray<FTcsAttributeModifierInstance>& OutModifiers) const;

	/**
	 * 处理属性修改器更新后的重算与事件广播逻辑。
	 *
	 * @param Modifiers 已更新的修改器实例列表
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual void HandleModifierUpdated(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

#pragma endregion


#pragma region AttributeCalculation

protected:
	/**
	 * 从当前持久化修改器集合中批量移除指定实例 ID，并在末尾一次性重建运行时缓存。
	 *
	 * @param ModifierInstIdsToRemove 要移除的 ModifierInstId 集合
	 * @param ChangeBatchId 本次变更批次号
	 * @return 是否实际移除了任意修改器
	 */
	bool RemoveStoredModifiersByInstIds(const TSet<int32>& ModifierInstIdsToRemove, int64 ChangeBatchId);

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
	 * 重新计算所有属性的 BaseValue。
	 *
	 * 执行器如果声明了 touched 集，只会为这些属性记录旧值并生成差异事件；
	 * 未声明时会回退到原来的全表快照路径，以保持行为完全一致。
	 *
	 * @param Modifiers 参与本次基础值重算的修改器集合
	 */
	virtual void RecalculateAttributeBaseValues(const TArray<FTcsAttributeModifierInstance>& Modifiers, bool bBroadcastEvents = true);

	/**
	 * 重新计算所有属性的 CurrentValue。
	 *
	 * 该流程与 BaseValue 重算共用 touched-report 机制；如果提供有效的 ChangeBatchId，
	 * 则会把本轮真实变更过的属性作为范围传播种子，尽量缩小后续 Clamp 传播的处理面。
	 *
	 * @param ChangeBatchId 本次增量变更批次号，< 0 表示保守的全量重算路径
	 */
	virtual void RecalculateAttributeCurrentValues(int64 ChangeBatchId = -1, bool bBroadcastEvents = true);

	// 属性修改器合并
	virtual void MergeAttributeModifiers(
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers);

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
