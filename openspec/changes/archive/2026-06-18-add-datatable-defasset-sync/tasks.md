# Tasks: DataTable ↔ DefAsset 双向同步

## 1. DeveloperSettings 扩展

- [x] 1.1 新增 `bEnableDataTableAutoSync` 总开关
- [x] 1.2 新增 `FTcsDataTableSyncConfig` 结构体 + `DataTableSyncConfigs` 数组
- [x] 1.3 在 `FTcsDataTableSyncConfig` 中新增 `ManagedDefAssetDirectory`、`TargetDataTable`、`bAllowDeleteOrphanDefAssets`
- [x] 1.4 `TargetDataTable` 使用软引用表达固定目标表，避免将配置继续建模为“路径根 + 自动派生多张表”
- [x] 1.5 实现配置校验：目录唯一、表唯一、`DefAssetClass` 非空、`TargetDataTable` 非空

## 2. Row Struct 定义

- [x] 2.1 创建 `FAttributeDefRow`（不重复声明 `AttributeDefId`，主键由 `RowName` 承载）
- [x] 2.2 创建 `FAttributeModifierDefRow`（不重复声明 `ModifierId`，主键由 `RowName` 承载）
- [x] 2.3 创建 `FBuffDefRow`（不重复声明 `StateDefId`，主键由 `RowName` 承载）
- [x] 2.4 创建 `FSkillDefRow`（不重复声明 `StateDefId`，主键由 `RowName` 承载）
- [x] 2.5 创建 `FSkillModifierDefRow`（不重复声明 `ModifierId`，主键由 `RowName` 承载）
- [x] 2.6 创建 `FStateSlotDefRow`（不重复声明 `StateSlotDefId`，主键由 `RowName` 承载）
- [x] 2.7 建立 `DefAssetClass -> RowStruct` 的静态描述符与 `ResolveExpectedRowStruct()`
- [x] 2.8 校验 `TargetDataTable` 的 `RowStruct` 与配置期望一致，不一致时拒绝同步

## 3. Subsystem 骨架

- [x] 3.1 创建 `UTcsDefAssetDataTableSyncSubsystem`
- [x] 3.2 注册 `OnPackageSaved` 委托
- [x] 3.3 注册 `OnAssetRemoved` / `OnInMemoryAssetDeleted`，处理手动删除 DefAsset 的同步删表行
- [x] 3.4 注册 `OnAssetRenamed`，处理受管目录迁移与重绑定
- [x] 3.5 实现 `FTcsPendingSyncRequest` / `FTcsRemovedDefAssetSnapshot` 等队列与快照类型
- [x] 3.6 实现“保存/删除/重命名回调只入队，deferred batch 执行真实同步”的调度链
- [x] 3.7 实现防循环与去重守卫（`bIsSyncingBatch` + pending set）
- [x] 3.8 在 batch 完成后统一调用一次 `UTcsDefinitionRegistrySubsystem::RequestRefresh()`

## 4. DataTable → DefAsset 同步

- [x] 4.1 根据 `RowName == DefId` 遍历 DataTable 行匹配已有 DefAsset
- [x] 4.2 为每种类型实现静态 `SyncRowToAsset`，先写入资产 ID 字段，再复制业务字段
- [x] 4.3 行无对应 DefAsset → 在该配置绑定的受管目录下创建新资产
- [x] 4.4 DataTable 保存时清理该受管目录中所有不在表内的孤立 DefAsset
- [x] 4.5 空表保存时收敛为该受管目录中的零个 DefAsset
- [x] 4.6 DataTable 保存时若发现重复 `RowName` 或多资产同 DefId 冲突，则中止该批次并报错

## 5. DefAsset → DataTable 同步

- [x] 5.1 根据 DefAsset 所在目录 + 类型匹配唯一的同步配置
- [x] 5.2 加载/自动创建该配置显式绑定的 DataTable
- [x] 5.3 根据资产 ID 字段生成 `RowName`
- [x] 5.4 为每种类型实现静态 `SyncAssetToRow`，仅复制业务字段
- [x] 5.5 更新/新增 DataTable 行，不删除其他行
- [x] 5.6 手动删除 DefAsset 时，根据被删资产的 DefId 删除对应 DataTable 行
- [x] 5.7 DefId 改名时执行“旧行删除 + 新行新增/更新”，并检测同表冲突

## 6. 显式目录-表绑定

- [x] 6.1 DataTable 保存时按表资产路径反查唯一配置，而不是按根路径派生
- [x] 6.2 同一 `DefAssetClass` 支持多条配置，但查找必须基于目录/表引用，不能只按类缓存
- [x] 6.3 空受管目录与其绑定 DataTable 严格对应
- [x] 6.4 受管目录迁移时执行“旧绑定删除 + 新绑定新增/更新”，而不是沿用旧配置

## 7. 失败策略与日志

- [x] 7.1 为配置无效、RowStruct 不匹配、DefId 缺失、DefId 冲突定义明确错误日志
- [x] 7.2 对目录外资产、无配置资产、非受管类型资产走幂等忽略路径

## 8. 验证

- [x] 8.1 `TireflyGameplayUtilsEditor Win64 Development` 编译通过
- [x] 8.2 `openspec validate add-datatable-defasset-sync --strict` 通过
