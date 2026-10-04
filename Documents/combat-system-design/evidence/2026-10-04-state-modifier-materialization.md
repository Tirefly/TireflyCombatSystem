# EVID-2026-10-04-state-modifier-materialization

- **文档 ID**：`EVID-2026-10-04-state-modifier-materialization`
- **类型**：调研与证据（EVID）
- **状态**：FROZEN
- **权威范围**：R5 Task 4（修正器物化 D3-19：物化入口 / 快照作为求值参数表 / 施加与刷新挂点 / 移除级联摘除 / 属性访问解析点）的**验收读数与可复核锚点**；实施过程与偏差住 [`LOG-IMPLEMENTATION`](../log/implementation-log.md)，拍板理由住 [`LOG-DECISIONS`](../log/decisions-log.md)
- **最后更新**：2026-10-04

> **证据纪律**（照 `EVID-2026-10-04-state-param-snapshot-and-level-sources` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。本文每条读数对应日志里的一行；"未覆盖"的面一律进 §5，不用推断冒充实测。

---

## 1. 验收命令与读数

两条命令在同一 PIE 会话内执行（命令间无重启），装置 = 宿主侧 `Source/TcsDev/`。

| 命令 | 读数 | 说明 |
|---|---|---|
| `Tcs.Test.Slice.Run` | **即时 40 / 0**；**延迟段累计 47 / 0**（收束行） | 即时 40 = Task 3 的 32 条 + 本轮 21a–21h；延迟累计 47 = 40 + 延迟块 7 条（7b / 20f–20l） |
| `Tcs.Test.Slice.Reject` | **10 / 0** | 原 A–I + 本轮检查 J |

**本轮新增判定**（`Run`，逐条对应日志一行）：

| 检查 | 判据 | 实测读数 |
|---|---|---|
| `21a` | 两条模板各物化出一条账本条目 | 施加=`EAR_Applied`；条目 护甲 `0→1`、攻击 `0→1` |
| `21b` | 引用类操作数**从该实例的快照取值** | 护甲 `5.000 → -15.000` = 基础值 + 快照 `-20`；模板兜底刻意取 `-999`，**未被取用** ⇒ 证明走的是快照而不是兜底 |
| `21c` | 字面量模板的**值约定在物化边界**转规范值 | 攻击 `30.000 → 30.500`（模板写 50 配 `VCF_Percent` ⇒ 规范值 0.5；不转换会是 +50） |
| `21d` | **挂载先于 `Applied` 广播** | 同一订阅者的处理序号：起点 35 → 属性变更 **37** → `Applied` **38** |
| `21e` | 刷新**先摘后挂**：条数不累加、数值随新快照 | 结果=`EAR_Refreshed`；条目仍 `1 / 1`；护甲 `-30.000` = 5 + 覆盖 `-35` |
| `21f` | 移除按**同一来源一次摘净**两条并归位 | 移除成功；条目 `0 / 0`；护甲 `5.000`、攻击 `30.000`（= 各自基线） |
| `21g` | **零修正器行**的定义不产生条目 | 换登记成功；施加=`EAR_Applied`；护甲条目 0；单位在册 1 |
| `21h` | 单位**无属性账本**时挂点静默无操作 | 施加=`EAR_Applied`；账本**前后**均不认识该单位（是/是）；单位在册 1 |

**本轮新增判定**（`Reject`）：

| 检查 | 判据 | 实测读数 |
|---|---|---|
| `J` | 修正器模板**空引用**被跳过且不中断施加 | 登记=成；施加=`EAR_Applied`；账本条目 0；单位在册 1；日志见 `LogTcsState: Warning: 修正器物化跳过：模板无法解析（定义=StateDef.Probe.Modifier 单位=11）` |

---

## 2. 区段锚点（可复核）

**运行日志**：`E:\Projects_Dev\LegendAutoChess\Saved\Logs\LegendAutoChess.log`（414,806 字节，最后写入 2026-10-05 00:12:59）

| 区段 | 行号 | 行数 | UTF-8 字节 | SHA-256 |
|---|---|---|---|---|
| 常规面（`Run`） | L2419–L2848 | 430 | 61,260 | `5dedd64bf93a528021d324040418c75ee1265e6603e783f9693261c2fd78c3fa` |
| 拒绝面（`Reject`） | L2849–L3003 | 155 | 24,585 | `eeb895d595ac385ead7d5e9a2cd6d85fb431ff23acd6a654530534a2b0a563f1` |

**区段边界的内容**：`Run` 首行 = `Cmd: Tcs.Test.Slice.Run`，末行 = `LogTcsIntegration: UTcsCombatEntityComponent: 已注销实体（句柄 8）`；`Reject` 首行 = `Cmd: Tcs.Test.Slice.Reject`，末行 = 一条输入法（IME）激活日志（命令输出之后的无关行，按"命令到命令"口径留在区内）。

**`Run` 区段内红字 = 恰 3 条，全部为既有预期**（零 `[FAIL]`、零 `Ensure`、零非预期）：

| 类别 | 条数 | 归属 |
|---|---|---|
| `Warning: 状态施加被拒：定义未在本世界登记（TcsEvent.State.Periodic）` | 1 | 检查 19f 的**预期**拒绝日志（探针词 = 原生事件 tag） |
| `Warning: 时长操作被拒：无限时值无到期条目可调` | 2 | 检查 20j 的**预期**拒绝日志（Extend / Set 各一条） |

> **口径订正（本轮发现，已改装置文案）**：本命令**不是**字面"零红字"——19f 与 20j 是**故意**红字。此前装置头部只写"零故意 ensure / 零 Error"并声称"红字都在 `.Reject`"，会让复核者把上面 3 条误判为非预期。现头部明写"**预期红字 = 3 条 Warning（19f ×1 / 20j ×2）——其余红字均为非预期**"。纪律口径随之明确：**"零红字"应读作"零非预期红字"**（预期红字 MUST 逐条列出并给出归属）。

**`Reject` 区段内红字 = 装置头部逐条列明的那一批**：2 条登记失败相关（`Chain.ChainId.IsValid()` ensure + `ExecuteChain: 链 None 未登记` Error）+ 1 条 `ModifyFlow` 降级 Warning + **状态面 7 条 Warning**（无效目标 / 未登记定义 / 悬空句柄 / 无限时值时长操作 ×2 / 未登记定义上的时长操作 / **修正器模板空引用**）。

---

## 3. 复算脚本（PowerShell，逐字可跑）

```powershell
$log = "E:\Projects_Dev\LegendAutoChess\Saved\Logs\LegendAutoChess.log"
$lines = Get-Content $log
$sha = [System.Security.Cryptography.SHA256]::Create()
foreach ($r in @(@("Run",2418,2847), @("Reject",2848,($lines.Count-1)))) {
  $text  = ($lines[$r[1]..$r[2]] -join "`n")
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($text)
  $h     = ($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString("x2") }) -join ""
  "{0}: L{1}-L{2} 行数={3} 字节={4} SHA256={5}" -f $r[0], ($r[1]+1), ($r[2]+1), ($text -split "`n").Count, $bytes.Length, $h
}
# 预期输出：
#   Run:    L2419-L2848 行数=430 字节=61260 SHA256=5dedd64bf93a528021d324040418c75ee1265e6603e783f9693261c2fd78c3fa
#   Reject: L2849-L3003 行数=155 字节=24585 SHA256=eeb895d595ac385ead7d5e9a2cd6d85fb431ff23acd6a654530534a2b0a563f1
# 红字分类（Run 区段应为恰 3 条）：
$lines[2418..2847] | Where-Object { $_ -match 'Error:|Warning:|Ensure condition|Fatal error|\[FAIL\]' }
```

---

## 4. 本轮装置缺陷与订正（全程 1 处缺陷 + 1 处口径订正）

### 4.1 缺陷：21h 首版夹具**自己造出一条 ensure**

- **现象**：首轮 PIE（16:05）`Run` 区段出现一条 `Handled ensure: Ensure condition failed: Stores.Contains(Unit)`（`TcsAttributeSubsystem.cpp:52`），栈顶为 `TeardownSliceRigActors()` → `AActor::Destroy` → `UTcsCombatEntityComponent::EndPlay()` → `UTcsAttributeSubsystem::UnregisterUnit`。
- **根因**：21h 首版夹具是"真造一个单位 → `ApplySliceAttributes` → **手动** `UnregisterUnit` 注销它的属性账本"，用来构造"单位没有属性账本"。而单位组件的 `EndPlay` 也会注销同一条账本 ⇒ **二次注销**撞 ensure。
- **判据**：**夹具的形状自己造出了红字，与被测机制无关**——该 ensure 与"挂点静默无操作"这条被测行为没有因果关系（21h 本身当时是 PASS 的）。
- **修法**：夹具改为**从未注册过的句柄**（`FTcsCombatEntityHandle(20261004)`，同拒绝面检查 F 的手法），不真造 Actor；并把 21h 的判据加强为"施加**前后**账本均不认识该单位"（排除"碰了之后再撤掉"）。
- **留痕**：含缺陷那一轮的证据（**不作验收锚点**）= `Saved\Logs\LegendAutoChess-backup-2026.10.04-16.00.55.log` 的 `Run` 区段 L2424–L2922（499 行 / 71,263 字节 / SHA-256 `f09d1f8fa2dc20d6d2ac700d30d366ac1c999e84ed21578b18d423d0f7c9c021`），ensure 在 L2863（报告行）与 L2897（`EnsureFailed` 汇总行）。

### 4.2 口径订正：装置两处头部文案的"预期红字数"与实测不符

| 位置 | 原文案 | 实测 | 处置 |
|---|---|---|---|
| `Reject` 头 | "状态面 **6** 条 Warning" | **7** 条（少列了检查 I 的"未登记定义上的时长操作"——Task 3 起就少列，本轮沿袭） | 改为 7 并补列该项 |
| `Run` 头 | "拒绝面检查在 `.Reject`（那才是会产生红字的命令）" | `Run` 自己就有 **3 条预期 Warning** | 改为明写"预期红字 = 3 条 Warning（19f ×1 / 20j ×2），其余均为非预期" |

两处文案是编进 DLL 的字符串，故**改文案后重编双配置并整轮重跑**，使"日志区段 ↔ 源码"严格一致（上文 §2 的两个哈希取自重跑后的最终构建：`UnrealEditor-TcsDev.dll` 00:08:24）。

---

## 5. 如实边界清单（未覆盖面，逐条给归属）

| # | 边界 | 归属 / 触发条件 |
|---|---|---|
| ① | 单机、单世界、单 PIE 进程；`Run`/`Reject` 同会话（跨 PIE 残留由装置既有"世界反初始化全量退订"口径覆盖，本轮未另测） | 同 Task 2/3 口径 |
| ② | **模板资产走"瞬态 `NewObject`"**：`TSoftObjectPtr` 的 `LoadSynchronous()`（真资产同步加载）路径**未被走到**（探针模板都是已加载对象，`Get()` 命中即返回） | 归 **Task 7** 端到端（内容侧建 `UTcsAttrModDef` 真资产） |
| ③ | **模板身份词 `TemplateTag` 零解析消费者**：本轮 `ModifierRows` 走**资产直引用**，模板身份词无人按 tag 解析 ⇒ 探针模板的 `TemplateTag` **留空**，其根归属未定 | 入册台账 **`ATTR-1`**（触发条件 = R6 技能侧 / R8 内容侧需要按 tag 解析模板时） |
| ④ | **`OPK_AttributeScaled` 操作数**：本轮只保证"不物化、属性与系数原样进账本"这条静态行为；其**实时求值路径**（M2 既有面）本轮未配该类模板实测 | 归 `attribute-pipeline` 既有面；本轮 21 未覆盖 |
| ⑤ | **模板操作数配"等级类源"**（`StateLevel*` / `InstigatorLevel*`）未被实测：21b 验的是引用源走快照。物化上下文由 `MakeContext` 与快照构建**同源装配**（静态可核），但"按等级取档"在物化点的行为**未配实测** | 若 Task 7 需要，加一条"等级源当操作数"的探针即可 |
| ⑥ | **脚本源读快照仍未验证**：甲案的反射壳让 `Ctx.ParamTable` 在**结构上**可达 C#（宿主参数源可读该实例快照），但本轮**没有配 C# 模板操作数** ⇒ "脚本能读快照"**仍是假设、不 claim** | 触发条件型：出现真实脚本操作数需求时补 C# 探针（归 `SCRIPT` 系列） |
| ⑦ | **属性账本"已被注销但仍持句柄"**这条路径未覆盖：21h 改用"从未注册的句柄"后，绕开了"注销后仍操作"这一形态（该形态会撞 §4.1 那条 ensure） | 归 `attribute-store` 面；本轮明示不测 |
| ⑧ | **重入窗口**：本轮闭合了**自己引入的两处**（挂载提交后重新定位实例与桶）；既有的"`Removed` 广播窗口内重入移除同一句柄 ⇒ 双次归还槽位"**未修** | 入册台账 **`STAT-5`**（触发条件 = Task 6 起回调里能改状态的消费者出现） |
| ⑨ | 刷新路径的"先摘后挂"在"**同一定义 + 不同覆盖值**"下验证；"同一 tag 在两次施加之间被换成不同行数的定义"（开发期行为）未覆盖 | 装置 21g/21h 已顺带覆盖"换定义"的登记面，未覆盖"已施加实例+定义被换"的组合 |
| ⑩ | 属性访问仍是"**直接依赖门面 + 单一解析点**"（`FTcsStateAttributeAccess`）；注入契约（`ITcsAttributeAccess`）未做 | 台账 `R-7`（触发条件 = 出现"宿主替换属性存储"或"脚本直写属性"的真实需求） |

---

## 6. 实施期发现（引擎/机制事实，供后续轮参考）

1. **`TSoftObjectPtr` 的取值应先 `Get()` 再 `LoadSynchronous()`**：前者命中已加载对象即返回、不发加载请求；后者在"对象是装置 `NewObject` 出来的瞬态实例"时无路径可解（弱引用命中才有效）。物化器按此顺序书写——顺带使**瞬态模板资产**成为装置验收的可行形态（GC 可达性由状态门面的 `AddReferencesWithStructARO` 对登记表副本里的 `TSoftObjectPtr` 承担）。
2. **"谁先到"必须有定序读数**：计数器只能证明"两枚事件都到了"，证不了顺序。可行手法 = 让**同一个订阅者**同时订阅两类事件，用它的**处理序号**做时间线（本轮 21d 即此形态；`PublishImmediate` 同步派发 ⇒ 处理顺序 = 广播顺序）。
3. **"挂了几条 / 摘干净没有"要看条目数，不是数值**：数值只能说明"变了"。装置侧读数 = `ResolveStore` → `FindInstance` → `ModifierSlots.Num()`（走非确保查询口，未注册单位返回 `nullptr` 而不是 ensure）。
4. **重入窗口是一条需要主动排查的纪律**：任何"可能广播"的调用之后，调用方手里的实例指针与桶引用都可能失效（订阅者可在其中改状态）。本轮在施加/刷新路径各加了一处"提交后重新定位"，并把移除路径的摘除**排在取实例指针之前**——这样既有守卫天然接住重入。既有的一处窗口见 §5 边界⑧。
