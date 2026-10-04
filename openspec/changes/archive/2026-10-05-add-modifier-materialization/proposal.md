# Change: 状态修正器物化（D3-19 落地）

## Why

R5 状态层已能让状态被施加、查到、到期（`state-instance-lifecycle`），并冻结自己的数值快照（`state-param-snapshot`）——但 `FTcsStateDefBase::ModifierRows` 至今**零消费者**：施加一个 buff 还改不动任何一个数值，"状态"只是一条会过期的记录。

本变更把 D3-19 的"修正器模板 + 引用行 + 覆写走参数传值"落成真实管线：施加时按模板物化出 M2 账本条目（`Source` = 状态实例来源句柄），移除/到期时按**同一个来源句柄**级联摘除。它是 R5 第一个"肉眼可见"的硬信号（属性数值首次被状态改动），也是参数快照与读取适配器的第一个真实消费者。

## What Changes

- **物化器**：新增 `FTcsStateModifierMaterializer::Materialize(...)`——逐条解析 `ModifierRows` 模板资产 → 求值操作数（`OPK_Literal` 走物化上下文求值并转规范值；`OPK_AttributeScaled` 按设计保持 live、不物化）→ 经显式构造入口装配账本条目。
- **快照作为"本次求值的参数表"**：新增反射壳 `UTcsStateParamTableReader : UObject, ITcsParamTableReader`（**只委托**既有纯 C++ 读取适配器，查找语义仍一份实现），由状态门面持有单例 + **RAII 栈式绑定**（嵌套按栈恢复），物化求值时装入 `FTcsParamEvaluateContext::ParamTable`——使模板里 `ParamRef` 形态的操作数能从**该状态自己的快照**取值（D3-19 原文"物化点从施加方 ParamSnapshot 解析"）。MUST NOT 为同一次求值再开一条并行的数值表通道（同一问题一个答案）。
- **施加挂点**：快照构建完成后、`TcsEvent.State.Applied` 广播之前，在**同一个属性变更批**内逐条 `ApplyModifier`（多个修正器只触发一次重算与一次广播；订阅者读到的已是改过的数值）。
- **刷新挂点**：刷新路径先按来源摘除旧条目、再按重建后的快照重挂（`Source` 不变）——账本恒与最近一次快照一致，条数不累加。
- **移除挂点**：撤销顺序扩为「撤时间条目 → **按来源摘除修正器（批内提交）** → `Expiring` → 广播 → 归还槽位」——`Expired` / `Removed` 的订阅者在回调里读到的是**已归位**的属性值；单位注销复用同一条链。
- **属性访问解析点**：新增 `FTcsStateAttributeAccess::Resolve(const UWorld*)`（住 `Private/State/`，头文件名与 TcsEffect 侧同类解析点**不重名**——UHT 要求全项目头文件名唯一）；它是**薄包装**（暴露面 = 白名单方法，不把门面指针交出去），本轮上限 = 语义面三个（`ApplyModifier` / `RemoveBySource` / `EvaluateCurrent`）+ 事务对（`BeginBatch` / `Commit`）+ 一个存在性判据（`ResolveStore`，只作"账本认不认识这个单位"）；将来补 `ITcsAttributeAccess` 注入契约时只换这一处。
- **账本条目构造入口**：`FTcsAttrModInstance` 补 `MakeFromDef(Row, Operand, Source)`——"模板行 → 账本条目"的字段映射的唯一声明处（住同时拥有两个形状的 TcsAttribute）。

## Impact

- **Affected specs**：
  - `state-modifier-materialization`（**ADDED**，新能力：物化入口与流水 / 物化上下文的参数表 / 施加挂点 / 刷新重物化 / 移除与到期级联摘除 / 属性访问解析点 / 来源句柄身份一致性）
  - `state-param-snapshot`（**MODIFIED** 1 条：读取适配器补"反射壳 + 门面单例 + RAII 栈式绑定"——原需求把"继承接口的时机"留白，本变更即该时机的落地）
- **Affected code**：
  - `Source/TcsState/`：新 `Public/State/TcsStateModifierMaterializer.h` + `Private/State/TcsStateModifierMaterializer.cpp`、新 `Public/State/TcsStateParamTableReader.h`（反射壳）、新 `Private/State/TcsStateAttributeAccess.h`、新 `Private/State/TcsStateOps_Modifier.cpp`；改 `TcsStateSubsystem.h/.cpp`（持壳 + 绑定作用域）、改 `Private/State/TcsStateOps.cpp`（apply / remove 两个挂点）
  - `Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h`：补显式构造入口（不改字段集）
  - 宿主装置（**LAC 仓** `Source/TcsDev/`）：新增属性面检查块与拒绝面探针
- **无数据/资产迁移**：本变更只新增类型、成员函数与挂点，`FTcsBuffDef` / `FTcsAttrModDefTableRow` 字段集不变，既有定义资产（`DA_Check_BuffDef`）无需改动即继续有效。

## 非目标

- 技能参数行分派（`UTcsSkillModDef` / R5.5-f）；关系表检查器（R5.5-e）；参数链式聚合（R6）。
- 属性写侧的注入契约（`ITcsAttributeAccess`，台账 `R-7`）与属性写入面的脚本可达（台账 `SCRIPT-9`）。
- 脚本源读快照的**行为**验证（反射壳使其在结构上可达，但"C# 侧能否真调通 `TrySetNumericParam`"本变更不 claim，需独立探针）。
