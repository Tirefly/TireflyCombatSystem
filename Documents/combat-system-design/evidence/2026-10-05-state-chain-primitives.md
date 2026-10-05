# EVID-2026-10-05-state-chain-primitives（R5 Task 6 / 6a：链原语与行为面接线）

> **结论一句话**：链里首次**能施加状态、能改属性**——`FTcsStepApplyState` 与 `FTcsStepModifyAttribute` 自注册生效并各有一条端到端读数；**状态自己的内联触发行首次有真实登记与退订**（在**自己的** `Applied` 上起链一次、在**自己的** `Removed` 上不起、移除后登记表按**实例锚点**回落）；链运行态首次持有**可读的"身份 + 因果边"**（`RunSource` 每运行一枚新号、`CausedBy` 触发行起链时 = 该行所属实例的锚点）；`AttributeCompare` 作为第三条内置条件实测能**门控**起链。
>
> **证据纪律**（照 `EVID-2026-10-05-state-stacking-policies` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。本文每条读数对应日志里的一行；"未覆盖"的面一律进 §5，不用推断冒充实测。

---

## 1. 读数（逐条对应日志一行）

**命令计数**（装置屏显 + 日志 `[TcsDevRig]` 行）：

| 命令 | 计数 | 对照（Task 5） | 变化 |
|---|---|---|---|
| `Tcs.Test.Slice.Run`（即时段） | **通过 61 / 失败 0** | 53 / 0 | **+8**（检查 23a–23h） |
| `Tcs.Test.Slice.Run`（延迟段收束） | **通过 68 / 失败 0** | 60 / 0 | +8 |
| `Tcs.Test.Slice.Reject` | **通过 11 / 失败 0** | 11 / 0 | 0（本轮未新增拒绝面检查） |

**装置头部声明与本轮实测一致**（L2435 逐字）：`预期红字 = **3 条 Warning**（检查 19f 未登记定义 ×1 / 检查 20j 无限时值时长操作 ×2）` ⇒ `Run` 区段红字**恰 3 条**、与清单逐条吻合（见 §2）。

### 1.1 链里施加状态：身份与锚点解耦（23a）

| 读数 | 日志行 | 值 |
|---|---|---|
| 链里 `ApplyState` 建实例 + 身份/锚点互异 | L3178 | `[PASS] 检查 23a：… 在册 0→1 期望 1；回执 Applied 以「在册实例 + Applied 事件 +1 期望 1」为据；层数=1 期望 1；Source.Id=115 CascadeAnchor.Id=116 两者非 0 且互异=是` |

**读法**：`Source`（施加方身份，= 该次运行的 `RunSource`）与 `CascadeAnchor`（级联撤销锚点，每实例恒发新号）是**两个不同的句柄**——这是 Task 6 裁定 1（`SPEC-02-states` §12.10 第 1 条）在链路径上的直接读数；两者由**同一枚发号器**在**同一次施加**里各发一枚，故必然相邻而不相等。

### 1.2 内联触发行：自己的事件起链、自己的死亡不起链、按锚点级联退订（23b–23d）

| 读数 | 日志行 | 值 |
|---|---|---|
| 自己的 `Applied` 起一次行为链 | L3179 | `[PASS] 检查 23b：… 行为链执行计数=1 期望 1；护甲 5.000→20.000 期望 20.000 = 施加前 + 修正器 10.0 + 行为链 5.0` |
| 自己的 `Removed` **不再**起链 | L3194 | `[PASS] 检查 23c：… 移除=成；行为链执行计数 1→1 期望不增；护甲 20.000→10.000 回退 10.000 期望 10.000 = 只摘修正器、不含该行贡献` |
| 登记表按**实例锚点**级联退订 | L3195 | `[PASS] 检查 23d：… 登记表行数 施加前=2 施加后=4 期望 4；移除后=2 期望 2 = 回到施加前` |

**读法**：23b/23c 是"接线时机"这对硬约束的判据——**登记排在 `Applied` 广播之前**（新登记的行必须看见自己的 `Applied`）、**退订排在 `Removed` 广播之前**（该实例自己的行 MUST NOT 看见自己的死亡事件）。23d 的基线 2 = 两条**内容资产**触发行（`EffectTriggerDef.Check.ArmorBreak` / `EffectTriggerDef.Check.Seed`，定义库装配期登记），施加减 2 条内联行 ⇒ 4，移除后回到 2 ⇒ 退订**只摘本实例那两条**。

**顺带一条副读数**（23c 的"回退 10.000"）：行为链挂上的 `ModifyAttribute` 条目是**常驻**的、锚在**该次运行的来源锚点**上，故**不随状态实例锚点摘除**——与"状态走了、修正器跟着走"（状态自己的修正器行）是两条不同的账（见 §5 第 6 条）。

### 1.3 `ModifyAttribute` 与 `AttributeCompare`（23e–23f）

| 读数 | 日志行 | 值 |
|---|---|---|
| `ModifyAttribute` 直改账本 | L3207 | `[PASS] 检查 23e：… 护甲 10.000→17.000 增加 7.000 期望 7.000；该链执行计数 0→1 期望 +1` |
| `AttributeCompare` 门控（三发布对照） | L3227 | `[PASS] 检查 23f：… 手搓行登记=成 阈值=18.000；①空载荷 ⇒ 账本 17.000 不变、执行计数 +0 期望 0；②带主体载荷且 A 低于阈值 ⇒ 账本 17.000 不变、执行计数 +0 期望 0；③抬到 80.000（>阈值）后同一发布 ⇒ 账本 80.000→87.000 增加 7.000 期望 7.000、执行计数 1→2 期望 +1` |

**读法**：**真正的门控证据是 ② 与 ③ 的对照**（同一载荷、只差"属性当前值与阈值的关系"）——①空载荷只落实"不可求值 ⇒ 不通过"这一档（条件读 `Context.Caster`，而 Caster 由**载荷读取器**解析 ⇒ 空载荷下恒不可求值，见 §4 第 4 条）。

### 1.4 同来源两实例：撤销互不牵连（23g）

| 读数 | 日志行 | 值 |
|---|---|---|
| **同一声明来源**施加两个不同定义，撤一个只掉自己的 | L3279 | `[PASS] 检查 23g：… 回执 主/次 0/0 期望 0/0；在册=2 期望 2；护甲 87.000→112.000 增量 25.000 期望 25.000 = 两条修正器 20.0 + 主实例自身 Applied 起的一次行为链 5.0（计数增量 1 期望 1）；两实例 CascadeAnchor.Id=130 / 128 互异=是；移除主实例后护甲 102.000 掉 10.000 期望 10.000、次实例仍在册=是` |

**读法**：这是裁定 1 的**核心判据读数**——两实例 `Source` **相同**（同一个声明来源）而 `CascadeAnchor` **互异**，故"撤一个"只摘掉它自己的 10.000，另一实例的条目仍在账本上。**若沿用旧口径（按 `Source` 撤销），这一撤会把两条实例的条目一起摘掉**（那正是本裁定存在的理由）。

### 1.5 链运行态的身份与因果边（23h）

| 读数 | 日志行 | 值 |
|---|---|---|
| 身份：两次运行 `RunSource` 互异且各自 = 实例 `Source` | L3349 | `[PASS] 检查 23h：… 探针步骤记录 1→3 期望 +2；两次运行 RunSource.Id=135 / 140 互异=是；对应实例 Source.Id=135 / 140 相符=是；宿主直调 CausedBy.Id=0 / 0 期望 0；触发行起链 RunSource.Id=118 CausedBy.Id=116 期望 116 = 主探针实例 CascadeAnchor.Id 相符=是` |

**探针步骤的独立证据**（"这两个字段真的在运行期有值、且取法正确"）——7 行 `Display`，逐字形态 `探针步骤（链运行态来源）：链=… 槽=… RunSource.Id=… CausedBy.Id=…（运行态句柄 i/g；本槽位第 N 条记录）`：

| 槽 | 日志行 | 逐字要点 |
|---|---|---|
| `Chain` | L3150 / L3281 | `RunSource.Id=115 CausedBy.Id=0`（宿主直调 ⇒ **无成因**，边为空 ✓） |
| `Triggered` | L3164 / L3253 / L3295 | `RunSource.Id=118 CausedBy.Id=116`（= 主探针实例 `CascadeAnchor.Id` ✓ 因果边来自**该行的 `Source`**） |
| `Compare` | L3197 / L3217 | `RunSource.Id=121 CausedBy.Id=0`（手搓行 ⇒ 行的 `Source` 未声明 ⇒ 边为空 ✓） |

**读法**：`CausedBy` 的语义实测成立——**状态施加期登记的行**（23b 的内联行）其 `Source` 就是该实例的**级联锚点**，故它起出的链 `CausedBy` = 那枚锚点；**宿主直调**与**装置手搓行**两种来路都没有成因 ⇒ `CausedBy.Id = 0` ✓（三档互不相同，正是"各档读数必须可分"的落地）。

---

## 2. 区段锚点（可复核）

**运行日志**：`E:\Projects_Dev\LegendAutoChess\Saved\Logs\LegendAutoChess.log`（512,123 字节，最后写入 2026-10-05 17:26:22；日志内时间戳为 UTC `2026.10.05-09.26`）

| 区段 | 行号 | 行数 | UTF-8 字节 | SHA-256 |
|---|---|---|---|---|
| `Run` | **L2433–L3513** | 1081 | 153,692 | `2cfa09fec0120c4fcaa2d97bedf7f9b9137f784b2f4767b092b1950ee84f801f` |
| `Reject` | **L3514–L3684** | 171 | 27,052 | `7458a9997fd1b3de18c90548a03ec6f10b38828eda70897c71d5745329632f34` |

**区段边界的内容**（"命令到命令"口径，与 Task 4 / 5 一致）：`Run` 首行 = `Cmd: Tcs.Test.Slice.Run`（L2433），末行 = `Reject` 命令行前一行；`Reject` 首行 = `Cmd: Tcs.Test.Slice.Reject`（L3514），末行 = 文件末行。

**`Run` 区段内红字 = 恰 3 条，全部为既有预期**（零 `[FAIL]`、零 `Ensure`、零非预期）——与装置头部（L2435）逐条吻合：

| 日志行 | 红字 | 归属 |
|---|---|---|
| L2739 | `LogTcsState: Warning: 状态施加被拒：定义未在本世界登记（DefTag=TcsEvent.State.Periodic）` | **检查 19f**（未登记定义被拒）的**故意**红字，紧跟其 `[PASS]` 行 |
| （两行） | `LogTcsState: Warning: 时长操作被拒：无限时值无到期条目可调（定义=StateDef.Probe.Infinite 句柄=0/1）` | **检查 20j**（无限时值时长操作 ×2）的故意红字 |

> **一处易误读点**：19f 的探针定义**故意**用了一枚事件词（`TcsEvent.State.Periodic`）当"未登记定义"的身份 ⇒ 那条 Warning 的 `DefTag=` 看上去像事件 tag，这是**夹具取词的形状**，不是"事件被当成定义施加"。

**`Reject` 区段红字 = 装置头部逐条列明的那一批**（1 条 `Chain.ChainId.IsValid()` ensure 块 + 1 条 `ExecuteChain: 链 None 未登记` Error + 1 条 `ModifyFlow` 降级 Warning + **状态面 9 条 Warning**），其中状态面计数按行归类为 `LogTcsState` 3+4+2 = **9** ✓（与 Task 5 收束时把头部由 7 改 9 的口径一致，本轮未改动装置头部）；另 `EnsureFailed` 1 处、`LogOutputDevice` 断言转储为 ensure 块的伴随输出。

---

## 3. 复算脚本（PowerShell，逐字可跑）

```powershell
$log = "E:\Projects_Dev\LegendAutoChess\Saved\Logs\LegendAutoChess.log"
$lines = Get-Content $log
$sha = [System.Security.Cryptography.SHA256]::Create()
foreach ($r in @(@("Run",2432,3512), @("Reject",3513,($lines.Count-1)))) {
  $text  = ($lines[$r[1]..$r[2]] -join "`n")
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
  $h     = ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join ""
  "{0}: L{1}-L{2} 行数={3} 字节={4} SHA256={5}" -f $r[0], ($r[1]+1), ($r[2]+1), ($text -split "`n").Count, $bytes.Length, $h
}
# 预期输出：
#   Run:    L2433-L3513 行数=1081 字节=153692 SHA256=2cfa09fec0120c4fcaa2d97bedf7f9b9137f784b2f4767b092b1950ee84f801f
#   Reject: L3514-L3684 行数=171 字节=27052 SHA256=7458a9997fd1b3de18c90548a03ec6f10b38828eda70897c71d5745329632f34
# 红字分类（Run 区段应为恰 3 条）：
$lines[2432..3512] | Where-Object { $_ -match 'Error:|Warning:|Ensure condition|Fatal error|\[FAIL\]' }
# 新增读数（应为 8 条 23a–23h + 7 行探针日志）：
$lines | Where-Object { $_ -match '检查 23[a-h]：|探针步骤（链运行态来源）' }
# 汇总（应为 61/0 与 68/0）：
$lines | Where-Object { $_ -match '即时判定汇总|（延迟）收束' }
```

**区段号随日志增长而变**——上表的行号是**本次会话**的值；复算时以 `Cmd: Tcs.Test.Slice.Run` / `Cmd: Tcs.Test.Slice.Reject` 两行定位（脚本里的 `2432/3513` 即按本次定位写死，跨会话复算请先重新定位）。

---

## 4. 缺陷与偏差记录

1. **首轮 PIE 唯一失败 = 检查 23g**（即时 `60/1`、延迟 `67/1`）：读数 `增量 30.000 期望 25.000`、`计数增量 2 期望 1`。**根因 = 内联触发行按事件 Tag 订阅与派发、不过滤载荷与单位**——`TcsTriggerEvaluator` 按 Tag 路由，`FTcsEffectTriggerDef::EventPayloadFilter` 至此**零消费者** ⇒ 主实例登记的那条 `Applied` 行会响应世界里**任何** `Applied` 事件，**包括次实例的**；"先施主、后施次"时次实例的 `Applied` 引爆了主实例的行（10+10+5+5 = 30、计数 2，算术自洽）。**修法 = 调整 23g 的施加顺序**（次定义先施加 ⇒ 主实例的行尚未登记 ⇒ 串扰窗口为零），`bPass23g` 的期望**一行未改**（期望本来是对的）。**该语义已入台账 `TRIG-6`**（见 §4 附注与台账），**不是夹具配错、也不是被测代码缺陷**：它是"行是事件 Tag 级规则、其生命周期与状态实例绑定"这一既有模型的真实语义。
   > **副产物**：23c/23g 两条读数**共同**证明了 `ModifyAttribute` 挂的是**常驻**条目（锚在运行来源锚点上，不随状态实例锚点摘除）——这是 §5 第 6 条边界的实测来源。
2. **一处工程偏离（装置屏显段）**：23 段**没有**挤进既有的 `Screen(10)` 段，而是自开 `FTcsDevRigScreenSection Wiring(100)`。判据 = 上一轮 PIE 日志实测：22 段收尾时 `Screen` 游标已到 **65**，再排 9 行会跨到 key 80–83，**正好压在 `Summary(80)` 上**（同键后写覆盖先写 ⇒ 末几条读数屏上缺尾，日志仍有）。段内调用风格与 22 段完全一致（`Wiring.Result(...)` + `检查 23x：…（实测 … 期望 …）`）。
3. **一处与指令原描述的偏差（装置夹具，有意）**：23f 原描述写"空载荷发布 ⇒ 条件不过"，但 `AttributeCompare` 读的是 `FTcsTriggerContext::Caster`，而 **Caster 由载荷读取器解析**（`FTcsTriggerReadPayloadInfo`）⇒ **空载荷下 Caster 恒无效、条件恒不可求值**，"条件过"的方向**永远不可达**。故 23f 落成**三条发布 / 三组读数**：①空载荷（落实原描述，验"不可求值 ⇒ 不通过"）；②带主体载荷（`FTcsDamageFlowCollectEvent`——本项目**唯一**登记了读取器的载荷类型，`Caster` ← `Context->Attacker`）且属性低于阈值；③把属性抬到阈值之上后**同一载荷**再发布。**门控证据 = ②③ 的对照**（同一载荷，只差"当前值与阈值的关系"）。
4. **一处夹具手段（如实记）**：23f 的"抬值"用 `SetBaseValue(unit, Armor, 阈值 + 50)`——依据是 `DA_Armor` 的 `Bounds` 全默认（无 `Min/Max/Mode`）且历史实测出现过 `护甲 5.000→-15.000`（无 Min 钳制）。**若将来给护甲加边界，此夹具 MUST 改成"再挂一条大修正器"**。
5. **一处夹具顺序约束（由 §4 第 1 条导出）**：23g 的两次施加**有先后约束**（次定义先、主定义后），已在夹具与检查处各留一行注释说明原因——后人重排这两行会立刻复现 30/2 的症状。

---

## 5. 如实边界清单（未覆盖面，逐条）

1. **`Expired` 路径的内联触发行未实测**：23c 只覆盖**显式 `Removed`**；到期与本命令走**同一个** `UnwireTriggerRows`（同一条移除链），但"到期路径"本身零读数。
2. **挂起链上的两字段读数未覆盖**：本段三条链全是**全即时链**（读数点靠"探针步骤在链首、运行态尚活"成立）；挂起链（`WaitDelay` / `WaitEvent`）上的 `RunSource` / `CausedBy` 未验。
3. **子链口径零读数**：`RunSubChain` 另发新号（子链 `RunSource` ≠ 父链）与"子链 `CausedBy` = 父链 `RunSource`"两条**只有实现与规格，无实测**。
4. **`Branch` 步骤里的 `AttributeCompare` 未覆盖**：本轮给 `TcsStepBranch` 也补了 `Context.World`（漏填会让链里的该条件**静默恒不通过**），但只验了**触发行求值器**那一侧。
5. **`GetRunSource` / `GetRunCausedBy` 的悬空句柄分支未正面触发**（返回 `Id == 0`）：只有 23h 的"宿主直调 `CausedBy.Id = 0`"间接印证 0 语义；"句柄无效/运行态已释放"那两条早退分支零读数（若要，宜住 `.Reject`）。
6. **`ModifyAttribute` 挂的是常驻账本条目**：锚在**该次运行的来源锚点**上，故**不随状态实例的移除而摘除**（23c 的"回退 10.000"与 23g 的"增量含 +5"两条读数共同证明）。要"临时改数值"用状态层（施加态 + 到期摘除）；本原语表达的是"链直接改一次账本"（D7-6 的唯一写入口）。**它也没有"按运行锚点回收"的公开入口**——回收需宿主自行按来源调 `RemoveBySource`。
7. **链侧参数表缺位**：`ApplyState` 与 `ModifyAttribute` 的引用类参数源（`ParamRef` 一类）**落兜底**——链侧没有 `ITcsParamTableReader` 的实现（零消费者，未建）。
8. **状态事件载荷没有读取器** ⇒ **状态事件起链的运行态 `Caster` / `Instigator` 恒无效**（本段两条内联行不依赖它：行为链的目标写死；`AttributeCompare` 那条手搓行改用带主体的伤害载荷）。连带：**内容侧今天无法按"哪个单位/哪个实例"自筛触发行**（`EventPayloadFilter` 零消费者 + 无读取器 ⇒ 条件也读不到主体）——即 §4 第 1 条串扰语义**在内容侧不可规避**，只能靠登记/退订的生命周期与施加顺序。
9. **单机单世界单 PIE 会话**：全部读数来自**一次** PIE 会话（命令间不重启）；无网络、无多世界、无 Mass 路径。
10. **Shipping 只验编译**（`LegendAutoChess Win64 Shipping` = `Result: Succeeded`），无 Shipping **运行期**验证；Development Editor 侧同样只跑本装置，未做压测。
11. **未覆盖面（与 6a 交付面无关但同轮存在）**：`Tcs.Test.Slice.Run` 的第一次运行（`60/1`）**不构成验收失败**——它暴露的是**装置夹具的顺序假设**（§4 第 1 条），其读数已留档在 §4；正式验收以本轮 `61/0` 为准。

---

## 6. 引擎 / 机制事实（本轮实测或源码核实，供后续轮）

1. **`TWeakObjectPtr<T>::operator=` 要求 `T` 是完整类型**：把宿主探针执行器的 `SetProbeOwner(UTcsEffectSubsystem*)` **内联在只前向声明门面的头里**，会让 UHT 生成的 `Module.TcsDev.gen.cpp` 报 `C2679`（`UTcsEffectSubsystem*` → `const UObject*` 转换不可用）——**实现挪进 `.cpp` 即解**。⇒ 头里只放声明，赋值/构造凡涉及 `TWeakObjectPtr<T>` 的一律落 `.cpp`。
2. **`TTcsInstancePool::Resolve` 没有 const 重载**（`IsValid` 有）：要写 `const` 只读访问器就必须 `const_cast`（先 `IsValid` 前置校验避开 `Resolve` 的 ensure）。同款先例 = `TcsStateOps_Lifetime.cpp` 的 `const_cast<FTcsStateInstance*>`。
3. **条件求值器签名拿不到世界**（`FTcsTriggerConditionTest` 只收"条件数据 + 上下文 + 随机值"）：要读账本的条件（`AttributeCompare`）必须把世界放进**上下文**（`FTcsTriggerContext::World`，非 `UPROPERTY` 的瞬时读数）。**且"谁构造上下文"必须一起改**——本轮实测构造点有**两处**（触发行求值器 + `Branch` 步骤），漏掉任一处会让该条件在该路径上**静默恒不通过**（不报错、不打日志，最难查的一类）。
4. **`RegisterStepExecutor` 的重复登记面 = `ensure` + `false`，且无 `Unregister` API** ⇒ 宿主探针执行器必须"先查注册表、有则复用"（同一 PIE 会话内重跑验收命令时才不会刷 ensure）；新 PIE 会话重置世界子系统登记表。
5. **屏显段键位预算**：`FTcsDevRigScreenSection` 的同键后写覆盖先写 ⇒ 新增检查段前**先用历史日志实测游标**（本轮实测 `Screen` 已到 65，而 `Summary` 用 80）⇒ 新段另开基址（本例 100）比"塞进老段"安全。
6. **内联触发行是"事件 Tag 级规则"**：`FTcsEffectTriggerInstance.Source` 只作**级联退订锚点**，**不参与派发过滤**（`TcsTriggerEvaluator` 全文不读它）；行的作用域 = "登记期内的全世界同 Tag 事件"。⇒ 6b 的行为 Fragment（同样按兴趣 Tag 订阅）会面临**同一语义**；Task 7 端到端若同时挂多个带行的状态，读数会含这份串扰（台账 `TRIG-6`）。

---

## 附：本轮交付面（口径索引）

- 规格口径：`SPEC-02-states` **§12.10**（11 条 Task 6 落地口径）、§12.1 表两行、§6 行；`SPEC-03-effects` §2.1 原语 **8 → 10**、§2 边界句改述
- 提案：`add-state-chain-primitives`（4 新能力 + 4 改能力 / 8 个 delta）——本文件是其验收证据；`add-state-behavior-fragments`（6b）**未实施**
- 台账：`TOOLS-7`（溯源树——边已可读、树与面板归 R8）、**`TRIG-6`**（本轮新增：触发行 Tag 级跨实例串扰）
