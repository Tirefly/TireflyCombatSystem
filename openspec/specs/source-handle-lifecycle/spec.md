# source-handle-lifecycle Specification

## Purpose
TBD - created by archiving change implement-source-handle. Update Purpose after archive.
## Requirements
### Requirement: 级联效果的 SourceHandle 传递

系统 MUST支持级联效果中 SourceHandle 的传递和继承。

**注**: 以下场景描述的是开发者可以实现的模式,插件提供必要的数据结构和API支持,但具体的传递逻辑由游戏项目实现。

#### Scenario: 技能生成陷阱，陷阱保存创建来源

**Given** 角色释放技能生成陷阱
**When** 陷阱被创建时
**Then** 陷阱保存技能的 SourceHandle 作为创建来源

#### Scenario: 陷阱造成伤害时继承 Source

**Given** 陷阱保存了创建它的技能 SourceHandle
**When** 陷阱对目标造成伤害时
**Then** 伤害的 SourceHandle 继承技能的 Source Definition，但 Instigator 是陷阱

#### Scenario: 级联效果的标签组合

**Given** 陷阱由技能生成，技能有 "Source.Skill" 标签
**When** 陷阱造成伤害时创建新的 SourceHandle
**Then** 新 SourceHandle 包含 "Source.Skill" 和 "Source.Trap" 标签

---

### Requirement: SourceHandle 的生命周期管理

系统 MUST正确管理 SourceHandle 的生命周期，避免内存泄漏。

#### Scenario: ModifierInstance 销毁时 SourceHandle 随之释放

**Given** ModifierInstance 包含 SourceHandle
**When** ModifierInstance 被移除并销毁
**Then** SourceHandle 随之释放，不造成内存泄漏

#### Scenario: Instigator 销毁后 SourceHandle 仍然有效

**Given** SourceHandle 的 Instigator 被销毁
**When** 访问 SourceHandle
**Then** SourceHandle 仍然有效，但 Instigator 引用失效（TWeakObjectPtr）

#### Scenario: Source Definition 持久化

**Given** SourceHandle 引用 DataTable 中的 Source Definition
**When** 运行时对象被销毁
**Then** Source Definition 仍然可以通过 DataTable 访问

---

### Requirement: 统一清理机制

系统 MUST支持使用同一 SourceHandle 统一清理多个效果。

#### Scenario: 技能结束时统一清理 State 和 Modifier

**Given** 技能同时产生 State 和 AttributeModifier，使用同一 SourceHandle
**When** 技能结束时调用统一清理
**Then** State 和 AttributeModifier 都被移除

#### Scenario: Buff 结束时移除所有效果

**Given** Buff 应用了多个 AttributeModifier，使用同一 SourceHandle
**When** Buff 结束时调用 `RemoveModifiersBySourceHandle()`
**Then** 所有该 Buff 的 Modifier 都被移除

---

### Requirement: 效果持久化和恢复（预留）

系统 MUST为后续的效果持久化和恢复预留扩展空间。

#### Scenario: SourceHandle 可序列化

**Given** 需要保存游戏状态
**When** 序列化 ModifierInstance
**Then** SourceHandle 的核心信息（Id、SourceName、SourceDefinition）可以被序列化

#### Scenario: 恢复时重建 SourceHandle

**Given** 从保存的游戏状态加载
**When** 反序列化 ModifierInstance
**Then** SourceHandle 的核心信息被正确恢复（Instigator 可能失效）

