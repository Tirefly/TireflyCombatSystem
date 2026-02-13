# state-tree-restart-semantics Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
### Requirement: 状态激活时的StateTree启动

状态激活时 MUST 使用正确的启动模式。

#### Scenario: 新状态激活使用冷启动

**Given**: 新创建的状态实例被激活

**When**: 状态进入Active阶段

**Then**:
- 调用 `RestartStateTree()`
- StateTree以全新状态启动

#### Scenario: 从Pause恢复使用热恢复

**Given**: 状态从Pause恢复

**When**: 状态重新进入Active阶段

**Then**:
- 调用 `StartStateTree()`
- StateTree继续之前的执行

#### Scenario: 从HangUp恢复使用热恢复

**Given**: 状态从HangUp恢复

**When**: 状态重新进入Active阶段

**Then**:
- 调用 `StartStateTree()`
- StateTree继续之前的执行

