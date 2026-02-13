# Proposal: 修复属性与状态模块的语义与API完整性问题

## Overview

本提案旨在解决TCS插件中属性模块和状态模块存在的多个语义不清晰和API不完整的问题。这些问题影响了系统的可预测性、可扩展性和正确性。

## Problem Statement

### 属性模块问题

1. **合并分组键错误** (attribute-modifier-merge-by-id)
   - **现状**: 合并按 `ModifierName` 分组，导致不同意义的修改器可能被错误合并
   - **影响**: 命名冲突会导致不相关的修改器被合并，破坏语义隔离
   - **位置**: `TcsAttributeManagerSubsystem.cpp:1008`

2. **动态范围clamp语义不清** (attribute-dynamic-range-two-phase-clamp)
   - **现状**: 动态范围clamp使用"当前值"解析，Base依赖Base语义不清
   - **影响**: 当范围依赖其他属性时，取值源和顺序不明确，可能导致循环依赖或不一致的结果
   - **位置**: 属性计算和clamp逻辑

3. **缺少直接操作API** (attribute-direct-manipulation-api)
   - **现状**: 缺少"移除属性/直接设置Base/Current"API
   - **影响**: 系统或编辑器工具无法进行属性重置/校正，只能滥用Modifier
   - **需求**: 允许直接操作属性值，同时保证内部统一触发Clamp和相关事件

### 状态模块问题

1. **StateTree重启语义不明** (state-tree-restart-semantics)
   - **现状**: `StartStateTree` 不重置 `StateTreeInstanceData`，重启语义不明
   - **影响**: 无法区分"热恢复"（从Pause/HangUp恢复）和"冷启动"（全新应用）
   - **需求**: 明确StateTree生命周期，支持两种重启模式

2. **堆叠清空无法触发移除** (state-stack-zero-removal)
   - **现状**: `StackCount` 最低为1，`RemoveStack` 不能降为0
   - **影响**: 无法实现"堆叠清空即移除"的语义
   - **位置**: `TcsState.cpp:261` - `FMath::Clamp(InStackCount, 1, MaxStackCount)`

3. **参数评估失败不阻止创建** (state-param-eval-strict-check)
   - **现状**: 参数评估失败仅日志，不阻止实例生成
   - **影响**: 带"缺参"的状态进入运行，可能导致未定义行为
   - **需求**: 新增严格审查逻辑，参数评估失败即判定创建失败（ApplyFailed）

4. **合并后仍广播ApplySuccess** (state-merge-notification-fix)
   - **现状**: 合并后仍广播 `ApplySuccess`，可能出现"应用即被合并移除"的短暂事件
   - **影响**: 事件监听者收到误导性通知
   - **位置**: `TcsStateManagerSubsystem.cpp:547-552`

## Proposed Solution

### 属性模块解决方案

1. **合并分组键改为ModifierId**
   - 将 `TcsAttributeManagerSubsystem::MergeAttributeModifiers` 中的分组键从 `ModifierName` 改为 `ModifierId`（即DataTable的RowName）
   - 确保只有相同定义的修改器才会被合并

2. **两阶段clamp方案**
   - 先对Base值使用WorkingBase进行clamp
   - 再对Current值使用WorkingCurrent进行clamp
   - 明确动态范围依赖的取值源与顺序

3. **添加直接操作API**
   - 添加 `RemoveAttribute`、`SetAttributeBaseValue`、`SetAttributeCurrentValue` 等API
   - 保证内部统一触发Clamp和相关事件
   - 提供安全的属性重置/校正机制

### 状态模块解决方案

1. **区分StateTree重启模式**
   - 添加 `RestartStateTree` 方法（冷启动，重置InstanceData）
   - 保留 `StartStateTree` 方法（热恢复，不重置InstanceData）
   - 明确两种模式的使用场景

2. **允许StackCount=0触发移除**
   - 修改 `SetStackCount` 允许值为0
   - 当StackCount降为0时，自动触发 `RequestRemoval(Reason=Expired/Removed)`
   - 更新相关验证逻辑

3. **参数评估失败严格检查**
   - 在状态实例创建时，严格检查所有参数评估结果
   - 任何参数评估失败即判定创建失败，返回ApplyFailed
   - 记录详细的失败原因

4. **修复合并后的通知逻辑**
   - 在广播 `ApplySuccess` 前检查状态是否仍然有效
   - 只有状态仍在槽位中且未过期时才广播
   - 避免"应用即被合并移除"的误导性事件

## Impact Assessment

### 破坏性变更

1. **属性合并分组键变更**: 如果现有配置中存在不同ModifierId但相同ModifierName的情况，合并行为会改变
2. **StackCount=0语义**: 现有代码假设StackCount>=1的逻辑需要更新
3. **参数评估严格检查**: 之前能"容错"创建的状态现在会失败

### 兼容性

- 大部分变更是修复语义问题，不影响正常使用场景
- 新增API向后兼容
- StateTree重启模式区分不影响现有调用

### 性能影响

- 合并分组键变更：无性能影响（仅改变分组依据）
- 两阶段clamp：轻微增加计算量，但提高正确性
- 参数评估严格检查：在创建阶段增加验证，但避免运行时错误
- 其他变更：无明显性能影响

## Alternatives Considered

### 属性合并分组

- **方案A**: 保持ModifierName分组，要求用户避免命名冲突
  - 缺点：依赖用户约定，容易出错
- **方案B**: 使用ModifierId分组（采用）
  - 优点：语义明确，自动隔离

### StateTree重启

- **方案A**: 添加参数控制是否重置（单一方法）
  - 缺点：API复杂，容易误用
- **方案B**: 分离为两个方法（采用）
  - 优点：语义清晰，不易出错

### StackCount=0处理

- **方案A**: 保持最低为1，添加显式移除API
  - 缺点：不符合直觉，需要额外调用
- **方案B**: 允许为0并自动触发移除（采用）
  - 优点：符合直觉，简化使用

## Success Criteria

1. 所有属性修改器按ModifierId正确分组合并
2. 动态范围clamp使用明确的两阶段逻辑
3. 提供完整的属性直接操作API
4. StateTree支持冷启动和热恢复两种模式
5. StackCount可以降为0并自动触发移除
6. 参数评估失败严格阻止状态创建
7. 合并后的通知逻辑正确，无误导性事件
8. 所有现有测试通过
9. 编译无错误和警告

## Dependencies

- 无外部依赖
- 需要更新相关的合并器和参数评估器实现
- 需要更新文档说明新的语义和API

## Timeline

预计实施时间：2-3个工作日

- Day 1: 属性模块修复（合并分组、两阶段clamp、直接操作API）
- Day 2: 状态模块修复（StateTree重启、StackCount=0、参数评估）
- Day 3: 通知逻辑修复、测试和文档更新
