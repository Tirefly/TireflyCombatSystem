# TCS 战斗系统设计工作区 · 入口索引

- **文档 ID**：`INDEX`
- **类型**：治理（入口索引）
- **状态**：生效中（2026-09-29 建立，取代原 `README.md` 的"文档清单"职能）
- **权威范围**：本文回答"**该读哪一篇**"。编号含义见 `GLOSSARY.md`；写法纪律见 `docs-convention.md`；逐条拍板流水见 `LOG-DECISIONS`。
- **最后更新**：2026-10-08（**R6 Task 3 已落地、取证并归档**：施法运行态 `FTcsCastRun` 池 + 六道具名门禁 `TryActivate` + 激活期参数快照 + 主链起链 + 施法事件四枚，**同批并入删除**技能侧 `AttrCapture` 整套（判据 = 零消费者 + 机制重叠 + 技能侧已被参数快照占满；**连带不建** `FTcsSkillAttributeAccess`）。**★ 取证方式发生关键转变：由 agent 自主无头完成**（`UnrealEditor-Cmd -game`，此前四轮误判为"必须用户手动跑 PIE"）——`Run` **26 PASS / 0 FAIL** + `Reject` **10 PASS / 0 FAIL**，**零 ensure / 零 Fatal**、`Run` 段零红字、**两轮复现性达标**，且 `C15`（ARO 保活）**带决定性阴性对照**（关 ARO ⇒ 24/2、读数 `存活=否`；开 ⇒ 26/0）。提案 `add-cast-run-and-gates` → 归档 `2026-10-09-add-cast-run-and-gates`（**+5 新增 / ~3 修改 / -0 删除**），**能力数 36 → 37**（新增 `skill-cast-runtime`），`openspec/changes/` **零活动提案**、`validate --all --strict` = **37 passed / 0 failed**。台账 **72 条**（`LEDGER-deferred` **65** + `LEDGER-reflection` **7**；Task 3 首轮取证期新增 `WAIT-12` = 实体查询注入点分散在两处门面，**待用户裁决**）、`Probe` 段位漂移仍挂 `TOOLS-8`（**归后续轮，MUST NOT 在 R6 内修**）；下一步 = **Task 4（参数链带式折叠）**，此前 **L-3 / L-2 / L-6 / L-5 四项悬置决策已全部裁定并写入计划**）

---

## 1. 现状一句话

TCS 战斗系统设计已完成 **M0–M9 全部决策拍板**，实施 **R4 轮已收束**（触发行与伤害修改器通道）、**R5 轮已收束**（M3 状态层 `TcsState`——Task 0~8 全部完成并取证，提案于 2026-10-06 归档），下一步 = **R6 轮（M5 技能层，`TcsSkill`）**（R5 的八个 Task：Task 0 前置契约、Task 1 模块 + Def 资产族、Task 2 实例与生命周期、Task 3 参数快照与时间语义、Task 4 修正器物化、Task 5 五轴堆叠与刷新政策、Task 6 链原语与行为面接线（6a / 6b 两半）、Task 7 端到端验收与首批真内容资产，均已于 2026-10-04 / 2026-10-05 落地；**Task 8 收束**已于 2026-10-05 完成文档面、**2026-10-06 完成归档**）：

- **R1/R2/R3 已完成**（核心 + 属性 + 六模块竖切，7 项人工检查点全通过）；
- **R4 已完成（2026-10-04）**——七个 Task（1 / 2 / 2.5 / 3 / 3.5 / 4 / 5）全部完成：触发行三段（数据形状 → 登记表与四道门求值器 → 独立资产载体 `UTcsEffectTriggerDefAsset` 与定义库装配）+ **伤害修改器唯一通道（D7-6）端到端**（破甲 `30 → 15`）+ 四个链原语（`SetVar` / `Branch` / `RunSubChain` / `WaitEvent`）与 P-A 目标排序相位。**四份证据**：`EVID-2026-10-04-trigger-def-asset` / `EVID-2026-10-04-chains-primitives` / `EVID-2026-10-04-modifier-channel` / `EVID-2026-09-30-modifyflow-acceptance`；`openspec validate --all --strict` = **26 passed / 0 failed**、`openspec/changes/` **零活动提案**。**唯一留白** = Task 3.5 的 `tasks.md` 6.6 三条降级路径（归 M5）；
- **R4.5「脚本通道收口」**（**非正式轮号**，2026-09-29 立 / 2026-09-30 落档）：a 已闭环、b/c 未开工——见 `PLN-R4` 的《R4.5 批次表》；
- **R5 已收束（2026-10-04 开工 / 2026-10-05 收束）**——计划 `PLN-R5`（**含《R5–R8 轮次路线图》与《R5.5 批次表》**），`SPEC-02-states` 已按实施视角收窄（其 §12），六条口径裁决已入 `LOG-DECISIONS`；**Task 1 已落地**（`TcsState` 成第 8 个模块 + 状态 Def 资产族 + 定义库第三条发现路径 + `StateDef` tag 根 9 → 10）；**Task 2 已落地**（状态实例 / per-unit 桶与代际 / 世界门面 / 六枚生命周期事件 + **状态定义改为逐世界登记**）；
  - **Task 0（前置契约）已于同日落地**——`FTcsParamEvaluateContext` 补 `Subject` / `EffectiveLevel`、`FTcsParamEnumerableSource` 基类落地、来源发号器改**进程唯一**（`WAIT-6` 消费；修法含一处必要偏离 = 出线 + 导出宏），双配置编译**零 warning**；**PIE 回归已过**（`Tcs.Test.Slice.Run` 19/0 + 延迟 7b、零红字，含"级联摘除恰 1 条"的行为级实证；证据 `EVID-2026-10-04-param-context-and-source-issuer`）。
  - **Task 2 验收已过**——`Tcs.Test.Slice.Run` 即时 **26/0**（新增检查 19a–19f）+ 延迟 7b PASS、区间零红字；`Tcs.Test.Slice.Reject` **7/7**（新增状态面 E/F/G）；证据 `EVID-2026-10-04-state-instance-lifecycle`。
  - **Task 3 验收已过（2026-10-04）**——数值与时间两条腿落地：**参数快照**（`FTcsParamSnapshot` + 值约定在写入点转规范值 + 纯 C++ 读取适配器）/ **四个等级源 + 宿主实体等级读口**（`ITcsEntityLevelProvider` + 派生上下文 `FTcsStateEvaluateContext`）/ **Duration-Period 到期堆**（重复到期条目 + `PeriodRefresh` 三态 + 时长操作堆同步）/ **R4.5-b 宿主参数源插槽**（`ITcsParamSourceHost` + `FTcsParamSource_HostDelegate`）。读数 = `Tcs.Test.Slice.Run` 即时 **32/0** + 延迟段 **39/0**（新增 `20a`–`20l`）、`Tcs.Test.Slice.Reject` **9/0**（新增 H/I）、双配置编译零 warning 零 error；证据 `EVID-2026-10-04-state-param-snapshot-and-level-sources`。
  - **Task 4 验收已过（2026-10-04）**——**状态第一次真的改得动数值**（D3-19 修正器物化）：`ModifierRows` 模板 → M2 账本条目（`Source` = 实例来源句柄）、模板里的引用类操作数**从该实例自己的快照取值**（经新增的**反射壳 + RAII 栈式绑定**把快照装进 `Ctx.ParamTable`）、施加/刷新**同批内**挂载且排在生命周期广播之前、移除/到期**按同一来源一次摘净**。读数 = `Tcs.Test.Slice.Run` 即时 **40/0** + 延迟段累计 **47/0**（新增 `21a`–`21h`）、`Tcs.Test.Slice.Reject` **10/0**（新增 J）、双配置编译零 warning 零 error；关键读点 = 护甲 `5.000 → -15.000`（快照 `-20`，模板兜底 `-999` **未被取用**）与定序读数"属性变更 **37** < `Applied` **38**"；证据 `EVID-2026-10-04-state-modifier-materialization`。**下一步 = Task 5（五轴堆叠与刷新政策）**。
  - **Task 5 验收已过（2026-10-05）**——**同一 buff 的重复施加首次有确定语义**（五轴堆叠与刷新政策）：组键三态（不分组 / 按来源 / 按发起者）+ Custom 逃逸位、`MaxStacks` 与溢出两档、数值叠加（取最大 / 累加）、时长刷新（不动 / 回满额）；`EApplyResult` **四档全部实测可达**、`TcsEvent.State.StackChanged` **首次有真实广播**（叠层路径，且定序早于 `Refreshed`）；宿主自定义决策 Fragment 被真实调用（样本恒不同组 ⇒ 每次新建）。读数 = `Tcs.Test.Slice.Run` 即时 **53/0** + 延迟累计 **60/0**（新增 `22a`–`22m`）、`Tcs.Test.Slice.Reject` **11/0**（新增 `K`）；证据 `EVID-2026-10-05-state-stacking-policies`（双区段哈希 `0841f12a…` / `dc3917a9…`）。**同轮修掉一处实现缺陷**：`ScheduleTime` 刷新恒回满额 ⇒ `ESD_None`（不动时长）那一档此前**在实现上从未存在**。
  - **Task 6（6a）验收已过（2026-10-05）**——**链里首次能施加状态、能改属性；状态自己的内联触发行首次有真实登记与退订**：`FTcsStepApplyState` / `FTcsStepModifyAttribute`（两个领域步骤 + 自注册，`TcsEffect` 首次真实使用到 `TcsAttribute` 这条**下层**边）+ `AttributeCompare`（第三条内置条件，实测能**门控**起链）+ **级联锚点与施加方身份解耦**（`CascadeAnchor` ≠ `Source`：一个来源可施加多个定义，按来源撤销会互相误摘）+ **链运行态"身份 + 因果边"**（`RunSource` 每运行一枚新号 / `CausedBy` 触发行起链 = 该行所属实例的锚点；**因果边 MUST NOT 参与撤销或共存判定**）。读数 = `Tcs.Test.Slice.Run` 即时 **61/0** + 延迟段收束 **68/0**（新增 `23a`–`23h`）、`Tcs.Test.Slice.Reject` **11/0**；证据 `EVID-2026-10-05-state-chain-primitives`（双区段哈希 `2cfa09fe…` / `7458a999…`）。**首轮唯一失败 23g（60/1）的真因是一条模型级发现**：内联触发行是"**事件 Tag 级**"规则、不过滤载荷与单位 ⇒ 跨实例串扰（入台账 `TRIG-6`）；**同轮还修掉一处跨文件缺陷**（`TcsStepBranch` 漏填 `Context.World` ⇒ 链里的 `AttributeCompare` 会静默恒不通过）。
  - **Task 6（6b）验收已过（2026-10-05）**——**状态首次有了作者面的行为回调，且状态事件第一次能按实例身份自筛**：`FTcsStateBehaviorFragment`（兴趣 Tag 列表 + 单个 `const` 泛化回调）住 `FTcsBuffDef.Fragments`，按**每兴趣 Tag 一条订阅**（共享 Handler 由门面 `UPROPERTY` 强持有、计数配对）接线——施加时接线排在 `Applied` 广播**之前**、移除 / 到期退订排在广播**之前**、刷新**不重挂**；**首个状态载荷读取器** + **两侧泛型主体匹配**（`TRIG-6` 甲案）把触发行从"事件 Tag 级规则"收窄为"**绑定行只认自己的实例主体**"，`TcsEffect` 仍**不反向依赖** `TcsState`；**句柄身份修复**（出线进程唯一正奇数代际，修掉"各桶各自发号 ⇒ 跨单位同号"的缺陷）与 **`STAT-5` 有限重入守卫**（旁表只覆盖真正的移除广播窗口）同轮落地。读数 = **两连 PIE**（同进程 `StopPIE → StartPIE`）各即时 **65/0** + 延迟段收束 **72/0**（新增 `23i`–`23l`）、`Tcs.Test.Slice.Reject` **11/0**；`Run` 红字**恰 3 条**（与 6a 同，**本轮未加故意红字**）；证据 `EVID-2026-10-05-state-behavior-fragments`（冻结快照 `05218d83…` + 三段区段哈希）。**关键读数**：23g **恢复"先主后次"** 后仍 `+25.000 / 计数 1`（即 `TRIG-6` 那条串扰**不再靠夹具顺序规避**）、23l 第二轮 `旧句柄拒绝=是` 且两轮各有 `状态行为反初始化：订阅 1→0 实例 1→0`。**⇒ R5 Task 6 两半全部完成；`openspec/changes/` 零活动提案。**
  - **Task 7 验收已过（2026-10-05）**——**状态层首次由真内容资产走通到底，且"修正器模板"成为定义库第四计数**：新建 4 个内容资产（`DA_ModDef_E2E` 修正器模板 / `DA_State_E2E` buff / `DA_Chain_E2E_Apply` 施加链 / `DA_Chain_E2E_Behavior` 行为链），链路 = 链施加 → 建实例（`DurationTime` 2.00s 有限时值）→ 内联触发行在 `Applied` 上再入起行为链 → **模板经快照 `ParamRef` 跨资产取到覆盖值 25.0** 落到护甲 → 0.50s 周期回调 → 2.60s 到期回收、触发行按锚点退订、**护甲精确复原**，而**行为链改的攻击停在 X−5**（链挂条目按设计常驻）。读数 = `Tcs.Test.State.Run` **两轮各 15/0 且复现性摘要逐字相同**、`Tcs.Test.State.Reject` **7/0**；**回归逐字不变**（`Slice.Run` 即时 **65/0** + 延迟 **72/0**、`Slice.Reject` **11/0**）；定义库就绪行 = `链 11 / 触发 2 / 状态 2 / 修正器模板 1 / 失败 0`（逐项吻合 §5.5 增量核对表）。插件侧新增 `ResolveAttrModDef` 身份解析与索引（`AttrModDef` 根 **10 → 11**，`ATTR-1` 闭合）；证据 `EVID-2026-10-05-state-layer-pie`（冻结快照 `db630575…` + 五段区段哈希）。**同轮留痕十条缺陷/事实（见证据 §4：六处缺陷 + 四条通道事实/教训）**（六处缺陷：内容资产 `ParamRef.Key` 笔误致取兜底 −999、事件面基线晚取致 `Applied` 增量恒 0、检查 S8/S14 残留旧算术 + 陈旧 DLL、摘要口径过宽、证据表格行号整体偏一行、**提交信息里的检查清单按计划文本推断而未读日志（11/15 条标题不符、S10 声称的"定序"读数本轮不存在）**；另四条通道事实/教训：UBT 与 Live Coding 互斥且后者掩盖陈旧 DLL、MCP 无 `IsDataValid` 入口、惰性初始化日志只写一次、Slate ref 不跨进程且日志随重启轮转），**并订正 `tasks.md` §6.3**（"全部读数逐字相同"过宽——句柄 id 进程单调、周期次数帧驱动）。**⚠️ 取证口径**：先关编辑器跑**完整双配置 UBT 构建**（Editor `7.96s` / Shipping `28.16s`，各 `Result: Succeeded`、零 warning 零 error），**再在重建后的二进制上重跑全部验收** ⇒ 证据全部读数对应**交付二进制**，而非 Live Coding 打过补丁的进程。**⇒ Task 7 验收完成；下一步 = Task 8 收束（**已于 2026-10-05 完成文档面、2026-10-06 完成归档**）。**
  - **R6 已开工（2026-10-06）——Task 0（硬前置）已落地并归档**：`R-2` 后段欠下的两张注册表**宿主脚本插槽**补齐（`ITcsTriggerConditionEvaluator` / `ITcsTriggerPayloadReader` 两个 `UINTERFACE(MinimalAPI, Blueprintable)` + 门面四个口含两个 `Unregister` 对；注册值**保留 `TFunction`**、**不造转发器**、**不改内置快路径**）。**PIE 四命令全绿（15 PASS / 0 FAIL）**：`Tcs.Test.Gc.Arm` 5/0 → `obj gc` → `Tcs.Test.Gc.Verify` 7/0（7/7 弱引用存活 + GC 后再广播仍扣血 2）→ `Tcs.Test.Gc.Reject` 3/0（聚合 11 项布尔全真）；**两轮独立 PIE 判定行 17/17 逐字相同**。证据 `EVID-2026-10-06-trigger-host-slots`。**留白一条**：新增 scenario 的"世界 B 判未命中"半（跨世界）**未实测**——本轮两次 PIE 虽先后造了两个世界，但 `Deinitialize` 的显式撤销总先行清干净，该分支仍不触发；已升级记录为 **`WAIT-11`**（用户 2026-10-06 裁定照常归档 + 留痕，同 2026-09-29 先例）。**⇒ 下一步 = Task 1（`TcsSkill` 模块 + Def 资产族），实施暂停待用户逐项复核。**
  - **R6–R8 未收束**（技能层 → 集成层 → 表现与编辑器工具）：Task 1~N 待实施；**冷却与 Cost 已明确改判 R6.5**。

> **最容易误判的一条**：原 `README.md` 里 plan3 的状态写着"待执行 / 5 Task"，**那是过期信息**（README 已于 2026-09-29 拆分为导航页，内容迁入 `log/`）——plan3 的 Task 1/2 已完成、R4 已扩容为 7 个 Task、并已于 2026-10-04 **全轮收束**。以本文 §1 与 `PLN-R4` 为准。



## 2. 我该读哪一篇？（按目的选）

| 你的目的 | 读这个 | 说明 |
|---|---|---|
| **了解系统全貌** | `SPEC-00-core`～`SPEC-10-presentation`（11 篇模块规格） | 一篇一个模块，统一模板：模块边界 / 类型词汇 / 入口服务 / 网络姿态 / 非目标 / 依据 / 验收钩子。**注意编号 ≠ 文件序**：`SPEC-09` 主题词是 `editor`（文件 `spec/08-…`） |
| **只想知道某个模块对外是什么** | 该模块 `SPEC` 的 §1 模块边界 + §2 类型词汇 | — |
| **想知道"当初为什么这么定"** | `GLOSSARY.md` 查到决策编号 → 按 §4 的落点进对应 `LOG-xx` / `DEC-xx` | 决策理由只住决策文档，不住规格 |
| **要知道哪些事还没做** | `LEDGER-deferred`（跨轮遗留台账，65 条） | 唯一待办登记册；按轮次分组 |
| **要知道某能力现在到底通没通** | `SPEC-TRACE`（契约追踪矩阵）+ `EVID-*`（证据） | 矩阵区分"静态实现 / glue / PIE 已验证 / 未验证" |
| **要接着干活（实施）** | `PLN-R5`（**R5 已收束**；含《R5–R8 轮次路线图》+《R5.5 批次表》） | 已完成轮次的计划：`PLN-R3-1`、`PLN-R3-2`、`PLN-R4`（**R4 已完成**，保留《R4.5 批次表》）。**Task 1–5 已落地**（各自提案已归档）；**Task 6 两半（6a / 6b）已于 2026-10-05 全部落地并归档**（即时 65/0 + 延迟 72/0 + `.Reject` 11/0，两连 PIE）；**Task 7（端到端验收 + 首批真内容资产）已于 2026-10-05 落地**（`Tcs.Test.State.Run` 两轮各 15/0、`.Reject` 7/0；回归 65/0 + 72/0 + 11/0 **逐字不变**；`AttrModDef` 根 10 → 11）；**Task 8 收束已完成**：设计文档 / `SPEC-TRACE` / 台账 / 两册日志 / `INDEX` 五面回写 + **提案 `verify-state-layer-e2e` 已归档为 `changes/archive/2026-10-06-verify-state-layer-e2e/`**（**归档日为 10-06 机器日期，非计划预写的 10-05**，已就地留痕）+ 两处 `## Purpose` 手工补正 ⇒ `openspec/changes/` **零活动提案**、`openspec validate --all --strict` = **34 passed / 0 failed**、能力数 33 → **34**；**下一步 = R6（M5 技能层 `TcsSkill`）** |
| **写涉及"反射"字样的文本** | `LEDGER-terminology`（**MUST 先通读**） | 该词在本仓承载 5 种含义，有唯一处方措辞 |
| **要查某个缩写/编号** | `GLOSSARY.md` | 15 个编号族 + 逐条注册表 |
| **要新增或改写文档** | `CONVENTION` | 类型/状态词/身份块/引用格式 |
| **想了解本轮重构做了什么** | 勘察报告：`docs/2026-09-29-tcs-docs-recon.md`（仓库根 `docs/` 目录） | 45 篇文档的结构与关系体检 |

### 2.1 目录结构（找文件用）

```
combat-system-design/
├── INDEX.md            ← 本文：入口（该读哪一篇 + 现状 + 三层真相源）
├── GLOSSARY.md         ← 术语与编号权威表 + 文档 ID → 路径映射
├── docs-convention.md  ← 文档规范（类型/状态词/引用格式/写作纪律）
├── README.md           ← 导航页 + 2026-09-02 历史前言（不再是入口）
├── spec/               ← 模块规格 SPEC-00-core ~ SPEC-10-presentation + SPEC-TRACE（12 篇）
│                          ⚠ 编号与文件序不同轨：SPEC-06-targeting→`10-…`、SPEC-07-notation→`11-…`、
│                            SPEC-08-damage→`09-…`、SPEC-09-editor→`08-…`、SPEC-10-presentation→`07-…`
├── decisions/          ← 决策记录 DEC-*（10 篇：立场书 + 4 篇早期裁决 + PV/折叠/AttributeSet/回调载体）
├── log/                ← 决策日志 LOG-*（8 篇：按模块轮次分片 + 拍板流水 + 实施记录）
├── plans/              ← 实施计划 PLN-*（7 篇：R3 计划一二 + 竖切剧本 + 内容指南 + R4 计划 + R5 计划 + **R6 计划**）
├── research/           ← 调研 RSCH-*（8 篇：AbilityKit 三路 + 复制 + 脚本 + C# + 目标吸收 + 边界审计）
├── evidence/           ← 证据 EVID-*（20 篇 PIE 取证，带 SHA-256 或区段哈希）
├── ledger/             ← 台账与规约 LEDGER-*（3 篇活文档）
└── HISTORICAL/         ← 历史存档（模块地图提案 v1，已被 DEC-00 §9 取代）
```

---

## 3. 三层真相源（谁说了算）

**这是本工作区此前从未写明的关键约定**：

| 层 | 载体 | 管什么 | 冲突时 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/<能力>/spec.md`（**37 条能力规格**） | MUST / Scenario——代码必须满足的行为 | **规格优先**（它进 `openspec validate --strict`） |
| **设计意图** | 本工作区 `SPEC-*` / `DEC-*` / `LOG-*` | 为什么这么设计、边界在哪、否决了什么 | 规格未覆盖处以此为准 |
| **项目约定** | `openspec/project.md` | 命名标准、依赖铁律、测试策略、过网结构纪律 | 全局约定优先 |

- **实现状态**既不在规格也不在规格文档里，只在 `SPEC-TRACE` + `LEDGER-deferred` + 计划注记三处登记。
- **规格目录正在引用本工作区文档的节号**（如 `02 §2.3`、`04 §2.1`）——所以**改节号等于改规格**，见 `CONVENTION` §5。

---

## 4. 文档清单

> 状态词义见 `CONVENTION` §4。**完整路径映射表在 `GLOSSARY.md` §5.1**（本表只列阅读顺序）。

### 4.1 模块规格（`SPEC`，11 + 1 篇）

| ID | 模块 | 编译层模块名 | 状态 | R 范围 |
|---|---|---|---|---|
| `SPEC-00-core` | M0 内核 | `TcsCore` | 生效中 | R3 已交付 |
| `SPEC-01-attributes` | M2 属性/数值 | `TcsAttribute` | 生效中 | R3 已交付 |
| `SPEC-02-states` | M3 状态/Buff | `TcsState` | 落地中 | **R5**（Task 1 已落模块与 Def 资产族，见其 §12.5） |
| `SPEC-03-effects` | M4 效果执行 | `TcsEffect` | 生效中 | R3 已交付；**触发行为已交付（R4，2026-10-04）**——触发行三段 + 四个链原语 + P-A 排序相位；15 原语已落 8（见 §2.1 实现状态） |
| `SPEC-04-skill` | M5 技能/施法 | `TcsSkill` | 待落地 | R6 |
| `SPEC-05-integration` | M6 集成 | `TcsIntegration` | 生效中 | R3 已交付（扩展 R7） |
| `SPEC-06-targeting` | 目标选择 | `TcsTargeting` | 生效中 | R3 已交付 |
| `SPEC-07-notation` | 策划记法 | `TcsNotation` | 待落地 | 骨架壳 R3；内容 R8 |
| `SPEC-08-damage` | 伤害瞬时流程 | `TcsDamage` | 生效中 | R3 已交付；**伤害修改器通道（D7-6）三段全通（R4，2026-10-04）**——提交侧 `FTcsStepModifyFlow` + 触发行订阅 + **端到端实证**（破甲 `30 → 15`，`EVID-2026-10-04-modifier-channel`）；余 `Heal` 原语与候选裁决（`DAMAGE-3` / `DAMAGE-4`，R5） |
| `SPEC-09-editor` | M8 编辑器与工具 | `TcsEditor` | 待落地 | R8 |
| `SPEC-10-presentation` | M7 表现 | `TcsCue` | 待落地 | R8 |
| `SPEC-TRACE` | 契约追踪矩阵 | （跨模块） | 活文档 | — |

### 4.2 决策记录（`DEC`）与决策日志（`LOG`）

| ID | 内容 | 状态 |
|---|---|---|
| `DEC-00-constitution` | R0 立场书：**§8 分离宪法**、**§9 模块物化规定（模块表真相源）** | 生效中 |
| `LOG-DECISIONS` | 逐条拍板流水（历史裁决 / 模块物化 / 拍板记录）——[`log/decisions-log.md`](log/decisions-log.md) | 活文档 |
| `LOG-IMPLEMENTATION` | 逐 Task 实施与验收记录 + 检查点状态——[`log/implementation-log.md`](log/implementation-log.md) | 活文档 |
| `DEC-2026-08-31-state-relation` | 裁决 1：状态关系表 + 状态槽归属 | 冻结 |
| `DEC-2026-08-31-event-bus` | 裁决 2a：事件分发 = 总线 + 订阅表 + 共享 Handler | 冻结 |
| `DEC-2026-08-31-tick-pump` | 裁决 2b：Tick 泵与组件执行边界 | 冻结 |
| `DEC-2026-08-31-visualization` | 裁决 3：可视化编辑范式 | 冻结 |
| `DEC-2026-09-01-abilitykit` | AbilityKit 对照综合 + 原语集/触发行提案 v1 | 冻结（**提案值已被取代**：17→15 原语、12→10 字段） |
| `LOG-00-core` | M0-min + M2 决策点（`D0-*`、`D2-*`、`MD-*`、`NET-*`） | 冻结（已拍板） |
| `LOG-01-states` | M3 决策点（`D3-*`） | 冻结（已拍板） |
| `LOG-02-effects` | M4 决策点（`D4-1`~`D4-5`、`D4-14`~`D4-17`、**`D7-*`**） | 冻结（已拍板） |
| `LOG-03-skill` | M5 决策点（`D5-*`、`PV-10`、**`D4-5`**） | 冻结（已拍板） |
| `LOG-04-integration` | M6 决策点（`D6-*`） | 冻结（已拍板） |
| `DEC-01-pv` | PV 系列：参数值来源策略体系（`PV-0`~`PV-10`） | 冻结（已拍板） |
| `DEC-02-fold-display` | 参数折叠与展示（`D5-5` v3 / `D5-17` v2→v3 / `D5-18` v3 / `PV-1`、`PV-10` 增补） | 冻结（已拍板） |
| `DEC-03-attribute-set` | 属性存在性与 AttributeSet（`D2-14` / `D2-15`） | 冻结（已拍板） |
| `DEC-04-callback-carriers` | **回调载体**：全仓 `TFunction` 用途分类与替换边界（A 类注册值可换 / B 类 `TFunctionRef` 不可换 / C 类内部回调不动、其中 `OnConsumed` 重做 / D 类静态自注册禁止换）+ 各角色生命周期策略（CDO vs 每执行实例）——[`decisions/dec-04-callback-carriers.md`](decisions/dec-04-callback-carriers.md) | **决策已拍板** 2026-09-29；**实现部分落地**（裁定 ⑤ 第一批 = 注册表寿命语义，已归档并实测；裁定 ③ 记账生效；**裁定 ④ 的形状已落地（2026-09-30，Task 3：去闭包 + 改名 + 消费事件 tag 就位，消费动作仍归 `DAMAGE-4`）**；余项待后续提案） |
| `LOG-99-misc` | Mass 连续内存备忘 | 冻结（**未拍板**，未来优化抽屉） |
| `DEC-2026-09-02-module-map` | 模块拆分地图提案 v1 | **历史存档**（被 `DEC-00-constitution` §9 取代） |

### 4.3 实施计划（`PLN`）

| ID | 轮次 | 内容 | 状态 |
|---|---|---|---|
| `PLN-R4` | **R4（已完成）** | 触发行与伤害修改器通道；**含《R4.5 批次表》（非正式轮号）**——R5–R8 路线图已于 2026-10-04 移交 `PLN-R5` | **已完成（2026-10-04）**：Task 1 / 2 / 2.5 / 3 / 3.5 / 4 / 5 全勾；**R4.5 的 b/c 仍未开工**（故本文档保持 `ACTIVE`，继续承载《R4.5 批次表》直到那两批收口） |
| `PLN-R5` | **R5（已完成）** | M3 状态层（`TcsState`）核心竖切：Task 0~8；**含《R5–R8 轮次路线图》（现行排期真相源）+《R5.5 批次表》（非正式轮号）** | **已完成（2026-10-05 收束 / 2026-10-06 归档）**：Task 0~8 **全部收束**（Task 8 = 收束：设计文档回写 / `SPEC-TRACE` 增量 / 台账勾销与对账 / 两册日志 / `INDEX` 同步 / **提案归档** / 本计划回写；检查点卡按门控刷新）；提案归档为 `changes/archive/2026-10-06-verify-state-layer-e2e/`、`openspec validate --all --strict` = **34 passed / 0 failed**、`openspec/changes/` **零活动提案**、能力数 33 → **34**。本文档**保持 `ACTIVE`**——继续承载《R5.5 批次表》（八批未开工）与《R5–R8 轮次路线图》（同 `PLN-R4` 承载《R4.5 批次表》的先例） |
| `PLN-R3-vertical-slice` | R3 | 竖切验收剧本（7 项人工检查单） | 冻结（7/7 已验） |
| `PLN-R3-1` | R3 | 计划一：Core + Attribute（Task 0–6） | 冻结（全部完成） |
| `PLN-R3-2` | R3 | 计划二：Effect + Targeting + Damage + Integration（Task 0–7） | 冻结（全部完成并验收） |
| `PLN-R3-content-guide` | R3 | 竖切内容资产建立指南（人工步骤 + 实测结果） | 冻结（已执行完毕） |

### 4.4 轮次路线图（R4–R8）

| 轮次 | 主题 | 前置 | 台账消费 |
|---|---|---|---|
| **R4**（**已完成 2026-10-04**） | M4a 触发行 + 伤害修改器通道 + 原语补齐 | R3 已收束 | `DAMAGE-1`（部分）/`DAMAGE-2`（部分）/`DAMAGE-3`（部分）/`DAMAGE-4`（边界）；新增 `CHAIN-1`~`CHAIN-6`、`TRIG-1`~`TRIG-5` |
| **R4.5**（**非正式轮号**，**2026-10-06 两处均已落地**） | 脚本通道收口（不占正式轮） | — | 不消费台账条目；权威登记 = `LEDGER-reflection` 的 `R-1` / `R-2`。**重排与落地**：`R-1`（参数源插槽）并入 `PLN-R5` Task 3——**✅ 已落地闭环 2026-10-05**；`R-2` 后段（两张注册表插槽）排到 R6 开工前——**✅ 已落地 2026-10-06（提案 `add-trigger-host-slots`，即 `PLN-R6` Task 0）**，双配置编译 0/0，**运行期往返取证待执行** |
| **R5**（**已完成 2026-10-05**） | M3 状态层（`TcsState`）核心竖切 | R4 已收束 | `STAT-2`（**已部分消费**）、`DAMAGE-5`（**已消费**）、`WAIT-6`（**已消费**）、`TRIG-4`（**已消费**，2026-10-05 Task 6 内联位落地）、`TRIG-5`（**部分消费**——仅 `AttributeCompare` 一支）、`DAMAGE-2`（**部分消费**——仅 `ModifyAttribute` 一支）、`ATTR-1`（**已闭合**，2026-10-05 Task 7）；**新增 `CHAIN-7` / `CHAIN-8`**（Task 7）；`STAT-3` / `DAMAGE-3` 余 `Heal` / `DAMAGE-4` 三项**本轮未触及** ⇒ 已改判 R5.5（见下行）。**前六项**（`STAT-2` / `DAMAGE-5` / `WAIT-6` / `TRIG-4` / `TRIG-5` / `DAMAGE-2`）的勾销时点见 `LEDGER-deferred` 变更记录 |
| **R5.5**（**非正式轮号**） | 原语与消费余额（R5 与 R6 之间，不占正式轮） | R5 核心落地（**已收束 2026-10-05**） | **四项改判入本批（2026-10-05 Task 8）**：`DAMAGE-2` 余 3（`Parallel`/`Repeat`/`OnError` → **R5.5-a**）、`DAMAGE-3` 余 `Heal`（→ **R5.5-b**）、`DAMAGE-4`（→ **R5.5-c**）、`STAT-3`（→ **R5.5-d**）；另有关系表族（→ R5.5-e）与技能参数行分派 / `R-7` / `SCRIPT-9`（f/g/h）；八批明细见 `PLN-R5` 的《R5.5 批次表》 |
| **R6** | M5 技能层（`TcsSkill`） | R5（`FSkillDef` 继承 `FStateDefBase`） | `STAT-1`、`CORE-1`（若成立） |
| **R7** | M6 集成层（两级单位 / StateTree / AttributeSet / 加载层） | R6 | `INTEG-1`~`INTEG-3`、`WAIT-2`/`WAIT-5`（**`WAIT-6` 已于 2026-10-04 随 R5 Task 0 消费，不再列此**——2026-10-05 Task 8 纠错） |
| **R8** | M7 表现 + M8 编辑器与工具 | R7 | `PRES-1`、`TOOLS-1`~`TOOLS-8`、`WAIT-1`/`WAIT-4`/`WAIT-9`/`WAIT-10` |

> 弹性：R6 可能拆两轮；`INTEG-3` 的加载层可拆独立轮；`WAIT-*` 条目在触发条件成立时随当轮消费。**`R4.5` 是非正式轮号**（不占正式轮）——逐批状态见 `PLN-R4` 的《R4.5 批次表》。

### 4.5 台账与追踪（`LEDGER`，活文档）

| ID | 内容 | 规模 |
|---|---|---|
| `LEDGER-deferred` | 跨轮遗留输入台账（**唯一待办登记册**）：按轮次分组的 65 条 + 变更记录 | 65 条 |
| `LEDGER-reflection` | 反射未解决项（`R-1`~`R-7`）+ 逐项专项调研登记册 | 7 项 |
| `LEDGER-terminology` | 「反射」术语规约（一词五义的唯一处方措辞）——**写含"反射"的文本前 MUST 通读** | — |

### 4.6 调研与证据（`RSCH` / `EVID`）

| ID | 内容 | 用途 |
|---|---|---|
| `RSCH-scripting-ustruct` | C# 脚本方案选型（UnrealSharp vs UnrealCSharp），`G-1`~`G-4` 定义处 | 脚本化决策依据 |
| `RSCH-csharp-authoring` | C# 编写 TCS 逻辑的可行性（glue 产物实证） | `SCRIPT-*` 条目来源 |
| `RSCH-replication` | UE 5.8 源码级：产物快照与 Cue 上下文能否过网 | 网络姿态依据 |
| `RSCH-abilitykit-pipeline` | AbilityKit A 路：技能管线与原语 | 原语集提案来源 |
| `RSCH-abilitykit-trigger` | AbilityKit B 路：触发器与属性修饰器 | 触发行提案来源 |
| `RSCH-abilitykit-editor` | AbilityKit C 路：编辑器工具与确定性/回放 | 裁决 3 校准来源 |
| `RSCH-targeting-absorption` | TcsTargeting 演进：AbilityKit 目标查找骨架吸收 + 形状族与指示器切片（**待落地**，含 P-A/P-B/P-C 提案切分） | P-A 排序相位 / P-B 形状族 / P-C 预览的提案输入 |
| `RSCH-boundary-audit` | **框架本体边界审计**：违规清单（五分处置）+ **32 项"合规但极易被误判"不许删名单** + 真缺陷 R-1~R-3 + 第三态「契约默认」的双条件裁定 | 「伤害模块边界整肃」提案的核心判据与**删除禁令** |
| `EVID-2026-09-28-scripting-e2e` | 宿主脚本插槽 E2E + 原生 GC 证据 | `SCRIPT-8` 验收 |
| `EVID-2026-09-28-selector-ref` | selector `ref` 容器回写两轮 PIE 证据 | 选择器签名验收 |
| `EVID-2026-09-29-registry-lifetime` | **两连 PIE：注册表跨世界寿命修复**（同进程二次登记不再被拒；含生效机制诊断） | 提案 `harden-registry-cross-world-lifetime` 核心判据 |
| `EVID-2026-09-30-modifyflow-acceptance` | `ModifyFlow` 提交侧验收：单次 PIE（注册可达 + 降级路径） | 提案 `add-damage-modifyflow-primitive` 验收（已归档） |
| `EVID-2026-10-03-scripting-gc-survival` | **宿主脚本槽位 GC 存活观测**：四槽位（选择器/过滤器/流程执行器/伤害委托）经 `obj gc` 后的死活 + 行为计数判据；**含跑前立的预注册预测**与负对照（**状态 PENDING**，实跑输出待回填） | 提案 `add-scripting-gc-survival-probe` 验收（宿主侧装置，零插件代码改动） |
| `EVID-2026-10-04-chains-primitives` | **R4 链原语批次六项人工检查**：嵌套熔断（17 层起链 / 深度 17 / 16 层父链回卷）、订阅计数配对、解锚可达范围（= 世界反初始化）、排序相位（去重 / NaN 排除 / 稳定键 / 取前 K）、既有回归；**5 过 1 留白**（三条降级路径归 M5） | 提案 `add-chain-primitives-and-target-sorting` 验收（已归档） |
| `EVID-2026-10-04-trigger-def-asset` | **触发行资产载体四轮 PIE**：发现 → 装配为触发行 → 规则生效（命中/挂起/唤醒/完成）、失败面（发现期拦下且不误伤其余）、引用链预检（Warning 但仍登记）、可复现与跨 PIE 零残留 | 提案 `add-effect-trigger-def-asset` 验收（已归档） |
| `EVID-2026-10-04-modifier-channel` | **修改器通道端到端验收**：两轮 `Tcs.Test.Slice.Run` 各 **19/0** 且关键行逐字一致（资产路径生效 `30→15`、C++ 行叠加 `7.5`、摘行还原、按来源级联摘除恰 1 条、载荷读取器 `Caster ← Attacker`、作者侧门 `IsDataValid == Valid 2/2`）+ 一轮 `Tcs.Test.Slice.Reject` **4/0**（降级路径命令化）；含快照 SHA-256 与八条边界 | R4 Task 4 验收（`PLN-R4`）；消费台账 `TRIG-1`、升级 `WAIT-6` |
| `EVID-2026-10-04-param-context-and-source-issuer` | **R5 Task 0 验收（零行为变更回归）**：`Tcs.Test.Slice.Run` 即时 **19/0** + 延迟 7b PASS、区间零红字；**检查 14「按来源级联摘除」摘掉恰 1 条**（装置改走正常发号后不误摘定义库来源 = `WAIT-6` 修复的行为级实证）；含 UHT 反射面静态证据与五条边界（单轮 / 基类零派生源 / 字段零真实读取 / 同族第二实例未验 / 区段哈希口径） | R5 Task 0 Step 4 验收（`PLN-R5`）；消费台账 `WAIT-6`、`DAMAGE-5`、`STAT-2` |
| `EVID-2026-10-04-tcs-state-def-asset` | **R5 Task 1 验收（模块物化 + 定义库第三路径 + 作者侧门）**：`Tcs.Test.Slice.Run` 即时 **20/0**（原 19 项 + 新增**检查 18**）+ 延迟 7b PASS、区间零红字；**检查 18 四项判据全过**（`DefTag` 解析成功 / `StatusTag` 一致 / `IsDataValid == Valid 1/1` / 失败清单 0 条）；含两条旁证（发现独立于装置 / 状态定义不参与世界装配）与五条边界 | R5 Task 1 Step 7 验收（`PLN-R5`）；规格面 = 归档 `2026-10-04-add-tcs-state-module`（四 delta） |
| `EVID-2026-10-04-state-instance-lifecycle` | **R5 Task 2 验收（实例 / per-unit 桶 / 门面 / 生命周期事件）**：`Tcs.Test.Slice.Run` 即时 **26/0**（新增检查 **19a–19f**）+ 延迟 7b PASS、区间零红字；`Tcs.Test.Slice.Reject` **7/7**（新增状态面 **E/F/G**，其中 G = 悬空句柄在槽位复用后**既被拒又不误伤**）；**关键读点 = 订阅者回调行早于广播者的"状态施加"行**（"广播早于释放槽位"的可观测证据）；**定义逐世界登记**由启动期装配行 `状态定义 1/1 条` 独立佐证；含七条边界与一条跨文档口径更新（Task 1 的"不做世界装配"读数两步读法） | R5 Task 2 Step 5 验收（`PLN-R5`）；规格面 = 提案 `add-state-instance-lifecycle`（`state-instance-lifecycle` ADDED ×7 + `integration-entity` MODIFIED） |
| `EVID-2026-10-04-state-param-snapshot-and-level-sources` | **R5 Task 3 验收（参数快照 / 等级源 / Duration-Period 到期堆 / R4.5-b 宿主插槽）**：`Tcs.Test.Slice.Run` 即时 **32/0** + 延迟段 **39/0**（新增 `20a`–`20l`）、`Tcs.Test.Slice.Reject` **9/0**（新增 H/I）；**关键读点 = `20b` 两条等级路取到不同的档**（被施加方 1 级 ⇒ 10.0 / 发起者 2 级 ⇒ 200.0，证明来源没被混成一个）与 **`20l` 窗口内"周期 4→4、到期 1→1"**（到期后周期回调停止的自证采样点）；含**四类装置缺陷的判据沉淀**（探针词静默无效 / 漏施加探针致假通过 / `ForEachState` 未按定义过滤 / 定时器绑 Actor + 采样延时拿世界时钟反算）与九条边界 | R5 Task 3 Step 7 验收（`PLN-R5`）；规格面 = 提案 `add-state-param-snapshot-and-level-sources`（`state-param-snapshot` 新能力 ×6 + `state-instance-lifecycle` ×3 + `state-def-asset` ×1）与 `add-param-source-host-slot`（`param-value` ×2） |
| `EVID-2026-10-04-state-modifier-materialization` | **R5 Task 4 验收（修正器物化 D3-19）**：`Tcs.Test.Slice.Run` 即时 **40/0** + 延迟段累计 **47/0**（新增 `21a`–`21h`）、`Tcs.Test.Slice.Reject` **10/0**（新增 J）；**关键读点 = 引用类操作数从快照取值**（护甲 `5.000 → -15.000` = 基础值 + 快照 `-20`，模板兜底 `-999` **未被取用**）与 **定序读数"属性变更 37 < `Applied` 38"**（计数只能证"都到了"，定序才证"谁先到"）；含**一处装置缺陷留痕**（夹具自造 ensure：真造单位 + 手动注销账本 ⇒ 拆除时二次注销）、**一条验收措辞订正**（"零红字" = 零**非预期**红字；`Run` 固有 3 条预期 Warning）与十条边界 | R5 Task 4 Step 5 验收（`PLN-R5`）；规格面 = 提案 `add-modifier-materialization`（新能力 `state-modifier-materialization` ×7 + `state-param-snapshot` ×1 MODIFIED） |

| `EVID-2026-10-05-state-stacking-policies` | **R5 Task 5 验收（五轴堆叠与刷新政策）**：`Tcs.Test.Slice.Run` 即时 **53/0** + 延迟段累计 **60/0**（新增 `22a`–`22m`）、`Tcs.Test.Slice.Reject` **11/0**（新增 `K`）；四档回执全可达 / `StackChanged` 首次广播且先于 `Refreshed` / `AddValues` 账本读数成倍（5→15→25）/ 满仓替换换句柄 / 时长两档可分（8.000 vs 3.000）/ 宿主决策样本被真实调用；双区段哈希 + 复算脚本 + **十二条边界** | Task 5 验收锚点 |

| `EVID-2026-10-05-state-chain-primitives` | **R5 Task 6（6a）验收（链原语与行为面接线）**：`Tcs.Test.Slice.Run` 即时 **61/0** + 延迟段收束 **68/0**（新增 `23a`–`23h`）、`Tcs.Test.Slice.Reject` **11/0**；**关键读点 = 身份与锚点解耦**（23a `Source.Id=115` / `CascadeAnchor.Id=116` 互异）、**"状态在，行为就在"的对偶**（23b 自己的 `Applied` 起链一次、23c 自己的 `Removed` **不起链**、23d 登记表 `2→4→2`）、**同来源两实例撤销互不牵连**（23g 移除只掉 10.000）、**因果边真的可读**（23h 触发行起链 `CausedBy.Id` = 主探针实例 `CascadeAnchor.Id`、宿主直调为空）；含**首轮唯一失败（23g `60/1`）与其根因**（内联触发行**事件 Tag 级、不过滤载荷** ⇒ 跨实例串扰，已入台账 `TRIG-6`）+ 一处跨文件实现缺陷（`Branch` 漏填 `Context.World` ⇒ 条件静默恒不通过）+ 十一章边界 | Task 6（6a）验收锚点；提案 = `add-state-chain-primitives`（4 新能力 + 4 改能力） |

| `EVID-2026-10-05-state-behavior-fragments` | **R5 Task 6（6b）验收（状态行为 Fragment 与状态载荷自筛）**：**两连 PIE**（同进程 `StopPIE → StartPIE`）各即时 **65/0** + 延迟段收束 **72/0**（新增 `23i`–`23l`）、`Tcs.Test.Slice.Reject` **11/0**；**关键读点 = 行为接线两侧硬约束**（23i 自身 `Applied` 一次 + `subs=4`/`instances=1` + 读取器 `Caster=Instigator/退Unit`、`Subject=Handle`；23k 自己的 `Removed` **不起行为**、外部递归移除 `返回=false`）、**状态载荷按实例自筛与外部扇出的对偶**（23j `刷新A/B=1/0` 与 `空外部载荷=1/1`；`订阅4→4不重挂`）、**`TRIG-6` 甲案在"先主后次"下成立**（23g 恢复原顺序仍 `+25.000 / 计数 1`）、**跨世界零残留与旧句柄拒绝**（23l 第二轮 `旧句柄拒绝=是`、两轮 `状态行为反初始化 订阅 1→0 实例 1→0`）；含冻结快照 + **三段区段哈希** + 复算脚本 + 一处身份缺陷（各桶各自发号 ⇒ 跨单位同号，按裁定 8 甲案修）+ 一处守卫宽度修正（`Expiring` ≠ 正在移除）+ **十二条边界** | Task 6（6b）验收锚点；提案 = `add-state-behavior-fragments`（1 新能力 + 3 改能力）；消费台账 `STAT-5`、落地 `TRIG-6`、新增 `STAT-9` |
| `EVID-2026-10-05-state-layer-pie` | **R5 Task 7 验收（状态层端到端 + 首批真内容资产）**：`Tcs.Test.State.Run`（新命令）**两轮各 15/0 且复现性摘要逐字相同** + `Tcs.Test.State.Reject`（新命令）**7/0**；**回归逐字不变**（`Slice.Run` 即时 **65/0** + 延迟 **72/0**、`Slice.Reject` **11/0**）；定义库就绪行 = `链 11 / 触发 2 / 状态 2 / 修正器模板 1 / 失败 0`（逐项吻合增量核对表）；**关键读点 = 快照三键两两可分**（覆盖 25.000 / 定义 10.000 / 兜底 −999.000 ⇒ 跨资产 `ParamRef` 通道成立且"按 `Row.Key` 命中"而非整体替换；等级键 30.000 同时排除"读 `LevelProvider`"）、**护甲精确复原 vs 攻击停在 X−5**（同一裁定 D10 的两面读数，证明状态修正器挂 `CascadeAnchor`、链挂条目挂 `RunSource`）、检查 18 `IsDataValid == Valid 2/2` 与 19a 门面内 `2 条`（§5.4 预声明的两处分母移动）；含冻结快照 `db630575…` + **五段区段哈希** + 复算脚本 + **十条缺陷/事实留痕**（六处缺陷：资产 `ParamRef.Key` 笔误取兜底 / 事件基线晚取 / 检查 S8/S14 旧算术 + 陈旧 DLL / 摘要口径过宽 / 证据表格行号整体偏一行 / **提交信息检查清单按计划文本推断而未读日志**；四条事实/教训：UBT 与 Live Coding 互斥且后者掩盖陈旧 DLL、MCP 无 `IsDataValid` 入口、惰性初始化日志只写一次、Slate ref 不跨进程且日志随重启轮转）+ **两处判据订正**（§6.3"逐字相同"过宽；区段字节不可作复现性判据）+ **二十条边界** | Task 7 验收锚点；提案 = `verify-state-layer-e2e`（1 新能力 + 3 改能力）；`ATTR-1` 闭合、新增 `CHAIN-7` / `CHAIN-8` |
| `EVID-2026-10-06-trigger-host-slots` | **R6 Task 0 验收（`R-2` 宿主脚本插槽后段）**：宿主用**纯 C#**（无 C++、无蓝图）定义的两个插槽内容，在**真实原生 GC 之后**仍被保活且**仍被抵达**——条件求值器与载荷读取器各被调用，且**条件取值真的门控了链**（绑 3 行 = 2 行条件过 + 1 行不过 ⇒ **只扣 2 份血**）；"每事件一次"语义在载荷读取侧成立；**两轮**（同会话内各自 `StopPIE → StartPIE`）判定行序列 **17/17 逐字相同**、各 **15 PASS / 0 FAIL**；含区段哈希 + 复算脚本 + 如实边界清单 | R6 Task 0 验收锚点；提案 = `add-trigger-host-slots`（`R-2` 后段，`LEDGER-reflection` 的 `R-2` 就此闭环）。**★ 本行与下行是 2026-10-08 补入的**——两篇证据此前**从未入表**（表停更于 `2026-10-05`，而实际证据已增至 21 篇），补齐时同步订正树计数 |
| `EVID-2026-10-07-skill-def-asset` | **R6 Task 1 验收（`TcsSkill` 模块 + 技能 Def 资产族 + 第五条发现路径）**：技能定义资产**真被内容目录发现**——第一次 PIE 会话 `0` 条 ⇒ `Prepare` 落盘 ⇒ 重启 PIE 后 **`1` 条**，**`0 → 1` 的唯一变量是磁盘上多了一个 `.uasset`**（构造性证据，非硬编码）；可**按 `DefTag` 解析**、主资产身份 = `[PrimaryAssetType, DefTag]`、**继承白拿的字段与施法语义逐项保真**、合法资产判 **`Valid`**（非 `NotValidated`）、表行往返保真且 **`RowName` ≠ `DefTag`**；作者期校验对**六个配置错误类别逐一拦下**（含"空时段表合法"的反向对照）。**合计 28 PASS / 0 FAIL**（`Run` 17 + `Reject` 11）；含冻结快照 + 区段哈希 + 复算脚本 + **八条边界** + 一处机制表述订正（ensure 的 **"每进程/每调用点"**，非"每会话"） | R6 Task 1 验收锚点；提案 = `add-tcs-skill-module`（已归档 `2026-10-08-add-tcs-skill-module`）；能力数 34 → **35**（新增 `skill-def-asset`）。**★ 本行为 2026-10-08 补入**（此前从未入表） |
| `EVID-2026-10-08-skill-registry` | **R6 Task 2 验收（已学技能账本 + 定义登记口 + 门禁第一道）**：**三轮取证，判据以第三轮为准** —— 第三轮 `SkillDef.Run` **17/0** + `Registry.Run` **17/0** + `Gate` **4/0** + `Reject` **7/0** = **45 PASS / 0 FAIL**；**关键读点 = 门禁判据的可证伪性**——`UTcsPieEntityQuery` 下 `IsAlive` 与 `IsEntityReady` **必然重合** ⇒ 真实现**分不出**"用了 Ready"与"误用了 Alive"；装置用**发散探针** `UTcsDevSkillGateQuery` 让两问**答案相反**，`G2`（Alive=真/Ready=假 ⇒ **必拒**）与 `G3`（Alive=假/Ready=真 ⇒ **必放**）因此互为反向对照。另：`L2` 用 `CompareScriptStruct` 证明**门面副本与定义库缓存逐字段一致**、`L7` 读到真资产字面量 17、`L10` 等级 `clamp(0, LevelBase + Σ)` 含**负值钳到 0**、`L14` 代际推进且旧句柄被拒。**★ 三轮结构**：第一轮（修复前）核对读数时抓到**两处缺陷**——`R2` **名不副实**（tag 未在 ini 声明 ⇒ 返回空 tag ⇒ 与 `R3` 同义）、拒绝面把"调用方错误"误报成"内容缺口"；修复后第二轮**行为级复验**；第三轮**补第二个验收资产**（`DA_SkillDef_E2E_Second`）⇒ **两条受内容规模限制的残留项清账**：`6.5`「按来源级联恰摘 1 条」**按字面实测**（同单位两条不同 `DefTag` ⇒ 读数 **2 → 1**、剩余 = 另一个 `DefTag`）、`6.4-①` 计数跃迁以 **`1 → 2`** 同构复现（唯一变量 = 磁盘多一个 `.uasset`）。**同批修两处"内容脆性"**（`D3`/`L1` 的 `== 1` → `>= 1`：内容条数不是契约）。**意外收获**：用户跑两次 `Prepare` ⇒ **`EVID-2026-10-07-skill-def-asset` §5 边界 4（幂等路径只走了创建半）就此闭合**。含三轮区段哈希 + 复算脚本 + **十九项如实边界**（含"首会话 3 条 FAIL = 天然阴性对照"与"判据放宽后计数精确性不再被验"） | R6 Task 2 验收锚点；提案 = `add-skill-registry`（`skill-registry` ADDED ×4 + `entity-query-contract` MODIFIED + `integration-entity` MODIFIED） |
| `EVID-2026-10-08-skill-cast-runtime` | **R6 Task 3 验收（施法运行态 + 六道具名门禁 + 事件四枚）**：`Run` **26 PASS / 0 FAIL** + `Reject` **10 PASS / 0 FAIL**（最终基线 `snapshot-R6Task3-run12.log` / `-reject5.log`），**全日志零 `ensure` / 零 `Fatal`**、`Run` 段**零红字**、**两轮复现性达标**（摘要逐字相同 + 26 项结论零差异）。**★ 决定性阴性对照**：把池的 ARO 遍历临时改为 `if(false)` 并重编 ⇒ `Run` **26/0 → 24/2**、`C15` 读数 **`存活=是` → `存活=否`**（差异恰为 `C15` 两条），还原后复跑回 26/0 ⇒ **证明该检查能真检出 ARO 失效、非恒过假阳性**。**★ 本包最重要的副产品 = 取证方式的转变**：由 **agent 自主无头完成**（`UnrealEditor-Cmd -game`），此前**连续四轮误判为"必须用户关编辑器并手动跑 PIE"**；并沉淀三条配套纪律（Live Coding 只阻断编辑器目标 / 无头取证 MUST 重建它所加载的目标 / `Move-Item` 保留 mtime 会让 UBT 跳过重编）。含五份快照 SHA256 + 复算命令 + **八条如实边界**（门禁只 4 道可拒、无自然终结路径、可打断性只验一档、`-game` 下资产发现为 0 等） | R6 Task 3 验收锚点；提案 = `add-cast-run-and-gates`（`skill-cast-runtime` ADDED ×5 + `skill-registry`/`skill-def-asset`/`instance-handle-pool` 各 MODIFIED） |

### 4.7 治理（`GOV`）

| ID | 用途 |
|---|---|
| `INDEX` | 本文——入口 |
| `GLOSSARY` | 术语与编号权威表 + 文档 ID 映射 |
| `CONVENTION` | 文档规范（类型 / 状态 / 身份块 / 引用格式 / 写作纪律） |

---

## 5. 更新纪律（谁该在什么时候改什么）

| 发生了什么 | 必须更新 |
|---|---|
| 新增/删除文档 | 本文 §4 + `GLOSSARY.md` §5.1（ID → 路径） |
| 新增决策编号 | `GLOSSARY.md` 对应族表 + 该决策的 `LOG`/`DEC` 文档 |
| 拍板一项 | `LOG-DECISIONS`（日期 + 决定 + 依据 + 落档点），并回写对应 `SPEC` |
| 一轮开工 | 通读 `LEDGER-deferred` 该轮区段 → 折进该轮计划 |
| 一轮收束 | `LEDGER-deferred` 勾销已消费条目（**不删条目**）+ 计划标完成 + `SPEC-TRACE` 更新验证状态 |
| 移动/改名文档 | `GLOSSARY.md` §5.1 + 全库引用（引 ID 者无需改） |
| 改了 `SPEC` 的节号 | 检查 `openspec/specs/*` 里对本工作区节号的引用（现有 10 处） |