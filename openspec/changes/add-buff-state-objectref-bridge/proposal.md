# 变更：为 BuffStateTree 增加 StateInstance ObjectRef 桥接 Evaluator

## 背景

`refactor-state-runtime-access-contract` 已经把 BuffStateTree 的根上下文收敛成单一 `BuffInstance`。这条边界是对的，因为 `UTcsStateInstance` 是共享执行态基类，不应该再次被重新暴露成 concrete root context。

但这次收敛也暴露出一个明确缺口：`UTcsStateInstance` 在 [ObjectRef] 区域中已经持有 `Owner`、`OwnerController`、`OwnerStateCmp`、`OwnerBuffCmp`、`Instigator` 等运行时引用，并且 C++ 节点实现代码可以通过 getter 正常访问；真正缺的是 **编辑器绑定面板里的统一可绑定入口**。

当前直接把这些字段暴露给 StateTree 绑定系统并不合适，原因有两个：

- `UTcsStateInstance` 当前的 `ObjectRef` 字段是运行时内部弱引用，不是 editor-facing binding contract
- 若改成一组 `PropertyFunction`，当需要整组暴露 `ObjectRef` 时会迅速退化成大量样板节点与重复桥接结构

因此本提案的目标是：在不破坏 `BuffInstance` 单根上下文约束、不修改 `UTcsStateInstance` 现有字段合同的前提下，为 BuffStateTree 提供一个单节点、成组输出的 ObjectRef 桥接能力。

## 变更内容

- 新增 `FTcsSTEvaluator_ObjectRef`，作为 BuffStateTree 内统一的 ObjectRef 桥接 evaluator。
- 新增 `FTcsSTEvaluator_ObjectRefInstanceData`，以 `Output` 分类暴露以下桥接输出：
  - `Owner`
  - `OwnerController`
  - `OwnerStateComponent`
  - `OwnerBuffComponent`
  - `OwnerAttributeComponent`
  - `OwnerSkillComponent`
  - `Instigator`
  - `InstigatorController`
  - `InstigatorStateComponent`
  - `InstigatorBuffComponent`
  - `InstigatorAttributeComponent`
  - `InstigatorSkillComponent`
- evaluator 通过 `TStateTreeExternalDataHandle<UTcsStateInstance>` 读取共享执行态外部数据，并在 `TreeStart` 时把 getter 结果同步到输出。
- evaluator 在 `TreeStop` 时清空桥接输出，避免旧引用在调试或重启场景下残留为陈旧显示值。
- 桥接输出继续保持 `TWeakObjectPtr` 语义，不把 `UTcsStateInstance` 的运行时弱引用桥接为额外强引用。
- BuffStateTree 的 schema root context 继续保持单一 `BuffInstance`；本 change 不新增第二个 root context，也不恢复 generic `StateInstance` root。

## 影响范围

- 受影响规范：
  - `buff-state-tree-objectref-bridge`
- 受影响代码：
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Public/StateTree/**`
  - `Plugins/TireflyCombatSystem/Source/TireflyCombatSystem/Private/StateTree/**`
- 受影响文档：
  - `Plugins/TireflyCombatSystem/README.md`
  - `Plugins/TireflyCombatSystem/Documents/文档：TCS编辑器测试方案（State-Buff-Attribute）.md`
- 与现有变更的关系：
  - 依赖 `refactor-state-runtime-access-contract` 已经收敛出的 `UTcsSTSchema_Buff` 单根上下文契约
  - 不回滚 `BuffInstance` 单根上下文，也不重新引入 generic `StateInstance` concrete schema
- 明确不在本提案范围内：
  - 让 `UTcsStateInstance::ObjectRef` 字段本体直接进入编辑器绑定面板
  - 为每个 ObjectRef 单独新增一组 `PropertyFunction`
  - 修改 `UTcsStateInstance` 当前弱引用字段为强引用或 editor-configurable 数据