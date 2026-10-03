# EVID-2026-10-04-chains-primitives：链原语批次六项定向人工检查（PIE 实跑）

- **文档 ID**：`EVID-2026-10-04-chains-primitives`
- **类型**：EVID / 证据
- **状态**：VALID
- **权威范围**：提案 `add-chain-primitives-and-target-sorting` 的 Task 6.1–6.5 人工检查实跑证据（PIE）
- **最后更新**：2026-10-04

## 目的与被验对象

验证 R4 Task 3.5 落地后的四条链路行为（四个链原语 + P-A 排序相位）：

1. **嵌套深度熔断**（`RunSubChain` 自激链 → 固定常量上限 + 父链回卷）；
2. **事件订阅计数配对**（同一 tag 多等待者共用一条订阅）；
3. **运行态释放即解锚**（可达范围 = 世界反初始化）；
4. **排序相位**（去重 / NaN 排除 / 严格字典序 / 稳定键决胜 / 取前 K）；
5. **既有回归**（不配排序项的既有链零重排）。

## 环境与夹具

- 编辑器：`LegendAutoChessEditor`（UE 5.8.3，Development），PIE 单实例；关卡为临时空关卡（夹具自带单位，不依赖关卡内容）。
- 触发器：既有开发命令 `Tcs.Test.Slice.Run [ChainId]`（`Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`）——它造 2 个单位（施法者 + 目标）、注入 `UTcsPieEntityQuery`、按 id 起链。
- 新增检查资产（`/Game/TcsDev/Checks/`，四个 `UTcsEffectChainDef`，已保存）：

| 资产 | 链 id | 步骤 |
|---|---|---|
| `DA_Check_SelfSub` | `EffectChain.Check.SelfSub` | `RunSubChain{自身, bWait=true}` |
| `DA_Check_WaitEvent` | `EffectChain.Check.WaitEvent` | `WaitEvent{TcsEvent.Attribute.ValueChanged, 无超时}` |
| `DA_Check_WaitEvent3` | `EffectChain.Check.WaitEvent3` | 3 × `RunSubChain{Check.WaitEvent, bWait=false}`（放支线 ⇒ 一条命令造 3 个并存等待者） |
| `DA_Check_Sort` / `DA_Check_SortMulti` | `EffectChain.Check.Sort` / `…SortMulti` | `SelectTargets{Selector, SortItems=[距离·升序], MaxCount=1}` → `Damage{45}`（后者选择器 = dev 多候选夹具） |

- 检查用 tag 已声明进宿主项目 `Config/DefaultGameplayTags.ini`（`EffectChain.Check.*` × 5，带 DevComment）。
- 日志级别：`Log LogTcsEffect Verbose`（订阅/登记/退订三条是 Verbose 级）。

## 6.1 嵌套深度熔断 —— **通过**

日志：`Saved/Logs/LegendAutoChess-backup-2026.10.03-18.57.43.log`（另 `…-15.36.27.log` 为同结果复跑）

| 观测 | 值 |
|---|---|
| `链 EffectChain.Check.SelfSub 起（步数=1 单次上限=64）` | **17 行**（= 深度实测值） |
| `RunFrom: 链 … 嵌套深度超上限（16）——断链`（Error）+ 同名 ensure 面 | 各 1 |
| `链 EffectChain.Check.SelfSub 完成（共 1 步）` | **16 行** |

**结论**：常量上限 16 在**深度 17** 触发；累计步数 17（每层 1 步）远低于共享预算 64 ⇒ 证实"预算由链定义给出、深度护栏先撞"的设计预期；**17 起 − 1 熔断 = 16 完成** ⇒ 被熔断的子链**照样唤醒父链**并逐层走完（"异常结束也唤醒父、不留死链"的实证）。

> 注：熔断面按规格带 `ensure`，PIE 里会弹断言框——**选 Continue**；选 Abort 会连带退出编辑器（本次实测踩中一次）。

## 6.2 事件订阅计数配对 —— **通过**

日志：`Saved/Logs/LegendAutoChess.log:2614-2678`

命令 `Tcs.Test.Slice.Run EffectChain.Check.WaitEvent3`（一条命令造 3 个等待者）后：

| 观测 | 行数 |
|---|---|
| `链事件等待：订阅（事件=TcsEvent.Attribute.ValueChanged）` | **1** |
| `链事件等待：登记等待者（… 等待者数=1 / 2 / 3）` | 3 |
| `WaitEvent[EffectChain.Check.WaitEvent]: PC=0 挂起等事件 …（无超时）` | 3 |

命令 `Tcs.Test.Slice.Run`（扣血 → 属性变更 → 发事件）后：

| 观测 | 行数 |
|---|---|
| `链 EffectChain.Check.WaitEvent 事件命中唤醒（事件=…）` | 3 |
| `WaitEvent[…]: 事件命中唤醒，PC=0 本步完成` | 3 |
| `链事件等待：摘除等待者（… 剩余=2 / 1 / 0）` | 3 |
| `链事件等待：退订（… 等待者归零）` | **1** |

**结论**：同一 tag 的 N 个等待者**只产生一条总线订阅**（订阅数不随等待者数增长）；唤醒走"快照 → 逐个代际校验 → **先摘登记** → 写载荷 → `ResumeRun`"，三条路径（命中/超时/释放）在此收敛为同一个 `RemoveWaiter`。

## 6.3 运行态释放即解锚（可达范围）—— **通过**

日志：`Saved/Logs/LegendAutoChess.log:2740-2750`（再造 3 个等待者）→ 停 PIE → `:2804-2810`（新 PIE 重新登记 5 条链 = 确为新会话）→ `:2866` 之后

| 观测 | 结果 |
|---|---|
| 停 PIE 期间的红字（Error / ensure / 断言） | **0** |
| 新 PIE 中再次发同一事件（`Tcs.Test.Slice.Run`）后的 `事件命中唤醒` | **0 行**（跨 PIE 零残留） |

**结论**：世界反初始化走 `FTcsChainEventWaitRegistry::Reset` 的**全量退订**（静默，不打逐 tag 的 `退订` 行）+ 清表，无残留订阅、无幽灵唤醒。

> **可达范围（重要）**：今天**没有公开的"取消/释放运行态"入口**——`ReleaseRun` 的五条调用路径都要求运行态**正在跑**，而挂起的链不跑，故"挂起中被释放"唯一可走世界反初始化。`ReleaseRun → DropEventWait` 与 `ParentRun` 通知两条分支的真实覆盖**待 M5 打断/取消轮**（台账 `CHAIN-6`）。

## 6.4 排序相位 —— **通过**（修复一处夹具缺口后）

夹具缺口（首次运行暴露，日志 `:2519`/`:2567`）：

```
SelectTargets[EffectChain.Check.SortMulti]: 候选 3 → 通过 3 → 去重 1 → 排序项 1 → 取前 0 → 目标集 0
```
成因 = 装置**先造单位、后注入实体查询**，而组件的登记发生在 `BeginPlay`（`TcsCombatEntityComponent.cpp:UTcsCombatEntityComponent::BeginPlay` 的 `RegisterEntity`）⇒ 登记落空 ⇒ 查询里空 ⇒ 距离评分对**每个**候选（含施法者）都取不到定位 ⇒ 全 NaN 排除。**框架行为符合规格**（NaN ⇒ 排除，不是排末尾）；坏的是夹具顺序。已修为"先注入查询、后造单位"（Live Coding 热补，`LegendAutoChess.log:2939-2949` `Live coding succeeded`）。

修复后两次运行（`Saved/Logs/LegendAutoChess.log:3071` / `:3120`，**逐字一致**）：

```
SelectTargets[EffectChain.Check.SortMulti]: 候选 4 → 通过 4（Filter 数 0）→ 去重 1 → 排序项 1 → 取前 2 → 目标集 1
```

| 判据 | 证据 |
|---|---|
| **资产可配（零 C++）** | `排序项 1` 生效；全部四项都是链资产上的字段（唯一 C++ 是 dev 夹具选择器，住 LAC `TcsDev`） |
| **去重** | `候选 4 → 去重 1`（夹具故意产 1 个重复句柄，最终只留一份） |
| **NaN 排除** | `取前 2`（= 去重后 3 个 − 1 个未注册句柄）；若只是"排到末尾"这里会是 3 |
| **取前 K** | `目标集 1`（`MaxCount=1`） |
| **稳定键决胜** | 两单位同在原点 ⇒ 距离**并列** ⇒ `Id` 升序 ⇒ **施法者**胜出：`3073: [属性变更] 单位 1 的 Attribute.Health：100.000 → 55.000` + `3083: 实际扣 45.0`（单位 1 = 先 spawn 的施法者） |
| **可复现** | 同命令两次，该行**逐字一致** |
| **零红字** | 两次运行及其收尾判定区间无 Error/ensure/断言；`[PASS] 检查 7a` / `[PASS] 检查 7b` |

## 6.5 既有回归 —— **通过**

日志：`Saved/Logs/LegendAutoChess.log:2693` / `:2904`（两次独立 PIE 各一次）

```
SelectTargets[EffectChain.SliceChain]: 候选 1 → 通过 1（Filter 数 0）→ 去重 0 → 排序项 0 → 取前 1 → 目标集 1
链 EffectChain.SliceChain 起（步数=3）→ 步 0 挂起（PC 停驻）→ 唤醒（PC=0）→ 完成（共 3 步）
```
**结论**：不配 `SortItems` 的既有链 `排序项 0`、零去重、零重排，且 `WaitDelay` 挂起-恢复路径照常 ⇒ "保序"措辞的唯一例外条件（配了排序项）在实测中成立。

资产侧静态取证：`Content/TcsDev/DA_SliceChain.uasset` / `DA_FormulaChain.uasset` 在两仓 `git status` 下**零改动**。

## 6.6 降级路径（无时钟 / 无总线 / 未注入查询）—— **未跑**

三条降级路径都要**刻意撤掉设施**，而现装置（`TcsDevSliceRig`）每次都注入查询与总线、且固定在有游戏世界的 PIE 下：

- **未注入查询**：可安排（需要一个"跳过注入"的装置开关，尚未提供）；
- **无时钟** / **无总线**：两者都是随游戏世界一并创建的子系统，**在不改代码的前提下无法撤掉**。

⇒ 本项**不在本批验收内**，如实留白（提案 `tasks.md` 的 6.6 保持未勾），归属 = M5 打断/取消轮顺带（届时"取消中设施正在拆"是真实场景）。

## 边界与可复用判据

- 本证据只覆盖**单一 PIE 进程**内的行为；跨进程/联网一致性不在范围内（排序确定性口径已按"同进程"收窄，见 `SPEC-06-targeting` §3）。
- **两条可复用夹具教训**（均为夹具缺陷，非框架缺陷）：
  1. **容器自引用**：`Array.Add(Array.Last())` 会把本容器元素引用喂给"会改本容器"的调用——扩容即悬空，引擎的容器自引用检查直接致命断言。MUST 先取局部值再 `Add`。
  2. **登记顺序**：把"能力注入"（`SetEntityQuery`）排在"被注入方登记"（组件的 `BeginPlay`）之后 ⇒ 登记静默落空，症状表现为"评分/查询全 NaN"这种**远离根因**的现象。
- 日志锚点写法沿用既有约定：`Saved/Logs/<文件>:<行>`；轮转过的会话在 `…-backup-<时间戳>.log` 里（**按 mtime 找，不按文件名猜**）。
