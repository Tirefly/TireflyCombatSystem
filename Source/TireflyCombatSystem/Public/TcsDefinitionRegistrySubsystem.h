// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/EngineSubsystem.h"
#include "TcsDefinitionRegistrySubsystem.generated.h"

class UTcsAttributeDefinition;
class UTcsAttributeModifierDefinition;
class UTcsBuffDefinition;
class UTcsSkillDefinition;
class UTcsSkillModifierDefinition;
class UTcsStateSlotDefinition;
class IAssetRegistry;
class FObjectPostSaveContext;
class UObject;
class UPackage;
struct FTcsSaveAllCommandHookState;
struct FToolMenuOwner;
struct FAssetData;
struct FPrimaryAssetTypeInfo;
struct FPropertyChangedEvent;



DECLARE_MULTICAST_DELEGATE_OneParam(FTcsDefinitionRegistryRefreshed, const UTcsDefinitionRegistrySubsystem*);



/**
 * 编辑器期 Definition 权威快照子系统。
 *
 * 监听 AssetRegistry 资产增删改、资产重命名、包保存、AssetManagerSettings 变更等编辑器事件，
 * 维护一份按 DefId 索引的 TSoftObjectPtr 快照，并在刷新后广播通知。
 * 同时负责 AssetManagerSettings 覆盖勘误：检测 TCS 各具体 DefAsset 类型是否已正确配置扫描路径，
 * 并在 Save All / ContentBrowser 保存时重复提示未修复的勘误。
 *
 * 注意：本子系统是编辑器期快照持有者，不承担运行时 authoritative cache 职责。
 * 运行时 Definition 查询由 UTcsDefinitionManagerSubsystem 承担。
 */
UCLASS()
class TIREFLYCOMBATSYSTEM_API UTcsDefinitionRegistrySubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

// EngineSubsystem 生命周期
#pragma region EngineSubsystem

public:
	/** 注册编辑器回调并执行首次快照重建。 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 反注册编辑器回调并清理状态。 */
	virtual void Deinitialize() override;

#pragma endregion


// 快照查询
#pragma region SnapshotQueries

public:
	/** @return AttributeDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>>& GetAttributeDefinitions() const
	{
		return AttributeDefinitions;
	}

	/** @return AttributeModifierDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>>& GetAttributeModifierDefinitions() const
	{
		return AttributeModifierDefinitions;
	}

	/** @return BuffDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsBuffDefinition>>& GetBuffDefinitions() const
	{
		return BuffDefinitions;
	}

	/** @return SkillDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsSkillDefinition>>& GetSkillDefinitions() const
	{
		return SkillDefinitions;
	}

	/** @return SkillModifierDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsSkillModifierDefinition>>& GetSkillModifierDefinitions() const
	{
		return SkillModifierDefinitions;
	}

	/** @return StateSlotDefinition 快照。 */
	const TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>>& GetStateSlotDefinitions() const
	{
		return StateSlotDefinitions;
	}

#pragma endregion


// 刷新状态与广播
#pragma region RefreshState

public:
	/** @return 快照刷新广播委托。 */
	FTcsDefinitionRegistryRefreshed& OnDefinitionsRefreshed()
	{
		return DefinitionsRefreshed;
	}

	/** @return 是否已完成首次刷新。 */
	bool HasCompletedInitialRefresh() const
	{
		return bHasCompletedInitialRefresh;
	}

	/** @return 当前刷新版本号。 */
	int32 GetRefreshRevision() const
	{
		return RefreshRevision;
	}

#if WITH_EDITOR
	/** 请求异步刷新快照（合并到下一帧 Tick 执行）。 */
	void RequestRefresh();

	/** 立即同步刷新快照。 */
	void RefreshDefinitionsNow();
#endif

#pragma endregion


// 编辑器期快照重建与扫描
#pragma region EditorSnapshot

#if WITH_EDITOR
private:
	/** 注册编辑器期资产与设置变更回调。 */
	void RegisterEditorCallbacks();

	/** 反注册编辑器期回调。 */
	void UnregisterEditorCallbacks();

	/** 延迟刷新 Tick 回调。 */
	bool HandleDeferredRefresh(float DeltaTime);

	/** 从 AssetRegistry 重建全部 DefAsset 快照。 */
	void RebuildSnapshot();

	/** 按 PrimaryAssetType 扫描并分发到对应类型的 Scan 函数。 */
	void ScanPrimaryAssetType(const FPrimaryAssetTypeInfo& TypeInfo, IAssetRegistry& AssetRegistry);

	/** 扫描 AttributeDefinition 并写入快照。 */
	void ScanAttributeDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 扫描 AttributeModifierDefinition 并写入快照。 */
	void ScanAttributeModifierDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 扫描 BuffDefinition 并写入快照。 */
	void ScanBuffDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 扫描 SkillDefinition 并写入快照。 */
	void ScanSkillDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 扫描 SkillModifierDefinition 并写入快照。 */
	void ScanSkillModifierDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 扫描 StateSlotDefinition 并写入快照。 */
	void ScanStateSlotDefinitions(const TArray<FAssetData>& AssetDataList);

	/** 清除已排队的刷新请求。 */
	void ClearQueuedRefresh();
#endif

#pragma endregion


// AssetManagerSettings 覆盖勘误
#pragma region CoverageValidation

#if WITH_EDITOR
private:
	/** 重建当前 AssetManagerSettings 覆盖勘误结果。 */
	void RefreshAssetManagerCoverageIssues(IAssetRegistry& AssetRegistry);

	/**
	 * 输出当前尚未修复的 AssetManagerSettings 勘误。
	 *
	 * @param bTriggeredBySave 是否由保存操作触发。
	 * @param PackageFileName 触发保存的包文件名（可选）。
	 */
	void ReportAssetManagerCoverageIssues(bool bTriggeredBySave, const FString& PackageFileName = FString()) const;

	/**
	 * 判断指定 DefAsset 类型是否被开发者显式忽略。
	 *
	 * @param DefinitionClass 待检查的 DefAsset 类型。
	 * @return 若在 DeveloperSettings 的忽略列表中则返回 true。
	 */
	bool IsDefinitionAssetTypeIgnored(const UClass* DefinitionClass) const;

	/**
	 * 判断资产是否属于 TCS 追踪的 DefAsset 类型。
	 *
	 * @param AssetData 待检查的资产数据。
	 * @return 若属于追踪类型则返回 true。
	 */
	bool IsTrackedDefinitionClass(const FAssetData& AssetData) const;

	/**
	 * 判断对象是否属于 TCS 追踪的 DefAsset 类型。
	 *
	 * @param AssetObject 待检查的对象。
	 * @return 若属于追踪类型则返回 true。
	 */
	bool IsTrackedDefinitionObject(const UObject* AssetObject) const;
#endif

#pragma endregion


// 编辑器保存钩子
#pragma region EditorSaveHooks

#if WITH_EDITOR
private:
	/** 注册 ContentBrowser 保存按钮钩子。 */
	void RegisterContentBrowserSaveButtonHook();

	/** 反注册 ContentBrowser 保存按钮钩子。 */
	void UnregisterContentBrowserSaveButtonHook();

	/** 注册主框架 Save All 命令钩子。 */
	void RegisterSaveAllCommandHook();

	/** 反注册主框架 Save All 命令钩子。 */
	void UnregisterSaveAllCommandHook();

	/** ContentBrowser 保存按钮被点击时的处理。 */
	void HandleContentBrowserSaveButton();

	/** Save All 命令被触发时的处理。 */
	void HandleSaveAllCommand();

	/**
	 * 延迟勘误通知 Tick 回调。
	 *
	 * @param DeltaTime 自上一帧以来的时间。
	 * @return 是否需要继续 Tick。
	 */
	bool HandleDeferredCoverageIssueNotification(float DeltaTime);

	/**
	 * 在保存完成后排队勘误通知。
	 *
	 * @param PackageFileName 触发保存的包文件名。
	 */
	void QueueCoverageIssueReportAfterSave(const FString& PackageFileName);
#endif

#pragma endregion


// 编辑器期资产事件回调
#pragma region EditorAssetCallbacks

#if WITH_EDITOR
private:
	/**
	 * 资产被添加时的回调。
	 *
	 * @param AssetData 新增资产数据。
	 */
	void OnAssetAdded(const FAssetData& AssetData);

	/**
	 * 资产被更新时的回调。
	 *
	 * @param AssetData 更新后的资产数据。
	 */
	void OnAssetUpdated(const FAssetData& AssetData);

	/**
	 * 资产被移除时的回调。
	 *
	 * @param AssetData 被移除的资产数据。
	 */
	void OnAssetRemoved(const FAssetData& AssetData);

	/**
	 * 资产被重命名时的回调。
	 *
	 * @param AssetData 重命名后的资产数据。
	 * @param OldObjectPath 重命名前的对象路径。
	 */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	/**
	 * 内存中资产被创建时的回调。
	 *
	 * @param AssetObject 新创建的内存资产对象。
	 */
	void OnInMemoryAssetCreated(UObject* AssetObject);

	/**
	 * 内存中资产被删除时的回调。
	 *
	 * @param AssetObject 被删除的内存资产对象。
	 */
	void OnInMemoryAssetDeleted(UObject* AssetObject);

	/**
	 * AssetManagerSettings 发生变更时的回调。
	 *
	 * @param SettingsObject 发生变更的设置对象。
	 * @param PropertyChangedEvent 属性变更事件。
	 */
	void OnAssetManagerSettingsChanged(UObject* SettingsObject, FPropertyChangedEvent& PropertyChangedEvent);

	/**
	 * 包保存完成后的回调，用于重复输出未修复勘误。
	 *
	 * @param PackageFileName 保存的包文件名。
	 * @param Package 保存的包对象。
	 * @param ObjectSaveContext 保存上下文。
	 */
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext);
#endif

#pragma endregion


// 编辑器期状态与句柄
#pragma region EditorState

#if WITH_EDITOR
private:
	/** 是否已注册编辑器回调。 */
	bool bHasRegisteredEditorCallbacks = false;

	/** 是否已完成首次刷新。 */
	bool bHasCompletedInitialRefresh = false;

	/** 是否有刷新请求排队中。 */
	bool bIsRefreshQueued = false;

	/** 是否正在执行刷新。 */
	bool bIsRefreshing = false;

	/** 刷新期间是否又收到新的刷新请求。 */
	bool bRefreshRequestedWhileRefreshing = false;

	/** 刷新后是否需要报告覆盖勘误。 */
	bool bShouldReportCoverageIssuesAfterRefresh = false;

	/** 是否有待处理的勘误通知。 */
	bool bHasPendingCoverageIssueNotification = false;

	/** 是否正在执行 Save All 命令。 */
	bool bIsExecutingSaveAllCommand = false;

	/** 待处理勘误通知是否由 Save All 触发。 */
	bool bPendingCoverageIssueTriggeredBySaveAll = false;

	/** 快照刷新版本号。 */
	int32 RefreshRevision = 0;

	/** 延迟刷新 Tick 句柄。 */
	FTSTicker::FDelegateHandle DeferredRefreshHandle;

	/** 延迟勘误通知 Tick 句柄。 */
	FTSTicker::FDelegateHandle DeferredCoverageIssueNotificationHandle;

	/** ContentBrowser 保存按钮启动回调句柄。 */
	FDelegateHandle ContentBrowserSaveButtonStartupCallbackHandle;

	/** 资产添加回调句柄。 */
	FDelegateHandle AssetAddedHandle;

	/** 资产更新回调句柄。 */
	FDelegateHandle AssetUpdatedHandle;

	/** 资产移除回调句柄。 */
	FDelegateHandle AssetRemovedHandle;

	/** 资产重命名回调句柄。 */
	FDelegateHandle AssetRenamedHandle;

	/** 内存资产创建回调句柄。 */
	FDelegateHandle InMemoryAssetCreatedHandle;

	/** 内存资产删除回调句柄。 */
	FDelegateHandle InMemoryAssetDeletedHandle;

	/** AssetManagerSettings 变更回调句柄。 */
	FDelegateHandle AssetManagerSettingsChangedHandle;

	/** 包保存回调句柄。 */
	FDelegateHandle PackageSavedHandle;

	/** 待处理的勘误保存计数。 */
	int32 PendingCoverageIssueSaveCount = 0;

	/** 待处理勘误的首个包文件名。 */
	FString PendingCoverageIssueFirstPackageFileName;

	/** Save All 命令钩子状态。 */
	TSharedPtr<FTcsSaveAllCommandHookState> SaveAllCommandHookState;

	/** 当前未忽略的 AssetManagerSettings 勘误缓存。 */
	TArray<FString> AssetManagerCoverageIssues;
#else
	/** 非编辑器模式下始终视为已完成首次刷新。 */
	bool bHasCompletedInitialRefresh = true;

	/** 快照刷新版本号。 */
	int32 RefreshRevision = 0;
#endif

#pragma endregion


// DefAsset 快照与广播
#pragma region SnapshotData

private:
	/** AttributeDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsAttributeDefinition>> AttributeDefinitions;

	/** AttributeModifierDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinition>> AttributeModifierDefinitions;

	/** BuffDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsBuffDefinition>> BuffDefinitions;

	/** SkillDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsSkillDefinition>> SkillDefinitions;

	/** SkillModifierDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsSkillModifierDefinition>> SkillModifierDefinitions;

	/** StateSlotDefinition 快照。 */
	TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinition>> StateSlotDefinitions;

	/** 快照刷新广播委托。 */
	FTcsDefinitionRegistryRefreshed DefinitionsRefreshed;

#pragma endregion
};
