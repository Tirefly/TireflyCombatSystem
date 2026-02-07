# state-pause-query Specification

## Purpose
修复 `UTcsStateInstance::IsStateTreePaused()` 的语义不一致问题，使其正确反映 StateTree 的暂停状态。

## ADDED Requirements

### Requirement: IsStateTreePaused() 正确反映 Pause/HangUp 状态

系统 MUST 确保 `IsStateTreePaused()` 函数在 StateInstance 处于 Pause 或 HangUp 阶段时返回 `true`，而不依赖 `bStateTreeRunning` 标志。

#### Scenario: Pause 状态下查询返回 true

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_Pause`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `true`

#### Scenario: HangUp 状态下查询返回 true

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_HangUp`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `true`

#### Scenario: Active 状态下查询返回 false

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_Active`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `false`

#### Scenario: Inactive 状态下查询返回 false

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_Inactive`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `false`

#### Scenario: Expired 状态下查询返回 false

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_Expired`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `false`

---

### Requirement: IsStateTreePaused() 不依赖 bStateTreeRunning 标志

系统 MUST 确保 `IsStateTreePaused()` 的实现只依赖 Stage 状态，不检查 `bStateTreeRunning` 标志。

#### Scenario: Pause 状态下即使 bStateTreeRunning 为 true 也返回 true

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_Pause`
**And** `bStateTreeRunning` 为 `true`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `true`

#### Scenario: HangUp 状态下即使 bStateTreeRunning 为 true 也返回 true

**Given** StateInstance 的 Stage 为 `ETcsStateStage::SS_HangUp`
**And** `bStateTreeRunning` 为 `true`
**When** 调用 `IsStateTreePaused()`
**Then** 返回 `true`

---

### Requirement: PauseStateTree() 和 IsStateTreePaused() 语义一致

系统 MUST 确保 `PauseStateTree()` 的行为与 `IsStateTreePaused()` 的查询结果语义一致。

#### Scenario: 调用 PauseStateTree() 后查询返回 true

**Given** StateInstance 处于 Active 状态且 StateTree 正在运行
**When** 调用 `PauseStateTree()`（将 StateInstance 从 TickScheduler 移除）
**And** StateInstance 的 Stage 被设置为 `SS_Pause` 或 `SS_HangUp`
**Then** 调用 `IsStateTreePaused()` 返回 `true`

#### Scenario: 调用 ResumeStateTree() 后查询返回 false

**Given** StateInstance 处于 Pause 或 HangUp 状态
**When** 调用 `ResumeStateTree()`（将 StateInstance 重新加入 TickScheduler）
**And** StateInstance 的 Stage 被设置为 `SS_Active`
**Then** 调用 `IsStateTreePaused()` 返回 `false`

---

## Implementation Notes

- 修改 `TcsState.h` 中 `IsStateTreePaused()` 的实现
- 移除 `&& !bStateTreeRunning` 条件
- 保持 `bStateTreeRunning` 标志用于区分 StateTree 是否已启动（Start/Stop）
- Stage 状态已经明确表示了 Pause/HangUp 状态，无需额外标志

## Related Capabilities

无
