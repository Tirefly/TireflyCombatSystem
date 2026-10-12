# R6.5-1 参数化批次取证（技能数值字段换参数源载体）

> 计划 `PLN-R6.5-1` 的追加批次（同日 Step 10~11）。**两条路线（无头 + 编辑器 PIE）均已取证**，
> 且**同一基线 DLL、无任何热补丁参与**。

- **文档 ID**：`EVID-2026-10-12-skill-numeric-param`
- **类型**：EVID / 证据
- **状态**：ACTIVE（双路线已取证，逐字一致）
- **权威范围**：`R6.5-1` Step 10~11（台账 `COST-1` / `COOLDOWN-1` 闭合 + `COST-2` 新增）

---

## 0. 交付二进制与源码基线

| 项 | 值 |
|---|---|
| 引擎 | `E:\UnrealEngine\UE_5.8` |
| 配置 | Development Editor + Game Shipping，**均 `Result: Succeeded`、0 warning / 0 error** |
| 基线 DLL | `UnrealEditor-TcsDev.dll` `16:30:27` / `UnrealEditor-TcsSkill.dll` `16:30:22`（**均晚于各自最新源码**） |
| 热补丁 | **零**（`*.patch_*.exe` 已清空、重建前即删除） |
| 命令 | `Tcs.Test.CooldownCost.Run` / `.Reject` / `Tcs.Test.SkillLayer.Run`（R6 回归） |

**★ 取证纪律的一次实际兑现**：本批**先构建、后取证**——两配置全量 UBT 写回基线 DLL 后才重开编辑器，
且重建前删除了陈旧 `patch_*.exe`。这直接沿用了本档同族证据
（`EVID-2026-10-11-r65-cooldown-cost` §1.1）记下的教训：**`UnrealEditor-Cmd` 与 PIE 共用同一个基础 DLL**，
故"重建了 Shipping"**不代表**无头路线的装置换了新代码。

---

## 1. 读数汇总

| 路线 | 命令 | 读数 | 复现性 |
|---|---|---|---|
| **无头** | `Tcs.Test.CooldownCost.Run` | **立即面 24/0**；**含跨帧尾段 27/0** | **零 FAIL** |
| **无头** | `Tcs.Test.CooldownCost.Reject` | **7/0** | **零 FAIL** |
| **无头** | `Tcs.Test.SkillLayer.Run`（R6 回归） | **24/0** | `[复现性摘要]` **与冻存基线逐字相同** |
| **编辑器 PIE** | `Tcs.Test.CooldownCost.Run` | **立即面 24/0**；**含跨帧尾段 27/0** | **与无头逐字一致** |
| **编辑器 PIE** | `Tcs.Test.CooldownCost.Reject` | **7/0** | 同上 |
| **编辑器 PIE** | `Tcs.Test.SkillLayer.Run`（R6 回归） | **24/0** | 摘要同基线 |

**分母演变（记此防误读）**：本批**新增 `J0`~`J5` 六条判据**
⇒ `Run` 立即面 **18/0 → 24/0**、尾段 **21/0 → 27/0**；`Reject` 恒 **7/0**（未动）。

**R6 回归摘要（PIE 侧，与基线逐字相同）**：

```
[复现性摘要]|C1 Entries=1/1 Own=1 Foreign=1 Cmp=1 Int=0|C2 Entries=1/0/0 Restored=1 Int=1 Cmp=0|C3 Entries=1/1/0 OldGone=1 NewLive=1 Int=1 Cmp=0|D1 Ctrl=0/0|
```

---

## 2. 新增判据 `J0`~`J5`（本批的能力面）

| # | 判据 | 实测读数 |
|---|---|---|
| `J0` | **夹具自证**：`Cost.Amount` 载体求值 = 配置基数 | `30.000`（**若为 0 则 J1~J4 全恒真**——这条是防"空夹具给假绿"的自证） |
| `J1` | **百分比减免**生效 | `24.000`（`30 × (1 − 0.2)`；来源 = `TAO_PercentAdd = -0.2`） |
| `J2` | **固定值减免**在百分比**之后**叠加 | `19.000`（`24 − 5`）；**读到 20.000 即次序错** |
| `J3` | 减免过度 ⇒ 钳 0 且判恒可付 | `0.000` + 可付 `true`（"免费"是合法配置结果） |
| `J4` | 减免按来源级联摘除后回到基数 | `30.000`（三条同源 ⇒ 一次摘净） |
| `J5` | 冷却侧 `Track.Duration` 载体求值已接上 | `Total = 0.5000`（`1.0 × (1 − 0.5)`）；**读到 0.0000 即载体没接上** |

**`J2` 为什么必须验"次序"**：`FlatAdd` 在 `FoldTcsAttributeBands` 里排在 `PercentAdd` **之后**
（`((B + ΣAdd) × (1 + ΣPct) × ΠMul) + ΣFlat`）。若实现把平加算在缩放之前，
读数会是 `(30 − 5) × 0.8 = 20.000`——**同一个"减免生效了"的表象，两种截然不同的算术**
⇒ 只验"数值变了"抓不到它。

---

## 3. ★★ 本批最值钱的产出：一处**真实设计缺陷**（行为级取证才抓到）

### 3.1 现象

**装置拒绝面检查 `d` 当场 FAIL**：

```
[FAIL] d 负 Amount ⇒ 恒不可付（`Base - (-50)` = 倒给资源，属漏洞形态；返回=true 期望 false）
```

**注意方向**：它**从"正确拒绝"翻成了"错误放行"**——即**放宽**了安全性。

### 3.2 根因

初版把"折叠后钳 `[0, +∞)`"当成**唯一**收口，于是**两件语义完全不同的事被压成同一个值**：

| 情形 | 语义 | 应有处置 |
|---|---|---|
| **负基数**（作者把 `Amount` 填成 `-50`） | **配置错误**、"倒给资源"的**漏洞形态** | **拦住** + `Warning` |
| **折叠后转负**（基数 30、减免 `-100`） | **减免过度** = 合法的"免费" | **钳 0**、**不报错** |

⇒ 负基数被钳成 `0` ⇒ `CanAffordCost` 的 `EffectiveAmount < 0` 守卫**永远不触发**
⇒ 检查 `d` 读到 `true`。

### 3.3 修法

`ResolveEffectiveCost` 增 **`OutBaseAmount`** 出参（输出**未钳的基数**）：

- **`CanAffordCost`** 与 **`CommitCost`** 的负值守卫改判 **`BaseAmount < 0`**（未钳的基数）；
- 而 `EffectiveAmount`（钳后）继续承担"是不是 0 / 扣多少"。

**两处都要改**：`CommitCost` 可被**独立调用**（`PayCost` 公开口 / 装置验边界），
缺守卫时负基数会**照扣**（`Base - (-50)` = 倒给资源）。

### 3.4 为什么这条值得单列（方法论）

**该缺陷编译通过、代码阅读也通过**——我写下初版时自认为"钳 0 更稳"（防负扣减听起来是加固）。
**只有行为级取证当场报出。** 这与本档同族证据记的 `C6` 恒真检查、`I2` 假红灯是**同一族**
（"装置要能证伪"的实兑），但**方向相反**：这次装置抓的是**我引入的新缺陷**，
而不是"装置自己写错了"。

**⇒ 判据（MUST）**：**"钳制"这类收口 MUST 先问"它抹掉的那部分信息有没有别的消费者"**——
本处"未钳的基数"就是被抹掉后仍被需要的信息。

---

## 4. 一处"文档先行、代码滞后"的如实登记

`SPEC-04-skill` §3.5 的冷却轨道字段**早在 2026-09-11 的 PV 系列换型时就写成
`Duration(FTcsParamValue)`**，而**代码停在 `double Duration = 0.0`**
（`R6.5-1` 主批按"只落字面量"实施，并**如实登记**为台账 `COOLDOWN-1`）。

**⇒ 本批不是"给设计加能力"，而是"把代码补到设计早就写下的形态"。**

**教训**：**类型形状的声称也会文档与代码分叉**，而 `openspec validate`
**不校验类型与文档是否一致**（它校验结构、计数与 scenario 覆盖）⇒
只有**实现批次的逐处核对**能抓到这一类。

---

## 5. 取证边界（如实，MUST NOT 读成"已全面验证"）

1. **两条路线均为单机、单进程**：无网络、无多世界并行、无 Shipping 运行期（Shipping 仅验编译）。
2. **`J1`~`J4` 用的是"宿主施加修正器"路径**（`ApplyParamModifiers`），
   **未验**"把减免写在定义自带 `ParamChainRows` 里"那条路——那受**物化时序**约束，
   属同族证据记的那条边界（本批**未改**物化时序）。**如实登记：该路径今天仍不可见。**
3. **"按等级配置数值"只验了机制可达**（`FTcsParamSource_StateLevelArray` 读基类
   `EffectiveLevel`，而技能侧三处求值点都已填它），**未构造逐等级的端到端读数**
   （需要造多个等级的同一技能并比读数）。**⇒ 该能力是"机制成立 + 静态核对"，不是"行为实测"。**
4. **多资源（成本组）未做**，已入台账 `COST-2`；`ECP_CustomFragment` 仍具名未实现（~~阻塞 `SCRIPT-9`，归属不变~~ —— **★ 2026-10-12 改正：该归因不成立，阻塞 = `COST-3`（扩展点形状未裁）。本行原文保留作现场记录**，MUST NOT 据旧字面派活；改正判据见 `LOG-DECISIONS` 第十九轮）。
5. **`STAT-11` 未动**：它追的是 `FTcsParamSource_AttributeScaled` 的 `ITcsAttributeProvider` 零实现，
   与"字段有没有载体"是**另一触发面**。

---

## 6. 复算脚本（本机逐字可跑）

```powershell
# 无头（每条命令一个进程；-ABSLOG 用绝对路径，相对路径会静默不生成）
& "E:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
  "E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject" `
  /Game/TcsDev/L_TcsDev_Slice -game -nullrhi -nosound -noPause -unattended `
  "-ABSLOG=E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\R65\X-Run.log" `
  "-ExecCmds=Tcs.Test.CooldownCost.Run"

# 从日志复算（编辑器持锁时 MUST 用 FileShare.ReadWrite）
& "E:\Projects_Dev\LegendAutoChess\Saved\McpProbe\readlog.ps1" -Match 'SUMMARY (Run|Reject)|\[(PASS|FAIL)\] J\d'
```

**期望读数**：`Run` 立即面 `24/0` + 尾段 `27/0`、`Reject` `7/0`、R6 回归 `24/0`。
