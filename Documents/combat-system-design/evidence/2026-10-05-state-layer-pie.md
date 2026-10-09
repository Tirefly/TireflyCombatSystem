# EVID-2026-10-05-state-layer-pie（R5 Task 7：状态层端到端竖切验收）

> **结论一句话**：状态层第一次被**真内容资产**（4 个 `.uasset`，非运行期手搓定义）走通到底——`DA_Chain_E2E_Apply` 起链 → `DA_State_E2E` 建实例（2.00s 有限时值）→ 内联触发行在 `Applied` 上再入起 `DA_Chain_E2E_Behavior` → `DA_ModDef_E2E` 这枚**修正器模板**经**快照 `ParamRef` 跨资产引用**取到 25.0 并落到护甲（+25，替代定义侧 10.0 与兜底 −999.0）→ 0.50s 周期回调 → 2.60s 自然到期回收、触发行按锚点退订、**护甲精确回到施加前**，而**行为链改的攻击停在 X−5**（链挂条目按设计常驻）。同轮既有三条命令回归**逐字不变**（65/0、72/0、11/0）。
>
> **证据纪律**（照 `EVID-2026-10-05-state-behavior-fragments` 先例）：冻结快照 + 区段哈希 + 复算脚本 + **如实边界清单**。本文每条读数对应日志里的一行；"未覆盖"的面一律进 §6，不用推断冒充实测。

---

> **实施注记（2026-10-09，按 `docs-convention` §7「冻结」只追加、正文不改）**：本文正文出现的
> `ModifierSlots` / `ParamSlots` 应读作 **`AttrModInstances`** / **`NumericParamModInstances`**
> （提案 `refactor-rename-modifier-ledger-fields`）。**改名判据** = 容器字段名 MUST 取元素类型名的
> 复数形式（`FTcsAttrModInstance` → `AttrModInstances`；`FTcsNumericParamModInstance` →
> `NumericParamModInstances`），**`Slot` 一词保留给"可按下标寻址、可复用、带代际的空位"**。
> 旧名沿袭自 AbilityKit 的"修改器槽位自由链表"，而本仓早已拆掉槽壳、元素直接就是修正器条目。
> 详见 `PLN-R6` 的「本次改名」落地记录与提案 `refactor-rename-modifier-ledger-fields`。


## 1. 读数（逐条对应日志一行）

**命令计数**（装置屏显 + 日志 `[TcsDevRig]` 行）：

| 命令 | 本轮（改后） | 基线（改动前） | 变化 |
|---|---|---|---|
| `Tcs.Test.State.Run`（竖切，即时段 + 延时段） | **通过 15 / 失败 0** | 命令不存在 | 新增 |
| `Tcs.Test.State.Reject`（拒绝面） | **通过 7 / 失败 0** | 命令不存在 | 新增 |
| `Tcs.Test.Slice.Run`（即时段）※回归 | **通过 65 / 失败 0** | 65 / 0 | **0** |
| `Tcs.Test.Slice.Run`（延迟段收束）※回归 | **通过 72 / 失败 0** | 72 / 0 | **0** |
| `Tcs.Test.Slice.Reject`※回归 | **通过 11 / 失败 0** | 11 / 0 | **0** |

**"零非预期红字"的落点**：`Tcs.Test.State.Run` 两轮区段内 `Warning:` / `Error:` 行数 **各 0 条**（§3 脚本 ③ 可复算）。拒绝面命令**故意**制造红字（实测区段内 **6 条 Warning + 3 条 Error**，逐条归属见 §2.2），故它**不**纳入本判据。

### 1.1 竖切即时段（S1–S10，**同一帧内完成**）

| 检查 | 日志行 | 读数 |
|---|---|---|
| S1 跨 PIE 无残留 | L2657 | `本世界总状态实例 0 期望 0；效果门面触发行 2` |
| S2 内容身份解析 | L2658 | `buff 定义 命中；修正器模板 命中；定义库失败清单 0 条期望 0` |
| S3 模板身份一致且不产生每世界登记 | L2659 | `同一指针=是；状态定义 2 条、触发行 2 条——查询前后不变` |
| S4 属性账本就绪 | L2672 | `Armor=5.000 / Attack=30.000；施加前 Armor 条目数=0 期望 0` |
| S5 内容链施加建实例 | L2699 | `在册 0→1 期望 +1；Source.Id=3 CascadeAnchor.Id=4 互异=是；Level=1 期望 1=LevelBase`（id 数值随进程前进，判据是**互异**而非定值） |
| S6 内联触发行随实例登记 | L2700 | `效果门面行 2→3 期望 +1 = buff 的 1 条 Applied 行` |
| S7 快照三键 | L2701 | 见其下逐字读数 |
| S8 真资产修正器物化 | L2702 | `Armor 5.000→30.000 净变 25.000 期望 25.000 = 覆盖值；Armor 条目数 1 期望 1` |
| S9 行为链起链 | L2703 | `Attack 30.000→25.000 变 -5.000 期望 -5.000` |
| S10 对照单位零波及 | L2704 | `在册状态 0 期望 0；Armor=5.000 Attack=30.000` |

**S7 是本轮最能自证的一条**（L2701 逐字）：三个键分别是**覆盖键 25.000**（≠定义 10.000、≠兜底 −999.000）、**负对照键 10.000**（未进 `Overrides` ⇒ 保定义值）、**等级键 30.000**（`LevelBase=1` 取数组首元；**装置已把施加者等级设为 2** ⇒ 若该源读 `LevelProvider` 会得 60.000，实测非该值 ⇒ `StateLevelArray` 只读 `Context.EffectiveLevel`）。三值两两可区分 ⇒ **"覆盖是按 `Row.Key` 逐键命中、不是整体替换"** 与 **"跨资产 `ParamRef` 通道真的通了"** 同时成立。负对照键的存在使"覆盖生效"无法用"整表被替换"解释。

### 1.2 竖切延时段（S11–S15，**0.60s / 2.60s 两拍**）

| 检查 | 日志行 | 读数 |
|---|---|---|
| S11 在飞状态与周期回调 | L2710 | `实例仍在册=是；周期回调累计 1 次要求 ≥1；Applied 累计 1 期望 1` |
| S12 到期回收 | L2731 | `实例已摘=是；Expired 累计 1 期望 1；Removed 累计 0 期望 0 = 到期路径而非显式移除` |
| S13 触发行按锚点退订 | L2732 | `效果门面行 3→2 期望回到施加前 2` |
| S14 修正器按锚点摘净、**护甲复原** | L2733 | `Armor 30.000→5.000 期望回到施加前 5.000；Armor 条目数 0 期望 0` |
| S15 行为链条目按设计**常驻** | L2734 | `Attack 30.000→25.000 期望 25.000 = 施加前 30.000 加链减量 -5.000` |

**S14 / S15 是同一裁定（design.md **D10**）的两面读数**，必须成对读：状态修正器挂 `Instance.CascadeAnchor`（`TcsStateModifierMaterializer.cpp:108`）⇒ 到期时随锚点摘净、**护甲回到 5.000**（Slots 1→0）；行为链条目挂 `Context.RunSource`（`TcsStepModifyAttribute.cpp:80`，每次起链新发的运行态句柄）⇒ **不随状态锚点摘除、攻击停在 25.000**。若两者落同一属性，这两个读数会互相掩盖；分落后各自可判。（`Removed 累计 0` 同时证明走的是**到期**路径而非显式移除。）

### 1.3 拒绝面（a–g）

| 检查 | 日志行 | 期望留痕 | 实测 |
|---|---|---|---|
| a 未登记定义被拒 | L2910 | 1 Warning | ✓ `DefTag=EffectChain.Check.Behavior`（**已声明但未登记**——刻意避开"未声明的 tag 本身无效"这个混淆） |
| b 无效目标句柄被拒 | L2912 | 1 Warning | ✓ |
| c 悬空句柄访问 | L2949 | 2 Warning | ✓ 查询面静默无红字 |
| d 未登记身份查询落 nullptr | L2950 | **0 红字** | ✓ 正常查询路径，不 `ensure` |
| e 定义登记两条硬门 | L2953 | 2 **Error** | ✓ 无效身份被拒 + 重复登记被拒（**不静默覆写**） |
| f 无限时值上的时长操作被拒 | L2958 | 2 Warning | ✓ 剩余 0.000→0.000 未被修改 |
| g 未登记链起链被拒 | L2962 | 1 **Error** | ✓ 不 `ensure` |

### 1.4 回归面（§5.4：**按结论回归、按增量核分母**）

| 检查 | 日志行 | 改动前 | 改动后 | 判定 |
|---|---|---|---|---|
| 检查 18 作者侧门 | L3601 | `IsDataValid == Valid 1/1 个` | **`2/2 个`** | 分母 +1（新 buff 资产**也过作者侧门**——收益，已预先在 §5.4① 声明的必然变化） |
| 检查 19a 逐世界登记 | L3602 | `门面内 1 条` | **`门面内 2 条`** | 分母 +1（DA_State_E2E 逐世界登记；已预先在 §5.4② 声明） |
| 检查 0 定义库就绪 | L4488 | `链 9 / 触发 2 / 状态 1 / 失败 0` | **`链 11 / 触发 2 / 状态 2 / 修正器模板 1 / 失败 0`** | 见 §1.5 增量核对表逐项吻合 |
| 检查 20l 周期计数窗口 | L4405 | 72/0 | **72/0** | **未被污染**（未动 `DA_Check_BuffDef`） |

### 1.5 §5.5 增量核对表（逐项对账）

| 计数 | 改动前 | 期望 | **实测** | 增量来源 | 判定 |
|---|---|---|---|---|---|
| 链定义 | 9 | 11 | **11** | +2 = `DA_Chain_E2E_Apply` / `DA_Chain_E2E_Behavior` | ✓ |
| 触发定义 | 2 | 2 | **2** | +0（本轮不新建触发定义资产；内联触发行住 buff 的 `Triggers`，**不进此计数**） | ✓ |
| 状态定义 | 1 | 2 | **2** | +1 = `DA_State_E2E` | ✓ |
| 修正器模板 | （原无此计数） | 1 | **1** | +1 = `DA_ModDef_E2E`（本变更 §2 新增的第四计数） | ✓ |
| 失败 | 0 | **0** | **0** | **非空即判失败** | ✓ |

逐字读数（L4488）：`定义库就绪——链定义 11 条，触发定义 2 条，状态定义 2 条，修正器模板 1 条，失败 0 条`；
装配行：`已装配到世界 Untitled_1——链定义 11/11 条，触发行 2/2 条，状态定义 2/2 条`。

### 1.6 §6.3 复现性：摘要行**逐字相同**

| 轮次 | 日志行 | 摘要 |
|---|---|---|
| 第一轮 | L2736 | `\|S4 Armor0=5.000 Attack0=30.000 Slots0=0\|S5 States=0/1 Decoupled=1 Lv=1\|S6 Rows=2->3\|S7 N=3 Mod=25.000 Ovr=10.000 Lvl=30.000\|S8 Armor=30.000 d=25.000 Slots=1\|S9 Attack=25.000 d=-5.000\|S11 Live=1 PerOK=1 App=1\|S12 Exp=1 Rem=0 Rows=2 Armor=5.000 Slots=0 Attack=25.000\|` |
| 第二轮 | L3199 | **逐字相同**（`-ceq` 为 `True`，§3 脚本 ④ 可复算） |

两轮均为 **15 / 0**（`State.Run` 汇总行 L2735 / L3198）。摘要里**刻意去掉两类按构造就不可能逐字相同的读数**（`tasks.md` §6.3 已同步订正）：

1. **`FTcsSourceHandle.Id` 是进程内单调计数器**——每起一轮 PIE 就前进（实测首轮 `Src=2 Anchor=3`、中间轮 `Src=25/26`、末轮 `Src=48/49`）。它是"分配顺序"的痕迹、**不是被测行为** ⇒ 摘要改记**不变量** `Decoupled=1`（"两 id 互异"）。检查 S5 行里仍逐轮打印真实 id。
2. **周期回调累计次数是帧驱动读数**（到期堆按帧泵点推进），同一窗口内可能是 1 也可能是 2 ⇒ 检查 S11 只断言 `≥1`（**下界，不用等号**），摘要改记 `PerOK=1`（"下界成立"），原始次数留在检查行。

### 1.7 跨 PIE 残留检查（§6.4）

| 判据 | 读数 |
|---|---|
| 停止 PIE 后无残留 | `IsPIERunning` → `{"returnValue":false}`（`StopPIE` 后实测） |
| 重进 PIE 起点归零 | **S1**：`本世界总状态实例 0 期望 0`（L2657 / L3121，每轮 `/Run` 的首条检查） |
| 触发行计数归零 | S1 同时读到 `效果门面触发行 2` = **内容触发行随定义库装配的稳态值**（非残留；两轮同值） |

`Tcs.Test.State.Run` 每轮自带完整 setup/teardown（结束时 `TeardownSliceRigActors()`），故 S1 的 `0/2` 就是"上一世界已退净、本世界只剩内容装配量"的读数。

---

## 2. 区段锚点（可复核）

**运行日志**：`E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\Task7\LegendAutoChess-task7-frozen.log`

- **冻结快照**：4,824 行 / 666,087 字节 / SHA-256 `db630575f8fa6e41daf26dfc80ac0f138a4c92d9b6819e834f9d19931523807f`
- **为什么先冻结再切区段**：本轮日志**同进程跨了五个 PIE 世界**（多轮 `StopPIE → StartPIE`），`Saved/Logs/LegendAutoChess.log` 会被后续编辑会话继续追加 ⇒ 区段行号会漂。故先把整份日志**原样复制**成快照，全部行号 / 哈希以快照为准。
- **⚠️ 本快照与"交付二进制"的对应关系（本轮最要紧的一条元信息）**：本文件的行号与哈希取自**重建后**的那一份（`2:14:42` 的 `UnrealEditor-TcsDev.dll`），即**双配置 UBT 构建产物**。此前一轮取证跑在**Live Coding 打过补丁的进程**上，其快照（`f0b5df27…` / 6,772 行 / 923,048 字节）**已作废**，仅作"修复过程留痕"的历史参考——**判定一律以本快照为准**。踩坑记录见 §4 缺陷 3。
- **`Cmd:` 行清单**（本快照内，共 5 次命令提交，**每次都是定稿态、无中间失败轮**）：L2652（`State.Run` 第一轮）/ L2900（`State.Reject`）/ L3116（`State.Run` 第二轮）/ L3361（`Tcs.Test.Slice.Run` 回归）/ L4592（`Tcs.Test.Slice.Reject` 回归）。**日志文件在编辑器重启时轮转**，故本快照比上一份**更干净**：不含任何"修复中"的失败区段。

| 区段 | 行号（快照内） | 行数 | UTF-8 字节 | SHA-256 | 计数 |
|---|---|---|---|---|---|
| `State.Run` 第一轮 | **L2652–L2736** | 85 | 13,726 | `f4c378290c8d6f7e410527c6d8cd07a68738f498eee70b2c85d9f9f94a2cce4b` | 15 / 0；零 `[FAIL]`、零红字 |
| `State.Run` 第二轮 | **L3116–L3199** | 84 | 13,620 | `1deb56be27c823c98c3f366dbfa2a16ae313770262f28b21d8014b900c6e4bfa` | 15 / 0；零 `[FAIL]`、零红字 |
| `State.Reject` | **L2900–L2964** | 65 | 9,770 | `23887a8f4a50f351aa45c9ce7572a9054a952e42cda9d2cc0de967e8f05ad388` | 7 / 0；故意红字见 §2.2 |
| §5 回归 `Slice.Run` | **L3361–L4412** | 1052 | 153,758 | `3d7a890eba90a6ffbc53a2c9022ff463531df75bea10a04f4c5573f54d2efd62` | 65 / 0 → 72 / 0 |
| §5 回归 `Slice.Reject` | **L4592–L4740** | 149 | 27,265 | `c0f4ef36d962a6fa3fe4e1639b88446c30c340b15519cfd9405a28392ed1a442` | 11 / 0 |

> **两轮 `State.Run` 的行数字节不同（85 vs 84 行 / 13,726 vs 13,620 字节）—— 已定位真因，且它不影响任何断言**：逐行归一化（剥掉时间戳 / 帧号 / 全部数字）后求得唯一差异行 = `LogTcsEffect: FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析（共 10 类）`，它**只出现在第一轮**（L2674）。真因 = **进程内一次性的步骤执行器登记表惰性解析**：该表在**本进程第一次真的执行一条链的步骤**时解析并打一行日志，此后每轮复用（快照内该行仅出现 1 次；同型的 `FTcsFlowStepExecutorRegistry: … 流程步骤执行器登记表已解析（共 14 类）` 也只出现 1 次，见 L3387）。**故这条差异是"进程级首次执行"的痕迹，不是行为差异** —— 两轮的**复现性摘要行逐字相同**（§1.6 的 `-ceq` 判据）、**各检查通过/失败结论相同**（均 15/0），断言面**零差异**。
>
> **这条差异本身是一处判据教训（值得与 §1.6 并列记）**：如果按"区段字节哈希相同"来判复现性，**第一轮必然失败**——因为**进程级惰性初始化**的日志天然只写一次。⇒ **可复现性判据 MUST 落在"摘要行 + 计数结论"上，MUST NOT 落在区段字节上**；凡"首次执行才出现"的初始化日志，都属于**进程生命周期痕迹**，与 §1.6 排除的**进程内单调量**（`FTcsSourceHandle.Id`）同族。

**区段切法与哈希口径**：区段按 `Cmd:` 行到"该轮收束汇总行"**整段原始字节**截取，哈希对截出的**原始字节**（保留原行尾）计算 ⇒ 复算时 MUST 用 `-Raw` 读字节而不是 `Get-Content` 拼字符串（后者会把行尾规范化掉）。

### 2.1 `State.Run` 区段红字 = **0** 条

两轮定稿区段内 `Warning:` / `Error:` 匹配行数**均为 0**。这比常规验收更严：`Run` 命令的设计意图就是**一条非预期红字都不该有**（题头逐字写着"本命令零非预期红字——预期红字 0 条"），且不存在"预期红字白名单"——因为它的施加路径全部走合法输入。

### 2.2 `State.Reject` 区段红字 = 装置头部逐条列明的那一批

`1 条 Warning（检查 a）+ 1 条 Warning（检查 b）+ 2 条 Warning（检查 c）+ 2 条 Error（检查 e）+ 2 条 Warning（检查 f）+ 1 条 Error（检查 g）`，实测与 §1.3 表逐条吻合、`OnScreen` 汇总 `通过 7 / 失败 0`（L2964）。**该命令的意图就是制造红字**，故它**不**纳入"零非预期红字"判据。

---

## 3. 复算脚本（逐字可跑）

```powershell
$frozen = "E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\Task7\LegendAutoChess-task7-frozen.log"

# ① 冻结快照哈希（应 = db630575…）
(Get-FileHash -LiteralPath $frozen -Algorithm SHA256).Hash.ToLower()

# ② 区段字节哈希（应依次 = f4c37829… / 1deb56be… / 23887a8f… / 3d7a890e… / c0f4ef36…）
$bytes = [System.IO.File]::ReadAllBytes($frozen)
$lines = [System.Text.Encoding]::UTF8.GetString($bytes) -split "(?<=`n)"
$sha = [System.Security.Cryptography.SHA256]::Create()
$plan = @(
  @{n='State.Run-1'; s=2652; e=2736}, @{n='State.Run-2'; s=3116; e=3199},
  @{n='State.Reject'; s=2900; e=2964}, @{n='Regress.Run'; s=3361; e=4412},
  @{n='Regress.Reject'; s=4592; e=4740}
)
foreach ($r in $plan) {
  $b = [System.Text.Encoding]::UTF8.GetBytes(($lines[($r.s-1)..($r.e-1)] -join ""))
  "{0}: L{1}-L{2} 行数={3} 字节={4} SHA256={5}" -f $r.n, $r.s, $r.e, ($r.e-$r.s+1), $b.Length,
    (($sha.ComputeHash($b) | ForEach-Object { $_.ToString("x2") }) -join "")
}

# ③ 两轮区段红字（应各为 0 条）
$lines[2651..2735] | Where-Object { $_ -match 'Warning:|Error:|Ensure condition|Fatal error|\[FAIL\]' }   # 应空
$lines[3115..3198] | Where-Object { $_ -match 'Warning:|Error:|Ensure condition|Fatal error|\[FAIL\]' }   # 应空

# ④ 复现性摘要逐字相同（应 True）
$dig = { param($a,$b) (($lines[($a-1)..($b-1)] | Select-String '复现性摘要' | Select-Object -First 1).Line -split '\[复现性摘要\]',2)[1] }
(& $dig 2652 2736) -ceq (& $dig 3116 3199)

# ⑤ 计数（应 15/0 两轮、7/0 拒绝面、65/0 与 72/0 与 11/0 回归）
$lines | Where-Object { $_ -match '状态层竖切汇总|状态层拒绝面汇总|即时判定汇总|段（延迟）收束|拒绝面汇总：通过 11' }

# ⑥ 定义库增量核对表（应 链11 / 触发2 / 状态2 / 修正器模板1 / 失败0）
($lines | Select-String '定义库就绪' | Select-Object -Last 1).Line

# ⑦ 两轮行数差 1 的真因（应只命中 1 行 = L2674；**MUST 用窄 pattern**——
#    宽 pattern '步骤执行器登记表已解析' 会同时命中 L3387 的 Damage 侧同名字段 '流程步骤执行器登记表已解析'）
$lines | Select-String 'FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析'
```

---

## 4. 缺陷与偏差记录

1. **一处内容资产缺陷（本轮自查发现并修复）**：`DA_ModDef_E2E` 的 `Def.Operand.Literal` 是 `FTcsParamSource_ParamRef`，其 `Key` 初值是**探索期笔误**留下的 `Attribute.Armor`（属性根下的 tag），而快照行的键全是 `TcsStateParam.Check.*` ⇒ `Overrides.Find(Row.Key)` 命中不到 ⇒ `ParamRef` 落 **`Fallback = -999.0`** ⇒ 护甲读数 `5.000 + (-999.0) = -994.000`。这正是首轮 S8 `净变 -999.000 期望 20.000` 的成因。**修法 = 把 `Key` 改为 `TcsStateParam.Check.Modifier`**，与 `Params` 第①行、`Overrides` 的键三者对齐（三方同名是 design.md D9 的身份判据）。修后 S8 `5.000→30.000 净变 25.000`。
   **资产写入机制事实**（本轮踩到并写进 §7）：`FTcsParamValue.Source` 是**构造期已初始化**的 `FInstancedStruct`，`JsonObjectConverter` 只在 `!IsValid()` 时才认 `{"_structType":…}`（`JsonObjectConverter.cpp:910-934`）⇒ 对该成员**必须**用 UE 文本格式字符串（`"/Script/TcsCore.TcsParamSource_ParamRef(Key=…,Fallback=-999.0)"`，走 `HasImportTextItem()` 分支 → `InitializeAs` 重置内存）；而裸声明、初值无效的载体（`Steps` 数组元素、`Selector`、`Triggers`）用对象形式即可。**两种载体的写法不同，且写错时前者静默无效、`set_properties` 仍返回 `true`** ⇒ 每次写入 MUST `get_properties` 读回。
2. **一处装置缺陷（本轮自查发现并修复）**：事件面基线（`BaseApplied` 等四槽）原先在 `ExecuteChain` **之后**才取，而施加路径**全程同步**（`ApplyState` 内即 `Broadcast(Applied)` → 触发行求值 → 行为链再入起链）⇒ `Applied` 增量恒为 0，S11 首轮读数 `Applied 累计 0 期望 1` 失败——**而 S9 的 `Attack 30.000→25.000` 已经证明行为链确实起过**（自相矛盾的读数对，这是发现该缺陷的直接线索）。**修法 = 基线一律提前到起链之前取**。修后 S11 `Applied 累计 1 期望 1`。
3. **一处装置判据与规格文本不符（本轮自查发现并修复）**：检查 S8 的期望值原写 `覆盖 25.000 + 行为链 -5.000 = 20.000`，是**行为链落点改为 `Attribute.Attack`（D10）之前**的旧算术；同段的 S14 消息里也残留 `TailArmorBefore + OverrideValue + BehaviorDelta`。两处均改为"护甲面只含状态修正器那一条"（净变 = 覆盖值本身）。**该缺陷只在二进制重建后才消失**——首轮实测一直读到旧文本，说明当时跑的是**陈旧 DLL**（`UnrealEditor-TcsDev.dll` 时间戳 `0:56:59` 早于源码改动 `1:03:47`）。⇒ **改源码后 MUST 重建再取证**，否则读数与源码不对应。
4. **一处摘要口径缺陷（本轮自查发现并修复）**：摘要行原先打印 `FTcsSourceHandle.Id` 与周期原始次数 ⇒ 每轮 PIE 的 id 都前进（`Src=2/3` → `25/26` → `48/49`），使 §6.3 的"逐字相同"**按构造不可能成立**。修法 = 摘要只留不变量（`Decoupled=1` / `PerOK=1`）。**同步订正 `tasks.md` §6.3**（原稿"全部读数逐字相同"过宽）。
5. **一处构建通道事实（本轮实测，非缺陷）—— 最终收口见补充**：编辑器运行期间 UBT 拒绝构建（`Unable to build while Live Coding is active`）。修复过程中改用 `LiveCoding.Compile` 控制台命令迭代（两次，均 `Live coding succeeded`、零 `error C`），使"改 → 编 → 跑"不必反复开关编辑器；但 **Live Coding 不是交付构建**。
   **最终收口（本轮结论）**：缺陷 3 暴露了 Live Coding 的**副作用**——它会让"当前跑的二进制"与"磁盘上最后一次 UBT 构建的二进制"**分叉**，且分叉**无声**（读数看着正常，只是与实际源码不对应）。故收尾时**关掉编辑器、跑完整的双配置 UBT 构建**（Editor Development `7.96s` / Game Shipping `28.16s`，各 `Result: Succeeded`、命中 `warning|error` 的行数**恰 1 条 = `Result: Succeeded` 自身**、Shipping 日志确认 `TcsDevSliceRig_State.cpp` 作为独立 TU 参与编译），**然后重新打开编辑器、在重建后的二进制上把全部验收重跑一遍**（本轮定稿快照 `db630575…` 即该轮取证）。⇒ **本文件全部读数都对应交付二进制**，Live Coding 只用于中途迭代、不用于取证。
6. **一处 MCP 工具面缺口（本轮实测）**：MCP 830 个工具中**没有任何一个**能跑 `IsDataValid`（`ObjectTools` 只有 6 个工具，无校验入口；`DataValidation` / `console|Exec` 搜索无可用结果）。`ProgrammaticToolset.execute_tool_script` 的沙箱**拒绝 `import unreal`**（只允许 `json, math, datetime, copy, re, time`）⇒ **Python 脚本面也调不到 `Object::IsDataValid`**（该虚函数未暴露给 Python）。§3.9 因此改走**编辑器校验子系统**：`EditorValidatorSubsystem.validate_assets_with_settings`，其第一步就是逐资产调 `UObject::IsDataValid`（引擎源码 `EditorValidatorSubsystem.cpp:657`，**无前置条件、必经**）——与装置检查 18 直调 `UTcsBuffDefAsset::IsDataValid` 走**同一个虚函数**，口径一致。
7. **一处判据教训（本轮踩到并订正，非缺陷）—— "首次执行才出现"的日志属于进程生命周期痕迹**：两轮 `State.Run` 的区段行数 / 字节数不同（85 vs 84 行 / 13,726 vs 13,620 字节），逐行归一化后定位到**唯一**差异 = `FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析（共 10 类）`（**只在第一轮** L2674，因该表在本进程**第一次真的执行链步骤**时才惰性解析）。⇒ 若按"**区段字节哈希相同**"判复现性，**第一轮必然失败**。**判据**：可复现性 MUST 落在"**摘要行 + 计数结论**"上，MUST NOT 落在区段字节上；凡"首次执行才出现"的初始化日志，与 §1.6 排除的**进程内单调量**（`FTcsSourceHandle.Id`）同族——**都属进程生命周期痕迹，不是被测行为**。
   附带一条**复算脚本自身的坑**：定位该行时若用宽 pattern `步骤执行器登记表已解析`，会同时命中 L3387 的 **Damage 侧同名字段** `流程步骤执行器登记表已解析（共 14 类）` ⇒ **MUST 带模块前缀**（`FTcsEffectStepExecutorRegistry:`）才是单义匹配。
8. **一处装置驱动通道事实（本轮实测，非缺陷）**：编辑器**重启后 Slate 控件 `ref` 全部重新分配**——上一进程可用的底栏输入框引用（`tb5`）在新进程里指向另一个控件，`FillForm` 返回 `returnValue:false`。**判定活控件的方法 = 探针法**（填一条 `py print('[探针] …')` 再查日志里是否出现 `Cmd:` 与对应输出），实测本进程的活控件是 **`tb3`**。⇒ 每次重启编辑器后 MUST 先探针确认引用，**MUST NOT** 假定引用可跨进程沿用（`task7_pie.ps1` / `regress.ps1` 已按本进程改为 `tb3`）。
   **另一条同族事实**：`Saved/Logs/LegendAutoChess.log` 在**编辑器重启时轮转**（不复用旧内容）⇒ 本快照**只含重启后的五次定稿提交**，比修复过程中的那一份**更干净**（不含任何失败中间态）。⇒ **取证 MUST 在同一次编辑会话内连续完成**，跨重启会把同一份"快照"拆成两个互不相干的文件。
9. **一处本文件自身的缺陷（本轮复核时发现并修正）—— 表格里的"日志行"曾整体偏移一行**：§1.1 / §1.2 的"日志行"列原先把每个检查指向**它的前一行**（S5→L2698 实际是链完成日志、S6→L2699 实际是 S5 行、……直到 S15 全部偏一行），§1.4 的 19a 写成 `—`、20l 指向了 S15 那一段的汇总行。**成因**：这些行号是从**上一份（已作废的）快照**里摘的，重映射脚本按**区段端点**做算术偏移，而两个快照的**区段头尾基准并不相同**（旧快照每轮前面多了一段修复期日志），于是偏移量对"检查行"整体差 1。**修法**：逐个检查**按其内容**（`检查 S\d+` / `检查 [a-g]` / `检查 19a` / `检查 20l` 的正则）在新快照内**重新定位**，不依赖算术偏移。**判据**：**快照换代后，行号引用 MUST 按内容重locate，MUST NOT 按算术偏移搬运**——偏移法只在两个快照的区段切法**逐字同源**时才成立（见 §2 那条"区段字节哈希不同"的教训，两者同源：**跨快照搬运读数必须先验证基准可搬**）。
10. **一处交付面缺陷（本轮收尾复核时发现，**证据本身无错**、错在交付伴随物）—— 提交信息里的检查清单曾按"计划文本 + 既往轮次序列"推断而非读日志**：LAC 提交 `0a4df71` 的正文把"检查 S1–S15"逐条标题列了出来，其中 **11/15 条与实测标题不符**，且 **S10 声称的读数在本轮根本不存在**——该笔写作"S10 修正器挂载定序早于 `Applied`（同订阅者读数 属性变更 < `Applied`）"，而本轮装置源码 / 证据 §1.1 / 冻结快照里 `定序` / `属性变更` / `AttributeChanged` 的**命中数全为 0**（**S10 实测标题 = 「对照单位零波及」**，即对照组 `ControlUnit` 在册状态为 0）。**成因** = 写提交信息时**没有去读日志里真实的 `检查 S\d+` 行**（而那批行就在手边、提取脚本也早已写好），改为按计划 Task 7 Step 2 的措辞与更早轮次的检查编号习惯**推断**——**计划文本描述的是"要验什么"，MUST NOT 当作"实测标题是什么"转述**。**判据**：**任何"检查编号 ↔ 标题"的对照表 MUST 从日志行逐字提取**（`检查 S\d+：`），`plan.md` / `design.md` / 既往提交信息**都不是**该对照表的合法来源。
    **本文件的地位不受影响**：§1.1 / §1.2 的标题本就**逐字取自日志**（编号 ↔ 标题 ↔ 日志行号 ↔ 原文读数 四列齐备、且行号已按内容重定位）⇒ **真值以本文件为准，不以任何提交信息为准**。
    **为什么不重写那笔提交**：改提交信息会改 hash，而**TCS 的 `b0482b2`（已推送）正文正引用 `0a4df71`**（计划 Task 7 落地结果块"散落真相"条）⇒ 改 LAC hash 会让该引用悬空；要修就得再改 TCS ⇒ TCS HEAD 变 ⇒ LAC 的子模块指针又得变 ⇒ **hash 环路，无法收敛**。叠加 `~/.agents/AGENTS.md` 的**跨电脑部署约定**（同一远端可能被另一台机器持有）⇒ **重写已推送历史在本项目一律不做**。处置 = **就地留痕**（本条 + 计划同条）+ 下一笔 LAC 提交信息内附更正表，**零 `force-push`**。

---

## 5. §3.9 资产作者侧校验与字节大小（实测）

| 资产 | 类型 | 字节 | SHA-256（文件） | 校验路由 | 结果 |
|---|---|---|---|---|---|
| `DA_ModDef_E2E` | `UTcsAttrModDef` | 2,829 | `6a0f06b0f25afb5295569864b824afea98449597ff8c2c799189a596f831c61f` | `EditorValidatorSubsystem` | `num_checked=1 num_valid=1 num_invalid=0 num_warnings=0` |
| `DA_State_E2E` | `UTcsBuffDefAsset` | 5,426 | `11ec21bb1060539ca68f472563238200e62716963c68c487e93d24f98b15e517` | 同上 + 装置检查 18 | 同上；检查 18 `IsDataValid == Valid 2/2 个` |
| `DA_Chain_E2E_Apply` | `UTcsEffectChainDef` | 2,894 | `916e42427d8cc84fa040586e598221148d83eeb21957a3a059bc335daec54b74` | `EditorValidatorSubsystem` | `num_checked=1 num_valid=1 num_invalid=0 num_warnings=0` |
| `DA_Chain_E2E_Behavior` | `UTcsEffectChainDef` | 3,023 | `4d8ff956ac36ca59dd1219bada8ef76a7b66c73c4637ec135b2bdcb8f24293a0` | `EditorValidatorSubsystem` | `num_checked=1 num_valid=1 num_invalid=0 num_warnings=0` |

复算脚本（逐字可跑）：

```powershell
# 资产字节与文件哈希
Get-ChildItem 'E:\Projects_Dev\LegendAutoChess\Content\TcsDev\E2E' -Filter '*.uasset' |
  Sort-Object Name | ForEach-Object {
    "{0}|{1}|{2}" -f $_.Name, $_.Length, (Get-FileHash $_.FullName -Algorithm SHA256).Hash
  }

# 作者侧校验（编辑器控制台；脚本 = Saved/TcsAcceptance/Task7/validate_assets.py）
#   py "E:/Projects_Dev/LegendAutoChess/Saved/TcsAcceptance/Task7/validate_assets.py"
# 期望逐资产：失败/警告数=0 (期望 0)，且 results.num_valid=1 / num_invalid=0 / num_warnings=0
```

> **口径说明（诚实标注）**：上表是**校验结论**，不是"逐条规则都被触发过"的证明——四个资产都是**干净资产**，所以走的是"零错误 / 零警告"分支；各资产 `IsDataValid` 里的**具体规则分支**（如 `DA_ModDef_E2E` 的"字面量缺数值来源"、"非覆盖带填 OverridePriority"）**本轮未被逐个构造**。装置检查 18 对 buff 资产的 `2/2` 是运行期独立复读的同款结论。

---

## 6. 如实边界清单（未覆盖面，逐条）

1. **单机 / 单进程 / 单 PIE 会话**：全部读数来自**同一个编辑器进程内的多次 PIE 会话**。无网络、无多世界并行、无 Mass 路径、**无 Shipping 运行期**验证（Shipping **只验编译**，见 §7）。
2. **等级源只验 `StateLevel*` 两型中的数组型**：本轮内容资产的等级行走 `FTcsParamSource_StateLevelArray`（`Values=[30,60]`、`Fallback=-1`），实测取 30.000。**`FTcsParamSource_StateLevelMap` 型零读数**；`FTcsParamSource_InstigatorLevelArray` / `_Map` 两型本轮**未被内容资产消费**（它们由既有检查 20b 覆盖，本轮不重复声明）。
3. **`LevelProvider` 读口本轮未被消费**：这是**易混点**——消费 `LevelProvider` 的是 `InstigatorLevel*` 两型；本轮内容资产的等级行是 `StateLevelArray`，该源按设计**只读 `Context.EffectiveLevel`**（S7 的 30.000 vs 装置等级 2 会给的 60.000 就是这个区分的读数）。两者不是同一件事。
4. **`ResolveAttrModDef` 运行期调用者为零**：本变更交付的是**身份解析与索引**（外加 S3 的"同一指针"读数），**不交付**"按 tag 取模板供物化"。物化点今天读的是状态实例自己的 `ModifierRows`（`TcsStateModifierMaterializer.cpp`），**尚未**经 `ResolveAttrModDef` 反查模板 ⇒ 该新 API **只有装置在调**。
5. **`AttrModDef` 根下的词尚只有 `Check` 形态一个**：正式内容词（非 `Check`）未出现 ⇒ 该根的**代表词覆盖极薄**，其词表形态尚无第二例可校。
6. **模板的运算数配成"等级源"的情形未验**：`Def.Operand.Literal.Source` 若换成 `StateLevelArray` 之类，物化点的取值路径未覆盖（本轮配的是 `ParamRef`）。
7. **内容资产 `Fragments` 留空的理由（D2）**：`Fragments` 是 `FInstancedStruct` 数组，而 `FTcsDevBehaviorSample` 住在 `Source/TcsDev/`（**切片侧**）⇒ 内容资产引用切片类型会在**切片退役后静默失效**（类型身份靠包导入表索引，`TcsSelSelf.h:19-22` 记录了这条纪律）。故**内容 buff 的 `Fragments` 必须留空**，行为片段路径**本轮不在内容路径上覆盖**（既有检查 23i–23l 覆盖的是运行期手搓定义那一路）。
8. **链挂的账本条目按设计常驻（实测项）**：由"护甲回到 X、攻击停在 X−5"两个读数共同证明。**没有任何框架侧回收触发点**——三条可能路径逐一核实为死路：① 全即时链的运行态在 `ExecuteChain` 返回前已释放（`TcsEffectSubsystem_Run.cpp:104-105`）⇒ 无"随运行结束"窗口；② 按因果边（`CausedBy`）级联回收**已被裁定③明文禁止**；③ `FTcsStepModifyAttribute` **刻意不提供**回收器。今天唯一合法用法 = 宿主自存 `RunSource` 再自调 `RemoveBySource`（该口公开且已有真实使用者 `TcsDamageSubsystem.cpp:226`，但**链原语路径零调用**）⇒ 登记为延后项 **`CHAIN-7`**。
9. **`FTcsStepModifyAttribute` 不等同 GAS Instant**：它写 `ModifierSlots` 而**不写 `BaseValue`**；会被任一 `TAO_Override` 整体抹掉（`TcsAttributeBandFold.h:161-164`：`bHasOverride` 直接 return，全部 `Add` 被丢弃）、参与 `PercentAdd`/`Mul` 复利、且可按来源摘除 ⇒ 效力与"直接改基础值"**不等同**。GAS Instant 的等价物是 `SetBaseValue`，其唯一真实调用方是伤害流程步（`TcsFlowStepsCore.cpp:202`）⇒ 登记为 **`CHAIN-8`**（**不并入 `DAMAGE-2`**）。
10. **行为片段的"刷新路径"未正面覆盖**（沿用 6b 边界 2）：本轮内容 buff 的 `Fragments` 为空，刷新语义不在内容路径上。
11. **空载荷 / 非派生片段的运行期 Warning 路径零读数**（沿用 6b 边界 3）。
12. **`Expired` 路径的行为退订无专门读数**（沿用 6b 边界 6）；但**触发行的到期退订**本轮**有**读数（S13）。
13. **未绑定全局触发行与外部无 `Subject` 事件的兼容性**：靠**代码评审 + 既有整轮回归**（检查 8–16 全 PASS），**无**专门场景（沿用 6b 边界 4）。
14. **typed 自定义载荷 + 自定义 reader 给出 `Subject` 的情形**：不做泛化 `Subject` 过滤，无自定义载荷场景读数（沿用 6b 边界 5）。
15. **`Interests` 层级匹配不做**（精确 Tag 匹配）；**脚本可达面为零**（虚分派在脚本侧物理不可达，归 `LEDGER-reflection` R-2 一族）。
16. **任意递归增删的终止性不承诺**：`STAT-9`（终止移除广播窗口内同定义重施）**刻意未修**，本轮登记不修（沿用 6b 边界 7、`STAT-9`）。
17. **`PeriodRefresh` 只验 `Keep`**；`EDP_Finite` 且"真的在测试窗口内到期"是**本轮首次覆盖**（此前 Task 3/4 用 3600s 刻意不出期）——S12 的 `Expired 累计 1` + `实例已摘=是` 即该覆盖的读数。
18. **真实资产等级类参数行仍为 0 行**（沿用 Task 3/4 结转）——本轮唯一的内容等级行就是 `TcsStateParam.Check.Level` 的**数组型**；`LevelMap` 型零读数。
19. **`ExtendDuration` 的上界未定**（沿用 Task 3/4 结转）。
20. **`STAT-9` 未修**（同上，属"已登记不修"而非遗漏）。

---

## 7. 引擎 / 机制事实（本轮实测或源码核实，供后续轮）

1. **`FInstancedStruct` 的 JSON 反序列化两条路 != 等价**：`{"_structType":…}` 只在**载体当前无效**时才被认（`JsonObjectConverter.cpp:910-934` 用 `if (!InstancedStruct->IsValid())` 把整段包住）；而 UE 文本格式字符串走 `HasImportTextItem()` 分支（`JsonObjectConverter.cpp:1054`）→ `FInstancedStruct::ImportTextItem`（`InstancedStruct.cpp:309-367`）→ **`InitializeAs`**（重置内存）。⇒ **构造期已初始化的载体**（如 `FTcsParamValue.Source`，其默认类型是 `Literal`）用对象形式会**静默无效且 `set_properties` 仍返回 `true`**；此时 MUST 用 `"/Script/<Module>.<Struct>(Field=Value,…)"` 字符串形式。`"None"` / `"()"` ⇒ `InitializeAs(nullptr)`。
2. **`FTcsParamSource_ParamRef` 的 miss 语义是 `Fallback`**：`TryGetNumericParam` 失败即返回 `Fallback`（`TcsParamSource_ParamRef.h:41-51`）⇒ **键写错不报错、只静默取兜底值**。诊断手法 = 让"覆盖值 / 定义值 / 兜底值"三者**两两不同**（本轮 25 / 10 / −999），则读数落在哪一支一目了然；负对照键再加一层"不是整体替换"的判别。
3. **`FTcsSourceHandle.Id` 是进程内单调计数器、跨世界不重置**：故**任何摘要 / 断言 MUST NOT 拿它当"可复现读数"**——它每轮 PIE 都前进。可复现的替代判据是**不变量**（如"两 id 互异"）。
4. **帧驱动的累计计数 MUST 用下界而非等号**：周期回调按帧泵点推进，同一时间窗口内次数随帧率浮动（本轮实测 1 次，但 `tasks.md` §3/§4 的既有纪律要求写 `≥1`）。摘要行若含原始次数，则"两轮逐字相同"这类判据会被帧率噪声打破。
5. **同步施加 + 事件再入 ⇒ 事件计数基线 MUST 在起链前取**：`ApplyState` 在**同一次调用内**完成"建实例 → 广播 `Applied` → 触发行求值 → 行为链起链"，若基线在起链后才取，`Applied` 增量恒为 0（§4 缺陷 2 的反面教材）。**这条对一切"同帧内既有起因又有结果"的计数判据通用。**
6. **`EditorValidatorSubsystem.validate_assets_with_settings` 是脚本面唯一能触达 `UObject::IsDataValid` 的通道**：引擎源码 `EditorValidatorSubsystem.cpp:657` 无条件首调 `IsDataValid`；而 `Object::IsDataValid` **未暴露给 Python**（`hasattr(asset,'is_data_valid')` 实测 `False`）、MCP 830 工具中也无校验入口、`execute_tool_script` 沙箱禁 `import unreal`。⇒ 需要"逐资产作者侧校验"的验收，**只能**走这个子系统（`ValidateAssetsSettings` 需开 `load_assets_for_validation`，并逐个资产单独调一次以便归因）。
7. **Live Coding 可用于"改 → 编 → 跑"快速迭代，但不是交付构建**：`LiveCoding.Compile` 控制台命令在本轮两次成功（零 `error C`），免去反复开关编辑器；但**Live Coding 激活时 UBT 直接拒绝构建**（`Unable to build while Live Coding is active`）⇒ 交付构建前 MUST 关编辑器。**且 Live Coding 会掩盖"跑了陈旧 DLL"**：一旦忘记重编，读数与源码不对应（§4 缺陷 3 即此）。
8. **UE 控制台的 `py` 命令支持传 `.py` 文件路径**（`ExecPythonCommandEx` 检测 `.py` 后缀，`PythonScriptPlugin.cpp:814-828`）；但**脚本不会自动执行 `run()`**——文件里若只定义函数而不在末尾调用，命令**静默成功且零输出**（本轮实测：`Cmd:` 行出现、无任何 `LogPython`）。⇒ 脚本 MUST 自带顶层调用。

---

## 附：本轮交付面（口径索引）

- **规格口径**：新能力 `state-layer-e2e-validation`（4 ADDED）；`gameplay-tag-governance` 3 MODIFIED（**根段注册表 10 → 11**）；`integration-entity` 1 MODIFIED；`attribute-types` 1 ADDED
- **提案**：`verify-state-layer-e2e`（**1 新能力 + 3 改能力**）——本文件是其验收证据，归档名 `2026-10-05-verify-state-layer-e2e`
- **内容资产（本变更首次交付真资产）**：`Content/TcsDev/E2E/` 下 4 个 `.uasset`（`DA_ModDef_E2E` / `DA_State_E2E` / `DA_Chain_E2E_Apply` / `DA_Chain_E2E_Behavior`），字节与哈希见 §5
- **插件侧实现**：`TcsDefinitionSubsystem.{h,cpp}` + `TcsDefinitionSubsystem_AttrModDef.cpp`（`ResolveAttrModDef` 身份解析与索引 = 第四计数"修正器模板"）
- **宿主装置**：`Source/TcsDev/Public/Dev/TcsDevSliceRig_State.h`、`TcsDevSliceRig_Internal.h`、`Source/TcsDev/Private/Dev/TcsDevSliceRig_State.cpp`（检查 S1–S15 + a–g）、`TcsDevSliceRig.cpp`（挂接 + 检查 18）
- **台账**：`ATTR-1` **闭合**（含残留边界）、Task 4 边界① 闭合；新增 **`CHAIN-7`**（链挂账本条目无框架侧回收触发点）与 **`CHAIN-8`**（链原语缺"直写基础值"），两条归属均 = 触发条件
- **取证脚本**：`Saved/TcsAcceptance/Task7/`（`task7_pie.ps1` 两轮竖切、`regress.ps1` §5 回归、`validate_assets.py` 作者侧校验、`call.ps1` + `args_*.json` 资产制作）
