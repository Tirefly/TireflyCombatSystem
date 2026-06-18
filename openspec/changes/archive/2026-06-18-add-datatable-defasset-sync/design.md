# Design: DataTable ↔ DefAsset 双向同步

## 1. DeveloperSettings 配置

```cpp
// TcsDeveloperSettings.h

#pragma region DataTableSync

/** DataTable 同步总开关。 */
UPROPERTY(EditAnywhere, Config, Category = "DataTable Sync")
bool bEnableDataTableAutoSync = false;

/** 每种 DefAsset 类型的同步配置。 */
UPROPERTY(EditAnywhere, Config, Category = "DataTable Sync",
    Meta = (EditCondition = "bEnableDataTableAutoSync"))
TArray<FTcsDataTableSyncConfig> DataTableSyncConfigs;

#pragma endregion
```

### FTcsDataTableSyncConfig

```cpp
USTRUCT()
struct FTcsDataTableSyncConfig
{
    GENERATED_BODY()

    /** DefAsset 类型。 */
    UPROPERTY(EditAnywhere)
    TSubclassOf<UPrimaryDataAsset> DefAssetClass;

    /** 该条配置受管的 DefAsset 文件夹。文件夹中的多个 DefAsset 严格对应这一张表。 */
    UPROPERTY(EditAnywhere, Meta = (ContentDir))
    FDirectoryPath ManagedDefAssetDirectory;

    /** 与该文件夹显式绑定的目标 DataTable。优先使用软引用，按需加载。 */
    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UDataTable> TargetDataTable;

    /** 是否同步 DefAsset → DataTable（默认为 true，保存 DefAsset 时回写）。 */
    UPROPERTY(EditAnywhere)
    bool bSyncDefAssetToDataTable = true;

    /** 是否允许 DataTable 保存时删除孤立 DefAsset。 */
    UPROPERTY(EditAnywhere)
    bool bAllowDeleteOrphanDefAssets = true;
};
```

### 1.1 配置校验规则

为了让这份提案足够接近可实现状态，`FTcsDataTableSyncConfig` 的运行前校验规则必须写死：

- `DefAssetClass` 不允许为空。
- `ManagedDefAssetDirectory.Path` 不允许为空，且必须是合法内容目录路径。
- `TargetDataTable` 不允许为空软引用；若软引用失效，则只允许在“资产路径仍可解析”的前提下补建缺失表。
- `ManagedDefAssetDirectory` 在所有配置项中必须唯一。
- `TargetDataTable` 在所有配置项中必须唯一。
- 同一 `DefAssetClass` 可以出现多次，但每条配置都必须绑定不同目录和不同 DataTable。

建议将校验接口显式建模为：

```cpp
struct FTcsDataTableSyncConfigValidationResult
{
    bool bValid = false;
    TArray<FText> Errors;
    TArray<FText> Warnings;
};

FTcsDataTableSyncConfigValidationResult ValidateConfig(const FTcsDataTableSyncConfig& Config) const;
bool ValidateAllConfigs(TArray<FText>& OutErrors, TArray<FText>& OutWarnings) const;
```

## 2. Row Struct 设计

当前实现将每种 DefAsset 的 `FTableRow` 子类集中放在运行时模块 `TireflyCombatSystem/Public/DataTableSync/TcsDefDataTableRows.h`，并在 `TireflyCombatSystem/Private/DataTableSync/TcsDefDataTableRows.cpp` 中维护静态类型描述符与同步辅助逻辑：

| DefAsset | Row Struct | 文件名 |
|----------|-----------|--------|
| `UTcsAttributeDefinition` | `FTcsAttributeDefRow` | `TcsDefDataTableRows.h` |
| `UTcsAttributeModifierDefinition` | `FTcsAttributeModifierDefRow` | `TcsDefDataTableRows.h` |
| `UTcsBuffDefinition` | `FTcsBuffDefRow` | `TcsDefDataTableRows.h` |
| `UTcsSkillDefinition` | `FTcsSkillDefRow` | `TcsDefDataTableRows.h` |
| `UTcsSkillModifierDefinition` | `FTcsSkillModifierDefRow` | `TcsDefDataTableRows.h` |
| `UTcsStateSlotDefinition` | `FTcsStateSlotDefRow` | `TcsDefDataTableRows.h` |

**Row Struct 业务字段名 = DefAsset 的 UPROPERTY 名，通过每类型静态同步函数直接赋值。**

`FGameplayTagContainer`、`FInstancedStruct` 均可作为 Row 的 UPROPERTY 类型——DataTable 编辑器原生支持 `FGameplayTagContainer` 列，`FInstancedStruct` 在 Row 中保存的是与 DefAsset 同类型的实例结构体（如 `FTcsSkillModifierFloatConfig`），静态同步函数负责完整深拷贝。

**设计原则：Row Struct 直接复用 DefAsset 中的已有嵌套结构体，除标识字段外属性名与类型 1:1 对应 DefAsset。** `RowName` 本身承载 `DefId`，Row Struct 内不再重复声明 `DefId` / `AttributeDefId` / `ModifierId` / `StateSlotDefId` 等标识字段。

### 示例 1：FTcsAttributeDefRow（1:1 镜像 UTcsAttributeDefinition）

```cpp
USTRUCT(BlueprintType)
struct FTcsAttributeDefRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FString AttributeCategory;

    UPROPERTY(EditAnywhere)
    FGameplayTag AttributeTag;

    UPROPERTY(EditAnywhere)
    FTcsAttributeRange AttributeRange;                // 嵌套结构体

    UPROPERTY(EditAnywhere)
    TSubclassOf<class UTcsAttributeClampStrategy> ClampStrategyClass;

    UPROPERTY(EditAnywhere)
    FInstancedStruct ClampStrategyConfig;             // FInstancedStruct

    UPROPERTY(EditAnywhere)
    bool bShowInUI = true;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "bShowInUI", EditConditionHides))
    FText AttributeName;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "bShowInUI", EditConditionHides))
    FText AttributeDescription;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "bShowInUI", EditConditionHides))
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "bShowInUI", EditConditionHides))
    bool bAsDecimal = false;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "bShowInUI", EditConditionHides))
    bool bAsPercentage = false;
};
```

### 示例 2：FTcsSkillModifierDefRow（含 FInstancedStruct）

```cpp
USTRUCT(BlueprintType)
struct FTcsSkillModifierDefRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    TSubclassOf<UTcsSkillEntrySelector> EntrySelectorClass;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "EntrySelectorClass != nullptr", EditConditionHides))
    FInstancedStruct EntrySelectorConfig;

    UPROPERTY(EditAnywhere)
    FGameplayTag TargetParamTag;

    UPROPERTY(EditAnywhere)
    ETcsStateParameterType TargetParamType = ETcsStateParameterType::SPT_Numeric;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "TargetParamTag.IsValid()", EditConditionHides))
    TSubclassOf<UTcsStateParamNumericModifierExecution> EvaluatorClass;

    UPROPERTY(EditAnywhere, Meta = (EditCondition = "EvaluatorClass != nullptr", EditConditionHides))
    FInstancedStruct EvaluatorConfig;

    UPROPERTY(EditAnywhere)
    int32 Priority = 0;

    UPROPERTY(EditAnywhere)
    ETcsSkillModifierMergePolicy MergePolicy = ETcsSkillModifierMergePolicy::Stack;
};
```

DataTable 编辑器中 `TMap` 列展开为每对 Key/Value 子列；`FInstancedStruct` 可展开编辑内部字段；`TSubclassOf` 显示为类选择器；`TSoftObjectPtr` 显示为资源引用选择器。**除标识字段由 `RowName` 承载外，其余业务字段均保持 1:1 映射。**

### 2.1 RowStruct 解析来源

当前提案已经要求“缺失 DataTable 时可自动创建”，那么实现前必须写死“创建成哪种 RowStruct”。否则 `TargetDataTable` 缺失时无法可靠建表。

建议引入显式描述符，而不是在创建时临时猜测：

```cpp
struct FTcsDefAssetSyncTypeDescriptor
{
    TSubclassOf<UPrimaryDataAsset> DefAssetClass;
    UScriptStruct* RowStruct = nullptr;
    FName DefIdPropertyName;
    FName TypeAlias;
};

const FTcsDefAssetSyncTypeDescriptor* FindTypeDescriptorByAssetClass(TSubclassOf<UPrimaryDataAsset> DefAssetClass) const;
UScriptStruct* ResolveExpectedRowStruct(const FTcsDataTableSyncConfig& Config) const;
bool ValidateDataTableRowStruct(const UDataTable* DataTable, const FTcsDataTableSyncConfig& Config, FText& OutError) const;
```

约束：

- `DefAssetClass -> RowStruct` 是静态一对一映射。
- 自动创建 DataTable 时，`RowStruct` 只能来自 `ResolveExpectedRowStruct()`，不能从已有行内容反推。
- 若已存在的 `TargetDataTable` 的 `RowStruct` 与配置期望不一致，同步必须拒绝执行并报错，而不是尝试兼容。

## 3. Subsystem 骨架

```cpp
// TcsDefAssetDataTableSyncSubsystem.h

UCLASS()
class UTcsDefAssetDataTableSyncSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    // 保存回调只入队，不直接执行同步
    void OnPackageSaved(const FString& PackageFileName, UPackage* Package, FObjectPostSaveContext ObjectSaveContext);

    // 资产删除事件：删除受管 DefAsset 时同步删表行
    void OnAssetRemoved(const FAssetData& AssetData);
    void OnInMemoryAssetDeleted(UObject* AssetObject);

    // 资产重命名 / 移动事件：受管目录变更时等价于旧绑定删除 + 新绑定新增
    void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);

    void QueueSyncFromSavedObject(UObject* SavedObject);
    void QueueDeleteSyncFromRemovedAsset(const FAssetData& RemovedAssetData);
    void QueueRebindSyncFromRenamedAsset(const FAssetData& AssetData, const FString& OldObjectPath);
    bool HandleDeferredSync(float DeltaTime);

    // DataTable → DefAsset
    void SyncDataTableToDefAssets(UDataTable* DataTable, const FTcsDataTableSyncConfig& Config);

    // DefAsset → DataTable
    void SyncDefAssetToDataTable(UPrimaryDataAsset* Asset, const FTcsDataTableSyncConfig& Config);

    // DefAsset 删除 → DataTable 删除行
    void RemoveDefAssetRowFromDataTable(const FAssetData& RemovedAssetData, const FTcsDataTableSyncConfig& Config);

    // 根据已保存的 DataTable / DefAsset / 删除事件反查配置
    const FTcsDataTableSyncConfig* FindConfigByDataTable(const UDataTable* DataTable) const;
    const FTcsDataTableSyncConfig* FindConfigByDefAsset(const UObject* DefAsset) const;
    const FTcsDataTableSyncConfig* FindConfigByRemovedAsset(const FAssetData& RemovedAssetData) const;

    // 加载配置中显式绑定的 DataTable；必要时按配置路径创建缺失表
    UDataTable* LoadOrCreateConfiguredDataTable(const FTcsDataTableSyncConfig& Config);
    UDataTable* CreateConfiguredDataTable(const FTcsDataTableSyncConfig& Config, UScriptStruct* RowStruct);

    // 字段映射：Row → DefAsset（主键由 RowName 提供）
    void CopyRowToDefAsset(const FTableRowBase& Row, UObject* DefAsset);

    // 字段映射：DefAsset → Row（主键写入 RowName）
    void CopyDefAssetToRow(const UObject* DefAsset, FTableRowBase& Row);

    // 具体资源操作
    UPrimaryDataAsset* CreateDefAssetForRow(FName DefId, const FTcsDataTableSyncConfig& Config);
    bool DeleteManagedDefAsset(UPrimaryDataAsset* Asset);
    bool UpsertDataTableRow(UDataTable* DataTable, FName RowName, const UObject* DefAsset, const FTcsDataTableSyncConfig& Config);
    bool RemoveDataTableRow(UDataTable* DataTable, FName RowName);

    bool TryExtractDefIdFromAsset(const UObject* DefAsset, FName& OutDefId) const;
    bool TryAssignDefIdToAsset(UObject* DefAsset, FName DefId) const;

    bool TryResolveManagedDirectory(const UObject* DefAsset, FString& OutDirectoryPath) const;
    bool TryResolveManagedDirectoryFromAssetData(const FAssetData& AssetData, FString& OutDirectoryPath) const;

    // 防循环与去重
    bool bIsSyncingBatch = false;
    TSet<FName> PendingPackageNames;
    TArray<TWeakObjectPtr<UObject>> PendingSyncObjects;
    TArray<FAssetData> PendingRemovedDefAssets;
    TMap<FSoftObjectPath, FName> CachedRemovedDefIds;
    TMap<FSoftObjectPath, FString> CachedRemovedDirectories;
    FTSTicker::FDelegateHandle DeferredSyncHandle;
};
```

### 3.1 子系统职责边界

为了避免实现时职责继续漂移，`UTcsDefAssetDataTableSyncSubsystem` 的职责边界应明确为：

- 只负责编辑器期 DataTable ↔ DefAsset 双向同步，不拥有 DefinitionRegistry 的事实主权。
- 只在 batch 结束后调用 `UTcsDefinitionRegistrySubsystem::RequestRefresh()`，不直接改写 Registry 快照。
- 只处理受管目录中的受管 `DefAssetClass`；目录外资产与无配置资产必须忽略。
- 只做 authoring 数据镜像，不负责运行时热更新、AssetManager 注册策略调整或 CSV 导入导出。

### 3.2 待处理请求与删除快照

删除同步和重绑定同步都要求“旧身份”可追溯，仅靠实时对象状态不够，因此需要显式快照类型：

```cpp
enum class ETcsDeferredSyncKind : uint8
{
    DataTableSaved,
    DefAssetSaved,
    DefAssetRemoved,
    DefAssetRebound,
};

struct FTcsPendingSyncRequest
{
    ETcsDeferredSyncKind Kind = ETcsDeferredSyncKind::DataTableSaved;
    TWeakObjectPtr<UObject> Object;
    FAssetData AssetData;
    FSoftObjectPath OldObjectPath;
};

struct FTcsRemovedDefAssetSnapshot
{
    FSoftObjectPath ObjectPath;
    FName DefId;
    FString ManagedDirectoryPath;
    TSubclassOf<UPrimaryDataAsset> DefAssetClass;
};
```

约束：

- `OnInMemoryAssetDeleted` 优先写入 `FTcsRemovedDefAssetSnapshot`。
- `OnAssetRemoved` 若拿不到运行时对象，则回退到 `AssetData` + 已缓存快照。
- 删除同步若最终无法确认 `DefId`，必须跳过该删除并输出错误日志，不能盲删整表或按资产名猜测。

## 4. 同步流程

### 4.1 DataTable 保存 → DefAsset

```
OnPackageSaved(DataTable):
    1. 通过 DataTable 资产路径匹配唯一的 SyncConfig
    2. 仅将该 DataTable 入队到 PendingSyncObjects
    3. 下一 tick 执行 deferred batch

DeferredSyncBatch(DataTable):
    if (bIsSyncingBatch) return;
    bIsSyncingBatch = true;

    1. 扫描 `ManagedDefAssetDirectory` 下的所有受管 DefAsset
    2. 建立 DefId → DefAsset 映射（DefId 取自资产自身 ID 字段）
    3. 遍历 DataTable 每一行:
       RowName = DefId
       if 已有 DefAsset:
           RowName 写回资产 ID 字段
           复制业务字段 Row → DefAsset
           MarkPackageDirty()
       else:
           NewObject
           RowName 写入新资产 ID 字段
           复制业务字段 Row → DefAsset
           MarkPackageDirty()
    4. 若 bAllowDeleteOrphanDefAssets 为 true:
       删除所有“DefId 不在 DataTable RowName 集合中”的孤立 DefAsset
    5. 调用一次 DefinitionRegistrySubsystem::RequestRefresh()

    bIsSyncingBatch = false;
```

### 4.2 DefAsset 保存 → DataTable

```
OnPackageSaved(DefAsset):
    1. 通过资产类型 + 所在目录匹配唯一的 SyncConfig
    2. 仅将该 DefAsset 入队到 PendingSyncObjects
    3. 下一 tick 执行 deferred batch

DeferredSyncBatch(DefAsset):
    if (bIsSyncingBatch) return;
    if (Config.bSyncDefAssetToDataTable == false) return;
    bIsSyncingBatch = true;

    1. 从 DefAsset 读取 DefId
    2. 加载或创建配置中显式绑定的目标 DataTable
    3. 以 RowName = DefId 查找/新增 DataTable 行
    4. 复制业务字段 DefAsset → Row
    5. MarkPackageDirty(DataTable)
    6. 调用一次 DefinitionRegistrySubsystem::RequestRefresh()

    bIsSyncingBatch = false;
```

### 4.3 DefAsset 删除 → DataTable 删除行

```
OnAssetRemoved(DefAsset) / OnInMemoryAssetDeleted(DefAsset):
    1. 确认该资产属于受管 DefAsset 类型与受管目录
    2. 读取被删资产删除前的 DefId（优先从 AssetData tag，必要时从对象实例缓存）
    3. 将“删除同步任务”入队到 PendingRemovedDefAssets
    4. 下一 tick 执行 deferred batch

DeferredSyncBatch(RemovedDefAsset):
    if (bIsSyncingBatch) return;
    bIsSyncingBatch = true;

    1. 加载配置中显式绑定的目标 DataTable
    2. 以 DefId 对应的 RowName 查找目标行
    3. 若该行存在，则从 DataTable 中移除该行
    4. MarkPackageDirty(DataTable)
    5. 调用一次 DefinitionRegistrySubsystem::RequestRefresh()

    bIsSyncingBatch = false;
```

### 4.4 DefAsset 重命名 / 移动 → 绑定重计算

```
OnAssetRenamed(DefAsset, OldObjectPath):
    1. 判断旧路径是否属于某个受管目录
    2. 判断新路径是否属于某个受管目录
    3. 若旧目录受管且新目录不受管：等价于“旧绑定删除”
    4. 若旧目录不受管且新目录受管：等价于“新绑定新增/更新”
    5. 若旧目录与新目录都受管但配置不同：等价于“旧表删行 + 新表增行”
    6. 若仅资产名变化且 DefId 不变：不因资产名变化改写 RowName
```

### 4.5 显式目录 → DataTable 绑定

```
Config.ManagedDefAssetDirectory = "/Game/TCS/AttributeDefs/Common"
Config.TargetDataTable = "/Game/TCS/AttributeTables/DT_CommonAttributeDefs.DT_CommonAttributeDefs"

一条配置严格绑定:
    /Game/TCS/AttributeDefs/Common/       ↔ /Game/TCS/AttributeTables/DT_CommonAttributeDefs
    /Game/TCS/AttributeDefs/Hero/HeroA/   ↔ /Game/TCS/AttributeTables/DT_HeroAAttributeDefs

约束:
    一个受管目录只对应一张 DataTable
    一张 DataTable 只对应一个受管目录
    同一 DefAssetClass 可通过多条配置绑定多个目录，但必须逐条显式声明
```

## 5. 字段映射

不使用反射——每种类型各一对静态 `SyncRowToAsset` / `SyncAssetToRow` 直接赋值，编译期类型安全，字段变更时编译报错。

### 5.1 标识字段规则

- `RowName` 是唯一主键，并承载 DefId。
- Row Struct 内不再重复声明 DefId 字段。
- `SyncRowToAsset` 必须先把 `RowName` 写回资产 ID 字段，再复制业务字段。
- `SyncAssetToRow` 必须从资产 ID 字段生成 `RowName`，再复制业务字段。

### 5.2 严格镜像规则

- 保存 DataTable 时，DataTable 是该同步组的结构权威源。
- 空表会收敛为其受管目录中的零个 DefAsset。
- 保存 DefAsset 时，只执行对应 DataTable 的单行新增/更新，不反向删除其他行。
- 手动删除受管 DefAsset 时，必须删除对应 DataTable 行。
- 受管 DefAsset 从一个绑定目录移动到另一个绑定目录时，必须表现为“旧表删行 + 新表增/改行”。
- 资产名称变化本身不是主键变化；只有资产内 ID 字段变化才影响 `RowName`。

### 5.3 配置与引用策略

- `FTcsDataTableSyncConfig` 的目录与 DataTable 绑定是显式 authoring 契约，不再从根路径自动派生多张表。
- `TargetDataTable` 优先使用 `TSoftObjectPtr<UDataTable>`，因为该能力是纯编辑器行为，按需加载更合适，也避免无意义的强持有。
- 若 `TargetDataTable` 软引用解析失败但配置路径仍有效，系统可以在该目标资产路径补建缺失 DataTable。
- 配置查找必须基于“DataTable 资产路径”或“DefAsset 所在目录 + 类型”匹配，不能只按 `DefAssetClass` 建单值缓存，否则多目录同类型会错配。

### 5.4 身份变更策略

- `RowName` 的来源永远是资产内部的 DefId 字段，而不是资产名。
- 若策划修改了受管 DefAsset 的 DefId 字段并保存，系统必须把这次变更视为“旧 RowName 删除 + 新 RowName 新增/更新”。
- 若保存时发现同一张 DataTable 内新旧 `DefId` 冲突，必须拒绝本次同步并输出错误，而不是覆盖既有行。
- 若 DataTable 保存时存在重复 `RowName` 对应到多个资产，必须以错误中止 batch，而不是随机选择一个资产继续同步。

### 5.5 失败与边界策略

- 配置无效、目录冲突、表冲突、RowStruct 不匹配、DefId 缺失、DefId 冲突都属于硬错误，当前批次应停止对应请求处理并记录 `Error` 日志。
- 单个请求失败不应导致整个子系统停机；后续独立请求仍可继续处理。
- 目录外资产、无配置资产、非受管类型资产属于正常忽略路径，只记录 `Verbose` 或不记录日志。
- 若 `TargetDataTable` 不存在且无法补建，当前请求失败并提示用户修正配置，不得偷偷改写到其他 DataTable。
- 删除同步若找不到对应行，可视为幂等成功，不作为错误。

### 5.6 事务与保存策略

- deferred sync batch 内的创建、更新、删除必须包裹在同一 `FScopedTransaction` 中。
- 同步过程中修改 counterpart 资产时只 `Modify()` + `MarkPackageDirty()`，默认不在 `OnPackageSaved` 回调链内立即二次 `SavePackage()`。

```cpp
// FTcsAttributeDefRow → UTcsAttributeDefinition
void SyncRowToAsset(const FTcsAttributeDefRow& Row, UTcsAttributeDefinition* Asset)
{
    Asset->AttributeDefId        = Row.GetRowName();
    Asset->AttributeCategory     = Row.AttributeCategory;
    Asset->AttributeTag          = Row.AttributeTag;
    Asset->AttributeRange        = Row.AttributeRange;
    Asset->ClampStrategyClass    = Row.ClampStrategyClass;
    Asset->ClampStrategyConfig   = Row.ClampStrategyConfig;
    Asset->AttributeName         = Row.AttributeName;
    Asset->AttributeDescription  = Row.AttributeDescription;
    Asset->bShowInUI             = Row.bShowInUI;
    Asset->Icon                  = Row.Icon;
    Asset->bAsDecimal            = Row.bAsDecimal;
    Asset->bAsPercentage         = Row.bAsPercentage;
}

// UTcsSkillModifierDefinition → FTcsSkillModifierDefRow
void SyncAssetToRow(const UTcsSkillModifierDefinition* Asset, FTcsSkillModifierDefRow& Row)
{
    Row.EntrySelectorClass  = Asset->EntrySelectorClass;
    Row.EntrySelectorConfig = Asset->EntrySelectorConfig;
    Row.TargetParamTag      = Asset->TargetParamTag;
    Row.TargetParamType     = Asset->TargetParamType;
    Row.EvaluatorClass      = Asset->EvaluatorClass;
    Row.EvaluatorConfig     = Asset->EvaluatorConfig;
    Row.Priority            = Asset->Priority;
    Row.MergePolicy         = Asset->MergePolicy;
}
```

每种 Row 类型各一对 `SyncRowToAsset` / `SyncAssetToRow`，编译期校验完整性。

## 6. 文件布局

```
TireflyCombatSystem/
  Public/
    DataTableSync/
            TcsDefDataTableRows.h
  Private/
    DataTableSync/
            TcsDefDataTableRows.cpp

TireflyCombatSystemEditor/
    Private/
        DataTableSync/
            TcsDefAssetDataTableSyncSubsystem.h
      TcsDefAssetDataTableSyncSubsystem.cpp
```
