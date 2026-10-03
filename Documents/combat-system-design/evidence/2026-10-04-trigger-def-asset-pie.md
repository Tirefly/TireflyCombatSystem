# EVID-2026-10-04-trigger-def-asset：触发定义资产载体四轮定向人工检查（PIE 实跑）

- **文档 ID**：`EVID-2026-10-04-trigger-def-asset`
- **类型**：EVID / 证据
- **状态**：VALID
- **权威范围**：提案 `add-effect-trigger-def-asset`（TCS）+ 提案 `add-effect-trigger-def-tag-root`（LAC）的人工检查实跑证据（PIE）
- **最后更新**：2026-10-04

## 目的与被验对象

验证 R4 Task 2.5 落地后的**触发定义资产载体**链路，五条：

1. **发现**——`UTcsEffectTriggerDefAsset` 被定义库按类扫到并缓存；
2. **装配**——缓存定义在世界装配期被登记成该世界的**触发行**（`Source` = 定义库来源句柄）；
3. **规则真能生效**——内容资产定义的规则命中事件 → 起链 → 挂起 → 唤醒 → 完成；
4. **失败面**——配置不完整的资产在**发现期**被拦下（失败清单 + 不进装配），且**不误伤其它资产**；
5. **引用链预检**——引用未登记链时**留 Warning 但仍旧登记**（与失败面形成关键对照）。

## 环境与夹具

- 编辑器：`LegendAutoChessEditor`（UE 5.8.3，Development）；PIE 单实例、临时空关卡（夹具自带单位，不依赖关卡内容）。
- 唯一日志：`Saved/Logs/LegendAutoChess.log`——**同一编辑器进程内连续四轮 PIE**，故行号天然构成轮次边界。
  - **快照口径（取证时刻）**：**422,625 字节 / 3,193 行**，mtime `2026-10-04 05:48:42`，**SHA-256 `b97bcda16af0d7f38bd14714cc8d1ccb07472978bcffdb0fe59f3aaac14f7815`**。⚠ 取证时编辑器**仍在运行**（日志仍可被追加）⇒ 该哈希是"取证时刻"的值，若要严格可比对应在**关闭编辑器后**重新取哈希（同 `EVID-2026-09-29-registry-lifetime` 的哈希口径说明）。**下述行号锚点全部取自该快照。**
  > **时间戳口径**：引擎日志时间戳为 **UTC**（比本机时间早 8 小时）——本机 `05:2x` 对应日志里的 `21:2x`。核对锚点请用**行号**，不要用时间戳推断轮次。
- 夹具资产（LAC 侧，`/Game/TcsDev/Checks/DA_Check_TriggerRule.uasset`，已入库）：

| 字段 | 值 |
|---|---|
| `TriggerTag` | `EffectTriggerDef.Check.Seed`（宿主 ini 声明——提案 `add-effect-trigger-def-tag-root`） |
| `Def.EventTag` | `TcsEvent.Attribute.ValueChanged`（扣血即发 ⇒ Run 命令天然命中） |
| `Def.EffectChainId` | `EffectChain.Check.WaitEvent`（单步 `WaitEvent{TcsEvent.Attribute.ValueChanged, 无超时}`——**不产生新伤害**，故不会自激成环） |
| 其余 | `Priority=0` / `TEG_Always` / 条件空 / `bConditionMissIsSilent=true` |

- 操作方式：**仅 Run 1/4** 需要在 PIE 内敲既有命令 `Tcs.Test.Slice.Run`；Run 2/3 只需开停 PIE（发现与装配都发生在世界起时）。

## Run 1：正路 + 幂等 + 回归 —— **通过**

命令：`Tcs.Test.Slice.Run`（PIE 内）。

| 判据 | 日志锚点（`LegendAutoChess.log`） |
|---|---|
| 发现路径 | `L2451 发现触发定义资产 1 个` |
| 就绪 + 失败清单空 | `L2452 定义库就绪——链定义 7 条，触发定义 1 条，失败 0 条` |
| 装配为触发行 | `L2473 已装配到世界 Untitled_1——链定义 7/7 条，触发行 1/1 条` |
| 行的身份与来源 | `L2472 触发登记：行=0/1 事件=TcsEvent.Attribute.ValueChanged 链=EffectChain.Check.WaitEvent 优先级=0` |
| **规则生效（命中）** | `L2561 链 EffectChain.Check.WaitEvent 起（步数=1 单次上限=64）` → `L2564 步 0 挂起（PC 停驻）` |
| **规则生效（唤醒）** | `L2579 事件命中唤醒（事件=TcsEvent.Attribute.ValueChanged）` → `L2580 唤醒（PC=0）` → `L2581 完成（共 1 步）` |
| 再次命中（行仍有效） | `L2582 起` → `L2584 步 0 挂起` |
| 幂等 | `触发行 \d+/\d+ 条` 全文件 **1 次**；`触发登记：` 全文件 **1 次** |
| 回归 | `[PASS] 检查 0` ~ `[PASS] 检查 7b`，`即时判定汇总：通过 9 / 失败 0` |
| 零红字 | PIE 区间内 TCS 侧 Error/Warning/ensure **零命中** |

**结论**：内容资产定义的"系统级规则"端到端成立——**零 C++**（资产 + ini 词即可），且这一跑把**挂起与唤醒两条路**都走通了（命中起链 → 挂起 → 下一事件唤醒 → 完成），超出原计划的"只验命中"。

## Run 2：失败面（`TriggerTag` 置空） —— **通过**

夹具态：`TriggerTag` 置空（`tagName=None`），其余不变。用户在本轮**仍敲了** `Tcs.Test.Slice.Run`——这反而补上一条判据（见末行）。

| 判据 | 日志锚点 |
|---|---|
| 资产仍被发现 | `L2668 发现触发定义资产 1 个` |
| 发现期拦下 | `L2669 定义库就绪——链定义 7 条，触发定义 0 条，失败 1 条` |
| 失败清单内容 | `L2670 Error: 登记失败——/Game/TcsDev/Checks/DA_Check_TriggerRule.DA_Check_TriggerRule: TriggerTag 为空`（**恰 1 条 Error**） |
| 该行**未**装配 | `L2690 已装配到世界 Untitled_1——链定义 7/7 条，触发行 0/0 条` |
| 无 ensure | `ensure` 全文件仅命中 rig 自陈行；`触发登记：` 仍为 1 次（未新增行） |
| **不误伤其余** | `通过 8 / 失败 1`——唯一红项 = `[FAIL] 检查 0`（失败清单非空，**预期**），`检查 1a`–`7b` 全绿 |

**结论**：四条校验在**发现期**生效，`RegisterTriggerRow` 的 ensure 拒绝面**未被走到**（口径与计划一致：不让编辑器刷 ensure 红字）。

## Run 3：引用链预检（`EffectChainId` 指向未登记链） —— **通过**

夹具态：`TriggerTag` 恢复、`Def.EffectChainId` = `DamageFlowTemplate.Default`（**格式合法但未登记为链**的 tag）。

| 判据 | 日志锚点 |
|---|---|
| 仍在发现期通过 | `L2884 定义库就绪——链定义 7 条，触发定义 1 条，失败 0 条` |
| 恰 1 条 Warning（含两者） | `L2904 Warning: 触发定义 EffectTriggerDef.Check.Seed 引用的链 DamageFlowTemplate.Default 在该世界未登记——仍登记该行（宿主可在运行期登记该链）` |
| **仍登记该行** | `L2906 已装配到世界 Untitled_1——链定义 7/7 条，触发行 1/1 条` |
| 无 Error / 无 ensure | 窗口内零命中 |

**结论**：D-14 的"Warning + 仍登记"落地成立；与 Run 2 的 `触发行 0/0` 构成对照——**同一张表里"内容缺口"与"契约违规"两种处置被清晰区分**。

## Run 4：可复现 + 跨 PIE 零残留 —— **通过**

夹具恢复到 Run 1 原态后原样重跑（同一命令）。

| 判据 | Run 1 | Run 4 | 比对 |
|---|---|---|---|
| 就绪行 | `L2452 链定义 7 条，触发定义 1 条，失败 0 条` | `L3037` 同文本 | **逐字一致** |
| 装配行 | `L2473 链定义 7/7 条，触发行 1/1 条` | `L3058` 同文本 | **逐字一致** |
| 登记行（含槽位下标） | `L2472 触发登记：行=0/1 …链=EffectChain.Check.WaitEvent…` | `L3057` 同文本 | **逐字一致**（`行=0` ⇒ 槽位不累积） |
| 生命周期 | `L2561/L2564/L2579/L2581/L2582` | `L3141/L3143/L3158/L3160/L3161` | 同构（起 → 挂起 → 唤醒 → 完成 → 再起 → 挂起） |
| 回归 | `通过 9 / 失败 0` | `通过 9 / 失败 0` | 一致 |

**结论**：①同输入两轮结果一致；②**跨 PIE 零残留**——前四轮 PIE 未往本轮累加（行数仍 `1/1`、槽位仍 `0`）。

## 边界与留白

- **`IsDataValid == Valid`（`NotValidated` 提升）本批未取到证据**：MCP 的资产工具集无"跑数据校验"入口（只有 create/save/load/delete/…），编辑器侧也没有可读的回执 ⇒ 本项**如实留作人工点**。建议 R4 Task 4 扩装置时用 C++ 直接断言（装置已在 `Tcs.Test.Slice.Reject` 的检查 A 对链资产这么做过，同款手法可复制）。
- **预检 Warning 的误报面**（D-14 已接受）：宿主"先起世界、后 `RegisterChain`"时装配期会报一条 Warning 而规则其实可用——Warning 非阻塞、且规则仍登记。
- **本证据只覆盖单进程内的四轮 PIE**；跨进程/联网一致性不在范围内。
- **一处计划外收获**：Run 2 中用户额外跑了常规命令，于是多拿到一条判据——**被拒的资产不误伤其余资产**（仅"检查 0"因失败清单非空翻红）。
- 日志锚点写法沿用既有约定：`Saved/Logs/<文件>:<行>`。

## 关联

- 提案（TCS）：`openspec/changes/archive/2026-10-04-add-effect-trigger-def-asset/`
- 提案（LAC）：`openspec/changes/archive/2026-10-04-add-effect-trigger-def-tag-root/`
- 计划：`plans/plan-r4-trigger-row.md` Task 2.5 的实施注记
- 台账：`ledger/deferred-inputs-ledger.md`（本轮新增条目）
- 设计：`openspec/changes/archive/2026-10-04-add-effect-trigger-def-asset/design.md`（D-11 ~ D-16）
