## 1. 前置：两处解耦（裁定 1 / 5）

- [x] 1.1 `FTcsStateInstance` 补 `CascadeAnchor`（`FTcsSourceHandle`，恒非 0），字段住"身份与归属"组；头注释写清**两个句柄的分工**（`Source` = 施加方身份 / 续杯叠层判据 + 载荷；`CascadeAnchor` = 级联撤销锚点）
- [x] 1.2 `TcsStateOps.cpp` 新建实例处恒发号（`Subsystem.SourceRegistry.Allocate()`），`Source` 仍按"声明则沿用、未声明则发号"
- [x] 1.3 修正器三处改走锚点：`TcsStateModifierMaterializer.cpp` 的 `MakeFromDef(..., Instance.CascadeAnchor)`、`MountModifiers` 的刷新摘旧、`TcsStateOps.cpp` 移除路径的 `StripModifiers(..., Found->CascadeAnchor)`
- [x] 1.4 `UTcsAttributeSubsystem` 补 `static UTcsAttributeSubsystem* Resolve(const UWorld*)`（唯一查找点，注释写明"将来补注入契约只换这一处"）；`FTcsStateAttributeAccess::Resolve` 改调它，并**订正**其头注释里"允许面含 `EvaluateCurrent`"的失实描述（裁定 6）
- [x] 1.5 编译（Development Editor）+ 既有检查回归：19/20/21/22 各段读数与 Task 5 封存值逐字一致（锚点解耦 MUST NOT 改变任何既有读数）

## 2. 链运行态的"身份 + 因果边"（裁定 2）

- [x] 2.1 `FTcsEffectContext` 补 `RunSource`（**身份**：本次运行的来源锚点）与 `CausedBy`（**因果边**：启动本次运行的来源句柄）两个字段；头注释写明"边只作溯源、MUST NOT 参与撤销或共存判定"
- [x] 2.2 `UTcsEffectSubsystem::ExecuteChain` 在黑板未自带 `RunSource` 时发放一枚（`FTcsSourceHandleRegistry::Allocate()`）；**子链另发新号**（`RunSubChain` 走黑板值语义拷贝 ⇒ MUST 显式覆盖子链的 `RunSource`）
- [x] 2.3 两处写边：`TcsTriggerEvaluator` 起链前 `ChainContext.CausedBy = Instance->Source`；`TcsEffectSubsystem_StepProtocol` 起子链前 `ChildContext.CausedBy = ParentRun->Context.RunSource`
- [x] 2.4 `CHAIN-1` 台账行就地补记（"触发行身份只到 `CausedBy` 一跳；链路仍无触发事件 tag"）

## 3. `FTcsStepModifyAttribute`（TcsEffect）

- [x] 3.1 `TcsEffect/Private/Attribute/TcsEffectAttributeAccess.h/.cpp`（新）：白名单薄壳（`ApplyModifier` / `EvaluateCurrent` / `IsLedgerReady`），`Resolve` 经 `UTcsAttributeSubsystem::Resolve`；实现体住本 `.cpp`（TcsEffect 内**唯一** include 属性门面头处）
- [x] 3.2 `Public/Chain/TcsStepModifyAttribute.h` + `Private/Chain/TcsStepModifyAttribute.cpp`（新）：字段 `Target` / `Attribute` / `Op`(`ETcsAttributeOp`) / `Operand`(`FTcsParamValue`)；`Source` = `Context.RunSource`；头注释写明"允许依赖**下层**领域模块（Attribute），MUST NOT 依赖上层（Damage/Targeting/State/Skill）"
- [x] 3.3 一行宏自注册 `UE_DEFINE_EFFECT_STEP_EXECUTOR(FTcsStepModifyAttribute, ExecuteStepModifyAttribute)`；失败面 = `Warning` + `TSR_Completed`（不断链）

## 4. `AttributeCompare` 触发条件（裁定 4）

- [x] 4.1 `TcsAttribute/Public/Attribute/TcsAttributeComparison.h`（新）：`ETcsAttributeComparison{ Greater / Less / GreaterOrEqual / LessOrEqual }`（含 ToolTip）
- [x] 4.2 `TcsTriggerCondition.h` 补 `FTcsTriggerCondition_AttributeCompare{ Attribute, Comparison, Threshold }`；`TcsTriggerCondition.cpp` 补求值器（读 `Context.Caster` 的当前值，经 `FTcsEffectAttributeAccess`）+ 自注册；同时订正 `TcsTriggerCondition.h:18-19` 那句"`AttributeCompare` 零消费者"的注释
- [x] 4.3 边界如实登记：求值失败（无属性账本 / 属性不存在）⇒ **条件不通过 + 无红字**（与"条件未过不是故障"口径一致）

## 5. `FTcsStepApplyState`（TcsState）

- [x] 5.1 `Source/TcsState/Public/Chain/` 与 `Private/Chain/`（新目录）+ `TcsStepApplyState.h/.cpp`（新）：字段 `Target` / `DefTag` / `Overrides`；目标缺省取 `Context.Targets[0]`；`Source` = `Context.RunSource`、`Instigator` = `Context.Instigator`；`ParamTable` 传空（裁定 7）
- [x] 5.2 一行宏自注册；失败面统一 `Warning` / `Error` + `TSR_Completed`（裁定 3，不断链）；`Instigator` 全无效时退化为目标

## 6. 内联触发行接线（`TRIG-4` 行为半）

- [x] 6.1 `TcsState/Private/State/TcsStateOps_Trigger.cpp`（新）：`WireTriggerRows(Subsystem, Instance, Def)` / `UnwireTriggerRows(Subsystem, Instance)`——以 `Instance.CascadeAnchor` 为 `Source` 逐条 `RegisterTriggerRow` / 一次 `UnregisterTriggerRowsBySource`；本文件是 **TcsState 内唯一 include `TcsEffectSubsystem.h` 处**（与属性解析点同款单点纪律）
- [x] 6.2 `Apply` 新实例路径：接线排在 `ScheduleTime` 之后、`Broadcast(Applied)` **之前**（新登记的行要能看见自己的 `Applied`）
- [x] 6.3 `Remove`：退订排在 `StripModifiers` 之后、广播**之前**（它不该看见自己的 `Removed` / `Expired`）；`RefreshStacked` 路径**不重登记**（同一实例，行已在册）
- [x] 6.4 撤销顺序注释与规格同步为：**摘修正器 → 退订触发行 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**

## 7. 宿主装置检查（LAC 仓，跨仓交付）

- [x] 7.1 `TcsDevSliceRig.cpp` 新增检查 **23a–23h**（正例面、零故意红字）：23a 链里 `ApplyState` 生效（回执 + 状态计数）；23b 内联触发行在 `Applied` 上起链一次；23c `Expired` 不再起链（零增量）；23d 移除后 `GetTriggerRowCount` 回落（按锚点级联退订）；23e `ModifyAttribute` 改得动账本（施加前后读数）；23f `AttributeCompare` 两向（过 / 不过）；23g **同来源两实例撤销互不牵连**（裁定 1 的读数）；23h 系统级触发行起链（两笔运行的 `RunSource` 互异 + `CausedBy` = 该行 `Source`——身份与因果边各一条读数）
- [x] 7.2 `Config/DefaultGameplayTags.ini` 补探针词（状态定义身份 + 链 id 两处功能根下；CRLF/无 BOM 保持）
- [x] 7.3 夹具纪律（`MEM-20261004-27`）：夹具形状不得自造红字；读数形态先写清再写夹具
- [x] 7.4 双配置编译（Development Editor + Shipping）+ 整轮 PIE：`Tcs.Test.Slice.Run` 即时/延迟段与 `.Reject` 面读数留档；**头部预期红字不变**（本轮不加故意红字）

## 8. 规格与文档

- [x] 8.1 `openspec archive add-state-chain-primitives --yes`；`openspec validate --all --strict --no-interactive` 全绿；`openspec list` 复核零活动提案
- [x] 8.2 归档后**手工补**新能力 `state-step-library` 的 `## Purpose`（占位句必须换掉——第 5 次实证）
- [x] 8.3 证据文档 `Documents/combat-system-design/evidence/2026-10-05-state-chain-primitives.md`（读数为先：区段行号 / 字节 / SHA-256 / 复算脚本 / 如实边界清单）
- [x] 8.4 设计文档回写：`SPEC-03-effects` §2.1 原语 8 → 10、§2 边界句改述为"不依赖上层领域模块"；`SPEC-02-states` §1 的 `FStepApplyState` 落地状态
- [x] 8.5 计划 `plan-r5-state-layer.md` Task 6 前六步勾选 + 《Task 6 落地结果》块（逐条记计划文本订正）；两册日志 + 台账 + `INDEX` 同步
