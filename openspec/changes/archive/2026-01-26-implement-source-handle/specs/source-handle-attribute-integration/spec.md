# Spec: Source Handle Attribute Integration

## ADDED Requirements

### Requirement: AttributeModifierInstance 集成 SourceHandle

系统 MUST在 AttributeModifierInstance 中添加 SourceHandle 字段，同时保持向后兼容。

#### Scenario: 新 API 创建的 Modifier 包含 SourceHandle

**Given** 使用新的 `ApplyModifierWithSourceHandle()` API
**When** 应用属性修改器
**Then** 创建的 ModifierInstance 包含完整的 SourceHandle 信息

#### Scenario: 旧 API 创建的 Modifier 自动生成 SourceHandle

**Given** 使用旧的 `ApplyModifier()` API
**When** 应用属性修改器
**Then** 系统自动为 ModifierInstance 生成 SourceHandle，使用 Modifier 的 SourceName

#### Scenario: SourceName 字段保持同步

**Given** ModifierInstance 包含 SourceHandle
**When** 访问 ModifierInstance 的 SourceName 字段
**Then** SourceName 与 SourceHandle.SourceName 一致

---

### Requirement: 按 SourceHandle 应用修改器

系统 MUST提供按 SourceHandle 应用属性修改器的 API。

#### Scenario: 应用带 SourceHandle 的修改器

**Given** 一个有效的 SourceHandle 和修改器定义列表
**When** 调用 `ApplyModifierWithSourceHandle()`
**Then** 修改器成功应用到目标实体，并返回创建的 ModifierInstance 列表

#### Scenario: 应用失败时的错误处理

**Given** 目标实体无效或没有 AttributeComponent
**When** 调用 `ApplyModifierWithSourceHandle()`
**Then** 返回 false，OutModifiers 为空，并记录错误日志

---

### Requirement: 按 SourceHandle 移除修改器

系统 MUST提供按 SourceHandle 精确移除属性修改器的能力。

#### Scenario: 移除特定 SourceHandle 的所有修改器

**Given** 实体上有多个来源的修改器，包括目标 SourceHandle
**When** 调用 `RemoveModifiersBySourceHandle()` 指定目标 SourceHandle
**Then** 只移除匹配该 SourceHandle.Id 的修改器，其他修改器不受影响

#### Scenario: 移除不存在的 SourceHandle

**Given** 实体上没有匹配的 SourceHandle
**When** 调用 `RemoveModifiersBySourceHandle()`
**Then** 返回 false，不影响现有修改器

#### Scenario: 同一 SourceName 多次应用的独立管理

**Given** 同一技能多次触发，每次生成不同的 SourceHandle
**When** 移除其中一个 SourceHandle 的修改器
**Then** 只移除该次触发的修改器，其他次触发的修改器保留

---

### Requirement: 按 SourceHandle 查询修改器

系统 MUST提供按 SourceHandle 查询属性修改器的能力。

#### Scenario: 查询特定 SourceHandle 的修改器

**Given** 实体上有多个来源的修改器
**When** 调用 `GetModifiersBySourceHandle()` 指定目标 SourceHandle
**Then** 返回所有匹配该 SourceHandle.Id 的 ModifierInstance

#### Scenario: 查询不存在的 SourceHandle

**Given** 实体上没有匹配的 SourceHandle
**When** 调用 `GetModifiersBySourceHandle()`
**Then** 返回 false，OutModifiers 为空

---

### Requirement: 向后兼容性

系统 MUST保持与现有代码的向后兼容性。

#### Scenario: 旧代码继续工作

**Given** 现有代码使用旧的 `ApplyModifier()` API
**When** 升级到新版本后运行
**Then** 代码正常工作，无需修改

#### Scenario: 旧 API 创建的 Modifier 可以按 SourceHandle 撤销

**Given** 使用旧 API 创建的 Modifier（自动生成 SourceHandle）
**When** 保存返回的 SourceHandle 并调用 `RemoveModifiersBySourceHandle()`
**Then** 成功移除该 Modifier

---

### Requirement: 蓝图支持

系统 MUST支持在蓝图中使用 SourceHandle 相关 API。

#### Scenario: 蓝图中创建 SourceHandle

**Given** 在蓝图中需要应用效果
**When** 调用 `CreateSourceHandle` 或 `CreateSourceHandleSimple` 节点
**Then** 成功创建 SourceHandle 并可以传递给其他节点

#### Scenario: 蓝图中应用和移除修改器

**Given** 在蓝图中有一个 SourceHandle
**When** 调用 `ApplyModifierWithSourceHandle` 和 `RemoveModifiersBySourceHandle` 节点
**Then** 成功应用和移除修改器
