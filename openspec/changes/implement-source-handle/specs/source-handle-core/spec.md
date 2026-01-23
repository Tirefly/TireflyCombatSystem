# Spec: Source Handle Core

## ADDED Requirements

### Requirement: SourceHandle 结构定义

系统 MUST提供一个统一的来源句柄结构 `FTcsSourceHandle`，用于标识和追踪效果的来源。

#### Scenario: 创建包含完整信息的 SourceHandle

**Given** 技能系统需要应用一个效果
**When** 创建 SourceHandle 时提供技能 Definition、施加者和标签
**Then** SourceHandle 包含唯一 ID、Source Definition 引用、施加者引用和分类标签

#### Scenario: 创建简化的 SourceHandle（用户自定义）

**Given** 用户自定义装备效果没有 DataTable Definition
**When** 创建 SourceHandle 时只提供效果名称和施加者
**Then** SourceHandle 包含唯一 ID、效果名称和施加者引用，但 Source Definition 为空

#### Scenario: 验证 SourceHandle 有效性

**Given** 一个已创建的 SourceHandle
**When** 调用 `IsValid()` 方法
**Then** 返回 true 当且仅当 ID >= 0

---

### Requirement: Source vs Instigator 概念区分

系统 MUST明确区分 Source（效果定义）和 Instigator（实际施加者）的概念。

#### Scenario: 技能直接造成伤害

**Given** 角色释放技能直接对目标造成伤害
**When** 创建 SourceHandle
**Then** Source 是技能 Definition，Instigator 是角色

#### Scenario: 技能生成陷阱，陷阱造成伤害

**Given** 角色释放技能生成陷阱，陷阱对目标造成伤害
**When** 陷阱创建伤害的 SourceHandle
**Then** Source 是技能 Definition（继承自陷阱的创建来源），Instigator 是陷阱

---

### Requirement: 唯一 ID 生成

系统 MUST为每个 SourceHandle 生成全局唯一的 ID。

#### Scenario: 多次创建 SourceHandle 生成不同 ID

**Given** AttributeManagerSubsystem 已初始化
**When** 连续调用 `CreateSourceHandle()` 多次
**Then** 每次返回的 SourceHandle 具有不同的 ID

#### Scenario: PIE 多世界 ID 隔离

**Given** 在 PIE 模式下运行多个 World
**When** 在不同 World 中创建 SourceHandle
**Then** 每个 World 的 ID 计数器独立，不会冲突

---

### Requirement: Source Definition 引用

系统 MUST支持通过 FDataTableRowHandle 引用 Source Definition。

#### Scenario: 引用技能 Definition

**Given** 技能定义在 DataTable 中
**When** 创建 SourceHandle 时提供 DataTable 引用和行名
**Then** SourceHandle 可以通过 `GetSourceDefinition<T>()` 获取完整的技能 Definition

#### Scenario: 处理空的 Source Definition

**Given** 用户自定义效果没有 DataTable Definition
**When** 创建 SourceHandle 时 Source Definition 为空
**Then** `GetSourceDefinition<T>()` 返回 nullptr，但 SourceHandle 仍然有效

---

### Requirement: 调试支持

系统 MUST提供调试友好的字符串输出。

#### Scenario: 生成调试字符串

**Given** 一个包含完整信息的 SourceHandle
**When** 调用 `ToDebugString()` 方法
**Then** 返回格式为 "[SourceName|ID] Instigator=ActorName" 的字符串

#### Scenario: Instigator 已销毁时的调试字符串

**Given** 一个 SourceHandle 的 Instigator 已被销毁
**When** 调用 `ToDebugString()` 方法
**Then** 返回格式为 "[SourceName|ID]" 的字符串，不包含 Instigator 信息

---

### Requirement: 作为 TMap Key 使用

系统 MUST支持 SourceHandle 作为 TMap 的 key。

#### Scenario: 使用 SourceHandle 作为 TMap key

**Given** 需要按 SourceHandle 索引数据
**When** 创建 `TMap<FTcsSourceHandle, float>`
**Then** 可以正常插入、查询和删除元素

#### Scenario: 基于 ID 的哈希和比较

**Given** 两个 SourceHandle 具有相同的 ID
**When** 比较它们或计算哈希值
**Then** 它们被认为是相等的，具有相同的哈希值
