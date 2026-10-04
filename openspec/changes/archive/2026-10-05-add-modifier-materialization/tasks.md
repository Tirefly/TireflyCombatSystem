## 1. 类型与入口

- [ ] 1.1 `FTcsAttrModInstance::MakeFromDef(Row, Operand, Source)` 显式构造入口（TcsAttribute；不改字段集）
- [ ] 1.2 `UTcsStateParamTableReader` 反射壳（委托纯类本体）+ `FTcsStateParamTableReader` 标注实现纯读取契约
- [ ] 1.3 状态门面持有壳单例（`UPROPERTY`）+ RAII 栈式绑定作用域（进入存旧、退出恢复）
- [ ] 1.4 `FTcsStateModifierMaterializer::Materialize` 逐条流水（解析模板 / 求值操作数 / 值约定转规范值 / 装配）

## 2. 挂点接线

- [ ] 2.1 `FTcsStateAttributeAccess::Resolve(const UWorld*)` 属性访问解析点（唯一解析处；头文件名避重）
- [ ] 2.2 `TcsStateOps_Modifier.cpp`：施加挂点（`BeginBatch` → 逐条 `ApplyModifier` → `Commit`，位于 `Applied` 广播之前）
- [ ] 2.3 刷新路径：先按来源摘除旧条目，再按重建快照重挂（`Source` 不变）
- [ ] 2.4 移除与到期挂点：撤时间条目 → 按来源摘除（批内提交）→ `Expiring` → 广播 → 归还槽位
- [ ] 2.5 单位注销与 `Deinitialize` 路径复核（复用同一条移除链，零残留）

## 3. 编译与冒烟

- [ ] 3.1 UBT Development Editor 编译通过（零 warning / 零 error）
- [ ] 3.2 UBT Game Shipping 编译通过（含新 `UPROPERTY` 面 ⇒ 本任务必跑）
- [ ] 3.3 宿主装置（**LAC 仓** `Source/TcsDev/`）新增属性面检查块 + 拒绝面探针（常规命令零红字，故意的失败独立成 `.Reject`）
- [ ] 3.4 PIE 实测：施加减益 buff ⇒ 账本条目出现 + `EvaluateCurrent` 前后可观测 ⇒ 驱散 ⇒ 条目消失且余量归位（`RemoveBySource` 返回恰 1 条）

## 4. 收口

- [ ] 4.1 归档提案（`openspec archive add-modifier-materialization --yes`），新能力 `## Purpose` **手写**（归档器只为新能力生成占位符）
- [ ] 4.2 `openspec validate --all --strict --no-interactive` 全绿 + `openspec/changes/` 零活动提案
- [ ] 4.3 `PLN-R5` Task 4 勾选 + 落地记录（含对计划文本的订正：解析点头文件名、构造入口落法）
- [ ] 4.4 证据文档（区段行号 + 复算脚本 + 边界清单）+ 台账 + 两册日志 + `SPEC-02-states` §12.8
