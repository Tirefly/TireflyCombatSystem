# Spec: 定义加载系统

## 概述

定义 Manager Subsystem 如何加载、注册和查询 DataAsset 定义，包括配置方式、注册表管理和加载策略。

## ADDED Requirements

### Requirement: DeveloperSettings 必须使用路径配置 + 自动扫描机制

DeveloperSettings MUST 使用 `TArray<FDirectoryPath>` 配置 DataAsset 路径，并通过 FAssetRegistryModule 自动扫描和监听资产变更。

**优先级**: P0 (Critical)

**理由**: 路径配置 + 自动扫描提供了更灵活的管理方式，避免手动维护 TMap，支持资产的动态添加和删除。

#### Scenario: 在项目设置中配置 Attribute 定义路径

**Given**: 打开项目设置 > Game > Tirefly Combat System

**When**: 查看 "Attribute Definition Paths" 字段

**Then**:
- 字段类型为 `TArray<FDirectoryPath>`
- 可以添加多个路径，如：
  - "/Game/TCS/Definitions/Attributes"
  - "/Game/Characters/Attributes"
- 字段有 ToolTip 说明："属性定义资产的搜索路径，系统会自动扫描这些目录下的所有 AttributeDefinitionAsset"
- 配置保存到 Config/DefaultTireflyCombatSystemSettings.ini

#### Scenario: 配置多个定义类型的路径

**Given**: 项目设置中的定义路径配置

**When**: 添加以下配置
- AttributeDefinitionPaths: ["/Game/TCS/Definitions/Attributes"]
- StateDefinitionPaths: ["/Game/TCS/Definitions/States"]
- StateSlotDefinitionPaths: ["/Game/TCS/Definitions/StateSlots"]
- AttributeModifierDefinitionPaths: ["/Game/TCS/Definitions/Modifiers"]

**Then**:
- 所有配置保存到 Config 文件
- 每个路径可以包含子目录
- 系统会递归扫描所有子目录

#### Scenario: 使用 AssetRegistry 自动扫描资产

**Given**: 配置了 AttributeDefinitionPaths = ["/Game/TCS/Definitions/Attributes"]

**When**: AttributeManagerSubsystem::Initialize() 被调用

**Then**:
- 使用 `FAssetRegistryModule::Get().GetAssetsByPath()` 扫描路径
- 过滤出 `UTcsAttributeDefinitionAsset` 类型的资产
- 加载所有找到的资产
- 将资产存入 `TMap<FName, UTcsAttributeDefinitionAsset*> AttributeDefRegistry`（Key 为 AttributeDefId）
- 日志输出："[AttributeManagerSubsystem] Scanned path '/Game/TCS/Definitions/Attributes', found 5 attribute definitions"

#### Scenario: 监听资产变更

**Given**: 系统已初始化并加载了所有定义

**When**: 开发者在编辑器中创建新的 AttributeDefinitionAsset 并保存到配置路径

**Then**:
- FAssetRegistryModule 触发 OnAssetAdded 事件
- AttributeManagerSubsystem 接收事件
- 自动加载新资产并添加到注册表
- 日志输出："[AttributeManagerSubsystem] New attribute definition added: 'NewAttribute'"

#### Scenario: 内部缓存 TMap 不暴露给编辑器

**Given**: AttributeManagerSubsystem 的实现

**When**: 查看类的成员变量

**Then**:
- 包含 `TMap<FName, TSoftObjectPtr<UTcsAttributeDefinitionAsset>> AttributeDefCache`（私有成员）
- 不使用 UPROPERTY 标记（不暴露给编辑器）
- 仅用于内部缓存和快速查找
- 在 Initialize() 时构建，在 Deinitialize() 时清空

---

### Requirement: Manager Subsystem 必须在 Initialize 时加载所有 DataAsset

Manager Subsystem MUST 在 Initialize() 方法中同步加载所有扫描到的 DataAsset。

**优先级**: P0 (Critical)

**理由**: 确保运行时所有定义都已加载，避免异步加载的复杂性。

#### Scenario: AttributeManagerSubsystem 初始化时加载定义

**Given**: 配置路径中有 5 个 Attribute 定义

**When**: 游戏启动，AttributeManagerSubsystem::Initialize() 被调用

**Then**:
- 扫描所有配置路径
- 遍历所有找到的 TSoftObjectPtr，调用 LoadSynchronous()
- 将加载的 DataAsset 存入 `TMap<FName, UTcsAttributeDefinitionAsset*> AttributeDefRegistry`
- 日志输出："[AttributeManagerSubsystem] Loaded 5 attribute definitions"
- 如果某个 DataAsset 加载失败，记录 Error 日志但继续加载其他

#### Scenario: StateManagerSubsystem 初始化时加载定义

**Given**: 配置路径中有 8 个 State 定义和 3 个 StateSlot 定义

**When**: 游戏启动，StateManagerSubsystem::Initialize() 被调用

**Then**:
- 加载所有 State 定义到 `StateDefRegistry`
- 加载所有 StateSlot 定义到 `StateSlotDefRegistry`
- 日志输出："[StateManagerSubsystem] Loaded 8 state definitions and 3 state slot definitions"
- 调用 `InitStateSlotDefs()` 初始化槽位

---

### Requirement: StateManagerSubsystem 必须支持灵活的加载策略

StateManagerSubsystem MUST 支持多种加载策略以优化内存和性能。

**优先级**: P1 (High)

**理由**: 状态定义数量可能很大，灵活的加载策略可以在内存占用和加载速度之间取得平衡。

#### Scenario: 定义加载策略枚举

**Given**: 查看 `ETcsStateDefLoadingStrategy` 枚举定义

**When**: 检查枚举值

**Then**:
- 包含 `PreloadAll` - 预加载所有状态定义
- 包含 `LoadOnDemand` - 按需加载状态定义
- 包含 `Hybrid` - 混合模式，预加载常用状态，其他按需加载

#### Scenario: 在 DeveloperSettings 中配置加载策略

**Given**: 打开项目设置 > Game > Tirefly Combat System

**When**: 查看 "State Definition Loading Strategy" 字段

**Then**:
- 字段类型为 `ETcsStateDefLoadingStrategy`
- 默认值为 `Hybrid`
- 字段有 ToolTip 说明："状态定义的加载策略。PreloadAll: 启动时加载所有；LoadOnDemand: 使用时加载；Hybrid: 预加载常用状态"

#### Scenario: PreloadAll 策略

**Given**: LoadingStrategy 设置为 `PreloadAll`

**When**: StateManagerSubsystem::Initialize() 被调用

**Then**:
- 扫描所有配置路径
- 同步加载所有状态定义
- 全部存入 StateDefRegistry
- 日志输出："[StateManagerSubsystem] PreloadAll strategy: Loaded 50 state definitions"

#### Scenario: LoadOnDemand 策略

**Given**: LoadingStrategy 设置为 `LoadOnDemand`

**When**: StateManagerSubsystem::Initialize() 被调用

**Then**:
- 扫描所有配置路径，但不加载资产
- 仅缓存 TSoftObjectPtr 到 StateDefCache
- 日志输出："[StateManagerSubsystem] LoadOnDemand strategy: Found 50 state definitions, will load on demand"

**When**: 代码调用 `GetStateDefAsset(FName("Stunned"))`

**Then**:
- 检查 StateDefRegistry，如果不存在则从 StateDefCache 加载
- 调用 LoadSynchronous() 加载资产
- 存入 StateDefRegistry 以便后续使用
- 返回加载的资产

#### Scenario: Hybrid 策略 - 配置常用状态路径

**Given**: LoadingStrategy 设置为 `Hybrid`

**When**: 在 DeveloperSettings 中配置 "Common State Definition Paths"

**Then**:
- 字段类型为 `TArray<FDirectoryPath>`
- 可以添加常用状态路径，如 "/Game/TCS/Definitions/States/Common"
- 字段有 ToolTip 说明："Hybrid 模式下预加载的常用状态路径"

#### Scenario: Hybrid 策略 - 加载行为

**Given**: LoadingStrategy 设置为 `Hybrid`，CommonStateDefinitionPaths = ["/Game/TCS/Definitions/States/Common"]

**When**: StateManagerSubsystem::Initialize() 被调用

**Then**:
- 扫描 CommonStateDefinitionPaths，同步加载所有状态（如 10 个）
- 扫描其他 StateDefinitionPaths，仅缓存 TSoftObjectPtr（如 40 个）
- 日志输出："[StateManagerSubsystem] Hybrid strategy: Preloaded 10 common states, cached 40 on-demand states"

---

### Requirement: Manager Subsystem 必须提供注册表查询 API

Manager Subsystem MUST 提供直接获取 DataAsset 的 API。

**优先级**: P0 (Critical)

**理由**: 提供统一的查询接口，隐藏内部实现细节。

#### Scenario: 获取 AttributeDefinitionAsset

**Given**: 系统中注册了 "Health" 属性

**When**: 调用 `AttributeManagerSubsystem->GetAttributeDefAsset(FName("Health"))`

**Then**:
- 在 AttributeDefRegistry 中查找 Key 为 "Health" 的条目
- 返回 `UTcsAttributeDefinitionAsset*`
- 如果找不到，返回 nullptr 并记录警告日志

#### Scenario: 获取 StateDefinitionAsset

**Given**: 系统中注册了 "Stunned" 状态

**When**: 调用 `StateManagerSubsystem->GetStateDefAsset(FName("Stunned"))`

**Then**:
- 在 StateDefRegistry 中查找 Key 为 "Stunned" 的条目
- 如果使用 LoadOnDemand 或 Hybrid 策略且未加载，则从 StateDefCache 加载
- 返回 `UTcsStateDefinitionAsset*`
- 如果找不到，返回 nullptr

#### Scenario: 获取所有 AttributeDefinitionAsset

**Given**: 系统中注册了多个属性

**When**: 调用 `AttributeManagerSubsystem->GetAllAttributeDefAssets()`

**Then**:
- 返回 `TArray<UTcsAttributeDefinitionAsset*>`
- 包含所有已注册的属性定义
- 数组顺序不保证

---

### Requirement: Manager Subsystem 必须构建 GameplayTag 映射

Manager Subsystem MUST 在初始化时构建 GameplayTag 到 DefinitionId 的双向映射。

**优先级**: P0 (Critical)

**理由**: 支持通过 GameplayTag 快速查找定义，用于技能系统和事件系统集成。

#### Scenario: 初始化时构建 AttributeTagToName 映射

**Given**: 注册表中有以下属性
- "Health" (AttributeTag = "TCS.Attribute.Health")
- "MaxHealth" (AttributeTag = "TCS.Attribute.MaxHealth")

**When**: AttributeManagerSubsystem 初始化

**Then**:
- `AttributeTagToName` 包含 2 个条目
- `AttributeTagToName[FGameplayTag("TCS.Attribute.Health")]` = "Health"
- `AttributeTagToName[FGameplayTag("TCS.Attribute.MaxHealth")]` = "MaxHealth"
- `AttributeNameToTag` 包含反向映射

#### Scenario: 初始化时构建 StateTagToName 映射

**Given**: 注册表中有以下状态
- "Stunned" (StateTag = "TCS.State.Stunned")
- "Frozen" (StateTag = "TCS.State.Frozen")

**When**: StateManagerSubsystem 初始化

**Then**:
- `StateTagToName` 包含 2 个条目
- `StateTagToName[FGameplayTag("TCS.State.Stunned")]` = "Stunned"
- `StateTagToName[FGameplayTag("TCS.State.Frozen")]` = "Frozen"
- `StateNameToTag` 包含反向映射

#### Scenario: 处理重复的 AttributeTag

**Given**: 两个属性有相同的 AttributeTag

**When**: AttributeManagerSubsystem 初始化

**Then**:
- 记录 Error 日志："Duplicate AttributeTag 'TCS.Attribute.Health' found"
- 只保留第一个映射
- 第二个属性的 Tag 映射被忽略

#### Scenario: 通过 GameplayTag 查找定义

**Given**: AttributeTagToName 映射已构建

**When**: 调用 `AttributeManagerSubsystem->GetAttributeDefIdByTag(FGameplayTag("TCS.Attribute.Health"))`

**Then**:
- 返回 "Health" (FName)
- 如果找不到，返回 NAME_None

---

### Requirement: Manager Subsystem 必须支持编辑器热重载

Manager Subsystem MUST 支持编辑器中资产的热重载和更新。

**优先级**: P1 (High)

**理由**: 提升开发效率，避免频繁重启编辑器。

#### Scenario: 监听资产修改事件

**Given**: 系统已初始化

**When**: 开发者在编辑器中修改 AttributeDefinitionAsset 并保存

**Then**:
- FAssetRegistryModule 触发 OnAssetUpdated 事件
- AttributeManagerSubsystem 接收事件
- 重新加载该资产
- 更新注册表中的条目
- 日志输出："[AttributeManagerSubsystem] Attribute definition 'Health' reloaded"

#### Scenario: 监听资产删除事件

**Given**: 系统已初始化

**When**: 开发者在编辑器中删除 AttributeDefinitionAsset

**Then**:
- FAssetRegistryModule 触发 OnAssetRemoved 事件
- AttributeManagerSubsystem 接收事件
- 从注册表中移除该条目
- 从 Tag 映射中移除相关条目
- 日志输出："[AttributeManagerSubsystem] Attribute definition 'Health' removed"

---

## MODIFIED Requirements

无（这是新的加载机制，不修改现有需求）

---

## REMOVED Requirements

### Requirement: DeveloperSettings 必须使用 TMap 存储 DataAsset 引用

**移除理由**: 改用路径配置 + 自动扫描机制，更灵活且易于维护。

**影响**:
- 移除 `TMap<FName, TSoftObjectPtr<DataAsset>>` 配置字段
- 改用 `TArray<FDirectoryPath>` 配置路径
- 内部使用 TMap 缓存，但不暴露给编辑器

**替代方案**: 使用 FAssetRegistryModule 自动扫描配置路径下的所有 DataAsset。

---

### Requirement: 保持现有 API 签名不变

**移除理由**: 不再保留结构体 API，全面迁移到 DataAsset API。

**影响**:
- 移除所有返回结构体的 API（如 `GetAttributeDefinition(FName, FTcsAttributeDefinition&)`）
- 只保留返回 DataAsset 的 API（如 `GetAttributeDefAsset(FName)`）

**替代方案**: 使用新的 DataAsset API，调用方需要更新代码。

---

### Requirement: 新增直接获取 DataAsset 的 API

**移除理由**: 因为只有 DataAsset API，不需要"新增"的概念。

**影响**:
- 不再区分"旧 API"和"新 API"
- 只有一套 DataAsset API

**替代方案**: 直接使用 DataAsset API，如 `GetAttributeDefAsset()`。

---

### Requirement: 移除 GetDefTable 相关方法

**扩展为**: 移除所有结构体相关 API

**理由**: 全面迁移到 DataAsset，不仅移除 GetDefTable，还移除所有结构体 API。

**影响**:
- 移除 `GetAttributeDefinition(FName, FTcsAttributeDefinition&)`
- 移除 `GetStateDefinition(FName, FTcsStateDefinition&)`
- 移除 `GetStateSlotDefinition(FName, FTcsStateSlotDefinition&)`
- 移除 `GetAttributeDefTable()`
- 移除 `GetStateDefTable()`

**替代方案**:
- 使用 `GetAttributeDefAsset(FName)` 返回 `UTcsAttributeDefinitionAsset*`
- 使用 `GetStateDefAsset(FName)` 返回 `UTcsStateDefinitionAsset*`
- 使用 `GetAllAttributeDefAssets()` 返回 `TArray<UTcsAttributeDefinitionAsset*>`

---

### Requirement: 支持 DataTable 的 FindRow 查找

**移除理由**: 不再使用 DataTable，改用注册表查找

**替代方案**: 使用 `GetAttributeDefAsset()` 等方法，内部使用 TMap 查找

---
