# Design: DataAsset 架构设计

## 架构概览

### 核心设计原则

1. **保持 API 兼容性**: 外部调用者无需修改代码，内部实现透明切换
2. **FName 作为主键**: 继续使用 FName 作为定义的唯一标识符，保持查找语义
3. **渐进式迁移**: 先迁移核心定义（Attribute、State、StateSlot），Modifier 定义后续评估
4. **零蓝图影响**: 当前无蓝图资产，无需考虑蓝图兼容性

### 系统架构图

```
┌─────────────────────────────────────────────────────────────┐
│                   UTcsDeveloperSettings                      │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ TArray<FDirectoryPath> AttributeDefinitionPaths       │ │
│  │ TArray<FDirectoryPath> StateDefinitionPaths           │ │
│  │ TArray<FDirectoryPath> StateSlotDefinitionPaths       │ │
│  │ TArray<FDirectoryPath> AttributeModifierDefinitionPaths│ │
│  │                                                        │ │
│  │ 内部缓存 (Transient):                                  │ │
│  │ TMap<FName, TSoftObjectPtr<UTcsAttributeDefAsset>>   │ │
│  │ TMap<FName, TSoftObjectPtr<UTcsStateDefAsset>>       │ │
│  │ TMap<FName, TSoftObjectPtr<UTcsStateSlotDefAsset>>   │ │
│  │ TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefAsset>>│ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            │
                            │ Asset Registry Scan & Monitor
                            ▼
┌─────────────────────────────────────────────────────────────┐
│              Manager Subsystems (Runtime)                    │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ TMap<FName, UTcsAttributeDefAsset*> Registry          │ │
│  │ TMap<FName, UTcsStateDefAsset*> Registry              │ │
│  │ TMap<FGameplayTag, UTcsStateDefAsset*> StateTagMap    │ │
│  │ TMap<FName, UTcsStateSlotDefAsset*> Registry          │ │
│  │ TMap<FName, UTcsAttributeModifierDefAsset*> Registry  │ │
│  └────────────────────────────────────────────────────────┘ │
│                                                              │
│  API:                                                         │
│  - GetAttributeDefAsset(FName) -> UTcsAttributeDefAsset*    │
│  - GetStateDefAsset(FName) -> UTcsStateDefAsset*            │
│  - GetStateDefAssetByTag(FGameplayTag) -> UTcsStateDefAsset*│
│  - GetStateSlotDefAsset(FName) -> UTcsStateSlotDefAsset*    │
│  - GetAttributeModifierDefAsset(FName) -> UTcsAttributeModifierDefAsset*│
└─────────────────────────────────────────────────────────────┘
```

## DataAsset 类设计

### 1. UTcsAttributeDefinitionAsset

```cpp
/**
 * 属性定义资产
 *
 * 用途: 定义单个属性的所有配置信息
 * 继承: UPrimaryDataAsset（支持 Asset Manager）
 * 命名约定: DA_Attr_<AttributeName> (例如: DA_Attr_Health)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsAttributeDefinitionAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * PrimaryAssetType 标识符
     * 注意：虽然 FPrimaryAssetType 是 FName 的 typedef，但使用 FPrimaryAssetType 更语义化
     */
    static const FPrimaryAssetType PrimaryAssetType;  // 在 .cpp 中定义为 "TcsAttributeDef"

    // ========== Identity ==========

    /**
     * 属性的唯一标识符
     * 对应原 DataTable 的 RowName
     * 必须全局唯一，推荐命名: Health, MaxHealth, AttackPower 等
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (ToolTip = "属性的唯一标识符，必须全局唯一"))
    FName AttributeDefId;

    // ========== Range ==========

    /** 属性数值范围 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range",
        meta = (ToolTip = "属性的最小值和最大值范围"))
    FTcsAttributeRange AttributeRange;

    /** Clamp 策略类 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range",
        meta = (ToolTip = "属性值超出范围时的处理策略"))
    TSubclassOf<UTcsAttributeClampStrategy> ClampStrategyClass;

    /** Clamp 策略配置 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range",
        meta = (ToolTip = "Clamp 策略的具体配置参数"))
    FInstancedStruct ClampStrategyConfig;

    // ========== Meta ==========

    /** 属性类别 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta",
        meta = (ToolTip = "属性的分类标签，用于组织和筛选"))
    FString AttributeCategory;

    /** 属性缩写（用于公式） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta",
        meta = (ToolTip = "在公式中使用的简短标识符"))
    FString AttributeAbbreviation;

    /** 属性的语义标识 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Meta",
        meta = (Categories = "TCS.Attribute", ToolTip = "用于语义查询的 GameplayTag"))
    FGameplayTag AttributeTag;

    // ========== Display ==========

    /** 属性名（本地化） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display",
        meta = (ToolTip = "显示给玩家的属性名称"))
    FText AttributeName;

    /** 属性描述（本地化） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Display",
        meta = (ToolTip = "属性的详细说明"))
    FText AttributeDescription;

    // ========== UI ==========

    /** 是否在UI中显示 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI",
        meta = (ToolTip = "是否在游戏界面中显示此属性"))
    bool bShowInUI = true;

    /** 属性图标 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI",
        meta = (ToolTip = "属性的图标资源"))
    TSoftObjectPtr<UTexture2D> Icon;

    /** 是否显示为小数 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI",
        meta = (ToolTip = "是否以小数形式显示数值"))
    bool bAsDecimal = true;

    /** 是否显示为百分比 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI",
        meta = (ToolTip = "是否以百分比形式显示数值"))
    bool bAsPercentage = false;

public:
    // ========== UPrimaryDataAsset Interface ==========

    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(PrimaryAssetType, AttributeDefId);
    }

#if WITH_EDITOR
    // ========== Editor Validation ==========

    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
```

### 2. UTcsStateDefinitionAsset

```cpp
/**
 * 状态定义资产
 *
 * 用途: 定义单个状态/技能/Buff 的所有配置信息
 * 继承: UPrimaryDataAsset
 * 命名约定: DA_State_<StateName> (例如: DA_State_Stunned, DA_Skill_Dash)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsStateDefinitionAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * PrimaryAssetType 标识符
     * 注意：虽然 FPrimaryAssetType 是 FName 的 typedef，但使用 FPrimaryAssetType 更语义化
     */
    static const FPrimaryAssetType PrimaryAssetType;  // 在 .cpp 中定义为 "TcsStateDef"

    // ========== Identity ==========

    /** 状态的唯一标识符（对应原 RowName） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (ToolTip = "状态的唯一标识符，必须全局唯一"))
    FName StateDefId;

    /** 状态的语义标识 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (Categories = "TCS.State", ToolTip = "用于语义查询的 GameplayTag，可通过此 Tag 快速查找状态"))
    FGameplayTag StateTag;

    // ========== 原 FTcsStateDefinition 的所有字段 ==========
    // (保持字段名和类型完全一致，便于迁移)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic",
        meta = (ToolTip = "状态类型：普通状态、技能或 Buff"))
    TEnumAsByte<ETcsStateType> StateType = ST_State;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic",
        meta = (ToolTip = "状态所属的槽位类型"))
    FGameplayTag StateSlotType;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Basic",
        meta = (ToolTip = "状态优先级，数值越大优先级越高"))
    int32 Priority = 0;

    // ... (其他所有字段，完整复制 FTcsStateDefinition)

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(PrimaryAssetType, StateDefId);
    }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

### 3. UTcsStateSlotDefinitionAsset

```cpp
/**
 * 状态槽定义资产
 *
 * 用途: 定义单个状态槽的配置
 * 继承: UPrimaryDataAsset
 * 命名约定: DA_Slot_<SlotName> (例如: DA_Slot_Action, DA_Slot_Buff)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsStateSlotDefinitionAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * PrimaryAssetType 标识符
     * 注意：虽然 FPrimaryAssetType 是 FName 的 typedef，但使用 FPrimaryAssetType 更语义化
     */
    static const FPrimaryAssetType PrimaryAssetType;  // 在 .cpp 中定义为 "TcsStateSlotDef"

    /** 槽位的唯一标识符（对应原 RowName） */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (ToolTip = "槽位的唯一标识符，必须全局唯一"))
    FName StateSlotDefId;

    // ========== 原 FTcsStateSlotDefinition 的所有字段 ==========

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot",
        meta = (ToolTip = "槽位的 GameplayTag 标识"))
    FGameplayTag SlotTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot",
        meta = (ToolTip = "关联的 StateTree 状态名称"))
    FName StateTreeStateName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot Gate",
        meta = (ToolTip = "槽位激活模式"))
    ETcsStateSlotActivationMode ActivationMode = ETcsStateSlotActivationMode::SSAM_PriorityOnly;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "State Slot Gate",
        meta = (ToolTip = "槽位门关闭时的行为策略"))
    ETcsStateSlotGateClosePolicy GateCloseBehavior = ETcsStateSlotGateClosePolicy::SSGCP_Pause;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Configuration",
        meta = (ToolTip = "状态抢占策略"))
    ETcsStatePreemptionPolicy PreemptionPolicy = ETcsStatePreemptionPolicy::SPP_PauseLowerPriority;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Slot Configuration",
        meta = (ToolTip = "相同优先级状态的处理策略"))
    TSubclassOf<UTcsStateSamePriorityPolicy> SamePriorityPolicy;

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(PrimaryAssetType, StateSlotDefId);
    }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

### 4. UTcsAttributeModifierDefinitionAsset

```cpp
/**
 * 属性修改器定义资产
 *
 * 用途: 定义属性修改器的配置信息
 * 继承: UPrimaryDataAsset
 * 命名约定: DA_Modifier_<ModifierName> (例如: DA_Modifier_DamageBoost)
 */
UCLASS(BlueprintType, Const)
class TIREFLYCOMBATSYSTEM_API UTcsAttributeModifierDefinitionAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /**
     * PrimaryAssetType 标识符
     * 注意：虽然 FPrimaryAssetType 是 FName 的 typedef，但使用 FPrimaryAssetType 更语义化
     */
    static const FPrimaryAssetType PrimaryAssetType;  // 在 .cpp 中定义为 "TcsAttributeModifierDef"

    /** 修改器的唯一标识符 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity",
        meta = (ToolTip = "修改器的唯一标识符，必须全局唯一"))
    FName AttributeModifierDefId;

    // ========== 原 FTcsAttributeModifierDefinition 的所有字段 ==========

    /** 目标属性 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier",
        meta = (ToolTip = "要修改的目标属性"))
    FName TargetAttribute;

    /** 修改器类型 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier",
        meta = (ToolTip = "修改器的计算类型（加法、乘法等）"))
    TEnumAsByte<ETcsAttributeModifierType> ModifierType;

    /** 修改值 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modifier",
        meta = (ToolTip = "修改器的数值"))
    float ModifierValue = 0.0f;

    // ... (其他所有字段，完整复制 FTcsAttributeModifierDefinition)

public:
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(PrimaryAssetType, AttributeModifierDefId);
    }

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
```

## 配置系统设计

### UTcsDeveloperSettings 改造

```cpp
UCLASS(Config = TireflyCombatSystemSettings, DefaultConfig)
class TIREFLYCOMBATSYSTEM_API UTcsDeveloperSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // ========== DataAsset 路径配置 ==========

    /** 属性定义资产路径 */
    UPROPERTY(Config, EditAnywhere, Category = "Definition Assets|Paths",
        meta = (ToolTip = "属性定义资产的搜索路径，系统会自动扫描这些路径下的所有 AttributeDefAsset"))
    TArray<FDirectoryPath> AttributeDefinitionPaths;

    /** 状态定义资产路径 */
    UPROPERTY(Config, EditAnywhere, Category = "Definition Assets|Paths",
        meta = (ToolTip = "状态定义资产的搜索路径"))
    TArray<FDirectoryPath> StateDefinitionPaths;

    /** 状态槽定义资产路径 */
    UPROPERTY(Config, EditAnywhere, Category = "Definition Assets|Paths",
        meta = (ToolTip = "状态槽定义资产的搜索路径"))
    TArray<FDirectoryPath> StateSlotDefinitionPaths;

    /** 属性修改器定义资产路径 */
    UPROPERTY(Config, EditAnywhere, Category = "Definition Assets|Paths",
        meta = (ToolTip = "属性修改器定义资产的搜索路径"))
    TArray<FDirectoryPath> AttributeModifierDefinitionPaths;

    // ========== State 加载策略 ==========

    /** State 定义的加载策略 */
    UPROPERTY(Config, EditAnywhere, Category = "Performance|State Loading",
        meta = (ToolTip = "State 定义的加载策略：全部预加载、按需加载或混合模式"))
    ETcsStateDefLoadingStrategy StateLoadingStrategy = ETcsStateDefLoadingStrategy::PreloadAll;

    /** 常用 State 定义路径（仅在混合模式下使用） */
    UPROPERTY(Config, EditAnywhere, Category = "Performance|State Loading",
        meta = (
            EditCondition = "StateLoadingStrategy == ETcsStateDefLoadingStrategy::Hybrid",
            EditConditionHides,
            ToolTip = "混合模式下需要预加载的常用 State 路径"
        ))
    TArray<FDirectoryPath> CommonStateDefinitionPaths;

    // ========== 内部缓存（不暴露给编辑器） ==========

    UPROPERTY(Transient)
    TMap<FName, TSoftObjectPtr<UTcsAttributeDefAsset>> CachedAttributeDefinitions;

    UPROPERTY(Transient)
    TMap<FName, TSoftObjectPtr<UTcsStateDefAsset>> CachedStateDefinitions;

    UPROPERTY(Transient)
    TMap<FName, TSoftObjectPtr<UTcsStateSlotDefAsset>> CachedStateSlotDefinitions;

    UPROPERTY(Transient)
    TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefAsset>> CachedAttributeModifierDefinitions;

#if WITH_EDITOR
    virtual void PostInitProperties() override;
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
    virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

private:
    // 注意：以下回调函数不需要 UFUNCTION 标记
    // 这些是纯 C++ 委托回调,使用 AddUObject 绑定,不涉及反射系统
    // 只有需要蓝图调用、网络 RPC 或动态委托的函数才需要 UFUNCTION

    /** 扫描指定路径下的所有 DataAsset 并更新缓存 */
    void ScanAndCacheDefinitions();

    /** 注册资产监听器 */
    void RegisterAssetListeners();

    /** 资产添加回调 */
    void OnAssetAdded(const FAssetData& AssetData);

    /** 资产移除回调 */
    void OnAssetRemoved(const FAssetData& AssetData);

    /** 资产重命名回调 */
    void OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath);
#endif
};
```

### 加载策略枚举

```cpp
/** State 定义的加载策略 */
UENUM(BlueprintType)
enum class ETcsStateDefLoadingStrategy : uint8
{
    /** 全部预加载：启动时加载所有 State 定义 */
    PreloadAll UMETA(DisplayName = "Preload All"),

    /** 按需加载：仅在使用时加载 State 定义 */
    OnDemand UMETA(DisplayName = "On Demand"),

    /** 混合模式：预加载常用 State，其他按需加载 */
    Hybrid UMETA(DisplayName = "Hybrid")
};
```

## 资产扫描和监听机制

### 自动扫描机制

在编辑器环境下，`UTcsDeveloperSettings` 会自动扫描配置的路径并缓存所有定义资产：

```cpp
void UTcsDeveloperSettings::ScanAndCacheDefinitions()
{
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    // 扫描属性定义
    for (const FDirectoryPath& Path : AttributeDefinitionPaths)
    {
        TArray<FAssetData> AssetDataList;
        AssetRegistry.GetAssetsByPath(FName(*Path.Path), AssetDataList, true);

        for (const FAssetData& AssetData : AssetDataList)
        {
            if (AssetData.AssetClassPath == UTcsAttributeDefAsset::StaticClass()->GetClassPathName())
            {
                if (UTcsAttributeDefAsset* Asset = Cast<UTcsAttributeDefAsset>(AssetData.GetAsset()))
                {
                    CachedAttributeDefinitions.Add(Asset->AttributeDefId, Asset);
                }
            }
        }
    }

    // 类似地扫描其他类型的定义...
}
```

### 资产监听机制

注册 Asset Registry 回调以监听资产变更：

```cpp
void UTcsDeveloperSettings::RegisterAssetListeners()
{
    IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

    // 监听资产添加
    AssetRegistry.OnAssetAdded().AddUObject(this, &UTcsDeveloperSettings::OnAssetAdded);

    // 监听资产移除
    AssetRegistry.OnAssetRemoved().AddUObject(this, &UTcsDeveloperSettings::OnAssetRemoved);

    // 监听资产重命名
    AssetRegistry.OnAssetRenamed().AddUObject(this, &UTcsDeveloperSettings::OnAssetRenamed);
}

void UTcsDeveloperSettings::OnAssetAdded(const FAssetData& AssetData)
{
    // 检查是否是定义资产类型
    if (AssetData.AssetClassPath == UTcsAttributeDefAsset::StaticClass()->GetClassPathName())
    {
        // 检查是否在配置的路径中
        for (const FDirectoryPath& Path : AttributeDefinitionPaths)
        {
            if (AssetData.PackagePath.ToString().StartsWith(Path.Path))
            {
                if (UTcsAttributeDefAsset* Asset = Cast<UTcsAttributeDefAsset>(AssetData.GetAsset()))
                {
                    CachedAttributeDefinitions.Add(Asset->AttributeDefId, Asset);
                    // 通知 Subsystem 更新
                }
                break;
            }
        }
    }
}

void UTcsDeveloperSettings::OnAssetRemoved(const FAssetData& AssetData)
{
    // 从缓存中移除
    // 通知 Subsystem 更新
}

void UTcsDeveloperSettings::OnAssetRenamed(const FAssetData& AssetData, const FString& OldObjectPath)
{
    // 更新缓存中的引用
    // 通知 Subsystem 更新
}
```

### 初始化时机

- **编辑器启动时**: `PostInitProperties()` 中执行初始扫描
- **配置修改时**: `PostEditChangeProperty()` 中重新扫描受影响的路径
- **运行时**: Subsystem 从缓存中读取，无需扫描

## Manager Subsystem 改造

### UTcsAttributeManagerSubsystem

```cpp
class TIREFLYCOMBATSYSTEM_API UTcsAttributeManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    // ========== Runtime Registry ==========

    /** 运行时注册表：AttributeDefId -> DataAsset */
    UPROPERTY()
    TMap<FName, UTcsAttributeDefAsset*> AttributeDefRegistry;

    /** AttributeTag -> AttributeDefId 映射（保持现有功能） */
    TMap<FGameplayTag, FName> AttributeTagToName;
    TMap<FName, FGameplayTag> AttributeNameToTag;

public:
    // ========== DataAsset API ==========

    /**
     * 获取属性定义资产（推荐使用）
     * 返回 DataAsset 指针，可以访问更多功能
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|Attribute")
    UTcsAttributeDefAsset* GetAttributeDefAsset(FName AttributeDefId) const;

    /**
     * 获取所有已注册的属性名称
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|Attribute")
    TArray<FName> GetAllAttributeNames() const;

private:
    /**
     * 从 DeveloperSettings 加载并注册所有 DataAsset
     */
    void LoadAndRegisterDefinitions();
};
```

### 实现细节

```cpp
void UTcsAttributeManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    LoadAndRegisterDefinitions();
}

void UTcsAttributeManagerSubsystem::LoadAndRegisterDefinitions()
{
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (!Settings)
    {
        UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to get TcsDeveloperSettings"), *FString(__FUNCTION__));
        return;
    }

    // 清空现有注册表
    AttributeDefRegistry.Empty();
    AttributeTagToName.Empty();
    AttributeNameToTag.Empty();

    // 从缓存中加载所有 DataAsset
    for (const auto& Pair : Settings->CachedAttributeDefinitions)
    {
        const FName& DefId = Pair.Key;
        const TSoftObjectPtr<UTcsAttributeDefAsset>& AssetPtr = Pair.Value;

        // 同步加载 DataAsset
        UTcsAttributeDefAsset* Asset = AssetPtr.LoadSynchronous();
        if (!Asset)
        {
            UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Failed to load AttributeDefAsset: %s"),
                *FString(__FUNCTION__), *DefId.ToString());
            continue;
        }

        // 验证 DefinitionId 一致性
        if (Asset->DefinitionId != DefId)
        {
            UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] DefinitionId mismatch: Key=%s, Asset.DefinitionId=%s"),
                *FString(__FUNCTION__), *DefId.ToString(), *Asset->DefinitionId.ToString());
        }

        // 注册到运行时注册表
        AttributeDefRegistry.Add(DefId, Asset);

        // 构建 Tag 映射
        if (Asset->AttributeTag.IsValid())
        {
            if (AttributeTagToName.Contains(Asset->AttributeTag))
            {
                UE_LOG(LogTcsAttribute, Error, TEXT("[%s] Duplicate AttributeTag: %s"),
                    *FString(__FUNCTION__), *Asset->AttributeTag.ToString());
            }
            else
            {
                AttributeTagToName.Add(Asset->AttributeTag, DefId);
                AttributeNameToTag.Add(DefId, Asset->AttributeTag);
            }
        }
    }

    UE_LOG(LogTcsAttribute, Log, TEXT("[%s] Loaded %d attribute definitions"),
        *FString(__FUNCTION__), AttributeDefRegistry.Num());
}

UTcsAttributeDefAsset* UTcsAttributeManagerSubsystem::GetAttributeDefAsset(FName AttributeDefId) const
{
    if (UTcsAttributeDefAsset* const* AssetPtr = AttributeDefRegistry.Find(AttributeDefId))
    {
        return *AssetPtr;
    }

    UE_LOG(LogTcsAttribute, Warning, TEXT("[%s] AttributeDefAsset not found: %s"),
        *FString(__FUNCTION__), *AttributeDefId.ToString());
    return nullptr;
}
```

### UTcsStateManagerSubsystem

```cpp
class TIREFLYCOMBATSYSTEM_API UTcsStateManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

private:
    // ========== Runtime Registry ==========

    /** 运行时注册表：StateDefId -> DataAsset */
    UPROPERTY()
    TMap<FName, UTcsStateDefAsset*> StateDefRegistry;

    /** StateTag -> StateDefId 映射 */
    TMap<FGameplayTag, FName> StateTagToName;

    /** 运行时注册表：StateSlotDefId -> DataAsset */
    UPROPERTY()
    TMap<FName, UTcsStateSlotDefAsset*> StateSlotDefRegistry;

    /** StateSlotTag -> StateSlotDefId 映射 */
    TMap<FGameplayTag, FName> StateSlotTagToName;

public:
    // ========== DataAsset API ==========

    /**
     * 获取状态定义资产
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    UTcsStateDefAsset* GetStateDefAsset(FName StateDefId) const;

    /**
     * 通过 StateTag 获取状态定义资产
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    UTcsStateDefAsset* GetStateDefAssetByTag(FGameplayTag StateTag) const;

    /**
     * 获取所有已注册的状态名称
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    TArray<FName> GetAllStateNames() const;

    /**
     * 获取状态槽定义资产
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    UTcsStateSlotDefAsset* GetStateSlotDefAsset(FName StateSlotDefId) const;

    /**
     * 通过 StateSlotTag 获取状态槽定义资产
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    UTcsStateSlotDefAsset* GetStateSlotDefAssetByTag(FGameplayTag StateSlotTag) const;

    /**
     * 获取所有已注册的状态槽名称
     */
    UFUNCTION(BlueprintCallable, Category = "TCS|State")
    TArray<FName> GetAllStateSlotNames() const;

private:
    /**
     * 从 DeveloperSettings 加载并注册所有 DataAsset
     * 根据配置的加载策略执行不同的加载逻辑
     */
    void LoadAndRegisterDefinitions();

    /**
     * 预加载所有 State 定义
     */
    void PreloadAllStates();

    /**
     * 预加载常用 State 定义（混合模式）
     */
    void PreloadCommonStates();

    /**
     * 按需加载单个 State 定义
     */
    UTcsStateDefAsset* LoadStateOnDemand(FName StateDefId);
};
```

### 灵活加载策略实现

```cpp
void UTcsStateManagerSubsystem::LoadAndRegisterDefinitions()
{
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (!Settings)
    {
        return;
    }

    StateDefRegistry.Empty();
    StateTagToName.Empty();
    StateSlotDefRegistry.Empty();
    StateSlotTagToName.Empty();

    // 加载 StateSlot 定义（总是预加载）
    for (const auto& Pair : Settings->CachedStateSlotDefinitions)
    {
        const FName& DefId = Pair.Key;
        const TSoftObjectPtr<UTcsStateSlotDefAsset>& AssetPtr = Pair.Value;

        UTcsStateSlotDefAsset* Asset = AssetPtr.LoadSynchronous();
        if (Asset)
        {
            StateSlotDefRegistry.Add(DefId, Asset);

            if (Asset->SlotTag.IsValid())
            {
                StateSlotTagToName.Add(Asset->SlotTag, DefId);
            }
        }
    }

    // 根据加载策略执行不同逻辑
    switch (Settings->StateLoadingStrategy)
    {
    case ETcsStateDefLoadingStrategy::PreloadAll:
        PreloadAllStates();
        break;

    case ETcsStateDefLoadingStrategy::OnDemand:
        // 不预加载，仅在使用时加载
        break;

    case ETcsStateDefLoadingStrategy::Hybrid:
        PreloadCommonStates();
        break;
    }

    // 初始化状态槽
    InitStateSlotDefs();
}

void UTcsStateManagerSubsystem::PreloadAllStates()
{
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();

    for (const auto& Pair : Settings->CachedStateDefinitions)
    {
        const FName& DefId = Pair.Key;
        const TSoftObjectPtr<UTcsStateDefAsset>& AssetPtr = Pair.Value;

        UTcsStateDefAsset* Asset = AssetPtr.LoadSynchronous();
        if (Asset)
        {
            StateDefRegistry.Add(DefId, Asset);

            if (Asset->StateTag.IsValid())
            {
                StateTagToName.Add(Asset->StateTag, DefId);
            }
        }
    }
}

void UTcsStateManagerSubsystem::PreloadCommonStates()
{
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();

    // 仅加载常用路径下的 State
    for (const auto& Pair : Settings->CachedStateDefinitions)
    {
        const TSoftObjectPtr<UTcsStateDefAsset>& AssetPtr = Pair.Value;
        FString AssetPath = AssetPtr.ToSoftObjectPath().ToString();

        // 检查是否在常用路径中
        bool bIsCommon = false;
        for (const FDirectoryPath& CommonPath : Settings->CommonStateDefinitionPaths)
        {
            if (AssetPath.StartsWith(CommonPath.Path))
            {
                bIsCommon = true;
                break;
            }
        }

        if (bIsCommon)
        {
            UTcsStateDefAsset* Asset = AssetPtr.LoadSynchronous();
            if (Asset)
            {
                StateDefRegistry.Add(Pair.Key, Asset);

                if (Asset->StateTag.IsValid())
                {
                    StateTagToName.Add(Asset->StateTag, Pair.Key);
                }
            }
        }
    }
}

UTcsStateDefAsset* UTcsStateManagerSubsystem::LoadStateOnDemand(FName StateDefId)
{
    // 检查是否已加载
    if (UTcsStateDefAsset* const* AssetPtr = StateDefRegistry.Find(StateDefId))
    {
        return *AssetPtr;
    }

    // 从 Settings 缓存中查找并加载
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (const TSoftObjectPtr<UTcsStateDefAsset>* AssetPtr = Settings->CachedStateDefinitions.Find(StateDefId))
    {
        UTcsStateDefAsset* Asset = AssetPtr->LoadSynchronous();
        if (Asset)
        {
            StateDefRegistry.Add(StateDefId, Asset);

            if (Asset->StateTag.IsValid())
            {
                StateTagToName.Add(Asset->StateTag, StateDefId);
            }

            return Asset;
        }
    }

    return nullptr;
}

UTcsStateDefAsset* UTcsStateManagerSubsystem::GetStateDefAsset(FName StateDefId) const
{
    if (UTcsStateDefAsset* const* AssetPtr = StateDefRegistry.Find(StateDefId))
    {
        return *AssetPtr;
    }

    // 按需加载模式下尝试加载
    const UTcsDeveloperSettings* Settings = GetDefault<UTcsDeveloperSettings>();
    if (Settings->StateLoadingStrategy == ETcsStateDefLoadingStrategy::OnDemand ||
        Settings->StateLoadingStrategy == ETcsStateDefLoadingStrategy::Hybrid)
    {
        return const_cast<UTcsStateManagerSubsystem*>(this)->LoadStateOnDemand(StateDefId);
    }

    return nullptr;
}

UTcsStateDefAsset* UTcsStateManagerSubsystem::GetStateDefAssetByTag(FGameplayTag StateTag) const
{
    if (const FName* DefId = StateTagToName.Find(StateTag))
    {
        return GetStateDefAsset(*DefId);
    }

    return nullptr;
}

UTcsStateSlotDefAsset* UTcsStateManagerSubsystem::GetStateSlotDefAsset(FName StateSlotDefId) const
{
    if (UTcsStateSlotDefAsset* const* AssetPtr = StateSlotDefRegistry.Find(StateSlotDefId))
    {
        return *AssetPtr;
    }

    UE_LOG(LogTcsState, Warning, TEXT("[%s] StateSlotDefAsset not found: %s"),
        *FString(__FUNCTION__), *StateSlotDefId.ToString());
    return nullptr;
}

UTcsStateSlotDefAsset* UTcsStateManagerSubsystem::GetStateSlotDefAssetByTag(FGameplayTag StateSlotTag) const
{
    if (const FName* DefId = StateSlotTagToName.Find(StateSlotTag))
    {
        return GetStateSlotDefAsset(*DefId);
    }

    return nullptr;
}

TArray<FName> UTcsStateManagerSubsystem::GetAllStateSlotNames() const
{
    TArray<FName> Names;
    StateSlotDefRegistry.GetKeys(Names);
    return Names;
}
```

## FTcsAttributeInstance 重构

### 旧设计的问题

在旧的 DataTable 方案中，`FTcsAttributeInstance` 存储完整的定义结构体：

```cpp
USTRUCT(BlueprintType)
struct FTcsAttributeInstance
{
    GENERATED_BODY()

    /** 属性定义（完整结构体） */
    UPROPERTY(BlueprintReadOnly)
    FTcsAttributeDefinition AttributeDef;

    /** 当前值 */
    UPROPERTY(BlueprintReadOnly)
    float CurrentValue;

    /** 基础值 */
    UPROPERTY(BlueprintReadOnly)
    float BaseValue;
};
```

**问题**：
- 每个实例都存储完整的定义数据，造成内存浪费
- 定义数据无法在运行时更新（已经复制到实例中）
- 序列化时会保存大量冗余数据

### 新设计

改为混合方案：运行时使用指针缓存，序列化使用 ID：

```cpp
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeInstance
{
    GENERATED_BODY()

    // ========== 运行时使用（不序列化） ==========

    /** 属性定义资产（运行时缓存，不序列化） */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Attribute")
    UTcsAttributeDefinitionAsset* AttributeDef = nullptr;

    // ========== 序列化使用 ==========

    /** 属性定义 ID（用于序列化和网络同步，插件不强制存档策略） */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    FName AttributeDefId;

    /** 当前值 */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    float CurrentValue = 0.0f;

    /** 基础值 */
    UPROPERTY(BlueprintReadOnly, Category = "Attribute")
    float BaseValue = 0.0f;

    // ========== 辅助方法 ==========

    /**
     * 获取属性定义资产（纯粹的 Get，只读）
     * @return 缓存的属性定义资产指针，如果未加载则返回 nullptr
     */
    UTcsAttributeDefinitionAsset* GetAttributeDefAsset() const
    {
        return AttributeDef;
    }

    /**
     * 加载属性定义资产并缓存
     * 如果缓存已存在，不会重复加载
     * @param World 用于获取 Subsystem 的 World 对象
     */
    void LoadAttributeDefAsset(UWorld* World)
    {
        // 如果已缓存，直接返回
        if (AttributeDef)
        {
            return;
        }

        // 从 Subsystem 查找并缓存
        if (!World)
        {
            return;
        }

        UGameInstance* GameInstance = World->GetGameInstance();
        if (!GameInstance)
        {
            return;
        }

        UTcsAttributeManagerSubsystem* Subsystem = GameInstance->GetSubsystem<UTcsAttributeManagerSubsystem>();
        if (!Subsystem)
        {
            return;
        }

        AttributeDef = Subsystem->GetAttributeDefAsset(AttributeDefId);
    }

    /**
     * 自定义序列化
     * 保存时：从 AttributeDef 同步 DefId
     * 加载时：只加载 DefId，AttributeDef 保持为 nullptr，在首次访问时自动加载
     */
    bool Serialize(FArchive& Ar)
    {
        // 保存时：确保 DefId 与 AttributeDef 同步
        if (Ar.IsSaving() && AttributeDef)
        {
            AttributeDefId = AttributeDef->AttributeDefId;
        }

        // 序列化字段
        Ar << AttributeDefId;
        Ar << CurrentValue;
        Ar << BaseValue;

        // 加载时：AttributeDef 保持为 nullptr，在首次访问时自动加载
        // DefAsset 是固定资产，不需要手动刷新

        return true;
    }

    /**
     * 网络序列化
     * 保存时：同步 DefId
     * 加载时：只加载 DefId，AttributeDef 保持为 nullptr，在首次访问时自动加载
     */
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        // 保存时：确保 DefId 与 AttributeDef 同步
        if (Ar.IsSaving() && AttributeDef)
        {
            AttributeDefId = AttributeDef->AttributeDefId;
        }

        // 序列化 DefId
        Ar << AttributeDefId;
        Ar << CurrentValue;
        Ar << BaseValue;

        // 加载时：AttributeDef 保持为 nullptr，在首次访问时自动加载
        // DefAsset 是固定资产，不需要手动刷新

        bOutSuccess = true;
        return true;
    }

    /**
     * 获取属性名称（本地化）
     */
    FText GetAttributeName(UWorld* World)
    {
        // 确保已加载
        LoadAttributeDefAsset(World);

        if (AttributeDef)
        {
            return AttributeDef->AttributeName;
        }
        return FText::FromName(AttributeDefId);
    }

    /**
     * 获取属性图标
     */
    TSoftObjectPtr<UTexture2D> GetIcon(UWorld* World)
    {
        // 确保已加载
        LoadAttributeDefAsset(World);

        if (AttributeDef)
        {
            return AttributeDef->Icon;
        }
        return nullptr;
    }
};

// 启用自定义网络序列化
template<>
struct TStructOpsTypeTraits<FTcsAttributeInstance> : public TStructOpsTypeTraitsBase2<FTcsAttributeInstance>
{
    enum
    {
        WithNetSerializer = true,
    };
};
```

### 优势

1. **运行时性能最优**: 直接指针访问，无需查询（~1-5 ns vs ~10-50 ns）
2. **序列化开销小**: 只序列化 DefId（8 bytes）
3. **网络同步友好**: 传递 DefId，接收端重新查找
4. **数据一致性**: 所有实例共享同一个定义
5. **符合 UE5 最佳实践**: 参考 GameplayAbilities 的 FGameplayEffectSpec 设计

### 迁移影响

需要更新所有访问 `AttributeDef` 字段的代码：

**旧代码**：
```cpp
FText Name = AttributeInstance.AttributeDef.AttributeName;
```

**新代码（方式1 - 直接访问缓存）**：
```cpp
// 先加载，再访问
AttributeInstance.LoadAttributeDefAsset(GetWorld());
if (AttributeInstance.GetAttributeDefAsset())
{
    FText Name = AttributeInstance.GetAttributeDefAsset()->AttributeName;
}
```

**新代码（方式2 - 使用便捷方法，推荐）**：
```cpp
// 使用便捷方法（内部会自动调用 Load）
FText Name = AttributeInstance.GetAttributeName(GetWorld());
```

**关键点**：
- Get 和 Load 明确分离，职责清晰
- Get 函数纯粹只读，不修改状态
- Load 函数专门负责加载和缓存
- DefAsset 是固定资产，加载后不会改变
- 加载存档或网络同步后，需要显式调用 Load 函数加载缓存

## FTcsAttributeModifierInstance 重构

### 旧设计的问题

在旧的 DataTable 方案中，`FTcsAttributeModifierInstance` 也存储完整的定义结构体：

```cpp
USTRUCT(BlueprintType)
struct FTcsAttributeModifierInstance
{
    GENERATED_BODY()

    /** 修改器定义（完整结构体） */
    UPROPERTY(BlueprintReadOnly)
    FTcsAttributeModifierDefinition ModifierDef;

    // ... 其他字段
};
```

**问题**：
- 与 AttributeInstance 相同的问题：内存浪费、数据冗余、无法更新

### 新设计

采用与 FTcsAttributeInstance 相同的混合方案：

```cpp
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeModifierInstance
{
    GENERATED_BODY()

    // ========== 运行时使用（不序列化） ==========

    /** 修改器定义资产（运行时缓存，不序列化） */
    UPROPERTY(Transient, BlueprintReadOnly, Category = "Modifier")
    UTcsAttributeModifierDefinitionAsset* ModifierDef = nullptr;

    // ========== 序列化使用 ==========

    /** 修改器定义 ID（用于序列化和网络同步，插件不强制存档策略） */
    UPROPERTY(BlueprintReadOnly, Category = "Modifier")
    FName ModifierDefId;

    // ... 其他字段（ModifierInstId, SourceHandle 等）

    // ========== 辅助方法 ==========

    /**
     * 获取修改器定义资产（纯粹的 Get，只读）
     * @return 缓存的修改器定义资产指针，如果未加载则返回 nullptr
     */
    UTcsAttributeModifierDefinitionAsset* GetModifierDefAsset() const
    {
        return ModifierDef;
    }

    /**
     * 加载修改器定义资产并缓存
     * 如果缓存已存在，不会重复加载
     * @param World 用于获取 Subsystem 的 World 对象
     */
    void LoadModifierDefAsset(UWorld* World)
    {
        if (ModifierDef)
        {
            return;
        }

        if (!World)
        {
            return;
        }

        UGameInstance* GameInstance = World->GetGameInstance();
        if (!GameInstance)
        {
            return;
        }

        UTcsAttributeManagerSubsystem* Subsystem = GameInstance->GetSubsystem<UTcsAttributeManagerSubsystem>();
        if (!Subsystem)
        {
            return;
        }

        ModifierDef = Subsystem->GetAttributeModifierDefAsset(ModifierDefId);
    }

    /**
     * 自定义序列化
     */
    bool Serialize(FArchive& Ar)
    {
        if (Ar.IsSaving() && ModifierDef)
        {
            ModifierDefId = ModifierDef->AttributeModifierDefId;
        }

        Ar << ModifierDefId;
        // ... 序列化其他字段

        return true;
    }

    /**
     * 网络序列化
     */
    bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
    {
        if (Ar.IsSaving() && ModifierDef)
        {
            ModifierDefId = ModifierDef->AttributeModifierDefId;
        }

        Ar << ModifierDefId;
        // ... 序列化其他字段

        bOutSuccess = true;
        return true;
    }
};

// 启用自定义网络序列化
template<>
struct TStructOpsTypeTraits<FTcsAttributeModifierInstance> : public TStructOpsTypeTraitsBase2<FTcsAttributeModifierInstance>
{
    enum
    {
        WithNetSerializer = true,
    };
};
```

### 优势

与 FTcsAttributeInstance 相同的优势：
1. **运行时性能最优**: 直接指针访问，无需查询
2. **序列化开销小**: 只序列化 DefId（8 bytes）
3. **网络同步友好**: 传递 DefId，接收端显式调用 Load 加载
4. **数据一致性**: 所有实例共享同一个定义
5. **职责分离清晰**: Get 和 Load 明确分离

## UTcsStateInstance 重构

### 旧设计的问题

在旧的 DataTable 方案中，`UTcsStateInstance` 也存储完整的定义结构体：

```cpp
UCLASS(BlueprintType)
class UTcsStateInstance : public UObject
{
    GENERATED_BODY()

    /** 状态定义（完整结构体） */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FTcsStateDefinition StateDef;

    /** 状态定义Id */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FName StateId = NAME_None;

    // ... 其他字段
};
```

**问题**：
- 与 AttributeInstance 相同的问题：内存浪费、数据冗余、无法更新

### 新设计

由于 UTcsStateInstance 是 UObject，可以使用简化方案：

```cpp
UCLASS(BlueprintType)
class TIREFLYCOMBATSYSTEM_API UTcsStateInstance : public UObject
{
    GENERATED_BODY()

    // ========== 直接存储指针，UE 自动处理序列化 ==========

    /** 状态定义资产 */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    UTcsStateDefinitionAsset* StateDef = nullptr;

    /** 状态定义 ID（保留作为备用标识符） */
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FName StateId = NAME_None;

    // ... 其他字段（StateInstId, SourceHandle 等）

    // ========== 辅助方法 ==========

    /**
     * 获取状态定义资产
     * UObject 可以直接访问，无需复杂的缓存逻辑
     */
    UTcsStateDefinitionAsset* GetStateDefAsset() const
    {
        return StateDef;
    }

    /**
     * 设置状态定义资产
     * 同时更新 StateId 以保持一致性
     */
    void SetStateDefAsset(UTcsStateDefinitionAsset* InStateDef)
    {
        StateDef = InStateDef;
        if (StateDef)
        {
            StateId = StateDef->StateDefId;
        }
    }
};
```

### 为什么 UObject 可以简化

- **自动序列化**: UE 的序列化系统会自动处理 UObject 指针的序列化和反序列化
- **自动引用管理**: UE 的垃圾回收系统会自动管理 UObject 引用
- **无需 Transient + SaveGame**: 直接使用 `UPROPERTY()` 即可
- **无需自定义序列化**: 不需要实现 Serialize() 和 NetSerialize()

### 优势

1. **代码简洁**: 无需复杂的缓存和序列化逻辑
2. **自动管理**: UE 自动处理序列化和引用
3. **性能优秀**: 直接指针访问，无额外开销
4. **易于维护**: 符合 UE 标准模式

## 性能考虑

### 加载性能

**DataTable 方案**:
- 加载一个 DataTable 文件
- FindRow: O(1) 哈希查找

**DataAsset 方案**:
- 加载多个 DataAsset 文件
- TMap 查找: O(1) 哈希查找
- 支持灵活的加载策略（全部预加载、按需加载、混合模式）

**优化策略**:
- 使用 TSoftObjectPtr 延迟加载
- 在 Subsystem Initialize 时批量同步加载
- State 定义支持按需加载，减少启动时间
- 编辑器环境下使用 Asset Registry 缓存，避免重复扫描

### 内存占用

**DataTable 方案**:
- 所有定义在一个 UDataTable 对象中
- 每个 AttributeInstance 存储完整的定义结构体（~100 bytes）

**DataAsset 方案（混合模式）**:
- 每个定义是独立的 UObject（额外开销约 100-200 bytes/对象）
- 每个 AttributeInstance 存储：
  - FName ID（8 bytes，用于序列化）
  - 指针缓存（8 bytes，运行时使用，不序列化）
  - 总计：16 bytes（运行时），8 bytes（序列化）

**影响评估**:
- 假设 100 个属性定义，DataAsset 额外开销约 10-20 KB
- 假设 1000 个 AttributeInstance：
  - 运行时：节省约 84 KB 内存（100 bytes → 16 bytes）
  - 序列化：节省约 92 KB 存储（100 bytes → 8 bytes）
- 总体内存占用显著优化

### 查找性能

**DataTable 方案**:
- `FindRow(FName)` → O(1) 哈希查找 → 返回结构体副本
- 每次访问都需要查询：~10-50 ns

**DataAsset 方案（混合模式）**:
- 首次访问：`TMap::Find(FName)` → O(1) 哈希查找 → 缓存指针：~10-50 ns
- 后续访问：直接指针访问 → ~1-5 ns
- 性能提升：10-50x（对于频繁访问的属性）

## 测试策略

### 单元测试

1. **DataAsset 创建和序列化**
   - 测试 DataAsset 的创建、保存、加载
   - 验证字段值正确保存

2. **注册表功能**
   - 测试 Manager Subsystem 的注册和查找
   - 验证 DefId 冲突检测

3. **资产扫描机制**
   - 测试路径扫描功能
   - 验证缓存更新逻辑

### 集成测试

1. **运行时功能**
   - 启动游戏，验证 Subsystem 正常初始化
   - 测试属性创建、修改等核心功能
   - 确保无回归

2. **加载策略测试**
   - 测试 State 的三种加载策略
   - 验证按需加载的正确性

### 性能测试

1. **加载时间对比**
   - 测量 DataTable 方案的加载时间
   - 测量 DataAsset 方案的加载时间（不同策略）
   - 确保性能持平或更好

2. **查找性能对比**
   - 测量 FindRow 的性能
   - 测量 TMap 查找的性能
   - 确保 O(1) 复杂度

## 文档更新

### 需要更新的文档

1. **README.md**
   - 更新架构说明
   - 添加 DataAsset 使用指南

2. **CLAUDE.md**
   - 更新开发指南
   - 说明新的定义创建流程

3. **API 文档**
   - 添加新 API 的文档
   - 说明 DataAsset 的使用方式

## 风险缓解

### 性能回归风险

**风险**: 新方案可能导致性能下降

**缓解**:
- 实施前进行性能基准测试
- 提供多种加载策略以适应不同场景
- 保留性能监控代码
- 如果性能不达标，提供优化方案（如缓存、预加载）

### 兼容性风险

**风险**: 现有代码可能依赖特定的数据结构

**缓解**:
- 提供辅助方法简化 DataAsset 访问
- 充分的单元测试覆盖
- 详细的迁移文档和示例代码

### 编辑器工作流风险

**风险**: 路径配置可能导致资产找不到

**缓解**:
- 提供清晰的路径配置界面和提示
- 自动扫描和缓存机制减少手动配置
- Asset Registry 监听确保实时更新
- 编辑器启动时验证配置完整性

## 总结

这个设计方案：
- ✅ 使用 UPrimaryDataAsset，符合 UE5 最佳实践
- ✅ 自动扫描和监听机制，减少手动配置
- ✅ 灵活的加载策略，适应不同性能需求
- ✅ 内存优化（AttributeInstance 只存储 ID）
- ✅ 性能持平或更优，无明显开销
- ✅ 改善编辑体验，提升开发效率
- ✅ 为未来扩展（如蓝图子类）预留空间
- ✅ StateTag 映射支持，增强查询能力
