# 变更：Ongoing AttributeModifier 依赖链惰性重算

## Why

Change 3 已落地 Operation 模型与唯一 `ApplyAttributeModifier` 入口，但 Ongoing 动态 Operand 仍是**受控入口拉取式**更新：只有 Apply / 显式 `RecalculateAttributeCurrentValues` 等路径才会读到最新依赖值。Attribute 或 StateParam 自身变化**不会**自动使相关 Ongoing 失效并重算，导致依赖变更与 CurrentValue 结果之间存在可见延迟。

本 change 在 Change 3 归档契约之上，引入“依赖变化只标脏、安全时机统一 Flush”的惰性依赖链，使目标 Component 内 Attribute 与本地 Buff StateParam 的变更能自动收敛到受影响的 Ongoing 父实例，且不引入每帧全量 Tick 或跨 Actor 全局图。

## What Changes

- 新增 DependencyKey / Revision / 反向依赖索引 / DirtyOngoing 集合。
- Evaluator 经只读 Context 读取可观察值时**自动收集**依赖，禁止依赖手工声明作为唯一路径。
- 依赖生产者成功提交真实值变化时递增 Revision，并通知持有反向索引的 AttributeComponent 标脏。
- **Flush 策略（已确认）**：
  - **同调用栈 / 受控事务末尾**：若本 Component 在 Apply、SetAttributeBaseValue、Commit、显式 Recalculate 等路径中产生 Dirty，则在该路径安全点合并 Flush。
  - **跨调用栈 Dirty**：合并到**帧末**（或等价延迟任务）统一 Flush；禁止在 StateParam / Attribute setter 内同步递归重算。
- 重算粒度固定为**父 Ongoing ModifierInstance**（多 Operation 整体 Dirty / 整体原子提交）。
- 构造依赖闭包 + Working Snapshot；稳定排序 / 拓扑排序；已注册父实例形成循环时记录 Error，并临时跳过最小循环 SCC（不使用 N 次迭代兜底）。
- 已成功提交的父 Ongoing 在后续重算中发生 Evaluator / Operator / Merger / Current Clamp 失败时，失败父实例（或不可分割失败组）立即停止贡献旧结果，但继续保留在唯一 Ongoing 注册表中；下一次 Attribute 事务重新尝试计算。
- 不新增 Quarantined / Disabled / Retry 容器；父实例的 `AppliedOperations` 为空即表示本轮无有效贡献，Definition / SourceHandle / Owner / Overrides 与上次成功依赖记录仍由原注册表维护。
- State Step 5 清理 SourceHandle 时，待移除父实例先从候选注册表删除；剩余失败父实例按同一临时跳过规则停止贡献，最终 Current 只由 Base 与本轮有效父实例推导，State 移除继续完成。
- 首版自动观察范围：
  - 目标 AttributeComponent 内 Attribute 的 **CurrentValue**
  - 目标本地 BuffInstance 可访问的 Numeric StateParam **effective** 值
- 未自动观察的依赖（跨 Actor Attribute、装备字段等）提供显式 `RequestOngoingModifierRecalculation(SourceHandle)`（或等价 API）：只标脏，仍由 Flush 执行。
- StateParam 契约收紧：值变化**不得**同步执行 Attribute 重算；**可以**发布依赖失效通知。

## Impact

- 受影响规范：
  - `attribute-modifier-runtime`（新增依赖链 / Flush / 循环检测契约）
  - `attribute-management`（Component 调度、与现有重算路径衔接）
  - `state-parameter-management`（失效通知、禁止同步下游重算）
- 受影响代码（实现阶段）：
  - `UTcsAttributeComponent` AttrMod Application / Evaluation / Aggregation / OngoingCalculation
  - Evaluator Context / Snapshot 读取路径
  - Numeric StateParam effective 提交路径
  - 可能的帧末调度（Component 或轻量 World 子系统，实现时定）
- **备注（规划 §13 Change 4）**：本 change 修改的是 Change 3 归档后的 Ongoing 受控惰性重算契约，**不是**旧 OperandBindings 契约。

## Non-Goals

- 不实现跨 Actor / 跨 Component 全局依赖图或全局重算调度器。
- 不在 StateParam setter 中同步调用 Attribute 重算。
- 不用固定次数迭代掩盖循环依赖。
- 不建立 SourceHandle → UObject 全局注册表。
- 不把临时跳过自动升级为 OwningState 移除；State 生命周期由业务层另行决定。
- 不在本 change 中迁移 Attribute 数值类型或实现任意精度大数；该基础数值模型另行提案。
- 不实现伤害模块（Change 5）。
- 不设自动化测试任务（用户级 UE5 规范）；验证以编译与人工场景为准。
