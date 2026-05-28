# 变更：优化 TCS Buff Merge 运行时

## 背景

当前 Buff merge 链路在每次槽位激活刷新时，仍然要支付一次“整槽位重新分组”的成本。

现在的 `UTcsBuffComponent::ProcessBuffMerging()` 会扫描整个 `FTcsStateSlot::States` 数组，重建临时的 `StateDefId -> BuffGroup` 分组，并对所有发现的分组重新执行 merge 分发，即使这次实际上只有一个分组发生了变化。这个实现目前足够直接，但也意味着运行时还没有形成下面这些持久化认知：

- 当前槽位里已经存在哪些 Buff merge group
- 哪些 group 现在是 dirty 的
- 一个 group 为什么会变 dirty
- 某个具体 merger 到底依赖哪些运行时变化

已有的对比文档已经得出结论：长期方向应该是“完整的 group-runtime 模型”，而不是持续重复做整槽位 regroup。本提案把这个方向正式化为一个独立能力，同时通过分阶段落地来控制一致性风险。

## 变更内容

- 新增一个独立的 Buff merge runtime capability，不再以临时 regroup 为中心，而是以槽位内的 group 运行时数据为中心。
- 在 `FTcsStateSlot` 上引入持久化 merge-group 运行时状态，包括：
  - 按 `StateDefId` 维护的 Buff merge group runtime 条目
  - dirty-group 集合
  - force-rebuild 回退路径
- 为 Buff merge 失效引入显式 dirty-reason 标记，至少覆盖：
  - 成员关系变化
  - 运行时数值变化
  - 执行阶段变化
  - 槽位 Gate 变化
  - 强制重建
- 为 `UTcsBuffMerger` 增加依赖声明能力，使运行时能够判断“当前 dirty reason 是否真的要求这个 merger 重新处理”。
- 保持当前 `Merge(BuffsToMerge, MergedBuffs, MergedOutBuffs)` 协议不变，并继续复用现有 merged-out removal 链。
- 把 `ProcessBuffMerging()` 从“整槽位 regroup”改为“基于已维护 runtime group 的 dirty-group 处理”。
- 整个运行时按两个阶段推进：
  - Phase 1：先把最终运行时模型本身搭起来，包括 group cache、dirty 集合、安全 rebuild 回退，以及基于 membership 的失效路径。
  - Phase 2：再接入 stack change、stage change、slot-gate change 等运行时敏感 dirty 源，并补齐 dirty reason / dependency behavior 的诊断面。

## 影响范围

- 受影响 specs：
  - `buff-merge-runtime`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/State/TcsStateSlot.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/TcsBuffComponent.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Buff/TcsBuffComponent.cpp`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/Buff/BuffMerger/TcsBuffMerger.h`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/Buff/BuffMerger/**` 下的内建 merger 实现
- 受影响文档：
  - `Plugins/TireflyCombatSystem/Documents/文档：BuffMerger流程优化方案对比（最小增量方案与完整方案）.md`
  - 描述 Buff merge 行为的实现说明 / 等待开发者手动执行的编辑器测试说明
- 明确不在本提案范围内：
  - 编辑器 authoring 相关改动
  - Skill 重复激活策略
  - 各个 merger 的业务语义重设计