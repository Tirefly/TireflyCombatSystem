# Tasks: DataTable ↔ DefAsset 双向同步

## 1. DeveloperSettings 扩展

- [ ] 1.1 新增 `bEnableDataTableAutoSync` 总开关
- [ ] 1.2 新增 `FTcsDataTableSyncConfig` 结构体 + `DataTableSyncConfigs` 数组

## 2. Row Struct 定义

- [ ] 2.1 创建 `FAttributeDefRow`
- [ ] 2.2 创建 `FAttributeModifierDefRow`
- [ ] 2.3 创建 `FBuffDefRow`
- [ ] 2.4 创建 `FSkillDefRow`
- [ ] 2.5 创建 `FSkillModifierDefRow`
- [ ] 2.6 创建 `FStateSlotDefRow`

## 3. Subsystem 骨架

- [ ] 3.1 创建 `UTcsDefAssetDataTableSyncSubsystem`
- [ ] 3.2 注册 `OnPackageSaved` 委托
- [ ] 3.3 实现防循环 `bIsSyncing` 守卫

## 4. DataTable → DefAsset 同步

- [ ] 4.1 遍历 DataTable 行匹配已有 DefAsset
- [ ] 4.2 Field-by-field CopyCompleteValue（反射映射）
- [ ] 4.3 行无对应 DefAsset → NewObject 创建
- [ ] 4.4 目录中孤立 DefAsset 清理

## 5. DefAsset → DataTable 同步

- [ ] 5.1 根据 DefAsset 路径反查子目录
- [ ] 5.2 加载/自动创建 DataTable
- [ ] 5.3 更新/新增 DataTable 行

## 6. 子目录映射

- [ ] 6.1 基路径下子目录自动发现
- [ ] 6.2 `DT_{前缀}_{子目录名}` 命名
- [ ] 6.3 子目录为空时不创建 DataTable

## 7. 验证

- [ ] 7.1 `TireflyGameplayUtilsEditor Win64 Development` 编译通过
- [ ] 7.2 `openspec validate add-datatable-defasset-sync --strict` 通过
