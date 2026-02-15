# Tasks: DataAsset 迁移实施任务

## Phase 1: 基础架构（核心变更）

### 1.1 创建 DataAsset 类定义

**目标**: 创建四个核心 DataAsset 类

**任务**:
- [x] 创建 `UTcsAttributeDefinitionAsset` 类
  - 文件: `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeDefinitionAsset.h`
  - 文件: `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeDefinitionAsset.cpp`
  - 继承 `UPrimaryDataAsset`
  - 添加 `AttributeDefId` 字段（FName）
  - 添加 `PrimaryAssetType` 静态变量（类型为 FPrimaryAssetType）
  - 复制 `FTcsAttributeDefinition` 的所有字段
  - 实现 `GetPrimaryAssetId()` 覆写

- [x] 创建 `UTcsStateDefinitionAsset` 类
  - 文件: `Source/TireflyCombatSystem/Public/State/TcsStateDefinitionAsset.h`
  - 文件: `Source/TireflyCombatSystem/Private/State/TcsStateDefinitionAsset.cpp`
  - 继承 `UPrimaryDataAsset`
  - 添加 `StateDefId` 字段（FName）
  - 添加 `StateTag` 字段（FGameplayTag）
  - 添加 `PrimaryAssetType` 静态变量（类型为 FPrimaryAssetType）
  - 复制 `FTcsStateDefinition` 的所有字段
  - 实现 `GetPrimaryAssetId()` 覆写

- [x] 创建 `UTcsStateSlotDefinitionAsset` 类
  - 文件: `Source/TireflyCombatSystem/Public/State/TcsStateSlotDefinitionAsset.h`
  - 文件: `Source/TireflyCombatSystem/Private/State/TcsStateSlotDefinitionAsset.cpp`
  - 继承 `UPrimaryDataAsset`
  - 添加 `StateSlotDefId` 字段（FName）
  - 添加 `PrimaryAssetType` 静态变量（类型为 FPrimaryAssetType）
  - 复制 `FTcsStateSlotDefinition` 的所有字段
  - 实现 `GetPrimaryAssetId()` 覆写

- [x] 创建 `UTcsAttributeModifierDefinitionAsset` 类
  - 文件: `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeModifierDefinitionAsset.h`
  - 文件: `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeModifierDefinitionAsset.cpp`
  - 继承 `UPrimaryDataAsset`
  - 添加 `AttributeModifierDefId` 字段（FName）
  - 添加 `PrimaryAssetType` 静态变量（类型为 FPrimaryAssetType）
  - 复制 `FTcsAttributeModifierDefinition` 的所有字段
  - 实现 `GetPrimaryAssetId()` 覆写

**验证**:
- 编译通过
- 可以在编辑器中创建这四种 DataAsset
- 字段在细节面板中正确显示

**依赖**: 无

---

### 1.2 修改 DeveloperSettings

**目标**: 将配置从 DataTable 引用改为路径配置 + 自动扫描

**任务**:
- [ ] 修改 `UTcsDeveloperSettings` 类
  - 文件: `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`
  - 删除旧字段（不保留 DEPRECATED 标记）:
    - `TSoftObjectPtr<UDataTable> AttributeDefTable`
    - `TSoftObjectPtr<UDataTable> StateDefTable`
    - `TSoftObjectPtr<UDataTable> StateSlotDefTable`
  - 添加新字段:
    - `TArray<FDirectoryPath> AttributeDefinitionPaths` - Attribute 定义资产路径列表
    - `TArray<FDirectoryPath> StateDefinitionPaths` - State 定义资产路径列表
    - `TArray<FDirectoryPath> StateSlotDefinitionPaths` - StateSlot 定义资产路径列表
    - `TArray<FDirectoryPath> AttributeModifierDefinitionPaths` - AttributeModifier 定义资产路径列表
    - `ETcsStateLoadingStrategy StateLoadingStrategy` - State 加载策略（枚举：LoadAll, LoadOnDemand）
  - 添加内部缓存字段（Transient）:
    - `TMap<FName, TSoftObjectPtr<UTcsAttributeDefinitionAsset>> CachedAttributeDefinitions`
    - `TMap<FName, TSoftObjectPtr<UTcsStateDefinitionAsset>> CachedStateDefinitions`
    - `TMap<FName, TSoftObjectPtr<UTcsStateSlotDefinitionAsset>> CachedStateSlotDefinitions`
    - `TMap<FName, TSoftObjectPtr<UTcsAttributeModifierDefinitionAsset>> CachedAttributeModifierDefinitions`
  - 更新 `IsDataValid()` 验证逻辑

**验证**:
- 编译通过
- 项目设置中可以看到新的路径配置字段
- 可以添加/删除路径
- 可以选择 State 加载策略

**依赖**: 1.1 完成

---

### 1.3 修改 AttributeManagerSubsystem

**目标**: 实现 DataAsset 的加载、注册和查找逻辑

**任务**:
- [ ] 修改 `UTcsAttributeManagerSubsystem` 类
  - 文件: `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeManagerSubsystem.h`
  - 文件: `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeManagerSubsystem.cpp`
  - 移除字段: `UDataTable* AttributeDefTable`
  - 添加字段:
    - `TMap<FName, UTcsAttributeDefinitionAsset*> AttributeDefRegistry` - 运行时注册表
    - `TMap<FName, UTcsAttributeModifierDefinitionAsset*> AttributeModifierDefRegistry` - Modifier 注册表
  - 修改 `Initialize()`:
    - 调用 `ScanAndCacheDefinitions()` 扫描并缓存所有定义
    - 从 DeveloperSettings 的缓存中加载 DataAsset
    - 同步加载所有 DataAsset
    - 构建 `AttributeDefRegistry` 和 `AttributeModifierDefRegistry` 映射
    - 构建 `AttributeTagToName` 映射（保持现有逻辑）
  - 添加新方法:
    - `UTcsAttributeDefinitionAsset* GetAttributeDefinitionAsset(FName DefId)` - 获取 DataAsset
    - `UTcsAttributeModifierDefinitionAsset* GetAttributeModifierDefinitionAsset(FName DefId)` - 获取 Modifier DataAsset
  - 移除方法:
    - 删除所有返回 `FTcsAttributeDefinition` 结构体的方法

- [ ] 修改 `UTcsGenericLibrary` 相关方法
  - 文件: `Source/TireflyCombatSystem/Public/TcsGenericLibrary.h`
  - 文件: `Source/TireflyCombatSystem/Private/TcsGenericLibrary.cpp`
  - 移除 `GetAttributeDefTable()` 方法
  - 修改 `GetAttributeNames()`:
    - 从 AttributeManagerSubsystem 获取注册表的 Keys

**验证**:
- 编译通过
- 运行时能够正确加载 DataAsset
- 可以通过 DefId 查找到对应的 DataAsset
- Modifier 定义也能正确加载和查找

**依赖**: 1.1, 1.2 完成

---

### 1.4 修改 StateManagerSubsystem

**目标**: 实现 State 和 StateSlot 的 DataAsset 加载逻辑，支持灵活加载策略

**任务**:
- [ ] 修改 `UTcsStateManagerSubsystem` 类
  - 文件: `Source/TireflyCombatSystem/Public/State/TcsStateManagerSubsystem.h`
  - 文件: `Source/TireflyCombatSystem/Private/State/TcsStateManagerSubsystem.cpp`
  - 移除字段:
    - `UDataTable* StateDefTable`
    - `UDataTable* StateSlotDefTable`
  - 添加字段:
    - `TMap<FName, UTcsStateDefinitionAsset*> StateDefRegistry` - State 注册表
    - `TMap<FGameplayTag, FName> StateTagToDefId` - StateTag 到 DefId 的映射
    - `TMap<FName, UTcsStateSlotDefinitionAsset*> StateSlotDefRegistry` - StateSlot 注册表
  - 修改 `Initialize()`:
    - 调用 `ScanAndCacheDefinitions()` 扫描并缓存所有定义
    - 从 DeveloperSettings 的缓存中加载 DataAsset
    - 根据 StateLoadingStrategy 决定加载策略:
      - LoadAll: 同步加载所有 State DataAsset
      - LoadOnDemand: 只加载 StateSlot DataAsset，State 按需加载
    - 构建 `StateDefRegistry`、`StateTagToDefId` 和 `StateSlotDefRegistry` 映射
  - 添加新方法:
    - `UTcsStateDefinitionAsset* GetStateDefinitionAsset(FName DefId)` - 获取 State DataAsset（支持按需加载）
    - `UTcsStateDefinitionAsset* GetStateDefinitionAssetByTag(FGameplayTag StateTag)` - 通过 StateTag 获取
    - `UTcsStateSlotDefinitionAsset* GetStateSlotDefinitionAsset(FName DefId)` - 获取 StateSlot DataAsset
  - 移除方法:
    - 删除所有返回 `FTcsStateDefinition` 和 `FTcsStateSlotDefinition` 结构体的方法
  - 修改 `InitStateSlotDefs()`:
    - 从 StateSlotDefRegistry 读取定义

- [ ] 修改 `UTcsGenericLibrary` 相关方法
  - 移除 `GetStateDefTable()` 和 `GetStateSlotDefTable()` 方法
  - 修改 `GetStateDefNames()`:
    - 从 StateManagerSubsystem 获取注册表的 Keys

**验证**:
- 编译通过
- 运行时能够正确加载 State 和 StateSlot DataAsset
- LoadAll 策略下所有 State 都被预加载
- LoadOnDemand 策略下 State 按需加载
- StateTag 映射正确工作
- StateSlot 初始化正常

**依赖**: 1.1, 1.2 完成

---

### 1.5 实现资产扫描和监听机制

**目标**: 实现自动扫描和缓存 DataAsset 的机制

**任务**:
- [ ] 在 `UTcsDeveloperSettings` 中实现扫描方法
  - 文件: `Source/TireflyCombatSystem/Public/TcsDeveloperSettings.h`
  - 文件: `Source/TireflyCombatSystem/Private/TcsDeveloperSettings.cpp`
  - 添加方法 `ScanAndCacheDefinitions()`:
    - 使用 Asset Registry 扫描配置路径中的所有 DataAsset
    - 填充 CachedAttributeDefinitions、CachedStateDefinitions 等缓存
    - 记录扫描结果到日志
  - 添加 Asset Registry 监听回调:
    - 监听资产添加/删除/重命名事件
    - 自动更新缓存
  - 在 `PostInitProperties()` 中:
    - 编辑器模式下注册 Asset Registry 监听
  - 在 `PostEditChangeProperty()` 中:
    - 路径配置改变时重新扫描

- [ ] 在编辑器启动时触发初始扫描
  - 确保 DeveloperSettings 加载后自动扫描
  - 在 Manager Subsystem 初始化前完成扫描

**验证**:
- 编译通过
- 编辑器启动时自动扫描并缓存所有 DataAsset
- 添加新 DataAsset 时自动更新缓存
- 删除 DataAsset 时自动从缓存移除
- 日志显示扫描结果

**依赖**: 1.1, 1.2 完成

---

### 1.6 重构 FTcsAttributeInstance

**目标**: 将 AttributeInstance 从存储完整定义改为存储 DefId

**任务**:
- [ ] 修改 `FTcsAttributeInstance` 结构体
  - 文件: `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeInstance.h`
  - 添加字段 `UPROPERTY(Transient) UTcsAttributeDefinitionAsset* AttributeDef` - 运行时缓存
  - 添加字段 `UPROPERTY() FName AttributeDefId` - 序列化使用（插件不强制存档策略）
  - 移除字段 `FTcsAttributeDefinition AttributeDef`
  - 添加辅助方法:
    - `UTcsAttributeDefinitionAsset* GetAttributeDefAsset() const` - 纯粹的 Get，只返回缓存
    - `void LoadAttributeDefAsset(UWorld* World)` - 专门的 Load，加载并缓存定义
    - `FTcsAttributeRange GetAttributeRange(UWorld* World)` - 便捷方法（内部调用 Load）
    - `FGameplayTag GetAttributeTag(UWorld* World)` - 便捷方法（内部调用 Load）
    - `bool Serialize(FArchive& Ar)` - 自定义序列化
    - `bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)` - 网络序列化

- [ ] 更新所有使用 AttributeInstance 的代码
  - 文件: `Source/TireflyCombatSystem/Public/Attribute/TcsAttributeComponent.h`
  - 文件: `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent.cpp`
  - 文件: 其他所有引用 `AttributeInstance.AttributeDef` 的地方
  - 将 `AttributeInstance.AttributeDef` 改为先调用 `LoadAttributeDefAsset(GetWorld())`，再使用 `GetAttributeDefAsset()`
  - 或使用便捷方法如 `GetAttributeRange(GetWorld())`（推荐）
  - DefAsset 是固定资产，加载后不会改变

**验证**:
- 编译通过
- AttributeInstance 正确存储 DefId
- 可以通过 DefId 获取完整定义
- 所有使用 AttributeInstance 的功能正常工作
- Get 和 Load 职责分离清晰
- 运行时访问定义无需查询（直接使用缓存）
- 序列化只保存 DefId（8 bytes）
- 网络同步正常工作
- 加载存档或网络同步后，显式调用 Load 函数加载缓存

**依赖**: 1.1, 1.2, 1.3 完成

---

### 1.7 编译和基础测试

**目标**: 确保所有代码编译通过，基础功能正常

**任务**:
- [ ] 清理编译
  - 删除 Binaries 和 Intermediate 文件夹
  - 重新生成项目文件
  - 执行完整编译（Development Editor 配置）

- [ ] 基础功能测试
  - 创建测试用的 DataAsset（至少各一个）
  - 在 DeveloperSettings 中配置
  - 启动编辑器，检查日志
  - 验证 Subsystem 初始化成功
  - 验证能够查询到定义

**验证**:
- 编译无错误和警告
- 编辑器启动无崩溃
- 日志显示 DataAsset 加载成功
- 可以通过 API 查询到定义

**依赖**: 1.1, 1.2, 1.3, 1.4, 1.5, 1.6 完成

---

## Phase 2: 编辑器支持

### 2.1 添加编辑器验证

**目标**: 在编辑器中提供数据验证

**任务**:
- [ ] 实现 `UTcsAttributeDefinitionAsset::IsDataValid()`
  - 验证 AttributeDefId 不为空
  - 验证 ClampStrategyClass 有效
  - 验证 AttributeTag 格式正确（如果非空）
  - 验证 Range 配置合理（MinValue < MaxValue）

- [ ] 实现 `UTcsStateDefinitionAsset::IsDataValid()`
  - 验证 StateDefId 不为空
  - 验证 StateTag 有效
  - 验证 StateType 为 Skill 时 StateSlotType 必须有效
  - 验证 Priority >= 0
  - 验证 Duration 配置合理

- [ ] 实现 `UTcsStateSlotDefinitionAsset::IsDataValid()`
  - 验证 StateSlotDefId 不为空
  - 验证 SlotTag 有效
  - 验证 SamePriorityPolicy 在 PriorityOnly 模式下必须设置

- [ ] 实现 `UTcsAttributeModifierDefinitionAsset::IsDataValid()`
  - 验证 AttributeModifierDefId 不为空
  - 验证相关配置合理

- [ ] 添加 PostEditChangeProperty 逻辑
  - 自动修正不合理的配置
  - 提供友好的编辑器提示

**验证**:
- 保存无效数据时显示错误
- 数据验证器（Data Validation Plugin）能够检测问题
- 编辑器提示清晰易懂

**依赖**: Phase 1 完成

---

### 2.2 创建资产工厂

**目标**: 简化 DataAsset 创建流程

**任务**:
- [ ] 创建 `UTcsAttributeDefinitionAssetFactory`
  - 文件: `Source/TireflyCombatSystemEditor/Private/TcsAttributeDefinitionAssetFactory.h`
  - 文件: `Source/TireflyCombatSystemEditor/Private/TcsAttributeDefinitionAssetFactory.cpp`
  - 在右键菜单中添加 "TCS > Attribute Definition" 选项
  - 创建时自动设置默认值（如 ClampStrategyClass）

- [ ] 创建 `UTcsStateDefinitionAssetFactory`
  - 在右键菜单中添加 "TCS > State Definition" 选项

- [ ] 创建 `UTcsStateSlotDefinitionAssetFactory`
  - 在右键菜单中添加 "TCS > State Slot Definition" 选项

- [ ] 创建 `UTcsAttributeModifierDefinitionAssetFactory`
  - 在右键菜单中添加 "TCS > Attribute Modifier Definition" 选项

**验证**:
- 右键菜单中可以看到新选项
- 创建的 DataAsset 有合理的默认值

**依赖**: Phase 1 完成

---

## Phase 3: 配置和文档

### 3.1 更新文档

**目标**: 更新所有相关文档

**任务**:
- [ ] 更新 `README.md`
  - 说明新的 DataAsset 架构
  - 更新配置说明
  - 添加使用指南

- [ ] 更新 `CLAUDE.md`
  - 更新 OpenSpec 相关说明
  - 添加 DataAsset 最佳实践

- [ ] 创建使用指南文档
  - 文件: `Documents/DataAsset_Usage_Guide.md`
  - 详细说明如何创建和配置 DataAsset
  - 提供最佳实践和示例

- [ ] 更新 API 文档
  - 标记废弃的 API（如 GetAttributeDefTable）
  - 说明新的 API 用法

**验证**:
- 文档清晰易懂
- 所有链接有效
- 代码示例可运行

**依赖**: Phase 1, Phase 2 完成

---

### 3.2 性能测试和优化

**目标**: 确保性能不低于原方案

**任务**:
- [ ] 创建性能测试用例
  - 测试定义查找性能（1000 次查询）
  - 测试 Subsystem 初始化时间
  - 测试内存占用

- [ ] 对比 DataTable 和 DataAsset 方案
  - 记录性能数据
  - 分析性能差异

- [ ] 优化（如果需要）
  - 优化 Registry 查找
  - 考虑缓存 ToStructDefinition 结果
  - 优化加载流程

**验证**:
- 查找性能 >= DataTable 方案
- 初始化时间增加 < 10%
- 内存占用增加 < 20%

**依赖**: Phase 1, Phase 2 完成

---

### 3.3 最终验证和清理

**目标**: 确保变更完整且无遗留问题

**任务**:
- [ ] 代码审查
  - 检查所有新增代码
  - 确保符合编码规范
  - 添加必要的注释

- [ ] 清理废弃代码
  - 移除 DataTable 相关的旧代码（如果确认不再需要）
  - 清理临时测试代码

- [ ] 更新 OpenSpec
  - 运行 `openspec validate migrate-definitions-to-dataasset --strict`
  - 修复所有验证问题
  - 标记变更为已完成

- [ ] 提交代码
  - 创建有意义的 commit message
  - 推送到版本控制

**验证**:
- 所有任务完成
- OpenSpec 验证通过
- 代码已提交

**依赖**: 所有前置任务完成

---

## 任务依赖关系

```
Phase 1: 基础架构
├─ 1.1 创建 DataAsset 类定义
├─ 1.2 修改 DeveloperSettings (依赖 1.1)
├─ 1.3 修改 AttributeManagerSubsystem (依赖 1.1, 1.2)
├─ 1.4 修改 StateManagerSubsystem (依赖 1.1, 1.2)
├─ 1.5 实现资产扫描和监听机制 (依赖 1.1, 1.2)
├─ 1.6 重构 FTcsAttributeInstance (依赖 1.1, 1.2, 1.3)
└─ 1.7 编译和基础测试 (依赖 1.1-1.6)

Phase 2: 编辑器支持
├─ 2.1 添加编辑器验证 (依赖 Phase 1)
└─ 2.2 创建资产工厂 (依赖 Phase 1)

Phase 3: 配置和文档
├─ 3.1 更新文档 (依赖 Phase 1, Phase 2)
├─ 3.2 性能测试和优化 (依赖 Phase 1, Phase 2)
└─ 3.3 最终验证和清理 (依赖所有前置任务)
```

## 并行化建议

可以并行执行的任务:
- 1.3 和 1.4 可以并行（修改不同的 Subsystem）
- 1.5 和 1.6 可以并行（独立功能）
- 2.1 和 2.2 可以并行（独立的编辑器功能）
- 3.1 和 3.2 可以并行（文档和性能测试）

## 工作量估算

### Phase 1: 基础架构
- **1.1 创建 DataAsset 类**: 中等复杂度，需要创建 4 个类并正确配置
- **1.2 修改 DeveloperSettings**: 中等复杂度，需要实现路径配置和缓存机制
- **1.3 修改 AttributeManagerSubsystem**: 高复杂度，需要重构加载逻辑和 API
- **1.4 修改 StateManagerSubsystem**: 高复杂度，需要实现灵活加载策略
- **1.5 实现资产扫描机制**: 中等复杂度，需要使用 Asset Registry API
- **1.6 重构 AttributeInstance**: 中等复杂度，需要更新多处代码
- **1.7 编译和测试**: 低复杂度，验证工作

### Phase 2: 编辑器支持
- **2.1 编辑器验证**: 中等复杂度，需要实现多个验证方法
- **2.2 资产工厂**: 低复杂度，标准的 Factory 实现

### Phase 3: 配置和文档
- **3.1 更新文档**: 低复杂度，文档编写工作
- **3.2 性能测试**: 中等复杂度，需要设计测试场景
- **3.3 最终验证**: 低复杂度，验证和清理工作
