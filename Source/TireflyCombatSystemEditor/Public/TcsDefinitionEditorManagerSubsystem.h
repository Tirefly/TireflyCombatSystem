// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "EditorSubsystem.h"

#include "TcsDefinitionEditorManagerSubsystem.generated.h"

class UDataTable;
class UEditorAssetSubsystem;
class UPackage;
class UPrimaryDataAsset;
class UTcsDeveloperSettings;
class FObjectPostSaveContext;
struct FAssetData;
struct FTcsDataTableSyncConfig;
struct FInstancedStruct;



#pragma region SyncRequest

/**
 * 延迟同步请求类型。
 */
enum class ETcsPendingSyncRequestType : uint8
{
	// 同步指定 DataTable 到受管 DefAsset 目录
	SyncDataTable,

	// 同步指定 DefAsset 回写到目标 DataTable
	SyncDefAsset,

	// 处理已删除 DefAsset 对应的 DataTable 行移除
	RemoveDefAsset,
};

/**
 * 延迟同步请求数据。
 */
struct FTcsPendingSyncRequest
{
	// 请求类型
	ETcsPendingSyncRequestType Type = ETcsPendingSyncRequestType::SyncDataTable;

	// 参与同步的对象路径
	FString ObjectPath;

	// 请求入队顺序，用于稳定排序
	uint64 Sequence = 0;
};

/**
 * DefAsset 与目标 DataTable 的缓存绑定信息。
 */
struct FTcsCachedDefAssetBinding
{
	// 当前 DefAsset 缓存的 DefId
	FName DefId = NAME_None;

	// 当前 DefAsset 绑定的目标 DataTable
	FSoftObjectPath TargetDataTable;
};

/**
 * DefAsset 删除前的快照信息。
 */
struct FTcsRemovedDefAssetSnapshot
{
	// 被删除资产的对象路径
	FString ObjectPath;

	// 被删除资产对应的 DefId
	FName DefId = NAME_None;

	// 被删除资产原先绑定的目标 DataTable
	FSoftObjectPath TargetDataTable;
};

#pragma endregion



/**
 * 编辑器期 Definition 管理中枢。
 *
 * 统一负责 DataTable ↔ DefAsset 桥接协调、防递归回写、受管 Def 缓存/索引/更新队列、
 * 编辑器期资产事件监听，以及在变更落地后驱动 DefinitionRegistry 刷新。
 */
UCLASS()
class TIREFLYCOMBATSYSTEMEDITOR_API UTcsDefinitionEditorManagerSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()


#pragma region EditorSubsystem

public:
	/**
	 * 判断当前环境是否需要创建该编辑器子系统。
	 *
	 * @param Outer 子系统宿主对象
	 * @return 返回 true 表示仅在编辑器环境创建该子系统
	 */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/**
	 * 初始化编辑器期 Definition 管理中枢。
	 *
	 * @param Collection 当前子系统集合
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/**
	 * 反注册编辑器回调并清理管理中枢缓存状态。
	 */
	virtual void Deinitialize() override;

#pragma endregion


#pragma region CallbackRegistration

private:
	/**
	 * 注册资产保存、删除、重命名和内存删除等编辑器回调。
	 */
	void RegisterEditorCallbacks();

	/**
	 * 注销此前注册的所有编辑器回调。
	 */
	void UnregisterEditorCallbacks();

#pragma endregion


#pragma region RequestQueue

	/**
	 * 处理一轮延迟同步请求。
	 *
	 * @param DeltaTime 当前 Ticker 传入的时间增量
	 * @return 返回 false，表示当前 Ticker 执行一次后即移除
	 */
	bool HandleDeferredSync(float DeltaTime);

	/**
	 * 向延迟队列压入一条同步请求。
	 *
	 * @param Type 请求类型
	 * @param ObjectPath 参与同步的对象路径
	 */
	void QueueSyncRequest(ETcsPendingSyncRequestType Type, const FString& ObjectPath);

	/**
	 * 为指定 DataTable 入队一条同步请求。
	 *
	 * @param DataTableObjectPath 目标 DataTable 的对象路径
	 */
	void QueueManagedDataTableSync(const FString& DataTableObjectPath);

	/**
	 * 为指定 DefAsset 入队一条同步请求。
	 *
	 * @param DefAssetObjectPath 目标 DefAsset 的对象路径
	 */
	void QueueManagedDefAssetSync(const FString& DefAssetObjectPath);

	/**
	 * 为已删除 DefAsset 入队一条移除请求。
	 *
	 * @param DefAssetObjectPath 已删除 DefAsset 的对象路径
	 */
	void QueueManagedDefAssetRemoval(const FString& DefAssetObjectPath);

#pragma endregion


#pragma region EditorCallbacks

	/**
	 * 处理包保存事件，并为包内相关 DataTable 或 DefAsset 入队同步请求。
	 *
	 * @param PackageFileName 已保存包的文件名
	 * @param Package 已保存的包对象
	 * @param ObjectSaveContext 本次保存的对象上下文
	 */
	void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext);

	/**
	 * 处理资产被移除后的同步逻辑。
	 *
	 * @param AssetData 被移除资产的注册表数据
	 */
	void OnAssetRemoved(const FAssetData& AssetData);

	/**
	 * 处理资产重命名后的同步逻辑。
	 *
	 * @param AssetData 重命名后资产的注册表数据
	 * @param OldObjectPath 重命名前的对象路径
	 */
	void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

	/**
	 * 处理内存中资产被删除后的同步逻辑。
	 *
	 * @param AssetObject 被删除的内存资产对象
	 */
	void OnInMemoryAssetDeleted(UObject* AssetObject);

#pragma endregion


#pragma region SyncProcessing

	/**
	 * 执行指定 DataTable 的同步请求。
	 *
	 * @param DataTableObjectPath 需要处理的 DataTable 对象路径
	 * @return 返回 true 表示本次处理产生了实际内容变更
	 */
	bool ProcessDataTableSync(const FString& DataTableObjectPath);

	/**
	 * 执行指定 DefAsset 的同步请求。
	 *
	 * @param DefAssetObjectPath 需要处理的 DefAsset 对象路径
	 * @return 返回 true 表示本次处理产生了实际内容变更
	 */
	bool ProcessDefAssetSync(const FString& DefAssetObjectPath);

	/**
	 * 执行已删除 DefAsset 的行清理请求。
	 *
	 * @param DefAssetObjectPath 已删除 DefAsset 的对象路径
	 * @return 返回 true 表示成功从目标 DataTable 移除了对应行
	 */
	bool ProcessDefAssetRemoval(const FString& DefAssetObjectPath);

	/**
	 * 按 DataTable 内容创建、更新或删除受管 DefAsset。
	 *
	 * @param Config 当前生效的同步配置
	 * @param DataTable 作为来源的 DataTable 实例
	 * @return 返回 true 表示至少有一个 DefAsset 发生了实际变更
	 */
	bool SyncDataTableToManagedDefAssets(const FTcsDataTableSyncConfig& Config, UDataTable& DataTable);

	/**
	 * 按 DefAsset 内容回写对应的 DataTable 行。
	 *
	 * @param Config 当前生效的同步配置
	 * @param DefAsset 作为来源的 DefAsset 实例
	 * @return 返回 true 表示目标 DataTable 发生了实际变更
	 */
	bool SyncManagedDefAssetToDataTable(const FTcsDataTableSyncConfig& Config, UPrimaryDataAsset& DefAsset);

	/**
	 * 从目标 DataTable 中移除已删除 DefAsset 对应的行。
	 *
	 * @param Snapshot 删除前缓存的 DefAsset 快照信息
	 * @return 返回 true 表示目标 DataTable 成功移除了对应行，并已被标记为脏
	 */
	bool RemoveDataTableRowForDeletedDefAsset(const FTcsRemovedDefAssetSnapshot& Snapshot);

#pragma endregion


#pragma region AssetIO

	/**
	 * 加载配置中声明的受管 DataTable。
	 *
	 * @param Config 当前生效的同步配置
	 * @param OutDataTable 输出加载成功的 DataTable 指针
	 * @return 返回 true 表示目标 DataTable 成功加载
	 */
	bool LoadManagedDataTable(const FTcsDataTableSyncConfig& Config, UDataTable*& OutDataTable) const;

	/**
	 * 按对象路径加载受管 DefAsset。
	 *
	 * @param DefAssetObjectPath 需要加载的 DefAsset 对象路径
	 * @param OutDefAsset 输出加载成功的 DefAsset 指针
	 * @return 返回 true 表示目标 DefAsset 成功加载
	 */
	bool LoadManagedDefAsset(const FString& DefAssetObjectPath, UPrimaryDataAsset*& OutDefAsset) const;

	/**
	 * 从 DataTable 指定行中提取 InstancedStruct 行数据。
	 *
	 * @param DataTable 作为来源的 DataTable
	 * @param RowName 目标行名
	 * @param OutRowData 输出提取到的行数据
	 * @return 返回 true 表示成功提取到有效的行数据
	 */
	bool ExtractRowDataFromDataTable(const UDataTable& DataTable, FName RowName, FInstancedStruct& OutRowData) const;

	/**
	 * 将指定行数据写入或覆盖到 DataTable 中。
	 *
	 * @param DataTable 需要写入的 DataTable
	 * @param RowName 目标行名
	 * @param RowData 需要写入的行数据
	 * @return 返回 true 表示行数据成功写入到目标 DataTable
	 */
	bool UpsertDataTableRow(UDataTable& DataTable, FName RowName, const FInstancedStruct& RowData) const;

	/**
	 * 将已加载资产标记为脏，交由用户后续决定何时保存。
	 *
	 * @param AssetObject 需要标记为脏的资产对象
	 * @return 返回 true 表示成功将资产标记为脏
	 */
	bool MarkLoadedAssetDirty(UObject* AssetObject) const;

	/**
	 * 立即保存已加载资产。
	 *
	 * @param AssetObject 需要立即保存的资产对象
	 * @return 返回 true 表示资产保存成功
	 */
	bool SaveLoadedAsset(UObject* AssetObject) const;

	/**
	 * 删除已加载的资产对象。
	 *
	 * @param AssetObject 需要删除的资产对象
	 * @return 返回 true 表示资产删除成功
	 */
	bool DeleteLoadedAsset(UObject* AssetObject);

	/**
	 * 确保受管 DefAsset 目录已存在。
	 *
	 * @param ManagedDirectoryPath 需要确保存在的受管目录路径
	 * @return 返回 true 表示目录已存在或已成功创建
	 */
	bool EnsureManagedDirectoryExists(const FString& ManagedDirectoryPath) const;

	/**
	 * 重新扫描受管 DefAsset 并重建路径到绑定信息的缓存。
	 */
	void RebuildManagedDefAssetBindings();

	/**
	 * 在同步产生实际变更后请求刷新定义注册表。
	 */
	void RefreshDefinitionRegistry() const;

#pragma endregion


#pragma region InternalHelpers

	/**
	 * 获取当前生效的 TCS 开发者设置对象。
	 *
	 * @return 返回默认的开发者设置对象；获取失败时返回 nullptr
	 */
	const UTcsDeveloperSettings* GetDeveloperSettings() const;

	/**
	 * 获取编辑器资产子系统。
	 *
	 * @return 返回编辑器资产子系统；非编辑器环境下返回 nullptr
	 */
	UEditorAssetSubsystem* GetEditorAssetSubsystem() const;

	/**
	 * 根据 DataTable 路径查找匹配的同步配置。
	 *
	 * @param DataTableObjectPath 目标 DataTable 的对象路径
	 * @return 返回匹配的同步配置；未找到时返回 nullptr
	 */
	const FTcsDataTableSyncConfig* FindSyncConfigByDataTablePath(const FString& DataTableObjectPath) const;

	/**
	 * 根据 DefAsset 路径查找匹配的同步配置。
	 *
	 * @param DefAssetObjectPath 目标 DefAsset 的对象路径
	 * @return 返回匹配的同步配置；未找到时返回 nullptr
	 */
	const FTcsDataTableSyncConfig* FindSyncConfigByDefAssetPath(const FString& DefAssetObjectPath) const;

	/**
	 * 收集指定配置目录下的全部受管 DefAsset 资产数据。
	 *
	 * @param Config 当前生效的同步配置
	 * @param OutAssetData 输出收集到的资产数据列表
	 * @return 返回 true 表示资产列表收集成功
	 */
	bool CollectManagedDefAssets(const FTcsDataTableSyncConfig& Config, TArray<FAssetData>& OutAssetData) const;

	/**
	 * 缓存 DefAsset 当前绑定的 DefId 和目标 DataTable。
	 *
	 * @param DefAssetObjectPath DefAsset 的对象路径
	 * @param DefId 当前资产对应的 DefId
	 * @param Config 当前生效的同步配置
	 */
	void CacheDefAssetBinding(const FString& DefAssetObjectPath, FName DefId, const FTcsDataTableSyncConfig& Config);

	/**
	 * 清除指定 DefAsset 的缓存绑定信息。
	 *
	 * @param DefAssetObjectPath 需要移除缓存的 DefAsset 对象路径
	 */
	void RemoveCachedDefAssetBinding(const FString& DefAssetObjectPath);

	/**
	 * 为待删除 DefAsset 构建删除快照。
	 *
	 * @param DefAssetObjectPath 目标 DefAsset 的对象路径
	 * @param OutSnapshot 输出构建完成的删除快照
	 * @return 返回 true 表示成功得到有效快照
	 */
	bool TryBuildRemovedSnapshot(const FString& DefAssetObjectPath, FTcsRemovedDefAssetSnapshot& OutSnapshot) const;

	/**
	 * 记录一段时间内应忽略的 DefAsset 删除事件。
	 *
	 * @param DefAssetObjectPath 需要抑制删除回调的 DefAsset 对象路径
	 */
	void TrackSuppressedDefAssetRemoval(const FString& DefAssetObjectPath);

	/**
	 * 消费并检测指定 DefAsset 的删除抑制状态。
	 *
	 * @param DefAssetObjectPath 需要检测的 DefAsset 对象路径
	 * @return 返回 true 表示当前删除事件应被抑制
	 */
	bool ConsumeSuppressedDefAssetRemoval(const FString& DefAssetObjectPath);

#pragma endregion


#pragma region RuntimeState

	// 是否已经注册编辑器回调
	bool bHasRegisteredCallbacks = false;

	// 当前是否处于同步应用阶段
	bool bIsApplyingSync = false;

	// 当前是否正在消费延迟同步请求队列
	bool bIsProcessingRequests = false;

	// 下一条延迟同步请求的顺序号
	uint64 NextRequestSequence = 1;

	// 延迟同步 Ticker 句柄
	FTSTicker::FDelegateHandle DeferredSyncHandle;

	// 资产移除事件句柄
	FDelegateHandle AssetRemovedHandle;

	// 资产重命名事件句柄
	FDelegateHandle AssetRenamedHandle;

	// 内存资产删除事件句柄
	FDelegateHandle InMemoryAssetDeletedHandle;

	// 包保存事件句柄
	FDelegateHandle PackageSavedHandle;

	// 待处理的延迟同步请求队列
	TArray<FTcsPendingSyncRequest> PendingSyncRequests;

	// DefAsset 对象路径到绑定信息的缓存映射
	TMap<FString, FTcsCachedDefAssetBinding> CachedDefAssetBindings;

	// 等待处理的 DefAsset 删除快照映射
	TMap<FString, FTcsRemovedDefAssetSnapshot> PendingRemovedSnapshots;

	// DefAsset 删除抑制过期时间映射
	TMap<FString, double> SuppressedDefAssetRemovalExpirations;

#pragma endregion
};
