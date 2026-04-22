// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "TcsAttribute.h"
#include "TcsAttributeChangeEventPayload.h"
#include "TcsAttributeModifier.h"
#include "TcsSourceHandle.h"
#include "TcsAttributeComponent.generated.h"



class UTcsAttributeManagerSubsystem;
class UTcsAttributeDefinitionAsset;
class UTcsAttributeModifierDefinitionAsset;



// 属性值改变事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsAttributeChangeDelegate,
	const TArray<FTcsAttributeChangeEventPayload>&, Payloads);

// 属性修改器添加事件委托声明
// (修改器实例)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsOnAttributeModifierAddedSignature,
	const FTcsAttributeModifierInstance&, ModifierInstance);

// 属性修改器移除事件委托声明
// (修改器实例)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsOnAttributeModifierRemovedSignature,
	const FTcsAttributeModifierInstance&, ModifierInstance);

// 属性修改器更新事件委托声明
// (修改器实例)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsOnAttributeModifierUpdatedSignature,
	const FTcsAttributeModifierInstance&, ModifierInstance);

// 属性达到边界值事件委托声明
// (属性名称, 边界类型: true=最大值, false=最小值, 旧值, 新值, 边界值)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FTcsOnAttributeReachedBoundarySignature,
	FName, AttributeName,
	bool, bIsMaxBoundary,
	float, OldValue,
	float, NewValue,
	float, BoundaryValue);



// 属性组件，保存战斗实体的属性相关数据
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Attribute Cmp"))
class TIREFLYCOMBATSYSTEM_API UTcsAttributeComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UTcsAttributeManagerSubsystem;

#pragma region ActorComponent

public:
	UTcsAttributeComponent();

protected:
	virtual void BeginPlay() override;

	// 缓存的 AttributeManager 指针（迁移期供 Phase C 下沉的业务方法直接访问）
	UPROPERTY()
	TObjectPtr<UTcsAttributeManagerSubsystem> AttrMgr;

	/**
	 * 懒加载获取 AttributeManager
	 * BeginPlay 已预热；业务方法中若首访为空，会在此补拉取并 ensureMsgf 诊断
	 *
	 * @return AttributeManager 指针；失败时返回 nullptr 并触发 ensureMsgf
	 */
	UTcsAttributeManagerSubsystem* ResolveAttributeManager();

#pragma endregion


#pragma region Attribute

public:
	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	// 获取特定属性的当前值
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	bool GetAttributeBaseValue(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float& OutValue) const;

	// 获取所有属性的当前值
	TMap<FName, float> GetAttributeValues() const;
	// 获取所有属性的基础值
	TMap<FName, float> GetAttributeBaseValues() const;

	// 广播属性当前值改变事件
	void BroadcastAttributeValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;
	// 广播属性基础值改变事件
	void BroadcastAttributeBaseValueChangeEvent(const TArray<FTcsAttributeChangeEventPayload>& Payloads) const;
	// 广播属性修改器添加事件
	void BroadcastAttributeModifierAddedEvent(const FTcsAttributeModifierInstance& ModifierInstance) const;
	// 广播属性修改器移除事件
	void BroadcastAttributeModifierRemovedEvent(const FTcsAttributeModifierInstance& ModifierInstance) const;
	// 广播属性修改器更新事件
	void BroadcastAttributeModifierUpdatedEvent(const FTcsAttributeModifierInstance& ModifierInstance) const;
	// 广播属性达到边界值事件
	void BroadcastAttributeReachedBoundaryEvent(
		FName AttributeName,
		bool bIsMaxBoundary,
		float OldValue,
		float NewValue,
		float BoundaryValue) const;

public:
	// 战斗实体的所有属性实例
	UPROPERTY(BlueprintReadOnly, Category = "Attribute")
	TMap<FName, FTcsAttributeInstance> Attributes;

	// 战斗实体的所有属性修改器实例
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
	// TODO(Perf): Value 当前为 TArray<int32>，`Remove(InstId)` 为 O(bucket)。
	//   批量移除同一 SourceHandle 下 K 个 Modifier 时整体退化为 O(K^2)。
	//   优化方向:
	//     1) 改为 TSet<int32>，单次删除 O(1)，桶内元素量通常较小，内存开销可接受；
	//     2) 保持 TArray 但在 RemoveModifiersBySourceHandle 路径直接整桶丢弃，跳过逐个 Remove；
	//     3) 批量移除 API 引入 "延迟紧凑化"：先标记后重建，避免 O(K^2)。
	TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds;

	// Modifier 实例 ID 到当前数组下标的映射 (性能优化 - 快速定位)
	// Key: ModifierInstId, Value: AttributeModifiers 数组中的当前索引
	// 注: 此映射在每次数组变更时更新，提供 O(1) 的 ID->Index 查询
	// TODO(Perf): 当前 RemoveAtSwap 路径已是 O(1) 维护；若未来新增 "多元素批量移除" 场景，
	//   避免对每个元素独立 Swap+Map 更新，可改为 "先收集所有待删索引 → 一次性重排 → 整体重建 Index Map"，
	//   将 K 次移除的总成本从 O(K) 次 Map 写入降为一次性 O(N) 扫描（当 K 接近 N 时更优）。
	TMap<int32, int32> ModifierInstIdToIndex;

	// 属性当前值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeValueChanged;

	// 属性基础值改变事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsAttributeChangeDelegate OnAttributeBaseValueChanged;

	// 属性修改器添加事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeModifierAddedSignature OnAttributeModifierAdded;

	// 属性修改器移除事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeModifierRemovedSignature OnAttributeModifierRemoved;

	// 属性修改器更新事件
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeModifierUpdatedSignature OnAttributeModifierUpdated;

	/**
	 * 属性达到边界值事件
	 * 当属性值达到最大值或最小值时广播
	 * bIsMaxBoundary: true表示达到最大值，false表示达到最小值（如HP归零）
	 */
	UPROPERTY(BlueprintAssignable, Category = "Attribute|Events")
	FTcsOnAttributeReachedBoundarySignature OnAttributeReachedBoundary;

#pragma endregion


#pragma region AttributeInstance

public:
	// 给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	virtual bool AddAttribute(
		UPARAM(Meta = (GetParamOptions = "TcsGenericLibrary.GetAttributeNames"))FName AttributeName,
		float InitValue = 0.f);

	// 批量给战斗实体添加属性
	UFUNCTION(BlueprintCallable, Category = "Attribute")
	void AddAttributes(const TArray<FName>& AttributeNames);

	/**
	 * 通过 GameplayTag 给战斗实体添加属性（非 virtual，通过 Tag 解析后调用 AddAttribute）
	 *
	 * @param AttributeTag 属性的 GameplayTag 标识
	 * @param InitValue 初始值
	 * @return 是否成功添加（Tag 有效、在映射中注册、且属性不存在时返回 true）
	 */
	UFUNCTION(BlueprintCallable, Category = "Attribute", Meta = (Categories = "TCS.Attribute"))
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


#pragma region AttributeModifier

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

	// TODO(Perf): 批量移除同一 SourceHandle 下 K 个 Modifier 时，桶维护退化为 O(K^2)。
	//   优化方向见 RemoveModifiersBySourceHandle 实现注释，以及 SourceHandleIdToModifierInstIds 成员注释。
	// 应用多个属性修改器
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

	// TODO(Perf): 批量移除同一 SourceHandle 下 K 个 Modifier 时，桶维护退化为 O(K^2)。
	//   优化方向见 RemoveModifiersBySourceHandle 实现注释，以及 SourceHandleIdToModifierInstIds 成员注释。
	// 从战斗实体移除多个属性修改器
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual void RemoveModifier(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

	/**
	 * 按 SourceHandle 移除属性修改器
	 * TODO(Perf): 当前实现逐个委托给 RemoveModifier，桶内 Remove 导致 O(K^2)。
	 *   优化优先级：1) 将 SourceHandleIdToModifierInstIds 桶类型改为 TSet<int32>；
	 *              2) 或提取 RemoveModifierInternal(无桶维护)，在末尾一次性整桶丢弃。
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

	// 处理属性修改器更新时的逻辑
	UFUNCTION(BlueprintCallable, Category = "Attribute|Modifier")
	virtual void HandleModifierUpdated(UPARAM(ref) TArray<FTcsAttributeModifierInstance>& Modifiers);

#pragma endregion


#pragma region AttributeCalculation

protected:
	// 属性夹值计算：所有动态范围依赖（ART_Dynamic）仅在本 Component 上解析。
	// 不支持跨 Actor 属性引用。自定义 ClampStrategy 接收的 Context 也绑定到本 Component。
	// 若未来需要跨 Actor 依赖，应扩展 FTcsAttributeClampContextBase 或引入跨 Component Resolver。

	// 重新计算属性基础值
	virtual void RecalculateAttributeBaseValues(const TArray<FTcsAttributeModifierInstance>& Modifiers);

	// 重新计算属性当前值
	virtual void RecalculateAttributeCurrentValues(int64 ChangeBatchId = -1);

	// 属性修改器合并
	virtual void MergeAttributeModifiers(
		const TArray<FTcsAttributeModifierInstance>& Modifiers,
		TArray<FTcsAttributeModifierInstance>& MergedModifiers);

	// 将属性的给定值限制在指定范围内
	// WorkingValues: 可选的工作集，用于从工作集读取动态范围属性值（两段式 Clamp）
	virtual void ClampAttributeValueInRange(
		const FName& AttributeName,
		float& NewValue,
		float* OutMinValue = nullptr,
		float* OutMaxValue = nullptr,
		const TMap<FName, float>* WorkingValues = nullptr);

	// 执行属性范围约束传播
	// 确保所有属性的 BaseValue 和 CurrentValue 都在其定义的范围内
	// 支持多跳依赖（如 HP <= MaxHP，MaxHP 依赖 Level）
	virtual void EnforceAttributeRangeConstraints();

#pragma endregion
};
