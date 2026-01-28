# Tasks: SourceHandle 机制实现

## 概述

本文档定义了 SourceHandle 机制的实现任务列表，按阶段组织，包含具体步骤、验证标准和依赖关系。

---

## 阶段 1：核心结构实现

### Task 1.1: 创建 FTcsSourceHandle 结构体

**目标**：创建核心的 SourceHandle 数据结构

**步骤**：
- [x] 在 `Public/` 目录创建 `TcsSourceHandle.h` 和 `TcsSourceHandle.cpp`
- [x] 定义 `FTcsSourceHandle` 结构体，包含以下字段：
  ```cpp
  UPROPERTY() int32 Id = -1;
  UPROPERTY() FDataTableRowHandle SourceDefinition;
  UPROPERTY() FName SourceName;
  UPROPERTY() FGameplayTagContainer SourceTags;
  UPROPERTY() TWeakObjectPtr<AActor> Instigator;
  ```
- [x] 实现 `IsValid()` 方法（检查 `Id >= 0`）
- [x] 实现 `ToDebugString()` 方法
- [x] 实现相等性运算符 (`operator==`, `operator!=`)
- [x] 实现 `GetTypeHash()` 友元函数（基于 `Id`）
- [x] 实现 `GetSourceDefinition<T>()` 模板方法
- [x] 添加详细的注释说明（Source vs Instigator 的区别）

**验证**：
- ✅ 编译通过，无警告
- ✅ `IsValid()` 正确判断有效性
- ✅ `ToDebugString()` 输出格式正确
- ✅ 可以作为 `TMap` 的 key 使用

**依赖**：无

---

### Task 1.2: 实现 NetSerialize 网络同步

**目标**：实现 SourceHandle 的网络序列化

**步骤**：
- [x] 在 `TcsSourceHandle.cpp` 中实现 `NetSerialize()` 方法
- [x] 序列化核心字段：
  - `Id`（int32，直接序列化）
  - `SourceDefinition.DataTable`（TObjectPtr，UE 自动处理）
  - `SourceDefinition.RowName`（FName，直接序列化）
  - `SourceName`（FName，直接序列化）
  - `SourceTags`（调用 `SourceTags.NetSerialize()`）
- [x] 条件序列化 `Instigator`（使用 RepBits 优化）
- [x] 添加 `TStructOpsTypeTraits` 特化，启用 `WithNetSerializer`
- [x] 添加注释说明网络同步机制

**验证**：
- ✅ 编译通过
- ✅ 网络环境下 SourceHandle 正确同步
- ✅ Instigator 正确映射到客户端 Actor

**依赖**：Task 1.1

---

### Task 1.3: 在 UTcsAttributeManagerSubsystem 中添加 ID 管理器

**目标**：实现全局唯一 ID 生成机制

**步骤**：
- [x] 在 `TcsAttributeManagerSubsystem.h` 中添加字段：
  ```cpp
  UPROPERTY()
  int32 GlobalSourceHandleIdCounter = 0;
  ```
- [x] 实现 `CreateSourceHandle()` 方法（完整版本）：
  - 参数：`SourceDefinition`, `SourceName`, `Instigator`, `SourceTags`
  - 递增计数器生成唯一 ID
  - 填充 SourceHandle 各字段
  - 返回新创建的 SourceHandle
- [x] 实现 `CreateSourceHandleSimple()` 方法（简化版本）：
  - 参数：`SourceName`, `Instigator`, `SourceTags`
  - 用于用户自定义效果（无 DataTable 引用）
- [x] 添加详细的注释说明
- [x] 考虑线程安全（GameThread 调用）

**验证**：
- ✅ 编译通过
- ✅ ID 生成唯一性（多次调用生成不同 ID）
- ✅ PIE 多世界隔离（每个 World 独立计数）

**依赖**：Task 1.1

---

## 阶段 2：属性系统集成

### Task 2.1: 修改 FTcsAttributeModifierInstance 结构

**目标**：在 ModifierInstance 中添加 SourceHandle 字段

**步骤**：
- [x] 在 `TcsAttributeModifier.h` 中的 `FTcsAttributeModifierInstance` 添加字段：
  ```cpp
  // 来源句柄
  UPROPERTY(BlueprintReadOnly)
  FTcsSourceHandle SourceHandle;
  ```
- [x] 保留现有的 `SourceName` 字段（向后兼容）
- [x] 添加注释说明新旧字段的关系：
  - `SourceHandle`：完整的来源信息
  - `SourceName`：冗余字段，用于快速访问和向后兼容
- [x] 更新构造函数（如需要）

**验证**：
- ✅ 编译通过
- ✅ 现有代码不受影响
- ✅ 可以访问 SourceHandle 字段

**依赖**：Task 1.1

---

### Task 2.2: 实现 ApplyModifierWithSourceHandle API

**目标**：提供带 SourceHandle 的新 API

**步骤**：
- [x] 在 `TcsAttributeManagerSubsystem.h` 中声明方法：
  ```cpp
  UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
  bool ApplyModifierWithSourceHandle(
      AActor* CombatEntity,
      const TArray<FTcsAttributeModifierDefinition>& ModifierDefs,
      const FTcsSourceHandle& SourceHandle,
      AActor* Instigator,
      AActor* Target,
      TArray<FTcsAttributeModifierInstance>& OutModifiers);
  ```
- [x] 在 `TcsAttributeManagerSubsystem.cpp` 中实现方法：
  - 为每个 ModifierDef 创建 ModifierInstance
  - 设置 `SourceHandle` 字段
  - 同步设置 `SourceName = SourceHandle.SourceName`
  - 调用现有的应用逻辑
- [x] 添加详细的注释说明
- [x] 添加参数验证（CombatEntity 有效性等）

**验证**：
- ✅ 编译通过
- ✅ 可以通过 SourceHandle 应用修改器
- ✅ ModifierInstance 正确填充 SourceHandle
- ✅ 蓝图可以调用

**依赖**：Task 1.3, Task 2.1

---

### Task 2.3: 实现 RemoveModifiersBySourceHandle API

**目标**：提供按 SourceHandle 移除修改器的 API

**步骤**：
- [x] 在 `TcsAttributeManagerSubsystem.h` 中声明方法：
  ```cpp
  UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
  bool RemoveModifiersBySourceHandle(
      AActor* CombatEntity,
      const FTcsSourceHandle& SourceHandle);
  ```
- [x] 在 `TcsAttributeManagerSubsystem.cpp` 中实现方法：
  - 从 AttributeComponent 获取所有 Modifier
  - 筛选出匹配 `SourceHandle.Id` 的 Modifier
  - 调用现有的移除逻辑
  - 返回是否成功移除
- [x] 添加详细的注释说明
- [x] 添加参数验证

**验证**：
- ✅ 编译通过
- ✅ 可以按 SourceHandle 精确撤销修改器
- ✅ 不影响其他 Modifier
- ✅ 蓝图可以调用

**依赖**：Task 2.1

---

### Task 2.4: 实现 GetModifiersBySourceHandle API

**目标**：提供按 SourceHandle 查询修改器的 API

**步骤**：
- [x] 在 `TcsAttributeManagerSubsystem.h` 中声明方法：
  ```cpp
  UFUNCTION(BlueprintCallable, Category = "TireflyCombatSystem|Attribute")
  bool GetModifiersBySourceHandle(
      AActor* CombatEntity,
      const FTcsSourceHandle& SourceHandle,
      TArray<FTcsAttributeModifierInstance>& OutModifiers);
  ```
- [x] 在 `TcsAttributeManagerSubsystem.cpp` 中实现方法：
  - 从 AttributeComponent 获取所有 Modifier
  - 筛选出匹配 `SourceHandle.Id` 的 Modifier
  - 输出到 OutModifiers
  - 返回是否找到
- [x] 添加详细的注释说明

**验证**：
- ✅ 编译通过
- ✅ 可以按 SourceHandle 查询修改器
- ✅ 返回正确的 Modifier 列表
- ✅ 蓝图可以调用

**依赖**：Task 2.1

---

### Task 2.5: 升级旧 API 以支持 SourceHandle

**目标**：保持向后兼容，旧 API 自动创建 SourceHandle

**步骤**：
- [x] 修改现有的 `ApplyModifier()` 方法实现
- [x] 为每个 Modifier 自动创建 SourceHandle（如果没有）：
  - 调用 `CreateSourceHandleSimple()` 生成新句柄
  - 使用 Modifier 的 `SourceName`
  - Instigator 从参数获取
- [x] 更新注释说明向后兼容逻辑
- [x] 确保旧代码继续工作

**验证**：
- ✅ 编译通过
- ✅ 旧代码无需修改即可工作
- ✅ 旧 API 创建的 Modifier 也有 SourceHandle
- ✅ 可以按 SourceHandle 撤销旧 API 创建的 Modifier

**依赖**：Task 1.3, Task 2.2

---

## 阶段 3：事件系统升级

### Task 3.1: 升级 FTcsAttributeChangeEventPayload

**目标**：将事件归因从 FName 升级为 SourceHandle

**步骤**：
- [x] 修改 `TcsAttributeChangeEventPayload.h` 中的结构：
  ```cpp
  // 变化来源记录（升级为 SourceHandle）
  UPROPERTY(BlueprintReadOnly)
  TMap<FTcsSourceHandle, float> ChangeSourceRecord;
  ```
- [x] 更新构造函数参数类型
- [x] 更新所有创建 EventPayload 的代码位置
- [x] 添加注释说明升级原因

**验证**：
- ✅ 编译通过
- ✅ 事件系统可以提供详细的 SourceHandle 归因
- ✅ 蓝图可以访问 SourceHandle 信息

**依赖**：Task 1.1

---

### Task 3.2: 更新事件发送逻辑

**目标**：在属性变化时正确记录 SourceHandle

**步骤**：
- [x] 在 `UTcsAttributeComponent` 中查找所有发送属性变化事件的位置
- [x] 更新为使用 SourceHandle 而非 FName 记录变化来源
- [x] 确保事件 payload 正确填充 SourceHandle 信息
- [x] 更新相关注释

**验证**：
- ✅ 编译通过
- ✅ 属性变化事件包含完整的 SourceHandle 归因信息
- ✅ 可以通过事件追踪伤害来源

**依赖**：Task 3.1

---

## 阶段 4：性能优化（可选）

### Task 4.1: 添加 SourceHandle 索引

**目标**：优化按 SourceHandle 查询的性能

**步骤**：
- [x] 在 `UTcsAttributeComponent` 中添加索引字段：
  ```cpp
  UPROPERTY()
  TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;
  ```
- [x] 在应用 Modifier 时更新索引
- [x] 在移除 Modifier 时更新索引
- [x] 在查询时使用索引加速
- [x] 添加注释说明索引维护逻辑

**验证**：
- ✅ 编译通过
- ✅ 按 SourceHandle 查询性能提升（O(1) 复杂度）
- ✅ 索引正确维护（增删改查一致）

**依赖**：Task 2.2, Task 2.3

---

## 阶段 5：验证和文档更新

### Task 5.1: 编译验证

**目标**：确保所有配置编译通过

**步骤**：
- [x] 使用 UnrealBuildTool 编译 Development 配置：
  ```bash
  "E:\UnrealEngine\UE_5.6\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe"
  TireflyGameplayUtilsEditor Win64 Development
  -Project="E:\Projects_Unreal\TireflyGameplayUtils\TireflyGameplayUtils.uproject"
  -rocket -progress
  ```
- [x] 使用 UnrealBuildTool 编译 DebugGame 配置
- [x] 确保无编译警告
- [x] 确保无编译错误

**验证**：
- ✅ Development 配置编译成功
- ✅ DebugGame 配置编译成功
- ✅ 无警告，无错误

**依赖**：所有实现任务

---

### Task 5.2: 更新文档

**目标**：提供清晰的使用文档

**步骤**：
- [x] 在 `Documents/` 目录添加 `SourceHandle使用指南.md`
- [x] 包含以下内容：
  - SourceHandle 概念说明（Source vs Instigator）
  - 基本使用示例（技能、陷阱、装备）
  - 死亡统计示例
  - 网络同步说明
  - 最佳实践
- [x] 更新 `CLAUDE.md` 添加 SourceHandle 说明
- [x] 编写迁移指南（从旧 API 到新 API）
- [x] 添加 API 参考文档

**验证**：
- ✅ 文档清晰，易于理解
- ✅ 示例代码可以运行
- ✅ 覆盖所有主要用例

**依赖**：所有实现任务

---

## 任务依赖关系图

```
阶段 1（核心结构）
├─ Task 1.1 (FTcsSourceHandle) ────────┐
│                                      │
├─ Task 1.2 (NetSerialize) ────────────┤
│   └─ 依赖: Task 1.1                  │
│                                      │
└─ Task 1.3 (ID 管理器) ───────────────┤
    └─ 依赖: Task 1.1                  │
                                       │
阶段 2（属性系统集成）                  │
├─ Task 2.1 (修改 ModifierInstance) ───┤
│   └─ 依赖: Task 1.1                  │
│                                      │
├─ Task 2.2 (ApplyModifier) ───────────┤
│   └─ 依赖: Task 1.3, Task 2.1        │
│                                      │
├─ Task 2.3 (RemoveModifiers) ─────────┤
│   └─ 依赖: Task 2.1                  │
│                                      │
├─ Task 2.4 (GetModifiers) ────────────┤
│   └─ 依赖: Task 2.1                  │
│                                      │
└─ Task 2.5 (升级旧 API) ──────────────┤
    └─ 依赖: Task 1.3, Task 2.2        │
                                       │
阶段 3（事件系统升级）                  │
├─ Task 3.1 (升级 EventPayload) ───────┤
│   └─ 依赖: Task 1.1                  │
│                                      │
└─ Task 3.2 (更新事件逻辑) ────────────┤
    └─ 依赖: Task 3.1                  │
                                       │
阶段 4（性能优化，可选）                │
└─ Task 4.1 (添加索引) ────────────────┤
    └─ 依赖: Task 2.2, Task 2.3        │
                                       │
阶段 5（验证和文档更新）                 │
├─ Task 5.1 (编译验证) ────────────────┤
│   └─ 依赖: 所有实现任务               │
│                                      │
└─ Task 5.2 (更新文档) ────────────────┘
    └─ 依赖: 所有实现任务
```

---

## 并行执行建议

可以并行执行的任务组：

**组 1（核心结构，可并行）**：
- Task 1.1 (FTcsSourceHandle)
- Task 1.3 (ID 管理器) - 依赖 Task 1.1 完成后

**组 2（属性系统 API，可并行）**：
- Task 2.2 (ApplyModifier)
- Task 2.3 (RemoveModifiers)
- Task 2.4 (GetModifiers)

**组 3（事件系统，顺序执行）**：
- Task 3.1 (升级 EventPayload)
- Task 3.2 (更新事件逻辑)

---

## 关键里程碑

1. **里程碑 1**：核心结构完成（阶段 1）
   - ✅ FTcsSourceHandle 可用
   - ✅ 网络同步实现
   - ✅ ID 生成机制就绪

2. **里程碑 2**：属性系统集成完成（阶段 2）
   - ✅ 可以使用 SourceHandle 应用/撤销修改器
   - ✅ 旧 API 向后兼容

3. **里程碑 3**：事件系统升级完成（阶段 3）
   - ✅ 事件提供完整的 SourceHandle 归因

4. **里程碑 4**：验证和文档更新完成（阶段 5）
   - ✅ 所有测试通过
   - ✅ 文档完善

---

## 成功标准

- ✅ 所有任务完成
- ✅ 编译通过，无警告
- ✅ 单元测试全部通过
- ✅ 旧代码向后兼容
- ✅ 新 API 功能完整
- ✅ 网络同步正常工作
- ✅ 文档完善清晰
- ✅ 性能满足要求（按 SourceHandle 查询 O(1)）

---

## 风险与缓解

**风险 1**：网络同步可能出现问题
- **缓解**：参考 GAS 的实现，充分测试网络环境

**风险 2**：性能影响
- **缓解**：添加索引优化，监控内存占用

**风险 3**：向后兼容性问题
- **缓解**：保留旧 API，充分测试现有代码

---

## 预估工作量

- **阶段 1**：核心结构 - 中等
- **阶段 2**：属性系统集成 - 中等
- **阶段 3**：事件系统升级 - 较小
- **阶段 4**：性能优化 - 较小（可选）
- **阶段 5**：验证和文档更新 - 中等

**总计**：中等规模的功能实现
