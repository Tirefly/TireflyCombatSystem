# Proposal: DataTable ↔ DefAsset 自动双向同步

## Why

策划需要批量管理 TCS 的 DefAsset 配置。逐个在 Content Browser 中双击编辑 `.uasset` 效率极低——新增一个英雄的 50 个技能 Buff 需要打开 50 次属性面板。用 DataTable 编辑后手动创建对应资产则容易遗漏。

核心需求：**保存 DataTable 时自动生成/更新/删除对应 DefAsset；直接编辑 DefAsset 时自动回写对应 DataTable；手动删除受管 DefAsset 时自动删除对应数据表行。**

## What Changes

| 类别 | 内容 |
|------|------|
| **新建 Subsystem** | `UTcsDefAssetDataTableSyncSubsystem : UEditorSubsystem`（在 Editor 模块） |
| **新建 Row Struct** | 每种 DefAsset 对应一个 `FTableRow` 结构体（6 个） |
| **DeveloperSettings 扩展** | 新增 `bEnableDataTableAutoSync` + `DataTableSyncConfigs` |
| **双向同步** | DT → DefAsset（创建/更新/删孤立）；DefAsset → DT（新增/更新行 / 删除行） |
| **严格镜像** | `RowName` 作为唯一主键并承载 DefId；空表与空目录严格对应 |
| **显式绑定** | 每条配置显式绑定 1 个受管 DefAsset 文件夹与 1 张固定 DataTable |
| **完整类型覆盖** | `FInstancedStruct`、`FGameplayTagContainer` 均通过静态直接赋值映射 |

### 不包含

- CSV / JSON 导入导出
- 运行时模块任何依赖

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 模块归属 | `TireflyCombatSystemEditor` | 复用 Factory/AssetDefinition/Registry；EditorSubsystem 天然适配 |
| Row Struct 位置 | Editor 模块 `Public/DataTableSync/` | 纯编辑器类型，不污染运行时 |
| 字段映射 | 每类型静态 `SyncRowToAsset` / `SyncAssetToRow` | 编译期校验，避免反射残留与双主键歧义 |
| 主键承载 | `RowName == DefId` | 避免 Row 内再维护一份重复 ID 字段 |
| DataTable 保存策略 | 严格镜像并允许删除孤立 DefAsset | 让表成为该同步组的结构权威源 |
| DefAsset 保存策略 | 保存时新增/更新对应行，删除时移除对应行 | 保持 DataTable 与受管资产集合严格对应 |
| 同步调度 | 保存回调只入队，下一 tick batch 执行 | 避免在 `OnPackageSaved` 中直接二次写盘造成 save-loop |
| 删除事件来源 | 监听资产删除事件而非保存回调 | 手动删除资产不会触发 `OnPackageSaved` |
| 配置模型 | DeveloperSettings 配置 `ManagedDefAssetDirectory` + `TargetDataTable` | 与“一个文件夹对应一张表”的 authoring 约束保持一致，避免多层路径派生歧义 |
| 接口完备性 | 在提案阶段补齐配置校验、RowStruct 解析、删除快照、重命名语义与失败策略 | 避免实现阶段继续临场拍板关键行为 |
