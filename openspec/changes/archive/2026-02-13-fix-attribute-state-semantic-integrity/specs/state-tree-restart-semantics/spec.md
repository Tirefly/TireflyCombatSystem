# Spec: StateTree重启语义

## ADDED Requirements

### Requirement: 区分冷启动和热恢复

StateTree MUST 支持两种重启模式：冷启动（重置InstanceData）和热恢复（保留InstanceData）。

#### Scenario: 冷启动重置InstanceData

**Given**:
- 状态实例有StateTree
- StateTree之前已经运行过，InstanceData包含状态

**When**: 调用 `RestartStateTree()`

**Then**:
- StateTreeInstanceData被重置
- 所有StateTree节点重新初始化
- StateTree从头开始执行
- 之前的执行状态被清除

#### Scenario: 热恢复保留InstanceData

**Given**:
- 状态实例处于Pause状态
- StateTreeInstanceData包含暂停时的状态

**When**: 调用 `StartStateTree()`

**Then**:
- StateTreeInstanceData保持不变
- StateTree从暂停点继续执行
- 节点状态保留
- 不重新初始化

#### Scenario: 全新应用状态使用冷启动

**Given**: 通过ApplyState应用新状态

**When**: 状态实例被创建并激活

**Then**:
- 内部调用 `RestartStateTree()`
- StateTree以全新状态启动
- InstanceData从零开始

#### Scenario: 从Pause恢复使用热恢复

**Given**: 状态实例处于Pause状态

**When**: 调用Resume恢复状态

**Then**:
- 内部调用 `StartStateTree()`
- StateTree继续之前的执行
- 保留暂停时的状态

#### Scenario: 从HangUp恢复使用热恢复

**Given**: 状态实例处于HangUp状态

**When**: 调用Resume恢复状态

**Then**:
- 内部调用 `StartStateTree()`
- StateTree继续之前的执行
- 保留挂起时的状态

### Requirement: RestartStateTree API

 MUST 提供RestartStateTree方法用于冷启动。

#### Scenario: RestartStateTree方法签名

**Given**: UTcsStateInstance类

**When**: 检查方法定义

**Then**:
-  MUST 有 `void RestartStateTree()` 方法
- 方法可以被蓝图调用
- 方法内部重置InstanceData并启动StateTree

#### Scenario: RestartStateTree执行流程

**Given**: 调用RestartStateTree

**When**: 方法执行

**Then**:
1. 停止当前StateTree（如果正在运行）
2. 重置StateTreeInstanceData
3. 重新初始化StateTree
4. 启动StateTree执行

### Requirement: StartStateTree保持现有语义

StartStateTree方法 MUST 保持热恢复语义。

#### Scenario: StartStateTree不重置InstanceData

**Given**: StateTree之前已运行，InstanceData有状态

**When**: 调用 `StartStateTree()`

**Then**:
- InstanceData不被重置
- StateTree继续执行
- 节点状态保留

### Requirement: 内部实现统一

两个方法内部 MUST 使用统一的实现逻辑。

#### Scenario: 共享内部实现

**Given**: RestartStateTree和StartStateTree方法

**When**: 检查实现

**Then**:
- 两者调用共同的内部方法 `StartStateTreeInternal(bool bResetInstanceData)`
- RestartStateTree传入true
- StartStateTree传入false
- 避免代码重复

## ADDED Requirements

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

## Implementation Notes

### API定义

```cpp
// UTcsStateInstance

/**
 * 冷启动StateTree：重置InstanceData并启动
 * 用于全新应用状态
 */
UFUNCTION(BlueprintCallable, Category = "State")
void RestartStateTree();

/**
 * 热恢复StateTree：保留InstanceData并启动
 * 用于从Pause/HangUp恢复
 */
UFUNCTION(BlueprintCallable, Category = "State")
void StartStateTree();

private:
/**
 * 内部实现：启动StateTree
 * @param bResetInstanceData 是否重置InstanceData
 */
void StartStateTreeInternal(bool bResetInstanceData);
```

### 实现示例

```cpp
void UTcsStateInstance::RestartStateTree()
{
    StartStateTreeInternal(true);
}

void UTcsStateInstance::StartStateTree()
{
    StartStateTreeInternal(false);
}

void UTcsStateInstance::StartStateTreeInternal(bool bResetInstanceData)
{
    if (!StateTreeRef.IsValid())
    {
        return;
    }

    // 停止当前StateTree
    if (bIsStateTreeRunning)
    {
        StopStateTree();
    }

    // 重置InstanceData（如果需要）
    if (bResetInstanceData)
    {
        StateTreeInstanceData.Reset();
        // 重新初始化
        StateTreeRef.InitializeInstanceData(StateTreeInstanceData);
    }

    // 启动StateTree
    StateTreeRef.Start(StateTreeInstanceData);
    bIsStateTreeRunning = true;
}
```

### 使用场景映射

| 场景 | 使用方法 | 原因 |
|------|---------|------|
| ApplyState | RestartStateTree | 全新状态，需要从头开始 |
| Resume from Pause | StartStateTree | 继续之前的执行 |
| Resume from HangUp | StartStateTree | 继续之前的执行 |
| 手动重启 | RestartStateTree | 强制重新开始 |

### 向后兼容性

- 现有调用StartStateTree的代码保持不变
- 新代码根据场景选择合适的方法
- 如果不确定，使用StartStateTree（保守选择）

## Related Specs

- `state-safety-fixes`: 状态安全修复
- `state-slot-activation-reentrancy`: 状态槽激活重入
