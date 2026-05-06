# TCS 状态槽激活流程与状态阶段转换

## 概述

本文档描述 `UTcsStateManagerSubsystem::UpdateStateSlotActivation` 的完整执行流程，以及状态实例（`UTcsStateInstance`）的合法阶段转换矩阵。

---

## 一、UpdateStateSlotActivation 执行流程

`UpdateStateSlotActivation` 是状态槽激活逻辑的核心函数，每次槽内状态发生变化时触发。函数按以下 8 步串行执行：

```
触发条件：状态被添加 / 移除 / 合并 / Gate 变化 / 优先级变化
         ↓
[Step 1] 防重入保护
         ↓
[Step 2] 清理过期状态
         ↓
[Step 3] 按优先级排序
         ↓
[Step 4] 叠层合并（Merging）
         ↓
[Step 5] Gate 一致性强制 ──── Gate 关闭 ──→ 按 GateClosePolicy 收敛 → 提前返回
         ↓ Gate 开启
[Step 6] 激活模式处理（ActivationMode）
         ↓
[Step 7] 最终清理
         ↓
[Step 8] 通知 + 排空队列
```

### Step 1 — 防重入保护

- 检查 `bIsUpdatingSlotActivation` 标志
- 若已在更新中，将 `(StateComponent, SlotTag)` 加入 `PendingSlotActivationUpdates` 队列，函数提前返回
- 防止在 Step 6/8 的事件回调中触发递归更新

### Step 2 — 清理过期状态

- 调用 `ClearStateSlotExpiredStates`
- 移除槽内所有 `Stage == SS_Expired` 的实例引用（实例本身已由 `FinalizeStateRemoval` 处理）

### Step 3 — 按优先级排序

- 调用 `SortStatesByPriority`
- 对槽内所有存活状态按 `UTcsStateDefinition::Priority` **降序**排列
- 排序结果影响 Step 4 的合并主实例选择和 Step 6 的激活判断

### Step 4 — 叠层合并（Merging）

- 调用 `ProcessStateSlotMerging`
- 按 `StateDefId` 分组，对每组调用对应的 `UTcsStateMerger` 策略
- 合并结果：每组保留一个"主实例"（通常是最高优先级或最新的），其余实例被移除
- 注意：`HasPendingRemovalRequest()` 为 true 的实例不参与合并，始终保留直到 StateTree 停止

### Step 5 — Gate 一致性强制

- 调用 `EnforceSlotGateConsistency`
- **Gate 开启**：无操作，继续执行
- **Gate 关闭**：对槽内所有 Active 状态按 `GateClosePolicy` 执行：
  - `SSGCP_HangUp`：调用 `HangUpState`（持续时间继续计时，StateTree 暂停）
  - `SSGCP_Pause`：调用 `PauseState`（持续时间冻结，StateTree 暂停）
  - `SSGCP_Cancel`：调用 `CancelState`（直接移除）
- Gate 关闭时，调用 `CleanupInvalidStates` 后**提前返回**，不执行 Step 6

### Step 6 — 激活模式处理

- 调用 `ProcessStateSlotByActivationMode`，根据 `UTcsStateSlotDefinition::ActivationMode`：

#### SSAM_PriorityOnly（优先级激活模式）

```
槽内状态（已按优先级排序）
  ├─ 最高优先级状态 → 激活（ActivateState）
  └─ 其余状态 → 按 PreemptionPolicy 处理：
       ├─ SPP_HangUpLowerPriority  → HangUpState
       ├─ SPP_PauseLowerPriority   → PauseState
       └─ SPP_CancelLowerPriority  → CancelState
```

同优先级状态由 `SamePriorityPolicy` 策略决定行为。

#### SSAM_AllActive（全部激活模式）

- 所有状态均调用 `ActivateState`，无抢占逻辑

### Step 7 — 最终清理

- 调用 `CleanupInvalidStates`
- 移除槽内所有 `nullptr` 或 `Stage == SS_Expired` 的实例

### Step 8 — 通知与排空队列

- 调用 `StateComponent->OnStateSlotChanged(SlotTag)` 广播槽位变化事件
- 清除 `bIsUpdatingSlotActivation = false`
- 调用 `DrainPendingSlotActivationUpdates`，处理 Step 1 中积压的请求（最多 10 轮防止无限循环）

---

## 二、状态阶段转换矩阵

状态实例的生命周期由 `ETcsStateStage` 枚举描述，共 5 个阶段：

| 阶段 | 枚举值 | 含义 |
|------|--------|------|
| 未激活 | `SS_Inactive` | 已加入槽位但尚未激活，或被停用 |
| 激活 | `SS_Active` | 正在运行，StateTree 执行中 |
| 挂起 | `SS_HangUp` | 被高优先级抢占或 Gate 关闭（HangUp 策略），持续时间**继续计时** |
| 暂停 | `SS_Pause` | 被高优先级抢占或 Gate 关闭（Pause 策略），持续时间**冻结** |
| 过期 | `SS_Expired` | 终态，已完成清理，等待 GC |

### 合法转换表

```
SS_Inactive ──────────────────────────────→ SS_Active     （激活）
SS_Active   ──────────────────────────────→ SS_Inactive   （停用）
SS_Active   ──────────────────────────────→ SS_HangUp     （挂起）
SS_Active   ──────────────────────────────→ SS_Pause      （暂停）
SS_Active   ──────────────────────────────→ SS_Expired    （过期/取消）
SS_HangUp   ──────────────────────────────→ SS_Active     （恢复）
SS_HangUp   ──────────────────────────────→ SS_Expired    （过期/取消）
SS_Pause    ──────────────────────────────→ SS_Active     （恢复）
SS_Pause    ──────────────────────────────→ SS_Expired    （过期/取消）
SS_Expired  → 不允许任何转换（终态）
```

### HangUp vs Pause 的区别

| 行为 | HangUp | Pause |
|------|--------|-------|
| 持续时间计时 | **继续** | **冻结** |
| 叠层合并参与 | 是 | 是 |
| StateTree 执行 | 暂停 | 暂停 |
| 典型触发场景 | 优先级抢占（默认）、Gate 关闭（HangUp 策略） | Gate 关闭（Pause 策略）、优先级抢占（Pause 策略） |

---

## 三、Gate 机制说明

Gate 是槽位级别的开关，由顶层 StateTree 的状态变化驱动（通过 `RefreshSlotsForStateChange`）。

- **Gate 开启**：槽位正常工作，状态可以被激活
- **Gate 关闭**：槽位被锁定，所有 Active 状态按 `GateClosePolicy` 收敛，新进入的状态也不会被激活

Gate 的典型用途：用顶层 StateTree 的状态（如"眩晕"、"死亡"）来控制某类槽位（如"技能槽"）的整体开关。

---

## 四、相关源码位置

| 函数 | 文件 |
|------|------|
| `UpdateStateSlotActivation` | `Private/State/TcsStateManagerSubsystem.cpp` |
| `ProcessStateSlotMerging` | `Private/State/TcsStateManagerSubsystem.cpp` |
| `EnforceSlotGateConsistency` | `Private/State/TcsStateManagerSubsystem.cpp` |
| `ProcessStateSlotByActivationMode` | `Private/State/TcsStateManagerSubsystem.cpp` |
| `ActivateState / DeactivateState / HangUpState / PauseState / ResumeState` | `Private/State/TcsStateManagerSubsystem.cpp` |
| `SetCurrentStage` | `Private/State/TcsStateInstance.cpp` |
| `ETcsStateStage` | `Public/State/TcsStateInstance.h` |
| `ETcsStateSlotActivationMode` | `Public/State/TcsStateSlot.h` |
| `ETcsStateSlotGateClosePolicy` | `Public/State/TcsStateSlot.h` |
| `ETcsStatePreemptionPolicy` | `Public/State/TcsStateSlot.h` |
