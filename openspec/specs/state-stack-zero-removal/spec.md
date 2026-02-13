# state-stack-zero-removal Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
### Requirement: 允许StackCount为0

StackCount MUST 允许值为0，并在降为0时自动触发移除。

#### Scenario: SetStackCount允许0

**Given**:
- 状态实例的StackCount为1
- MaxStackCount为5

**When**: 调用 `SetStackCount(0)`

**Then**:
- StackCount被设置为0
- 不被clamp到1
- 自动触发 `RequestRemoval`

#### Scenario: StackCount为0触发移除

**Given**: 状态实例的StackCount为1

**When**: 调用 `SetStackCount(0)`

**Then**:
- 自动调用 `RequestRemoval(ETcsStateRemovalRequestReason::Expired)`
- 状态进入移除流程
- 不继续执行后续逻辑

#### Scenario: RemoveStack降为0触发移除

**Given**:
- 状态实例的StackCount为2

**When**: 调用 `RemoveStack(2)`

**Then**:
- StackCount降为0
- 自动触发 `RequestRemoval`
- 状态被移除

#### Scenario: AddStack从0恢复

**Given**:
- 状态实例的StackCount为0（已请求移除但未完成）

**When**: 调用 `AddStack(1)`

**Then**:
- 如果状态尚未完全移除，StackCount恢复为1
- 如果状态已移除，操作失败

### Requirement: 更新StackCount验证逻辑

所有StackCount相关的验证 MUST 允许0。

#### Scenario: GetStackCount返回0

**Given**: 状态实例的StackCount为0

**When**: 调用 `GetStackCount()`

**Then**:
- 返回0
- 不返回1或其他默认值

#### Scenario: CanStack检查MaxStackCount

**Given**:
- 状态实例的StackCount为0
- MaxStackCount为5

**When**: 调用 `CanStack()`

**Then**:
- 返回true（0 < 5）
- 允许堆叠

#### Scenario: 合并器处理StackCount=0

**Given**:
- 合并器收到StackCount为0的状态

**When**: 执行合并逻辑

**Then**:
- 正确处理StackCount=0的情况
- 不假设StackCount>=1
- 可能跳过该状态（因为即将被移除）

### Requirement: 移除原因选择

StackCount降为0时的移除原因 MUST 明确。

#### Scenario: 自然耗尽使用Expired

**Given**: 通过RemoveStack自然减少到0

**When**: 触发移除

**Then**:
- 使用 `ETcsStateRemovalRequestReason::Expired`
- 表示堆叠自然耗尽

#### Scenario: 主动设置为0使用Removed

**Given**: 通过SetStackCount(0)主动设置

**When**: 触发移除

**Then**:
- 使用 `ETcsStateRemovalRequestReason::Removed`
- 表示主动移除

### Requirement: 日志记录

StackCount降为0触发移除时 MUST 记录日志。

#### Scenario: 记录移除日志

**Given**: StackCount降为0

**When**: 触发移除

**Then**:
- 记录Warning级别日志
- 日志包含状态名称
- 日志包含移除原因
- 日志包含StackCount变化（从X到0）

