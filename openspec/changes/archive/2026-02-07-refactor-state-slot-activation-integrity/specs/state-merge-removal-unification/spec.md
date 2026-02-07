# Capability: 状态合并移除统一化

## ADDED Requirements

### Requirement: 合并移除使用 RequestStateRemoval

状态合并逻辑 MUST 使用 `RequestStateRemoval` 而不是直接 Finalize。

#### Scenario: 合并淘汰走统一路径

**Given** 两个状态需要合并
**And** 合并策略决定淘汰其中一个状态
**When** 执行合并逻辑
**Then** 调用 `RequestStateRemoval` 而不是直接 `FinalizeStateRemoval`
**And** 移除原因设置为 `Custom`
**And** 自定义原因设置为 `"MergedOut"`

#### Scenario: StateTree 退场逻辑执行

**Given** 一个状态被合并淘汰
**And** 该状态有 StateTree 退场逻辑
**When** 调用 `RequestStateRemoval`
**Then** StateTree 退场逻辑有机会执行
**And** 状态正确清理

### Requirement: 避免直接 Finalize 导致的再入

合并逻辑 MUST NOT 直接调用 `FinalizeStateRemoval`，以避免递归调用 `UpdateStateSlotActivation`。

#### Scenario: 不会导致递归调用

**Given** 系统正在执行槽位激活更新
**And** 在更新过程中发生状态合并
**When** 合并逻辑淘汰一个状态
**Then** 不会立即触发 `UpdateStateSlotActivation`
**And** 移除请求被延迟处理
**And** 不会发生递归调用
