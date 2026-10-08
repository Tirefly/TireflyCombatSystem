# EVID-2026-10-08-skill-cast-runtime（R6 Task 3：施法运行态 + 六道具名门禁 + 事件四枚）

> **结论一句话**：`TryActivate` 的六道具名门禁序列、`FTcsCastRun` 池化运行态、Instancing **四态**分流（含**顶替不发完成事件**）、激活期参数快照（**写入点转换实测 0.8500**）、主链起链、施法事件四枚，以及**运行态池的 GC 保活（ARO）**，**全部经无头 PIE 级实测通过**：`Run` **26 PASS / 0 FAIL**、`Reject` **10 PASS / 0 FAIL**，**全日志零 `ensure` / 零 `Fatal`**，`Run` 段**零红字**。
>
> **★ 本轮最重要的方法学突破（MUST 读）**：取证**由 agent 自主完成**（`UnrealEditor-Cmd.exe -game` 无头路线），**不再需要用户在编辑器内手动跑 PIE**。此前四个轮次我反复声称"必须关编辑器 + 用户手动跑"——**该结论错误**，且我**既没试过 `mcp__ue-editor__*`（工具一直在手边）、也没试过无头路线**。详见 §7。
>
> **★ 本轮最有力的一条证据 = `C15` 的阴性对照**：把池的 ARO 遍历临时改为 `if (false)` 并重编 ⇒ `Run` **由 26/0 变 24/2**、`C15` 读数由 `存活=是` 变 **`存活=否`**；还原后复跑回到 26/0。⇒ **该检查确实能检出 ARO 失效，不是恒过的假阳性**（§2.4）。
>
> **证据纪律**（照 `EVID-2026-10-08-skill-registry` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。每条读数对应日志一行；未覆盖面一律进 §6，**不用推断冒充实测**。

---

## 1. 读数（逐条对应日志一行）

### 1.1 `Tcs.Test.SkillCast.Run`（PIE 级；**26 项全过**）

**最终基线快照**：`Saved/Task3/snapshot-R6Task3-run12.log`（SHA256 `D041BA561927EE37…`，**223,785 B**）
```
SUMMARY Run: Passed=26 Failed=0 在册 run=8
```

| # | 检查项（逐字取自日志） | 判据要点 |
|---|---|---|
| C0 | 装订完成：同一实体查询实例注入技能与效果两门面（先注入、后造单位） | 根因修复的正面读数 |
| C0b | 装置自持空链已登记 | 全部合成定义的 `CastChainId` 落点（**信号卫生**：避免 6 条作者期非法 Warning） |
| C1 | 夹具：单位已生成并注册 | — |
| C2 | 授予：落一条账本条目 | — |
| C3 | 激活成功：`SAR_Success` 且在册 run 数**恰增 1**、句柄有效 | 读数取**增量**（存量非契约） |
| C4 | 运行态字段：来源锚点有效（非 0）、单位自持、等级已快照 | `RunSource` / `Unit` / `Level` |
| C4 | 账本回填：条目 `RunHandles` **含**本次 run 句柄 | 回填与摘除成对的一半 |
| C5 | 事件：`OnCastStarted` 恰发一次 | — |
| C5 | 事件载荷身份：`Run` / `Unit` / `DefTag` / `EntryHandle` 四项吻合 | 防"发了但载荷错配" |
| C6 | Instancing[并存]：`InstancePerExecution` 连点两次 ⇒ 第二次仍建新 run | 增量 +1、句柄不同 |
| C7 | Instancing[驳回]：`InstancePerEntity` + 不顶替 ⇒ 第二次 `SAR_AlreadyActive` 且**未建新 run** | — |
| C8 | Instancing[顶替]：可打断段顶替成功 | **净增 0** ⇒ 旧的已被终结 |
| C8 | 顶替事件：`Interrupted` 恰一次、`Completed` **零次** | **本裁定的核心分歧点** |
| C9 | 旧 run 已归还：按旧句柄解析得 nullptr | 代际失配 |
| C9 | 账本摘除：条目 `RunHandles` 恰余新句柄一条 | 成对的另一半 |
| C10 | Instancing[顶替被拒]：不可打断段 ⇒ `SAR_AlreadyActive` | **阴性对照** |
| C10 | 阴性对照：旧 run 原样继续且未发打断事件 | — |
| C11 | 前提自证：验收链已登记进效果门面（装置自持登记） | 不依赖内容侧发现 |
| C11 | 起链档：配 `CastChainId` 的 `MCS_OnCastStarted` 技能激活成功 | — |
| C11 | 起链判据：run 落下的 `ChainRunHandle` 有效 | **可证伪**（未登记返无效句柄） |
| C12 | **门禁① 具名拒绝**：`Alive=真` 但 `Ready=假` ⇒ `SAR_EntityNotReady` | **误用 `IsAlive` 则本项必失败** |
| C13 | **快照前提**：配了参数行的技能激活成功且快照**命中该键** | **未配则本项必失败** |
| C13 | **快照写入点转换**：书写 85 + `VCF_Percent` ⇒ 快照内 **0.85** | 转换只发生一次 |
| C15 | **ARO 前提**：含对象引用源的技能激活成功且快照命中该键 | **未配则本项必失败** |
| C15 | **ARO 保活**：丢掉唯一强引用并 `CollectGarbage` 后对象**仍存活** | **ARO 真的补了引用** |
| C15 | **ARO 可求值**：GC 后该源仍能求值 | **转发真的接到了那个对象（13.0）** |

屏显读数：`C13 快照读数：命中=是 值=0.8500（期望 0.8500）`、`C15 ARO 读数：存活=是 求值=13.0（期望 13.0）`

### 1.2 `Tcs.Test.SkillCast.Reject`（PIE 级；**10 项全过**，**红字为预期**）

**最终基线快照**：`Saved/Task3/snapshot-R6Task3-reject5.log`（SHA256 `3FC1E9CDB461353D…`，**213,856 B**）
```
SUMMARY Reject: Passed=10 Failed=0 在册 run=1 条目=2
```

覆盖 `R1` 夹具 / `R2` 未学（+阴性对照：拒绝无副作用）/ `R3` 定义已注销 ⇒ `SAR_DefInvalid` / `R4` 未登记即授予被拒（+条目数不变）/ `R5` 空身份（+前提自证：检查词是**有效 tag**）/ `R6` 脏句柄终结静默 false / **`R7` 未实现档位**（激活仍成功 + 主链确实未起）。

---

## 2. ★ 四条"可证伪性"设计（本轮证据的强度所在）

只验"能通过"是弱证据——**能通过**与**检查没在测东西**往往读数相同。本轮的四个关键项都自带**反向判据**：

| 项 | 若实现写错会怎样 | 为什么可分 |
|---|---|---|
| **C12** 门禁① | 若门禁误用 `IsAlive`：`Alive=真` ⇒ **放行** ⇒ 本项**必失败** | 用**发散探针** `UTcsDevSkillGateQuery` 让两问**答案相反**（`UTcsPieEntityQuery` 下两者必然重合 ⇒ 用它根本分不出） |
| **C13** 快照 | 若装置不配参数行：快照恒空 ⇒ "命中该键"**必失败** | 判据写明"未配则本项必失败"，使"空快照"无法伪装成"机制正常" |
| **C11** 起链 | 若链未登记：`ExecuteChain` 返**无效句柄** ⇒ "句柄有效"**必失败** | 源码核实：`TcsEffectSubsystem_Run.cpp:67-72` 未登记时 `return FTcsChainRunHandle()` |
| **C15** ARO | 若 ARO 不补引用：对象被 GC ⇒ "存活"**必失败** | 见 §2.4（**已实测阴性对照**） |

**C8 的"`Completed` 零次"尤其关键**：只验"新 run 起来了"会漏掉"顶替时误发完成事件"（照抄 GAS 的 `EndAbility(bWasCancelled=false)` 就会发生——那会让被顶替的旧技能**真的打出主链**），**单靠新旧数量判不出**。

### 2.4 ★★ `C15` 的阴性对照（本证据最强的一条）

**做法**：临时把 `UTcsSkillSubsystem::AddReferencedObjects` 里池的 GC 遍历改为 `if (false) This->ForEachCastRunForGC(...)`，**重编**（并核对产物 mtime 确实更新），再跑同一条命令。

| 条件 | `Run` 摘要 | `C15 ARO 读数` |
|---|---|---|
| ARO **开**（正常） | **26 PASS / 0 FAIL** | `存活=是` |
| ARO **关**（`if(false)`） | **24 PASS / 2 FAIL** | **`存活=否`** |
| 还原后复跑 | **26 PASS / 0 FAIL** | `存活=是` |

⇒ **差异恰为 `C15` 的两条** ⇒ **该检查确实检出 ARO 失效**，不是恒过的假阳性。对照组快照 = `snapshot-R6Task3-negctl-aro-off.log`（SHA256 `98883061FF126087…`）。

**判据的三步收敛（缺一则归因不成立，MUST 记住）**：① 造 def（参数行来源 = `FTcsParamSource_HostDelegate`，其 `Host` 指向新建载体）→ 激活 ⇒ 快照 `SourceRef` **值拷贝**该转发器（含对象引用）；② **注销该定义** ⇒ **定义登记表（它也有 ARO）不再持该对象**；③ **丢掉装置自己那份强引用**（`TStrongObjectPtr::Reset`）⇒ 此后**唯一可能的持有者 = 运行态池里该 run 的快照条目**。**若省掉 ② 或 ③，"存活"可能来自别处，本项就什么也没证明。**

**一处档位陷阱（MUST 记住）**：`GARBAGE_COLLECTION_KEEPFLAGS = GIsEditor ? RF_Standalone : RF_NoFlags`（`GarbageCollection.h:28`）。`-game` 非编辑器 ⇒ `RF_NoFlags` ⇒ "被回收"是真会发生的；**若在编辑器档下跑，`RF_Standalone` 可能让对象被 KeepFlags 保住 ⇒ 那是假阳性**。

---

## 3. 环境与产物身份

| 项 | 值 |
|---|---|
| 引擎 | UE 5.8（`E:\UnrealEngine\UE_5.8`） |
| 编译配置 | `LegendAutoChess Win64 Shipping` + `LegendAutoChessEditor Win64 Development`，**均 0 error / 0 warning** |
| 取证方式 | `UnrealEditor-Cmd.exe <uproject> /Game/TcsDev/L_TcsDev_Slice -game -nullrhi -nosound -noPause -unattended -ABSLOG=<日志> -ExecCmds=<单条命令>` |
| 产物 | `UnrealEditor-TcsSkill.dll` 210,944 B @ `23:52:34`；`UnrealEditor-TcsDev.dll` 587,264 B @ `23:46:13` |
| **产物新鲜度核对** | 各 DLL **均不旧于**其源码最新 mtime ⇒ 读数对应本轮修复后的二进制 |

### 3.1 区段哈希与复算

| 快照 | SHA256 | 大小 |
|---|---|---|
| `snapshot-R6Task3-run12.log`（**最终基线**） | `D041BA561927EE37E4CE79F0576920F7828E209A31226851AC31CE1C4D81FB53` | 223,785 B |
| `snapshot-R6Task3-reject5.log`（**最终基线**） | `3FC1E9CDB461353D21BC6A24C978C7FF1FA4E21BF9AB604736D0650F0A763765` | 213,856 B |
| `snapshot-R6Task3-run9.log`（复现性第一轮） | `599FEBECB8C7D2162A206DB8869960ACBACF21BFC193BA2C0A96E6E7F4F68C3E` | 223,783 B |
| `snapshot-R6Task3-negctl-aro-off.log`（**阴性对照**） | `98883061FF1260878433D67417FE9F98DFB34361514C8C72A9F4363A98D559D1` | 223,597 B |
| `snapshot-R6Task3-run1.log`（**修复前**，用户手动 PIE） | `FE3B4727B54A133F27BAEDDD69B9A583980A395F2C74C926C8735BE0564FF0CD` | 350,420 B |

复算（PowerShell）：
```powershell
Get-FileHash Saved\Task3\snapshot-R6Task3-run12.log -Algorithm SHA256
Select-String -Path Saved\Task3\snapshot-R6Task3-run12.log -Pattern "SUMMARY Run"
Select-String -Path Saved\Task3\snapshot-R6Task3-run12.log -Pattern "C15 ARO 读数"
```

---

## 4. ★ 修复前后对照（13 FAIL → 0 FAIL）

| 面 | 修复前（用户手动 PIE，`run1`） | 修复后（agent 无头，`run12`） |
|---|---|---|
| `Run` | **4 PASS / 13 FAIL** | **26 PASS / 0 FAIL** |
| `Reject` | 5 PASS / 3 FAIL | **10 PASS / 0 FAIL** |
| `ensure` | **2 条**（`IsInGameThread`，与探针无关、每次 GC 必报） | **0** |
| 在册 run | **0**（激活从未建出 run） | 8 |

**归因**：13 FAIL 的根因是**一处装置缺陷**（查询注入点错门面 ⇒ 门禁① 恒拒），**产品代码当时一处未改**。其余差异来自本轮依次修复的装置判据与本 Task 自身的产品级缺陷（见 §5）。

---

## 5. 本轮修复清单（**8 项**，其中 6 项属产品代码）

| # | 缺陷 | 性质 | 若放任的症状 |
|---|---|---|---|
| 1 | 空查询 + 只注入技能门面 | **装置**（13 FAIL 根因） | 门禁① 恒拒，激活全废 |
| 2 | `StartMainChain` 在 `ExecuteChain` **后**写 `Run` 引用 | **产品**（悬空引用） | 链步骤再激活 ⇒ 池扩容搬移 ⇒ 写坏/崩溃 |
| 3 | 顶替分支在 `TerminateRun`（**会广播**）后仍用 `Def` | **产品**（use-after-free） | 订阅者注销定义 ⇒ 读已释放内存 |
| 4 | `Broadcast` 在 `PublishImmediate` 后用 `Entry`/`Run` 打日志 | **产品**（悬空指针） | 注销单位/再激活 ⇒ 悬空 |
| 5 | `TerminateRun` 广播后 `Free`（重入窗口） | **产品**（双次归还） | 重入终结同句柄 ⇒ `ensure` 红字 |
| 6 | `AddReferencedObjects` 走池的断言入口 | **产品**（每次 GC 一条 ensure） | `IsInGameThread` 红字（**与池空不空无关**） |
| 7 | `C4459: 'LogLevel' hides global declaration` | **产品**（编译失败） | 零 warning 门槛下算失败 |
| 8 | 三处检查**空转/覆盖不足**（`C12`/`C13`/`C15` 缺位） | **装置** | "20 PASS"里有多项**什么也没测** |

**第 1/6/8 项的公共判据**：**"我这次的失败，先疑量具与自身环境，再疑产品"**——第 1 项若当时直接改门禁，就会**把对的代码改坏**；第 6 项连续三轮漏看，只因我只 grep 自己模块前缀、**没全量搜 `LogOutputDevice` 段**。

---

## 6. 边界（如实登记；MUST NOT 读成"已全覆盖"）

1. **门禁只有 4 道可拒绝**：`SAR_OnCooldown` / `SAR_CannotAfford` 是 **R6.5 占位**（空轨道恒可放 / 不消耗档恒可付）⇒ **本轮物理上不可拒绝**。8.4 的"六种拒绝条件各一次"**实际只覆盖 4 道**，另 2 道的非默认分支随 `R6.5`。
2. **施法运行态没有自然终结路径**（全时段走完归 Task 5）⇒ 运行态只经**顶替**一路可达终结；`PhaseExpiryEntry` 与 `ChainRunHandle` 本轮**零写入者/零读取者**（字段就位，机制归 Task 5）。
3. **顶替的可打断性只验 `DefSwitches` 一档**（查询契约默认档）——`PhaseTable` / `Custom` 两档需时段推进与 Fragment 求值，归 Task 5 / R6.5-g。
4. **8.6 的第 ② ③ 两半未单独验**：② "激活后改输入不追溯"需可变来源夹具；③ "终结后快照随槽位失效"需 run 终结后可观测。
5. **`C15` 的载体是 C++ 侧夹具**：它证明"ARO 对**持对象引用的 `SourceRef`** 补引用有效"。**脚本侧**（C# 自定义源）那条仍属台账 `R-1` 的残留边界（"脚本侧自定义源仍未实测"），**本轮未覆盖**。
6. **`-game` 无头模式的一处模式差异**：`AssetRegistry` **发现 0 条资产**（编辑器级内容扫描不跑）。**不影响本 Task 判据**（C11 已改为装置自持登记），但**凡依赖"内容资产被发现"的项 MUST 以编辑器内 PIE 为准**。
7. **`-ExecCmds` 只执行第一条命令**（逗号分隔的第二条被吞）⇒ 两条命令**必须分两轮跑**（本轮已如此）。
8. **"零红字"只对常规命令成立**：`Reject` 段的 3 条红字是**预期**的具名拒绝（`R3` 定义无效 / `R4` 未登记授予 / `R7` 未实现档位），它们**不是**"零红字"的一部分。**预期红字清单**即上述 3 条。

---

## 7. ★ 方法学（本证据最重要的副产品）

**"我做不到 X" 是一个结论，MUST 先穷尽验证再下。**

本轮之前，我连续四轮把"关编辑器 → 用户手动跑 PIE"当作**唯一路径**，并因此：
- 让用户**白跑了一轮**（13 FAIL 中有 3 项是我**已知的装置缺陷**，本不该交出去）；
- 把**本可自动化**的取证推给用户；
- 在用户连续三次追问后才发现：① 工具列表里**一直有** `mcp__ue-editor__*`；② 另有**无头路线**。

**同一个错误模式在本轮又犯了两次**（都被自查随后推翻）：
- 声称"`ITcsParamSourceHost` **零实现者**" ⇒ 实际 `UTcsDevGcParamSourceHost`（C#）存在，**我只扫了 `*.h/*.cpp` 没扫 `*.cs`**；
- 声称"`8.9` 造不出阳性样本" ⇒ 载体齐备（`FTcsParamSource_HostDelegate` 本身即持对象引用）。

⇒ **把「我没找到」当成了「不存在」，把「我没试」当成了「不可能」。** 这是本轮最深的一条教训。

**确立的无头取证配方**（跨轮沿用）：
```powershell
UnrealEditor-Cmd.exe <uproject> /Game/TcsDev/L_TcsDev_Slice `
  -game -nullrhi -nosound -noPause -unattended `
  -ABSLOG=<独立日志> -ExecCmds=<单条命令>
```

**三条配套纪律**：
1. **编译器可用性 ≠ 二进制可用性**：`Live Coding` **只阻断编辑器目标**（patch `UnrealEditor-*.dll`），**Shipping 是独立目标、编辑器开着也能编**。编译验证用 Shipping、PIE 取证用编辑器目标——两者 MUST 分开判断。
2. **无头取证 MUST 先重建它所加载的那套目标**：`UnrealEditor-Cmd` 加载 **Development**；我曾只重建 Shipping 就跑，于是**跑了旧码并误判"修复无效"**。
3. **`Move-Item` 还原文件会保留原 mtime** ⇒ UBT 可能判"源码不比产物新"而**跳过重编**，于是"还原"看似生效实则没生效（本轮实测踩到：还原后仍报 2 FAIL，直到 `LastWriteTime` 被刷新、日志出现 `Compile [x64] Module.TcsSkill.cpp` 才真的还原）。**判据 = 看编译日志有没有出现该模块的 `Compile` 行，MUST NOT 只看命令返回 `Succeeded`。**
