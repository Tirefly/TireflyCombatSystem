# source-handle-network-sync Specification

## Purpose
TBD - created by archiving change implement-source-handle. Update Purpose after archive.
## Requirements
### Requirement: SourceHandle 网络序列化

系统 MUST支持 SourceHandle 的网络序列化，确保客户端和服务器的一致性。

#### Scenario: 服务器创建的 SourceHandle 同步到客户端

**Given** 服务器上创建了一个 SourceHandle 并应用效果
**When** 效果复制到客户端
**Then** 客户端接收到完整的 SourceHandle 信息（Id、SourceDefinition、SourceName、SourceTags）

#### Scenario: Instigator 引用正确映射

**Given** 服务器上的 SourceHandle 包含 Instigator 引用
**When** 复制到客户端
**Then** 客户端的 Instigator 引用正确映射到客户端的 Actor 实例

#### Scenario: 空 Instigator 的处理

**Given** 服务器上的 SourceHandle 的 Instigator 为空或已销毁
**When** 复制到客户端
**Then** 客户端的 Instigator 也为空，不会导致错误

---

### Requirement: 条件复制优化

系统 MUST优化网络带宽，只在必要时复制字段。

#### Scenario: Instigator 有效时才复制

**Given** SourceHandle 的 Instigator 有效
**When** 序列化 SourceHandle
**Then** Instigator 被包含在网络数据中

#### Scenario: Instigator 无效时不复制

**Given** SourceHandle 的 Instigator 无效或为空
**When** 序列化 SourceHandle
**Then** Instigator 不被包含在网络数据中，节省带宽

---

### Requirement: DataTable 引用的网络同步

系统 MUST正确同步 Source Definition 的 DataTable 引用。

#### Scenario: DataTable 引用同步

**Given** 服务器上的 SourceHandle 引用 DataTable 中的技能 Definition
**When** 复制到客户端
**Then** 客户端可以通过相同的 DataTable 和行名访问技能 Definition

#### Scenario: 客户端查询 Source Definition

**Given** 客户端接收到 SourceHandle
**When** 调用 `GetSourceDefinition<T>()`
**Then** 成功获取 DataTable 中的 Definition 数据

---

### Requirement: 网络环境下的伤害追踪

系统 MUST在网络环境下支持完整的伤害来源追踪。

#### Scenario: 客户端死亡时统计伤害来源

**Given** 客户端角色在网络游戏中死亡
**When** 统计死亡前的伤害来源
**Then** 可以正确识别：
- 造成伤害的玩家（Instigator）
- 使用的技能（SourceName 和 SourceDefinition）
- 伤害类型（SourceTags）

#### Scenario: 跨客户端的伤害归因

**Given** 玩家 A 在客户端 A 上，玩家 B 在客户端 B 上
**When** 玩家 A 对玩家 B 造成伤害
**Then** 客户端 B 上的伤害事件包含正确的 SourceHandle，指向玩家 A

---

### Requirement: 网络性能优化

系统 MUST确保网络同步不会显著增加带宽占用。

#### Scenario: SourceHandle 的网络数据大小

**Given** 一个包含完整信息的 SourceHandle
**When** 序列化为网络数据
**Then** 数据大小合理（~40-60 bytes，取决于字段内容）

#### Scenario: 批量效果的网络性能

**Given** 服务器同时应用 10 个效果到客户端
**When** 复制 SourceHandle 信息
**Then** 网络带宽占用合理（~400-600 bytes）

---

### Requirement: 网络错误处理

系统 MUST正确处理网络同步中的错误情况。

#### Scenario: DataTable 不存在时的处理

**Given** 客户端缺少服务器引用的 DataTable
**When** 接收到 SourceHandle
**Then** SourceHandle 仍然有效，但 `GetSourceDefinition<T>()` 返回 nullptr 并记录警告

#### Scenario: Instigator 映射失败时的处理

**Given** 客户端无法映射服务器的 Instigator NetGUID
**When** 接收到 SourceHandle
**Then** SourceHandle 仍然有效，但 Instigator 为空

---

### Requirement: 网络同步测试

系统 MUST提供网络同步的测试支持。

#### Scenario: 单机模拟网络环境测试

**Given** 在编辑器中使用 "Play as Listen Server" 和 "Play as Client"
**When** 服务器应用效果到客户端
**Then** 客户端正确接收 SourceHandle 信息

#### Scenario: 专用服务器环境测试

**Given** 在专用服务器环境中运行
**When** 服务器应用效果到多个客户端
**Then** 所有客户端正确接收 SourceHandle 信息

