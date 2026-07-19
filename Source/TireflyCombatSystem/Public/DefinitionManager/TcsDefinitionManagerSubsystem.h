// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "GameplayTagContainer.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TcsDefinitionManagerSubsystem.generated.h"



// 前向声明
class UPrimaryDataAsset;
class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;
class UTcsBuffDefinition;
class UTcsSkillDefinition;
class UTcsSkillModifierDefinition;
class UTcsStateComponent;
class UTcsStateDefinition;
class UTcsStateSlotDefinition;



/**
 * 单资产异步加载完成回调委托。
 *
 * @param DefId 加载的 Definition ID。
 * @param bSuccess 加载是否成功。
 * @param Definition 加载完成的 Definition 指针；失败时为 nullptr。
 */
DECLARE_DELEGATE_ThreeParams(FOnTcsDefinitionAsyncLoaded, FName, bool, UPrimaryDataAsset*);

/**
 * 批量异步加载全部完成回调委托。
 *
 * @param RequestedDefIds 本次请求加载的全部 Definition ID。
 * @param LoadedDefinitions 成功加载的 Definition 指针列表。
 */
DECLARE_DELEGATE_TwoParams(FOnTcsDefinitionsBatchLoaded, const TArray<FName>&, const TArray<UPrimaryDataAsset*>&);



/**
 * 异步预加载全部完成后触发的多播委托。
 * 在 bIsRuntimeReady 置为 true 时广播。
 * 若监听者在广播之后才注册，需自行检查 IsRuntimeReady() 决定是否立即执行。
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnTcsDefinitionManagerReady);



/**
 * Definition source cache 条目。
 * 持有资产软引用与对应的 PrimaryAssetId，供同步/异步加载使用。
 */
struct FTcsDefinitionSourceEntry
{
	/** 资产软引用，不持有已加载资产。 */
	TSoftObjectPtr<UPrimaryDataAsset> SoftPtr;

	/** 资产 PrimaryAssetId，供 AssetManager 异步加载使用。 */
	FPrimaryAssetId AssetId;
};



/**
 * 运行时 Definition 管理子系统。
 *
 * 作为 TCS 运行时 Definition source cache、loaded cache、同步/异步加载与类型化查询的统一归口。
 * source cache 持有软引用（TSoftObjectPtr），loaded cache 持有硬引用（TObjectPtr）。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsDefinitionManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// State 模块内部桥接
	friend class UTcsStateComponent;

// GameInstanceSubsystem 生命周期
#pragma region GameInstanceSubsystem

public:
	/** 初始化运行时 Definition source cache，并触发异步预加载。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 清理运行时 Definition cache。 */
	virtual void Deinitialize() override;

#pragma endregion


// Runtime-ready 状态
#pragma region RuntimeState

public:
	/**
	 * 查询当前 DefinitionManager 是否已完成全局异步预加载。
	 *
	 * 此 flag 表示跨全部 Definition 域的预加载批次已完成，不区分具体域。
	 * 仅消费单一 Definition 域（如 State 或 Attribute）的运行时组件
	 * MUST NOT 以此全局就绪条件阻塞自身初始化；
	 * 若需判断特定 Definition 是否可用，应直接调用对应的 Get...Definition() 检查返回值。
	 *
	 * @return 若当前子系统已完成异步预加载并可供运行时查询，则返回 true。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	bool IsRuntimeReady() const { return bIsRuntimeReady; }

	/**
	 * 异步预加载全部完成后的多播委托。在 bIsRuntimeReady 置为 true 时广播。
	 * 广播时机：全部预加载批次完成、或无资产需要预加载时立即广播。
	 * 若监听者在广播后才注册，需自行检查 IsRuntimeReady() 决定是否立即执行。
	 */
	UPROPERTY(BlueprintAssignable, Category = "TireflyCombatSystem|Definition")
	FOnTcsDefinitionManagerReady OnRuntimeReady;

protected:
	/** 当前 DefinitionManager 是否已完成 runtime-ready 初始化。 */
	UPROPERTY(Transient)
	bool bIsRuntimeReady = false;

#pragma endregion


// State-like Definition 查询（Buff / Skill）
#pragma region StateLikeDefinitionQueries

public:
	/**
	 * 获取 Buff 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param BuffDefId Buff 定义 ID。
	 * @return Buff 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsBuffDefinition* GetBuffDefinition(FName BuffDefId) const;

	/**
	 * 通过 GameplayTag 获取 Buff 定义资产。
	 *
	 * 先查 tag 索引；未命中则从 source cache 逐条同步加载并匹配 tag。
	 *
	 * @param BuffTag Buff 定义语义标签。
	 * @return Buff 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsBuffDefinition* GetBuffDefinitionByTag(FGameplayTag BuffTag) const;

	/**
	 * 获取 Skill 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param SkillDefId Skill 定义 ID。
	 * @return Skill 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsSkillDefinition* GetSkillDefinition(FName SkillDefId) const;

protected:
	/**
	 * 为 State 模块提供 State-like Definition 的内部高频查询。
	 *
	 * 先查派生的 StateDefinitions 聚合缓存；未命中时从派生的 StateDefinitionSources
	 * 进行一次 O(1) source 查询并按需加载。该入口不对 Blueprint 或其他模块公开。
	 *
	 * @param StateDefId State 模块内部使用的状态定义 ID。
	 * @return State-like 定义资产；未找到、类型冲突或加载失败时返回 nullptr。
	 */
	const UTcsStateDefinition* GetStateDefinition(FName StateDefId) const;

#pragma endregion


// StateSlot Definition 查询
#pragma region StateSlotDefinitionQueries

public:
	/**
	 * 获取 StateSlot 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param StateSlotDefId StateSlot 定义 ID。
	 * @return StateSlot 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsStateSlotDefinition* GetStateSlotDefinition(FName StateSlotDefId) const;

	/**
	 * 通过 GameplayTag 获取 StateSlot 定义资产。
	 *
	 * 先查 tag 索引；未命中则从 source cache 逐条同步加载并匹配 tag。
	 *
	 * @param StateSlotTag StateSlot 语义标签。
	 * @return StateSlot 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsStateSlotDefinition* GetStateSlotDefinitionByTag(FGameplayTag StateSlotTag) const;

	/** @return source cache 中全部 StateSlot 定义 ID。 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	TArray<FName> GetAllStateSlotDefIds() const;

#pragma endregion


// Attribute Definition 查询
#pragma region AttributeDefinitionQueries

public:
	/**
	 * 获取 Attribute 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param AttributeDefId Attribute 定义 ID。
	 * @return Attribute 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsAttributeDefinition* GetAttributeDefinition(FName AttributeDefId) const;

	/**
	 * 通过 GameplayTag 获取 Attribute 定义资产。
	 *
	 * 先查 tag 索引；未命中则从 source cache 逐条同步加载并匹配 tag。
	 *
	 * @param AttributeTag Attribute 语义标签。
	 * @return Attribute 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsAttributeDefinition* GetAttributeDefinitionByTag(FGameplayTag AttributeTag) const;

	/**
	 * 通过 GameplayTag 解析对应的 AttributeDefId。
	 *
	 * @param AttributeTag 要解析的 Attribute 语义标签。
	 * @return 对应的 AttributeDefId；未找到时返回 NAME_None。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	FName ResolveAttributeDefIdByTag(FGameplayTag AttributeTag) const;

	/**
	 * 获取 AttributeModifier 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param AttributeModifierDefId AttributeModifier 定义 ID。
	 * @return AttributeModifier 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsAttributeModifierDefinition* GetAttributeModifierDefinition(FName AttributeModifierDefId) const;

	/** @return source cache 中全部 Attribute 定义 ID。 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	TArray<FName> GetAllAttributeDefIds() const;

	/** @return source cache 中全部 AttributeModifier 定义 ID。 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	TArray<FName> GetAllAttributeModifierDefIds() const;

#pragma endregion


// SkillModifier Definition 查询
#pragma region SkillModifierDefinitionQueries

public:
	/**
	 * 获取 SkillModifier 定义资产。
	 *
	 * 先查 loaded cache；未命中则从 source cache 按需同步加载并写入 loaded cache。
	 *
	 * @param SkillModifierDefId SkillModifier 定义 ID。
	 * @return SkillModifier 定义资产指针；未找到时返回 nullptr。
	 */
	UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Definition")
	const UTcsSkillModifierDefinition* GetSkillModifierDefinition(FName SkillModifierDefId) const;

#pragma endregion


// 异步预加载
#pragma region AsyncPreload

public:
	/**
	 * 请求异步预加载 DeveloperSettings 配置中需要预加载的 DefinitionAsset。
	 *
	 * 按 DeveloperSettings 中各类型的加载策略：
	 * - PreloadAll：异步加载该类型全部资产。
	 * - PreloadSelected：异步加载白名单中的资产。
	 * - OnDemand：跳过预加载。
	 *
	 * 全部异步批次完成后设置 bIsRuntimeReady。
	 */
	void RequestAsyncPreload();

protected:
	/**
	 * 异步预加载完成回调。
	 *
	 * @param LoadedAssetIds 本次批次加载的 PrimaryAssetId 列表。
	 */
	void OnAsyncPreloadComplete(TArray<FPrimaryAssetId> LoadedAssetIds);

	/** 当前尚未完成的异步预加载批次数。 */
	int32 PendingPreloadBatchCount = 0;

#pragma endregion


// 单资产按需异步加载
#pragma region AsyncLoad

public:
	/**
	 * 异步加载单个 BuffDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param BuffDefId 要加载的 Buff 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadBuffDefinitionAsync(FName BuffDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 异步加载单个 SkillDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param SkillDefId 要加载的 Skill 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadSkillDefinitionAsync(FName SkillDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 异步加载单个 StateSlotDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param StateSlotDefId 要加载的 StateSlot 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadStateSlotDefinitionAsync(FName StateSlotDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 异步加载单个 AttributeDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param AttributeDefId 要加载的 Attribute 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadAttributeDefinitionAsync(FName AttributeDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 异步加载单个 AttributeModifierDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param AttributeModifierDefId 要加载的 AttributeModifier 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadAttributeModifierDefinitionAsync(FName AttributeModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 异步加载单个 SkillModifierDefinition。
	 *
	 * 资产已在 loaded cache 中时立即同步回调，不走异步路径。
	 * 同一 DefId 的并发请求只发起一次实际加载，完成后统一广播。
	 *
	 * @param SkillModifierDefId 要加载的 SkillModifier 定义 ID。
	 * @param Callback 加载完成回调。
	 */
	void LoadSkillModifierDefinitionAsync(FName SkillModifierDefId, const FOnTcsDefinitionAsyncLoaded& Callback);

protected:
	/**
	 * 单资产异步加载的内部实现。
	 *
	 * 先查 loaded cache；命中则同步回调。再查 PendingAsyncLoads 去重。最后发起异步加载。
	 *
	 * @param SourceCache 对应类型的 source cache。
	 * @param DefId 要加载的 Definition ID。
	 * @param EntryName 发起加载的公开入口名，用于统一失败诊断。
	 * @param Callback 加载完成回调。
	 */
	void StartAsyncLoad(
		const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
		FName DefId,
		FName EntryName,
		const FOnTcsDefinitionAsyncLoaded& Callback);

	/**
	 * 单资产异步加载完成后的统一处理。
	 *
	 * @param AssetId 加载完成的 PrimaryAssetId。
	 * @param DefId 发起请求时使用的 Definition ID。
	 * @param EntryName 发起加载的公开入口名，用于统一失败诊断。
	 * @param Callbacks 等待该资产加载的全部回调。
	 */
	void OnAsyncDefinitionLoaded(
		FPrimaryAssetId AssetId,
		FName DefId,
		FName EntryName,
		TArray<FOnTcsDefinitionAsyncLoaded> Callbacks);

	/**
	 * 将异步加载完成的资产写入对应的 loaded cache 与 tag 索引。
	 *
	 * @param AssetId 已完成加载的 PrimaryAssetId，用于选择目标类型缓存。
	 * @param Asset 已完成加载的 DefinitionAsset。
	 * @return 成功写入与 AssetId 匹配的类型化缓存时返回 true；类型不匹配或资产无效时返回 false。
	 */
	bool WriteLoadedAssetToCache(const FPrimaryAssetId& AssetId, UPrimaryDataAsset* Asset);

	/** 按需异步加载的待处理请求（同一 PrimaryAssetId 的并发请求合并）。 */
	TMap<FPrimaryAssetId, TArray<FOnTcsDefinitionAsyncLoaded>> PendingAsyncLoads;

#pragma endregion


// 批量按需异步加载
#pragma region BatchAsyncLoad

public:
	/**
	 * 批量异步加载多个 BuffDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param BuffDefIds 要加载的 Buff 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadBuffDefinitionsBatch(const TArray<FName>& BuffDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

	/**
	 * 批量异步加载多个 SkillDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param SkillDefIds 要加载的 Skill 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadSkillDefinitionsBatch(const TArray<FName>& SkillDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

	/**
	 * 批量异步加载多个 StateSlotDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param StateSlotDefIds 要加载的 StateSlot 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadStateSlotDefinitionsBatch(const TArray<FName>& StateSlotDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

	/**
	 * 批量异步加载多个 AttributeDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param AttributeDefIds 要加载的 Attribute 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadAttributeDefinitionsBatch(const TArray<FName>& AttributeDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

	/**
	 * 批量异步加载多个 AttributeModifierDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param AttributeModifierDefIds 要加载的 AttributeModifier 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadAttributeModifierDefinitionsBatch(const TArray<FName>& AttributeModifierDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

	/**
	 * 批量异步加载多个 SkillModifierDefinition。
	 *
	 * 已在 loaded cache 中的跳过异步加载，全部完成后统一回调。
	 *
	 * @param SkillModifierDefIds 要加载的 SkillModifier 定义 ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void LoadSkillModifierDefinitionsBatch(const TArray<FName>& SkillModifierDefIds, const FOnTcsDefinitionsBatchLoaded& Callback);

protected:
	/**
	 * 批量异步加载的内部实现。
	 *
	 * 对已在 loaded cache 中的立即收集；对其余的逐个走单资产异步路径，全部完成后统一回调。
	 *
	 * @param SourceCache 对应类型的 source cache。
	 * @param EntryName 发起批量加载的公开入口名，用于统一失败诊断。
	 * @param DefIds 要加载的 Definition ID 列表。
	 * @param Callback 批量加载完成回调。
	 */
	void StartBatchAsyncLoad(
		const TMap<FName, FTcsDefinitionSourceEntry>& SourceCache,
		FName EntryName,
		const TArray<FName>& DefIds,
		const FOnTcsDefinitionsBatchLoaded& Callback);

#pragma endregion


// Definition cache 内部辅助函数
#pragma region DefinitionCacheInternal

protected:
	/** 从 AssetManager 重建所有具体 DefAsset source cache（不加载资产）。 */
	void RebuildSourceCache();

	/**
	 * 从 State 模块内部聚合 source cache 按需同步加载 State-like Definition。
	 *
	 * @param StateDefId State 模块内部使用的状态定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 State-like Definition；未注册、类型冲突或加载失败时返回 nullptr。
	 */
	const UTcsStateDefinition* LoadStateDefinitionSync(FName StateDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 BuffDefinition 并写入 typed 与 State 聚合缓存。
	 *
	 * @param BuffDefId 要加载的 Buff 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 BuffDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsBuffDefinition* LoadBuffDefinitionSync(FName BuffDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 SkillDefinition 并写入 typed 与 State 聚合缓存。
	 *
	 * @param SkillDefId 要加载的 Skill 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 SkillDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsSkillDefinition* LoadSkillDefinitionSync(FName SkillDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 StateSlotDefinition 并写入 typed cache。
	 *
	 * @param StateSlotDefId 要加载的 StateSlot 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 StateSlotDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsStateSlotDefinition* LoadStateSlotDefinitionSync(FName StateSlotDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 AttributeDefinition 并写入 typed cache。
	 *
	 * @param AttributeDefId 要加载的 Attribute 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 AttributeDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsAttributeDefinition* LoadAttributeDefinitionSync(FName AttributeDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 AttributeModifierDefinition 并写入 typed cache。
	 *
	 * @param AttributeModifierDefId 要加载的 AttributeModifier 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 AttributeModifierDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsAttributeModifierDefinition* LoadAttributeModifierDefinitionSync(FName AttributeModifierDefId, const TCHAR*& OutFailureCategory) const;

	/**
	 * 从 source cache 按需同步加载 SkillModifierDefinition 并写入 typed cache。
	 *
	 * @param SkillModifierDefId 要加载的 SkillModifier 定义 ID。
	 * @param OutFailureCategory 输出失败类别；成功时保持调用方提供的值。
	 * @return 已加载的 SkillModifierDefinition；未注册、类型不匹配或加载失败时返回 nullptr。
	 */
	const UTcsSkillModifierDefinition* LoadSkillModifierDefinitionSync(FName SkillModifierDefId, const TCHAR*& OutFailureCategory) const;

	/** 重建 BuffTag 与 StateSlotTag 查询索引（仅遍历已 loaded 的资产）。 */
	void RebuildTagIndexes();

	/**
	 * 记录统一格式的 Definition 查询失败诊断。
	 *
	 * @param QueryKey 查询的 DefId 或 tag key。
	 * @param EntryName 发起查询或加载的入口名。
	 * @param FailureCategory 失败类别，例如 NotRegistered、TypeMismatch 或 LoadFailed。
	 */
	void LogDefinitionQueryFailure(FName QueryKey, const TCHAR* EntryName, const TCHAR* FailureCategory) const;

#pragma endregion


// Definition source cache（软引用，Initialize 时从 AssetManager 填充，不加载资产）
#pragma region SourceCache

public:
	/** BuffDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> BuffDefinitionSources;

	/** SkillDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> SkillDefinitionSources;

	/** StateSlotDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> StateSlotDefinitionSources;

	/** AttributeDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> AttributeDefinitionSources;

	/** AttributeModifierDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> AttributeModifierDefinitionSources;

	/** SkillModifierDefinition source cache。 */
	TMap<FName, FTcsDefinitionSourceEntry> SkillModifierDefinitionSources;

protected:
	/**
	 * 从具体 Buff / Skill source cache 派生的 State 模块内部索引。
	 *
	 * 它不自行扫描 AssetManager、不定义独立加载策略，只用于 StateDefId 的 O(1) 查找。
	 */
	TMap<FName, FTcsDefinitionSourceEntry> StateDefinitionSources;

	/** 同时属于 BuffDef 与 SkillDef 的 StateDefId，拒绝进入 State 模块的弱类型查询。 */
	TSet<FName> AmbiguousStateDefinitionIds;

#pragma endregion


// Definition loaded cache（硬引用，持有已加载资产，防止 GC）与 tag 查询索引
#pragma region LoadedCache

public:
	/** BuffDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsBuffDefinition>> BuffDefinitions;

	/** SkillDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsSkillDefinition>> SkillDefinitions;

protected:
	/**
	 * 从具体 Buff / Skill loaded cache 派生的 State 模块内部高频聚合缓存。
	 *
	 * 它不拥有独立加载生命周期，只镜像已经解析成功的具体 State-like Definition。
	 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsStateDefinition>> StateDefinitions;

public:
	/** StateSlotDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsStateSlotDefinition>> StateSlotDefinitions;

	/** AttributeDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsAttributeDefinition>> AttributeDefinitions;

	/** AttributeModifierDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsAttributeModifierDefinition>> AttributeModifierDefinitions;

	/** SkillModifierDefinition loaded cache。 */
	UPROPERTY(Transient)
	mutable TMap<FName, TObjectPtr<UTcsSkillModifierDefinition>> SkillModifierDefinitions;

	/** BuffTag 到 BuffDefId 的运行时查询索引。 */
	mutable TMap<FGameplayTag, FName> BuffTagToDefId;

	/** StateSlotTag 到 StateSlotDefId 的运行时查询索引。 */
	mutable TMap<FGameplayTag, FName> StateSlotTagToDefId;

	/** AttributeTag 到 AttributeDefId 的运行时查询索引。 */
	mutable TMap<FGameplayTag, FName> AttributeTagToDefId;

#pragma endregion
};
