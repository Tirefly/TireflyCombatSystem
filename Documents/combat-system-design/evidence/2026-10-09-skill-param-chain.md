# EVID-2026-10-09-skill-param-chain（R6 Task 4：技能参数账本 + 双形状修正器 + `STAT-1` 收束）

> **结论一句话**：技能参数**真的能改数**了——双形状修正器（照 M2 的 D2-13）、每条目一套参数槽位、
> Apply/Removal 成对、`CompeteGroup` 读侧选优（无休眠池）、五带折叠经**共享折叠器**调一次，
> **全部经无头 PIE 级实测通过**：`Run` **26 PASS / 0 FAIL**、`Reject` **5 PASS / 0 FAIL**，
> **全日志零 `ensure` / 零 `Fatal`**，`Run` 段**零红字**；**台账 `STAT-1` 就此收束**（三处消费者齐备）。
>
> **★ 本轮最有力的一条证据 = `P16`（A′ 裁定的运行时判据）**：参数行 `KeyA = 42`、链行用
> `ParamRef(KeyA, Fallback = -999)` ⇒ 读数 **42.0000**（不是兜底的 -999）⇒ **证明快照真的被当作
> 求值参数表消费**。"代码里调了 `Scope.GetTable()`"只证明管线存在，**证不了它接上了**——因为
> `ParamRef` 在参数表缺失时**静默落兜底**，两者可以给出同样的读数。§2.5。
>
> **★ 本轮最有力的一条**阴性**证据 = `P14`**：`ParamRef`（`AllowsValueConvention()` = **false**）
> 配 `VCF_Percent` ⇒ 读数仍 **0.850000**（若被无条件转换会是 **0.008500**）⇒ 证明"转不转"确由
> **源自身声明**，而不是写死的无条件转换。§2.4。
>
> **★ 本轮暴露一处我漏实现的明文需求（`P13` 专测）**：物化行 MUST "随施法终结**级联摘除**"，
> 而 `TerminateRun` 当时只摘 `RunHandles` ⇒ 槽位会**逐次累积**（"放两次技能、参数翻倍"），
> 且**单次激活读数完全正常**（只验一次的检查必漏）。§2.6。
>
> **证据纪律**（照 `EVID-2026-10-08-skill-cast-runtime` 先例）：区段哈希 + 复算脚本 +
> **如实边界清单**。每条读数对应日志一行；未覆盖面一律进 §6，**不用推断冒充实测**。

---

## 1. 读数（逐条对应日志一行）

### 1.1 `Tcs.Test.SkillParamChain.Run`（PIE 级；**26 项全过**）

**最终基线快照**：`Saved/Task4/snapshot-R6Task4-paramchain-run11.log`
（SHA256 `B987BE5E07464F7E…`，**229,797 B**）
```
SUMMARY Run: Passed=26 Failed=0
```

| # | 检查项（逐字取自日志） | 判据要点 / 对应 tasks |
|---|---|---|
| P1 | 技能门面可得 | — |
| P1b | 装置自持空链已登记 | **信号卫生**：全部合成定义的 `CastChainId` 落点（漏则每次激活 1 条 Error） |
| P2 | 夹具：单位已生成并注册 | — |
| P3a | 授予：含参数链行的合成定义落账本 | — |
| P3b | 五带折叠一次到位 ⇒ **330.0000** | 9.1；`Add+10 / PercentAdd+0.5 / Mul2`，初值 100 |
| P4 | 顺序无关（Mul 先写）⇒ **330.0000** | 9.2；读数取**两次求值结果**，未取"排序被调用过" |
| P5 | Override 三级比较 ⇒ **5.0000** | 9.3；**含阴性对照**（值 999 优先 0 的陷阱条未胜）+ `Add+1000` 被一并覆盖 |
| P6a | 外部施加：`ESM_ById` 选中条目并施加 2 条 | 9.10 前置 |
| P6b | 竞争组取组内最大 ⇒ **30.0000**（不是 40） | 9.4 |
| P6c | 按来源摘除 ⇒ 恰摘 **1** 条 | 9.11 |
| P6d | 落选者自动递补 ⇒ **10.0000**（无唤醒调用） | 9.5 |
| P6e | 重复摘同一来源 ⇒ 返回 **0** 且不 ensure | 9.11 |
| P7 | 值约定只转一次 ⇒ **0.8500** | 9.7（允许约定的源） |
| P8 | 无参数行初值为 0 ⇒ **7.0000** | 9.6 |
| P9a | Apply 落目标条目：选中条目槽位恰增 2 | 9.10 |
| P9b | 未选中条目不变：另一条目槽位数不变 | 9.10（**成对判据**） |
| P10a | `ByTag` 档：父 tag 层级命中多个子技能条目 | 选择器四档载体实读 |
| P10b | `ByTag` 档按来源成对摘除：**摘除 6 条** | >1 即层级命中多条目 |
| P11a | `Level` 键**不进**普通参数表（键 A 无参数行无槽位 ⇒ miss） | 9.8 前半 |
| P11b | `Level` 键进等级 ⇒ run 生效等级 **3** | 9.8 后半（`LevelBase=1` + `Level` 键 `Add+2`） |
| P12a | `EPM_Snapshot` 行进快照（11.0000） | 9.14 正向半 |
| P12b | `EPM_Live` 行**不进**快照数值面 | 9.14（缺失 `Mode` 分流时本项必失败） |
| P13a | 激活期物化条数稳定：两轮峰值均 **3** | 9.12（累积时第二轮必翻倍） |
| P13b | 施法终结按来源摘净：终结后 **0** | **本轮新发现的漏实现项的专测**（§2.6） |
| P14 | 能力位为假的源原样进账本 ⇒ **0.850000**（误转则 0.0085） | 9.7 对照组（**阴性证据**） |
| P16 | 快照真被当参数表消费 ⇒ **42.0000**（未接上则 -999） | **A′ 裁定的运行时判据**（§2.5） |

### 1.2 `Tcs.Test.SkillParamChain.Reject`（PIE 级；**5 项全过**，红字为预期）

**最终基线快照**：`Saved/Task4/snapshot-R6Task4-paramchain-reject1.log`
（SHA256 `3DCCCBC452CBD08F…`，**212,361 B**）
```
SUMMARY Reject: Passed=5 Failed=0
```

| # | 检查项（逐字取自日志） | 判据要点 |
|---|---|---|
| R2 | 夹具：单位已生成并注册 | — |
| R3a | `Custom` 档：入口返回 false（具名未实现，不静默当 `All`） | 9.9 |
| R3b | `Custom` 档**零修正被施加**：槽位数 **0→0** 逐字不变 | 9.9（**MUST NOT 只查"有 Warning"**） |
| R4 | 单位未注册：施加被忽略并返回 false（不 ensure） | M2 `D2-14` 同款口径 |
| R5 | 单位未注册：按来源摘除返回 0（不 ensure） | 同上 |

**本命令的 2 条 Warning 均为预期**（自带屏显声明）：
```
参数修正器施加未实现：选择器档 `Custom` 的片段契约归 R6.5-g——本次零修正被施加（单位 3）
参数修正器施加被拒：单位 999999 未注册（账本无桶）——忽略
```
> **为什么 MUST 独立成命令**（`MEM-20260918-07`）：它必然留 Warning ⇒ 混进常规命令会让
> "**常规验收命令零红字**"这条信号失效。**本轮我在实现期一度把它写进常规命令并自我论证"只是 Warning"**
> ——那正是该纪律要防的形态，已改正为独立 `.Reject` 命令。

### 1.3 回归（本 Task 改了 `FTcsCastOps::TerminateRun`，故必须回归施法面）

| 命令 | 读数 | 红字 | ensure |
|---|---|---|---|
| `Tcs.Test.SkillCast.Run`（Task 3 基线对照） | **26 / 0**（= Task 3 基线逐字相同） | **0** | 0 |
| `Tcs.Test.SkillCast.Reject` | **10 / 0**（= Task 3 基线） | 3（**Task 3 已登记的预期红字**） | 0 |
| `Tcs.Test.Slice.Run` | 3 PASS / 3 FAIL（检查 1b/2/4） | **0** | 0 |

**`Slice.Run` 的 3 项 FAIL 的归因（MUST 读，防误判为本轮回归）**：全部是**依赖"内容资产被发现"**的项
（链自动缓存 / 世界装配 / 属性定义装载），而 `-game` 无头模式 `AssetRegistry` **发现 0 条资产**——
这一点 **Task 3 的证据已明文登记**（`2026-10-08-skill-cast-runtime.md:161`）。**归属证据**：
① `git status --porcelain` 显示本轮 `Source/TcsIntegration/**` 改动数 = **0**（该缓存逻辑住那里）；
② Task 3 自己的回归基线用的就是 `SkillCast.Run`（`Saved/Task3/headless-regress.log`），本轮已逐字复现 **26/0**。

### 1.4 复现性

| 轮次 | 快照 | SUMMARY |
|---|---|---|
| run10 | `snapshot-R6Task4-paramchain-run10.log` | 26 / 0 |
| run11 | `snapshot-R6Task4-paramchain-run11.log` | 26 / 0 |

**逐字比对**：两轮 `[PASS]/[FAIL] + SUMMARY` 行各 **27 行**，`Compare-Object` = **零差异**
（按**结论行**比对，未比区段字节/行数）。

---

## 2. ★ 关键判据与阴性对照（本轮的证据核心）

### 2.1 `P3b` 五带一次到位（不是逐步施加）
`Add+10 / PercentAdd+0.5 / Mul2`，初值 100 ⇒ **330.0000**。
若实现是"边读边施加"（先 `Mul` 组后 `Add` 组），读数会是 `((100×2)+10)×1.5 = 315` ⇒ 可分辨。

### 2.2 `P4` 顺序无关
**刻意把 `Mul` 写在 `Add` 之前**，读数仍 **330.0000** ⇒ 顺序无关由折叠器纯函数性保证，
调用点**没有**"按带序排序"的补丁（`tasks 3.4`）。

### 2.3 `P5` Override 按优先级三级比较（**含阴性对照**）
三条同键：`Override 999 @优先级0`（陷阱条）、`Override 5 @优先级7`（应胜）、`Add 1000`（应被覆盖）
⇒ 读数 **5.0000**。
**阴性对照的意义**：若实现是"取数值最大者"，陷阱条会胜出（读数 999）⇒ **本项即失败**；
若实现漏了"Override 覆盖其它带"，读数会被 `Add 1000` 影响 ⇒ 也不等于 5。

### 2.4 `P14` 值约定能力位（**阴性证据**）
`ParamRef`（`AllowsValueConvention()` 返回 **false**）配 `VCF_Percent`、兜底 0.85 ⇒ 读数 **0.850000**。
若实现是"无条件按约定列转换"，读数会是 **0.008500**（差两个数量级）⇒ 证明判据真由**源自身声明**。
**为什么必须与 `P7` 成对**：`P7`（Literal 允许 ⇒ 85→0.85）在"无条件转换"的实现下**照样通过** ⇒ 单靠它判不出判据归属。

### 2.5 ★ `P16` 快照真被当作求值参数表消费（A′ 的运行时判据）
定义：参数行 `KeyA = 42`（进快照）+ 链行 `Override, ParamRef(KeyA, Fallback = -999)` ⇒ 读数 **42.0000**。

**为什么这是本 Task 最关键的一条**：A′ 裁定的全部内容是"快照 = 物化期的参数表载体"。
只验"代码里调了 `Scope.GetTable()`"是**静态**证据，**证不了它接上了**——
`FTcsParamSource_ParamRef::Evaluate`（`TcsParamSource_ParamRef.h:44-50`）在 `Context.ParamTable` 为空时
**静默返回 `Fallback`**（零报错）⇒ "没接上"与"接上了"可以给出**完全一样**的读数（只要兜底恰好等于期望）。
故判据 MUST 取**发散对**：真值 42 vs 兜底 -999。

### 2.6 ★ `P13` 施法终结按来源摘除（本轮新发现的漏实现项）
```
P13 读数：峰值 第一轮=3 第二轮=3（期望均 3）；终结后=0（期望 0）
```
- **`P13a` 为什么必要**：若终结不摘，第二轮峰值会变 **6** —— 而**单次激活的读数完全正常**。
  ⇒ **只验一次激活的检查必然会漏掉这个缺陷**（这正是 `MEM-20261008-02` 的形态）。
- **`P13b` 是"真的摘了"的正面读数**：终结后归零。

**该缺陷的经过（MUST 留痕）**：提案/delta 明文写了物化行"随施法终结**级联摘除**"，
但我在实现 `TerminateRun` 时只摘了 `RunHandles`（沿既有代码），**漏掉了新写的参数槽位**。
发现路径 = 我为自己装置写"两轮激活"的检查时**推演到**"第二轮会翻倍"。

---

## 3. 环境与产物身份

| 项 | 值 |
|---|---|
| 引擎 | UE 5.8（`E:\UnrealEngine\UE_5.8`） |
| 编译配置 | `LegendAutoChess Win64 Shipping` + `LegendAutoChessEditor Win64 Development`，**均 0 error / 0 warning** |
| 取证方式 | `UnrealEditor-Cmd.exe <uproject> /Game/TcsDev/L_TcsDev_Slice -game -nullrhi -nosound -noPause -unattended -ABSLOG=<日志> -ExecCmds=<单条命令>` |
| 宿主装置命令 | `Tcs.Test.SkillParamChain.Run`（常规，零红字）/ `Tcs.Test.SkillParamChain.Reject`（拒绝面，红字预期） |

> **无头路线的两处已知限制（沿用 Task 3 登记，本轮再次实证）**：
> ① `AssetRegistry` 发现 **0 条资产** ⇒ 凡依赖"内容资产被发现"的检查（如 `Slice.Run` 检查 1b/2/4、
> `SkillRegistry.Run`）**MUST 以编辑器内 PIE 为准**；② `-game` 不自行退出，取证脚本按固定时长
> 后 `Kill`（读数在固定时长内已全部落盘）。

### 3.1 区段哈希与复算

| 快照 | SHA256（前 16 位） | 大小 |
|---|---|---|
| `snapshot-R6Task4-paramchain-run11.log`（**最终基线**） | `B987BE5E07464F7E…` | 229,797 B |
| `snapshot-R6Task4-paramchain-reject1.log`（**最终基线**） | `3DCCCBC452CBD08F…` | 212,361 B |
| `snapshot-R6Task4-regress-cast-final.log`（回归） | `2B979B2B6A09FA26…` | 223,830 B |

复算（PowerShell）：
```powershell
Get-FileHash Saved\Task4\snapshot-R6Task4-paramchain-run11.log -Algorithm SHA256
Select-String -Path Saved\Task4\snapshot-R6Task4-paramchain-run11.log -Pattern "SUMMARY Run"
Select-String -Path Saved\Task4\snapshot-R6Task4-paramchain-run11.log -Pattern "P16 读数"
# 零红字 / 零 ensure 全量核对（MUST NOT 按模块前缀过滤）
Select-String -Path Saved\Task4\snapshot-R6Task4-paramchain-run11.log -Pattern "LogTcs\w+: (Warning|Error):"
Select-String -Path Saved\Task4\snapshot-R6Task4-paramchain-run11.log -Pattern "Ensure condition failed|Fatal error"
```

---

## 4. `STAT-1` 收束的可复算判据

台账 `STAT-1`（"参数链 / 流程属性黑板接入 `FoldTcsAttributeBands`——折叠器单份、三处共用"）本轮**全三处齐备**：

| 消费者 | 落点 | 时序 |
|---|---|---|
| M2 属性聚合 | `TcsAttributePipeline.cpp:251` | 全仓首个消费者 |
| TcsDamage 流程属性 | `TcsFlowAttributes.cpp:45` | 2026-09-20（plan2 Task 3） |
| **M5 技能参数链** | **`TcsParamChain_Evaluate.cpp:98`** | **2026-10-09（本变更）** |

**计数判据（MUST 数"真实调用点"，MUST NOT 数符号总命中）**：全仓真实调用点 **恰 3 处**；
`FoldTcsAttributeBands` 符号总命中 **9 处** = 定义 **1** + 注释 **5** + 调用 **3**——注释会让计数虚高
（**本项我先写错过一次**：初稿写"总命中 8 / 注释 4"，实测复算才发现是 9 / 5 ⇒ 已在证据、台账、
`tasks.md` 三处一并订正；这正说明**计数 MUST 复算、MUST NOT 凭记忆**）。
复算：
```powershell
cd Plugins\Tirefly\TireflyCombatSystem\Source
Select-String -Path (Get-ChildItem -Recurse -Include *.h,*.cpp | % FullName) -Pattern "FoldTcsAttributeBands"
```

---

## 5. 本轮对提案的三条裁决 + 一处装置缺陷（全部已回写规格）

| # | 事项 | 结论 | 回写位置 |
|---|---|---|---|
| ① | **快照消费面** | **A′**：快照只做**求值期参数表载体**（先例 = 状态侧 `TcsStateModifierMaterializer`）；`GetNumericParam` 保持**实时**；**不新增 run 作用域读口**（今天无消费者） | `skill-cast-runtime` delta 重写 + 本证据 §2.5 |
| ② | **`Level` 特殊键的词** | 由**插件原生声明**为 `TcsStateParam.Level`（+ 模块导出宏）——它是框架自己解释的词，下放宿主会让漏配变成**静默失效**。**声明模块 = `TcsState`**（`Public/Param/TcsStateParamKeys.h`，见 §7） | `skill-param-chain` delta + `spec/05-module-skill.md` 订正 ③ |
| ③ | **选择器枚举** | 新落 `ETcsEntrySelectorMode`；"零新枚举"口径**收窄**为"修正器的三个字段类型复用既有类型" | `skill-param-chain` delta 需求正文 + 场景 |
| ④ | **装置第一版 5 项假 FAIL** | 我未激活就读定义侧链行，而 `ParamChainRows` 的契约是"**激活期物化**"⇒ 读数退化成参数行初值 | `tasks.md §12.4(a)` + 本节 §2.6 |

---

## 6. ★ 一处由用户追问触发的归属订正：`Level` 键的声明模块（MUST 留痕）

**用户 2026-10-09 追问**：「StateDef 里就有 Level 参数，所以那个 Level Tag 的声明定义，是否应该放在
`TcsState` 模块里」——**复核后判定用户正确**，我初版落在 `TcsSkill` 是错的，已迁移。

**核实到的三条判据（不是"感觉更合理"）**：

| # | 判据 | 证据 |
|---|---|---|
| ① | **语义归属在本模块** | `LevelBase` / `MaxLevel` 声明在 `FTcsStateDefBase`（`Def/TcsStateDefBase.h:55/:59`），而 `FTcsSkillDefData : FTcsStateDefBase`（技能 Def **继承**它）⇒ **M5 没有自己的"等级定义"层**；D3-11「Level 终定」的家是 `spec/03-module-states.md:62` §3.5 |
| ② | **依赖方向（决定性）** | `TcsSkill/TcsSkill.Build.cs` 的 `PublicDependencyModuleNames` **已含 `TcsState`** ⇒ 声明在上游时两侧消费零成本；**若留在 `TcsSkill`，状态侧将来消费（状态等级修正）就要反向依赖 `TcsSkill` = 成环** |
| ③ | **契约词的声明方判据** | `gameplay-tag-governance`：「框架词汇（框架自己分发或自己解析的契约词）MUST 由**插件模块原生声明**」——判据是"谁解释那个词"，而公式由等级域解释 |

**我初版错在哪（错误形态留痕）**：我把"**本 Task 消费该键**"读成了"**本 Task 声明该键**"。
消费者 ≠ 所有者：M5 只是 `Level` 的**一个**消费者（状态侧若将来落等级参数链，是同词的第二消费者），
而**本体论（`LevelBase`/`MaxLevel`/D3-11 公式）与依赖上游都在 `TcsState`**。
这与 `MEM-20261009-01` 同族：**新形状撞上既有归属时，先假定自己错**。

**迁移动作与验收**：

| 项 | 内容 |
|---|---|
| 新建 | `TcsState/Public/Param/TcsStateParamKeys.h`（含三条判据的完整注释）+ `Private/Param/TcsStateParamKeys.cpp` |
| 删除 | `TcsSkill/Public/Skill/TcsSkillParamKeys.h` + `Private/Skill/TcsSkillParamKeys.cpp` |
| 改 include | `TcsParamChain_Evaluate.cpp` → `Param/TcsStateParamKeys.h`；**并清掉两处死 include**（`TcsParamChain_Materialize.cpp` 与宿主装置都只 include 而未使用该符号——顺手修掉） |
| 导出宏 | `TCSSTATE_API`（模块导出宏按模块名派生） |
| **验收** | 双配置 **0 error / 0 warning**；`Run` **26/0**（**`P11b` 仍读 3** ⇒ 跨模块符号解析与运行期 tag 注册都成立）、`Reject` **5/0**、零红字 / 零 ensure |

> **一处反向收益（值得记）**：宿主装置的 `P11` 用的是**字面 tag 名** `TEXT("TcsStateParam.Level")`
> 而非导出符号 ⇒ 它顺带成了"**原生 tag 真的在运行期注册成功**"的独立证据（若注册失败，
> `RequestGameplayTag(..., false)` 返回空 tag ⇒ `ParamKey` 为无效 tag ⇒ P11b 读数不会是 3）。
> 这与"符号能链接"是**两条不同的**判据，两条本轮都成立。

---

## 7. ★ 选择器的形状修正与词汇定案（2026-10-09 用户裁定，MUST 留痕）

**用户裁定（原话要点）**："历史遗留问题…`TcsEntrySelector` **首先我觉得应该改名叫 `TcsSkillEntrySelector`**…
之前所有 DefAsset 的 `DefId` 还是 `FName`，但现在都改成了 `GameplayTag`，可能你在推进度时，为了保证
`ById` 能够留存，**选择了筛选 `EntryHandle`，但这不现实，因为 `EntryHandle` 编辑器不应该能配置，
这是运行时产物**。我现在下结论：`ByTag` 枚举值改名为 `ByDefTag`；至于 `ById`…**先给 `ById` 留个空壳**…
我觉得所有 `StateDef` 都应该新增一个 `GameplayTagContainer` 变量，用来标识…**类别特征**…
`EntrySelector` 应该就是根据这个 `GameplayTagContainer` 变量来筛选 `SkillEntry` 的…感觉可以直接复用
GAS 的 `GameplayTagQuery` 相关内容。"

### 7.1 我原实现的确切错误（错误形态留痕）

我为了让 `ById` 档"有个能留存的东西"，把它的载体选成了 `FTcsSkillEntryHandle`
（`Index` + `Generation`）⇒ **该档永远只能从代码里传入**：句柄是桶内槽位下标 + 进程内发号代际，
编辑器里既没有、也无法预先填出合法值。于是对"**以配置为入口的外部施加**"这个场景而言，
**这一档等于不存在**——它在我的装置里"能过"，只是因为**装置是代码**。
**判据（可迁移）**：**字段的可配置性 MUST 与它所在入口的入口形态一致**——
配置入口里出现"只能由代码构造的值"，那一档就是**假档**（`MEM-20261009-01` 同族：形状要照实际消费者定）。

**同时纠正一处我漏看的历史**：`ById` 的"Id"由来是 Def 资产身份原为 `FName DefId`，
而**2026-09-22 已全量迁移到 `GameplayTag`（`DefTag`）** ⇒ "按 Id 筛"的正当形态就是**按 tag 筛**，
已被 `ByDefTag` 覆盖（同一份内容身份不需要两档）。我保留 `ById` 时**没有意识到它已经失去存在理由**。

### 7.2 落地的四处修正

| # | 修正 | 判据 |
|---|---|---|
| ① | 类型名 `FTcsEntrySelector` → **`FTcsSkillEntrySelector`**（枚举 → `ETcsSkillEntrySelectorMode`，前缀 `ESS_`；文件 → `TcsSkillEntrySelector.h`） | 它**只服务技能账本条目**（状态侧没有"已学条目"这一层）；名字带 `Skill` 免得将来第二个域出现时逼出改名（那时改动面是整个公开面） |
| ② | `ByTag` → **`ByDefTag`**（字段 `TagFilter` → `DefTagFilter`） | 原档名含混——"Tag"在 TCS 里同时指属性词/状态词/事件词/参数键等**七八种**东西 ⇒ 与字段名 `DefTag` 对齐，读代码即知"按哪个 tag" |
| ③ | **删除 `ById` 档**（判据见 §7.1） | 不可被作者配置 + 历史由来已消失 + 带代际的句柄过滤位会诱导"易碎的长期引用" |
| ④ | 新增 **`ESS_ByCategoryTags` 空壳** + 字段 **`FGameplayTagQuery CategoryTags`**（**零消费者**） | 承接删除位；行为 = **具名报出未实现 + 零修正被施加**（与 `Custom` 同款） |

### 7.3 词汇定案：`CategoryTags`（而非 `SpecTag` / `TraitTag` / `FacetTag`）

用户先提 `CategoryTags` 并征询，我复核后**同意**，且给出的是**仓内事实**而非口味：

| 候选 | 判定 | 依据 |
|---|---|---|
| **`CategoryTags`** | **✅ 采用** | ① 与用户自己的措辞一致（"类别特征"、"投掷**类**"、"引导**类**"）；② **有同族先例**——`DamageCategory` 根在 TCS 已是"**供条件匹配的伤害分类集**"（触发侧 `FTcsCondition_HasAllTags`，`gameplay-tag-governance` 根段注册表），本字段是**同一形态扩到定义级**，不是新造机制；③ 中性 ⇒ 适配"可能另立 `TcsGameplayTag` 模块"（那时它要同时服务 StateDef 与 SkillDef，`SkillCategoryTags` 一类名字当场作废） |
| `SpecTag` | **❌ 排除（仓内事实）** | **`SPEC-02-states` / `SPEC-04-skill` / `SPEC-05-skill` / `SPEC-07-notation` 是本仓设计文档的编号前缀** ⇒ `SpecTag` 会被读成"规格文档的 tag" |
| `Descriptor` | ❌ 排除 | 已被 openspec 能力名 `plugin-descriptor` 占用 |
| `TraitTags` | 备选留档 | 域内最干净（仅引擎 `TStructOpsTypeTraits` 命中），且与伤害分类**刻意拉开距离**；代价是"左手/右手/双手释放"这类**档位**读感偏"特质" |
| `FacetTags` | 备选留档 | 分类学上最精确（facet = 一条正交轴，正对"三选一"结构）；代价是可读性需解释 |
| `MarkerTags` / `KeywordTags` / `ArchetypeTags` | ❌ 排除 | 分别偏"单布尔标记" / "规则文本" / "单一原型"（与"多维共存"矛盾） |

**档名取复数 `ByCategoryTags`**（筛的是一个容器/查询）；`ByDefTag` 保持单数（那里匹配**单个**身份词）。

### 7.4 为什么只留空壳（用户明示"之后再落地"，我补出可执行判据）

① **载体不存在**——`StateDef` 侧的类别标识容器字段尚未新增（用户裁定"所有 StateDef 都应新增"，
属**独立变更**，不在本 Task）；② **规则未拍板**——`HasAll` / `HasAny` 一类筛选尚无消费场景；
③ **可能另立模块**——用户指出 GameplayTag 筛选在 TCS 里将是重要角色、**甚至可能单独开
`TcsGameplayTag` 模块** ⇒ 现在写进 `TcsSkill` 内部将来极可能要搬家。
**台账登记 = `STAT-10`**（载体 + 规则 + 模块归属三项一并裁）。

### 7.5 一处方法论收益（值得记）

**"先只留空壳"使这次改名的成本 = 几行字符串**：字段名、档名、装置检查名、台账行——没有一处
是"已落地的机制"要拆。反面教材正是 §7.1：**我为了保住一档，先给它选了个错的载体**，
于是"留"下来的不是空壳而是一个**假档**（在装置里能过、在配置里不存在）。
⇒ **判据**：档位暂不可实现时，**留空壳（具名报错 + 零副作用）优于留一个凑合的载体**。

---

## 8. ★ 定义侧 `Source` 的删除与账本侧改名（2026-10-09 用户质疑后订正，MUST 留痕）

**用户原话**："为什么 `FTcsNumericParamModifier` 也需要携带 `SourceHandle`，它不能作为纯 Def 类内容吗，
毕竟它有对应的运行时 `NumericParamModifierInstance`（现在好像叫 `NumericParamInstance`，感觉像是参数实例，
但是注释却写道是**技能参数修正器实例**）"——**两问都成立，且第一问指向的是一处真实缺陷**（不只是"可以更纯"）。

### 8.1 那个字段是**假字段**（三条判据）

| # | 判据 | 实测证据 |
|---|---|---|
| ① | **它不是作者可配、也不会被序列化** | `FTcsSourceHandle` 是**非反射纯 C++ struct**（`TcsSourceHandle.h:20` 裸 `struct`）⇒ **不能作 `UPROPERTY`**。**UHT 产物实测**：`FTcsNumericParamModifier` 的反射属性恰为 `ParamKey / Op / Operand / ValueConvention / SortKey / OverridePriority / CompeteGroup`，**`NewProp_Source` 命中数 = 0** ⇒ 它**既不进细节面板、也不进资产**，只活在内存里 |
| ② | **定义侧路径根本不读它** | `TcsParamChain_Materialize.cpp:65` 传的是 `RunSource`；**全库唯一读 `Row.Source` 的地方是外部施加路径**（`TcsParamChain.cpp:122`） |
| ③ | **M2 的既有形状即此** | `FTcsAttrModDefTableRow`（定义侧行）**没有 `Source` 字段**——它是 `MakeFromDef(Row, Operand, InSource)` 的**形参**。我把形参提升成了字段 |

**语义根因**：**"源"从来不是"这一行"的属性**——同一个 `ParamChainRows[0]`，甲玩家靠装备给、
乙玩家靠天赋给 ⇒ **来源因实例而异，属实例而非 Def**。

> **这一条与前面两次是同一错误形态**（`ById` 档选了不可配置的 `EntryHandle`；`STAT-5` 同族）：
> **给一个值选载体时，先问"它的生命周期属于谁"**。判据成型：
> **因实例而异的值 MUST NOT 住 Def 类型**；而 **Def 类型上的字段 MUST 是"作者可配 + 会被序列化"的**，
> 否则即假字段。⇒ **一个 Def 类型上"配不了也存不下"的字段，只会误导后人。**

### 8.2 落地的三处修正 + 一条连带新增的守卫

| # | 修正 | 实证 |
|---|---|---|
| ① | **删定义侧 `Source`**；`Source` 只住账本侧，由**施加方**给出 | UHT 反射属性表（§8.1①） |
| ② | **`ApplyParamModifiers` 新增 `FTcsSourceHandle Source` 形参**（外部施加入口自报来源） | `P6a` 改为**两次独立施加**（"两个来源各挂一条"必须走两次调用——与真实用法一致：装备一次、天赋一次）；`P6b`~`P6e` 读数不变（30 / 恰摘 1 / 递补 10 / 重复摘 0） |
| ③ | **账本侧改名** `FTcsNumericParamInstance` → **`FTcsNumericParamModInstance`** | 与 M2 `FTcsAttr**Mod**Instance` 逐字同构；规则 = `[表词] + Mod + 面后缀` |
| ④ | **连带新增守卫：`Source` 无效 ⇒ 拒绝施加且零写入** | 新增 `.Reject` 检查 `R6a`/`R6b`：返回 false、**槽位 0→0**（判据 MUST 是"槽位未变"，MUST NOT 只查"返回 false"——"先写后判"也能返回 false） |

**④ 的必要性**：`Source` 是"按来源摘除"的锚点 ⇒ 无锚点的条目**永远摘不掉**，表现为
"参数改不回去"（静默、且远离根因）。⇒ 宁可拒绝这次施加。

### 8.3 同批订正：我注释里自造的布尔侧名字（用户追问触发）

用户追问"**BoolSwitch 的相关变量命名都是什么样的**"——查证后发现**我在形状预留里写错了**：

| 面 | 数值侧（实况） | 布尔侧（实况） |
|---|---|---|
| 参数行 | `FTcsNumericParamRow` | **`FTcsBoolSwitchRow`**（R6 Task 1 已交付） |
| 修正器 | `FTcsNumericParamModifier` | **`FBoolSwitchModifier{SwitchKey, Value, Source}`**（设计名，D5-5 v2） |
| 账本条目 | `FTcsNumericParamModInstance` | **`FTcsBoolSwitchModInstance`** ← **我在注释里曾写 `FTcsBoolSwitchParamInstance`，错** |

**规则 = `[表词] + Mod + 面后缀`**，两张表各有表词：数值侧 `NumericParam`、布尔侧 `BoolSwitch`。
我那个名字把布尔表词与数值表词 `Param` **混装**，两头都不沾——`FTcsBoolSwitchRow` 本身也不带 `Param`
（设计管它叫"布尔**开关**表"）。**该错名已从注释、`proposal.md`、两份 research 文档、plan 一并订正。**

### 8.4 本轮验收读数（改后复跑）

```
Tcs.Test.SkillParamChain.Run     26 PASS / 0 FAIL   零红字  零 ensure
Tcs.Test.SkillParamChain.Reject   9 PASS / 0 FAIL   4 条预期 Warning（+1 = 新增的来源无效守卫）
双配置编译                        0 error / 0 warning
```

---

## 9. 如实边界清单（未覆盖面；MUST NOT 读成"已覆盖"）

1. **`AttributeScaled` 类操作数本轮**不**可用**（**预先存在的缺口，非本轮引入**）：
   该源要求**派生**上下文 `FTcsAttributeEvaluateContext`（持 `ITcsAttributeProvider`），
   而技能侧 `MakeContext` 只装配**基础**上下文 `FTcsParamEvaluateContext` ⇒ 该源的 checked cast 失败、
   **落各自 `Fallback`**。全仓 `ITcsAttributeProvider` **零实现者**（只有 `TcsAttribute` 内的契约声明与
   消费点；`TcsCombatEntityComponent` 是候选实现方但未实现）。⇒ **设计 §3.4 那句"攻击力×倍率"的
   `AttributeScaled` 半边，本轮只验了 `ParamRef` 半边**（`P16`）。归属 = 出现"技能读属性值"的真实
   消费者时（M6 集成侧），可参照 `FTcsStateEvaluateContext` 的既有派生机制补技能侧派生上下文。
2. **`ESM_Custom` 档的片段契约**未实现（归 `R6.5-g`）——本轮只验"具名报出 + 零施加"。
3. **`UTcsSkillModDef` 模板引用形态**未落（该类型全仓 `Source/` 零命中）——`ParamChainRows` 只落内联形态，归 `R6.5-f`。
4. **技能账本无"冻结暂存区"** ⇒ `RemoveParamModifiersBySource` 只扫在册条目（M2 需额外扫 `FrozenAttributes`）。
   **若将来出现"技能条目冻结"，该扫描面 MUST 同批补上**——已写入 `TcsParamChain.h` 的边界登记。
5. **`Level` 修正是单趟**：`Level` 键若配"按等级取档"的数值源，读到的是折叠**前**的等级
   （`Entry->Level`）；"等级修正链套等级链"不存在于设计语料，**不为其预建多趟收敛**。
6. **冷却与 Cost 未做**（`R6.5-a`/`R6.5-d`）：门禁第 ③⑤ 道仍是占位恒过。
7. **`CHAIN-7`（链挂条目回收）未在本轮闭合**：本轮摘的是**技能定义侧物化行**（M5 技能账本）；
   链步骤经 `Context.RunSource` 挂到 **M2 属性账本**的修正器仍归 Task 5 Step 3 的三路统一回收例程。
8. **`SkillRegistry.Run` 装置无法无头验收**：它读取**真内容资产**（`DiscoverSkillDefs`、
   "真资产配置的字面量 17"），而 `-game` 发现 0 条资产 ⇒ 本命令的 13 FAIL **属环境限制、非代码缺陷**，
   MUST 以编辑器内 PIE 为准。
9. **本轮零改动面（反查证据）**：`Source/TcsDamage/**`、`Source/TcsTargeting/**`、
   `Source/TcsEffect/**`、`Source/TcsIntegration/**` 的 `git status --porcelain` 计数均为 **0**。
10. **宿主装置文件超出"300 行"体例（如实登记）**：`TcsDevSkillParamChainProbe.cpp` = **713 行**。
    **判据澄清**："≤300 行"**不是**本仓 `openspec/specs/` 的硬规则（全仓反查无此条），既有插件文件
    已到 626 行、宿主装置 `TcsDevSliceRig.cpp` 已到 **3094** 行 ⇒ 713 行**沿用装置侧既有体例**，
    且它是**宿主开发用装置、不进交付物**。**未收项**：若后续要收，按命令面切两个 `.cpp`
    （`Run` / `Reject` 已在函数层分离）。
