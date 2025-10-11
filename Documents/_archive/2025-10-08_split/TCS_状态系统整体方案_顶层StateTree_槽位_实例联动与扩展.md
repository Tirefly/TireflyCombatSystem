# TCS 状态系统整体方案：顶层 StateTree ↔ 槽位 ↔ 实例 联动与扩展（新版）

文档目的：整合并取代“仅限映射联动”的旧范围，将顶层 StateTree、状态槽（StateSlot）、状态实例（StateInstance）联动的现状与规划统一在一份文档中，便于设计与实现对齐。旧文档保留以供对比。

- 适用版本：UE 5.6（StateTree 外部/上下文数据句柄已适配）
- 面向读者：系统/玩法程序、技术策划
- 关联代码：Plugins/TireflyCombatSystem/Source/TireflyCombatSystem

## 1. 结论速览
- 已实现（核心闭环）：
  - UTcsStateComponent 继承 UStateTreeComponent；槽位 <-> 顶层 StateTree 状态名 映射；Gate 开/关；两种激活模式；按优先级激活；实例 StateTree 生命周期与 Tick；数据表驱动配置；StateTree 任务/条件适配 5.6。
- 尚未实现（按优先级）：
  1) 合并器接入 AssignStateToStateSlot（Same/Diff Instigator）
  2) 管理器 ApplyStateToSpecificSlot（直达指定槽位）
  3) 槽位排队与延迟应用（Queue/Process/Clear；与 Gate 联动）
  4) 持续时间策略 ETcsDurationTickPolicy（ActiveOnly/Always/...）
  5) 暂停/恢复与抢占策略（GateCloseBehavior/PreemptionPolicy/HangUp）
  6) 顶层 Slot Debug Evaluator（调试可视化）

## 2. 总体架构
三层职责清晰：
- 顶层 StateTree：可视化编排“槽位 Gate 开/关与时序”。
- 组件/槽位：存储、优先级、激活、（未来）合并与排队、（未来）持续时间策略、（未来）暂停恢复语义。
- 状态实例：各自运行 StateTree（执行技能/Buff 等逻辑）。

核心链路：顶层状态进入/退出 → 组件 Gate 切换 → UpdateStateSlotActivation(Slot) → 模式/优先级决定激活 → 实例 StateTree 启停与 Tick。

## 3. 数据与配置
- 槽位定义（数据表行）：FTcsStateSlotDefinition
  - 字段：SlotTag、ActivationMode（PriorityOnly/PriorityHangUp/AllActive）、StateTreeStateName（用于映射）
  - 配置入口：UTcsCombatSystemSettings::SlotConfigurationTable（行结构要求 FTcsStateSlotDefinition）
- 槽位 Gate：UTcsStateComponent::SlotGateMap（默认开启；由顶层 StateTree 驱动开/关）
- 状态定义（数据表行）：FTcsStateDefinition
  - 关键字段：StateSlotType、Priority、DurationType/Duration、SameInstigatorMergerType、DiffInstigatorMergerType、StateTreeRef、Tags ...
  - 未来增强（设计已定，尚未落地）：
    - ETcsDurationTickPolicy（见第 8.2）
    - ETcsPreemptionPolicy（同槽抢占暂停语义，见第 8.3）
- 代码常用 API（组件侧，节选）：
  - BuildStateSlotMappings() / IsSlotGateOpen() / SetSlotGateOpen()
  - AssignStateToStateSlot() / GetHighestPriorityActiveState() / GetActiveStatesInStateSlot() / GetAllStatesInStateSlot()
  - OnStateTreeStateChanged(...)（接收顶层状态切换） / UpdateStateSlotActivation(Slot)
- 代码常用 API（管理器）：
  - GetStateDefinition() / CreateStateInstance() / ApplyState()

## 4. 运行时流程（实现）
- 启动：BeginPlay → InitializeStateSlots() 读取数据表 → BuildStateSlotMappings() 建立“槽位 ↔ 顶层状态名”映射。
- 顶层切换：OnStateTreeStateChanged(...) → 基于当前 ActiveNames 决定每个 Slot 的 Gate 开/关 → 对变更的槽位调用 UpdateStateSlotActivation()。
- 分配：AssignStateToStateSlot(State, Slot)
  - 清理无效项 → 查找同 DefId（当前为“跳过重复”的占位策略；将由合并器替代）→ 插入并按 Priority 排序。
  - Gate 关闭：全部置 Inactive（存储，不激活）。
  - Gate 打开：按 ActivationMode 激活（PriorityOnly 仅最高优先级；AllActive 批量）。
- Tick：UTcsStateComponent::TickComponent()
  - 对激活且 SDT_Duration 的状态递减剩余时间（当前策略等价于 ActiveOnly）
  - 对 Active 的实例推进其 StateTree Tick。

附：策略调用时机（设计约定）
- 轻量前置（CheckImmunity）：在 `ApplyState()` 管线开始处，仅评估免疫；
  - 若命中免疫：直接拒绝应用（不创建实例）；
  - 否则：进入分配流程（不返回“缩放/抗性”）。
- 净化（Cleanse）：由顶层 StateTree 的任务在需要时触发（例如进入某状态节点 OnEnter 调用“Cleanse”任务），传入“选择器策略句柄 + 策略参数”以执行移除。

## 5. 顶层 StateTree 与节点
- 条件：FTcsStateCondition_SlotAvailable → 判断 Gate 开启 且 槽位未被激活态占用（可扩展优先级/预占位策略）。
- 任务：FTcsCombatTask_ApplyState → 通过 UTcsStateManagerSubsystem::ApplyState() 应用状态（Owner/Instigator 使用 Context 句柄回退）。
- Schema：TcsCombatStateTreeSchema（上下文外部数据约束）。

附：ActivationMode 语义（同槽策略统一）
- `PriorityOnly`：仅最高优先级实例处于 Active，较低优先级实例被停止（Cancel）。
- `PriorityHangUp`：仅最高优先级实例处于 Active，较低优先级实例进入挂起（HangUp/暂停 Tick，不 Stop，不 Exit）。当高优先级退出时，按优先级恢复。
- `AllActive`：同槽所有实例均可 Active（组件负责并发语义）。

## 6. 管理器与跨实体
- GetStateDefinition/ CreateStateInstance/ ApplyState 实现数据表驱动与实例工厂。
- 未来扩展：ApplyStateToSpecificSlot(Target, DefId, Source, TargetSlot, Params)（见第 8.1）。

## 7. 配置规范
- 数据表：
  - SlotConfigurationTable：行结构 FTcsStateSlotDefinition，填写 SlotTag、ActivationMode、（可选）StateTreeStateName。
  - StateDefTable：行结构 FTcsStateDefinition，定义状态的元信息与 StateTreeRef。
- 顶层 StateTree：
  - 将“Gate 状态”命名为与 StateTreeStateName 一致；保持唯一。

## 8. 扩展设计（未实现项的落地方案）
### 8.1 槽位排队与延迟应用（Queue/Process/Clear）
- 目标：当 Gate 未开启或策略不允许时，暂存待应用；Gate 打开后自动应用，避免状态丢包与竞态。
- 组件 API 草案：QueueStateForSlot(State, Slot)，ProcessQueuedStates(DeltaTime)，ClearQueuedStatesForSlot(Slot)
- 数据：FQueuedStateData{ StateInstance, TargetSlot, EnqueueTime, TTL, IsExpired() }；组件成员 QueuedStates。
- 流程：AssignStateToStateSlot 遇阻 → Queue；Gate 打开或定时处理 → 再次尝试 Assign；过期清理。

### 8.2 持续时间计时策略 ETcsDurationTickPolicy
- 候选：ActiveOnly（默认）、Always、OnlyWhenGateOpen、ActiveOrGateOpen、ActiveAndGateOpen。
- 行为：按策略决定是否递减 RemainingDuration（SDT_Duration）；不改变激活/停用与 StateTree 启停。

### 8.3 暂停/恢复 与 抢占策略
- Gate 关闭行为（Slot 级）：PauseOnGateClose / CancelOnGateClose。
- 同槽优先级抢占（State 级）：PreemptionPolicy = PauseOnPreempt / CancelOnPreempt。
- 挂起语义：Stage=SS_HangUp，保留 FStateTreeInstanceData，不 Stop；Resume 后直接继续 Tick。

### 8.4 合并策略接入 UTcsStateMerger_*
- 命中同 DefId 时，按 Same/Diff Instigator 选择合并器；用合并结果替换 Slot 集合并重排；统一触发 UpdateStateSlotActivation。

### 8.5 顶层 Slot Debug Evaluator
- 目标：在顶层调试器中展示每槽 Gate/ActiveCount/TotalStored/HighestActiveDefId。
- 方案：轻量 Evaluator，外部数据绑定 UTcsStateComponent，Tick 汇总快照并输出描述。

## 9. API 一览（实现/计划）
- 组件（实现）：BuildStateSlotMappings / SetSlotGateOpen / IsSlotGateOpen / AssignStateToStateSlot / GetHighestPriorityActiveState / GetActiveStatesInStateSlot / GetAllStatesInStateSlot / UpdateStateSlotActivation / OnStateTreeStateChanged
- 组件（计划）：QueueStateForSlot / ProcessQueuedStates / ClearQueuedStatesForSlot / PauseStateInstance / ResumeStateInstance / PauseSlot / ResumeSlot
- 管理器（实现）：GetStateDefinition / CreateStateInstance / ApplyState
- 管理器（计划）：ApplyStateToSpecificSlot
- StateTree 节点（实现）：FTcsStateCondition_SlotAvailable / FTcsCombatTask_ApplyState / FTcsCombatTask_ModifyAttribute / 若干 Condition
- 调试（计划）：FTcsSlotDebugEvaluator

## 10. 验收与测试建议
- 功能：两种激活模式；Gate 开/关一致性；顶层切换可视化；ApplyState 行为正确（有/无槽位）。
- 边界：重复 DefId 的合并与排序（实现后）；Gate 切换抖动与并发；长队列退化；时长策略覆盖。
- 性能：批量状态激活/移除的排序与映射维护；Tick 里外部数据拉取与日志级别。

## 11. 迁移与兼容
- 旧 API 兼容：TryAssignStateToStateSlot/ GetStateSlotCurrentState → 由新实现内部转发。
- 配置兼容：未配置的槽位默认为 AllActive；无 StateTreeStateName 仅失去 Gate 联动，槽位功能仍可用。
- 文档对齐：本文件合并了以下文档的“现状 + 规划”，旧文档可保留对比或删除：
  - TCS_状态槽系统架构分析与改进.md（激活模式/优先级/数据表/查询）
  - TCS插件StateTree集成优化开发计划.md（继承/映射/联动/Queue/指定槽接口等）
  - TireflyCombatSystem_开发计划.md（双层架构与命名已更新为 UTcs*）

## 12. 参考与附录
- 代码主类：UTcsStateComponent、UTcsStateInstance、UTcsStateManagerSubsystem、TcsCombatStateTree*、TcsCombatSystemSettings。
- 配置示例：
  - Project Settings → Game → Tcs Combat System：SlotConfigurationTable（FTcsStateSlotDefinition），StateDefTable（FTcsStateDefinition）。
- 术语：
  - Gate：由顶层 StateTree 决定槽位是否允许激活；
  - PriorityOnly/AllActive：槽位激活模式；
  - HangUp：暂停但不停止子 StateTree；
  - Merger：同定义的状态合并策略。
