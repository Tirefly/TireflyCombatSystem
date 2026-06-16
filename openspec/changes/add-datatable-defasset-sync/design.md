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

    /** DefaultEngine.ini 中该类型 PrimaryAssetTypesToScan 配置的基路径。 */
    UPROPERTY(EditAnywhere, Meta = (ContentDir))
    FDirectoryPath DataTableBasePath;

    /** 是否同步 DefAsset → DataTable（默认为 true，保存 DefAsset 时回写）。 */
    UPROPERTY(EditAnywhere)
    bool bSyncDefAssetToDataTable = true;
};
```

## 2. Row Struct 设计

每种 DefAsset 一个 `FTableRow` 子类，放在 Editor 模块 `Public/DataTableSync/`：

| DefAsset | Row Struct | 文件名 |
|----------|-----------|--------|
| `UTcsAttributeDefinition` | `FAttributeDefRow` | `TcsAttributeDefRow.h` |
| `UTcsAttributeModifierDefinition` | `FAttributeModifierDefRow` | `TcsAttributeModifierDefRow.h` |
| `UTcsBuffDefinition` | `FBuffDefRow` | `TcsBuffDefRow.h` |
| `UTcsSkillDefinition` | `FSkillDefRow` | `TcsSkillDefRow.h` |
| `UTcsSkillModifierDefinition` | `FSkillModifierDefRow` | `TcsSkillModifierDefRow.h` |
| `UTcsStateSlotDefinition` | `FStateSlotDefRow` | `TcsStateSlotDefRow.h` |

**Row Struct 列名 = DefAsset 的 UPROPERTY 名，通过反射自动映射。**

`FGameplayTagContainer`、`FInstancedStruct` 均可作为 Row 的 UPROPERTY 类型——DataTable 编辑器原生支持 `FGameplayTagContainer` 列，`FInstancedStruct` 在 Row 中保存的是与 DefAsset 同类型的实例结构体（如 `FTcsSkillModifierFloatConfig`），`CopyCompleteValue` 自动处理完整深拷贝。

**设计原则：Row Struct 直接复用 DefAsset 中的已有嵌套结构体，属性名与类型 1:1 对应 DefAsset。** 每行 RowName = DefAssetId。

### 示例 1：FAttributeDefRow（1:1 镜像 UTcsAttributeDefinition）

```cpp
USTRUCT(BlueprintType)
struct FAttributeDefRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName AttributeDefId;

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
    FText AttributeName;

    UPROPERTY(EditAnywhere)
    FText AttributeDescription;

    UPROPERTY(EditAnywhere)
    bool bShowInUI = true;

    UPROPERTY(EditAnywhere)
    TSoftObjectPtr<UTexture2D> Icon;

    UPROPERTY(EditAnywhere)
    bool bAsDecimal = false;

    UPROPERTY(EditAnywhere)
    bool bAsPercentage = false;
};
```

### 示例 2：FSkillModifierDefRow（含 FInstancedStruct）

```cpp
USTRUCT(BlueprintType)
struct FSkillModifierDefRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere)
    FName ModifierId;

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

DataTable 编辑器中 `TMap` 列展开为每对 Key/Value 子列；`FInstancedStruct` 可展开编辑内部字段；`TSubclassOf` 显示为类选择器；`TSoftObjectPtr` 显示为资源引用选择器。**所有 DefAsset UPROPERTY 均 1:1 映射，无任何跳过字段。**

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
    // 保存回调
    void OnPackageSaved(const FString& PackageFileName, UObject* Outer);

    // DataTable → DefAsset
    void SyncDataTableToDefAssets(UDataTable* DataTable, const FTcsDataTableSyncConfig& Config);

    // DefAsset → DataTable
    void SyncDefAssetToDataTable(UPrimaryDataAsset* Asset, const FTcsDataTableSyncConfig& Config);

    // 根据 DefAsset 的路径反查所属 DataTable
    UDataTable* FindDataTableForDefAsset(UPrimaryDataAsset* Asset, const FTcsDataTableSyncConfig& Config);

    // 根据子目录名推断 DataTable 名并加载/创建
    UDataTable* LoadOrCreateDataTable(const FString& SubDirectory, const FTcsDataTableSyncConfig& Config);

    // 字段映射：Row → DefAsset（同名属性 Copy）
    void CopyRowToDefAsset(const FTableRowBase& Row, UObject* DefAsset);

    // 字段映射：DefAsset → Row（同名属性 Copy）
    void CopyDefAssetToRow(const UObject* DefAsset, FTableRowBase& Row);

    // 防循环标志
    bool bIsSyncing = false;

    // 缓存 AssetManager 扫描配置，避免重复查询
    TMap<TSubclassOf<UPrimaryDataAsset>, FString> CachedBasePaths;
};
```

## 4. 同步流程

### 4.1 DataTable 保存 → DefAsset

```
OnPackageSaved(DataTable):
    if (bIsSyncing) return;
    bIsSyncing = true;

    1. 从 DataTable 的 FilePath 匹配 Config:
       - 检查 DataTable 路径是否在 DataTableBasePath 的子目录下
       - 匹配 DefAssetClass + RowStruct 对应

    2. 加载/扫描目录下已有 DefAsset:
       - 通过 AssetRegistry 获取 BasePath/SubDirectory/ 下所有 DefAssetClass 的资产
       - 建立 AssetName → Asset 映射

    3. 遍历 DataTable 每一行:
       RowName = Row.RowName (即 DefAssetId)
       if 已有 DefAsset:
           复制 Row → DefAsset, MarkPackageDirty()
       else:
           NewObject, 复制 Row → DefAsset, SavePackage()

    4. 清理孤立 DefAsset:
       目录中有但 DataTable 中无 → 标记删除/移动

    bIsSyncing = false;
```

### 4.2 DefAsset 保存 → DataTable

```
OnPackageSaved(DefAsset):
    if (bIsSyncing) return;
    if (Config.bSyncDefAssetToDataTable == false) return;

    bIsSyncing = true;

    1. 匹配 Config:
       - 检查 DefAsset 类型是否在 Config 列表中
       - 从 FilePath 提取子目录名

    2. 定位 DataTable:
       - 加载/创建 DT_{前缀}_{子目录}

    3. 更新 DataTable 行:
       - RowName = DefAsset.AssetName
       - 复制 DefAsset → Row, MarkPackageDirty(DataTable)

    bIsSyncing = false;
```

### 4.3 子目录 → DataTable 命名

```
Config.DataTableBasePath = "/Game/TCS/AttributeDefs"

自动扫描子目录:
  /Game/TCS/AttributeDefs/Common/   →  DT_TCS_AttributeDefs_Common
  /Game/TCS/AttributeDefs/Heroes/   →  DT_TCS_AttributeDefs_Heroes

DataTable 路径 = {DataTableBasePath}/{SubDirName}.uasset
```

## 5. 字段映射

不使用反射——直接赋值，编译期类型安全，字段变更时编译报错。

```cpp
// FAttributeDefRow → UTcsAttributeDefinition
void SyncRowToAsset(const FAttributeDefRow& Row, UTcsAttributeDefinition* Asset)
{
    Asset->AttributeDefId        = Row.AttributeDefId;
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

// UTcsSkillModifierDefinition → FSkillModifierDefRow
void SyncAssetToRow(const UTcsSkillModifierDefinition* Asset, FSkillModifierDefRow& Row)
{
    Row.ModifierId          = Asset->ModifierId;
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
TireflyCombatSystemEditor/
  Public/
    DataTableSync/
      TcsDefAssetDataTableSyncSubsystem.h
      TcsAttributeDefRow.h
      TcsAttributeModifierDefRow.h
      TcsBuffDefRow.h
      TcsSkillDefRow.h
      TcsSkillModifierDefRow.h
      TcsStateSlotDefRow.h
  Private/
    DataTableSync/
      TcsDefAssetDataTableSyncSubsystem.cpp
```
