# Capability: 状态 Gate 关闭逻辑重构

## ADDED Requirements

### Requirement: 单一权威函数

系统 MUST 提供单一权威函数处理 Gate 关闭逻辑。

#### Scenario: HandleGateClosed 函数存在

**Given** 系统需要处理 Gate 关闭
**When** 定义 Gate 关闭逻辑
**Then** 存在函数 `HandleGateClosed`
**And** 函数接受槽位相关参数
**And** 函数统一处理所有 Gate 关闭场景

#### Scenario: 所有路径使用权威函数

**Given** 系统有多个可能触发 Gate 关闭的代码路径
**When** Gate 需要关闭
**Then** 所有路径都调用 `HandleGateClosed`
**And** 不存在重复的 Gate 关闭逻辑
**And** 行为一致

### Requirement: GateCloseBehavior 应用

系统 MUST 正确应用配置的 `GateCloseBehavior`。

#### Scenario: HangUp 行为

**Given** 槽位配置 `GateCloseBehavior = HangUp`
**And** 槽位有 Active 状态
**When** Gate 关闭
**Then** 所有 Active 状态变为 HangUp
**And** 触发 StateTree 挂起逻辑

#### Scenario: Pause 行为

**Given** 槽位配置 `GateCloseBehavior = Pause`
**And** 槽位有 Active 状态
**When** Gate 关闭
**Then** 所有 Active 状态暂停
**And** 触发 StateTree 暂停逻辑

#### Scenario: Cancel 行为

**Given** 槽位配置 `GateCloseBehavior = Cancel`
**And** 槽位有 Active 状态
**When** Gate 关闭
**Then** 所有 Active 状态被移除
**And** 调用 `RequestStateRemoval` 移除状态
**And** 移除原因为 `Cancelled`

### Requirement: 不变量保证

系统 MUST 保证 Gate 关闭后的不变量。

#### Scenario: Gate 关闭后无 Active 状态

**Given** 槽位的 Gate 关闭
**When** 执行 `HandleGateClosed`
**Then** 槽位内不存在 `Stage = SS_Active` 的状态
**And** 在开发模式下通过断言检查
**And** 违反时立即发现

### Requirement: 移除重复的 Gate 关闭逻辑

现有的分散在多处的 Gate 关闭逻辑 MUST 移除。

#### Scenario: 只保留权威函数

**Given** 系统有多处 Gate 关闭逻辑
**When** 重构完成
**Then** 只有 `HandleGateClosed` 函数包含 Gate 关闭逻辑
**And** 其它地方都调用此函数
**And** 不存在重复代码
