# Spec: DataAsset 架构

## 概述

定义 TCS 插件中核心定义系统的 DataAsset 架构，包括 AttributeDef、StateDef、StateSlotDef、AttributeModifierDef 的资产类设计和管理方式。

## ADDED Requirements

### Requirement: DataAsset 类必须继承 UPrimaryDataAsset

所有 DataAsset 类 MUST 继承自 UPrimaryDataAsset 以获得 Asset Manager 集成和异步加载支持。

**优先级**: P0 (Critical)

**理由**: UPrimaryDataAsset 提供了 Asset Manager 集成、异步加载支持和资产 ID 管理，是 UE5 推荐的数据资产基类。

#### Scenario: 创建 AttributeDefinitionAsset

**Given**: 开发者需要定义一个新的属性（如 Health）

**When**: 在内容浏览器中右键创建资产

**Then**:
- 可以看到 "Tirefly Combat System > Attribute Definition" 选项
- 创建的资产类型为 `UTcsAttributeDefinitionAsset`
- 资产继承自 `UPrimaryDataAsset`
- 资产的 PrimaryAssetType 为 "TcsAttributeDef"

#### Scenario: 创建 StateDefinitionAsset

**Given**: 开发者需要定义一个新的状态（如 Stunned）

**When**: 在内容浏览器中右键创建资产

**Then**:
- 可以看到 "Tirefly Combat System > State Definition" 选项
- 创建的资产类型为 `UTcsStateDefinitionAsset`
- 资产继承自 `UPrimaryDataAsset`
- 资产的 PrimaryAssetType 为 "TcsStateDef"

#### Scenario: 创建 StateSlotDefinitionAsset

**Given**: 开发者需要定义一个新的状态槽（如 Action 槽）

**When**: 在内容浏览器中右键创建资产

**Then**:
- 可以看到 "Tirefly Combat System > State Slot Definition" 选项
- 创建的资产类型为 `UTcsStateSlotDefinitionAsset`
- 资产继承自 `UPrimaryDataAsset`
- 资产的 PrimaryAssetType 为 "TcsStateSlotDef"

#### Scenario: 创建 AttributeModifierDefinitionAsset

**Given**: 开发者需要定义一个新的属性修改器（如 DamageModifier）

**When**: 在内容浏览器中右键创建资产

**Then**:
- 可以看到 "Tirefly Combat System > Attribute Modifier Definition" 选项
- 创建的资产类型为 `UTcsAttributeModifierDefinitionAsset`
- 资产继承自 `UPrimaryDataAsset`
- 资产的 PrimaryAssetType 为 "TcsAttributeModifierDef"

---

### Requirement: DataAsset 必须包含具体命名的 DefinitionId 字段

每个 DataAsset MUST 包含 FName 类型的具体命名的 DefinitionId 字段作为唯一标识符:
- AttributeDefinitionAsset 使用 `AttributeDefId`
- StateDefinitionAsset 使用 `StateDefId`
- StateSlotDefinitionAsset 使用 `StateSlotDefId`
- AttributeModifierDefinitionAsset 使用 `AttributeModifierDefId`

**优先级**: P0 (Critical)

**理由**: 使用具体命名的 ID 字段提高代码可读性和类型安全性，对应原 DataTable 的 RowName，是查找和引用定义的主键。

#### Scenario: 设置 AttributeDefinitionAsset 的 ID

**Given**: 创建了一个新的 AttributeDefinitionAsset

**When**: 在细节面板中编辑资产

**Then**:
- 可以看到 "Identity" 分类
- 其中有 "Attribute Def Id" 字段（类型为 FName）
- 字段有 ToolTip 说明："属性的唯一标识符，必须全局唯一"
- 可以输入如 "Health"、"MaxHealth" 等标识符

#### Scenario: AttributeDefId 用于运行时查找

**Given**: 系统中注册了多个 AttributeDefinitionAsset

**When**: 代码调用 `AttributeManagerSubsystem->GetAttributeDefAsset(FName("Health"))`

**Then**:
- 系统使用 "Health" 作为 Key 在注册表中查找
- 返回对应的 AttributeDefinitionAsset
- 如果找不到，返回 nullptr 并记录警告日志

#### Scenario: StateDefId 用于运行时查找

**Given**: 系统中注册了多个 StateDefinitionAsset

**When**: 代码调用 `StateManagerSubsystem->GetStateDefAsset(FName("Stunned"))`

**Then**:
- 系统使用 "Stunned" 作为 Key 在注册表中查找
- 返回对应的 StateDefinitionAsset
- 查找时间复杂度为 O(1)（使用 TMap）

---

### Requirement: DataAsset 必须定义 PrimaryAssetType 静态变量

每个 DataAsset 类 MUST 定义静态常量 PrimaryAssetType 用于 Asset Manager 识别。

**优先级**: P0 (Critical)

**理由**: 统一管理 PrimaryAssetType 字符串，避免硬编码，便于维护和查询。

#### Scenario: AttributeDefinitionAsset 定义 PrimaryAssetType

**Given**: 查看 `UTcsAttributeDefinitionAsset` 类定义

**When**: 检查类的静态成员

**Then**:
- 包含 `static const FPrimaryAssetType PrimaryAssetType;`
- 虽然 FPrimaryAssetType 是 FName 的 typedef,但使用 FPrimaryAssetType 更语义化
- 在 cpp 文件中定义为 `const FPrimaryAssetType UTcsAttributeDefinitionAsset::PrimaryAssetType = FPrimaryAssetType(TEXT("TcsAttributeDef"));`
- GetPrimaryAssetId() 使用此静态变量

#### Scenario: 使用 PrimaryAssetType 查询资产

**Given**: 需要查询所有 Attribute 定义资产

**When**: 调用 `AssetManager->GetPrimaryAssetIdList(UTcsAttributeDefinitionAsset::PrimaryAssetType, OutAssetIds)`

**Then**:
- 返回所有 AttributeDefinitionAsset 的 AssetId 列表
- 可以遍历列表加载所有资产

---

### Requirement: DataAsset 字段必须与原结构体保持一致

DataAsset 的字段 MUST 与原结构体完全一致，包括字段名称、类型和元数据。

**优先级**: P0 (Critical)

**理由**: 保持字段一致性确保数据迁移无损，且便于维护和理解。

#### Scenario: AttributeDefinitionAsset 包含所有原字段

**Given**: 原 `FTcsAttributeDefinition` 结构体有 15 个字段

**When**: 查看 `UTcsAttributeDefinitionAsset` 的定义

**Then**:
- 包含所有 15 个字段（除了 FTableRowBase 继承）
- 字段名称完全一致（如 AttributeRange, ClampStrategyClass 等）
- 字段类型完全一致
- 字段的 UPROPERTY 元数据保持一致（EditCondition, Categories 等）
- 所有 UPROPERTY 包含 ToolTip 元数据说明字段用途

#### Scenario: StateDefinitionAsset 包含所有原字段

**Given**: 原 `FTcsStateDefinition` 结构体有约 30 个字段

**When**: 查看 `UTcsStateDefinitionAsset` 的定义

**Then**:
- 包含所有字段（除了 FTableRowBase 继承）
- 字段名称、类型、元数据完全一致
- 复杂字段如 `TArray<FTcsStateParameter>` 正确保留
- 所有 UPROPERTY 包含 ToolTip 元数据

---

### Requirement: StateDefinitionAsset 必须包含 StateTag 字段

StateDefinitionAsset MUST 包含 FGameplayTag 类型的 StateTag 字段用于 GameplayTag 映射。

**优先级**: P0 (Critical)

**理由**: 与 AttributeTag 保持一致，支持通过 GameplayTag 查找状态定义。

#### Scenario: 设置 StateDefinitionAsset 的 StateTag

**Given**: 创建了一个 StateDefinitionAsset，StateDefId 为 "Stunned"

**When**: 在细节面板中编辑资产

**Then**:
- 可以看到 "State Tag" 字段（类型为 FGameplayTag）
- 字段有 ToolTip 说明："状态的 GameplayTag，用于标签查询和事件触发"
- 可以选择如 "TCS.State.Stunned" 等标签
- 字段可选（某些状态可能不需要 Tag）

#### Scenario: 通过 StateTag 查找状态定义

**Given**: 系统中注册了多个 StateDefinitionAsset

**When**: 代码调用 `StateManagerSubsystem->GetStateDefIdByTag(FGameplayTag("TCS.State.Stunned"))`

**Then**:
- 返回 "Stunned" 的 StateDefId
- 如果找不到，返回 NAME_None

---

### Requirement: 必须创建 AttributeModifierDefinitionAsset

系统 MUST 创建 AttributeModifierDefinitionAsset 类用于定义属性修改器。

**优先级**: P0 (Critical)

**理由**: 属性修改器是战斗系统的核心机制，需要独立的 DataAsset 类型进行管理。

#### Scenario: AttributeModifierDefinitionAsset 包含核心字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的定义

**When**: 检查类的字段

**Then**:
- 包含 `AttributeModifierDefId` (FName) - 唯一标识符
- 包含 `TargetAttributeDefId` (FName) - 目标属性 ID
- 包含 `ModifierType` (ETcsAttributeModifierType) - 修改器类型（加法、乘法等）
- 包含 `BaseValue` (float) - 基础值
- 包含 `Priority` (int32) - 优先级
- 包含 `Duration` (float) - 持续时间（-1 表示永久）
- 包含 `bStackable` (bool) - 是否可堆叠
- 所有字段包含 ToolTip 元数据

#### Scenario: 创建伤害修改器资产

**Given**: 需要定义一个增加 10% 攻击力的修改器

**When**: 创建 AttributeModifierDefinitionAsset 并配置

**Then**:
- AttributeModifierDefId = "AttackPowerBoost"
- TargetAttributeDefId = "AttackPower"
- ModifierType = Multiplicative
- BaseValue = 0.1 (10%)
- Duration = -1 (永久)
- bStackable = true

---

### Requirement: DataAsset 必须标记为 Const

DataAsset 类 MUST 在 UCLASS 宏中标记为 Const 以防止运行时修改。

**优先级**: P1 (High)

**理由**: 定义数据在运行时不应被修改，标记为 Const 提供编译期保护。

#### Scenario: DataAsset 类声明为 Const

**Given**: 查看 DataAsset 类的 UCLASS 宏

**When**: 检查类声明

**Then**:
- `UCLASS(BlueprintType, Const)` 包含 Const 标记
- 运行时无法修改 DataAsset 的字段
- 编辑器中可以正常编辑（编辑器模式不受影响）

---

### Requirement: DataAsset 必须支持编辑器验证

DataAsset 类 MUST 实现编辑器验证逻辑以在保存时检查配置错误。

**优先级**: P1 (High)

**理由**: 在编辑器中提前发现配置错误，避免运行时问题。

#### Scenario: 验证 DefinitionId 不为空

**Given**: 创建了一个 AttributeDefinitionAsset 但未设置 AttributeDefId

**When**: 保存资产或运行数据验证

**Then**:
- 编辑器显示验证错误："AttributeDefId cannot be empty"
- 资产标记为无效
- 无法通过验证

#### Scenario: 验证 Skill 状态必须有 StateSlotType

**Given**: 创建了一个 StateDefinitionAsset，StateType 为 Skill

**When**: StateSlotType 为空时保存资产

**Then**:
- 编辑器显示验证错误："Skill state must define StateSlotType"
- 资产标记为无效
- 提示用户设置 StateSlotType

#### Scenario: 验证 AttributeModifier 的目标属性存在

**Given**: 创建了一个 AttributeModifierDefinitionAsset

**When**: TargetAttributeDefId 指向不存在的属性

**Then**:
- 编辑器显示验证警告："Target attribute 'XXX' not found in registry"
- 资产可以保存但标记为有警告
- 提示用户检查目标属性配置

---

### Requirement: DataAsset 必须实现 GetPrimaryAssetId

每个 DataAsset 类 MUST 重写 GetPrimaryAssetId() 方法返回基于 DefinitionId 的唯一资产标识符。

**优先级**: P0 (Critical)

**理由**: UPrimaryDataAsset 要求实现此方法，用于 Asset Manager 识别和管理资产。

#### Scenario: AttributeDefinitionAsset 返回正确的 AssetId

**Given**: 有一个 AttributeDefId 为 "Health" 的 AttributeDefinitionAsset

**When**: 调用 `Asset->GetPrimaryAssetId()`

**Then**:
- 返回 `FPrimaryAssetId("TcsAttributeDef", "Health")`
- PrimaryAssetType 为 "TcsAttributeDef"
- PrimaryAssetName 为 "Health"（即 AttributeDefId）

#### Scenario: StateDefinitionAsset 返回正确的 AssetId

**Given**: 有一个 StateDefId 为 "Stunned" 的 StateDefinitionAsset

**When**: 调用 `Asset->GetPrimaryAssetId()`

**Then**:
- 返回 `FPrimaryAssetId("TcsStateDef", "Stunned")`
- PrimaryAssetType 为 "TcsStateDef"
- PrimaryAssetName 为 "Stunned"

#### Scenario: AttributeModifierDefinitionAsset 返回正确的 AssetId

**Given**: 有一个 AttributeModifierDefId 为 "AttackBoost" 的 AttributeModifierDefinitionAsset

**When**: 调用 `Asset->GetPrimaryAssetId()`

**Then**:
- 返回 `FPrimaryAssetId("TcsAttributeModifierDef", "AttackBoost")`
- PrimaryAssetType 为 "TcsAttributeModifierDef"
- PrimaryAssetName 为 "AttackBoost"

---

### Requirement: 所有 UPROPERTY 必须包含 ToolTip 元数据

所有 DataAsset 的 UPROPERTY 字段 MUST 包含 ToolTip 元数据说明字段用途。

**优先级**: P1 (High)

**理由**: 提高编辑器可用性，帮助开发者理解每个字段的作用。

#### Scenario: AttributeDefinitionAsset 字段包含 ToolTip

**Given**: 查看 AttributeDefinitionAsset 的字段定义

**When**: 检查 UPROPERTY 宏

**Then**:
- 每个字段都有 `meta=(ToolTip="...")` 元数据
- ToolTip 清晰描述字段用途
- 示例：`UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Identity", meta=(ToolTip="属性的唯一标识符，必须全局唯一"))`

#### Scenario: 编辑器中显示 ToolTip

**Given**: 在编辑器中编辑 AttributeDefinitionAsset

**When**: 鼠标悬停在字段名称上

**Then**:
- 显示 ToolTip 提示信息
- 帮助开发者理解字段含义

---

### Requirement: FTcsAttributeInstance 必须使用混合方案

FTcsAttributeInstance MUST 使用混合方案：运行时使用指针缓存,序列化使用 DefId。

**优先级**: P0 (Critical)

**理由**: 平衡运行时性能和序列化开销,符合 UE5 最佳实践(参考 GameplayAbilities 系统)。

#### Scenario: FTcsAttributeInstance 包含运行时缓存和序列化字段

**Given**: 查看 FTcsAttributeInstance 的定义

**When**: 检查结构体字段

**Then**:
- 包含 `UPROPERTY(Transient) UTcsAttributeDefinitionAsset* AttributeDef` - 运行时缓存
- 包含 `UPROPERTY() FName AttributeDefId` - 序列化使用（插件不强制存档策略）
- AttributeDef 不参与序列化(Transient)
- AttributeDefId 参与序列化

#### Scenario: 运行时访问定义无需查询

**Given**: 有一个 AttributeInstance,AttributeDef 已缓存

**When**: 代码访问 `AttributeInstance.GetAttributeDefAsset()`

**Then**:
- 直接返回缓存的 AttributeDef 指针
- 无需查询 Subsystem
- 访问时间 ~1-5 ns(指针解引用)

#### Scenario: 首次访问时需要显式加载

**Given**: 有一个 AttributeInstance,AttributeDef 为 nullptr(刚加载存档)

**When**: 代码调用 `AttributeInstance.LoadAttributeDefAsset(World)`

**Then**:
- 从 AttributeManagerSubsystem 查询 AttributeDefId
- 缓存查询结果到 AttributeDef
- 后续通过 GetAttributeDefAsset() 直接访问缓存

#### Scenario: 序列化时只保存 DefId

**Given**: 需要保存游戏存档

**When**: 序列化 AttributeInstance

**Then**:
- 只序列化 AttributeDefId(8 bytes)
- AttributeDef 不序列化(Transient)
- 序列化开销最小

#### Scenario: 加载存档后显式加载

**Given**: 加载了游戏存档

**When**: 调用 `AttributeInstance.LoadAttributeDefAsset(World)`

**Then**:
- 如果 AttributeDef 缓存为空，从 AttributeDefId 查找并缓存
- 如果缓存已存在，不会重复加载
- 后续通过 GetAttributeDefAsset() 直接访问缓存
- DefAsset 是固定资产，加载后不会改变

#### Scenario: 网络同步只传输 DefId

**Given**: 需要网络同步 AttributeInstance

**When**: 执行网络序列化

**Then**:
- 只传输 AttributeDefId(8 bytes)
- 接收端需要显式调用 LoadAttributeDefAsset() 加载 AttributeDef
- 网络开销最小

---

### Requirement: FTcsAttributeModifierInstance 必须使用混合方案

FTcsAttributeModifierInstance MUST 使用混合方案：运行时使用指针缓存,序列化使用 DefId。

**优先级**: P0 (Critical)

**理由**: 与 FTcsAttributeInstance 保持一致的架构,平衡运行时性能和序列化开销。

#### Scenario: FTcsAttributeModifierInstance 包含运行时缓存和序列化字段

**Given**: 查看 FTcsAttributeModifierInstance 的定义

**When**: 检查结构体字段

**Then**:
- 包含 `UPROPERTY(Transient) UTcsAttributeModifierDefinitionAsset* ModifierDef` - 运行时缓存
- 包含 `UPROPERTY() FName ModifierDefId` - 序列化使用（插件不强制存档策略）
- ModifierDef 不参与序列化(Transient)
- ModifierDefId 参与序列化

#### Scenario: 运行时访问定义无需查询

**Given**: 有一个 ModifierInstance,ModifierDef 已缓存

**When**: 代码访问 `ModifierInstance.GetModifierDefAsset()`

**Then**:
- 直接返回缓存的 ModifierDef 指针
- 无需查询 Subsystem
- 访问时间 ~1-5 ns(指针解引用)

#### Scenario: 首次访问时需要显式加载

**Given**: 有一个 ModifierInstance,ModifierDef 为 nullptr(刚加载存档)

**When**: 代码调用 `ModifierInstance.LoadModifierDefAsset(World)`

**Then**:
- 从 AttributeManagerSubsystem 查询 ModifierDefId
- 缓存查询结果到 ModifierDef
- 后续通过 GetModifierDefAsset() 直接访问缓存

#### Scenario: 序列化时只保存 DefId

**Given**: 需要保存游戏存档

**When**: 序列化 ModifierInstance

**Then**:
- 只序列化 ModifierDefId(8 bytes)
- ModifierDef 不序列化(Transient)
- 序列化开销最小

#### Scenario: 网络同步只传输 DefId

**Given**: 需要网络同步 ModifierInstance

**When**: 执行网络序列化

**Then**:
- 只传输 ModifierDefId(8 bytes)
- 接收端需要显式调用 LoadModifierDefAsset() 加载 ModifierDef
- 网络开销最小

---

### Requirement: UTcsStateInstance 必须使用简化方案

UTcsStateInstance MUST 直接存储 DataAsset 指针,利用 UObject 自动序列化机制。

**优先级**: P0 (Critical)

**理由**: UTcsStateInstance 是 UObject,可以利用 UE 自动序列化机制,无需手动管理缓存和序列化。

#### Scenario: UTcsStateInstance 直接存储 DataAsset 指针

**Given**: 查看 UTcsStateInstance 的定义

**When**: 检查类字段

**Then**:
- 包含 `UPROPERTY(BlueprintReadOnly, Category="State") UTcsStateDefinitionAsset* StateDef` - 直接存储指针
- 包含 `UPROPERTY(BlueprintReadOnly, Category="State") FName StateId` - 保留作为备用标识符
- 不需要 Transient 标记(UObject 自动处理)
- 不需要 SaveGame 标记(UObject 自动处理)

#### Scenario: UObject 自动序列化

**Given**: 需要保存游戏存档

**When**: 序列化 StateInstance

**Then**:
- UE 自动序列化 StateDef 指针
- 自动处理引用关系
- 无需手动实现 Serialize() 方法

#### Scenario: 运行时直接访问

**Given**: 有一个 StateInstance

**When**: 代码访问 `StateInstance->GetStateDefAsset()`

**Then**:
- 直接返回 StateDef 指针
- 无需查询 Subsystem
- 无需缓存管理

#### Scenario: 网络同步自动处理

**Given**: 需要网络同步 StateInstance

**When**: 执行网络序列化

**Then**:
- UE 自动处理 UObject 指针的网络同步
- 自动维护引用关系
- 无需手动实现 NetSerialize() 方法

#### Scenario: 设置定义资产时同步更新 StateId

**Given**: 需要设置 StateInstance 的定义

**When**: 调用 `StateInstance->SetStateDefAsset(NewStateDef)`

**Then**:
- 设置 StateDef 指针
- 自动从 StateDef 获取 StateDefId 并更新 StateId
- 保持 StateDef 和 StateId 的一致性

---

## MODIFIED Requirements

无（这是新增的架构，不修改现有需求）

---

## REMOVED Requirements

### Requirement: DataAsset 必须提供转换方法

**移除理由**: 不再保留结构体 API，直接使用 DataAsset，无需转换方法。

**影响**:
- 移除 `ToStructDefinition()` 方法
- 移除 `FTcsAttributeDefinition`、`FTcsStateDefinition` 等结构体的 API
- 所有 API 直接返回 DataAsset 指针

**替代方案**: 使用 `GetAttributeDefAsset()` 等方法直接获取 DataAsset。

---
