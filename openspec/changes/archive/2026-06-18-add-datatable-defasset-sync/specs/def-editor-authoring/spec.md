# def-editor-authoring Spec Delta

## ADDED Requirements

### Requirement: DataTable ↔ DefAsset 双向自动同步

TCS 编辑器集成 SHALL 支持通过 `UTcsDeveloperSettings` 启用的 DataTable ↔ DefAsset 自动同步机制。保存 DataTable 时自动创建/更新/删除其绑定目录下的 DefAsset；保存 DefAsset 时自动回写其绑定 DataTable 行。每条同步配置都显式绑定一个受管 DefAsset 文件夹和一张固定 DataTable，并形成严格镜像同步关系。

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

#### Scenario: 手动删除 DefAsset 后自动删除 DataTable 行
- **WHEN** 策划在受管相对子目录中手动删除某个 DefAsset
- **THEN** 同步系统 SHALL 通过资产删除事件识别该删除操作，而不是依赖保存回调
- **AND** 对应 DataTable 中 `RowName == DefId` 的行 SHALL 被删除并标记为脏

#### Scenario: 保存 DefAsset 后自动创建缺失的 DataTable
- **WHEN** 策划在某个受管目录下新建并保存一个 DefAsset，且该配置绑定的 DataTable 尚不存在
- **THEN** 系统自动创建目标 DataTable
- **AND** 以该 DefAsset 的 ID 字段值作为 `RowName` 新增对应行
- **AND** 新建 DataTable 的 `RowStruct` SHALL 来自该 `DefAssetClass` 的静态类型描述符，而不是运行时猜测

#### Scenario: 双向同步防止无限循环
- **WHEN** DataTable 保存触发 DefAsset 更新，或 DefAsset 保存触发 DataTable 更新
- **THEN** 同步系统 SHALL 将已保存对象入队并在下一 tick batch 执行真实同步
- **AND** batch 同步过程 SHALL 通过防循环守卫与去重集合阻止递归

#### Scenario: 每条配置显式绑定一个目录和一张表
- **WHEN** DeveloperSettings 配置 `ManagedDefAssetDirectory="/Game/TCS/AttributeDefs/Common"` 且 `TargetDataTable="/Game/TCS/AttributeTables/DT_CommonAttributeDefs"`
- **THEN** 该目录中的 DefAsset 只与这张固定 DataTable 建立同步关系
- **AND** 同步系统 SHALL NOT 再从根路径或相对子目录自动派生其他 DataTable

#### Scenario: 孤立 DefAsset 被清理
- **WHEN** DataTable 中已删除某行，但子目录中仍存在对应的 DefAsset
- **THEN** 保存该 DataTable 后，同步操作 SHALL 删除该孤立 DefAsset

#### Scenario: 空表与空目录严格对应
- **WHEN** 某个受管 DataTable 被保存且表中没有任何行
- **THEN** 对应 DefAsset 相对子目录 SHALL 收敛为零个 DefAsset

#### Scenario: 同类型多目录配置不发生错配
- **WHEN** 同一个 `DefAssetClass` 在 DeveloperSettings 中配置了多条目录-表绑定
- **THEN** 同步系统 SHALL 基于 DataTable 路径或 DefAsset 所在目录匹配唯一配置
- **AND** SHALL NOT 仅按 `DefAssetClass` 选择目标 DataTable

#### Scenario: 目标 DataTable 行结构不匹配时拒绝同步
- **WHEN** 某条配置绑定的 `TargetDataTable` 的 `RowStruct` 与该 `DefAssetClass` 的期望 RowStruct 不一致
- **THEN** 同步系统 SHALL 拒绝执行该请求并输出错误
- **AND** SHALL NOT 尝试写入不匹配的 DataTable

#### Scenario: DefAsset 迁移到另一受管目录时重绑定
- **WHEN** 某个受管 DefAsset 被移动到另一条配置绑定的受管目录
- **THEN** 同步系统 SHALL 将该变化视为“旧 DataTable 删除对应行 + 新 DataTable 新增或更新对应行”

#### Scenario: DefId 修改时重建主键行
- **WHEN** 策划修改了受管 DefAsset 内部的 DefId 字段并保存
- **THEN** 同步系统 SHALL 删除旧 `RowName` 对应的数据行，并以新 DefId 作为 `RowName` 新增或更新数据行
- **AND** 若新旧主键与现有行冲突，系统 SHALL 拒绝同步并输出错误

### Requirement: Row Struct 直接赋值映射

除标识字段外，Row Struct 与 DefAsset 的 UPROPERTY SHALL 保持类型和名称 1:1 一致。映射通过每种类型各一对编译期静态 `SyncRowToAsset` / `SyncAssetToRow` 函数直接赋值，不使用运行时反射。

#### Scenario: `RowName` 承载 DefId
- **WHEN** 同步系统处理任意一行 DataTable 与其对应 DefAsset
- **THEN** `RowName` SHALL 作为唯一主键并承载 DefId
- **AND** Row Struct 内 SHALL NOT 再重复声明 `DefId` / `AttributeDefId` / `ModifierId` / `StateSlotDefId` 等同义标识字段

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
