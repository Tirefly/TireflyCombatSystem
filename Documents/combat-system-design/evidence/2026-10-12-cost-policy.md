# EVID-2026-10-12-cost-policy — R6.5-CC 消耗策略化验收

- **文档 ID**：`EVID-2026-10-12-cost-policy`
- **类型**：EVID / 验收证据
- **权威范围**：`PLN-R6.5-CC`（消耗策略化收窄轮）的**验收读数与边界**；本轮把消耗从"档位枚举 + 写死扣减"改为**策略模式**并补**脚本面宿主插槽**。**不在本文**：冷却侧既有读数（见 `EVID-2026-10-11-r65-cooldown-cost`）、参数化批次（见 `EVID-2026-10-12-skill-numeric-param`）。
- **最后更新**：2026-10-12

---

## 1. 本轮改点与落地形状

| 项 | 内容 |
|---|---|
| **动机** | 用户 2026-10-12 主张："`CanAfford` 的函数重写，这个不应该由 TCS 框架内部写死，应该允许扩展；`CommitCost` 同理"；并进一步改点为**策略模式**（"感觉也不需要 `ETcsCostPolicy`，直接用策略模式就好"、"TCS 自带一个 `ResourceAttr`，但也遵循这套方案"） |
| **时机枚举** | `ETcsCostTiming` → **`ETcsSkillCostTiming`**，加 `ECT_None`（"不消耗"的**唯一**入口，默认档）⇒ 取代原 `ECP_None` 枚举档 |
| **冷却侧连带** | `ETcsCooldownTiming` → **`ETcsSkillCooldownTiming`**（裁定 R12：两侧同批加域前缀保持对称；**只改类型名、值不动**） |
| **配置类型** | `FTcsCostConfig`（`Policy` 枚举 + `Timing` + `ResourceAttr` + `Amount`）→ **`FTcsSkillCostConfig`**（`Timing` + `CostMode: FInstancedStruct`） |
| **策略族** | `FTcsSkillCostPolicy`（`USTRUCT` 虚分派基类，**中性默认 + `meta=(Hidden)`**，虚析构）+ `FTcsSkillCostPolicy_ResourceAttr`（**内建也走同一套**）+ `FTcsSkillCostPolicy_HostDelegate` / `ITcsSkillCostPolicyHost`（脚本面） |
| **删除** | `ETcsCostPolicy` 三档（`ECP_None` / `ECP_ResourceAttr` / `ECP_CustomFragment`）**整体退役**，原字面保留作痕迹并逐条交代去向 |
| **行为** | `CanAfford` / `CommitCost` 从"写死在 `FTcsCostOps`"改为**分派给策略**；`Timing == None` ⇒ 不看策略；`Timing != None` 而策略空 ⇒ **fail-closed** |
| **原子性** | 框架把策略的提交包进 `BeginBatch` / `Commit`（用户裁定推翻旧禁令） |

---

## 2. 读数（无头，`UnrealEditor-Cmd`）

| 命令 | 读数 | 与冻存基线比对 |
|---|---|---|
| `Tcs.Test.CooldownCost.Run` | **立即面 28/0 + 尾段 31/0** | 基线 24/0 + 27/0；**本批新增 K1/K2/K3/K5 四条**；**零 verdict flips、零 FAIL**（基线 27 项逐条仍在） |
| `Tcs.Test.CooldownCost.Reject` | **7/0** | 与基线同分母；`g` 的**断言取反**（见 §3） |
| `Tcs.Test.SkillLayer.Run`（R6 回归） | **24/0** | `[复现性摘要]` 与基线**逐字相同**（`digest identical: True`） |

**编译**：`UnrealEditor` 与 `UnrealGame` 双配置均 **0 error / 0 warning**。

**UHT 产物核对（防"假字段"，逐项实查 `.gen.cpp`）**：
- 新反射类型 4 个：`FTcsSkillCostPolicy` / `FTcsSkillCostPolicy_ResourceAttr` / `FTcsSkillCostPolicy_HostDelegate` / `FTcsSkillCostConfig`；
- `NewProp_Timing`（**EnumProperty**，指向 `Z_Construct_UEnum_TcsSkill_ETcsSkillCostTiming`）、
  `NewProp_CostMode`（`FInstancedStruct`，带 `{"BaseStruct","/Script/TcsSkill.TcsSkillCostPolicy"}` 元数据）、
  `NewProp_ResourceAttr`、`NewProp_Amount` —— 全部**真反射字段**；
- 基类带 `{"Hidden",""}` 元数据（picker 不可选基类）——按设计；
- 冷却侧 UHT 输出已是 `Z_Construct_UEnum_TcsSkill_ETcsSkillCooldownTiming`（旧名仅存于我写的改名留痕注释里）。

---

## 3. `COST-3` 闭合的**硬判据**（本档最值钱的一条）

台账 `COST-3` 追的是"宿主的 `CanAfford` / `CommitCost` 能不能替换框架写死的实现"。
**只验"返回值对"是不够的**——那分不清"宿主策略被调到"与"框架走了别的分支还恰好同值"。

⇒ 装置加了 `UTcsDevR65HostCostPolicy`（最小宿主策略），并**故意让它返回 ≠ 配置的扣减量**
（`GR65HostCostAmount = 7.0`，配置基数是 30）：

```
[PASS] K1 宿主策略 `CanAfford` **被真实调用**（转发器装宿主=是；可付=true；调用计数=1 期望 ≥1）
[PASS] K2 宿主策略 `CommitCost` **被真实调用**且**扣减量由策略决定**
       （提交=true；调用计数=1 期望 ≥1；实扣 7.000 期望 7.000——若读到配置值则证明框架没走策略）
[PASS] K3 转发器空 Host ⇒ **fail-closed**（可付=false；提交=false；实扣 0.000；
       且宿主计数未增=是——MUST NOT 免费放）
[PASS] K5 策略提交后**无残留批**（PeekPending=40.000 须等于 EvaluateCurrent=40.000）
```

**K2 的 7.000 是本档的关键读数**：它**不等于配置的 30**，故只可能来自**策略的实现**。

---

## 4. 本批的一处**行为收紧**（`g` 判据取反）

**换型前**：`ECP_CustomFragment` 档是"具名降级"——`CanAffordCost` 恒 `true` + `Warning`、
`CommitCost` 零扣减 ⇒ **配了自定义消耗的技能免费放**。

**该形态与同一文件自己的判据冲突**（`TcsCostOps.cpp` 的"属性门面不可得"一节原文）：

> 资源都没法查就放技能，等于免费放——那是"**配置了消耗却静默不生效**"的最坏形态

⇒ **同一个失败类别，一处拦住、一处放过。**

**换型后**：真实现取代降级；且"`Timing != None` 而 `CostMode` 为空"改为 **fail-closed**。
装置拒绝面 `g` 的断言**逐字取反**，即这次收紧的行为级证据：

```
[PASS] g `Timing!=None` 而 `CostMode` 空 ⇒ fail-closed
       （可付=false 期望 false；提交=false 期望 false 零扣减，实扣 0.000）
```

**MUST NOT 与 `CQM_Custom` 的降级类比**（一处真实差异）：后者回退 `DefSwitches`
= **一个有定义的行为**；本处回退 = **免费**，**不是一个安全的默认**。

---

## 5. ★ 实施期纠正的两处**我自己的判断**（MUST 留痕）

### 5.1 我反对"宿主可替换内建档"（B 案）的理由**不成立**

**我此前的论证**：让宿主改写内建 `ResourceAttr` 路径会打破"唯一实现 / TOCTOU 不变量"，
并制造"第二真相"。

**该论证混淆了两件事**：

| | 我当时说的 | 真正的不变量 |
|---|---|---|
| 内容 | "全局只有**一份函数实现**" | "门禁与提交对**同一次施法**用**同一个权威**" |

策略模式**完整保留**后者（门禁⑤ 与提交点在**同一个策略对象**上调用）。
而"第二真相"那条**反过来指向**：把内建写死、**同时**另开一条自定义路径，才是**两条会分叉的路**。
⇒ 用户裁定"内建也走同一套"**恰恰消掉了**它。

### 5.2 我关于 `BeginBatch` 原子性的论断**有一半是错的**（源码核实）

**我此前的论证**：用 `BeginBatch`/`Commit` 包成本可同时给"全有全无"与"防重入"。

**源码核实**（`TcsAttributePipeline_Cascade.cpp:96-104` / `TcsAttributePipeline.cpp:122-158`）：

```cpp
// SetBaseValue：写入是【立即】的，批【不】推迟它
Instance->BaseValue = NewBaseValue;
Instance->bDirty = true;
if (Store->BatchDepth == 0 && PushEvalStack(Attribute)) { Recalculate(...); }  // 仅"重算+广播"被推迟
```

| 批**确实**给的 | 批**不**给的 |
|---|---|
| ① 重算与广播推迟到最外层 `Commit`（`BatchDepth` 嵌套计数） | ❌ **回滚 / 撤销写入** |
| ② 订阅者**看不到中间态** ⇒ **重入窗口关闭** | |

⇒ **"全有全无"不能只靠批**，必须配"**先判后扣**"（先全判 `CanAfford`，全过才进批扣）。
**MUST NOT** 把批当作事务/回滚来依赖。该边界已写进 `TcsCostOps.h` 与
`ITcsSkillCostPolicyHost::CommitCost` 的注释（"MUST 自己先全判后扣"）。

**顺带确证**：`BeginBatch` / `Commit` **都没有 `UFUNCTION`**
（`TcsAttributeSubsystem.h:285` / `:288`，全文件仅 10 处 `UFUNCTION`）
⇒ **脚本宿主根本调不到批** ⇒ "框架包住"不只是便利，**是让脚本宿主也拿到该保障的唯一途径**。

---

## 6. 实施期踩坑（MUST 留痕，防复犯）

| # | 坑 | 症状 | 判据 |
|---|---|---|---|
| 1 | **`UCLASS` MUST 住头文件** | `C2143 syntax error: missing ';' before '<class-head>'` | 类住 `.cpp`（或匿名命名空间）时 **UHT 不为其生成反射代码**。本轮**两次**命中（先住匿名命名空间、后住 `.cpp`）⇒ 装置用的宿主类已移进 `TcsDevSliceRig_CooldownCost.h` |
| 2 | 装置 file-local 符号撞名 | `C2084 已有主体`，且报错落在"本次没改的文件" | unity 合并把同模块多个 `.cpp` 并进**同一 TU** ⇒ 匿名命名空间随之合并 ⇒ 一律带装置前缀（`GR65`） |
| 3 | **无头进程残留锁 DLL** | `LNK1104` + `UbaSessionServer - ERROR opening file ... used by another process` | `UnrealEditor-Cmd.exe` 在 `-game` 下**不自行退出** ⇒ 取证完 MUST 显式 `Stop-Process`，否则下一次链接必失败（**不是代码问题**） |

---

## 7. 边界（MUST NOT 外推）

1. **单机 / 单世界 / 单进程**；全部读数为无头 `UnrealEditor-Cmd`。
2. **脚本（C#）侧宿主策略未实测往返**——本轮验的是 **C++ 侧**宿主策略
   （`UTcsDevR65HostCostPolicy` 覆写 `_Implementation`）。
   `ITcsSkillCostPolicyHost` 的**脚本可达性是静态判据**（UHT 产物有该 `UCLASS` 与两个
   `BlueprintNativeEvent`），**"能导出 ≠ 能往返"**（同 `R-1` 的既有教训）。
3. **未做 PIE 路线取证**：本轮读数全部来自无头。**两条路线的 DLL 同源**（见
   `EVID-2026-10-11-r65-cooldown-cost` §1.1 的教训），但**MUST NOT** 因此宣称"双路线一致通过"。
4. **`CostMode` 的 BaseStruct 收窄只保证 picker 过滤**：运行期取内层仍需 `GetPtr<T>()` 判空
   （裸 `FInstancedStruct` 的既有代价，见 `TcsCostTypes.h` 的注记）。
5. **跨单位成本未做**：`BeginBatch`/`Commit` 形参是**逐单位**的 ⇒ 宿主策略若跨单位扣减，
   框架包的那层覆盖不到第二个单位（`TcsCostOps.h` 已如实登记）。无需求，**不入册**。
6. **未做"宿主策略改账本"的实测**：装置策略刻意**不改账本**（判据由 `OutApplied` 承担）
   ⇒ "策略内部写属性 + 框架批"这一组合**未被行为级覆盖**（静态上批已开）。
7. **冷却侧只改了枚举名**（`ETcsSkillCooldownTiming`），**行为零改动**；其读数由 R6.5-1 覆盖。

---

## 8. 复算脚本（本机逐字可跑）

```powershell
# 先确保没有残留的无头进程（否则链接会 LNK1104）
Get-Process UnrealEditor-Cmd -ErrorAction SilentlyContinue | Stop-Process -Force

# 双配置编译
$ubt='E:\UnrealEngine\UE_5.8\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe'
& $ubt LegendAutoChessEditor Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload
& $ubt LegendAutoChess Win64 Development -Project='E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject' -WaitMutex -NoHotReload

# 三条命令（每条一个进程；-ABSLOG 用绝对路径，相对路径会静默不生成）
$dir='E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\R65'
foreach($c in @('Tcs.Test.CooldownCost.Run','Tcs.Test.CooldownCost.Reject','Tcs.Test.SkillLayer.Run')){
  $log="$dir\CC2-" + ($c -replace '.*\.','') + ".log"
  & "E:\UnrealEngine\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" `
    "E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject" `
    /Game/TcsDev/L_TcsDev_Slice -game -nullrhi -nosound -noPause -unattended `
    "-ABSLOG=$log" "-ExecCmds=$c"
  Start-Sleep -Seconds 3
  Get-Process UnrealEditor-Cmd -ErrorAction SilentlyContinue | Stop-Process -Force
}
```

**期望读数**：`Run` 立即面 **28/0** + 尾段 **31/0**、`Reject` **7/0**、
R6 回归 **24/0** 且 `[复现性摘要]` 与基线逐字相同。
