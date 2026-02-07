# Proposal: fix-semantic-inconsistencies

## Problem Statement

TCS 插件的 State 和 Attribute 模块存在两个语义不一致的问题，影响了 API 的正确性和事件通知的完整性：

### 问题 1：UTcsStateInstance::IsStateTreePaused() 语义不一致

**位置**: `TcsState.h:636`、`TcsState.cpp:913`

**问题描述**:
- `PauseStateTree()` 的实际行为是将 StateInstance 从 TickScheduler 中移除，但**不会**修改 `bStateTreeRunning` 标志
- `IsStateTreePaused()` 的判断条件要求 `Stage` 为 `SS_HangUp` 或 `SS_Pause` **且** `bStateTreeRunning == false`
- 由于 Pause/HangUp 状态下 `bStateTreeRunning` 通常仍为 `true`，导致该函数几乎恒返回 `false`

**影响**:
- 蓝图和 C++ 代码无法正确查询 StateTree 的暂停状态
- 与 `PauseStateTree()` 的实际行为语义不符
- 可能导致依赖此查询的逻辑出现错误

### 问题 2：Attribute Range Enforcement 缺少边界事件

**位置**: `TcsAttributeManagerSubsystem.cpp:1132-1256`

**问题描述**:
- `EnforceAttributeRangeConstraints()` 在 Range 约束变化时会 Clamp BaseValue 和 CurrentValue
- 会广播 `BroadcastAttributeBaseValueChangeEvent` 和 `BroadcastAttributeValueChangeEvent`
- 但**不会**调用 `BroadcastAttributeReachedBoundaryEvent`

**对比**:
- 在 `RecalculateAttributeBaseValues()` (line 877) 和 `RecalculateAttributeCurrentValues()` (line 982) 中
- 当值达到边界时都会调用 `BroadcastAttributeReachedBoundaryEvent`

**影响**:
- 当动态上限（如 MaxHealth）降低，导致当前值（如 Health）被 Clamp 到新上限时
- 虽然会触发 Value Changed 事件，但不会触发 `OnAttributeReachedBoundary` 事件
- 游戏逻辑可能依赖边界事件来触发特殊效果（如满血触发 Buff），这些效果在被动 Clamp 场景下不会触发
- 语义不一致：主动修改达到边界会触发事件，被动 Clamp 到边界却不会

## Proposed Solution

### 方案 1：修复 IsStateTreePaused() 实现

移除 `IsStateTreePaused()` 中的 `!bStateTreeRunning` 条件，使其正确反映 Pause/HangUp 状态：

```cpp
// 修改前
bool IsStateTreePaused() const
{
    return (Stage == ETcsStateStage::SS_HangUp || Stage == ETcsStateStage::SS_Pause) && !bStateTreeRunning;
}

// 修改后
bool IsStateTreePaused() const
{
    return (Stage == ETcsStateStage::SS_HangUp || Stage == ETcsStateStage::SS_Pause);
}
```

**理由**:
- Pause 的语义是"暂停 Tick，但保留运行状态"
- Stage 已经明确表示了 Pause/HangUp 状态
- `bStateTreeRunning` 应该只用于区分 StateTree 是否已启动（Start/Stop）

### 方案 2：在 Range Enforcement 中添加边界事件

在 `EnforceAttributeRangeConstraints()` 中检测值是否被 Clamp 到边界，如果是则触发 `BroadcastAttributeReachedBoundaryEvent`：

**实现要点**:
1. 在 Clamp 操作后，检测新值是否等于边界值（Min 或 Max）
2. 如果达到边界，调用 `BroadcastAttributeReachedBoundaryEvent`
3. 保持与 `RecalculateAttributeBaseValues()` 和 `RecalculateAttributeCurrentValues()` 中的边界检测逻辑一致

**理由**:
- 保持事件通知的完整性和一致性
- 无论是主动修改还是被动 Clamp，达到边界都应该触发边界事件
- 游戏逻辑可以依赖边界事件来实现特殊效果

## Impact Analysis

### 影响范围

**问题 1**:
- 修改文件：`TcsState.h`
- 影响：所有调用 `IsStateTreePaused()` 的代码
- 风险：低（修复语义不一致，使查询结果更准确）

**问题 2**:
- 修改文件：`TcsAttributeManagerSubsystem.cpp`
- 影响：所有监听 `OnAttributeReachedBoundary` 事件的代码
- 风险：低（增加事件通知，不影响现有逻辑）

### 向后兼容性

**问题 1**:
- 可能影响依赖 `IsStateTreePaused()` 错误行为的代码
- 但由于原实现几乎恒返回 `false`，实际依赖此查询的代码应该很少
- 修复后的行为才是正确的语义

**问题 2**:
- 完全向后兼容
- 只是增加了边界事件的触发场景，不影响现有逻辑
- 如果没有监听器，事件广播不会产生任何影响

### 性能影响

**问题 1**:
- 无性能影响（只是移除一个条件判断）

**问题 2**:
- 轻微性能影响（增加边界检测和事件广播）
- 但 `EnforceAttributeRangeConstraints()` 本身就是在属性变化时调用，频率不高
- 边界检测只是简单的浮点数比较，开销可忽略

## Alternatives Considered

### 问题 1 的替代方案

**方案 A：在 PauseStateTree() 中设置 bStateTreeRunning = false**
- 优点：保持 `IsStateTreePaused()` 的实现不变
- 缺点：
  - 改变了 Pause 的语义，使其与 Stop 混淆
  - 可能影响 `ResumeStateTree()` 的逻辑（需要重新 Start）
  - 风险更高

**方案 B：添加新的 bStateTreePaused 标志**
- 优点：不改变现有标志的语义
- 缺点：
  - 增加了状态管理的复杂度
  - Stage 已经明确表示了 Pause 状态，添加新标志是冗余的
  - 需要在多处维护新标志的一致性

**选择方案 1 的理由**：最简单、最直接，符合 Pause 的语义

### 问题 2 的替代方案

**方案 A：不触发边界事件，只触发 Value Changed 事件**
- 优点：保持现有行为
- 缺点：
  - 语义不一致
  - 游戏逻辑需要同时监听 Value Changed 和 Boundary 事件，增加复杂度

**方案 B：移除所有边界事件，统一使用 Value Changed 事件**
- 优点：简化事件系统
- 缺点：
  - 破坏性变更，影响所有依赖边界事件的代码
  - 边界事件提供了更精确的语义，对游戏逻辑有价值

**选择方案 2 的理由**：保持事件系统的完整性和一致性，向后兼容

## Open Questions

无

## Success Criteria

1. `IsStateTreePaused()` 在 Pause/HangUp 状态下返回 `true`
2. `EnforceAttributeRangeConstraints()` 在值被 Clamp 到边界时触发 `OnAttributeReachedBoundary` 事件
3. 所有现有测试通过
4. 编译无错误和警告
