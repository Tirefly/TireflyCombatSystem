# Spec: 属性 SourceHandle 稳定索引

## 概述

本规范定义属性系统中 SourceHandle 索引机制的重构,从基于不稳定数组下标的缓存改为基于稳定 ModifierInstId 的缓存,确保查询和移除操作的正确性。

## ADDED Requirements

### Requirement: SourceHandle 索引 MUST 使用稳定 ID

**优先级**: P0 (关键)

**描述**:
`UTcsAttributeComponent` MUST use stable `ModifierInstId` as index key instead of array indices to ensure index validity after Modifier deletion operations. `UTcsAttributeComponent` 必须使用稳定的 `ModifierInstId` 作为索引键,而不是数组下标,以确保在 Modifier 删除操作后索引仍然有效。

#### Scenario: 数据结构定义

**Given** `UTcsAttributeComponent` 类定义

**When** 声明 SourceHandle 索引相关的成员变量

**Then** 必须包含以下字段:
- `TMap<int32, TArray<int32>> SourceHandleIdToModifierInstIds` - SourceHandle.Id 到 ModifierInstId 列表的映射
- `TMap<int32, int32> ModifierInstIdToIndex` - ModifierInstId 到当前数组下标的映射
- `TArray<FTcsAttributeModifierInstance> AttributeModifiers` - Modifier 实例数组(保持不变)

**And** 不应包含 `SourceHandleIdToModifierIndices` 字段(已废弃)

**And** 这些字段不应使用 `UPROPERTY` 标记(运行时缓存,无需序列化)

---

### Requirement: Modifier 插入时 MUST 更新索引

**优先级**: P0 (关键)

**描述**:
When adding new Modifier to `AttributeModifiers` array, MUST synchronously update both index caches. 当向 `AttributeModifiers` 数组添加新 Modifier 时,必须同步更新两个索引缓存。

#### Scenario: 插入 Modifier 更新索引

**Given** 一个有效的 `FTcsAttributeModifierInstance` 实例,其 `ModifierInstId` 为 `InstId`,`SourceHandle.Id` 为 `SourceId`

**When** 将该 Modifier 添加到 `AttributeModifiers` 数组

**Then** 必须执行以下操作:
1. 将 Modifier 添加到 `AttributeModifiers` 数组
2. 获取新添加元素的下标 `NewIndex`
3. 更新 `ModifierInstIdToIndex[InstId] = NewIndex`
4. 如果 `SourceHandle.IsValid()` 为 true,则更新 `SourceHandleIdToModifierInstIds[SourceId].AddUnique(InstId)`

**And** 操作必须是原子的(要么全部成功,要么全部失败)

---

### Requirement: Modifier 删除时 MUST 使用 RemoveAtSwap

**优先级**: P0 (关键)

**描述**:
When deleting Modifier, MUST use `RemoveAtSwap` instead of regular `RemoveAt` to avoid shifting entire array and only need to fix one swapped element's index. 删除 Modifier 时必须使用 `RemoveAtSwap` 而不是普通的 `RemoveAt`,以避免整个数组左移,并只需修正一个被 swap 的元素的索引。

#### Scenario: 删除 Modifier 更新索引

**Given** `AttributeModifiers` 数组中存在一个 Modifier,其 `ModifierInstId` 为 `InstId`,`SourceHandle.Id` 为 `SourceId`

**And** `ModifierInstIdToIndex[InstId]` 返回下标 `Index`

**When** 删除该 Modifier

**Then** 必须执行以下操作:
1. **在调用 RemoveAtSwap 之前**,必须先拷贝要删除的 Modifier 数据(值拷贝,不是引用),以避免后续广播事件时引用失效
2. 从 `ModifierInstIdToIndex` 中移除 `InstId`
3. 从 `SourceHandleIdToModifierInstIds[SourceId]` 中移除 `InstId`
4. 如果 `SourceHandleIdToModifierInstIds[SourceId]` 为空,则移除该桶
5. 如果 `Index != LastIndex` (有元素被 swap 过来):
   - 获取被 swap 元素的 `ModifierInstId` 为 `SwappedId`
   - 更新 `ModifierInstIdToIndex[SwappedId] = Index`
6. 使用 `AttributeModifiers.RemoveAtSwap(Index)` 删除元素
7. 使用拷贝的 Modifier 数据广播 `AttributeModifierRemoved` 事件

**And** 删除操作的时间复杂度必须是 O(1)

**And** 必须确保广播事件时传递的是**被删除的 Modifier** 的数据,而不是被 swap 过来的 Modifier 的数据

#### Scenario: 删除时避免引用失效 (Critical Bug Fix)

**Given** `AttributeModifiers` 数组包含多个 Modifier,要删除的 Modifier 在索引 `Index` 位置

**And** 代码中使用 `const FTcsAttributeModifierInstance& RemovedModifier = AttributeModifiers[Index]` 获取引用

**When** 调用 `AttributeModifiers.RemoveAtSwap(Index)` 删除元素

**Then** `RemovedModifier` 引用指向的内存位置会被 swap 操作修改:
- 如果 `Index != LastIndex`,该位置会被最后一个元素的数据覆盖
- 如果 `Index == LastIndex`,该位置的内存可能被释放

**And** 此时使用 `RemovedModifier` 引用会导致:
- 广播错误的 Modifier 数据(被 swap 过来的元素)
- 未定义行为(访问已释放的内存)

**Therefore** 必须在调用 `RemoveAtSwap` **之前**进行值拷贝:
```cpp
// 正确做法
const FTcsAttributeModifierInstance& RemovedModifierRef = AttributeModifiers[Index];
const FTcsAttributeModifierInstance RemovedModifier = RemovedModifierRef; // 值拷贝
// ... 执行索引更新和缓存清理 ...
AttributeModifiers.RemoveAtSwap(Index);
BroadcastAttributeModifierRemovedEvent(RemovedModifier); // 使用拷贝的数据
```

**And** 不能使用以下错误做法:
```cpp
// 错误做法 - 引用失效
const FTcsAttributeModifierInstance& RemovedModifier = AttributeModifiers[Index];
AttributeModifiers.RemoveAtSwap(Index); // 引用失效!
BroadcastAttributeModifierRemovedEvent(RemovedModifier); // 错误数据或未定义行为
```

---

### Requirement: SourceHandle 查询 MUST 支持自愈

**优先级**: P0 (关键)

**描述**:
When querying Modifier by SourceHandle, MUST automatically prune stale ModifierInstId (exists in `SourceHandleIdToModifierInstIds` but not in `ModifierInstIdToIndex`). 通过 SourceHandle 查询 Modifier 时,如果发现陈旧的 ModifierInstId (在 `SourceHandleIdToModifierInstIds` 中存在但在 `ModifierInstIdToIndex` 中不存在),必须自动剔除。

#### Scenario: 查询时自动剔除陈旧 ID

**Given** `SourceHandleIdToModifierInstIds[SourceId]` 包含 ID 列表 `[Id1, Id2, Id3]`

**And** `ModifierInstIdToIndex` 中只存在 `Id1` 和 `Id3`,不存在 `Id2`

**When** 调用 `GetModifiersBySourceHandle(SourceId)`

**Then** 必须返回 `Id1` 和 `Id3` 对应的 Modifier 实例

**And** 必须从 `SourceHandleIdToModifierInstIds[SourceId]` 中移除 `Id2`

**And** 最终 `SourceHandleIdToModifierInstIds[SourceId]` 应为 `[Id1, Id3]`

---

### Requirement: SourceHandle 移除操作 MUST 保证正确性

**优先级**: P0 (关键)

**描述**:
When removing Modifier by SourceHandle, MUST remove all associated Modifiers without omission or incorrect deletion. 通过 SourceHandle 移除 Modifier 时,必须移除所有关联的 Modifier,不能遗漏或误删。

#### Scenario: 移除所有关联 Modifier

**Given** `SourceHandleIdToModifierInstIds[SourceId]` 包含 ID 列表 `[Id1, Id2, Id3]`

**And** 这些 ID 在 `ModifierInstIdToIndex` 中都存在

**When** 调用 `RemoveModifiersBySourceHandle(SourceId)`

**Then** 必须删除所有三个 Modifier 实例

**And** `SourceHandleIdToModifierInstIds[SourceId]` 必须被移除(桶为空)

**And** `ModifierInstIdToIndex` 中不应再包含 `Id1`, `Id2`, `Id3`

**And** `AttributeModifiers` 数组中不应再包含这些 Modifier

#### Scenario: 移除时避免遍历修改冲突

**Given** `SourceHandleIdToModifierInstIds[SourceId]` 包含 ID 列表

**When** 调用 `RemoveModifiersBySourceHandle(SourceId)`

**Then** 必须先拷贝 ID 列表,再遍历删除

**And** 不能在遍历 `SourceHandleIdToModifierInstIds[SourceId]` 的同时修改它

---

## 相关规范

- `source-handle-core` - SourceHandle 核心定义
- `source-handle-attribute-integration` - SourceHandle 与属性系统集成

## 实施注意事项

1. **原子性**: 所有索引更新操作必须是原子的,避免中间状态
2. **性能**: 删除操作必须是 O(1),不能扫描所有桶
3. **自愈**: 查询操作必须能自动剔除陈旧 ID,保持缓存健康
4. **测试**: 必须覆盖大量插入、随机删除、陈旧 ID 等场景
