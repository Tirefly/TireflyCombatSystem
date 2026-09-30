# Tasks: ModifyFlow 链原语（伤害修改器通道的提交侧）

> 来源：`Documents/combat-system-design/plans/plan-r4-trigger-row.md` Task 3。
> 验证纪律：**禁 TDD**；验证 = UBT 编译（Development，含 `UPROPERTY`/UHT 面）+ 定向人工检查；**本批不引入自动化测试**。

## 1. 消耗策略改造（**前置**——不先做，第 2 节的步骤 struct 编不过）

- [x] 1.1 `Source/TcsDamage/Public/Flow/TcsFlowAttributes.h`：`FTcsConsumePolicy` **改名 `FTcsDamageModifierConsumePolicy`** + 去掉 `TFunction<void()> OnConsumed`，只留 `{ int32 MaxUses; double Cooldown; int32 SortKey; }`；补注释说明"消费行为经事件语义表达（`DEC-04` §3.3 / 用户 2026-09-30 拍板），本结构保持纯数据"
- [x] 1.2 全库同步改名与引用：`Source/TcsDamage/` 内的 `FTcsFlowAttributeSubmit::Consume`、`FTcsFlowAttributes::Submit(...)` 形参、`TcsFlowAttributes.cpp`；**注释面**同步（`TcsDamageFlowContextView.h:21`、`TcsFlowDataSteps.h:44`——后者改写为"数据步骤不带消耗策略（职责划分：纯数值写入）"，不再引用已删除的 `TFunction` 理由）
- [x] 1.3 `TcsDamageSubsystem`（或 TcsDamage 的原生 tag 声明点）：按事件命名公约声明消费事件 tag `Tcs.Event.Damage.<事件名>`（**只声明、不发布**——发布归台账 `DAMAGE-4`）；常量名逐点换下划线（`Tag_Tcs_Event_Damage_...`）、声明处带 `TCSDAMAGE_API`（跨模块消费面纪律，`WAIT-9`）
- [x] 1.4 全库 grep `FTcsConsumePolicy` 与 `OnConsumed`：**活动代码零残留**；**文档/规格/归档提案的旧名不改**（归档提案是冻结历史；活动文档由第 5 节统一回写）

## 2. ModifyFlow 链原语

- [x] 2.1 新建 `Source/TcsDamage/Public/Chain/TcsStepModifyFlow.h`：`USTRUCT() FTcsStepModifyFlow`，四字段（`TargetKey` / `Op` / `Operand` / `Consume`），全 `UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain")`
- [x] 2.2 新建 `Source/TcsDamage/Private/Chain/TcsStepModifyFlow.cpp`：执行器实现（载荷解出 → `Submit` → 恒 `TSR_Completed`；载荷缺失/类型不符 → Warning + 完成；`TargetKey` 无效 → 执行器内兜底契约键）+ 一行 `UE_DEFINE_EFFECT_STEP_EXECUTOR`
- [x] 2.3 `Build.cs` 若有新增模块依赖需求则同步（预期**零新增**——只用已达成的依赖）

## 2.5 收集事件载荷读取器登记（用户 2026-09-27 裁定并入本 Task）

- [x] 2.5.1 为 `FTcsDamageFlowCollectEvent` 实现并登记载荷读取器（`UE_DEFINE_TRIGGER_PAYLOAD_READER`，住 TcsDamage）：`Caster ← FTcsDamageFlowContext::Attacker`、`ClassificationTags ← 上下文直通`；空载荷/空 `Context` → 返回默认构造
- [x] 2.5.2 核验"每事件一次"语义未被破坏（读取点仍在求值器的各行共用层，MUST NOT 挪进逐候选行循环）

## 3. 编译验证

- [x] 3.1 Development 编译，**零警告**（UHT 面是本批重点：消耗策略进 `UPROPERTY` 是改造前必错项）
- [x] 3.2 **Shipping 编译**——本批含 `UPROPERTY`/UHT 面，按仓内纪律必须双配置（Shipping 是唯一能照出"编辑器专用 API 泄漏"与反射面差异的检查）
- [x] 3.3 依赖面 grep 复验：`Source/TcsEffect/` 无任何 TcsDamage 类型 include；`git diff --stat -- Source/TcsEffect` 为空

## 4. 定向人工检查（2026-09-30 已完成；命令纪律：常规命令零红字，预期失败面独立成 `.Reject`）

- [x] 4.1 **配置面实证**（本批直接解除的阻塞）：链资产里 `FTcsStepModifyFlow` 的四个字段可配（picker 搜 `ModifyFlow` → `TcsStepModifyFlow`），`Consume.MaxUses = 3` 保存 → 重载资产后值原样保留。**另有两条不依赖编辑器的证据**：①UHT 差分（三字段旗标字与既有可持久化属性逐位相同、`Consume` 的 struct getter 指向本类型、两配置产物 SHA256 逐字节相同）；②运行期保真——`检查 3` 步数由 10 → 11 且全可解析（见 §6）
- [x] 4.2 **注册可达**（**无独立检查面 ⇒ 用运行反证**；用户 2026-09-30 裁定本批不加代码）：`FTcsEffectStepExecutorRegistry` 只有 `GetDynamicKeys()`（仅动态项）、**无静态项枚举 API**，仓内唯一入库调试口是 `Tcs.Damage.DumpRecords` ⇒ 静态自注册项无法被独立查询。**反证已成立**：链里带 `FTcsStepModifyFlow` 跑一次，**执行器被调用并打出本模块的 Warning**（`LegendAutoChess.log:2601`）——未注册则解释器报"未知步骤类型"并断链，也不会有这行。枚举面归 M8/TOOLS-6 Explain 轮（已登记台账 `TOOLS-6`）
- [x] 4.5 **降级面**：`EventPayload` 为空时步骤留 Warning 且链继续（不崩溃、不静默通过）——**实测成立**（同一次运行：Warning 恰一条、链继续走完、全日志零 Error 零 ensure）。**其"独立成 `.Reject` 命令"的收口随装置一并归 Task 4**
- **4.3 / 4.4 已移出本提案 → R4 Task 4**（用户 2026-09-30 裁定：连同测试结果说明一起并入，使上下文完整）。**移出理由**：两者都要**触发行**，而全仓 `RegisterTriggerRow` / `FTcsEffectTriggerInstance` 在 `TcsEffect/` 之外**使用点为零**、Task 2.5 的 `UTcsEffectTriggerDefAsset` 载体未实现 ⇒ 触发行今天**只能 C++ 注册**，属 Task 4 Step 2「装置扩展」的活（`4.3` 原文即自注"端到端（**Task 4 主体的一部分**）"）
  - 4.3 端到端"破甲"：触发行命中 → 单步链 `[ModifyFlow]` → 黑板多出一笔提交 → 后续 `Damage` 读到修正值（`BaseDamage` 经 `Op=Add` 后扣血值与预期一致）
  - 4.4 载荷读取器实证：订阅 `Tcs.Event.Damage.AfterDamage` 的触发行，`Caster` 解析为本次流程的 `Attacker`（改造前读到空句柄）

## 5. 收束

- [x] 5.1 设计文档回写：`09-module-damage.md` §2.3（伤害修改器唯一通道——补 `ModifyFlow` 落地状态与"消费行为 = 事件语义"的口径）；`04-module-effects.md` 若需（链原语清单）
- [x] 5.2 `deferred-inputs-ledger.md`：`DAMAGE-3` 标"部分消费（`ModifyFlow` 已落地，`Heal` 余）"；`DAMAGE-4` 补注"形状已定（`DEC-04` 裁定 ④ 已落地），消费动作仍待 R5/M4a"；`SCRIPT-3` 补注"`TFunction` 根因已消除"
- [x] 5.3 `decisions-log.md` 追加本条（含"`FTcsConsumePolicy` 改事件语义"的裁定落点）
- [x] 5.4 提案归档（`openspec archive add-damage-modifyflow-primitive`）+ `openspec validate --specs --strict` 全绿——**归档前已在 `%TEMP%` 副本预演过一次**（`+3 ~3 −0`、`validate --specs` 24/24），故正式归档为可预期操作
- [ ] 5.5 提交边界：只 stage 本批文件（`Source/*/Private/Testing/` 按惯例排除）；**提交需用户明确授权**（清单已按"每个文件只出现在一个提交里"分区验证：源码 9 / 文档 12 / OpenSpec 归档后 8）

## 非目标（MUST NOT 扩大）

- `MaxUses` / `Cooldown` 的**实际消费动作**（扣次数、起冷却、标记已消费）→ 台账 `DAMAGE-4`，R5/M4a
- `FTcsFlowExecute` 的 `BestIndex` 消费落地 → 同上
- 消费事件的**发布**（本批只声明 tag 与形状）
- 其余链原语（`SetVar`/`Branch`/`RunSubChain`/`WaitEvent`）与 **P-A 排序相位** → Task 3.5
- `Heal` 执行器 → 台账 `DAMAGE-3` 余项

## 6. 验收证据（2026-09-30；用户执行 + 日志复核）

**人工侧（编辑器内）**：picker 可见 `TcsStepModifyFlow`、四字段可配、`Max Uses=3` 存得住；探测后已把该步从 `DA_SliceChain` 删除并保存（`LegendAutoChess.log:2625`，11:59:00），夹具复原。

**装置侧**（`Tcs.Test.Slice.Run` 单次运行，`LegendAutoChess.log:2557`–`:2620`）：

| 判定 | 结果 |
|---|---|
| 检查 0（定义库就绪 / 失败清单） | PASS——`IsRuntimeReady=true`、失败 0 条 |
| 检查 1a / 1b（自动发现 + 缓存） | PASS——扫到链资产 2 个、缓存 2/2（未调 `RegisterChain`） |
| 检查 2（世界装配） | PASS——2/2 `FindChain` 可查 |
| **检查 3（内容保真哨兵）** | **PASS——11/11 步全可解析、不可解析 0**。该数由 10（3+7）→ 11 = 新增的 `ModifyFlow` 步 ⇒ **新步骤类型存档后仍可解析**（4.1 持久化的运行期实证） |
| 检查 4 / 5 / 6 | PASS（属性 4/4；Attack=30.0 / Armor=5.0 / Health=100.0；自研流程 100.0 → 75.0） |
| 检查 7a / 7b | PASS——链起成功；施法者 `Health 100.0 → 55.0`、实际扣 **45.0** = 链资产配置的 `DamageBase` |
| 即时汇总 | **通过 9 / 失败 0** |
| 执行器 Warning | **恰一条**（`:2601`）：`ModifyFlow[Tcs.Chain.Slice_Chain]: 链上下文无流程收集载荷（EventPayload 为空或类型不符）——本步按完成处理` |
| 全日志 Error / ensure | **0 命中**（`LogTcsDamage: Error` / `LogTcsEffect: Error` / `Ensure condition failed` / `Fatal error`） |

**两处如实标注（MUST NOT 当作已证）**：
1. 该 `ModifyFlow` 步是**追加在链尾**的（Warning 出现在流程完成之后：`:2600` 流程完成 → `:2601` Warning），故"扣血仍 45"**不能**充当"降级路径不干扰流程值"的证据——该结论成立靠的是**代码路径**（无载荷 ⇒ 在 `Submit` 之前返回），不是这次运行。
2. 本次走的是**降级路径**（链上下文无收集事件载荷）；**成功提交路径从未被执行过**（需触发行 ⇒ Task 4）。
