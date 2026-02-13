# Design: 修复属性与状态模块的语义与API完整性问题

## Architecture Overview

本设计文档详细说明如何修复TCS插件中属性模块和状态模块的语义问题和API缺失。这些修复涉及多个子系统，需要仔细协调以保持系统一致性。

## Design Decisions

### 1. 属性修改器合并分组键 (attribute-modifier-merge-by-id)

#### 当前实现

```cpp
// TcsAttributeManagerSubsystem.cpp:1008
ModifiersToMerge.FindOrAdd(Modifier.ModifierDef.ModifierName).Add(Modifier);
```

按 `ModifierName` 分组，导致不同DataTable行（不同ModifierId）但相同ModifierName的修改器被错误合并。

#### 设计方案

**分组键改为ModifierId**:
- ModifierId是DataTable的RowName，全局唯一
- 每个ModifierId代表一个独立的修改器定义
- 只有相同ModifierId的修改器实例才应该被合并

**实现细节**:
需要在 `FTcsAttributeModifierInstance` 中追踪原始的ModifierId（DataTable RowName），然后使用它作为分组键。

#### 权衡

- **优点**: 语义明确，自动隔离不同定义的修改器
- **缺点**: 需要追踪ModifierId（需要修改数据结构）
- **替代方案**: 使用 `(ModifierName, SourceHandle)` 组合键，但这会改变合并语义

### 2. 动态范围两阶段Clamp (attribute-dynamic-range-two-phase-clamp)

#### 当前问题

动态范围（如 `MaxValue = "MaxHealth"`）在clamp时使用"当前值"解析，导致：
1. Base值clamp依赖Current值，语义不清
2. 可能出现循环依赖
3. 顺序敏感，难以预测

#### 设计方案

**两阶段Clamp**:

**阶段1: Base值Clamp**
- 使用 `WorkingBase` 解析动态范围
- 确保Base值的范围约束基于其他属性的Base值
- 避免循环依赖（Base -> Base）

**阶段2: Current值Clamp**
- 使用 `WorkingCurrent` 解析动态范围
- Current值的范围约束基于其他属性的Current值
- 反映实时状态（包括修改器效果）

**实现流程**:
```
1. 计算所有属性的WorkingBase（应用Base修改器）
2. 对每个属性的Base值进行Clamp（使用WorkingBase解析范围）
3. 计算所有属性的WorkingCurrent（应用Current修改器）
4. 对每个属性的Current值进行Clamp（使用WorkingCurrent解析范围）
```

#### 权衡

- **优点**: 语义清晰，避免循环依赖，可预测
- **缺点**: 增加计算复杂度（两次clamp）
- **性能影响**: 轻微，但换来正确性

### 3. 属性直接操作API (attribute-direct-manipulation-api)

#### 当前缺失

无法直接操作属性值，只能通过修改器间接修改。这导致：
1. 无法实现属性重置
2. 编辑器工具难以调试
3. 某些场景需要滥用修改器

#### 设计方案

**新增API**:

```cpp
// UTcsAttributeManagerSubsystem

// 移除属性（重置为初始状态）
bool RemoveAttribute(AActor* CombatEntity, FName AttributeName);

// 直接设置Base值
bool SetAttributeBaseValue(AActor* CombatEntity, FName AttributeName, float NewValue, bool bTriggerEvents = true);

// 直接设置Current值
bool SetAttributeCurrentValue(AActor* CombatEntity, FName AttributeName, float NewValue, bool bTriggerEvents = true);

// 重置属性到定义的初始值
bool ResetAttribute(AActor* CombatEntity, FName AttributeName);
```

**实现要点**:
1. 所有API内部统一调用Clamp逻辑
2. 支持控制是否触发事件（用于批量操作）
3. 记录操作日志（用于调试）
4. 验证输入参数（属性存在性、值合法性）

**事件触发**:
- `SetAttributeBaseValue` -> `OnAttributeBaseValueChanged`
- `SetAttributeCurrentValue` -> `OnAttributeValueChanged`
- `RemoveAttribute` -> 移除所有相关修改器 -> 触发相应事件

#### 权衡

- **优点**: 提供完整的属性操作能力，简化调试和工具开发
- **缺点**: 增加API表面积，需要文档说明使用场景
- **安全性**: 通过参数验证和事件触发保证一致性

### 4. StateTree重启语义 (state-tree-restart-semantics)

#### 当前问题

`StartStateTree` 不重置 `StateTreeInstanceData`，导致：
1. 无法区分"热恢复"和"冷启动"
2. 从Pause恢复和全新应用使用相同逻辑
3. 可能导致状态不一致

#### 设计方案

**分离两种重启模式**:

**冷启动 (RestartStateTree)**:
- 重置 `StateTreeInstanceData`
- 重新初始化所有StateTree节点
- 用于全新应用状态（ApplyState）

**热恢复 (StartStateTree)**:
- 保留 `StateTreeInstanceData`
- 继续之前的执行状态
- 用于从Pause/HangUp恢复

**API设计**:
```cpp
// UTcsStateInstance

// 冷启动：重置InstanceData并启动StateTree
void RestartStateTree();

// 热恢复：保留InstanceData并启动StateTree
void StartStateTree();

// 内部实现
void StartStateTreeInternal(bool bResetInstanceData);
```

**使用场景**:
- `ApplyState` -> `RestartStateTree()` （冷启动）
- `Resume from Pause` -> `StartStateTree()` （热恢复）
- `Resume from HangUp` -> `StartStateTree()` （热恢复）

#### 权衡

- **优点**: 语义清晰，不易误用
- **缺点**: 增加一个API
- **替代方案**: 单一方法+参数控制，但容易出错

### 5. 堆叠清空自动移除 (state-stack-zero-removal)

#### 当前问题

```cpp
// TcsState.cpp:261
int32 NewStackCount = FMath::Clamp(InStackCount, 1, MaxStackCount);
```

StackCount最低为1，无法实现"堆叠清空即移除"。

#### 设计方案

**允许StackCount=0**:

```cpp
// 新的逻辑
int32 NewStackCount = FMath::Clamp(InStackCount, 0, MaxStackCount);

if (NewStackCount == 0)
{
    // 自动触发移除
    RequestRemoval(ETcsStateRemovalRequestReason::Expired);
    return;
}
```

**实现要点**:
1. 修改 `SetStackCount` 允许值为0
2. 当StackCount降为0时，自动调用 `RequestRemoval`
3. 更新所有StackCount相关的验证逻辑（允许0）
4. 更新合并器逻辑（处理StackCount=0的情况）

**移除原因选择**:
- 使用 `Expired` 表示自然耗尽
- 或使用 `Removed` 表示主动移除
- 根据调用上下文决定

#### 权衡

- **优点**: 符合直觉，简化使用
- **缺点**: 需要更新所有假设StackCount>=1的代码
- **兼容性**: 可能影响现有逻辑，需要仔细测试

### 6. 参数评估严格检查 (state-param-eval-strict-check)

#### 当前问题

参数评估失败仅记录日志，不阻止实例创建，导致：
1. 带缺参的状态进入运行
2. 运行时可能出现未定义行为
3. 难以调试参数配置错误

#### 设计方案

**严格检查逻辑**:

```cpp
// 在状态实例创建时
bool bAllParamsValid = true;
TArray<FString> FailedParams;

for (const auto& ParamPair : StateDef.Parameters)
{
    bool bEvalSuccess = EvaluateParameter(ParamPair);
    if (!bEvalSuccess)
    {
        bAllParamsValid = false;
        FailedParams.Add(ParamPair.Key.ToString());
    }
}

if (!bAllParamsValid)
{
    UE_LOG(LogTcsState, Error, TEXT("State '%s' parameter evaluation failed: %s"),
        *StateDefId.ToString(),
        *FString::Join(FailedParams, TEXT(", ")));

    // 返回ApplyFailed
    return ETcsStateApplyFailReason::CreateInstanceFailed;
}
```

**实现要点**:
1. 在 `CreateStateInstance` 中添加参数验证阶段
2. 收集所有失败的参数名称
3. 记录详细的错误日志
4. 返回明确的失败原因
5. 不创建状态实例

**错误处理**:
- 失败原因: `CreateInstanceFailed` 或新增 `ParameterEvaluationFailed`
- 日志级别: Error
- 通知: 广播 `OnStateApplyFailed` 事件

#### 权衡

- **优点**: 提前发现配置错误，避免运行时问题
- **缺点**: 可能导致之前"容错"的配置失败
- **兼容性**: 破坏性变更，需要修复所有参数配置错误

### 7. 合并通知修复 (state-merge-notification-fix)

#### 当前问题

```cpp
// TcsStateManagerSubsystem.cpp:547-552
StateComponent->StateInstanceIndex.AddInstance(StateInstance);

OwnerStateCmp->NotifyStateApplySuccess(...);
```

合并后仍广播 `ApplySuccess`，但状态可能已被合并移除。

#### 设计方案

**修复通知逻辑**:

```cpp
// 在合并后检查状态是否仍然有效
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
    UE_LOG(LogTcsState, Verbose, TEXT("State '%s' was merged and removed, skipping ApplySuccess notification"),
        *StateInstance->GetStateDefId().ToString());
}
```

**实现要点**:
1. 在广播前检查状态是否仍在槽位中
2. 检查状态是否已过期
3. 只有状态有效时才广播
4. 记录跳过通知的日志（Verbose级别）

**事件顺序**:
1. 创建状态实例
2. 执行合并逻辑
3. 如果被合并移除 -> 广播 `OnStateMerged` 和 `OnStateRemoved`
4. 如果仍然有效 -> 广播 `OnStateApplySuccess`

#### 权衡

- **优点**: 避免误导性事件，事件语义清晰
- **缺点**: 需要额外的状态检查
- **性能影响**: 可忽略（仅增加两次查询）

## Cross-Cutting Concerns

### 事件一致性

所有修改都需要保证事件触发的一致性：
1. 属性变更 -> 触发相应事件
2. 状态变更 -> 触发相应事件
3. 事件顺序符合逻辑
4. 避免重复或遗漏事件

### 日志和调试

所有修改都需要添加适当的日志：
1. Error级别：严重问题（参数评估失败、配置错误）
2. Warning级别：潜在问题（StackCount=0触发移除）
3. Verbose级别：调试信息（跳过通知、合并详情）

### 测试策略

每个修改都需要对应的测试：
1. 单元测试：验证核心逻辑
2. 集成测试：验证子系统协作
3. 边界测试：验证边界情况（StackCount=0、参数缺失）
4. 回归测试：确保不破坏现有功能

## Implementation Order

建议的实施顺序（按依赖关系）：

1. **attribute-modifier-merge-by-id** - 独立修改，无依赖
2. **attribute-dynamic-range-two-phase-clamp** - 独立修改，无依赖
3. **attribute-direct-manipulation-api** - 依赖前两项完成
4. **state-tree-restart-semantics** - 独立修改，无依赖
5. **state-stack-zero-removal** - 独立修改，无依赖
6. **state-param-eval-strict-check** - 独立修改，无依赖
7. **state-merge-notification-fix** - 依赖state-stack-zero-removal

## Risk Mitigation

### 破坏性变更风险

**风险**: 修改可能破坏现有功能
**缓解**:
1. 充分的测试覆盖
2. 逐步实施，每个修改独立验证
3. 保留旧行为的兼容模式（如果必要）

### 性能风险

**风险**: 两阶段clamp和严格检查可能影响性能
**缓解**:
1. 性能测试验证影响
2. 优化关键路径
3. 考虑缓存机制

### 兼容性风险

**风险**: 现有配置可能不兼容新逻辑
**缓解**:
1. 提供迁移指南
2. 添加验证工具检查配置
3. 详细的错误日志帮助定位问题

## Future Considerations

1. **属性依赖图**: 考虑构建属性依赖图，自动检测循环依赖
2. **参数验证工具**: 提供编辑器工具验证参数配置
3. **事件回放**: 考虑添加事件回放功能，用于调试
4. **性能监控**: 添加性能监控，跟踪clamp和合并的开销
