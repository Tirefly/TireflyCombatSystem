# state-merge-notification-fix Specification

## Purpose
TBD - created by archiving change fix-attribute-state-semantic-integrity. Update Purpose after archive.
## Requirements
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

