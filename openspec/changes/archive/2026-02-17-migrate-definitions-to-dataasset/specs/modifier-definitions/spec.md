# Spec: 属性修改器定义系统

## 概述

定义 AttributeModifierDefinitionAsset 的架构、字段设计和加载管理方式，用于统一管理属性修改器的定义数据。

## ADDED Requirements

### Requirement: 必须创建 AttributeModifierDefinitionAsset 类

系统 MUST 创建 `UTcsAttributeModifierDefinitionAsset` 类继承自 UPrimaryDataAsset。

**优先级**: P0 (Critical)

**理由**: 属性修改器是战斗系统的核心机制，需要独立的 DataAsset 类型进行管理，与 AttributeDef、StateDef 保持架构一致。

#### Scenario: AttributeModifierDefinitionAsset 类定义

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的类声明

**When**: 检查类的基本结构

**Then**:
- 继承自 `UPrimaryDataAsset`
- UCLASS 标记为 `BlueprintType, Const`
- 定义静态常量 `static const FName PrimaryAssetType;`（值为 "TcsAttributeModifierDef"）
- 重写 `GetPrimaryAssetId()` 方法
- 包含编辑器验证逻辑

#### Scenario: 在内容浏览器中创建资产

**Given**: 开发者需要定义一个新的属性修改器

**When**: 在内容浏览器中右键创建资产

**Then**:
- 可以看到 "Tirefly Combat System > Attribute Modifier Definition" 选项
- 创建的资产类型为 `UTcsAttributeModifierDefinitionAsset`
- 资产的 PrimaryAssetType 为 "TcsAttributeModifierDef"

---

### Requirement: AttributeModifierDefinitionAsset 必须包含核心字段

AttributeModifierDefinitionAsset MUST 包含以下核心字段用于定义修改器行为。

**优先级**: P0 (Critical)

**理由**: 这些字段定义了修改器的完整行为，是属性修改系统的基础。

#### Scenario: 定义 AttributeModifierDefId 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 AttributeModifierDefId 字段

**Then**:
- 字段类型为 `FName`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Identity"`
- ToolTip："属性修改器的唯一标识符，必须全局唯一"
- 用作注册表的 Key

#### Scenario: 定义 TargetAttributeDefId 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 TargetAttributeDefId 字段

**Then**:
- 字段类型为 `FName`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Target"`
- ToolTip："目标属性的 DefId，指定此修改器作用于哪个属性"
- 可以通过下拉列表选择已注册的属性 ID

#### Scenario: 定义 ModifierType 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 ModifierType 字段

**Then**:
- 字段类型为 `ETcsAttributeModifierType`（枚举）
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Modifier"`
- ToolTip："修改器类型：Additive（加法）、Multiplicative（乘法）、Override（覆盖）"
- 枚举值：
  - `Additive` - 加法修改（BaseValue + ModifierValue）
  - `Multiplicative` - 乘法修改（BaseValue * (1 + ModifierValue)）
  - `Override` - 覆盖修改（直接设置为 ModifierValue）

#### Scenario: 定义 BaseValue 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 BaseValue 字段

**Then**:
- 字段类型为 `float`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Modifier"`
- ToolTip："修改器的基础值。对于 Additive 类型，表示增加的数值；对于 Multiplicative 类型，表示百分比（0.1 = 10%）"
- 默认值为 0.0

#### Scenario: 定义 Priority 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 Priority 字段

**Then**:
- 字段类型为 `int32`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Modifier"`
- ToolTip："修改器的优先级，数值越大优先级越高。同一属性的多个修改器按优先级顺序应用"
- 默认值为 0

#### Scenario: 定义 Duration 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 Duration 字段

**Then**:
- 字段类型为 `float`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Lifetime"`
- ToolTip："修改器的持续时间（秒）。-1 表示永久生效，0 表示瞬时生效，>0 表示定时生效"
- 默认值为 -1.0（永久）

#### Scenario: 定义 bStackable 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 bStackable 字段

**Then**:
- 字段类型为 `bool`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Stacking"`
- ToolTip："是否允许堆叠。如果为 true，同一修改器可以多次应用；如果为 false，后续应用会覆盖或刷新现有修改器"
- 默认值为 false

#### Scenario: 定义 MaxStackCount 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 MaxStackCount 字段

**Then**:
- 字段类型为 `int32`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Stacking", meta=(EditCondition="bStackable")`
- ToolTip："最大堆叠层数。-1 表示无限堆叠，>0 表示最大堆叠数"
- 默认值为 -1（无限）
- 仅在 bStackable 为 true 时可编辑

#### Scenario: 定义 ModifierTag 字段

**Given**: 查看 `UTcsAttributeModifierDefinitionAsset` 的字段定义

**When**: 检查 ModifierTag 字段

**Then**:
- 字段类型为 `FGameplayTag`
- UPROPERTY 标记：`EditAnywhere, BlueprintReadOnly, Category="Tags"`
- ToolTip："修改器的 GameplayTag，用于标签查询和分类管理"
- 可选字段（可以为空）

---

### Requirement: AttributeModifierDefinitionAsset 必须实现 GetPrimaryAssetId

AttributeModifierDefinitionAsset MUST 重写 GetPrimaryAssetId() 方法返回基于 AttributeModifierDefId 的唯一资产标识符。

**优先级**: P0 (Critical)

**理由**: UPrimaryDataAsset 要求实现此方法，用于 Asset Manager 识别和管理资产。

#### Scenario: 返回正确的 AssetId

**Given**: 有一个 AttributeModifierDefId 为 "AttackPowerBoost" 的 AttributeModifierDefinitionAsset

**When**: 调用 `Asset->GetPrimaryAssetId()`

**Then**:
- 返回 `FPrimaryAssetId("TcsAttributeModifierDef", "AttackPowerBoost")`
- PrimaryAssetType 为 "TcsAttributeModifierDef"
- PrimaryAssetName 为 "AttackPowerBoost"（即 AttributeModifierDefId）

---

### Requirement: AttributeModifierDefinitionAsset 必须支持编辑器验证

AttributeModifierDefinitionAsset MUST 实现编辑器验证逻辑以在保存时检查配置错误。

**优先级**: P1 (High)

**理由**: 在编辑器中提前发现配置错误，避免运行时问题。

#### Scenario: 验证 AttributeModifierDefId 不为空

**Given**: 创建了一个 AttributeModifierDefinitionAsset 但未设置 AttributeModifierDefId

**When**: 保存资产或运行数据验证

**Then**:
- 编辑器显示验证错误："AttributeModifierDefId cannot be empty"
- 资产标记为无效

#### Scenario: 验证 TargetAttributeDefId 不为空

**Given**: 创建了一个 AttributeModifierDefinitionAsset 但未设置 TargetAttributeDefId

**When**: 保存资产或运行数据验证

**Then**:
- 编辑器显示验证错误："TargetAttributeDefId cannot be empty"
- 资产标记为无效

#### Scenario: 验证 MaxStackCount 的合理性

**Given**: 创建了一个 AttributeModifierDefinitionAsset，bStackable = true，MaxStackCount = 0

**When**: 保存资产或运行数据验证

**Then**:
- 编辑器显示验证警告："MaxStackCount should be -1 (unlimited) or > 0"
- 提示用户修正配置

---

### Requirement: DeveloperSettings 必须配置 AttributeModifier 定义路径

DeveloperSettings MUST 包含 `AttributeModifierDefinitionPaths` 字段用于配置修改器定义的搜索路径。

**优先级**: P0 (Critical)

**理由**: 与其他定义类型保持一致，使用路径配置 + 自动扫描机制。

#### Scenario: 在项目设置中配置路径

**Given**: 打开项目设置 > Game > Tirefly Combat System

**When**: 查看 "Attribute Modifier Definition Paths" 字段

**Then**:
- 字段类型为 `TArray<FDirectoryPath>`
- 可以添加多个路径，如 "/Game/TCS/Definitions/Modifiers"
- 字段有 ToolTip 说明："属性修改器定义资产的搜索路径，系统会自动扫描这些目录下的所有 AttributeModifierDefinitionAsset"
- 配置保存到 Config/DefaultTireflyCombatSystemSettings.ini

---

### Requirement: AttributeManagerSubsystem 必须加载和管理 AttributeModifier 定义

AttributeManagerSubsystem MUST 在 Initialize() 时扫描和加载所有 AttributeModifierDefinitionAsset。

**优先级**: P0 (Critical)

**理由**: 统一管理所有属性相关的定义，包括属性本身和属性修改器。

#### Scenario: 初始化时加载修改器定义

**Given**: 配置路径中有 10 个 AttributeModifier 定义

**When**: AttributeManagerSubsystem::Initialize() 被调用

**Then**:
- 扫描 AttributeModifierDefinitionPaths
- 使用 FAssetRegistryModule 查找所有 `UTcsAttributeModifierDefinitionAsset`
- 同步加载所有资产
- 将资产存入 `TMap<FName, UTcsAttributeModifierDefinitionAsset*> AttributeModifierDefRegistry`（Key 为 AttributeModifierDefId）
- 日志输出："[AttributeManagerSubsystem] Loaded 10 attribute modifier definitions"

#### Scenario: 提供查询 API

**Given**: 系统中注册了 "AttackPowerBoost" 修改器

**When**: 调用 `AttributeManagerSubsystem->GetAttributeModifierDefAsset(FName("AttackPowerBoost"))`

**Then**:
- 在 AttributeModifierDefRegistry 中查找 Key 为 "AttackPowerBoost" 的条目
- 返回 `UTcsAttributeModifierDefinitionAsset*`
- 如果找不到，返回 nullptr 并记录警告日志

#### Scenario: 获取所有修改器定义

**Given**: 系统中注册了多个修改器定义

**When**: 调用 `AttributeManagerSubsystem->GetAllAttributeModifierDefAssets()`

**Then**:
- 返回 `TArray<UTcsAttributeModifierDefinitionAsset*>`
- 包含所有已注册的修改器定义
- 数组顺序不保证

---

### Requirement: AttributeModifierDefinitionAsset 必须支持 ModifierTag 映射

AttributeManagerSubsystem MUST 构建 ModifierTag 到 AttributeModifierDefId 的映射。

**优先级**: P1 (High)

**理由**: 支持通过 GameplayTag 查找修改器定义，便于基于标签的查询和过滤。

#### Scenario: 初始化时构建 ModifierTagToName 映射

**Given**: 注册表中有以下修改器
- "AttackPowerBoost" (ModifierTag = "TCS.Modifier.AttackPowerBoost")
- "DefenseBoost" (ModifierTag = "TCS.Modifier.DefenseBoost")

**When**: AttributeManagerSubsystem 初始化

**Then**:
- `ModifierTagToName` 包含 2 个条目
- `ModifierTagToName[FGameplayTag("TCS.Modifier.AttackPowerBoost")]` = "AttackPowerBoost"
- `ModifierTagToName[FGameplayTag("TCS.Modifier.DefenseBoost")]` = "DefenseBoost"
- `ModifierNameToTag` 包含反向映射

#### Scenario: 通过 ModifierTag 查找修改器定义

**Given**: 系统中注册了多个修改器定义

**When**: 调用 `AttributeManagerSubsystem->GetAttributeModifierDefIdByTag(FGameplayTag("TCS.Modifier.AttackPowerBoost"))`

**Then**:
- 返回 "AttackPowerBoost" 的 AttributeModifierDefId
- 如果找不到，返回 NAME_None

#### Scenario: 处理重复的 ModifierTag

**Given**: 两个修改器有相同的 ModifierTag

**When**: AttributeManagerSubsystem 初始化

**Then**:
- 记录 Error 日志："Duplicate ModifierTag 'TCS.Modifier.AttackPowerBoost' found"
- 只保留第一个映射
- 第二个修改器的 Tag 映射被忽略

---

## MODIFIED Requirements

无（这是新增的定义类型，不修改现有需求）

---

## REMOVED Requirements

无（保持向后兼容，不移除现有功能）

---
