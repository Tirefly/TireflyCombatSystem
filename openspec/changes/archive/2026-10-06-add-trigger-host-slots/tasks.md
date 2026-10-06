## 1. 契约层（提案）

- [x] 1.1 `openspec validate add-trigger-host-slots --strict --no-interactive` 通过
- [x] 1.2 用户批准提案后才开始实施（本清单的执行门槛）

## 2. 类型升格与宿主契约（`TcsEffect`）

- [x] 2.1 **先实测** `FTcsTriggerContext` 的非 `UPROPERTY` 裸成员 `const UWorld* World` 能否随 `BlueprintType` 升格通过 UHT——若被拒，改为门面按句柄访问器取或移入派生上下文（**不预设结论**）
- [x] 2.2 `FTcsTriggerContext` 升格 `USTRUCT(BlueprintType)`
- [x] 2.3 `FTcsTriggerPayloadInfo` 升格 `USTRUCT(BlueprintType)`
- [x] 2.4 新增 `ITcsTriggerConditionEvaluator`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) bool Test(const FInstancedStruct&, const FTcsTriggerContext&, double)`）——形参 **MUST NOT 带 `const`**（UHT thunk 硬规则）
- [x] 2.5 新增 `ITcsTriggerPayloadReader`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) FTcsTriggerPayloadInfo Read(const FInstancedStruct&)`）
- [x] 2.6 **不造 `USTRUCT` 策略基类、不造转发器、不改注册值类型**（`FTcsTriggerConditionTest` / 读取器 `TFunction` 原样）

## 3. 门面登记口（`UTcsEffectSubsystem`）

- [x] 3.1 `UFUNCTION() bool RegisterConditionEvaluator(const UScriptStruct*, TScriptInterface<ITcsTriggerConditionEvaluator>)`——包 `TFunction` 转发进既有注册表
- [x] 3.2 `UFUNCTION() bool RegisterPayloadReader(const UScriptStruct*, TScriptInterface<ITcsTriggerPayloadReader>)`
- [x] 3.3 两处 **GC 强持有**：宿主对象进 `UPROPERTY` 数组（从 `TScriptInterface::GetObject()` 取；`TScriptInterface` 本身不构成 GC 强引用）
- [x] 3.4 两处**弱引用**：把宿主对象与登记世界交给注册表（**复用**已落地的 `Lifetime` 表，MUST NOT 重做寿命机制）
- [x] 3.5 拒绝面：键为空 / 宿主对象为空 / 同世界活对象重复（保留首个）
  - **落地订正（2026-10-06）**：两张注册表的 `Register` 返回 `void` ⇒ 若只在注册表里拒绝，调用侧**分不出拒绝与成功**，"重复登记被拒"这条负对照（5.7）无法成立。故**拒绝判在门面**：`Register` 前显式 `Registry::Get().Find(...)`（`Find` 顺带做寿命校验 ⇒ 失效条目仍走替换路径），`ensureMsgf(false, "…重复登记——保留首个登记")` + `return false`。
- [x] 3.6 实现**无转发器**（两张注册表的注册值保持 `TFunction` 原样）
  - **落地订正（2026-10-06）**：原写"实现住 `Private/Trigger/TcsTriggerHostSlots.cpp`（单文件）"，实际住 **`Private/TcsEffectSubsystem_HostSlots.cpp`**——随本模块既有的 `_Run` / `_RunAccess` / `_ChainWait` / `_Trigger` / `_StepProtocol` 门面分片惯例。拆分的**唯一动因是 300 行风格上限**（拆前 397 行 ⇒ 拆后 229 + 191）。
- [x] 3.7 **两个 `Unregister` 配对口**（原清单漏列，实施中补齐）
  - `UFUNCTION() bool UnregisterConditionEvaluator(const UScriptStruct*)` + `UFUNCTION() bool UnregisterPayloadReader(const UScriptStruct*)`。**为什么必需**：`RegisterStepExecutor` 无撤销口 ⇒ 步骤执行器插槽在同一 PIE 会话**不可重臂**；而条件/载荷两张注册表本就有 `Unregister`，缺了门面口会让"重新布置一次探针"被自己的重复登记门挡住，表现为**装置不可重跑**。只撤动态登记——对内置/属主模块的静态自注册键返回 `false` 且不动其路径。
  - **刻意不做的收缩**：`UnregisterXxx` **不**同步收缩 `RegisteredConditionEvaluators` / `RegisteredPayloadReaders`。多留一个引用只是过度持有（无害）；误删一个仍被**别的**动态键引用的对象会让那条键**静默失效**（WAIT-8 形态）。

## 4. 编译门槛

- [x] 4.1 UBT **Development Editor** 零 warning 零 error
- [x] 4.2 UBT **Game Shipping** 零 warning 零 error
- [x] 4.3 内置三条件与属主模块读取器的静态自注册路径未受影响（双轨并存）

## 5. 宿主侧实测（跨仓：LAC）

- [x] 5.1 **复用**既有 `TcsDevGcProbe` 两段式装置（`Arm` → `obj gc` → `Verify`）——MUST NOT 另写一套
  - **落地订正（2026-10-06）**：原文写"复用 `ATcsHostScriptingE2EProbe`"——该名在本仓**不存在**，是落笔时的拟定名。实际形态 = `UTcsDevGcProbeDriverBase`（C++ 驱动基类）+ `UTcsDevGcProbeDriver`（C# 派生），经 `Tcs.Test.Gc.Arm` / `Tcs.Test.Gc.Verify` 驱动。**用户裁定（`probe_host`）**：扩展该装置，不另建 `TcsDevSkillCostProbe`。
- [x] 5.2 C++ 补强**仅一处**：`UTcsDevGcProbeDriverBase::PublishProbeEvent(FGameplayTag, const FInstancedStruct&)`
  - **落地订正（2026-10-06）**：原文写"`Source/TcsDev/…/TcsDevSkillCostProbe.*`（宿主专属 **SkillCost** 条件 + 载荷读取器）"——该装置不建。**用户裁定（`probe_route`）**＝纯 C# 为主 + 一处 C++ 补强，且经**逐项实测**该"一处"收窄为仅**发布**：`PublishImmediate` / `PublishFrameEnd` 是裸声明（无 `UFUNCTION`）⇒ 脚本层无法发布事件，这是唯一真的够不到的能力。另五件候选的否证见 `TcsDevGcProbeDriver.h:111-124`。
- [x] 5.3 C# 派生实现：用 `ITcsTriggerConditionEvaluator` / `ITcsTriggerPayloadReader` 定义宿主自己的注册内容
  - 落地文件（LAC 仓 `Script/LegendAutoChessCS/TcsDevGcFixtures/`）＝ `TcsDevGcHostCondition.cs` / `TcsDevGcHostPayload.cs`（两个脚本结构体，作注册表键）/ `TcsDevGcHostSlots.cs`（两个宿主 `UObject` 实现契约）/ `TcsDevGcHostSlotsProbe.cs`（驱动 partial）/ `TcsDevGcHostProbeCondition.cs` + `TcsDevGcHostProbePayload.cs`（**拒绝面专用键**——与会话键隔离，否则"真撤真换"会抽掉会话对象强持有、让 `Verify` 报假失败）
- [x] 5.4 **"能导出 ≠ 能往返"判据**：① glue 产物里两个方法生成**真实方法体**（非空壳，非空 `ToNative`/`FromNative`）；② 宿主脚本实例注册后，求值真的走脚本实现
  - **① 静态证据（2026-10-06）**：`Intermediate/UnrealSharp/UHT/{Editor,Game}/TcsEffect/TcsTriggerConditionEvaluator.generated.cs` 与 `…PayloadReader.generated.cs`（各 102 行）里两个方法都生成**真实方法体**：`Bind_UClass.CallGetNativeFunctionFromClassAndName` → `CallGetNativeFunctionParamsSize` → 逐参 `CallGetPropertyOffsetFromName`（`Test`：`ConditionData`/`Context`/`RandomValue`/`ReturnValue`；`Read`：`Payload`/`ReturnValue`）→ `StructMarshaller<…>.ToNative` → `CallInvokeNativeFunctionOutParms` → `BoolMarshaller`/`StructMarshaller.FromNative`，marshaller 再委派给 `ScriptInterfaceMarshaller<T>`。**非空壳**由"逐参取偏移 + 双向封送"两处结构性特征判定（空壳只会有 `return default` / 空 `ToNative`）。
  - **② 运行时证据**：见 `EVID-2026-10-06-trigger-host-slots` §1.1——宿主条件求值器增量 **3**（绑 3 行）、宿主载荷读取器增量 **1**（每事件一次），且宿主侧 `CallCount`/`LastExpectedResult`/`LastMarker` 自证达到脚本实现。**如实边界**：未做"把宿主换成纯 C++ 实现"的反向对照（见该 EVID §5.2）。
- [x] 5.5a 寿命判据·**同世界**：同世界内经 GC 后仍生效（MUST NOT 被静默回收）
  - `EVID-2026-10-06-trigger-host-slots` §1.2：7/7 弱引用存活，且 GC 后**再广播一次**仍扣血 2 ⇒ 把"保活"与"仍被抵达"绑在一起（只证存活不够——可能活着但注册表已丢指针）。
- [ ] 5.5b 寿命判据·**跨世界**：世界 B 查询判未命中且不误解引用
  - **未验，MUST NOT 据本轮勾选**：本装置只跑单个 PIE 世界，**没有**造第二个 `UWorld`。该面属 `harden-registry-cross-world-lifetime`（2026-09-29 已落地）的既有取证范围。**如实记在** `EVID-2026-10-06-trigger-host-slots` §5.1。原 5.5 是一行两半，本轮拆开以免"一半实测"被读成"整条实测"。
- [x] 5.6 "每事件一次"语义：同一事件一次广播挂多行时，宿主读取器只被调用一次
  - **判据加强（2026-10-06）**：同一事件绑 **3 行 = 2 行条件恒过 + 1 行恒不过**，一次广播同时证三件事——载荷读取增量 = **1**（读取点在行循环外，`TcsTriggerEvaluator.cpp:43`）、条件求值增量 = **3**、扣血 = **2 × 单发**。末项是"**条件取值真的门控了链**"的唯一判据：只绑恒过的行时，"返回值被尊重"与"返回值被忽略（恒当通过）"**完全无法区分**。
  - **实测（2026-10-06）**：三项全中（1 / 3 / 2），见 `EVID-2026-10-06-trigger-host-slots` §1.1。
- [x] 5.7 阴性对照：`.Reject` 命令覆盖拒绝面（重复登记 / 空键 / 空宿主），证明红字来自拒绝而非异常
  - **纪律（我第一版写错、已改）**：拒绝面 MUST 独立成 opt-in 命令——初版把这些检查放进常规 `Verify` 路径，会**打破"常规验收命令零红字"这条验收信号**。现为驱动 `VerifyRejections` 挂点 + 装置 **`Tcs.Test.Gc.Reject`** 独立命令（照 `Tcs.Test.State.Reject` 先例）。**判据取返回值，不数红字**——红字数量变化 MUST NOT 造成假通过/假失败。
  - 覆盖 8 项（条件侧）+ 3 项（载荷侧）：空键 / 空宿主 / 首个登记成功 / 同键重复被拒 / 撤销成功 / 撤销后重登记成功 / 内置键不可撤 / 内置键登记被拒。末两项是**免费**的既有控制面。
  - **实测（2026-10-06）**：11 项**全 `True`**，聚合为 3 条 `[PASS]`（见 EVID §1.3）。
  - **"不数红字"这条纪律本轮被引擎机制当场验证**：`ensure` 的**同一调用点每会话只打印一次**（`Engine/Source/Runtime/Core/Public/Misc/AssertionMacros.h:364-365` 原文：*"a given call site will only print the callstack … the first time an ensure is hit in a session"*）。本装置共 **6 个** `ensureMsgf` 调用点（`TcsEffectSubsystem_HostSlots.cpp` 的 `:40`/`:45`/`:61`/`:106`/`:111`/`:120`）⇒ **第 1 轮 `Reject` 出 12 条红字、第 2 轮出 0 条**，而两轮的 11 项布尔**全 `True`**。若当初按"数红字"判，第 2 轮会**假失败**。红字清单见 EVID §4.2，机理见 EVID §6.1。

## 6. 收束

- [x] 6.1 台账回写：`LEDGER-reflection` 的 `R-2` 行 + 家族表三处（`:271` / `:277` / `:287`）按本提案形态订正（**2026-10-06 完成**；第三处 `:287` 顺势抓出一处真缺陷——它把条件求值器所在模块写成 `TcsTrigger`，**该模块从不存在**（全库实测该名仅此一处、`.uplugin` 历史亦无），已按实证订正为 `TcsEffect` 并留痕）
- [x] 6.2 证据包（`EVID-*`）：摘要行（只留不变量）、检查编号 ↔ 标题对照表（**逐字取自日志**）、预期红字清单、如实边界
  - **落地（2026-10-06）**：`Documents/combat-system-design/evidence/2026-10-06-trigger-host-slots.md`（六节：读数 / 区段锚点 / 复算脚本 / 装置与验收记录 / **如实边界** / 引擎与机制事实）。§4.1 是**一轮的 17 条判定行逐字序列**；§4.2 是 6 个 `ensureMsgf` 站点的预期红字清单；§5 七条边界（含 5.5b 未验、反向对照未做、装置不可重跑、控制台回显不可作读数）。
  - **本轮顺带抓出并订正的真缺陷**：① 记录里的构建命令把工程名写成 `LegendAutoChss.uproject`（**少一个 `e`**）⇒ 自动化必失败（`GetProjectRootFolder` 异常 + `ExitCode=1`），已在 EVID §3.3 订正为 `LegendAutoChess.uproject` 并留痕；② `Script/*.cs` 只要落盘即触发 UnrealSharp 热重载重建产物——本轮因一次注释编辑使 `LegendAutoChessCS.dll` 一度变成 `84992 B`，重建后 **SHA256 逐字节回到证据值 `8255CDEA2E1E87E6`（84480 B）**，故读数仍对应同一产物身份（EVID §2.1 有完整注记）。
- [x] 6.3 可复现性：跑两轮，判据 = 摘要行逐字相同 + 各检查结论相同（**MUST NOT** 比字节/行数）；取证期 **MUST NOT** 用 Live Coding
  - **实测（2026-10-06）**：两轮独立 PIE 会话（各自 `StopPIE` → `StartPIE`），判定行序列 **17/17 逐字相同**，各 **15 PASS / 0 FAIL**。
  - **为什么必须两次会话**：`RegisterStepExecutor` 无撤销口，且 `Arm` 在任何槽位失败即中止（`TcsDevGcProbe.cpp:307-313`）⇒ **同一次 PIE 会话内第二次正例 `Arm` 必然被拒**。**新增的两个宿主槽位本身可重臂**（各有 `Unregister`），但**整个装置不可重跑**——两件事 MUST NOT 混为一谈（用户裁定的 `probe_host` 形态下这是固有边界）。
  - **MUST NOT 比字节/行数已遵守**：本判据若比日志行数会**必然假失败**——`ensure` 的会话一次性语义（见 5.7）使第 1、2 轮的**红字行数不同**（12 vs 0），而判定行序列完全相同。
  - **未用 Live Coding**：取证期未使用该通道（`Binaries/Win64/` 下 12 个 `UnrealEditor-TcsDev.patch_*.exe` 是历史残留，本轮未触碰）。
- [x] 6.4 `openspec archive add-trigger-host-slots --yes` + `validate --all --strict` 全绿 + 能力数对账
  - **用户裁定（2026-10-06）**：5.5b（跨世界半）**留空归档**，照 `harden-registry-cross-world-lifetime`（2026-09-29）先例——该轮同一缺口也如实登记后归档。缺口已升级记录为 **`WAIT-11`**（`LEDGER-deferred` 触发条件型）：原文只说"从未验"，本轮升级为"**两次独立 PIE 复核仍不成立**"（跨世界分支每次都被 `Deinitialize` 的显式撤销先行清干净）。**MUST NOT** 因归档而读成"已验"。
  - **能力数对账（订正后口径，2026-10-06 实测）**：本变更的 delta **只有 `## MODIFIED Requirements`**、且 `effect-trigger` 能力**已存在** ⇒ 归档**不新增能力**，能力数 **34 → 34（不变）**。这是与 `add-tcs-skill-module` 的**关键差别**（后者含 `skill-def-asset` 的 `## ADDED Requirements` ⇒ 归档时 +1 ⇒ 34 → 35）。
  - **⚠ 计数口径陷阱（本轮实测踩到）**：`openspec spec list --long | <行过滤>` 会数出 **35**，但 `Get-ChildItem openspec/specs -Directory` 数出 **34**——**目录数是权威**（前者把某行表头/warning 也算进去了）。**判据 MUST 用目录数或 CLI 的 JSON 长度，MUST NOT 用行计数**。
  - **`validate --all` 的 items 数**：= 能力数 + 活跃变更数。归档前 34 + 2 = **36**；归档后 34 + 1 = **35**。⚠ 故归档后 items **应当变成 35**——先前写的"36 不变"是**错的**，已按实测订正。
