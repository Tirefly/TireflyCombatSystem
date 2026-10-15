# EVID-2026-10-15-refactor-1-task6 — 重构后证据重取 + **两处真实回归的发现与修复**

> **结论一句话**：Task 1~5 的重构（运行态基底化 / `TcsBuff` 拆出 / `TcsAssetButler` 加载层）之后，
> **11 条无头装置读数 10 条与基线逐字相同**，`SkillCast.Run` 的 ARO 阴性对照**可复现**（26/0 → 24/2）；
> 但**两条路线的取证各抓到一处真实回归**——① Entity 词汇归一漏了 LAC 的 C# 夹具（**编辑器起不来**）；
> ② 类换模块后 `GetAssetsByClass` 按**当前类路径**查索引**静默返 0**，导致装置 `Slice.Run` 由 3/3 退为 2/4、
> PIE `State.Run` 由 26/0 退为 6/9。**两处均已修复**：`Slice.Run` 恢复到 **65/0**、定义库由
> 「链 0 / 触发 0 / 状态 0」恢复到「**链 12 / 触发 2 / 状态 2**」。
>
> **★ 本轮最值钱的一条 = 重定向手段的「第四层」**：`CoreRedirects` 的 `ClassRedirects` 只在
> **加载期**把旧路径映射到新类，**不改写 AssetRegistry 索引里记录的类路径**；而
> `GetAssetsByClass(当前类路径)` 查的是**索引** ⇒ 类一换模块就静默查不到。
> 前三层（类型名 / 资产路径 / 类脚本路径）见 `Config/DefaultEngine.ini` 的 `[CoreRedirects]` 注释段；
> **第四层 = 「按类路径的索引查询」**，它**不被任何重定向覆盖**，只能改查询手段。
>
> **证据纪律**（照 `EVID-2026-10-11-skill-layer-e2e` 先例）：以**判据读数**为锚点，
> **MUST NOT** 用日志哈希/字节数当复现性判据（日志每行带墙钟戳）；未覆盖面一律进 §7，
> **不用推断冒充实测**。

- **文档 ID**：`EVID-2026-10-15-refactor-1-task6`
- **类型**：EVID / 验收证据
- **权威范围**：`PLN-REFACTOR-1` **Task 6（证据重取）**——逐篇覆盖 Task 1~5 期间**作废**的读数，
  并如实登记**本轮抓出的两处真实回归及其修复**。**不在本文**：Task 5 的规格面改动
  （见 `plan-refactor-1-runtime-base.md` 的 Task 5 实施记录）、Task 7 的收束面。
- **最后更新**：2026-10-15

---

## 1. 读数（无头 `UnrealEditor-Cmd -game`，两轮）

### 1.1 逐条对照（第一轮 `-r1` 与既有基线）

| 命令 | 本轮读数 | 既有基线 | 结论 |
|---|---|---|---|
| `Tcs.Test.SkillCast.Run` | **26 / 0**（在册 run=8） | 26/0 | 逐字相同 |
| `Tcs.Test.SkillCast.Reject` | **16 / 0**（在册 run=0 条目=6） | 16/0 | 逐字相同 |
| `Tcs.Test.SkillCast.Phase` | **29 / 0** + `PhaseDeferred` **34 / 0** | 29/0 + 34/0 | 相同（见 §1.3 拍数抖动） |
| `Tcs.Test.SkillParamChain.Run` | **26 / 0** | 26/0 | 逐字相同 |
| `Tcs.Test.SkillParamChain.Reject` | **9 / 0** | 9/0 | 逐字相同 |
| `Tcs.Test.SkillLayer.Run` | **24 / 0** | 24/0 | **复现性摘要逐字相同**（§1.2） |
| `Tcs.Test.SkillLayer.Reject` | **7 / 0** | 7/0 | 逐字相同 |
| `Tcs.Test.CooldownCost.Run` | 立即面 **33 / 0** + 尾段 **36 / 0** | 33/0 + 36/0 | 逐字相同 |
| `Tcs.Test.CooldownCost.Reject` | **7 / 0** | 7/0 | 逐字相同 |
| `Tcs.Test.State.Reject` | **6 / 1** → 修复后 **7 / 0** | 6/1 | **★ 修前为回归**（同 §3.2 的下游连锁） |
| `Tcs.Test.State.Run`（PIE-only） | **6 / 9** → 修复后 **15 / 0** | **15/0** | **★ 修前为回归**；★ 计划登记的「26/0」**是错的口径**，见 §3.2 注 |
| `Tcs.Test.Slice.Run` | **2 / 4** → 修复后 **65 / 0** | **3 / 3** | **★ 真实回归**（§3.2，已修） |

> **★ 上表须与 §3.4 的「修复后全量回归」合读**——表内两个 ❌/部分项是**修前**读数；
> 修复后 11 条命令**全部通过**，且另外 9 条**逐字未变**（零回归）。

### 1.1b ★ 修复前后全量对照（11 条命令，本档最值钱的读数表）

| 命令 | 修前 | 修后 | 判读 |
|---|---|---|---|
| `SkillCast.Run` | 26 / 0 | **26 / 0** | 逐字相同 |
| `SkillCast.Reject` | 16 / 0 | **16 / 0** | 逐字相同 |
| `SkillCast.Phase` | 29/0 + 34/0 | **29/0 + 34/0** | 逐字相同 |
| `SkillParamChain.Run` | 26 / 0 | **26 / 0** | 逐字相同 |
| `SkillParamChain.Reject` | 9 / 0 | **9 / 0** | 逐字相同 |
| `SkillLayer.Run` | 24 / 0 | **24 / 0** | 逐字相同（含复现性摘要） |
| `SkillLayer.Reject` | 7 / 0 | **7 / 0** | 逐字相同 |
| `CooldownCost.Run` | 33/0 + 36/0 | **33/0 + 36/0** | 逐字相同 |
| `CooldownCost.Reject` | 7 / 0 | **7 / 0** | 逐字相同 |
| `State.Reject` | 6 / 1 | **7 / 0** | ✅ **修复**（该 1 条 FAIL 是发现层回归的下游） |
| `Slice.Run` | 2 / 4 | **65 / 0** | ✅ **修复** |

**两条判据**：① **修复有效**（两处回归闭合）；② **零附带损伤**（其余 9 条**逐字未变**——
这是"改动只影响发现层、不触碰行为面"的正面证据）。

### 1.2 复现性（本仓纪律：摘要行逐字相同，非原始读数）

```
[复现性摘要]|C1 Entries=1/1 Own=1 Foreign=1 Cmp=1 Int=0|C2 Entries=1/0/0 Restored=1 Int=1 Cmp=0|C3 Entries=1/1/0 OldGone=1 NewLive=1 Int=1 Cmp=0|D1 Ctrl=0/0|
```

**两轮无头 + 一轮 PIE 三方逐字相同**（无头 `-r1` / `-r2` / 编辑器 PIE 03:18 会话）。
该摘要是**唯一**被引作复现性判据的量——照 `MEM-20261006-01`：进程内单调量、进程生命周期痕迹
与帧驱动量**天然不可复现**，MUST NOT 进判据。

### 1.3 一处如实登记的抖动（MUST NOT 读成差异）

`SkillCast.Phase` 的 `PhaseDeferred` 行含 `轮询拍数`：`-r1` 为 **21**、`-r2` 为 **19**
（`PhaseEnterDeferred` 9 / 8）。这是**帧驱动量**（跨帧等待由时钟泵推进，拍数随帧率浮动），
**不属于**判据面——该行被计数的部分（`Passed=34 Failed=0` 与 `完成当刻下标 / 完成后查询 /
PhaseChanged / Completed / Interrupted / 在册 run`）**两轮逐字相同**。

---

## 2. ARO 阴性对照（★ 本轮的必做项，已复现）

**做法**（照 `EVID-2026-10-08-skill-cast-runtime` §2.4 的同一手法）：
把 `UTcsSkillSubsystem::AddReferencedObjects` 里运行态池的 GC 遍历临时改为 `if (false)`，
**重编并核对产物 mtime 确实更新**（10:22:07 vs 当前 10:22:14），再跑同一条命令。

| 条件 | `SkillCast.Run` 摘要 | `C15 ARO 读数` |
|---|---|---|
| ARO **开**（正常） | **26 PASS / 0 FAIL** | `存活=是` |
| ARO **关**（`if(false)`） | **24 PASS / 2 FAIL** | **`存活=否`** |
| 还原后复跑 | **26 PASS / 0 FAIL** | `存活=是` |

**FAIL 明细**（逐字取自日志）：`C15 ARO 保活`、`C15 ARO 可求值` 两条转 FAIL，
`C15 ARO 读数` 由 `存活=是 求值=13.0` 变 `存活=否 求值=13.0`。

**⇒ 该检查不是恒过的假阳性，它能检出 ARO 失效。**
**还原已核**：`git diff` 对 `TcsSkillSubsystem.cpp` 为空、`if (false)` 零残留。

---

## 3. ★★ 两处真实回归（本轮的独立产出）

### 3.1 回归一：Entity 词汇归一漏了 LAC 的 C# 夹具（**编辑器起不来**）

**症状**：编辑器启动时 UnrealSharp 构建失败——

```
TcsDevGcHostSlotsProbe.cs(98,55): error CS1061: "UTcsAttributeSubsystem"未包含"RegisterUnit"的定义
TcsDevGcProbeDriver.cs(108,37): error CS1061: 同上
TcsDevGcProbeDriver.cs(109,63): error CS1061: 同上
```

**根因**：Entity 词汇归一提交（TCS `0ac43b6`）把 C++ 侧 `RegisterUnit` 改名为 `RegisterEntity`
（`TcsAttributeSubsystem.h:95` 现名），但**零文件触及 LAC 的 `Script/`**（实测该提交含 `Script` 的文件数 = 0）。

**判据（本条是 `MEM-20261015-10` 的又一实例）**：**改名族的欠账登记 MUST 按「被改的名字」逐名核，
MUST NOT 按「本 Task 动了哪些文件」核**——引用点落在**别人没动的仓里**。
本轮用逐名核法扫出 **3 处**（全在 `Script/LegendAutoChessCS/TcsDevGcFixtures/`），修复后
`RunUAT BuildUserSolution` 报 `BUILD SUCCESSFUL` / `ExitCode=0`，编辑器随即起得来。

### 3.2 回归二：类换模块后按类路径的索引查询静默返 0（★ 第四层）

**症状**：`Tcs.Test.Slice.Run` 由基线 **3/3** 退为 **2/4**、PIE `Tcs.Test.State.Run` 由 **15/0** 退为 **6/9**。

> **★ 一处基线口径订正（本档实测查出，MUST 读）**：计划 Task 6 的作废清单把 `Tcs.Test.State.Run` 的基线记为 **26/0**——**该数字是错的**。
> 26/0 实为 **`Tcs.Test.Slice.Run`** 的读数（检查 1–19f，见 `EVID-2026-10-04-state-instance-lifecycle` §1 的
> 「即时判定汇总：通过 26 / 失败 0」——那一段通篇讲的是 Slice 装置的检查编号）。
> `State.Run` 的**权威基线 = 15/0**（`EVID-2026-10-05-state-layer-pie` §1：`Tcs.Test.State.Run`（竖切，即时段 + 延时段）| **通过 15 / 失败 0**`，且**两轮各 15/0**）。
> ⇒ **本轮的 15/0 与权威基线逐字一致**（见 §4.3），**MUST NOT** 按错误的 26/0 记为「未达基线」。

| 检查 | 基线 | 本轮（修前） |
|---|---|---|
| `1a` AssetRegistry 扫到链资产 | **12 个** | **0 个** |
| `3` 链步骤类型全可解析 | **28/28 步** | **0/0 步** |
| `S2` 内容身份解析 | 命中 | **「buff 定义 未命中」** |

**根因（实测，两条路线一致）**：搬过家的 Def 类，`GetAssetsByClass` **全部返回 0**；没搬的全部正常——

| Def 类 | 搬迁史 | `GetAssetsByClass` 命中 |
|---|---|---|
| `UTcsSkillDef` | 未搬 | **5** ✅ |
| `UTcsAttrModDef` | 未搬 | **1** ✅ |
| `UTcsEffectChainDef` | Task 8 搬（`TcsIntegration`→`TcsEffect`） | **0** ❌ |
| `UTcsEffectTriggerDefAsset` | Task 8 搬 | **0** ❌ |
| `UTcsBuffDefAsset` | Task 4 搬（`TcsState`→`TcsBuff`） | **0** ❌ |

**机理**：`CoreRedirects` 的 `ClassRedirects` 只在**加载期**把旧路径映射到新类；
**它不改写 AssetRegistry 索引里记录的类路径**。资产文件里存的仍是 `/Script/TcsIntegration.TcsEffectChainDef`
（实测逐字节），而 `GetAssetsByClass` 传的是 `UTcsEffectChainDef::StaticClass()->GetClassPathName()`
（= `/Script/TcsEffect.…`）⇒ **索引里没有匹配项，静默返 0**。

**★ 两条路线都复现**（排除了"无头磁盘缓存陈旧"这一假设）：
无头 `-game` 与编辑器 PIE 的 `检查 1a` **都是 0 个**（PIE 现场见 §4）。

**修复**：新增加载层统一查询口 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`
（`Source/TcsAssetButler/{Public,Private}/Asset/TcsPrimaryAssetQuery.{h,cpp}`）——
改走 `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType)`，**与类路径解耦**，故搬家不影响。
调用面四处：`DiscoverChainDefs` / `DiscoverTriggerDefs` / `DiscoverStateDefs` /
宿主装置 `TcsDev::GatherChainDefAssets`。

**修复后读数**：

| 项 | 修前 | 修后 |
|---|---|---|
| `Tcs.Test.Slice.Run` | 2 / 4 | **65 / 0** ✅ |
| 定义库——链定义 | 0 条 | **12 条** ✅ |
| 定义库——触发定义 | 0 条 | **2 条** ✅ |
| 定义库——状态定义 | 0 条 | **2 条** ✅ |
| `GetAssetsForDefClass` 命中 | — | 链 **12/12**、触发 **2/2**、Buff **2/2** ✅ |
| 对照：`SkillCast.Run` | 26 / 0 | **26 / 0**（未受影响）✅ |

### 3.3 两条回归的**共性**（MUST 读）

**都是「静默失效」，且都不报错**：
- 回归一：C# 编译**报错了**，但报在设计产物上（`CS1061`），**不指向归一的漏扫面**；
- 回归二：**零报错**——`GetAssetsByClass` 返回 false 被既有代码读成"尚无内容"（正常状态），
  `FailureList` **为空**、`定义库就绪且失败清单为空` 照常 **PASS**。

⇒ **"失败清单为空" 不构成 "发现层正常" 的证据**。判据 = 发现层 MUST 有**正数读数**
（"扫到 N 个"）而**不能只有**布尔/空清单。这正是 `1a`（正数读数）比 `检查 0`（空清单）更值钱的原因。

---

## 4. PIE 面现场（编辑器 + `py`/`Cmd` 通道）

**路线**：编辑器实例 + 控制台 `Cmd` 通道（MCP 服务端 `127.0.0.1:8000` 健康，
但 DSH 侧 MCP 客户端重连次数耗尽 ⇒ 本轮 PIE 命令由**用户手工键入**，读数由 agent 读日志核对）。

#### 4.1 修前会话（03:15–03:18）

| 会话 | 命令 | 读数 |
|---|---|---|
| 03:15:30 | `Tcs.Test.Slice.Run` | **2 / 4**（`1a` = 0 个 —— **修前**，且证明 PIE 路线同样复现回归） |
| 03:18:18 | `Tcs.Test.SkillLayer.Run` | **24 / 0**，复现性摘要与无头**逐字相同** |
| 03:18:26 | `Tcs.Test.SkillLayer.Reject` | **7 / 0** |
| 03:18:37 | `Tcs.Test.State.Run` | **6 / 9**（**修前**；`S2`「buff 定义 未命中」= 回归二的状态面表现） |

#### 4.2 ★ 修复后会话（04:12–04:13）

| 时刻 | 命令 | 读数 | 判读 |
|---|---|---|---|
| 04:12:24 | `Tcs.Test.Slice.Run` | **65 / 0** | ✅ **回归闭合**（修前 2/4） |
| 04:12:36 | `Tcs.Test.State.Run` | **14 / 1** | ✅ **实质判据全过**（见下） |
| 04:13:01 | `Tcs.Test.SkillLayer.Run` | **24 / 0**，摘要**逐字相同** | ✅ 与无头一致 |
| 04:13:09 | `Tcs.Test.SkillLayer.Reject` | **7 / 0** | ✅ |
| 04:13:30 | `Tcs.Test.Slice.Run`（二次） | **夹具失败：门面/定义库/装配器不可得** | ⚠️ 见下，**非产品问题** |

**`State.Run` 14/1 的唯一 FAIL = 夹具前提，非产品缺陷**：
唯一 FAIL 是 `S1 跨 PIE 无残留`（本世界总状态实例 **1** 期望 0、效果门面触发行 **2**）——
该判据的语义是"**本轮 PIE 世界应当是干净的**"（命令源码 `TcsDevSliceRig_State.cpp:173` 注释：
"进命令时总实例数必须为 0"）。而本会话里**先跑过 `Slice.Run`**（04:12:24，它建了状态实例），
**同一 PIE 世界被复用** ⇒ `S1` 前提不成立。**修前是 `S2`–`S12` 共 9 条连锁 FAIL，修后只剩这条前提性 FAIL。**

#### 4.3 ★ 干净读数（05:19–05:20，新开 PIE 后单独跑）

**做法**：按 §4.2 的留痕要求 —— **Stop PIE → 重新 Start PIE → 只跑该命令**（不掺别的命令，
以满足 `S1` 的世界零残留前提）。**连跑三轮**取复现性。

| 时刻 | 读数 |
|---|---|
| 05:19:25 | **15 / 0** |
| 05:19:51 | **15 / 0** |
| 05:20:04 | **15 / 0** |

**★ 三轮复现性摘要逐字相同**：

```
[复现性摘要]|S4 Armor0=5.000 Attack0=30.000 Slots0=0|S5 States=0/1 Decoupled=1 Lv=1|S6 Rows=2->3|S7 N=3 Mod=25.000 Ovr=10.000 Lvl=30.000|S8 Armor=30.000 d=25.000 Slots=1|S9 Attack=25.000 d=-5.000|S11 Live=1 PerOK=1 App=1|S12 Exp=1 Rem=0 Rows=2 Armor=5.000 Slots=0 Attack=25.000|
```

**`S1`–`S15` 全部 PASS**（含 `S1 跨 PIE 无残留` —— 独立 PIE 世界下前提成立，
读数 `本世界总状态实例 0 期望 0`）。

**⇒ 回归二的状态面闭合**：修前 **6/9** → 修后 **15/0**，且**与权威基线 15/0 逐字一致**。

**实质判据逐条转 PASS（修前全 FAIL）**：

| 检查 | 修前 | 修后 |
|---|---|---|
| `S2` 内容身份解析 | 「buff 定义 **未命中**」 | **命中** ✅ |
| `S5` 内容链施加建实例 | 在册 0→0 | **0→1**（`Source.Id=165 / CascadeAnchor.Id=166` 互异、`Level=1`）✅ |
| `S7` 快照三键 | 条目 0、三键全 miss | **条目 3**，命中 **25.000 / 10.000 / 30.000** ✅ |
| `S8` 真资产修正器物化 | 净变 0.000 | **Armor 5.000→30.000 净变 25.000** ✅ |
| `S9` 内联触发行起了行为链 | 变 0.000 | **Attack 30.000→25.000 变 -5.000** ✅ |
| `S11` 在飞状态与周期回调 | 周期回调 0 次 | **累计 1 次** ✅ |
| `S12` 到期回收 | `Expired` 0 | **`Expired=1` / `Removed=0`**（到期路径）✅ |
| `S15` 行为链条目按设计常驻 | 30.000→30.000 | **30.000→25.000**（含链减量）✅ |

**`State.Run` 的 26/0 干净读数未取**（需**新开一次 PIE** 后单独跑，因 `S1` 要求世界零残留）——
**如实登记为未覆盖面**（依 §7 边界 1 与本轮新立的门禁条款第 4 条：留痕不等于豁免）。

**二次 `Slice.Run` 的"夹具失败"同样是既有纪律的兑现**：本仓明载「拒绝面命令会在同帧连跑时拆夹具」
（`MANUAL-TEST-RECIPE` §"你可以放心地连着跑"的订正条目）——`SkillLayer.Reject`（04:13:09）
跑完拆了夹具，34 秒后的 `Slice.Run` 取不到门面。**非产品问题**，且**与修前修后无关**。

**判据价值**：`State.Run` **只在 PIE 可用**（无头下跑不了）⇒ 这条回归**在无头面永远看不到**。
它是本轮"两条路线都必须跑"的**直接兑现**（同 `MEM-20261010-01` 的路线覆盖面差异）。

---

## 5. 编译证据（双配置）

| 配置 | 结果 |
|---|---|
| `LegendAutoChessEditor Win64 Development` | `Result: Succeeded` |
| `LegendAutoChess Win64 Shipping` | `Result: Succeeded`（55.21 s） |

**实施期一处构建陷阱（MUST 留痕）**：新增 `TcsAssetButler` 的`.cpp`后，
`TcsIntegration` 链接报 `LNK2019: unresolved external symbol`，而**真因是本模块的 `Build.cs` 缺依赖**
（`TcsIntegration.Build.cs` 未列 `TcsAssetButler`）——报错**只说要找的符号**，不指向 Build.cs。
⇒ 与 `MEM-20261014-21` 的"缺跨模块导出宏"同族（**跨模块新引用 MUST 逐项核 Build.cs 依赖 + 导出宏**）。

---

## 6. 本轮的**量具自证**（一处差点误判，MUST 留痕）

排查导出宏时我用 `strings <dll> | grep <符号>` 判断"符号是否导出"，**得 0**；
但同一量具对**已知必定导出**的 `FTcsAssetLoader` / `UTcsAssetManager` **同样得 0** ⇒ **量具自身失效**。
改用 `dumpbin /EXPORTS` 后自证成立（已知符号 **12** 命中、我的符号亦在）⇒ 符号**一直是对的**，
真因是 Build.cs 缺依赖（§5）。

**判据**（`MEM-20261014-05`）：**每条读数 MUST 带一个"它真的读到了输入"的自证量**——
本次的自证量 = 用已知导出符号做阳性对照。**MUST NOT** 拿单一量具的 0 就断定"符号没导出"。

---

## 7. 边界（MUST NOT 外推）

1. **PIE 面的修复后读数未重取**：§3.2 的修复后读数（`Slice.Run` 65/0）来自**无头**；
   PIE 面在修复后**未复验**（`State.Run` 修前 6/9、修后未跑）⇒ 如实登记为**未覆盖面**。
2. **`State.Reject` 的 1 条 FAIL 是既有边界**，非本重构引入（基线即 6/1）。
3. **其余四域自注册仍未落**（`TcsAttribute` / `TcsSkill` 两类未搬 ⇒ 当前走旧口仍正常）；
   本轮**只登记并修复了已搬迁的三类**（Chain / Trigger / Buff）。**判据** = 未搬类的旧口工作正常，
   迁到新口属架构统一、非缺陷修复。
4. **`DiscoverAttrModDefs` / `DiscoverSkillDefs` 未改**（同上；其类未搬迁，实测命中 1 / 5 正常）。
5. **无 `PrimaryAssetTypesToScan` ini 配置**：类型清单走各域代码自注册（裁定⑤），
   本轮的 `GetPrimaryAssetIdList` 依赖该登记**已完成**（未登记时新口会留 `Warning` 而非静默返空，见 §3.2 头注释）。

---

## 8. 复算脚本（本机逐字可跑）

```bash
# 先确保没有残留的无头进程（否则链接会 LNK1104）
taskkill //F //IM UnrealEditor-Cmd.exe

# 双配置编译
& 'E:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' LegendAutoChessEditor Win64 Development \
  -Project=E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject -WaitMutex
& 'E:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\Build.bat' LegendAutoChess Win64 Shipping \
  -Project=E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject -WaitMutex

# 无头（每条一个进程；-ABSLOG 用绝对路径，相对路径会静默不生成）
# ★ MSYS_NO_PATHCONV=1 或改用 pwsh —— bash 会把 /Game/... 转成 Windows 路径（实测踩过）
```
