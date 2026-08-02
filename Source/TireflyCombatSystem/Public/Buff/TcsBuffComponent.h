// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Buff/BuffMerger/TcsBuffMergeRuntime.h"
#include "Buff/TcsBuffChangeEventPayload.h"
#include "Buff/TcsBuffDurationTracker.h"
#include "Buff/TcsBuffTypes.h"
#include "TcsSourceHandle.h"
#include "TcsBuffComponent.generated.h"



class UTcsBuffInstance;
class UTcsRuntimeBootstrapSubsystem;
class UTcsStateComponent;
class UTcsStateDefinition;
class UTcsStateInstance;
struct FTcsStateSlot;



#pragma region BuffDelegate

// Buff 运行时变化批量事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsBuffRuntimeDeltaBatchDelegate,
	const TArray<FTcsBuffRuntimeDeltaEventPayload>&, Payloads);

// Buff 移除批量事件委托声明
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FTcsBuffRemovedBatchDelegate,
	const TArray<FTcsBuffRemovedEventPayload>&, Payloads);

#pragma endregion



/**
 * Buff 宿主组件。
 *
	 * 用途：承接挂在 Actor 上的 Buff 运行时聚合语义，例如持续时间跟踪、叠层变化和调试叠加层。
 */
UCLASS(ClassGroup = (TireflyCombatSystem), Meta = (BlueprintSpawnableComponent, DisplayName = "Tirefly Buff Cmp"))
class TIREFLYCOMBATSYSTEM_API UTcsBuffComponent : public UActorComponent
{
	GENERATED_BODY()

	friend class UTcsBuffInstance;


#pragma region ActorComponent

public:
	UTcsBuffComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	/** 在组件初始化时接入 runtime bootstrap。 */
	virtual void InitializeComponent() override;

	/** 在组件反初始化时退出 runtime bootstrap。 */
	virtual void UninitializeComponent() override;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

#pragma endregion


#pragma region RuntimeBootstrap

public:
	/**
	 * 查询当前 BuffComponent 是否已完成 runtime prepare。
	 *
	 * @return 若当前组件已完成 runtime prepare，则返回 true
	 */
	UFUNCTION(BlueprintPure, Category = "Buff|Runtime")
	bool IsRuntimePrepared() const { return bRuntimePrepared; }

	/**
	 * 查询当前 BuffComponent 是否已满足完整 runtime-ready 条件。
	 *
	 * @return 若当前组件已满足完整 runtime-ready 条件，则返回 true
	 */
	UFUNCTION(BlueprintPure, Category = "Buff|Runtime")
	bool IsRuntimeReady() const;

	/**
	 * 显式执行 Buff runtime prepare。
	 *
	 * @return 若 prepare 成功，则返回 true
	 */
	bool PrepareBuffRuntime();

protected:
	/** 缓存的 RuntimeBootstrapSubsystem 指针。 */
	UPROPERTY(Transient)
	TObjectPtr<UTcsRuntimeBootstrapSubsystem> RuntimeBootstrapSubsystem;

	/** 当前 BuffComponent 是否已完成 runtime prepare。 */
	UPROPERTY(Transient)
	bool bRuntimePrepared = false;

	/**
	 * 懒加载获取 RuntimeBootstrapSubsystem。
	 *
	 * @return RuntimeBootstrapSubsystem 指针；失败时返回 nullptr
	 */
	UTcsRuntimeBootstrapSubsystem* ResolveRuntimeBootstrapSubsystem();

#pragma endregion


#pragma region Owner

public:
	/**
	 * 获取或创建指定 Actor 上的 Buff 宿主组件。
	 *
	 * @param OwnerActor 目标 Actor
	 * @return 对应的 Buff 宿主组件；失败时返回 nullptr
	 */
	static UTcsBuffComponent* GetOrCreateForActor(AActor* OwnerActor);

	/** @return 当前 Buff 组件所属的共享 State 宿主组件。 */
	UFUNCTION(BlueprintPure, Category = "Buff")
	UTcsStateComponent* GetOwnerStateComponent() const;

	/**
	 * 初始化所属的 State 宿主组件。
	 *
	 * @param InStateComponent 负责共享状态主流程的宿主组件
	 */
	void InitializeOwnerStateComponent(UTcsStateComponent* InStateComponent);

#pragma endregion


#pragma region DurationAndQuery

public:
	/**
	 * 获取 Buff 剩余持续时间。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @return 剩余持续时间；无限时长返回 -1.0f
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	float GetBuffRemainingDuration(const UTcsBuffInstance* BuffInstance) const;

	/**
	 * 按当前 Buff 总时长刷新剩余持续时间。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	void RefreshBuffRemainingDuration(UTcsBuffInstance* BuffInstance);

	/**
	 * 直接写入 Buff 剩余持续时间。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param InDurationRemaining 新的剩余持续时间
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Duration")
	void SetBuffRemainingDuration(UTcsBuffInstance* BuffInstance, float InDurationRemaining);

	/**
	 * 获取指定槽位中的 Buff 实例集合。
	 *
	 * @param SlotTag 目标槽位标签
	 * @param OutBuffs 输出 Buff 实例集合
	 * @return 如果找到 Buff 实例则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Query")
	bool GetBuffsInSlot(FGameplayTag SlotTag, TArray<UTcsBuffInstance*>& OutBuffs) const;

	/**
	 * 获取指定 DefId 的 Buff 实例集合。
	 *
	 * @param BuffDefId Buff 定义 ID
	 * @param OutBuffs 输出 Buff 实例集合
	 * @return 如果找到 Buff 实例则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Query")
	bool GetBuffsByDefId(FName BuffDefId, TArray<UTcsBuffInstance*>& OutBuffs) const;

	/**
	 * 获取当前宿主的全部激活 Buff。
	 *
	 * @param OutBuffs 输出激活 Buff 集合
	 * @return 如果找到激活 Buff 则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Query")
	bool GetAllActiveBuffs(TArray<UTcsBuffInstance*>& OutBuffs) const;

	/**
	 * 检查是否存在指定 DefId 的 Buff。
	 *
	 * @param BuffDefId Buff 定义 ID
	 * @return 如果存在则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Query")
	bool HasBuffWithDefId(FName BuffDefId) const;

	/**
	 * 检查指定槽位中是否存在激活 Buff。
	 *
	 * @param SlotTag 目标槽位标签
	 * @return 如果存在激活 Buff 则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Query")
	bool HasActiveBuffInSlot(FGameplayTag SlotTag) const;

#pragma endregion


#pragma region Lifecycle

public:
	/**
	 * 尝试在当前宿主身上应用指定 Buff 定义。
	 *
	 * @param BuffDefId 要应用的 Buff 定义 ID
	 * @param Instigator Buff 发起者
	 * @param BuffLevel Buff 等级
	 * @param ParentSourceHandle 父级来源句柄
	 * @return 如果成功进入共享 State 主流程则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	bool ApplyBuff(
		FName BuffDefId,
		AActor* Instigator,
		int32 BuffLevel = 1,
		const FTcsSourceHandle& ParentSourceHandle = FTcsSourceHandle());

	/**
	 * 尝试在当前宿主身上应用带父来源的 Buff 定义。
	 *
	 * @param BuffDefId 要应用的 Buff 定义 ID
	 * @param Instigator Buff 发起者
	 * @param BuffLevel Buff 等级
	 * @param ParentSourceHandle 父级来源句柄
	 * @param ParentSourceDefId 直接父来源的 Definition Id
	 * @return 如果成功进入共享 State 主流程则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	bool ApplyBuffFromSourceHandle(
		FName BuffDefId,
		AActor* Instigator,
		int32 BuffLevel,
		const FTcsSourceHandle& ParentSourceHandle,
		FPrimaryAssetId ParentSourceDefId);

	/**
	 * 请求从当前宿主身上移除指定 Buff 实例。
	 *
	 * @param BuffInstance 要移除的 Buff 实例
	 * @param RemovalReason 移除原因；默认使用通用 Removed
	 * @return 如果成功收敛到共享移除流程则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	bool RemoveBuff(UTcsBuffInstance* BuffInstance, FName RemovalReason = NAME_None);

	/**
	 * 注册一个刚进入 State 主流程的 Buff 实例。
	 *
	 * @param StateInstance 刚成功进入槽位的状态实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	void RegisterBuffInstance(UTcsStateInstance* StateInstance);

	/**
	 * 注销一个已经退出 State 主流程的 Buff 实例。
	 *
	 * @param StateInstance 即将从运行时移除的状态实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	void UnregisterBuffInstance(UTcsStateInstance* StateInstance);

	/**
	 * 请求移除一个 Buff 实例。
	 *
	 * @param StateInstance 目标 Buff 实例
	 * @param RemovalReason 移除原因
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	void RemoveBuffInstance(UTcsStateInstance* StateInstance, FName RemovalReason);

	/**
	 * 请求将一个 Buff 实例收敛到自然过期流程。
	 *
	 * @param StateInstance 目标 Buff 实例
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Lifecycle")
	void ExpireBuffInstance(UTcsStateInstance* StateInstance);

	/**
	 * Tick 当前组件管理的 Buff 生命周期。
	 *
	 * @param DeltaTime 本帧时间增量
	 */
	void TickBuffLifecycles(float DeltaTime);

	/**
	 * 处理 Buff 叠层数写回后的统一增量反应。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldStackCount 写回前的叠层数
	 * @param NewStackCount 写回后的叠层数
	 */
	void HandleBuffStackCountChangedInternal(
		UTcsBuffInstance* BuffInstance,
		int32 OldStackCount,
		int32 NewStackCount);

	/**
	 * 处理 Buff 持续时间耗尽后的统一生命周期反应。
	 *
	 * @param BuffInstance 持续时间已耗尽的 Buff 实例
	 */
	void HandleBuffDurationExpired(UTcsBuffInstance* BuffInstance);

	/**
	 * 刷新持续时间跟踪器中的失效实例。
	 *
	 * 用途：在共享宿主完成槽位清理后，同步剔除已失效的 Buff 跟踪项。
	 */
	void RefreshTrackedBuffs();

#pragma endregion


#pragma region MergeRuntime

public:
	/**
	 * 对指定槽位中的 Buff 执行合并编排。
	 *
	 * @param StateSlot 目标槽位运行时数据
	 */
	void ProcessBuffMerging(FTcsStateSlot* StateSlot);

#pragma endregion


#pragma region Debug

public:
	/**
	 * 生成供 StateComponent 调试快照使用的 Buff 叠加信息。
	 *
	 * @param StateInstance 目标状态实例
	 * @param OutStackCount 输出叠层数
	 * @param OutDurationText 输出可视化剩余时长文本
	 */
	void GetDebugStateOverlay(const UTcsStateInstance* StateInstance, int32& OutStackCount, FString& OutDurationText) const;

	/**
	 * 获取指定槽位的 Buff merge 运行时调试信息。
	 *
	 * @param SlotTag 目标槽位标签
	 * @param OutDebugLines 输出的调试文本行
	 * @return 如果成功找到槽位运行时数据则返回 true，否则返回 false
	 */
	UFUNCTION(BlueprintCallable, Category = "Buff|Debug")
	bool GetBuffMergeDebugLines(FGameplayTag SlotTag, TArray<FString>& OutDebugLines) const;

#pragma endregion


#pragma region EventSurface

public:
	/**
	 * 广播 Buff 运行时变化批量事件。
	 *
	 * @param Payloads 本次批量收敛后的运行时变化载荷
	 */
	void BroadcastBuffRuntimeDeltaBatchEvent(const TArray<FTcsBuffRuntimeDeltaEventPayload>& Payloads) const;

	/**
	 * 广播 Buff 移除批量事件。
	 *
	 * @param Payloads 本次批量收敛后的移除载荷
	 */
	void BroadcastBuffRemovedBatchEvent(const TArray<FTcsBuffRemovedEventPayload>& Payloads) const;

	/** Buff 运行时变化批量事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Buff|Events")
	FTcsBuffRuntimeDeltaBatchDelegate OnBuffRuntimeDelta;

	/** Buff 移除批量事件。 */
	UPROPERTY(BlueprintAssignable, Category = "Buff|Events")
	FTcsBuffRemovedBatchDelegate OnBuffRemoved;

	/**
	 * 记录 Buff 叠层变化事件。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldStackCount 旧叠层数
	 * @param NewStackCount 新叠层数
	 */
	void NotifyBuffStackChanged(UTcsBuffInstance* BuffInstance, int32 OldStackCount, int32 NewStackCount);

	/**
	 * 记录 Buff 最大叠层数变化事件。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldMaxStackCount 旧最大叠层数
	 * @param NewMaxStackCount 新最大叠层数
	 */
	void NotifyBuffMaxStackCountChanged(UTcsBuffInstance* BuffInstance, int32 OldMaxStackCount, int32 NewMaxStackCount);

	/**
	 * 记录 Buff 周期变化事件。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldPeriod 旧周期
	 * @param NewPeriod 新周期
	 */
	void NotifyBuffPeriodChanged(UTcsBuffInstance* BuffInstance, float OldPeriod, float NewPeriod);

	/**
	 * 记录 Buff 持续时间刷新事件。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param NewDuration 新的剩余持续时间
	 */
	void NotifyBuffDurationRefreshed(UTcsBuffInstance* BuffInstance, float NewDuration);

	/**
	 * 记录 Buff 移除事件。
	 *
	 * @param BuffInstance 被移除的 Buff 实例
	 * @param RemovalReason Buff 移除原因
	 */
	void NotifyBuffRemoved(UTcsBuffInstance* BuffInstance, FName RemovalReason);

#pragma endregion


#pragma region OwnerStateBridge

private:
	/**
	 * 绑定共享 State 宿主的生命周期事件。
	 *
	 * @param InStateComponent 目标 State 宿主组件
	 */
	void BindOwnerStateEvents(UTcsStateComponent* InStateComponent);

	/**
	 * 解除共享 State 宿主的生命周期事件绑定。
	 *
	 * @param InStateComponent 目标 State 宿主组件
	 */
	void UnbindOwnerStateEvents(UTcsStateComponent* InStateComponent);

	/**
	 * 响应共享 State 宿主的状态应用成功事件。
	 *
	 * @param TargetActor 被应用状态的目标 Actor
	 * @param StateDefId 状态定义 ID
	 * @param CreatedStateInstance 成功留存在共享主链中的状态实例
	 * @param TargetSlot 目标槽位
	 * @param AppliedStage 应用后的实际阶段
	 */
	UFUNCTION()
	void HandleOwnerStateApplySuccess(
		AActor* TargetActor,
		FName StateDefId,
		UTcsStateInstance* CreatedStateInstance,
		FGameplayTag TargetSlot,
		ETcsStateStage AppliedStage);

	/**
	 * 响应共享 State 宿主的状态移除事件。
	 *
	 * @param StateComponent 广播事件的 State 宿主组件
	 * @param StateInstance 被移除的状态实例
	 * @param RemovalReason 移除原因
	 */
	UFUNCTION()
	void HandleOwnerStateRemoved(
		UTcsStateComponent* StateComponent,
		UTcsStateInstance* StateInstance,
		FName RemovalReason);

	/**
	 * 响应共享 State 宿主的状态阶段变化事件。
	 *
	 * @param StateComponent 广播事件的 State 宿主组件
	 * @param StateInstance 发生阶段变化的状态实例
	 * @param PreviousStage 变化前阶段
	 * @param NewStage 变化后阶段
	 */
	UFUNCTION()
	void HandleOwnerStateStageChanged(
		UTcsStateComponent* StateComponent,
		UTcsStateInstance* StateInstance,
		ETcsStateStage PreviousStage,
		ETcsStateStage NewStage);

	/**
	 * 响应共享 State 宿主的槽位 Gate 状态变化事件。
	 *
	 * @param StateComponent 广播事件的 State 宿主组件
	 * @param SlotTag 发生 Gate 变化的槽位标签
	 * @param bIsOpen 变化后的 Gate 状态
	 */
	UFUNCTION()
	void HandleOwnerSlotGateStateChanged(
		UTcsStateComponent* StateComponent,
		FGameplayTag SlotTag,
		bool bIsOpen);

	/**
	 * 响应共享 State 宿主的槽位激活刷新扩展点。
	 *
	 * @param StateComponent 广播事件的共享 State 宿主
	 * @param SlotTag 正在刷新的槽位标签
	 * @param StateSlot 对应的槽位运行时数据
	 */
	void HandleOwnerStateSlotActivation(
		UTcsStateComponent* StateComponent,
		FGameplayTag SlotTag,
		FTcsStateSlot* StateSlot);

	/**
	 * 响应共享 State 宿主的调试叠加层扩展点。
	 *
	 * @param StateComponent 广播事件的共享 State 宿主
	 * @param StateInstance 需要生成叠加信息的状态实例
	 * @param OutStackCount 输出叠层数
	 * @param OutDurationText 输出时长文本
	 */
	void HandleOwnerStateDebugOverlay(
		UTcsStateComponent* StateComponent,
		const UTcsStateInstance* StateInstance,
		int32& OutStackCount,
		FString& OutDurationText);

#pragma endregion


#pragma region InternalHelpers

private:
	/**
	 * 把共享状态实例安全解析为 Buff 实例。
	 *
	 * @param StateInstance 目标共享状态实例
	 * @return 解析成功时返回 Buff 实例，否则返回 nullptr
	 */
	UTcsBuffInstance* ResolveBuffInstance(UTcsStateInstance* StateInstance) const;

	/**
	 * 把共享状态实例安全解析为只读 Buff 实例。
	 *
	 * @param StateInstance 目标共享状态实例
	 * @return 解析成功时返回只读 Buff 实例，否则返回 nullptr
	 */
	const UTcsBuffInstance* ResolveBuffInstance(const UTcsStateInstance* StateInstance) const;

	/**
	 * 对同一 DefId 的 Buff 组执行单次合并。
	 *
	 * @param BuffsToMerge 参与合并的 Buff 实例集合
	 * @param OutMergedBuffs 输出保留下来的 Buff 实例集合
	 * @param OutMergedOutBuffs 输出在合并中被淘汰的 Buff 实例集合
	 */
	void MergeBuffStateGroup(
		TArray<UTcsBuffInstance*>& BuffsToMerge,
		TArray<UTcsBuffInstance*>& OutMergedBuffs,
		TArray<UTcsBuffInstance*>& OutMergedOutBuffs);

	/**
	 * 根据当前槽位状态重建 Buff merge group runtime。
	 *
	 * @param StateSlot 需要重建运行时分组的目标槽位
	 */
	void RebuildBuffMergeGroups(FTcsStateSlot* StateSlot);

	/**
	 * 判断当前 dirty reason 是否要求重新执行该 group 的 merger。
	 *
	 * @param DirtyReasons 当前 group 累积的 dirty 原因
	 * @param DependencyFlags 当前 merger 声明的依赖集合
	 * @return 需要重新处理时返回 true
	 */
	bool ShouldProcessBuffMergeGroup(
		ETcsBuffMergeDirtyReason DirtyReasons,
		ETcsBuffMergeDependencyFlags DependencyFlags) const;

	/**
	 * 处理本轮 Buff 合并中显式标记为淘汰者的实例移除。
	 *
	 * @param StateSlot 当前正在刷新的槽位运行时数据
	 * @param MergedOutBuffs 本轮合并明确淘汰的 Buff 状态集合
	 */
	void RemoveMergedOutBuffs(
		FTcsStateSlot* StateSlot,
		const TArray<UTcsBuffInstance*>& MergedOutBuffs);

	/**
	 * 把指定 Buff 对应的 merge group 标记为脏，并请求所属槽位刷新。
	 *
	 * @param BuffInstance 发生运行时变化的 Buff 实例
	 * @param DirtyReason 本次并入的 dirty 原因
	 */
	void MarkBuffMergeGroupDirty(UTcsBuffInstance* BuffInstance, ETcsBuffMergeDirtyReason DirtyReason);

	/** @return 当前组件缓存或解析到的共享 State 宿主组件。 */
	UTcsStateComponent* ResolveOwnerStateComponent() const;

	/**
	 * 进入 Buff 公共事件批处理作用域。
	 *
	 * 用途：把同一条运行时链路里的多个公开结果暂存起来，等最外层作用域结束后再统一广播。
	 */
	void BeginPublicEventBatch();

	/**
	 * 退出 Buff 公共事件批处理作用域。
	 *
	 * 用途：只有最外层批次结束时才会真正触发挂起事件的统一排空。
	 */
	void EndPublicEventBatch();

	/**
	 * 立即广播当前挂起的 Buff 公共事件。
	 *
	 * 用途：把已经折叠完成的 RuntimeDelta / Removed 结果提交给外部监听者。
	 */
	void FlushPendingPublicEvents();

	/**
	 * 查找指定 Buff 的挂起运行时变化载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @return 如果当前批次里已存在对应载荷则返回其指针，否则返回 nullptr
	 */
	FTcsBuffRuntimeDeltaEventPayload* FindPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance);

	/**
	 * 查找指定 Buff 的挂起移除载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @return 如果当前批次里已存在对应移除载荷则返回其指针，否则返回 nullptr
	 */
	FTcsBuffRemovedEventPayload* FindPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance);

	/**
	 * 检查指定 Buff 是否已经进入本批次的移除结果集合。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @return 已经挂起移除事件则返回 true，否则返回 false
	 */
	bool HasPendingBuffRemovedEvent(UTcsBuffInstance* BuffInstance) const;

	/**
	 * 丢弃指定 Buff 当前批次里已记录的运行时变化载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 */
	void DiscardPendingBuffRuntimeDeltaEvent(UTcsBuffInstance* BuffInstance);

	/**
	 * 把 Buff 叠层变化折叠进当前批次的 RuntimeDelta 载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldStackCount 本批次首次观察到的旧叠层数
	 * @param NewStackCount 本次更新后的最新叠层数
	 */
	void QueueBuffStackChangedEvent(UTcsBuffInstance* BuffInstance, int32 OldStackCount, int32 NewStackCount);

	/**
	 * 把 Buff 最大叠层变化折叠进当前批次的 RuntimeDelta 载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldMaxStackCount 本批次首次观察到的旧最大叠层数
	 * @param NewMaxStackCount 本次更新后的最新最大叠层数
	 */
	void QueueBuffMaxStackCountChangedEvent(UTcsBuffInstance* BuffInstance, int32 OldMaxStackCount, int32 NewMaxStackCount);

	/**
	 * 把 Buff 周期变化折叠进当前批次的 RuntimeDelta 载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param OldPeriod 本批次首次观察到的旧周期
	 * @param NewPeriod 本次更新后的最新周期
	 */
	void QueueBuffPeriodChangedEvent(UTcsBuffInstance* BuffInstance, float OldPeriod, float NewPeriod);

	/**
	 * 把 Buff 持续时间刷新结果写入当前批次的 RuntimeDelta 载荷。
	 *
	 * @param BuffInstance 目标 Buff 实例
	 * @param NewDuration 本批次结束时应对外可见的最终剩余时长
	 */
	void QueueBuffDurationRefreshedEvent(UTcsBuffInstance* BuffInstance, float NewDuration);

	/**
	 * 把 Buff 移除结果写入当前批次的 Removed 载荷。
	 *
	 * @param BuffInstance 被移除的 Buff 实例
	 * @param RemovalReason 本次移除原因
	 */
	void QueueBuffRemovedEvent(UTcsBuffInstance* BuffInstance, FName RemovalReason);

#pragma endregion


#pragma region RuntimeState

private:
	// 共享 State 宿主组件缓存；Buff 侧的移除、定义查询等流程仍需回到它的统一主链。
	UPROPERTY(Transient)
	TWeakObjectPtr<UTcsStateComponent> OwnerStateComponent;

	// 公共 Buff 事件的本地批处理深度。
	int32 PublicEventBatchDepth = 0;

	// 待统一广播的 Buff 运行时变化事件。
	TArray<FTcsBuffRuntimeDeltaEventPayload> PendingBuffRuntimeDeltaEvents;

	// 待统一广播的 Buff 移除事件。
	TArray<FTcsBuffRemovedEventPayload> PendingBuffRemovedEvents;

	// 槽位激活刷新扩展点绑定句柄。
	FDelegateHandle OwnerStateSlotActivationHandle;

	// 调试叠加层扩展点绑定句柄。
	FDelegateHandle OwnerStateDebugOverlayHandle;

	// 按实例维护“需要进行时长 Tick 的有限时长 Buff”注册表。
	UPROPERTY()
	FTcsBuffDurationTracker DurationTracker;

#pragma endregion
};
