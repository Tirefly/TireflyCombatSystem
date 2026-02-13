# Spec: 堆叠清空自动移除

## ADDED Requirements

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

## REMOVED Requirements

### Requirement: StackCount最低为1

移除"StackCount最低为1"的约束。

#### Scenario: 旧的clamp逻辑

**Given**: 旧代码

```cpp
int32 NewStackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
```

**When**: 更新代码

**Then**: 改为

```cpp
int32 NewStackCount = FMath::Clamp(InStackCount, 0, MaxStackCount);
```

## Implementation Notes

### SetStackCount实现

```cpp
void UTcsStateInstance::SetStackCount(int32 InStackCount)
{
    int32 MaxStackCount = GetMaxStackCount();
    if (MaxStackCount <= 0)
    {
        return;
    }

    int32 OldStackCount = GetStackCount();
    int32 NewStackCount = FMath::Clamp(InStackCount, 0, MaxStackCount);

    if (OldStackCount == NewStackCount)
    {
        return;
    }

    // 设置新的StackCount
    NumericParameters.FindOrAdd(Tcs_Generic_Name_StackCount) = static_cast<float>(NewStackCount);

    // 如果降为0，自动触发移除
    if (NewStackCount == 0)
    {
        UE_LOG(LogTcsState, Warning, TEXT("State '%s' StackCount reached 0, requesting removal"),
            *GetStateDefId().ToString());

        // 根据调用上下文选择移除原因
        ETcsStateRemovalRequestReason Reason = (InStackCount == 0)
            ? ETcsStateRemovalRequestReason::Removed
            : ETcsStateRemovalRequestReason::Expired;

        RequestRemoval(Reason);
        return;
    }

    // 广播StackCount变化事件
    if (OwnerStateCmp.IsValid())
    {
        OwnerStateCmp->NotifyStateStackCountChanged(this, OldStackCount, NewStackCount);
    }
}
```

### RemoveStack实现

```cpp
void UTcsStateInstance::RemoveStack(int32 StackDelta)
{
    if (StackDelta <= 0)
    {
        return;
    }

    int32 CurrentStack = GetStackCount();
    int32 NewStack = FMath::Max(0, CurrentStack - StackDelta);

    SetStackCount(NewStack);
    // SetStackCount内部会处理降为0的情况
}
```

### 合并器更新

所有堆叠合并器需要更新以处理StackCount=0：

```cpp
// TcsStateMerger_StackDirectly::Merge_Implementation
for (UTcsStateInstance* State : StatesToMerge)
{
    const int32 StateStackCount = State->GetStackCount();

    // 允许StackCount为0
    if (StateStackCount < 0)
    {
        UE_LOG(LogTcsStateMerger, Error, TEXT("Invalid StackCount: %d"), StateStackCount);
        continue;
    }

    // StackCount为0的状态即将被移除，可以跳过
    if (StateStackCount == 0)
    {
        UE_LOG(LogTcsStateMerger, Verbose, TEXT("Skipping state with StackCount=0 (pending removal)"));
        continue;
    }

    TotalStackCount += StateStackCount;
}
```

### 使用场景

1. **Buff堆叠耗尽**: 每次触发效果减少1层，降为0时自动移除
2. **技能充能**: 使用技能消耗充能，充能为0时技能不可用（自动移除）
3. **护盾层数**: 每次受击减少1层，层数为0时护盾消失

### 注意事项

- StackCount=0后状态会进入移除流程，但可能不会立即移除
- 在移除完成前，StackCount仍然为0
- 合并器应该跳过StackCount=0的状态
- 事件监听者应该处理StackCount=0的情况

## Related Specs

- `state-merge-removal-unification`: 状态合并移除统一
- `state-safety-fixes`: 状态安全修复
