## 1. 依赖记录与索引

- [x] 1.1 定义 `DependencyKey`、`DependencyRevision` 以及父实例依赖记录结构。
- [x] 1.2 在 AttributeComponent 上实现 `ReverseDependencyIndex` 与 `DirtyOngoingModifierSet`。
- [x] 1.3 在 Ongoing 重算路径中，于 Evaluator Context 读取可观察值时自动收集并写回父实例依赖记录，并重建反向索引。

## 2. Flush 调度（策略 C）

- [x] 2.1 实现 `FlushDirtyOngoingModifiers`：合并 Dirty、依赖闭包、自排除 Snapshot、拓扑/稳定序、父实例原子重算、Clamp/范围传播/最终事件。
- [x] 2.2 在 Apply / SetAttributeBaseValue / Commit / 显式 Recalculate 等受控路径安全点：若 Dirty 非空则同步 Flush。
- [x] 2.3 实现跨调用栈 Dirty 的帧末（或等价延迟）Flush；禁止在依赖生产者 setter/回调内同步递归全图重算。
- [x] 2.4 循环依赖：保留现有检测与诊断，补充精确 SCC 输出；已注册父实例形成循环时 Error 并临时跳过最小 SCC，Initial Apply 仍零提交。

## 3. 生产者挂钩

- [x] 3.1 Attribute CurrentValue 成功提交且发生真实变化时递增 Revision，并 MarkDirty 反向索引中的父实例。
- [x] 3.2 目标本地 Buff Numeric StateParam effective 成功提交且真实变化时递增 Revision，并通知目标 AttributeComponent MarkDirty。
- [x] 3.3 确认 StateParam 路径不调用同步 Attribute 全量重算。

## 4. 显式回退与边界

- [x] 4.1 实现 `RequestOngoingModifierRecalculation(SourceHandle)`（或等价命名）：仅标脏匹配来源的 Ongoing 父实例。
- [x] 4.2 明确首版不自动观察：跨 Actor Attribute、非目标 Component、装备/业务 UObject 字段。
- [x] 4.3 与 RemoveOngoing / FinalizeStateRemoval 协作：移除父实例时清理依赖记录与反向索引，避免悬空 Dirty。

## 5. 无效贡献临时跳过与恢复

- [x] 5.1 保持唯一 Ongoing 注册表，移除 `BlockedDirtyOngoingModifierInstIds`；以空 `AppliedOperations` 表示已注册父实例本轮无有效贡献，不新增 Quarantined / Disabled / Retry 容器。
- [x] 5.2 已注册父实例 Evaluator 失败时按父实例整体临时跳过；Merger 失败或多来源 Operator 失败时跳过完整 `ModifierDefId` 组，并从 Current 移除旧贡献。
- [x] 5.3 循环时临时跳过最小 SCC；Current Clamp / Range 失败时跳过受影响闭包；Base Clamp / Range 配置无合法结果时所有构建 Fatal。
- [x] 5.4 任意 Attribute 事务重新尝试全部空贡献父实例；成功恢复 AppliedOperations，失败保持为空且只记录 Error，不广播中间事件。
- [x] 5.5 Initial Apply 新候选在完整候选图中失败时整次零提交，不保存空贡献注册项，也不改变既有 Current。
- [x] 5.6 State Step 5 先从候选注册表删除待清理 SourceHandle，再运行同一有效贡献筛选并原子提交；重复 Apply、SourceHandle 查询与 Owner 清理只维护唯一注册表。

## 6. 规格与验证

- [x] 6.1 保持 delta 与实现一致；必要时修正 `attribute-modifier-runtime` Purpose 旧 OperandBindings 残留描述。
- [x] 6.2 执行 `openspec validate add-ongoing-attribute-dependency-recalculation --strict --no-interactive`。
- [x] 6.3 编译 `TireflyGameplayUtilsEditor Win64 Development` 及受影响 Glue/Managed。
- [x] 6.4 人工场景：Attribute 依赖链收敛、StateParam 失效标脏、同帧合并、循环 SCC 临时跳过、Evaluator/Operator/Merger 错误停止旧贡献、后续事务恢复、Initial Apply 零提交、Source Step 5 与显式 Request；不设自动化测试任务。
