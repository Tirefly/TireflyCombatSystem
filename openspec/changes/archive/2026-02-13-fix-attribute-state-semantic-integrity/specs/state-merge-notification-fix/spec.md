# Spec: 状态合并通知修复

## ADDED Requirements

### Requirement: 合并后检查状态有效性再通知

状态合并后， MUST 检查状态是否仍然有效，只有有效状态才广播ApplySuccess事件。

#### Scenario: 状态仍在槽位中才广播ApplySuccess

**Given**:
- 新状态被应用并执行合并
- 合并后状态仍在槽位的States列表中

**When**: 合并完成

**Then**:
- 检查 `StateSlot->States.Contains(StateInstance)`
- 如果包含，广播 `OnStateApplySuccess`
- 如果不包含，跳过广播

#### Scenario: 状态未过期才广播ApplySuccess

**Given**:
- 新状态被应用并执行合并
- 合并后状态的Stage不是Expired

**When**: 合并完成

**Then**:
- 检查 `StateInstance->GetCurrentStage() != ETcsStateStage::SS_Expired`
- 如果未过期，广播 `OnStateApplySuccess`
- 如果已过期，跳过广播

#### Scenario: 状态被合并移除不广播ApplySuccess

**Given**:
- 新状态被应用
- 合并策略导致新状态立即被移除（如UseOldest保留旧状态）

**When**: 合并完成

**Then**:
- 新状态不在槽位的States列表中
- 不广播 `OnStateApplySuccess`
- 记录Verbose级别日志说明跳过通知

#### Scenario: 状态有效时正常广播

**Given**:
- 新状态被应用并执行合并
- 合并后状态仍然有效（在槽位中且未过期）

**When**: 合并完成

**Then**:
- 添加到StateInstanceIndex
- 广播 `OnStateApplySuccess` 事件
- 事件包含正确的状态信息

### Requirement: 事件顺序正确性

状态合并相关的事件 MUST 按正确顺序触发。

#### Scenario: 被合并移除的事件顺序

**Given**: 新状态被合并并移除

**When**: 合并完成

**Then**:
1. 广播 `OnStateMerged` 事件（如果有）
2. 广播 `OnStateRemoved` 事件
3. 不广播 `OnStateApplySuccess` 事件

#### Scenario: 合并成功的事件顺序

**Given**: 新状态被合并但保留

**When**: 合并完成

**Then**:
1. 广播 `OnStateMerged` 事件（如果有）
2. 广播 `OnStateApplySuccess` 事件
3. 不广播 `OnStateRemoved` 事件

#### Scenario: 不合并的事件顺序

**Given**: 新状态不需要合并（NoMerge策略）

**When**: 应用完成

**Then**:
1. 直接广播 `OnStateApplySuccess` 事件
2. 不广播 `OnStateMerged` 事件

### Requirement: 日志记录

跳过ApplySuccess通知时 MUST 记录日志。

#### Scenario: 记录跳过通知日志

**Given**: 状态被合并移除，跳过ApplySuccess通知

**When**: 检查到状态无效

**Then**:
- 记录Verbose级别日志
- 日志包含状态名称
- 日志说明原因（被合并移除）
- 便于调试和追踪

## ADDED Requirements

### Requirement: 状态有效性检查辅助函数

 MUST 提供检查状态是否仍然有效的辅助函数。

#### Scenario: IsStateStillValid函数

**Given**: 需要检查状态是否有效

**When**: 调用辅助函数

**Then**:
- 检查状态是否在槽位中
- 检查状态是否未过期
- 返回bool结果

#### Scenario: 辅助函数实现

**Given**: 实现IsStateStillValid

**When**: 检查状态

**Then**:
```cpp
bool IsStateStillValid(const UTcsStateInstance* StateInstance, const FTcsStateSlot* StateSlot)
{
    if (!StateInstance || !StateSlot)
    {
        return false;
    }

    return StateSlot->States.Contains(const_cast<UTcsStateInstance*>(StateInstance))
        && StateInstance->GetCurrentStage() != ETcsStateStage::SS_Expired;
}
```

## Implementation Notes

### 修复位置

```cpp
// TcsStateManagerSubsystem.cpp:547-552
// 旧代码：
StateComponent->StateInstanceIndex.AddInstance(StateInstance);

OwnerStateCmp->NotifyStateApplySuccess(
    StateInstance->GetOwner(),
    StateInstance->GetStateDefId(),
    StateInstance,
    StateDef.StateSlotType,
    StateInstance->GetCurrentStage());

// 新代码：
if (StateSlot->States.Contains(StateInstance) &&
    StateInstance->GetCurrentStage() != ETcsStateStage::SS_Expired)
{
    StateComponent->StateInstanceIndex.AddInstance(StateInstance);

    // 只有状态仍然有效时才广播 ApplySuccess
    OwnerStateCmp->NotifyStateApplySuccess(
        StateInstance->GetOwner(),
        StateInstance->GetStateDefId(),
        StateInstance,
        StateDef.StateSlotType,
        StateInstance->GetCurrentStage());
}
else
{
    // 状态已被合并移除，不广播ApplySuccess
    UE_LOG(LogTcsState, Verbose,
        TEXT("State '%s' was merged and removed, skipping ApplySuccess notification"),
        *StateInstance->GetStateDefId().ToString());
}
```

### 完整的合并流程

```cpp
// 1. 创建状态实例
UTcsStateInstance* StateInstance = CreateStateInstance(...);

// 2. 执行合并逻辑
bool bWasMerged = false;
if (ShouldMerge)
{
    MergeStates(StateSlot, StateInstance, bWasMerged);
}

// 3. 检查状态是否仍然有效
if (IsStateStillValid(StateInstance, StateSlot))
{
    // 状态有效，广播ApplySuccess
    StateComponent->StateInstanceIndex.AddInstance(StateInstance);
    OwnerStateCmp->NotifyStateApplySuccess(...);
}
else
{
    // 状态被合并移除，跳过ApplySuccess
    UE_LOG(LogTcsState, Verbose, TEXT("State merged and removed, skipping notification"));
}
```

### 事件监听者注意事项

事件监听者应该理解：
1. `OnStateApplySuccess` 表示状态成功应用且仍然有效
2. 如果状态被合并移除，会收到 `OnStateMerged` 和 `OnStateRemoved`，但不会收到 `OnStateApplySuccess`
3. 不应假设每次ApplyState都会收到ApplySuccess事件

### 测试场景

1. **NoMerge策略**: 应该收到ApplySuccess
2. **UseNewest策略**: 新状态应该收到ApplySuccess，旧状态收到Removed
3. **UseOldest策略**: 旧状态不收到新事件，新状态不收到ApplySuccess
4. **Stack策略**: 应该收到ApplySuccess（堆叠后仍然有效）

### 向后兼容性

- 这是行为修复，不是破坏性变更
- 事件监听者可能需要更新逻辑以处理新的事件顺序
- 如果监听者依赖"每次Apply都有ApplySuccess"，需要更新

## Related Specs

- `state-merge-removal-unification`: 状态合并移除统一
- `state-stack-zero-removal`: 堆叠清空自动移除
- `state-safety-fixes`: 状态安全修复
