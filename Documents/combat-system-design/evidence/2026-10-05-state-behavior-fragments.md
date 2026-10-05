# EVID-2026-10-05-state-behavior-fragments（R5 Task 6 / 6b：状态行为 Fragment）

> **结论一句话**：状态首次有了**作者面的行为回调**——`FTcsStateBehaviorFragment`（兴趣 Tag 列表 + 一个 `const` 泛化回调）住 `FTcsBuffDef.Fragments`，施加时按**每兴趣 Tag 一条订阅**接线、移除 / 到期在广播**之前**退订，回调里能读到"哪个实例、哪个单位、哪个定义、几层、几级"；**首个状态载荷读取器**同时落地，使**状态事件第一次能按实例身份自筛**（`TRIG-6` 甲案），触发行因此从"事件 Tag 级规则"收窄为"**绑定行只认自己的实例主体**"；`23g` 在**恢复"先主后次"**的顺序下仍读到 `+25 / 起链一次`，即该串扰语义在**不靠夹具顺序**的情况下被消除。
>
> **证据纪律**（照 `EVID-2026-10-05-state-stacking-policies` / `EVID-2026-10-05-state-chain-primitives` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。本文每条读数对应日志里的一行；"未覆盖"的面一律进 §5，不用推断冒充实测。

---

## 1. 读数（逐条对应日志一行）

**命令计数**（装置屏显 + 日志 `[TcsDevRig]` 行）：

| 命令 | 本轮首轮 PIE | 本轮第二 PIE（同进程、重开会话） | 对照（6a） | 变化 |
|---|---|---|---|---|
| `Tcs.Test.Slice.Run`（即时段） | **通过 65 / 失败 0** | **通过 65 / 失败 0** | 61 / 0 | **+4**（检查 23i–23l） |
| `Tcs.Test.Slice.Run`（延迟段收束） | **通过 72 / 失败 0** | **通过 72 / 失败 0** | 68 / 0 | +4 |
| `Tcs.Test.Slice.Reject` | **通过 11 / 失败 0** | —（本轮同进程只跑一次） | 11 / 0 | 0（本轮未新增拒绝面检查） |

**装置头部声明与本轮实测一致**（L2443 逐字）：`预期红字 = **3 条 Warning**（检查 19f 未登记定义 ×1 / 检查 20j 无限时值时长操作 ×2）` ⇒ `Run` 区段红字**恰 3 条**、与清单逐条吻合（见 §2）。

### 1.1 行为 Fragment 收自己的事件（23i）

| 读数 | 日志行 | 值 |
|---|---|---|
| 自身 `Applied` 唤起行为**恰好一次**，且订阅 / 实例计数可读 | L3392 | `[PASS] 检查 23i：自身Applied一次（次数=1期望1；subs=4期望4；instances=1期望1；读取器Caster=Instigator/退Unit且Subject=Handle与三字段反射=是）` |

**读法**：这一条同时验了三件事——① **`Applied` 广播之前订阅已完成**（否则自己的 `Applied` 收不到，次数为 0）；② 订阅表按 **Tag** 计数（`subs=4` = 该实例兴趣的四个 Tag 各一条，**不随实例数增长**）；③ **状态载荷读取器真的注册了**：`Caster` 取**有效 `Instigator`、否则退 `Unit`**、`Subject` 装 `FTcsStateHandle`，且载荷三字段（`Unit` / `Handle` / `Cause`）反射可达。

### 1.2 跨实例 / 跨单位隔离、刷新不重挂、外部事件扇出（23j）

| 读数 | 日志行 | 值 |
|---|---|---|
| 两定义两单位无串扰；刷新不重订阅；空外部载荷向存活候选扇出；重复兴趣去重 | L3402 | `[PASS] 检查 23j：两定义两单位无串扰=是；刷新A/B次数=1/0期望1/0，订阅4→4不重挂；空外部载荷扇出A/B=1/1期望1/1，重复兴趣去重subs4/instances2` |
| **重入子读数**：`Applied` / `Refreshed` 自移除均成立且清理干净 | L3421 | `[提示] 23j重入子读数：Applied自移除=成；Refreshed自移除=成（结果=1期望1、旧句柄0/57拒绝、subs/instances=0/0期望0/0）` |

**读法**：`刷新A/B次数=1/0` 是**状态载荷按实例身份自筛**的直接读数——A 刷新时只有 A 收到（1），B **不增**（0）；而 `空外部载荷扇出A/B=1/1` 是**非状态载荷按兴趣全扇出**的对偶读数（两实例各 +1）。`订阅4→4不重挂` 落实"刷新 / 叠层路径 MUST NOT 重订阅"。`旧句柄0/57拒绝` 是**句柄代际**在**蜜月期之内**的一次独立读数（旧句柄在**同一次 PIE 内**已判失效）。`subs/instances=0/0` 说明两个自移除实例把订阅与在册都带回了零。

### 1.3 退订先于广播、三路有限重入、作者期校验（23k）

| 读数 | 日志行 | 值 |
|---|---|---|
| 自己的 `Removed` **不再**唤起行为；外部递归移除**静默拒绝**；作者期两条规则生效 | L3423 | `[PASS] 检查 23k：先退订后Removed（片段A/B=0/0期望0/0）；外部Removed重入次数=1期望1，重入返回=false期望false；Applied/Refreshed自移除=成/成；作者校验=通过` |
| 作者期读数明细（只读 `DataValidationContext`、不写 Warning） | L3422 | `[提示] 作者期行为校验：合法=1期望Valid；空载荷=0错误1；异类型=0错误1（只读DataValidationContext、不写Warning）` |

**读法**：`片段A/B=0/0` 是**退订排在 `Removed` 广播之前**的判据读数（若退订晚于广播，该实例自己的行为会收到自己的死亡事件 ⇒ 计数 ≥1）。`重入次数=1期望1、重入返回=false` 是 **`STAT-5` 有限重入守卫**的实测：**外层广播恰一次、递归 `Remove` 同句柄返回 `false` 且不产生第二笔广播 / 第二次释放**。`作者校验=通过` 落实 `IsDataValid` 的两条新规则（空载荷 / 非派生类型各报 1 条 Error）。

### 1.4 生命周期实物哨兵：跨世界清理与旧句柄拒绝（23l，**两连 PIE**）

| 轮次 | 日志行 | 值 |
|---|---|---|
| 首轮：起点零，结尾**故意保留**一条真实无限哨兵 | L3429 | `[PASS] 检查 23l：生命周期实物哨兵（起点subs/instances=0/0期望0/0；跨世界=待下一PIE复核，未宣称跨世界通过，旧句柄拒绝=未测；结尾保留句柄0/59 单位-155，subs/instances=1/1期望1/1）` |
| 第二 PIE：**旧句柄被拒**、起点归零、结尾再留一枚新哨兵 | L4833 | `[PASS] 检查 23l：生命周期实物哨兵（起点subs/instances=0/0期望0/0；跨世界=已实际复核，旧句柄拒绝=是；结尾保留句柄0/135 单位-331，subs/instances=1/1期望1/1）` |
| 世界拆解清理（**两轮各一次**） | L3738 / L4931 | `LogTcsState: 状态行为反初始化：订阅 1→0 实例 1→0` |

**读法**：这是本轮**唯一**靠"**重开 PIE 会话**"才能成立的读数，故分成两轮写、**首轮不冒充跨世界已验**（首轮屏显逐字写着"待下一PIE复核，未宣称跨世界通过"）。第二 PIE 的三条判据：① **起点 `0/0`** ⇒ 上一世界的订阅**没有跨世界残留**（`Deinitialize` 的全量退订生效，且与 L3738 的 `1→0 / 1→0` 日志互证）；② **旧句柄 `0/59` 被拒** ⇒ 句柄代际**跨世界不重置**（裁定 8 甲案）；③ 结尾 `0/135` 的新哨兵 `subs/instances=1/1` ⇒ 新世界的行为订阅是**新登记**的，不是旧残留。**两轮的结尾都故意保留一枚哨兵、不在命令末尾清理**——这正是"下一轮能看到 `1→0` 而不是 `0→0`"的前提。

### 1.5 `TRIG-6` 甲案：恢复"先主后次"后串扰不再出现（23g）

| 读数 | 日志行 | 值 |
|---|---|---|
| **同一声明来源**施加两个不同定义，撤一个只掉自己的；**主实例自己的 `Applied` 起链恰一次** | L3287 | `[PASS] 检查 23g：同来源两实例撤销互不牵连（回执 主/次 0/0 期望 0/0；在册=2 期望 2；护甲 87.000→112.000 增量 25.000 期望 25.000 = 两条修正器 20.0 + 主实例自身 Applied 起的一次行为链 5.0（计数增量 1 期望 1）；两实例 CascadeAnchor.Id=128 / 132 互异=是；移除主实例后护甲 102.000 掉 10.000 期望 10.000、次实例仍在册=是）` |

**读法**：6a 首轮 PIE 的**唯一失败**就在这里（`增量 30.000 期望 25.000` / `计数增量 2 期望 1`），当时的**修法是把施加顺序改成"次定义先"**（把串扰窗口压到零）——那是**绕过**，不是修复。本轮 `TRIG-6` 甲案（首个状态载荷读取器 + 两侧 `Subject` 精确匹配）落地后，**顺序恢复为"先主后次"**（即重新暴露原症状的那种排法），期望值**一行未改**，读数仍为 `+25 / 计数增量 1`。⇒ **该串扰语义已在"最不利顺序"下被消除**，不是被夹具藏起来的。

### 1.6 回归面：6a 全部读数保持

| 读数 | 日志行（首轮） | 值 |
|---|---|---|
| 链里施加状态：身份与锚点解耦（23a–23h 未回归） | L3178–L3346 | 6a 的八条读数**逐条仍 PASS**（含 23h 的 `RunSource.Id=135 / 140` 互异、`CausedBy.Id=116` = 主探针实例锚点） |

---

## 2. 区段锚点（可复核）

**运行日志**：`E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\Task6b\LegendAutoChess-6b-frozen.log`

- **冻结快照**：4,938 行 / 698,237 字节 / SHA-256 `05218d83cbd0f3d1d2d2e5c1afd3d0490801bbaabe5114ffc7bcc4e3d583edd4`
- **为什么先冻结再切区段**：本轮日志**同进程跨了两个 PIE 世界**（`StopPIE → StartPIE`），`Saved/Logs/LegendAutoChess.log` 会被后续编辑会话继续追加 ⇒ 区段行号会漂。故先把整份日志**原样复制**成快照，全部行号 / 哈希以快照为准。

| 区段 | 行号（快照内） | 行数 | UTF-8 字节 | SHA-256 | 计数 |
|---|---|---|---|---|---|
| `Run`（首轮 PIE） | **L2441–L3493** | 1053 | 153,815 | `9c8ade405cae4908f36c06458460a6318a25b3fe3c38a7bf9bad222cecfd5b0c` | 65 / 0 → 72 / 0；红字 **恰 3**、零 `[FAIL]`、零 Ensure |
| `Reject` | **L3522–L3670** | 149 | 27,313 | `8692f61fee8c6a587fbc66313d8474e1b5bf48786842dda9d99e9c1b9a7aff70` | 11 / 0；故意红字见 §2.2 |
| `Run`（第二 PIE，同进程） | **L3847–L4897** | 1051 | 153,777 | `11324a202f2e409108daeaa618484608872ad1657ff6d35060a7642e1cf12de4` | 65 / 0 → 72 / 0；红字 **恰 3**、零 `[FAIL]`、零 Ensure |

**区段切法与哈希口径**：区段按 `Cmd:` 行到"下一条 `Cmd:` 行之前"**整段原始字节**截取（首行 = 该 `Cmd:` 行、末行 = 该轮收束汇总行），哈希对截出的**原始字节**（保留原行尾）计算 ⇒ 复算时 MUST 用 `-Raw` 读字节而不是 `Get-Content` 拼字符串（后者会把行尾规范化掉）。`Run` 第二轮的末行取 `===== 检查 20 段（延迟）收束：通过 72 / 失败 0 =====`（该轮之后无更多 `Run` 相关行）。

### 2.1 `Run` 区段内红字 = 恰 3 条，全部为既有预期

| 日志行（首轮） | 红字 | 归属 |
|---|---|---|
| L2702 | `LogTcsState: Warning: 状态施加被拒：定义未在本世界登记（DefTag=TcsEvent.State.Periodic）` | **检查 19f**（未登记定义被拒）的**故意**红字，紧跟其 `[PASS]` 行 |
| L3479 / L3480（两行） | `LogTcsState: Warning: 时长操作被拒：无限时值无到期条目可调（定义=StateDef.Probe.Infinite 句柄=0/65）` | **检查 20j**（无限时值时长操作 ×2）的故意红字 |

**本轮未新增任何故意红字**（任务 4.3 的硬约束）：新增的 23i–23k 三条检查、以及运行期"空载荷 / 异类型片段"的**运行期** Warning 路径**都没有被故意触发**（作者期那两条走 `DataValidationContext`，**不写 Warning**——故 `Run` 区段红字总数与 6a 相同）。

> **一处易误读点**（沿用 6a 的说明）：19f 的探针定义**故意**用了一枚事件词（`TcsEvent.State.Periodic`）当"未登记定义"的身份 ⇒ 那条 Warning 的 `DefTag=` 看上去像事件 tag，这是**夹具取词的形状**，不是"事件被当成定义施加"。

### 2.2 `Reject` 区段红字 = 装置头部逐条列明的那一批

`2 条登记失败 + 1 条 Error（未登记链）+ 1 条 Warning（ModifyFlow 降级）+ 状态面 9 条 Warning`，实测 `LogTcsState` 计 **9** 条 ✓、`EnsureFailed` **1** 处（`Chain.ChainId.IsValid()`，L3672）、`LogTcsEffect: Error` 1 条；另有 `LogOutputDevice` 的 ensure 转储为其伴随输出（`errors=62` 这个原始计数把多行堆栈都算了进去，**不是 62 个独立故障**——独立故障按上面这批归类）。

---

## 3. 复算脚本（逐字可跑）

```powershell
# ① 冻结快照哈希（应 = 05218d83…）
$frozen = "E:\Projects_Dev\LegendAutoChess\Saved\TcsAcceptance\Task6b\LegendAutoChess-6b-frozen.log"
(Get-FileHash -LiteralPath $frozen -Algorithm SHA256).Hash.ToLower()

# ② 三个区段的字节哈希（应分别 = 9c8ade40… / 8692f61f… / 11324a20…）
$bytes = [System.IO.File]::ReadAllBytes($frozen)
$text  = [System.Text.Encoding]::UTF8.GetString($bytes)
$lines = $text -split "(?<=`n)"
$starts = 0..($lines.Count-1) | Where-Object { $lines[$_] -match 'Cmd: Tcs\.Test\.Slice\.Run' }   # 应 2 条
$reject = 0..($lines.Count-1) | Where-Object { $lines[$_] -match 'Cmd: Tcs\.Test\.Slice\.Reject' } # 应 1 条
$plan = @(
  @{n='Run1';    s=$starts[0]; e=(0..($lines.Count-1) | Where-Object { $lines[$_] -match '段（延迟）收束：通过 72 / 失败 0' } | Select-Object -First 1)},
  @{n='Reject';  s=$reject;    e=(0..($lines.Count-1) | Where-Object { $lines[$_] -match '拒绝面汇总：通过 11 / 失败 0' } | Select-Object -First 1)},
  @{n='Run2';    s=$starts[1]; e=(0..($lines.Count-1) | Where-Object { $lines[$_] -match '段（延迟）收束：通过 72 / 失败 0' } | Select-Object -Last 1)}
)
$sha = [System.Security.Cryptography.SHA256]::Create()
foreach ($r in $plan) {
  $seg = ($lines[$r.s..$r.e] -join "")
  $b   = [System.Text.Encoding]::UTF8.GetBytes($seg)
  $h   = ($sha.ComputeHash($b) | ForEach-Object { $_.ToString("x2") }) -join ""
  "{0}: L{1}-L{2} 行数={3} 字节={4} SHA256={5}" -f $r.n, ($r.s+1), ($r.e+1), ($r.e-$r.s+1), $b.Length, $h
}

# ③ 计数与红字分类（应：3 条红字 / 0 条 FAIL / 0 条 Ensure）
$lines[2440..3492] | Where-Object { $_ -match 'Error:|Warning:|Ensure condition|Fatal error|\[FAIL\]' }
# ④ 新增读数（应 4 条 23i–23l）
$lines | Where-Object { $_ -match '检查 23[i-l]：' }
# ⑤ 清理与汇总（应 2 条"订阅 1→0"、2 条 65/0、2 条 72/0）
$lines | Where-Object { $_ -match '状态行为反初始化|即时判定汇总|段（延迟）收束' }
```

> **注**：脚本 ② 的 `-split "(?<=`n)"` 保留行尾，故 `-join ""` 拼回的字节与原始区段一致；这是 §2 里"哈希对原始字节"那条口径的可执行版本。

---

## 4. 缺陷与偏差记录

1. **一处身份缺陷（本轮发现，按裁定 8 甲案修复）**：`FTcsStateRegistry::AllocateSlot` 原先**每个桶各从 1 起发代际** ⇒ 两个单位各自的**首个**实例都会得到 `Handle{Index=0, Generation=1}`，而 `GetState(Handle)` **扫描所有桶**、`FTcsStateHandle` 里又**没有单位段** ⇒ **拿 A 单位首实例的句柄去查会命中 B 单位的首实例**（错误的单位 / 错误的移除 / 事件误路由）。**修法 = 出线（`TcsStateRegistry.cpp` 文件作用域）进程唯一**发号器：每次分配取**不复用的正奇数**、释放 `+1` 置偶数、**跨世界不重置**、`int32` 正空间耗尽 `Fatal`（不回绕）。判据读数 = **23j 的 `旧句柄0/57拒绝`**（同 PIE 内旧句柄已失效）+ **23l 第二轮的 `旧句柄 0/59 被拒`**（跨世界不重置）。被否的乙案（给 `FTcsStateHandle` 加 `Unit` 段）**需要迁移反射面与全部消费者**，本轮只把 `Unit` 加到**事件载荷**上。
2. **一处收窄修正（`STAT-5` 守卫的宽度）**：最初把"正在移除"判据写成 `Found->Phase == ESP_Expiring` ⇒ **过宽**——`RefreshStacked` 在刷新期间**也临时用 `Expiring`**（`TcsStateOps_Stack.cpp:235–249`）⇒ 合法的一次 `Refreshed` 自移除会被**静默拒绝**。**修法 = `InRemovalBroadcastHandles` 旁表**，**只**在**真正的移除广播窗口**标记该句柄（进入广播前加入、广播返回后移除）。23k 的 `Applied/Refreshed自移除=成/成` 与 `外部Removed重入次数=1期望1、返回=false` 是这条修正的两面读数。
3. **四处跨广播悬空写（代码评审发现，本轮修掉，无独立读数）**：`RefreshStacked` 跨两次广播持 `Live` 指针对照；`EPR_Immediate` / `PushPeriod` 回调在 `Broadcast` 返回后仍读实例；`TcsStepApplyState` / `TcsStepModifyAttribute` 在副作用之后仍读 `Run` / `Context` / `Step`。**统一修法 = 副作用前取值快照、副作用后按句柄重查**（"广播后身份重查"那条规格的落地）。**这四条没有专门读数**——它们只在"回调里改世界"的组合下才会暴露，本轮实测覆盖的是 `Applied` / `Refreshed` 自移除这一支（23j 子读数）。
4. **一处规格文本订正（职责归属）**：`FTcsStateEventPayload` 原**没有 `Unit` 字段**，而 Task 0.1 的描述假设"读取器能拿到施加方"——实际只能拿到 `Instigator` 与 `Handle`。故本轮**给载荷新增反射字段 `Unit`** 并从 `Instance.Unit` 填入，读取器的 `Caster` 才写得出"有效 `Instigator`、否则退 `Unit`"。这条订正**同时**是 23i 的一个读数面（`读取器Caster=Instigator/退Unit`）。
5. **一处任务文本订正（"零实现方"不成立）**：任务与提案原写"该注册表属主模块自登记读取器场景至今**零实现方**"——**事实错误**（`TcsDamage` 早已登记 `FTcsDamageFlowCollectEvent` 的读取器）。已改为"本轮补**首个状态**读取器"。
6. **一处评审发现（未修，登记为 `STAT-9`）**：在**终止移除广播窗口内**对**同一 `Target` + 同一 `DefTag`** 重新 `ApplyState`，可能**刷新那个尚未释放的 `Expiring` 实例**、重新挂上修正器与时间条目，随后被外层移除一并释放 ⇒ 遗留一份无人回收的修正器。**本轮不改语义**（复活 / 排队 / 拒绝三种策略都还没有真实内容消费者），**只登记**。⇒ **`STAT-5` 的有限重入保证 MUST NOT 被外推为"任意递归组合都安全"**：它承诺的只有"**递归 `Remove` 同句柄返回 false 且不重复广播 / 释放**"这一支。
7. **一处工程事实（首编两处 warning，已修）**：`FTcsStateBehaviorFragment` 引入虚函数后 C4265（"有虚函数但析构非虚"）出现于插件与宿主样本 ⇒ 给基类补 `virtual ~FTcsStateBehaviorFragment() = default;`；同时 `FGCObject::AddReferencedObjects` 的裸 `UTcsStateSubsystem*` 成员触发增量 GC 口径提醒 ⇒ 收窄为 `TObjectPtr`。**修后双配置编译零 warning / 零 error**。

---

## 5. 如实边界清单（未覆盖面，逐条）

1. **单机 / 单进程 / 双 PIE 世界**：全部读数来自**同一个编辑器进程内的两次 PIE 会话**（`StopPIE → StartPIE`）。无网络、无多世界并行、无 Mass 路径、**无 Shipping 运行期**验证（Shipping 只验编译）。
2. **行为片段的"刷新路径"未正面覆盖**：23j 的 `订阅4→4不重挂` 证的是**订阅数不变**；`WireBehaviors` 在刷新路径上**本就不被调用**，这条"不重挂"是**由设计保证**并被计数侧面印证，**不是**在刷新回调里逐条核对片段清单得来的。
3. **空载荷 / 非派生片段的运行期 Warning 路径未故意触发**：本轮只验了**作者期** `IsDataValid` 的两条规则（23k 的 `空载荷=0错误1；异类型=0错误1`，走 `DataValidationContext`、**不写日志**）。运行期"片段无效 ⇒ 跳过 + Warning"那条分支**零读数**。
4. **未绑定全局触发行与外部无 `Subject` 事件的兼容性**：保留原有 Tag 路由这条，靠的是**代码评审 + 既有整轮回归**（检查 8–16 的触发行语境全部仍 PASS），**没有**专门的"全局行仍然生效"场景。
5. **typed 自定义载荷 + 自定义 reader 给出 `Subject` 的情形**：Fragment **不做**泛化 `Subject` 过滤（按兴趣扇出），`Subject` 只约束 effect-trigger 的绑定行。这条**只有规格与实现，无自定义载荷场景的读数**。
6. **`Expired` 路径的行为退订未单独读数**：23k 覆盖**显式 `Removed`**；到期走**同一条移除链**（同一个 `UnwireBehaviors`），但"到期路径"本身零读数（与 6a 的第 1 条边界同款）。
7. **任意递归增删的终止性不承诺**：`STAT-5` 只保证"**递归 `Remove` 同句柄返回 false**"这一支；`STAT-9` 那一支（终止广播内同定义重施）**明确未修**。回调里"施加新状态 / 注销单位 / 追加移除别的句柄"等组合未系统覆盖。
8. **`Interests` 重复项去重**只验了"同一片段内配置重复 Tag"（23j 的 `重复兴趣去重subs4/instances2`）；**跨片段**兴趣去重（两个片段兴趣同一 Tag）走的是同一 `AddUnique` 路径但无独立读数。
9. **行为回调的脚本可达面为零**：虚分派在脚本侧物理不可达（与决策 Fragment 同款，归 `LEDGER-reflection` R-2 一族）。
10. **`Interests` 的层级匹配不做**（精确 Tag 匹配，裁定 5）：配置父 Tag 不会命中子 Tag 事件。
11. **跨单位 / 跨定义的隔离**只验了"两单位两定义"的**一档组合**（23j）；更多单位、同单位多实例同 Tag、以及"实例在回调里被换成另一个定义"等组合未覆盖。
12. **一处设施事实**：23l **故意不清理**结尾哨兵（否则下一轮看不到 `1→0`）⇒ 每跑完一遍 `Tcs.Test.Slice.Run` 都会在世界里留一枚哨兵实例；这是**夹具的有意行为**，不是泄漏（世界拆解会清掉，读数即 L3738 / L4931）。

---

## 6. 引擎 / 机制事实（本轮实测或源码核实，供后续轮）

1. **`UGameplayMessageSubsystem`（事件总线）的订阅表对 Handler 持弱引用** ⇒ 共享 Handler **必须**由门面以 `UPROPERTY` 强持有，否则会被 GC 掉、订阅**静默失效**（无红字、无日志，症状 = 行为回调永远不响）。⇒ 凡是"总线订阅 + 独立 Handler 对象"的组合，持有关系 MUST 有一张显式清单。
2. **每 Tag 一条订阅 + 计数配对**是本场景的**唯一可行路**：总线 `Subscribe` 只收 `UTcsEventHandler*`、派发只传 `(EventTag, Payload)` ⇒ 回调**无法自辨身份**（拿不到订阅句柄）。故身份只能靠**旁表**（事件 Tag → 候选句柄）认领，每实例各订一次**收益为零、成本为正**（订阅表随实例数膨胀、退订易漏）。先例 = `FTcsChainEventWaitRegistry`。
3. **`FInstancedStruct` 的"泛化识别"必须用带类型参数的 `GetPtr<T>()`**：`GetPtr<FTcsStateEventPayload>()` 在**类型不符时返回 `nullptr`** ⇒ 这正是"状态载荷自筛、其余载荷按兴趣扇出"那条分支的**判别式**（比"读 `Cause` 猜类型"之类的办法可靠，且不引入对上层类型的依赖）。
4. **`UScriptStruct::CompareScriptStruct(A, B, 0)` 是"逐字段值相等"的现成口径**：用于两侧 `Subject` 的**精确匹配**（先比 `GetScriptStruct()` 同一性、再比值相等）。⇒ 需要"泛型载荷主体相等"判据时，不必自己序列化比较。
5. **反射类型作为泛型容器键**（`FTcsTriggerPayloadReaderRegistry` 用 `const UScriptStruct*` 作键）在本轮**第二次**被证明可用：`UE_DEFINE_TRIGGER_PAYLOAD_READER(FTcsStateEventPayload, …)` 与既有 Damage 读取器**并存**、互不干扰。⇒ "首个状态读取器"落地**不需要**改注册表本体。
6. **C4265（有虚函数但析构非虚）在本工程口径下按 warning 处理**：`USTRUCT` 抽象基类**要么**把所有虚函数清掉、**要么**补 `virtual ~T() = default;`。⇒ 新增虚回调的 USTRUCT 基类 MUST 同时给虚析构（`= 0` / `PURE_VIRTUAL` 仍禁用，见 Task 5 纪律）。
7. **增量 GC 口径下 `FGCObject` 的引用成员用 `TObjectPtr`**：裸指针成员会触发"引用收集用旧接口"的提醒；`FGCObject` 派生类的对象引用成员统一 `TObjectPtr<T>`。
8. **"有限重入"的守卫宽度必须按"真正的窗口"划，不能按"看起来像的状态"划**：`Expiring` **不是**"正在移除"的同义词（刷新过渡态也用它）⇒ 守卫 MUST 用**旁表**在**进入广播前后**成对标记 / 解除（本轮实测的反面教材见 §4 第 2 条）。

---

## 附：本轮交付面（口径索引）

- **规格口径**：`SPEC-02-states` **§12.10**（Task 6 落地口径，6b 增补）、§12.1 表 §5 行（行为 Fragment 订阅挂接由"随提案"改为**已落地**）、§5 正文（接线顺序两面）
- **提案**：`add-state-behavior-fragments`（**1 新能力 + 3 改能力 / 4 个 delta**：`state-behavior-fragments` **4 ADDED**、`state-def-asset` 1 MODIFIED、`effect-trigger` 3 MODIFIED、`state-instance-lifecycle` 4 MODIFIED）——本文件是其验收证据，归档名 `2026-10-05-add-state-behavior-fragments`
- **台账**：`TRIG-6`（内联触发行 Tag 级跨实例串扰）**已落地**；`STAT-5`（有限重入）**已消费**；本轮新增 **`STAT-9`**（终止广播内同定义重施）
- **宿主装置**：`Source/TcsDev/Public|Private/Dev/TcsDevBehaviorSample.h/.cpp`（行为样本 + 重入观测器）、`TcsDevSliceRig_Behavior.cpp`（检查 23i–23l 本体）、`TcsDevSliceRig.cpp`（挂接点 + 23g 顺序恢复）、`TcsDevStepProbeRunSource.h/.cpp`（探针新增 `Caster` 读数）
