# 状态实例生命周期落地：per-unit 桶 + 世界门面 + 六枚生命周期事件 + 定义逐世界登记

- **文档 ID**：`EVID-2026-10-04-state-instance-lifecycle`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：R5 Task 2（`PLN-R5`）的验收面——状态实例**可建可查可删**、全生命周期广播到总线且订阅者同拍收得到、per-unit 桶的代际校验挡住悬空句柄且不误伤、以及状态定义**逐世界登记**进状态门面这条新装配路径
- **最后更新**：2026-10-04
- **被测对象**：`Source/TcsState/Public/State/{TcsStateHandle.h, TcsStateInstance.h, TcsStateRegistry.h, TcsStateOps.h, TcsStateEvents.h}` +
  `Private/State/{TcsStateRegistry.cpp, TcsStateOps.cpp, TcsStateOps_Query.cpp, TcsStateOps_Events.cpp}` + `Public/TcsStateSubsystem.h` + `Private/{TcsStateSubsystem.cpp, TcsStateSubsystem_Definition.cpp}` + `Public/State/TcsStateEnums.h`（补三枚举）；
  `Source/TcsIntegration`（`SeedWorld` 第三条装配路径 + 头注释口径）；宿主 `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（**检查 19a–19f** + 拒绝面 **检查 E/F/G**）+ `Dev/TcsDevScreenObserver.h/.cpp`（状态事件计数与屏显）+ `Dev/TcsDevBootstrap.h/.cpp`（三枚订阅 + 观测者取用口）
- **验证范围**：TCS 插件 + LAC 宿主。**代码改动面 = 20 文件**（插件 12 新 + 4 改；宿主 4 改——其中 `TcsDevSliceRig.cpp` 两处：常规检查块 + 拒绝面检查块）
- **完整来源**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess-backup-2026.10.04-10.55.26.log`（**冻结备份**，379,296 字节，整文件 SHA-256 `9feacb21eb9a7724b8d5191769904892984f74e2e3cc7fd103ad4bb3dbaff03b`）
  - **常规验收区段 = L2420–L2711（292 行）**，区段 SHA-256 **`357c274834c2e6c5a4afc593f5d116a16bf717e41c49c33821756fbf8e33cd11`**（40,582 字节）
  - **拒绝面区段 = L2416–L2499（84 行）**，来源 = 活动日志 `LegendAutoChess.log`，区段 SHA-256 **`125b191fc0f282b0b4606420c2d482d74be14653f6b9b0460ebcacff4b58b92b`**（13,943 字节）
  - **哈希口径**：区段文本按行以 `\n` 连接 + 末尾补一个 `\n`、UTF-8 编码后取 SHA-256——范围写明故可复算。**两次命令住两个日志文件**（跑拒绝面时编辑器写满一轮，常规验收的那一份已滚成 `-backup-2026.10.04-10.55.26`）：故常规区段锚冻结备份、拒绝面区段锚活动日志。**日志时间戳为 UTC，本地时间 = UTC + 8 小时**（`10.53` = 本地 `18:53`）
  - **区段外的前置锚点**（同一 PIE 会话启动期，供交叉核对，**不在上列哈希范围内**）：L2332（发现状态定义资产 1 个）、L2333（就绪行含状态定义 1 条）、**L2358（世界装配行现含"状态定义 1/1 条"）**
  - **复算脚本（两条区段同一段 PowerShell，`%b` = 上列备份全路径、`%a` = 活动日志全路径）**：
    ```powershell
    function Hash([string]$f,[int]$s,[int]$e) {
      $l = Get-Content -LiteralPath $f -Encoding UTF8
      $t = (($l[($s-1)..($e-1)]) -join "`n") + "`n"
      $b = [System.Text.Encoding]::UTF8.GetBytes($t)
      "L$s–L$e 行数=$($e-$s+1) 字节=$($b.Length) SHA256=" + `
        ((([System.Security.Cryptography.SHA256]::Create().ComputeHash($b)) | ForEach-Object { $_.ToString('x2') }) -join '')
    }
    Hash '<%b>' 2420 2711   # 期望 357c2748…cd11（292 行 / 40582 字节）
    Hash '<%a>' 2416 2499   # 期望 125b191f…b92b（84 行 / 13943 字节）
    ```

## 0. 夹具与前置

**内容侧零新增资产**：沿用 Task 1 建的 `/Game/TcsDev/Checks/DA_Check_BuffDef`（`DefTag` = `Buff.Def.StatusTag` = `StateDef.Check.Burn`）。本 Task 的**唯一内容依赖**就是它——不新增资产恰好是"定义 → 实例"这条链需要的全部输入。
**既有夹具不变**：链资产 9 个、触发定义 2 条、属性定义 4 个，装置自造两个单位。

**编译前置**：`LegendAutoChessEditor Win64 Development` 与 `LegendAutoChess Win64 Shipping` **双配置 `Result: Succeeded`**、日志 `error`/`warning` 命中数均 0；产出 `UnrealEditor-TcsState.dll`（221,696 字节）。
**执行方式**：编辑器内起 PIE（世界 `Untitled_1`）后人工敲两条命令——常规验收 `Tcs.Test.Slice.Run`，拒绝面 `Tcs.Test.Slice.Reject`。两者都只在 PIE/Game 世界成立 ⇒ **人工验证点**（装置不提供无人值守入口）。

## 1. 常规验收：即时判定与汇总

| 行号 | 读数 | 判定 |
|---:|---|---|
| 2560 / 2566 | `[PASS] 检查 9：对照组（资产行未点灯，门③不通过）` / `检查 10：**资产路径**生效（点灯后减半）` | 修改器通道未回退 |
| 2569 / 2572 | `[PASS] 检查 11：提交生效（挂 C++ 行 ⇒ 两笔 0.5 叠加）` / `检查 12：摘行还原（按句柄摘除返回 true）` | 事务与摘除未回退 |
| 2575 / 2578 | `[PASS] 检查 13：重挂生效` / `检查 14：按来源级联摘除（摘掉 1 条）` | 级联摘除未回退（`WAIT-6` 行为级实证保持） |
| 2634 / 2657 | `[PASS] 检查 15：载荷读取器（施法者被扣 7.0）` / `检查 16：反向对照（摘掉载荷行 1 条）` | 载荷与来源隔离未受影响 |
| 2658 / 2659 | `[PASS] 检查 17：触发定义资产作者侧门（Valid 2/2）` / `检查 18：状态定义资产发现与作者侧门（Valid 1/1）` | Task 1 的门未回退 |
| 2660 | `[PASS] 检查 19a：状态定义已逐世界登记（门面内 1 条；验收定义时值策略=Finite）` | **★ 新装配路径**（见 §2） |
| 2663 | `[PASS] 检查 19b：施加建实例 + TcsEvent.State.Applied 到达（句柄=0/1 在册=是 来源=14；事件计数 0→1）` | **★ 本批核心判据**（见 §3） |
| 2666 | `[PASS] 检查 19c：重复施加命中同组 ⇒ Refreshed（结果=1；Applied 0→1 未增；Refreshed 0→1）` | 共存决策暂用位成立 |
| 2669 | `[PASS] 检查 19d：移除 ⇒ Removed（原因可读=是；Removed 0→1；旧句柄查询被拒=是）` | 撤销 + 悬挂 |
| 2677 | `[PASS] 检查 19e：单位注销逐条 Removed（移除 1 条；Removed 1→2；该单位在册 0）` | 批量注销 |
| 2679 | `[PASS] 检查 19f：未登记定义被拒（结果=3；期望 3 = Rejected）` | 内容缺口拒绝面 |
| 2683 | `===== 即时判定汇总：通过 26 / 失败 0（检查 7b 见延迟判定）=====` | **★ 即时判定 26/0**（原 20 项 + 新增 19a–19f） |
| 2702 | `[PASS] 检查 7b：WaitDelay 到期后续走，Damage 步扣血生效` | **★ 延迟判定通过** |

**全区间 `[PASS]` 行 27 / `[FAIL]` 行 0**（26 项即时 + 7b 延迟）。

**前置锚点（区段外，启动期读数）**：L2332 `发现状态定义资产 1 个`（发现面不变）｜L2333 就绪行含 `状态定义 1 条`｜**L2358 `已装配到世界 Untitled_1——链定义 9/9 条，触发行 2/2 条，状态定义 1/1 条`**——世界装配行**新出现第三段计数**，即"定义逐世界登记"这条改造的独立于装置的证据（早于两条命令 7 秒）。

## 2. 关键读点之一：定义怎么到运行态（19a + 装配行）

Task 1 的 `integration-entity` 规格写"状态定义只进缓存、不被装配到世界"（当时没有消费方），Task 2 把它改为"**缓存 + 逐世界登记进状态门面**"。两条证据合起来把"两步都在"钉死：

| 证据 | 读数 | 为什么是判据 |
|---|---|---|
| L2358（启动期） | 世界装配行末段 `状态定义 1/1 条` | 装配路径**真的**把定义写进了世界门面——且与链/触发行同一次装配、同一条日志 |
| L2660（19a） | `门面内 1 条；验收定义时值策略=Finite` | 门面侧**查得到**，且读到的是**定义内容**（时值策略取自 `FTcsBuffDef`）而非只有一个空身份 |

**依赖方向仍是单向的**：定义库（`TcsIntegration`）→ 状态门面的登记口，门面侧零 `TcsIntegration` 引用（否则成环）。这一点由"编译通过"本身即证：若门面反向 include 定义库，`TcsState` → `TcsIntegration` → `TcsState` 的模块环会在 UBT 阶段被拒。

## 3. 关键读点之二：**广播早于释放**（19b 的相邻两行）

19b 判据里"订阅者收到的载荷对得上本实例"这条，证据是**两行的先后顺序**：

| 行号 | 内容 |
|---:|---|
| 2661 | `[TcsDevScreen] [状态事件] TcsEvent.State.Applied：句柄=0/1 定义=StateDef.Check.Burn 来源=14 发起=2 层数=1 等级=1 …` |
| 2662 | `LogTcsState: 状态施加：单位=2 定义=StateDef.Check.Burn 句柄=0/1 来源=14 等级=1` |

`UTcsDevScreenObserver::HandleEvent_Implementation` 是**真实订阅者**（`UTcsDevBootstrap::SubscribeScreenObserver` 以立即通道订阅三枚状态 tag），它在回调里读载荷全文并写下 `LastStateEventSummary`。L2661 在 L2662（广播者自己那句"状态施加"日志）**之前** ⇒ 回调是在 `Apply` 内部、槽位已分配但**尚未返回**的那一拍跑的，**且读到了句柄/来源/层数/等级全套字段**。

这正是"`Phase → Expiring` → 广播 → 归还槽位"这条撤销顺序想保的东西：**若改成"先释放再广播"**，订阅者拿到的会是已清零的载荷（句柄无效）——19b 的这条判据会立刻失败。故它不只是"事件到达"的证据，也是**顺序纪律**的证据。

**三条附证**：① 19c 刷新后句柄仍是 `0/1`（同一实例复用，非新建；`Applied` 计数未增）；② 19d 移除后旧句柄查询被拒；③ 19e 单位注销后该单位在册 0。

## 4. 拒绝面：7/7，且"拒绝不误伤"被单独钉住

| 行号 | 读数 | 判定 |
|---:|---|---|
| 2420 | `[PASS] 检查 A：双真相资产被 IsDataValid 拒（1 条错误）` | 既有（作者侧门） |
| 2471 | `[PASS] 检查 B：空 ChainId 链被拒（登记 API 拒绝空 id）` | 既有 |
| 2473 | `[PASS] 检查 C：未登记链 id 触发被拒（Error 日志 + 无效句柄，不崩溃）` | 既有 |
| 2478 | `[PASS] 检查 D：ModifyFlow 无流程载荷时降级` | 既有 |
| 2480 | `[PASS] 检查 E：无效目标被拒（结果=3；期望 3 = Rejected；预期 1 条 Warning）` | **★ 本批新增** |
| 2482 | `[PASS] 检查 F：未登记定义被拒（结果=3 = Rejected；预期 1 条 Warning）` | **★ 本批新增** |
| 2492 | `[PASS] 检查 G：悬空句柄被拒且不误伤（首施=0 首移除=成功 旧句柄查询被拒=是 再移除被拒=是 无辜实例存活=是）` | **★ 本批新增（见下）** |
| 2498 | `===== 拒绝面汇总：通过 7 / 失败 0 =====` | **★ 7/7** |

**检查 G 的证据链（三行连读，slot 复用的真实路径）**：

| 行号 | 读数 | 说明 |
|---:|---|---|
| 2486 / 2488 | `状态施加 … 句柄=0/1` → `状态移除 … 句柄=0/1 原因=1` | 建实例（槽位 0 代际 1）→ 移除（代际 +1 使 `0/1` 悬空） |
| 2490 | `状态施加 … 句柄=0/3` | 再施加 ⇒ **复用槽位 0**、代际 3 ⇒ 旧句柄 `0/1` 的 `Index` 正指向这个新实例 |
| 2491 | `Warning: 状态移除被拒：句柄悬空（0/1）` | 用旧句柄再移除：**被代际校验挡住** |
| 2492 | `… 再移除被拒=是 无辜实例存活=是` | 且**没有误摘**那个刚复用该槽位的新实例 |

**为什么必须真施加再真移除**：代际只在**槽位释放**时 +1，"曾经有效、现已被回收"的句柄**无法凭空构造**——故这条既是拒绝面证据，也是 `RemoveState → ReleaseSlot → 槽位复用` 这条链的回归证据。

**预期红黄字（本命令的合法产物）**：3 条状态 Warning（L2479 / L2481 / L2491）+ 既有 2 条登记失败 + 1 条 Error（L2472 未登记链）+ 1 条 ModifyFlow Warning（L2476）+ 1 处**故意**的 `Handled ensure`（L2426–L2467，来自检查 B 的 `RegisterChain` 空 id 拒绝面——**既有行为，非本批引入**）。

## 5. 零红字扫描（**只对常规验收命令成立**）

常规验收区段 **L2420–L2711** 内 `Error:` / `Ensure condition` / `Fatal error` / `[FAIL]` **零命中**；`Warning:` **恰 1 处**，即 L2678 的 `LogTcsState: Warning: 状态施加被拒：定义未在本世界登记（DefTag=TcsEvent.State.Periodic）`——它是 **19f 判据的预期产物**（"未登记定义被拒"这条路必须留一条 Warning，否则拒绝就成了静默失败）。
**拒绝面命令不受此约**：`Tcs.Test.Slice.Reject` 的自我声明就是"本命令故意触发校验失败与 Error 日志"（L2417–L2419 两行表头即该声明）。

## 6. 本证据证明什么 / 不证明什么（MUST NOT 外推）

- **证明**：① 状态定义**逐世界登记**进状态门面（装配行 + 19a 两条独立证据）；② 实例**可建可查可删**（19b/19c/19d）；③ 全生命周期事件面**真实到达订阅者**且**在回调那一拍实例可读**（§3 的相邻两行）；④ 重复施加走刷新而非新建（19c 句柄不变 + `Applied` 未增）；⑤ **代际校验挡住悬空句柄且不误伤**（§4 检查 G 的 slot 复用链）；⑥ 既有 20 项检查与延迟判定**逐项未回退**（26/0 + 7b，零红字）。
- **不证明（七条边界）**：
  ① **只跑单轮**——未做"两条命令两轮关键行逐字一致"（本批无既有行为改动，故未重复该更强口径）；
  ② **时值面零覆盖**：`DurationRemaining` / `PeriodRemaining` / `ExpiryEntry` 只有字段落点，**没有一条真实到期或周期被验**——`ExtendDuration` / `SetRemaining` 的 `Infinite` 拒绝面亦**未实测**（无 `Infinite` 定义资产可用），归 Task 3；
  ③ **快照面零覆盖**：`Overrides` 形参在两次命令里都传空表且不被消费，参数求值/等级源/值约定转换全归 Task 3；
  ④ **修正器面零覆盖**：本批**不触碰** `UTcsAttributeSubsystem`——buff 还不会改数值（`ModifierRows` 物化归 Task 4）；
  ⑤ **`Stacked` 档与五轴未验**：19c 的"同组"判据是"同单位 + 同 `DefTag`"（`GroupBy = None` 语义），是 **Task 5 五轴共存决策的暂用位**；`EAR_Stacked` 本轮不可达、`MaxStacks`/溢出/数值叠加/时长刷新四轴零行为；
  ⑥ **`Cancelled` 无内建产生者**：19d 的原因由装置显式传入（关系表族归 R5.5-e）；
  ⑦ **`TcsEvent.State.StackChanged` / `Periodic` 两枚 tag 本轮零广播**（前者随 Task 5、后者随 Task 3）——本证据只证六枚 tag **声明**成立（编译级 + 原生注册）与其中三枚的真实到达。

## 7. 与 Task 1 证据的一处口径更新（防跨文档误读）

`EVID-2026-10-04-tcs-state-def-asset` 的 §1 前置锚点表把"世界装配行不含状态定义"记为"**★ 状态定义不做世界装配的反证**"。**那条读数今天依然成立**（它记的是该时刻的事实），但**规格口径已由本批改变**：`integration-entity` 现要求状态定义**在装配世界时逐世界登记**（本证据 §2）。两处并读时的正确读法：Task 1 证明的是"**缓存那一步**不建每世界结构"，本批新增的是"**装配那一步**把定义写进门面"——两步而非互斥。
