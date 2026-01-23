# TCS Attribute/State SourceHandle 机制设计与落地方案

目标：把 AttributeModifier 的 “SourceName” 从“仅归因字段”升级为“可用于生命周期管理”的统一来源句柄（SourceHandle），并与 State 的后续“来源句柄”机制对齐。

---

## 1. 设计目标与约束

### 1.1 目标

- **唯一性**：同一 SourceName 的多次应用可以被区分（例如技能同名多次触发、同一 Buff 多次叠加）。
- **可撤销**：支持按来源撤销（Remove by SourceHandle），避免“只能按 ModifierName/Tag 粗删”。
- **可追踪**：事件里能提供“来源归因”，同时不牺牲调试友好度（仍可按 SourceName 汇总）。
- **可统一**：同一来源可以同时产生 State + AttributeModifier，二者共享同一 SourceHandle（便于技能结束时统一清理）。
- **不强制**：不要求所有项目都必须维护复杂对象引用；句柄可以只包含 Id + DebugName。

### 1.2 约束/原则

- **不依赖资产迁移**：TCS 仍处开发期，可接受 C++/蓝图签名调整，但尽量保持旧接口可用（包装/重载）。
- **不要求强制 GC 管理**：句柄内部引用尽量使用 `TWeakObjectPtr`，避免延长生命周期。
- **网络/复制**：本方案优先满足单机/本地逻辑一致性；如后续需要网络同步，可在句柄中增加可复制字段（如 `FGuid`）。

---

## 2. SourceHandle 数据结构

### 2.1 推荐结构（最小可用 + 可扩展）

新增一个公共结构（建议放在 TCS 公共头文件目录，后续 State/Attribute 共用）：

- `FTcsSourceHandle`（`USTRUCT(BlueprintType)`）
  - `FGuid Id`：全局唯一来源 id（强烈推荐）
  - `FName SourceName`：用于人类可读/汇总（非唯一）
  - `TWeakObjectPtr<UObject> SourceObject`：可选（技能对象、效果对象、Buff 资产实例等）
  - `TWeakObjectPtr<AActor> Instigator`：可选（便于“按施加者撤销”或 debug）
  - `FGameplayTagContainer SourceTags`：可选（用于分类/筛选）

配套：
- `bool IsValid() const`：至少 `Id.IsValid()`
- `FString ToDebugString() const`：输出 `SourceName + Id`（必要时追加 Instigator）
- 作为 `TMap` key 时（若需要）：提供 `==` 与 `GetTypeHash`（以 `Id` 为主）

### 2.2 Id 的生成策略

推荐使用 `FGuid::NewGuid()` 生成，优势：
- 低耦合、无需全局计数器；
- 日志/追踪稳定；
- 未来做网络同步也更容易。

如果你更偏好连续 id，也可用 `int64` 单调递增（WorldSubsystem 内部维护），但要确保：
- PIE 多世界隔离；
- 线程安全（或只在 GameThread 生成）。

---

## 3. Attribute 模块改造点（代码级）

### 3.1 修改的数据结构

#### 3.1.1 `FTcsAttributeModifierInstance`

新增字段（推荐）：
- `FTcsSourceHandle SourceHandle;`

保留字段（短期兼容）：
- `FName SourceName`（旧字段继续存在，但作为“Debug/汇总字段”）

建议规则：
- 如果提供了 `SourceHandle`，则 `SourceName` = `SourceHandle.SourceName`（保持一致）。

#### 3.1.2 `FTcsAttributeChangeEventPayload`

现状：
- `TMap<FName, float> ChangeSourceRecord;`

建议升级（两种方案，二选一）：

**方案（彻底升级，开发期可接受）**
- 直接把 `ChangeSourceRecord` key 改为 `FTcsSourceHandle`（会影响蓝图/序列化）

### 3.2 新增/改造的 API（建议）

在 `UTcsAttributeManagerSubsystem` 增加重载：

- `ApplyModifier(CombatEntity, Modifiers, SourceHandle, Instigator, Target)`
- `RemoveModifiersBySourceHandle(CombatEntity, SourceHandle, OptionalFilter...)`
- `GetModifiersBySourceHandle(CombatEntity, SourceHandle, OutModifiers)`

同时保留旧接口：
- `SourceName` 版本内部构造一个 `FTcsSourceHandle`（只填 `SourceName` + 新 `Id`），这样旧用法也能享受“可撤销”的能力（前提是调用方保留返回句柄）。

### 3.3 句柄返回机制（关键）

为了让调用方能“撤销”，Apply 类 API 必须能返回句柄。建议：

- `ApplyModifier...` 返回 `bool` + out param
- 若一次 Apply 要同时创建多个来源（少见），则返回 `TArray<FTcsSourceHandle>`

---

## 4. 与 State 的统一（设计对齐）

### 4.1 同一来源产生 State + AttributeModifier

目标：技能系统创建一个 `FTcsSourceHandle`，随后：
- Apply Skill State：把 `SourceHandle` 写入 StateInstance（后续会实现）
- Apply AttributeModifiers：把同一个 `SourceHandle` 写入 ModifierInstance

好处：
- “技能结束/被取消”时，可以用同一个 `SourceHandle` 做统一清理：
  - Remove Skill State（或 RequestRemoval）
  - Remove AttributeModifiersBySourceHandle

### 4.2 移除策略建议

- **由调用方决定**：TCS 提供按句柄删除能力，但不强制什么时候删。
- 对于“Buff 类 StateTree 停止时移除 Buff 效果”，建议在 StateTree 中显式调用：
  - 提供可复用的 StateTreeTask 触发“按 SourceHandle 撤销 AttributeModifiers”

---

## 5. 迁移/兼容策略

### 5.1 旧项目/旧代码

- 旧调用只提供 `SourceName`：仍可工作（归因/统计维持不变）
- 若希望撤销：需要调用方保留 `FTcsSourceHandle`（建议新 API 返回）

### 5.2 文档与示例

建议在示例中形成固定流程：
1) 创建 `SourceHandle`（技能激活/效果触发时）
2) Apply State（写入同一句柄）
3) Apply AttributeModifiers（写入同一句柄）
4) 结束/取消时：按句柄撤销（State + Attribute）

---

## 6. 分步落地计划（建议顺序）

1) 新增 `FTcsSourceHandle` 公共结构与基础工具函数（IsValid/ToDebugString）。
2) Attribute：
   - `FTcsAttributeModifierInstance` 增加 `SourceHandle`
   - 新增 Apply/Remove by SourceHandle 的 API（同时保留旧 API）
   - 事件 payload 增加“可选的按句柄归因记录”（方案 A）
3) 示例/文档：
   - 添加最小示例（技能开关 -> 句柄 -> Apply/Remove）
4) State（后续）：
   - StateInstance 增加 `SourceHandle`
   - 统一移除流程与 ConfirmRemoval Task 对接

---

## 7. 风险与注意事项

- `TWeakObjectPtr` 在对象销毁后会失效：句柄仍然有效（靠 `Id`），但 debug 信息会少一部分。
- 如果未来要做网络同步：建议把 `FGuid` 作为复制字段；不要依赖指针字段。
- 如果要作为 `TMap` key：建议 key 只用 `FGuid`，避免 USTRUCT hash/蓝图兼容问题。
