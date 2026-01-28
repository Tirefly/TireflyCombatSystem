# source-handle-event-attribution Specification

## Purpose
TBD - created by archiving change implement-source-handle. Update Purpose after archive.
## Requirements
### Requirement: 属性变化事件包含 SourceHandle 归因

系统 MUST在属性变化事件中提供完整的 SourceHandle 归因信息。

#### Scenario: 单个来源造成属性变化

**Given** 一个技能对目标造成伤害
**When** 触发属性变化事件
**Then** 事件的 ChangeSourceRecord 包含该技能的 SourceHandle 和伤害值

#### Scenario: 多个来源同时造成属性变化

**Given** 多个效果同时对目标造成伤害
**When** 触发属性变化事件
**Then** 事件的 ChangeSourceRecord 包含所有来源的 SourceHandle 和各自的伤害值

#### Scenario: 事件中可以访问 Source Definition

**Given** 属性变化事件包含 SourceHandle
**When** 通过 SourceHandle 调用 `GetSourceDefinition<T>()`
**Then** 可以获取完整的 Source Definition 信息

---

### Requirement: SourceHandle 数据访问便利性

系统 MUST提供便利的 API 使开发者能够轻松访问和处理 SourceHandle 数据,以便实现各种统计和追踪功能。

#### Scenario: 从事件中提取 SourceHandle 信息

**Given** 开发者监听属性变化事件
**When** 需要提取 SourceHandle 的各个字段
**Then** 可以直接访问 SourceHandle.SourceName、SourceHandle.Instigator、SourceHandle.SourceTags 等字段

#### Scenario: 按 SourceHandle 字段分组数据

**Given** 开发者收集了多个属性变化事件
**When** 需要按某个字段（如 SourceName 或 Instigator）分组统计
**Then** SourceHandle 提供的字段可以直接用作 TMap 的 key 或分组依据

#### Scenario: 处理 Instigator 已销毁的情况

**Given** SourceHandle 的 Instigator 可能已被销毁
**When** 开发者访问 Instigator 字段
**Then** TWeakObjectPtr 机制确保不会崩溃,开发者可以通过 SourceName 和 SourceTags 继续处理数据

#### Scenario: 开发者实现自定义统计功能（示例）

**Given** 开发者想实现"死亡前伤害追踪"功能
**When** 监听属性变化事件并收集 SourceHandle
**Then** 可以轻松实现:
- 按 SourceName 统计各技能造成的伤害
- 按 Instigator 统计各敌人造成的伤害
- 按 SourceTags 统计各类型造成的伤害
- 记录最近 N 秒的伤害事件
- 在角色死亡时分析伤害来源

**注**: 具体的统计和追踪逻辑由游戏项目实现,插件只提供数据访问能力

---

### Requirement: 事件归因的蓝图支持

系统 MUST支持在蓝图中访问事件归因信息。

#### Scenario: 蓝图中访问 ChangeSourceRecord

**Given** 在蓝图中监听属性变化事件
**When** 事件触发时
**Then** 可以遍历 ChangeSourceRecord，访问每个 SourceHandle 和对应的变化值

#### Scenario: 蓝图中查询 Source Definition

**Given** 在蓝图中获得 SourceHandle
**When** 需要查询完整的 Source Definition
**Then** 可以调用蓝图节点获取 Definition 信息

---

### Requirement: 性能优化

系统 MUST确保事件归因不会显著影响性能。

#### Scenario: 大量并发伤害的性能

**Given** 100 个敌人同时对目标造成伤害
**When** 触发属性变化事件
**Then** 事件处理时间不超过 1ms

#### Scenario: ChangeSourceRecord 的内存占用

**Given** 一次属性变化涉及 10 个不同来源
**When** 创建 ChangeSourceRecord
**Then** 内存占用合理（~600 bytes，每个 SourceHandle ~60 bytes）

