# Proposal: 状态槽位激活完整性重构

## 概述

本提案旨在修复状态系统中的若干正确性和可维护性问题，包括：

1. **合并移除统一化**: 将合并移除统一走 RequestStateRemoval 路径，避免直接 Finalize 导致的再入风险
2. **槽位激活去再入**: 通过延迟请求机制消除 UpdateStateSlotActivation 的递归/嵌套调用
3. **同优先级 tie-break 策略化**: 将硬编码的同优先级排序规则改为可扩展的策略对象
4. **Gate 关闭逻辑重构**: 收敛到单一权威函数，降低重复逻辑与维护成本

## 动机

### 问题 1: 合并移除直接 Finalize 导致再入风险

**当前情况**:
- 状态合并逻辑在决定"合并淘汰"某个 state 时，直接调用 Finalize removal
- Finalize 会触发槽位激活更新（UpdateStateSlotActivation）
- 这导致在槽位激活更新过程中嵌套调用 UpdateStateSlotActivation（再入风险）

**影响**:
- 递归调用可能导致状态不一致
- 难以追踪和调试
- 可能导致性能问题

### 问题 2: UpdateStateSlotActivation 缺少再入保护

**当前情况**:
- UpdateStateSlotActivation 可以在执行过程中被递归调用
- 没有机制防止或延迟这种嵌套调用
- 状态变化可能在不确定的时机发生

**影响**:
- 状态更新顺序不确定
- 难以保证不变量
- 测试和验证困难

### 问题 3: 同优先级 tie-break 规则硬编码

**当前情况**:
- 在 PriorityOnly 槽位中，多个 state 同优先级时的排序规则是硬编码的
- 不同类型的槽位（Buff vs Skill）可能需要不同的 tie-break 策略
- 无法扩展或自定义

**影响**:
- 缺乏灵活性
- 难以适应不同的游戏玩法需求
- 修改规则需要修改核心代码

### 问题 4: Gate 关闭逻辑分散

**当前情况**:
- Gate 关闭行为在多个函数中重复实现
- 不变量检查分散在不同位置
- 维护成本高，容易出现不一致

**影响**:
- 代码重复
- 难以保证一致性
- 修改时容易遗漏某些路径

## 目标

1. **统一移除路径**: 所有状态移除（包括合并淘汰）都走 RequestStateRemoval
2. **消除再入**: 通过延迟请求机制确保槽位激活更新在有限步内收敛
3. **策略化 tie-break**: 将同优先级排序规则改为可扩展的 UObject 策略
4. **单一权威函数**: Gate 关闭逻辑收敛到一个函数，保证不变量

## 非目标

- State 免疫系统（不在本轮范围内）
- 对象池集成（保持现有实现）
- State 实例的网络同步（不在本轮范围内）
- PendingRemoval 超时强制 StopStateTree（由项目开发者自行负责）

## 影响范围

**修改的文件**:
- `UTcsStateManagerSubsystem` (头文件和实现)
- `UTcsStateComponent` (可能需要少量修改)
- 新增策略基类: `UTcsStateSamePriorityPolicy`

**影响的系统**:
- 状态系统核心
- 槽位激活机制
- 状态合并逻辑

**向后兼容性**:
- API 签名保持不变
- 行为更确定性，但可能暴露之前被掩盖的问题
- 需要为现有槽位配置默认的 tie-break 策略

## 实施策略

采用分阶段实施:

1. **Phase 1**: 合并移除统一化
   - 修改合并逻辑使用 RequestStateRemoval
   - 确保 StateTree 退场逻辑有机会执行

2. **Phase 2**: 槽位激活去再入
   - 添加延迟请求机制
   - 实现队列排空逻辑
   - 确保有限步收敛

3. **Phase 3**: 同优先级 tie-break 策略化
   - 设计策略接口
   - 实现内置策略（UseNewest、UseOldest）
   - 集成到槽位激活流程

4. **Phase 4**: Gate 关闭逻辑重构
   - 创建权威函数 HandleGateClosed
   - 移除重复路径
   - 保证不变量

## 风险评估

**高风险**:
- 槽位激活去再入可能影响现有玩法时序（需要仔细测试）

**中风险**:
- 合并移除统一化可能改变状态生命周期（需要验证 StateTree 退场逻辑）
- tie-break 策略化需要为现有槽位选择合适的默认策略

**低风险**:
- Gate 关闭逻辑重构（主要是代码整理，行为不变）

## 成功标准

1. 合并移除不再直接 Finalize，都走 RequestStateRemoval
2. UpdateStateSlotActivation 不会递归调用，通过延迟请求机制收敛
3. 同优先级排序规则可配置，内置至少两种策略
4. Gate 关闭逻辑只有一个权威函数
5. 所有现有测试通过
6. 新增测试覆盖所有修复的问题

## 相关文档

- [TCS 核心修复提案草案](../../Documents/tcs-core-fixes-proposal-draft.zh-CN.md)
- [State 系统架构文档](../../CLAUDE.md)

---

## 实施更新

### Phase 2 数据结构 Bug 修复 (2026-02-07)

**问题发现**:
在实施 Phase 2（槽位激活去再入）时，通过代码审查发现延迟请求机制的数据结构设计存在严重 bug：

**原实现**:
```cpp
TMap<FGameplayTag, TWeakObjectPtr<UTcsStateComponent>> PendingSlotActivationUpdates;
```

**Bug 分析**:
- 使用 `SlotTag` 作为 Map 的 Key
- 当多个 Actor 共享相同的 SlotTag（如 `Combat.Attack`）时，后来的请求会覆盖前面的请求
- 导致部分 Actor 的状态槽激活更新丢失
- 严重影响多 Actor 战斗场景（如 10 个敌人同时战斗）

**修复方案**:
```cpp
TMap<TWeakObjectPtr<UTcsStateComponent>, TSet<FGameplayTag>> PendingSlotActivationUpdates;
```

**修复说明**:
- 使用 `StateComponent` 作为 Map 的 Key
- 每个 Component 维护独立的待更新 SlotTag 集合
- 完全避免了多 Actor 覆盖问题
- 支持批量处理同一 Component 的多个 Slot 更新

**影响文件**:
- `TcsStateManagerSubsystem.h:163` - 数据结构定义
- `TcsStateManagerSubsystem.cpp:673` - 入队逻辑
- `TcsStateManagerSubsystem.cpp:685-720` - 出队和处理逻辑

**验证**:
- ✅ 编译通过
- ✅ 多 Actor 共享相同 SlotTag 不会互相覆盖
- ✅ 延迟请求机制正确工作

此次修复确保了延迟请求机制在多 Actor 场景下的正确性，是 Phase 2 实施的关键改进。

