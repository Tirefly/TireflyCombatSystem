# 修改器通道端到端验收：两轮 PIE 与拒绝面行为证据

- **文档 ID**：`EVID-2026-10-04-modifier-channel`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：R4 Task 4（`PLN-R4`）的端到端验收面——`09 §2.3`「伤害修改器唯一通道」在**内容资产路径**下成立、触发行三个生命周期半边（提交生效 / 摘行还原 / 按来源级联摘除）、触发载荷读取器（`Caster ← Attacker`），以及 Task 3 移入的三项收口（`4.3` 端到端破甲 / `4.4` 载荷读取器实证 / `4.5` 降级路径命令化）
- **最后更新**：2026-10-04
- **被测对象**：`Source/TcsEffect` 的 `FTcsTriggerRegistry` + `UTcsTriggerEvaluator` + 门面触发器 API（Task 1/2 交付）；`Source/TcsDamage` 的 `FTcsStepModifyFlow` + 收集事件载荷读取器（Task 3 交付）；`Source/TcsIntegration` 的 `UTcsEffectTriggerDefAsset` + `UTcsDefinitionSubsystem` 触发装配（Task 2.5 交付）
- **验证范围**：TCS 插件 + LAC 宿主装置与内容资产。**插件源码本轮零改动**（改动面全在 LAC：`Source/TcsDev/**` + `Config/DefaultGameplayTags.ini` + `Content/TcsDev/Checks/**`）
- **完整来源**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`
  - 取证快照：**435380 字节 / 3203 行**；行号锚点均以该快照为准
  - **SHA-256 `e8ef1dadd854aef6cbf21a59471b3627f23f3dfa63197f6150e81f1a67028515`**
  - **哈希口径**：取证时编辑器**仍在运行**，但该日志自 `2026.10.03-22.30.52`（UTC）后再未增长（文件最后写入 = 本地 `06:30:52`）⇒ 哈希与下列行号锚点属**同一快照**。**日志时间戳为 UTC，本地时间 = UTC + 8 小时**。

## 0. 本轮的夹具、装置与前置（先立事实，再看判据）

**三件内容资产**（经 UE MCP 建，`DataAssetTools.create` + `ObjectTools.set_properties`，逐字段回读核对后 `save_assets`；`is_dirty` 均为 false）：

| 资产 | 类 | 关键字段（回读值） |
|---|---|---|
| `Content/TcsDev/Checks/DA_Check_ArmorBreakChain` | `UTcsEffectChainDef` | `ChainId = EffectChain.Check.ArmorBreak`；1 步 `TcsStepModifyFlow{ targetKey = DamageFlowKey.BaseDamage, op = TAO_Mul, operand = Literal(0.5) }` |
| `Content/TcsDev/Checks/DA_Check_PayloadCasterChain` | `UTcsEffectChainDef` | `ChainId = EffectChain.Check.PayloadCaster`；`SelectTargets(SelSelf)` → `Damage{ flowTemplateId = DamageFlowTemplate.Default, damageBase = Literal(7), targetAttrKey = Attribute.Health }` |
| `Content/TcsDev/Checks/DA_Check_ArmorBreakTrigger` | `UTcsEffectTriggerDefAsset` | `TriggerTag = EffectTriggerDef.Check.ArmorBreak`；`Def{ eventTag = TcsEvent.Damage.PreExecute, effectChainId = EffectChain.Check.ArmorBreak, gateTags = [EffectTriggerDef.Check.ArmorBreakGate] }` |

**装置侧**（LAC `Source/TcsDev/**`）：新流程模板 `DamageFlowTemplate.Check.ArmorBreakModifier`（六步：`CollectStart → BaseDamage → PreExecute → Execute → AfterDamage → Completed`；两个数值步骤**刻意不带 delegate**——基础值取上下文输入、执行量取裁决候选，避免公式逃生口给判据引入无关变量）；`Tcs.Test.Slice.Run` 追加**检查 8–17**、`Tcs.Test.Slice.Reject` 追加**检查 D**。

**词表可见性**（不只"我写对了"——让注册方列出来）：运行中的编辑器 `GameplayTagsToolset.ListTags` 实测列全五个新词——`EffectChain.Check.{ArmorBreak, PayloadCaster}`、`EffectTriggerDef.Check.{ArmorBreak, ArmorBreakGate}`、`DamageFlowTemplate.Check.ArmorBreakModifier`。

**编译**：`LegendAutoChessEditor Win64 Development` + `LegendAutoChess Win64 Shipping` 双配置 **`Result: Succeeded`、0 error / 0 warning**（Shipping 侧另确认 `#if WITH_EDITOR` 包裹未留下 C4505 未引用函数告警）。

**为什么破甲行配了 `GateTags`**：该资产是常驻内容，行一旦默认点灯，此后**任何含 `PreExecute` 步骤的模板**都会被它静默减半 ⇒ 设计为"**默认暗**"，由装置在检查块内点灯、块末灭灯（`PassesGateTags` 语义 = 列出的门全部点亮才通过，`TcsTriggerEvaluator.cpp:143-161`）。这也顺带给台账 `TRIG-3`（定义库装配行没有摘除入口）一个可用口径：**启停走点灯，不走摘除**。

## 1. 前置：资产路径的装配证据（PIE 起世界时，无命令）

| 行号 | 日志行 | 判定 |
|---:|---|---|
| 2467 | `UTcsDefinitionSubsystem: 发现触发定义资产 2 个` | 触发定义资产扫到 2 个（Task 2.5 的 Seed + 本轮破甲） |
| 2468 | `定义库就绪——链定义 9 条，触发定义 2 条，失败 0 条` | 失败清单为空（作者侧门全过） |
| 2491 | `触发登记：行=0/1 事件=TcsEvent.Attribute.ValueChanged 链=EffectChain.Check.WaitEvent` | Task 2.5 夹具行 |
| 2492 | `触发登记：行=1/1 事件=TcsEvent.Damage.PreExecute 链=EffectChain.Check.ArmorBreak` | **★ 破甲行由定义库装配**——零 C++ 注册即成为世界的触发行 |
| 2493 | `已装配到世界 Untitled_1——链定义 9/9 条，触发行 2/2 条` | 链在前、行在后，装配完整 |

**这一节是本轮相对计划原文的关键升级**：Task 4 的 2026-09-30 承接注记写"`UTcsEffectTriggerDefAsset` 载体未实现 ⇒ 触发行今天只能 C++ 注册"，而 Task 2.5 已把该载体落地 ⇒ 主场景改走**策划真实作者路径**（内容资产 → 定义库装配 → 求值器 → 链）。C++ 注册只保留给"需要句柄与来源锚点"的生命周期半边。

## 2. 第 1 轮：正路（命令 `Tcs.Test.Slice.Run`，行 2557；帧 502）

**既有检查（0–7a，回归面）全 PASS**：2561 / 2562 / 2563 / 2564 / 2565 / 2566 / 2579 / 2590 / 2595；`检查 7b` 见 2815（施法者 `Health 100.0 → 55.0`，实际扣 `45.0` = 链资产配置的 `DamageBase`，2814）。

**新增检查（8–17，本节的核心证据）**：

| 行号 | 检查 | 观测（实际扣 / 黑板折叠值 / 期望） | 判据含义 |
|---:|---|---|---|
| 2609 | 检查 8 | `ResolveTriggerDef=命中；本世界触发行 2 条` | 定义库**发现**该资产（装配成行由检查 10 反证） |
| 2621 | 检查 9 | `30.0 / 30.0 / 30.0` | **对照组**：资产行未点灯 ⇒ 门③不通过 ⇒ 全按原值 |
| 2642 | 检查 10 | `15.0 / 15.0 / 15.0` | **★ 资产路径端到端生效**：定义库装配的行 → 破甲链 → `ModifyFlow` 提交 → `Execute` 读到修正值 |
| 2666 | 检查 11 | `7.5 / 7.5 / 7.5` | **提交生效**（C++ 注册的行）：同链再挂一行 ⇒ 两笔 `Mul 0.5` 叠加 |
| 2688 | 检查 12 | `15.0 / 15.0 / 15.0` | **摘行还原**：`UnregisterTriggerRow` 返回 `true` ⇒ 回到单笔减半（资产行仍在） |
| 2712 | 检查 13 | `7.5 / 7.5 / 7.5` | 重挂生效（级联检查的初态；同时证重挂可重复） |
| 2735 | 检查 14 | `摘掉 1 条；15.0 / 15.0 / 15.0` | **按来源级联摘除**：只摘本来源的行（返回值恰 1），资产行未受牵连 |
| 2771 | 检查 15 | `施法者被扣 7.0` | **★ 载荷读取器（4.4）**：`AfterDamage` 行的 `Caster` 解析出本次流程的 `Attacker`（改造前读到空句柄 ⇒ `FTcsSelSelf` 不产出目标 ⇒ 施法者不掉血） |
| 2794 | 检查 16 | `摘掉载荷行 1 条 ⇒ 施法者扣 0.0；目标 15.0` | **反向对照**：摘除后不再掉血，且资产行仍生效 |
| 2795 | 检查 17 | `IsDataValid == Valid 2/2 个；累计错误 0 条` | **台账 `TRIG-1` 收口**：触发定义资产的作者侧门取到运行期回执（Task 2.5 当时只有"失败清单能报错"的行为证据） |
| 2796 | 即时判定汇总 | `通过 19 / 失败 0` | 既有 9 项 + 新增 10 项 |
| 2815 | 检查 7b | `[PASS]` | 既有延迟判定无回归 |

**零红字核查**：第 1 轮区间（2557–2815）无 `Error` / `ensure` 命中；全日志 `ModifyFlow[` **恰 1 次**命中（且落在拒绝面，见 §4）⇒ **`Run` 里的成功提交路径全程没有走过降级分支**。

## 3. 第 2 轮：可复现（命令 `Tcs.Test.Slice.Run`，行 2825；帧 27）

同一 PIE 进程内重跑，**关键行逐字一致**：

| 检查 | 第 1 轮行号 | 第 2 轮行号 | 观测（两轮同值） |
|---:|---:|---:|---|
| 8 | 2609 | 2878 | `触发行 2 条` |
| 9 | 2621 | 2890 | `30.0 / 30.0 / 30.0` |
| 10 | 2642 | 2911 | `15.0 / 15.0 / 15.0` |
| 11 | 2666 | 2935 | `7.5 / 7.5 / 7.5` |
| 12 | 2688 | 2957 | `15.0 / 15.0 / 15.0` |
| 13 | 2712 | 2981 | `7.5 / 7.5 / 7.5` |
| 14 | 2735 | 3004 | `摘掉 1 条；15.0 / 15.0 / 15.0` |
| 15 | 2771 | 3040 | `施法者被扣 7.0` |
| 16 | 2794 | 3063 | `摘掉载荷行 1 条 ⇒ 施法者 0.0；目标 15.0` |
| 17 | 2795 | 3064 | `Valid 2/2；错误 0` |
| 汇总 | 2796 | 3065 | `通过 19 / 失败 0` |
| 7b | 2815 | 3084 | `[PASS]`（数值行 2814 / 3083 亦逐字一致） |

**顺带的一条机制实证（不在计划判据内，但本装置免费产出）**：两轮之间 C++ 注册的行**反复挂/摘**，日志里的槽位与代际为 `行=2/1 → 2/3 → 2/5 → 2/7 → 2/9 → 2/11`（2643 / 2689 / 2736 / 2912 / 2958 / 3005）——**同一槽位 `2` 被复用 6 次、代际逐次递增**，且各次 `UnregisterTriggerRow` / `UnregisterTriggerRowsBySource` 都精准命中当前那一代（检查 12/14/16 全 PASS）⇒ 空闲链表复用 + 代际校验在真实运行中成立（Task 2 错误 3 的处置被行为面确认）。

## 4. 拒绝面（命令 `Tcs.Test.Slice.Reject`，行 3101；帧 86）：`4.5` 命令化

| 行号 | 检查 / 日志 | 判定 |
|---:|---|---|
| 3104 | 检查 A：双真相资产被 `IsDataValid` 拒（1 条错误） | PASS（既有） |
| 3155 | 检查 B：空 `ChainId` 链被拒（登记 API 拒绝空 id） | PASS（既有） |
| 3157 | 检查 C：未登记链 id 触发被拒 | PASS（既有） |
| **3161** | **检查 D：ModifyFlow 无流程载荷时降级（链找到且走完、未挂起）** | **PASS（本轮新增）** |
| 3162 | 拒绝面汇总：`通过 4 / 失败 0` | — |

**预期红黄字恰四条**（该命令是唯一允许出红黄字的常规入口）：

| 行号 | 行 |
|---:|---|
| 3110–3151 | `LogOutputDevice: Error: === Handled ensure: ===` + `Ensure condition failed: Chain.ChainId.IsValid()` + 调用栈（检查 B 故意触发；`FDebug::EnsureFailed` 见 3154） |
| 3156 | `LogTcsEffect: Error: UTcsEffectSubsystem::ExecuteChain: 链 None 未登记——拒绝起链`（检查 C 故意触发） |
| 3159 | `LogTcsDamage: Warning: ModifyFlow[EffectChain.Check.ArmorBreak]: 链上下文无流程收集载荷（EventPayload 为空或类型不符）——本步按完成处理`（**恰一条**） |
| 3164 | `EnsureFailed: Error: Ensure condition failed: Chain.ChainId.IsValid()`（同上 ensure 的汇总行） |

**检查 D 的判据为何可信**：`ExecuteChain` 对**已登记链恒返回有效句柄**（运行态先分配；全即时链在返回前走完并释放——`TcsEffectSubsystem_Run.cpp:74-96`），对未登记链返回**无效**句柄 ⇒ `句柄有效 && !IsRunActive` 精确表述"链找到了 + 进了 + 没挂起（= 走完）"，而"降级后卡在 `Running`"与"链没找到"都会让它变红。**这条判据不看日志文本**，日志只作旁证。

## 5. 边界（MUST NOT 外推）

1. **只覆盖单机、单世界、单 PIE 进程**；跨世界共享与网络姿态（`ExecutionGate` 非默认值）未覆盖。
2. **修改器的成功提交路径只经 `Op=Mul` 落在 `DamageFlowKey.BaseDamage` 键**上验过；`Execute` 的**候选裁决**通道（`ExecuteCandidates` + `SortKey` 选一 + 成功才消费）**未被本装置覆盖**——本轮修改器直接改 `BaseDamage` 键，不走候选通道（`DAMAGE-4` 的消费动作仍未落地）。
3. **生命周期半边覆盖的是装置注册的行**（`Source` = 装置直写的高位 Id）；**定义库装配的行没有摘除/重装入口**（台账 `TRIG-3`，本轮给它的是"点灯启停"口径）。
4. **装置用高位 Id 规避了 `FTcsSourceHandle` 跨注册表重号的缺陷**（`FTcsSourceHandleRegistry::NextId` 是每实例计数器，而插件内有定义库与伤害门面两个注册表 ⇒ 各自的头一个 Id 都是 1）——**该缺陷本身未修**，见台账 **`WAIT-6`**（2026-09-20 已入册，本轮把它的触发条件从"M6 多来源并存"提前到"来源级联一进入实用面"，并补上真实碰撞场景）。装置侧的规避**不是机制保证**。
5. **`IsDataValid == Valid` 只覆盖 `UTcsEffectTriggerDefAsset`**；链资产缺"提升为 `Valid`"段（台账 `TOOLS-2` ③）不在本轮，故链资产**未**纳入该判据。
6. **`4.4` 的判据是行为观测**（施法者掉 7）而非"读取器返回的句柄值"直接断言——载荷读取器没有公共读口，这是本轮能做到的最强观测。
7. **GC 面未覆盖**：本装置未构造"行内 `FInstancedStruct` 内层持对象引用"的夹具，`FTcsTriggerRegistry::AddReferencedObjects` 的存活语义仍无行为证据（与 `WAIT-8` 同款留白）。
8. **一条命令的失败不是本轮的缺陷**：本次实测前有两条 `Tcs.Test.Integration.Reject` 被提交（3094/3099）并返回 `Command not recognized`——那**不是**本项目注册的命令（注册的只有 `Tcs.Test.Slice.Run` / `.Reject` / `Tcs.Test.Gc.Arm*`），与代码无关，保留在案以说明取证过程。

## 6. 结论

- **`09 §2.3` 伤害修改器唯一通道端到端成立**：内容资产（触发定义 + 单步链）→ 定义库装配 → 事件命中 → `ModifyFlow` 提交黑板 → `Execute` 读到修正值 → 扣血生效；两个观测面（伤害记录 `Final` 折叠值 / 生命实际扣量）同时落在期望值上。
- **触发行三个生命周期半边全部成立**：提交生效 / 摘行还原（按句柄）/ 按来源级联摘除，且级联**不误伤**同 Tag 的他家来源与另一个 `EventTag` 的行。
- **Task 3 移入的三项收口**：`4.3` 端到端破甲 ✓、`4.4` 载荷读取器 ✓（`Caster ← Attacker` 首次取到行为证据）、`4.5` 降级路径命令化 ✓（住 `.Reject`，常规命令保持零红字）。
- **台账 `TRIG-1` 可消费**（`IsDataValid == Valid` 回执已取到）。
- **本轮零新增台账条目**：装置实现期发现的"来源句柄跨注册表重号"= 早前已入册的 **`WAIT-6`**（不重复登记，只补触发条件与证据）；本文件 §5 的其余边界逐条挂既有条目（§5.7 GC 面 = `WAIT-8` 族留白、§5.2 候选裁决 = `DAMAGE-4`、§5.5 链资产校验提升段 = `TOOLS-2` ③、§5.3 定义库行的摘除入口 = `TRIG-3`）。
