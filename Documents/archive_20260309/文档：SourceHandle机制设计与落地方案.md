# TCS SourceHandle 机制设计与落地方案（现行实现说明）

本文件用于说明 TireflyCombatSystem（TCS）当前已经落地的 SourceHandle 机制实现细节，避免旧版设计文档与现状不一致造成误导。

结论：SourceHandle 已在 Attribute 模块完整落地（含网络序列化能力），State 模块的“来源对齐”属于后续可选增强，不在本文件范围内展开。

---

## 1. 设计目标（已达成）

- **唯一性**：同一 `SourceName` 的多次应用可区分（`Id` 单调递增）。
- **可撤销**：支持按来源撤销（Remove by SourceHandle）。
- **可追踪**：事件归因可使用 SourceHandle（不仅仅是 SourceName 文本）。
- **可调试**：保留 `SourceName` 与 `ToDebugString()` 便于日志/快照。
- **低耦合**：`Instigator` 使用 `TWeakObjectPtr<AActor>`，不延长对象生命周期。
- **可复制（能力就绪）**：实现 `NetSerialize`，可作为 replicated 数据的一部分同步。

---

## 2. 数据结构：`FTcsSourceHandle`

实现位置：
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/TcsSourceHandle.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/TcsSourceHandle.cpp`

字段（现行实现）：
- `int32 Id`：全局唯一来源 ID（单调递增，`-1` 表示无效）。
- `FDataTableRowHandle SourceDefinition`：Source 定义（可选，用户自定义效果可为空）。
- `FName SourceName`：冗余字段，用于快速访问与调试汇总（非唯一）。
- `FGameplayTagContainer SourceTags`：来源分类/过滤标签。
- `TWeakObjectPtr<AActor> Instigator`：施加者（可选/弱引用）。

辅助能力（现行实现）：
- `bool IsValid() const`：`Id >= 0`。
- `FString ToDebugString() const`：`[SourceName|Id]`，并在 Instigator 有效时追加名称。
- `operator== / GetTypeHash`：基于 `Id`（支持作为 `TMap` key）。
- `NetSerialize`：序列化 `Id / SourceDefinition(DataTable+RowName) / SourceName / SourceTags`，并对 Instigator 做条件序列化。

---

## 3. Id 生成与作用域（现行实现）

Id 由 `UTcsAttributeManagerSubsystem` 内部的计数器生成：
- `int32 GlobalSourceHandleIdMgr`
- 每次 `CreateSourceHandle*` 调用时自增并写入 `FTcsSourceHandle::Id`

语义与注意事项：
- **唯一性范围**：同一个 Subsystem 生命周期内单调递增（通常对应同一 GameInstance/World 生命周期）。
- **不保证跨存档稳定**：重启/重新进入 PIE 会重新开始计数。
- **不要求网络全局一致**：网络同步由 `NetSerialize` + replicated 容器属性决定（见第 6 节）。

---

## 4. Attribute 模块改造点（现行实现）

### 4.1 `FTcsAttributeModifierInstance`

实现位置：
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeModifier.h`

新增字段：
- `FTcsSourceHandle SourceHandle;`

兼容字段：
- `FName SourceName` 仍保留（用于旧调用路径与 debug/汇总）。

同步规则（现行实现）：
- 通过 `ApplyModifierWithSourceHandle` 创建的 Modifier：会写入 `SourceHandle`，并同步 `SourceName = SourceHandle.SourceName`。
- 旧 API（只传 `SourceName`）创建的 Modifier：`SourceHandle` 保持无效（`Id == -1`）。

### 4.2 `FTcsAttributeChangeEventPayload::ChangeSourceRecord`

实现位置：
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeChangeEventPayload.h`

现行实现：
- `TMap<FTcsSourceHandle, float> ChangeSourceRecord;`

说明：
- key 基于 `Id` 判等/哈希，因此同名来源的多次应用不会互相覆盖。
- 事件侧可以同时用 `SourceName`（可读）与 `Id`（可精确定位）来做 debug/统计。

### 4.3 新增 API（现行实现）

实现位置：
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Attribute/TcsAttributeManagerSubsystem.h`
- `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Attribute/TcsAttributeManagerSubsystem.cpp`

SourceHandle 创建：
- `CreateSourceHandle(SourceDefinition, SourceName, SourceTags, Instigator)`
- `CreateSourceHandleSimple(SourceName, Instigator)`

按来源应用/查询/撤销：
- `ApplyModifierWithSourceHandle(CombatEntity, SourceHandle, ModifierIds, OutModifiers)`
- `RemoveModifiersBySourceHandle(CombatEntity, SourceHandle)`
- `GetModifiersBySourceHandle(CombatEntity, SourceHandle, OutModifiers)`

兼容性说明（非常重要）：
- 旧的 `CreateAttributeModifier/ApplyModifier` 仍可用，但不会自动生成有效的 `SourceHandle.Id`；
- 若你需要 “按来源撤销/查询”，请使用 `CreateSourceHandle* + ApplyModifierWithSourceHandle` 这一套新流程。

### 4.4 索引（现行实现）

为了让 Remove/Get by SourceHandle 为 O(1)，AttributeComponent 维护索引：
- `TMap<int32, TArray<int32>> SourceHandleIdToModifierIndices;`

注意：
- 只有 `Modifier.SourceHandle.IsValid()` 的修改器才会进入索引；
- 因此旧 API 创建的修改器（无有效 SourceHandle）不会被 “Remove/Get by SourceHandle” 命中。

---

## 5. 与 State 的对齐（后续）

当前 State 模块尚未把 SourceHandle 写入 StateInstance；若后续需要统一 “技能结束/取消 -> 清理 State + Attribute”：
- 复用 `FTcsSourceHandle`（不要再造一套句柄结构）
- 让 StateInstance/StateApply 流程可选携带 SourceHandle（并进入 DebugSnapshot）

---

## 6. 网络同步要点（现行实现）

SourceHandle 本身支持网络序列化（`NetSerialize`），但是否会同步取决于你是否把包含它的字段做了 replication：
- 需要把包含 `FTcsSourceHandle` 的属性标记为 `Replicated/ReplicatedUsing`
- 并在 `GetLifetimeReplicatedProps` 中注册

额外注意：
- `SourceDefinition.DataTable` 会随句柄序列化（复制指针引用），要求客户端可加载对应 DataTable（或允许 SourceDefinition 为空，仅依赖 `Id/SourceName/Tags`）。
- `Instigator` 是条件序列化：保存时有效才会序列化；否则客户端可能为空。

---

## 7. 参考文档

- 使用指南：`Plugins/TireflyCombatSystem/Documents/SourceHandle使用指南.md`
- OpenSpec：`Plugins/TireflyCombatSystem/openspec/changes/implement-source-handle/`

