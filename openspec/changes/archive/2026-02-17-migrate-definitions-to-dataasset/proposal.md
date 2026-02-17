# Proposal: 将定义系统从 DataTable 迁移到 DataAsset

## Change ID
`migrate-definitions-to-dataasset`

## 概述

将 TCS 插件中的核心定义系统（AttributeDef、StateDef、StateSlotDef）从基于 DataTable 的行结构迁移到独立的 DataAsset 架构。这是一个重大的架构变更，旨在提升编辑体验、增强类型安全性、改善版本控制，并为未来的扩展性奠定基础。

## 动机

### 当前架构的局限性

**DataTable 行结构的问题**:
1. **编辑体验差**: 必须通过 DataTable 的行编辑器操作，无法直接在内容浏览器中创建/编辑
2. **版本控制困难**: 所有定义在一个文件中，团队协作时容易产生合并冲突
3. **缺乏继承和多态**: 无法创建蓝图子类或扩展定义
4. **查找依赖不直观**: 只能通过 FName 字符串查找，无法利用 UE 的资产引用系统
5. **验证时机受限**: 只能在运行时或手动验证，无法利用编辑器的资产验证系统

### DataAsset 的优势

**编辑体验**:
- 每个定义是独立的资产文件，可以直接在内容浏览器中创建、编辑、复制
- 支持文件夹分类组织（如 Attributes/Combat/、Attributes/Movement/）
- 可以使用资产缩略图、标签、搜索等 UE 编辑器功能

**技术优势**:
- 支持蓝图子类扩展（可以创建 BP_AttributeDefBase 等）
- 更强的类型安全（UPrimaryDataAsset 提供完整的 UObject 功能）
- 可以添加自定义编辑器验证逻辑（PostEditChangeProperty）
- 支持 Asset Manager 的异步加载和内存管理

**架构优势**:
- 可以直接引用 DataAsset（TSoftObjectPtr<UAttributeDefAsset>）
- 更好的依赖追踪（UE 的 Reference Viewer 可以显示引用关系）
- 独立文件减少合并冲突
- 支持资产重定向和重命名

## 影响范围

### 受影响的系统

1. **Attribute 系统**
   - `FTcsAttributeDefinition` → `UTcsAttributeDefinitionAsset`
   - `FTcsAttributeModifierDefinition` → `UTcsAttributeModifierDefinitionAsset`
   - `UTcsAttributeManagerSubsystem` 的加载和查找逻辑
   - `UTcsGenericLibrary::GetAttributeDefTable()` 及相关 API（将被删除）
   - `FTcsAttributeInstance` 重构（硬指针方案：直接存储 DataAsset 硬指针 + DefId）
   - `FTcsAttributeModifierInstance` 重构（硬指针方案：直接存储 DataAsset 硬指针 + DefId）

2. **State 系统**
   - `FTcsStateDefinition` → `UTcsStateDefinitionAsset`
   - `UTcsStateDefinitionAsset` 添加 `StateTag` 字段
   - `UTcsStateManagerSubsystem` 的加载和查找逻辑
   - `UTcsGenericLibrary::GetStateDefTable()` 及相关 API（将被删除）
   - `UTcsStateInstance` 重构（硬指针方案：直接存储 DataAsset 硬指针 + DefId）
   - 支持灵活的加载策略（LoadAll / LoadOnDemand）

3. **StateSlot 系统**
   - `FTcsStateSlotDefinition` → `UTcsStateSlotDefinitionAsset`
   - StateSlot 的初始化和查找逻辑
   - `UTcsGenericLibrary::GetStateSlotDefTable()` 及相关 API（将被删除）

4. **配置系统**
   - `UTcsDeveloperSettings` 改为路径配置 + 自动扫描方式
   - 使用 `TArray<FDirectoryPath>` 配置资产路径
   - 添加内部缓存机制（Transient）
   - 添加 State 加载策略配置

5. **编辑器工具**
   - 实现资产扫描和 Asset Registry 监听机制
   - 提供编辑器验证

### 向后兼容性

**破坏性变更**:
- 删除所有返回结构体的 API（如 `GetAttributeDefinition(FName, FTcsAttributeDefinition&)`）
- 项目设置中的 DataTable 引用需要改为路径配置
- `FTcsAttributeInstance` 结构体字段变更（硬指针方案：UPROPERTY 硬指针 + DefId）
- `FTcsAttributeModifierInstance` 结构体字段变更（硬指针方案：UPROPERTY 硬指针 + DefId）
- `UTcsStateInstance` 类字段变更（硬指针方案：UPROPERTY 硬指针 + DefId）
- 所有使用 `AttributeInstance.AttributeDef`、`ModifierInstance.ModifierDef`、`StateInstance.StateDef` 的代码需要更新
- 移除所有 LoadSynchronous() 调用，直接使用硬指针访问

**兼容性策略**:
- 不提供过渡期的双重支持，一次性完全迁移
- 保持 Manager Subsystem 的查找 API 签名兼容（但返回 DataAsset 而非结构体）
- 提供清晰的文档说明 API 变更

## 实施策略

### 阶段划分

**Phase 1: 基础架构（核心变更）**
- 创建 DataAsset 基类（UPrimaryDataAsset）
- 实现路径配置 + 自动扫描的混合方案
- 实现资产扫描和 Asset Registry 监听机制
- 修改 Manager Subsystem 的加载逻辑
- 重构 FTcsAttributeInstance（硬指针方案：UPROPERTY 硬指针 + DefId）
- 重构 FTcsAttributeModifierInstance（硬指针方案：UPROPERTY 硬指针 + DefId）
- 重构 UTcsStateInstance（硬指针方案：UPROPERTY 硬指针 + DefId）
- 更新 FTcsAttributeClampContextBase 支持硬指针
- ✅ **已完成** (2026-02-16)
- 重构 UTcsStateInstance（UObject 简化方案：直接存储 DataAsset 指针）
- 删除所有结构体 API

**Phase 2: 编辑器支持**
- 添加资产验证逻辑

**Phase 3: 配置和文档（完善生态）**
- 更新文档和示例
- 提供最佳实践指南
- 性能测试和优化

### 技术方案

**DataAsset 设计**:
```cpp
// 使用 UPrimaryDataAsset 作为基类
UCLASS(BlueprintType)
class UTcsAttributeDefinitionAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

    // 定义的唯一标识符（对应原来的 RowName）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName AttributeDefId;

    // PrimaryAssetType 静态变量(使用 FPrimaryAssetType 类型更语义化)
    static const FPrimaryAssetType PrimaryAssetType;

    // 原 FTcsAttributeDefinition 的所有字段
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Range")
    FTcsAttributeRange AttributeRange;

    // ... 其他字段
};
```

**注册表系统**:
```cpp
class UTcsAttributeManagerSubsystem
{
    // 启动时扫描并注册
    TMap<FName, UTcsAttributeDefinitionAsset*> AttributeDefRegistry;
    TMap<FName, UTcsAttributeModifierDefinitionAsset*> AttributeModifierDefRegistry;

    // 新 API：直接获取 DataAsset
    UTcsAttributeDefinitionAsset* GetAttributeDefinitionAsset(FName DefId);
    UTcsAttributeModifierDefinitionAsset* GetAttributeModifierDefinitionAsset(FName DefId);
};
```

**配置方式**:
- 在 `UTcsDeveloperSettings` 中使用路径配置：`TArray<FDirectoryPath>`
- 添加内部缓存（Transient）：`TMap<FName, TSoftObjectPtr<UDataAsset>>`
- 实现 `ScanAndCacheDefinitions()` 方法自动扫描路径
- 使用 Asset Registry 监听资产变化，自动更新缓存

**FTcsAttributeInstance 重构（混合方案）**:
```cpp
struct FTcsAttributeInstance
{
    // 运行时缓存(不序列化)
    UPROPERTY(Transient)
    UTcsAttributeDefinitionAsset* AttributeDef;

    // 序列化使用(插件不强制存档策略)
    UPROPERTY()
    FName AttributeDefId;

    // 辅助方法
    UTcsAttributeDefinitionAsset* GetAttributeDefAsset() const;  // 纯粹的 Get
    void LoadAttributeDefAsset(UWorld* World);  // 专门的 Load
    bool Serialize(FArchive& Ar);
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};
```

**FTcsAttributeModifierInstance 重构（混合方案）**:
```cpp
struct FTcsAttributeModifierInstance
{
    // 运行时缓存(不序列化)
    UPROPERTY(Transient)
    UTcsAttributeModifierDefinitionAsset* ModifierDef;

    // 序列化使用(插件不强制存档策略)
    UPROPERTY()
    FName ModifierDefId;

    // 辅助方法
    UTcsAttributeModifierDefinitionAsset* GetModifierDefAsset() const;  // 纯粹的 Get
    void LoadModifierDefAsset(UWorld* World);  // 专门的 Load
    bool Serialize(FArchive& Ar);
    bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
};
```

**UTcsStateInstance 重构（UObject 简化方案）**:
```cpp
class UTcsStateInstance : public UObject
{
    // 直接存储指针,UE自动处理序列化
    UPROPERTY(BlueprintReadOnly, Category = "State")
    UTcsStateDefinitionAsset* StateDef;

    // 保留作为备用标识符
    UPROPERTY(BlueprintReadOnly, Category = "State")
    FName StateId;
};
```

## 风险和缓解

### 风险

1. **路径配置错误**: 配置的路径不存在或包含错误的资产类型
   - **缓解**: 在 DeveloperSettings 中添加验证逻辑，启动时检查路径有效性

2. **性能影响**: 从 TMap 查找可能比 DataTable 的 FindRow 慢
   - **缓解**: 使用 TMap<FName, UDataAsset*> 保持 O(1) 查找，性能相当

3. **学习曲线**: 团队需要适应新的工作流程
   - **缓解**: 提供详细文档和示例，新工作流程实际上更直观

4. **Instance 重构风险**: 修改核心数据结构可能影响多处代码
   - **缓解**:
     - FTcsAttributeInstance 和 FTcsAttributeModifierInstance 使用混合方案平衡性能和序列化
     - UTcsStateInstance 使用 UObject 简化方案，直接存储指针
     - Get 和 Load 明确分离，职责清晰
     - DefAsset 是固定资产，加载后不会改变
     - 提供详细的迁移指南和代码示例

5. **资产扫描性能**: 启动时扫描大量资产可能影响启动速度
   - **缓解**: 使用缓存机制，只在路径配置改变时重新扫描

### 回滚计划

如果实施失败，可以：
1. 使用版本控制回滚到迁移前的状态
2. 保留原有的 DataTable 资产作为备份

## 成功标准

1. **功能完整性**: 所有现有功能正常工作，无回归
2. **性能持平**: 查找和加载性能不低于原 DataTable 方案
3. **资产扫描机制**: 自动扫描和缓存机制正常工作，Asset Registry 监听正确响应
4. **文档完善**: 更新所有相关文档和示例
5. **编辑器体验**: 策划/设计师反馈新工作流程更高效
6. **API 清晰**: 新的 DataAsset API 使用简单直观
7. **Instance 重构**:
   - **FTcsAttributeInstance**:
     - 所有使用 AttributeInstance 的代码正常工作
     - 运行时访问性能优于 DefId 查询方案
     - 序列化开销等同于 DefId 方案
     - 网络同步正常工作
     - 缓存刷新机制正确可靠
   - **FTcsAttributeModifierInstance**:
     - 所有使用 ModifierInstance 的代码正常工作
     - 混合方案性能和序列化优势与 AttributeInstance 一致
   - **UTcsStateInstance**:
     - 所有使用 StateInstance 的代码正常工作
     - UObject 自动序列化机制正常工作
     - 指针引用正确维护

## 替代方案

### 方案 A: 保持 DataTable，增强工具
- 为 DataTable 编辑器添加自定义工具
- 不改变底层架构
- **缺点**: 无法解决版本控制和继承问题

### 方案 B: 混合方案（DataTable + DataAsset）
- 同时支持两种方式
- **缺点**: 维护成本高，架构复杂

### 方案 C: 使用 Struct Asset Plugin
- 使用第三方插件管理结构体资产
- **缺点**: 引入外部依赖，不如原生 DataAsset 稳定

**推荐**: 完全迁移到 DataAsset（当前提案）

## 已确认的决策

在实施过程中，以下问题已经确认：

1. **是否需要支持蓝图子类**？
   - **决策**: 不需要。DataAsset 类设计为最终类，不支持蓝图继承。
   - **理由**: 简化设计，减少复杂度，当前需求不需要蓝图扩展。

2. **配置方式偏好**？
   - **决策**: 混合方案（路径配置 + 自动扫描）
   - **实现**: 使用 `TArray<FDirectoryPath>` 配置路径，自动扫描并缓存所有 DataAsset
   - **理由**: 既便捷又灵活，减少手动配置工作量

3. **是否需要过渡期的双重支持**？
   - **决策**: 不需要，一次性完全迁移
   - **理由**: 简化架构，避免维护双重系统的复杂度

4. **Modifier 定义是否也迁移**？
   - **决策**: 是，AttributeModifierDef 也一起迁移到 DataAsset
   - **理由**: 保持架构一致性，提供统一的编辑体验

5. **是否保留结构体 API**？
   - **决策**: 不保留，删除所有返回结构体的 API
   - **理由**: 简化 API，避免维护转换逻辑，直接使用 DataAsset

6. **State 加载策略**？
   - **决策**: 支持灵活的加载策略（LoadAll / LoadOnDemand）
   - **实现**: 在 DeveloperSettings 中配置 `ETcsStateLoadingStrategy`
   - **理由**: 提供性能优化选项，适应不同项目需求

7. **Instance 类型如何处理**？
   - **决策**: 根据类型采用不同方案
   - **实现**:
     - **FTcsAttributeInstance** (struct): 混合方案
       - 运行时使用 `UPROPERTY(Transient) UTcsAttributeDefinitionAsset* AttributeDef` 缓存
       - 序列化使用 `UPROPERTY() FName AttributeDefId`（插件不强制存档策略）
       - 提供 `GetAttributeDefAsset()` 纯粹的 Get 函数（只读）
       - 提供 `LoadAttributeDefAsset(UWorld*)` 专门的 Load 函数（加载并缓存）
       - 实现自定义序列化和网络序列化
     - **FTcsAttributeModifierInstance** (struct): 混合方案
       - 与 AttributeInstance 相同的处理方式
       - 运行时缓存 `ModifierDef` 指针，序列化 `ModifierDefId`
       - Get 和 Load 明确分离
     - **UTcsStateInstance** (UObject): 简化方案
       - 直接存储 `UPROPERTY() UTcsStateDefinitionAsset* StateDef` 指针
       - UE 自动处理序列化，无需手动管理
   - **理由**:
     - Struct 需要混合方案平衡性能和序列化
     - UObject 可以利用 UE 自动序列化机制
     - Get 和 Load 职责分离，符合单一职责原则
     - 运行时性能最优(直接指针访问)
     - 序列化开销最小(struct 只保存 DefId, UObject 自动处理)
     - 网络同步友好(struct 传递 DefId, UObject 自动同步)
     - 符合 UE5 最佳实践(参考 GameplayAbilities 系统)
     - 插件不强制存档策略，由游戏项目决定

## 时间线

实施时间取决于具体的代码复杂度和测试需求，建议按阶段逐步完成：
- **Phase 1 (基础架构)**: 核心变更，需要仔细测试
- **Phase 2 (编辑器支持)**: 提升编辑体验
- **Phase 3 (配置和文档)**: 完善生态

## 相关资源

- UE5 官方文档: [Primary Data Assets](https://docs.unrealengine.com/5.0/en-US/API/Runtime/Engine/Engine/UPrimaryDataAsset/)
- UE5 官方文档: [Asset Manager](https://docs.unrealengine.com/5.0/en-US/asset-management-in-unreal-engine/)
- 现有代码: `TcsAttribute.h`, `TcsState.h`, `TcsStateSlot.h`
- 现有代码: `TcsAttributeManagerSubsystem.cpp`, `TcsStateManagerSubsystem.cpp`
