# Proposal: DataTable ↔ DefAsset 自动双向同步

## Why

策划需要批量管理 TCS 的 DefAsset 配置。逐个在 Content Browser 中双击编辑 `.uasset` 效率极低——新增一个英雄的 50 个技能 Buff 需要打开 50 次属性面板。用 DataTable 编辑后手动创建对应资产则容易遗漏。

核心需求：**保存 DataTable 时自动生成/更新 DefAsset；直接编辑 DefAsset 时自动回写 DataTable。**

## What Changes

| 类别 | 内容 |
|------|------|
| **新建 Subsystem** | `UTcsDefAssetDataTableSyncSubsystem : UEditorSubsystem`（在 Editor 模块） |
| **新建 Row Struct** | 每种 DefAsset 对应一个 `FTableRow` 结构体（6 个） |
| **DeveloperSettings 扩展** | 新增 `bEnableDataTableAutoSync` + `DataTableSyncConfigs` |
| **双向同步** | DT → DefAsset（创建/更新/删孤立）；DefAsset → DT（回写行） |
| **子目录映射** | 基路径下每个非空子目录 → 自动对应一个 DataTable |
| **完整类型覆盖** | `FInstancedStruct`、`FGameplayTagContainer` 均通过反射 CopyCompleteValue 映射 |

### 不包含

- CSV / JSON 导入导出
- 运行时模块任何依赖

## 关键设计决策

| 决策 | 选择 | 理由 |
|------|------|------|
| 模块归属 | `TireflyCombatSystemEditor` | 复用 Factory/AssetDefinition/Registry；EditorSubsystem 天然适配 |
| Row Struct 位置 | Editor 模块 `Public/DataTableSync/` | 纯编辑器类型，不污染运行时 |
| 字段映射 | 反射同名属性自动 Copy | 零手动映射代码 |
| DataTable 命名 | `DT_{前缀}_{子目录名}` | 可预测，自动创建 |
| 防循环守卫 | `bIsSyncing` 标志 | 两次 `OnPackageSaved` 回调不触发递归 |
| 基路径约定 | DeveloperSettings 配置一个 `BasePath` | 一个 DefAsset 类型一个基路径，自动遍历子目录 |
