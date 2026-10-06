# EVID-2026-10-06-trigger-host-slots（R6 Task 0：`R-2` 宿主脚本插槽后段）

> **结论一句话**：宿主项目用**纯 C#**（无 C++、无蓝图）定义的两个插槽内容，在**真实原生 GC 之后**仍被框架保活且**仍被抵达**——条件求值器与载荷读取器各被调用，且**条件取值真的门控了链**（绑 3 行 = 2 行条件过 + 1 行条件不过 ⇒ 只扣 2 份血，不是 3 份）；"每事件一次"语义在**载荷读取**上成立（一次广播 ⇒ 读取器增量**恰为 1**，与绑了几行无关），而条件求值按**行**计（增量 3）。两个登记的**拒绝面** 11 项判据全部取自登记口**返回值**（8 项条件侧 + 3 项载荷侧），不靠"日志里有没有红字"。
>
> **证据纪律**（照 `EVID-2026-10-05-state-stacking-policies` 先例）：区段哈希 + 复算脚本 + **如实边界清单**。本文每条读数对应日志里的一行；"未覆盖"的面一律进 §5，不用推断冒充实测。

---

## 1. 读数（逐条对应日志一行）

**命令序列**（`UTcsDevGcProbeDriver` 驱动，四条独立命令）：

| # | 命令 | 判据 | 本轮读数 |
|---|---|---|---|
| 1 | `Tcs.Test.Gc.Arm` | 5 条 PASS | **通过 5 / 失败 0** |
| 2 | `obj gc` | 引擎真实原生 GC | `Collecting garbage and resetting GC timer.` |
| 3 | `Tcs.Test.Gc.Verify` | 7 条 PASS | **通过 7 / 失败 0** |
| 4 | `Tcs.Test.Gc.Reject` | 3 条 PASS（聚合 11 项布尔） | **通过 3 / 失败 0** |
|  |  | **合计** | **通过 15 / 失败 0** |

### 1.1 `Arm`（布置）—— 5 条

> 注：本轮实测的**最后两轮**（见 §3）逐字相同；下面取最后一轮，日志行为 `LegendAutoChess.log`。

| 读数 | 值 |
|---|---|
| 基线：选中目标 / 扣血 / 脚本流程步骤 | `选中 2 个目标（预期 2）/ 扣血 14（预期 14 = 每发 7 × 2 发）/ 脚本流程步骤调用 4（预期 4）` |
| 布置：五槽位挂进链/模板 | `五个槽位已挂进链/模板；驱动侧引用已清空，存活只取决于框架登记表` |
| **宿主条件求值器槽位** | `事件广播一次、绑 3 行 ⇒ 宿主条件求值器被调用增量 3（预期 3）；最近一次条件 ExpectedResult=False` |
| **宿主载荷读取器槽位** | `事件广播一次 ⇒ 宿主载荷读取器被调用增量 1（**预期恰为 1**——读取点在行循环之外，与绑了几行无关；读到 Marker=2026）` |
| **宿主插槽·账本落点** | `绑 3 行（2 行条件过 + 1 行条件不过）⇒ 链只跑条件过的那些、扣血 2（预期 2 = 单发 1 × 2 行）——**条件取值真的门控了链**（若返回值被忽略，会多扣 1 份）` |
| 收尾 | `[提示] 已登记被测脚本对象 7 个；夹具已挂进框架的链/模板，GC 后应当存活` / `GC_READY：请在控制台执行 obj gc，然后跑 Tcs.Test.Gc.Verify` |

**为什么"扣血 2"是必要条件而非充分条件**：只绑恒过的行时，"返回值被尊重"与"返回值被忽略（恒当通过）"**读数完全一致**。绑 3 行（含 1 行恒不过）后两者才分开——忽略返回值会扣 3 份，尊重则扣 2 份。

### 1.2 `Verify`（GC 后复核）—— 7 条

**GC 前置**：`已执行过 Unreal obj gc`（日志有 `Collecting garbage and resetting GC timer.`；装置自己也打印提示：`下列结果只有在此前日志能证明执行过 Unreal obj gc 时才是 GC 证据`）。

| # | 读数 | 值 |
|---|---|---|
| 0 | 死活判定 | `GC_CHECK_BEGIN：被测对象 7 个，存活 7 个（弱引用判定——引擎里唯一在 GC 后安全的死活判据）` |
|  | 逐槽位存活（7 行） | `选择器` / `过滤器` / `伤害委托` / `流程执行器` / `参数源` / **`宿主条件求值器（UTcsDevGcHostConditionEvaluator）`** / **`宿主载荷读取器（UTcsDevGcHostPayloadReader）`** —— 全部 `存活` |
| 1 | GC 存活性 | `被测脚本对象在真实原生 GC 后仍存活（装置侧弱引用仍可解析）` |
| 2 | 选择器/过滤器槽位 | `GC 后仍选中 2 个目标（预期 2——脚本选择器与过滤器仍被抵达）` |
| 3 | 流程执行器槽位 | `脚本流程步骤调用增量 4（预期 4 = 每发 2 步 × 2 发）` |
| 4 | 伤害委托槽位 | `扣血 14（预期 14——C# 公式仍被抵达）` |
| 5 | 参数源槽位（行为面） | `GC 后第二发 Damage 的 DamageBase 仍取到脚本参数源值 13（装置日志里应仍见 incoming=13）` |
| 6 | **宿主插槽·GC 后行为面** | `GC 后再次广播：链再扣血 2（预期 2 = 单发 1 × 2 条件过行、1 行条件不过不起链）——宿主条件求值器与宿主载荷读取器两个脚本对象都仍被两张注册表保活且都被抵达` |
| 7 | 正例通过 | `脚本对象在真实原生 GC 后仍存活，且各槽位行为与调用计数符合基线` |
|  | 收尾 | `GC_CHECK_END` |

**这一条是本 Task 的核心判据**：#6 把"**保活**"与"**仍被抵达**"绑在一起——只证存活不够（可能活着但注册表已丢指针），故 GC 后**再广播一次**并读账本变化。

### 1.3 `Reject`（拒绝面阴性对照）—— 3 条聚合 / 11 项布尔

| 读数 | 值 |
|---|---|
| 条件拒绝面（8 项） | `空键被拒=True / 空宿主被拒=True / 首个登记成功=True / 同键重复被拒=True / 撤销成功=True / 撤销后重登记成功=True / 内置键不可撤=True / 内置键登记被拒=True（八项全真为通过）` |
| 载荷拒绝面（3 项） | `空键被拒=True / 空宿主被拒=True / 同键重复被拒=True（三项全真为通过）` |
| 汇总 | `拒绝面全部如期：每个登记口都拒绝了非法输入、且判据来自返回值` |

**判据取自返回值**（装置自述：`本命令的**全部**红字均为预期——每条检查靠登记口的返回值判定，不靠'日志里有红字'`）。

**末两项是"免费"的既有控制面**：`内置键不可撤` / `内置键登记被拒` 用的是**内置静态自注册**的既有条件类型——它本来就不在 `Lifetimes` 表里，故 `Unregister` 必须返回 `false`、重复 `Register` 必须被拒。**这两项若不成立，说明动态/静态双轨边界被破坏了**，而不是新功能有 bug。

### 1.4 与预期不符的一处读数（如实登记）

**第 2 轮 `Reject` 未产生任何 `EnsureFailed` 红字，第 1 轮产生了 12 条**，而两轮的 11 项布尔**全为 `True`**。

- **原因（有引擎源码依据）**：`ensure` 的**同一调用点每会话只打印一次** callstack/报告——`Engine/Source/Runtime/Core/Public/Misc/AssertionMacros.h:364-365`：
  > *"By default a given call site will only print the callstack and submit the 'crash report' the first time an ensure is hit in a session; `ensureAlways` can be used instead if you want to handle every failure"*
- **本装置共 6 个 `ensureMsgf` 调用点**（`TcsEffectSubsystem_HostSlots.cpp` 的行 `40` / `45` / `61` / `106` / `111` / `120`）。第一轮包含 `Ensure condition failed` 的日志行共 12 行：每个调用点分别出现在 `LogOutputDevice` 与 `EnsureFailed` 两个通道，各通道 6 行。**日志行数不等于失败调用次数**。~~原解释：一轮最多产生 6 条，第 1 轮因 `&&` 短路与重入出现 12 条。~~ **该解释没有依据，撤回；本轮仅确认上述通道与调用点分布，不据此推断重入或多次执行。**
- **⇒ 这条读数反而正面印证了 `tasks.md 5.7` 的设计决定**：把通过判据放在**登记口返回值**上，而不是"日志里有没有红字"。若当初按"数红字"判据，第 2 轮会**假失败**。
- **MUST NOT** 把"第 2 轮没有红字"读成"拒绝面没生效"——同轮 11 项布尔全 `True` 且日志有正常的撤销/重登记记录（`已登记宿主求值器` → `登记已撤销` → `已登记宿主求值器` → `登记已撤销`）。

---

## 2. 区段锚点（可复核）

### 2.1 源码与产物

| 项 | 值 |
|---|---|
| TCS `HEAD` | `b213dea` |
| LAC `HEAD` | `eeaeb0f` |
| 引擎 | UE **5.8**（`EngineAssociation: "5.8"`） |
| `UnrealEditor-TcsEffect.dll` | `478720 B` / `2026-10-06 18:49:47` / `sha256:C8C8FB1267BCA487…` |
| `UnrealEditor-TcsDev.dll` | `480768 B` / `2026-10-06 18:43:05` / `sha256:4B08EAC1EE6EE1EA…` |
| `LegendAutoChessCS.dll` | `84480 B` / `sha256:8255CDEA2E1E87E6…` |

**产物 mtime 均晚于各自源码** ⇒ 跑的是本轮交付的二进制，不是陈旧产物。

> **`LegendAutoChessCS.dll` 的身份注记**：该文件在 `2026-10-06 21:47:52` 被编辑器的 C# 热重载**意外重建过一次**（编辑 `.cs` 注释即触发，产物一度变为 `84992 B`）。随后用正确的构建命令重建，**SHA256 逐字节回到 `8255CDEA2E1E87E6`（84480 B）** ⇒ 本文读数对应的产物身份**成立**，无需重跑 PIE。**教训**：`Script/*.cs` 只要落盘就会触发 UnrealSharp 重载，**取证后 MUST NOT 再动 `.cs`**（哪怕只改注释）。
>
> **本轮同时订正一处命令笔误**：先前记录的 `-Project=…\LegendAutoChss.uproject`（**少一个 `e`**）在自动化里会报 `GetProjectRootFolder` 异常并 `ExitCode=1`。正确命令见 §3.3。

### 2.2 本 Task 新增/改动的源码位置

| 文件 | 位置 | 内容 |
|---|---|---|
| `TcsEffect/Public/Trigger/TcsTriggerCondition.h` | `:105` / `:133-134` | `UINTERFACE(MinimalAPI, Blueprintable)` + `Test(...)` `BlueprintNativeEvent` |
| `TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h` | `:115` / `:127` | `UINTERFACE` + `Read(...)` + `FTcsTriggerPayloadRead` |
| `TcsEffect/Public/TcsEffectSubsystem.h` | `:200-203` / `:221-224` / `:242` / `:254` | 两个登记口 + 两个撤销口 |
| `TcsEffect/Private/TcsEffectSubsystem_HostSlots.cpp` | `:40` `:45` `:61` `:106` `:111` `:120` | **6 个 `ensureMsgf` 拒绝点**（预期红字的权威清单） |
| `TcsDev/Private/Dev/TcsDevGcProbe.cpp` | `:466` `:473` `:478` | 三条命令注册（`Arm` / `Verify` / `Reject`） |
| `TcsDev/Public/Dev/TcsDevGcProbeDriver.h` | `:108-109` | `PublishProbeEvent`（唯一一处 C++ 补强） |
| `Script/LegendAutoChessCS/TcsDevGcFixtures/TcsDevGcHostSlots.cs` | `:22` / `:61` | 两个宿主 `UObject` 实现契约 |
| `Script/LegendAutoChessCS/TcsDevGcFixtures/TcsDevGcHostSlotsProbe.cs` | `:345-416` | `VerifyHostSlotsNegatives()`（11 项） |

---

## 3. 复算脚本（PowerShell，逐字可跑）

**前提**：编辑器已开启 `console.CmdLink.enable 1`（开启命名管道 `\\.\pipe\UnrealEngine-CLI`）。**该步无法由脚本自身完成**——管道不开时无任何控制台通道；实际执行方式是**经编辑器 UI 的命令条**（`SlateInspectorToolset` 的 `tb3` 控件）输入一次。

```powershell
# 1) 开启控制台通道（每编辑器会话一次）。本步在编辑器 UI 命令条里执行：
#    console.CmdLink.enable 1
#    判据：以下路径出现
[System.IO.Directory]::GetFiles("\\.\pipe\") | Where-Object { $_ -match 'UnrealEngine' }

# 2) 发送一条控制台命令（协议：int32 ArgC + ArgC*(int32 Len + char[Len] 含 NUL)）
#    附完整客户端脚本，见 §3.2 的 _tmp_cmd.ps1 等价实现
```

### 3.1 从日志复算（无需控制台）

```powershell
$log = "E:\Projects_Dev\LegendAutoChess\Saved\Logs\LegendAutoChess.log"
$t = [System.IO.File]::ReadAllText($log, [System.Text.Encoding]::UTF8)
$lines = $t.Split("`n")

# 2.1) 四条命令各自的 PASS/FAIL 计数（按 ===== 横幅切段）
$banners = 0..($lines.Count-1) | Where-Object { $lines[$_] -match 'TcsDevGcProbe\].*=====\s*Tcs\.Test\.Gc\.(\w+)' }
"命令横幅数: $($banners.Count)"          # 期望 4（正常一轮）

# 2.2) 全日志 TcsDevGcProbe 判定行计数
$pass = ($lines | Where-Object { $_ -match 'TcsDevGcProbe\].*\[PASS\]' }).Count
$fail = ($lines | Where-Object { $_ -match 'TcsDevGcProbe\].*\[FAIL\]' }).Count
"PASS=$pass FAIL=$fail"

# 2.3) 两轮可复现性：取最后两轮的判定行序列，逐条比较
#     （判据 = 逐字相同，MUST NOT 比字节数/行数——见 tasks.md 6.3）
```

### 3.2 已用过的客户端（存档备查）
本轮实际用 PowerShell 的 `System.IO.Pipes.NamedPipeClientStream` 直连 `\\.\pipe\UnrealEngine-CLI` 实现；协议取自引擎插件源码 `Engine/Plugins/CmdLinkServer/Source/CmdLinkServer/Private/CmdLinkServer.cpp`：
- **发送**：`int32 ArgC`，随后 `ArgC` 组 `(int32 ArgLen, char[ArgLen])`，字符串**含结尾 NUL**（UTF-8）；`ArgV[0]` 是 exe 路径，**服务端跳过**；`ArgV[1..]` 以空格拼成命令行。
- **接收**：`int32 RespLen`，随后 `char[RespLen]`（**含结尾 NUL**）。
- **坑**：PowerShell 里 `[byte[]] + [byte]` 会退化成 `Object[]` ⇒ 必须用 `List[byte].AddRange`，否则请求长度错（表现为写成功但永无回包）。
- **编码**：回包是 **ANSI**，中文经此通道显示为 `?`。**权威读数 MUST 取日志文件（UTF-8）**，不要以控制台回显为准。

### 3.3 重建 C# 产物（本轮订正过命令笔误）

```powershell
& "E:\UnrealEngine\UE_5.8\Engine\Build\BatchFiles\RunUAT.bat" BuildUserSolution `
  -ScriptDir="E:\Projects_Dev\LegendAutoChess\Plugins\UnrealSharp\Build\Scripts" `
  -Project="E:\Projects_Dev\LegendAutoChess\LegendAutoChess.uproject" `
  -OutputPath="E:\Projects_Dev\LegendAutoChess\Binaries\Managed\net10.0" `
  -TargetConfiguration=Development clp=ErrorsOnly
```

- **成功形态**：`BUILD SUCCESSFUL` + `AutomationTool exiting with ExitCode=0 (Success)`。
- **笔误形态（本轮踩过）**：`-Project` 写成 `LegendAutoChss.uproject`（少一个 `e`）⇒ 16 秒后以
  `UnrealSharp.Automation.Utilities.PathUtilities.GetProjectRootFolder` 异常 + `ExitCode=1 (Error_Unknown)` + `BUILD FAILED` 收场。
- **`-TargetConfiguration=Development`** 会被 AutomationTool 映射为 `Release`（见输出里的 `bin\Release\net10.0`）。
- **MUST NOT 用 `dotnet build` 代替**——UnrealSharp 的胶水/加载顺序（`UserCode.LoadOrder.json`）由该命令生成。

---

## 4. 本轮装置与验收记录

| 项 | 值 |
|---|---|
| 装置形态 | **扩展既有 `TcsDevGcProbe`**（用户裁定 `probe_host`）——`UTcsDevGcProbeDriverBase` + C# 派生 `UTcsDevGcProbeDriver`；**未**另建 `TcsDevSkillCostProbe` |
| 槽位数 | **五 → 七**（新增：**宿主条件求值器** / **宿主载荷读取器**）。入链/入模板的**既有五槽** = 选择器 / 过滤器 / 流程步骤执行器 / 伤害委托 / 参数值来源（后者为 R5 Task 3 补入）。**注意口径**：`TcsDevGcProbeDriver.cs:22` 的类注写"五个脚本槽位"是**准确的**——它只描述该文件负责的那五个；本 Task 新增的两个在第二个 partial 文件 `TcsDevGcHostSlotsProbe.cs` 里（`Probe.Gc.Slots` 这个 tag 的自述也按七槽位记） |
| C++ 补强 | **一处**：`PublishProbeEvent`（用户裁定 `probe_route`）。理由：`PublishImmediate` 是**裸声明无 `UFUNCTION`** ⇒ 脚本层够不到，这是唯一真缺口 |
| 拒绝对照 | `.Reject` **独立命令**（照 `Tcs.Test.State.Reject` 先例），不混进常规 `Verify` ⇒ 保住"常规验收命令零红字"这条信号 |
| 运行方式 | 编辑器内 PIE（`PlayMode_InViewPort`，warmup 10s）；命令经 CmdLink 命名管道下发 |
| **未用 Live Coding** | 是（`Binaries/Win64/` 下存在 12 个 `UnrealEditor-TcsDev.patch_*.exe` 历史残留，本轮取证未使用该通道） |
| 可复现性 | **两轮**（同一次编辑器会话内，各自 `StopPIE` → `StartPIE` 重新起 PIE）⇒ 判定行序列 **17/17 逐字相同**，各 15 PASS / 0 FAIL |

### 4.1 一轮的完整判定行序列（17 条，逐字）

```
===== Tcs.Test.Gc.Arm：脚本对象 GC 存活观测（布置）=====
[PASS] 基线：选中 2 个目标（预期 2）/ 扣血 14（预期 14 = 每发 7 × 2 发）/ 脚本流程步骤调用 4（预期 4）；参数源槽位的证据是装置里那行 `incoming=13`（见日志）
[PASS] 布置：五个槽位已挂进链/模板；驱动侧引用已清空，存活只取决于框架登记表
[PASS] 宿主条件求值器槽位：事件广播一次、绑 3 行 ⇒ 宿主条件求值器被调用增量 3（预期 3）；最近一次条件 `ExpectedResult=False`
[PASS] 宿主载荷读取器槽位：事件广播一次 ⇒ 宿主载荷读取器被调用增量 1（**预期恰为 1**——读取点在行循环之外，与绑了几行无关；读到 `Marker=2026`）
[PASS] 宿主插槽·账本落点：绑 3 行（2 行条件过 + 1 行条件不过）⇒ 链只跑条件过的那些、扣血 2（预期 2 = 单发 1 × 2 行）——**条件取值真的门控了链**（若返回值被忽略，会多扣 1 份）
GC_READY：请在控制台执行 obj gc，然后跑 Tcs.Test.Gc.Verify
===== Tcs.Test.Gc.Verify：脚本对象 GC 存活观测（复核）=====
GC_CHECK_BEGIN：被测对象 7 个，存活 7 个（弱引用判定——引擎里唯一在 GC 后安全的死活判据）
[PASS] GC 存活性：被测脚本对象在真实原生 GC 后仍存活（装置侧弱引用仍可解析）
[PASS] 选择器/过滤器槽位：GC 后仍选中 2 个目标（预期 2——脚本选择器与过滤器仍被抵达）
[PASS] 流程执行器槽位：脚本流程步骤调用增量 4（预期 4 = 每发 2 步 × 2 发）
[PASS] 伤害委托槽位：扣血 14（预期 14——C# 公式仍被抵达）
[PASS] 参数源槽位（行为面）：GC 后第二发 Damage 的 `DamageBase` 仍取到脚本参数源值 13（装置日志里应仍见 `incoming=13`）
[PASS] 宿主插槽·GC 后行为面：GC 后再次广播：链再扣血 2（预期 2 = 单发 1 × 2 条件过行、1 行条件不过不起链）——宿主条件求值器与宿主载荷读取器两个脚本对象都仍被两张注册表保活且都被抵达
[PASS] 正例通过：脚本对象在真实原生 GC 后仍存活，且各槽位行为与调用计数符合基线
GC_CHECK_END
===== Tcs.Test.Gc.Reject：宿主插槽拒绝面（**故意触发 ensure 红字**；常规检查请用 Tcs.Test.Gc.Verify）=====
[PASS] 宿主插槽·条件拒绝面：空键被拒=True / 空宿主被拒=True / 首个登记成功=True / 同键重复被拒=True / 撤销成功=True / 撤销后重登记成功=True / 内置键不可撤=True / 内置键登记被拒=True（八项全真为通过）
[PASS] 宿主插槽·载荷拒绝面：空键被拒=True / 空宿主被拒=True / 同键重复被拒=True（三项全真为通过）
[PASS] 拒绝面全部如期：每个登记口都拒绝了非法输入、且判据来自返回值
```

### 4.2 预期红字清单（`.Reject` 专属）

`TcsEffectSubsystem_HostSlots.cpp` 的 **6 个 `ensureMsgf` 站点**，每条按其触发条件出现：

| 源码行 | 消息 |
|---|---|
| `:40` | `UTcsEffectSubsystem::RegisterConditionEvaluator: ConditionStruct 为空` |
| `:45` | `UTcsEffectSubsystem::RegisterConditionEvaluator: 宿主求值器对象为空（条件类型 %s）` |
| `:61` | `UTcsEffectSubsystem::RegisterConditionEvaluator: 条件类型 %s 重复登记——保留首个登记` |
| `:106` | `UTcsEffectSubsystem::RegisterPayloadReader: PayloadStruct 为空` |
| `:111` | `UTcsEffectSubsystem::RegisterPayloadReader: 宿主读取器对象为空（载荷类型 %s）` |
| `:120` | `UTcsEffectSubsystem::RegisterPayloadReader: 载荷类型 %s 重复登记——保留首个登记` |

**MUST NOT 用来判通过/失败**（见 §1.4：同一会话第二轮起，这些行**按引擎设计不再打印**）。

**另**：`Arm` / `Verify` 两命令**零红字**（本轮实测；仅第 1 轮在 PIE 尚未起来时的两条 `[FAIL]` 是预期的前置检查，见 §5）。

---

## 5. 如实边界（MUST NOT 外推）

1. **本轮未验"世界 B 查询判未命中"**（`tasks.md 5.5` 的后半）。已验的是**同世界内经 GC 后仍生效**；跨世界隔离属于 `harden-registry-cross-world-lifetime`（2026-09-29 已落地）的既有取证面，本轮的装置**没有**做第二个 `UWorld`。⇒ `tasks.md 5.5` 只应勾"同世界存活"这一半，跨世界那半**MUST NOT** 据本文勾选。
   - **★ 本轮复核的加强结论（比"没做"更强）**：本轮的两次 PIE（`StopPIE` → `StartPIE`）**确实先后造了两个世界**，但跨世界分支**依然未被触发**——原因是 `UTcsEffectSubsystem::Deinitialize`（`Private/TcsEffectSubsystem.cpp:48-61`）在两个 `GetDynamicKeys()` 循环里把两张表的动态条目**显式撤干净**了，故世界 B 的登记走的是"键不存在 ⇒ 正常登记"路径（日志为 `已登记宿主求值器`），**不是**"跨世界失效 ⇒ 移除 + Warning"路径。
   - **否证判据（可复算）**：全日志 `LogTcsEffect` 的 Warning 行里，**"属另一世界"与"已被回收"两种文案零命中**；这两条文案的唯一出处是 `TcsTriggerCondition.cpp:125-141` / `TcsTriggerPayloadReader.cpp:124-141`。
   - **这不推翻既有结论**：`EVID-2026-09-29-registry-lifetime-pie` §证据边界第 1、2 条**早已如实登记该面零行为验证**（原文：*"均未被触发——因为显式撤销总是先行清干净"*），且用户 2026-09-27 已裁定"先实测但只做记录，不因结果改变设计"。**本轮的新增价值 = 把"从未验过"升级为"两次独立 PIE 复核仍不成立"**，并据此新入册 `WAIT-11`（`LEDGER-deferred` 触发条件型）。
2. **`5.4 ②` 判据是"求值真的走脚本实现"，本轮由两个增量读数证明**——条件侧增量 3（绑 3 行）、载荷侧增量 1（每事件一次），且宿主侧 `CallCount` 是自证。**但本文没有做"宿主对象不是脚本实现"的反向对照**（即：没有替换成纯 C++ 宿主再验一遍）。该反例由 §1.3 的拒绝面间接覆盖一部分。
3. **装置整体不可重入与"新槽位可重臂"是两件事**：两个新登记口**有** `Unregister` ⇒ 新槽位可重臂；但 `RegisterStepExecutor` **无撤销口**，且 `Arm` 在任何槽位失败即中止（`TcsDevGcProbe.cpp:307-313`）⇒ **同一 PIE 会话内第二次正例 `Arm` 必然被拒**。故 §4 的"两轮"是**两次独立 PIE 会话**换来的。**MUST NOT** 读成"整个装置可反复重跑"。
4. **`.Reject` 的 11 项布尔是"装置侧聚合"，不是 11 条独立 PASS 行**：日志里它们是 3 条 `[PASS]`（条件面 8 项 / 载荷面 3 项 / 汇总 1 条）。本文 §1.3 已按此如实分列。
5. **控制台回显不可作读数**：CmdLink 通道是 ANSI，中文显示为 `?`。本文所有逐字读数取自 `Saved/Logs/LegendAutoChess.log`（UTF-8）。
6. **`Arm` 第 1 轮曾出现 2 条 `[FAIL]`**（`Bootstrap 子系统不可得（GameInstance 未就绪？）` / `尚未布置`）——那是**PIE 起来之前**的探测，属**预期的前置拒绝**，不是缺陷。它们在后续轮次不再出现。
7. **未验两轮之间的"零状态残留"**：两轮各自重新 `StartPIE`，但同一编辑器进程内静态注册表是否完全归零**未单独取证**。§1.3 的"内置键不可撤"能间接说明静态面未被污染，但这是**间接**证据。

---

## 6. 引擎与机制事实（本轮确认，可复用）

1. **`ensure` 是"同一调用点每会话一次"**（不是每次触发）——`AssertionMacros.h:364-365`；想要每次都报用 `ensureAlways`。**⇒ 任何"数红字"的验收判据都不可靠**；拒绝面判据 MUST 取返回值。
2. **MCP `ue-editor` 的 DSH 客户端会持有陈旧 session id**。编辑器重启后 `tools/call` 报 `Unknown session id … client should reinitialize`，而客户端**不会**自动重新 initialize。**绕过方式**：直接对 `http://127.0.0.1:8000/mcp` 说 streamable-http —— 先 `initialize`（响应头带新的 `Mcp-Session-Id`），再带该头发 `tools/call`。顶层工具只有 3 个：`list_toolsets` / `describe_toolset` / `call_tool`（`call_tool` 的 `tool_name` **不带** toolset 前缀，toolset 名另用 `toolset_name` 给）。
3. **`AllToolsets` 里没有任何"执行控制台命令"的工具**（830 个工具全扫：无 console-exec）。可用的两条通道是：
   - **`SlateInspectorToolset`** 驱动编辑器 UI（`Snapshot` 找控件 ref → `Click` → `Type` + `submit`）。**控制台命令条 = `Snapshot` 里 `text "Cmd"` 标签之后的那个 `textbox`**（本机为 `tb3`，1733px 宽）。ASCII 命令经此可确认生效（判据：副作用而非回显）。
   - **`CmdLinkServer` 命名管道**（`\\.\pipe\UnrealEngine-CLI`）——需先 `console.CmdLink.enable 1`。**这是唯一能拿到命令返回值全文的通道。**
4. **`EditorAppToolset` 有完整 PIE 控制**：`StartPIE`（`options.bSimulate` / `playMode` / `warmupSeconds`）/ `StopPIE` / `IsPIERunning`。
5. **属性变更自动广播事件**：`FTcsAttributePipeline::Recalculate` → `BroadcastChange` → `Tag_TcsEvent_Attribute_ValueChanged`。这正是装置里 `EffectChain.Check.WaitEvent` 那一大段日志的来源——**本轮读数里那些 `WaitEvent[...]` 行是既有链自己的行为**，与本 Task 无关，MUST NOT 计入本 Task 的判据面。
