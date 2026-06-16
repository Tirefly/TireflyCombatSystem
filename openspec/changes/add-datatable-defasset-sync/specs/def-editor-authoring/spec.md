# def-editor-authoring Spec Delta

## ADDED Requirements

### Requirement: DataTable ↔ DefAsset 双向自动同步

TCS 编辑器集成 SHALL 支持通过 DeveloperSettings 启用的 DataTable ↔ DefAsset 自动同步机制。保存 DataTable 时自动创建/更新对应目录下的 DefAsset；保存 DefAsset 时自动回写对应 DataTable 行。每个 DefAsset 类型独立配置，一个基路径对应一组同步。

#### Scenario: 保存 DataTable 后自动创建 DefAsset
- **WHEN** 策划在 DataTable 中新增一行（RowName = "ATTR_NewAttack"），保存 DataTable
- **THEN** 对应子目录下自动创建 `UTcsAttributeDefinition` 资产，属性值从 DataTable 行映射
- **AND** 创建的资产立即与 AssetManager 兼容

#### Scenario: 保存 DataTable 后自动更新已有 DefAsset
- **WHEN** 策划修改 DataTable 中已有行的属性值，保存 DataTable
- **THEN** 对应的已存在 DefAsset 被更新并标记为脏

#### Scenario: 保存 DefAsset 后自动回写 DataTable
- **WHEN** 策划双击 DefAsset 修改属性并保存
- **THEN** 对应 DataTable 中的行被更新并标记为脏

#### Scenario: 双向同步防止无限循环
- **WHEN** DataTable 保存触发 DefAsset 更新，后者保存再次触发 OnPackageSaved
- **THEN** Subsystem 通过 `bIsSyncing` 守卫阻止递归

#### Scenario: 每个子目录自动对应一个 DataTable
- **WHEN** DeveloperSettings 配置 BasePath="/TCS/AttributeDefs"
- **THEN** 子目录 `/TCS/AttributeDefs/Common/` 自动对应 `DT_TCS_AttributeDefs_Common`

#### Scenario: 孤立 DefAsset 被清理
- **WHEN** DataTable 中已删除某行，但子目录中仍存在对应的 DefAsset
- **THEN** 同步操作时该孤立 DefAsset 被标记删除

### Requirement: Row Struct 直接赋值映射

Row Struct 与 DefAsset 的 UPROPERTY SHALL 保持类型和名称 1:1 一致。映射通过每种类型各一对编译期静态 `SyncRowToAsset` / `SyncAssetToRow` 函数直接赋值，不使用运行时反射。

#### Scenario: 直接赋值编译期校验完整性
- **WHEN** DefAsset 新增或删除一个 UPROPERTY 字段
- **THEN** 对应的 Sync 函数编译报错，强制同步更新 Row Struct

#### Scenario: 嵌套结构体在 DataTable 中自动展开
- **WHEN** Row 包含 `FTcsStateParameter CooldownParam`
- **THEN** DataTable 编辑器自动展开为子列 `CooldownParam.ParameterType`、`CooldownParam.NumericParamEvaluator` 等

#### Scenario: FInstancedStruct 和 FGameplayTagContainer 完整映射
- **WHEN** Row Struct 包含 `FInstancedStruct` 或 `FGameplayTagContainer` 类型的 UPROPERTY
- **THEN** 直接赋值 MUST 完整深拷贝到 DefAsset 属性
- **AND** DataTable 编辑器 MUST 能直接编辑这些类型的值
