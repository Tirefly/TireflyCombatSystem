# 脚本对象 GC 存活性观测：预注册预测与实跑证据（`Tcs.Test.Gc.Arm` / `Tcs.Test.Gc.Verify`）

- **文档 ID**：`EVID-2026-10-03-scripting-gc-survival`
- **类型**：EVID / 证据
- **状态**：**PASS**（正例与负例均通过；日志快照哈希已冻结）
- **权威范围**：提案 `add-scripting-gc-survival-probe` 的人工验收面——四个**宿主脚本槽位**（选择器 / 过滤器 / 流程步骤执行器 / 伤害流程委托）在**真实原生 GC**（`obj gc`）之后的**存活与可调用性**
- **最后更新**：2026-10-03

> **本文件为什么先写一半**：本装置要证明的命题是"GC 后仍能调用"，而它的失败方向恰好是"以为通过了"。故**预测先于实跑落盘**（`MEM-20261003-04`：被下游配置遮蔽的改动只有负对照能观测——先立预测，再跑），使"命中"成为一个**可证伪**的观测，而不是事后叙述。

## 被测对象与装置

- **装置（宿主侧，提案自述不动插件仓代码）**：
  - C++：`Source/TcsDev/Private/Dev/TcsDevGcProbe.cpp`（两端命令 + 驱动解析 + 死活判据 + 最终判定）· `Source/TcsDev/{Public,Private}/Dev/TcsDevGcProbeDriver.{h,cpp}`（驱动契约基类）
  - C#：`Script/LegendAutoChessCS/TcsDevGcFixtures/TcsDevGcProbeDriver.cs`（建链/建模板/记基线/复核）
  - 夹具：同目录 `TcsDevGcTargetHosts.cs` / `TcsDevGcFlowStep.cs` / `TcsDevGcDamageFormula.cs`（自退役的 `TcsProbe/*` 恢复并改名，保留调用计数器）
- **被测链**：`Probe.Gc.Slots` = `SelectTargets(脚本选择器 + 脚本过滤器) → StepDamage(流程模板 Probe.Gc.Flow)`
- **被测模板**：`Probe.Gc.Flow` = `CollectStart → BaseDamage(C# 委托) → 脚本步骤 ×2 → Execute(C# 委托) → Completed`
- **四个槽位的引擎侧保活路径**（装置要测的就是这些路径是否真的成立）：
  | 槽位 | 挂点 | 保活路径 |
  |---|---|---|
  | 选择器 | `FTcsSelHostDelegate::Host`（`UPROPERTY TScriptInterface`） | 链 → `UTcsEffectSubsystem::AddReferencedObjects` → `AddPropertyReferencesWithStructARO(FTcsEffectChain::StaticStruct(), …)` |
  | 过滤器 | `FTcsFilterHostDelegate::Host`（同款） | 同上 |
  | 流程执行器 | `UTcsDamageSubsystem::RegisteredFlowStepExecutors` | 本身就是 `UPROPERTY` 数组 |
  | 伤害委托 | 步骤 `Delegate`（`UPROPERTY TScriptInterface<ITcsDamageFlowDelegate>`） | 模板 → `UTcsDamageSubsystem::AddReferencedObjects` → 同款逐模板收集 |
- **驱动侧纪律**：`Arm` 末尾**清空全部夹具字段**——此后存活只取决于上表的框架侧引用，不取决于驱动"还记不记得它们"。

## 判据（两条腿，缺一不可）

1. **死活**（C++ 侧，**权威**）：被测对象以 `TWeakObjectPtr` 登记——`TWeakObjectPtr` 是引擎里唯一在 GC 之后**安全**失效的引用形态（对已回收对象做任何解引用、含 `IsValid()`，都是读已释放内存）。装置侧判完后把结论作为 `Verify` 的入参交给驱动，驱动**只在判定为存活时才调用**夹具。
2. **行为与计数**（C# 侧）：`IsValid()` 会在"包装非空但原生对象已回收"时**假通过**，直接调用则**静默失败**（不崩、不报错、结果不生效）。故判据落在**可见数字**上：脚本步骤静态计数增量（预期 2）、链选出的目标数（预期 2）、扣血（预期 7——脚本公式返回 7，而链步骤配的是 25）。

## 预注册预测（**跑前立**，2026-10-03，逐字）

正例（`Tcs.Test.Gc.Arm` → `obj gc` → `Tcs.Test.Gc.Verify`）：

| # | 预测行 | 预期 |
|---:|---|---|
| 1 | `基线` | PASS：选中 2 个目标 / 扣血 7 / 脚本流程步骤调用 2 |
| 2 | `布置` | PASS：四个槽位已挂进链/模板；驱动侧引用已清空 |
| 3 | `GC 存活性` | PASS：被测脚本对象在真实原生 GC 后仍存活（**4/4**） |
| 4 | `选择器/过滤器槽位` | PASS：GC 后仍选中 2 个目标 |
| 5 | `流程执行器槽位` | PASS：调用增量 2 |
| 6 | `伤害委托槽位` | PASS：扣血 7 |
| 7 | 末行 | `[PASS] 正例通过：…` |

负例（`Tcs.Test.Gc.Arm --negative` → `obj gc` → `Tcs.Test.Gc.Verify`）：

| # | 预测行 | 预期 |
|---:|---|---|
| 8 | `布置` | PASS：负对照模式，四个夹具均未挂进链/模板，引擎侧零反射引用 |
| 9 | `GC 存活性` | **FAIL**：被测脚本对象已被真实原生 GC 回收（**0/4**） |
| 10 | 末行 | `[PASS] 负对照如期失败：… 装置可信` |

**两种"预测落空"分别意味着什么**（都不是装置缺陷，而是发现）：

- **负例 4/4 仍存活** ⇒ UnrealSharp（或某个引擎侧注册表）在托管/宿主侧把 C# `UObject` 全部钉住 ⇒ **GC 顾虑整体不成立**；装置按 `design.md` D6/3.7 自曝"当前不可信"，拒绝输出通过结论。
- **正例 0/4 存活** ⇒ 上表四条保活路径**至少一条不成立**（ARO 未递归进 `FInstancedStruct` 内层、或脚本对象不被该路径覆盖）⇒ 框架级发现。

## 跑前修订记录（2026-10-03，对抗性评审之后）

> 独立评审（带完整上下文的子代理，**未改任何文件**）在实跑**之前**发现 1 条阻断级 + 1 条致错级缺陷；均已修复并重新通过双配置编译。
> **预注册预测不变**——上面的预测描述的是**被测机制**，不是装置内部实现。

| # | 级别 | 缺陷 | 修法 |
|---:|---|---|---|
| A | 🔴 **阻断** | `GetRunTargets` 对"全即时链"**恒返回空数组**（`TcsEffectSubsystem.cpp:206` 原文"全即时链在返回前即走完（运行态已释放）"+ `:256-268` 对已释放运行态返回空）⇒ 链无等待步时"选中目标数"恒为 0 ⇒ 基线必 FAIL；叠加 B 后仍会打 `GC_READY`，最终报出"对象明明全活却失败"——**最容易被误读成框架保活不成立** | 链末尾追加 `FTcsStepWaitDelay(0.25)`（有退役装置先例；扣血在等待步**之前**的同步段完成，行为判据不受影响） |
| B | 🟠 致错 | `RunGcProbeArm` 只以"监视列表为空"为中止条件、**无视驱动的失败槽位**；而打印用的 `ClearSlotReports()` 把失败计数一并清零 ⇒ 半装配（执行器登记被拒 / 链或模板登记被拒 / 基线不符）也会进入 GC 阶段。叠加"`RegisterStepExecutor` 无 Unregister"⇒ 同会话第二次正例 Arm 必被拒，而上一批链/模板仍在册（保活的是**上一批**夹具）⇒ 复核结论张冠李戴 | 打印**前**取失败计数，`> 0` 即中止且不置 Armed，失败信息进输出 |
| C | 🟡 红字 | 选择器产出 2 个目标，但只有 caster 挂了 `Health` ⇒ 流程对 secondary `SetBaseValue` 命中 `ensureMsgf`（"该单位未持有此属性"）⇒ 首次基线跑带 ensure 红块（**装置副作用**，不是被测信号） | 给 secondary 同样 `AddAttribute` + `SetBaseValue` |
| D | 🟡 可定位性 | 死活只报总数 `存活 x/4`，**指不出是哪个槽位**（与 `design.md` Goals 不符——本装置存在的理由就是"静默失效极难定位"） | `WatchTarget` 增槽位名参数；装置逐槽位打印「存活 / 已回收」 |
| E | 🔵 低 | 模板的 `Execute` 步第 4 个参数（`Delegate`）传了 `null` ⇒ 两个新钩子的脚本实现**根本不会被抵达**（此前"那行打印会出现"的说法不成立） | 改传 `formula`；两钩子为中性返回，基线数字不变 |
| F | 🔵 低 | `Probe.Gc.Slots` 的 DevComment 写"四槽位"却只列 3 个 | 补全四槽位名 |
| G | 🔵 低 | 驱动实例化失败时未查空指针（`NewObject` 建托管类在本仓无先例）⇒ 会崩在 `Driver->Arm(...)` 上 | 加空指针兜底 + 可读错误 |

**评审同时证实（源码级，独立于本次修改）**：

1. **Outer 不是 GC 可达边**——主路径只走 UPROPERTY schema（`GarbageCollection.cpp:6571`），聚类路径受 `RequiresCookedData()` 门控且 `CanBeClusterRoot()` 默认 `false`（`UObjectBaseUtility.h:410-413`）⇒ 负例不会因 `NewObject<T>(this)` 的 Outer 关系逃过回收（**这是负对照有效的前提**）。
2. **保活链四条全闭合**——关键的两层 `FInstancedStruct` 嵌套（链 → `Steps` → `FTcsStepSelectTargets` → `Selector` → `FTcsSelHostDelegate::Host`）由 `InstancedStruct.cpp:507,528` 的递归 ARO 覆盖。
3. **无"调用已回收对象"的残留路径**——脚本侧在任何夹具调用之前早退。
4. **判定逻辑四组合皆正确**——负例的"`GC 存活性` FAIL + 末行 PASS"语义由装置的 Note 消歧。

**评审的诚实边界**（未验证项）：① 扣血数字是否受 C 影响（依赖 `Context.Targets` 遍历顺序语义）；③ Outer 是否还有引擎别处的可达边（只证了主路径与聚类路径）。

**其中一项已在源码层闭环（2026-10-03 补）**：
- ② **C++ `NewObject` 实例化托管类**——`UCSManagedClassCompiler` 把 C# 类的 `ClassConstructor` 设为 `UCSClass::ManagedObjectConstructor`（`CSManagedClassCompiler.cpp:113`），而 `ClassConstructor` 是**类自身的属性**，与调用方是谁无关；该构造器先跑原生类构造、再初始化托管属性、最后 `OwningAssembly->CreateManagedObjectFromNative(…)`（`CSClass.cpp:7-47`）⇒ 托管实例随原生对象构造一并创建。唯一会让托管实例延后的 `bDeferredCreation` **只在 CDO 创建窗口内为真**（`CreateDeferredManagedCDO` 置真 → `FinalizeManagedCDO` 立即置假并创建托管 CDO，`CSManagedClassCompiler.cpp:258-272`），运行时命令执行时必为假。⇒ "无先例"是**工程先例**问题，不是机制问题；运行期不再预期硬失败。

## 首轮尝试与装置修订（2026-10-03 18:07，**装置自身失败，非被测机制**）

**现象**：首次 `Tcs.Test.Gc.Arm` 报
`[FAIL] 夹具失败：未找到驱动类 TcsDevGcProbeDriver（C# 脚本程序集是否已加载？类名取引擎名——\`U\` 前缀已去）`

**取证（同一日志快照的行号锚点）**：

| 锚点 | 内容 | 判读 |
|---|---|---|
| 1779-1783 | `LogBlueprint: Compiling Blueprint '/Script/LegendAutoChessCS.TcsDevGc{DamageFormula,FlowExecutor,ProbeDriver,TargetSelectorHost,TargetFilterHost}'` | **五个托管类都已注册**（`09.42.54`，编辑器启动期） |
| 2511-2513 | `10.07.22` 命令执行 → FAIL | 类存在比命令早 **25 分钟** ⇒ 失败在**装置侧的类查找**，不在装载 |

**根因（2026-10-03 18:18 实证确认）**：托管类的 **UClass 名带 BP 生成类后缀 `_C`**——修订后的装置打出了答案行：
`UTcsDevGcProbe: 驱动类解析为 TcsDevGcProbeDriver_C（候选 2 个）`。即上文两支候选中的第 ① 支成立（"类名带 `_C` 而 Blueprint 路径不带"）：`GetAdjustedFieldName` 命名的是 **Blueprint**，UE 为它生成的**类**才带 `_C`，而旧代码只按 `EngineName` 精确匹配 UClass ⇒ 恒不命中。第 ② 支（托管基类未解析）**不成立**——`IsChildOf` 判据在同一轮里作为三路并集的一支参与且未报错。

**修订**（`TcsDevGcProbe.cpp`，.cpp-only；双配置重编 Succeeded）：

1. **候选收集改三路并集**：精确引擎名 ∪ 精确 `U` 前缀名 ∪ 名字**包含** `TcsDevGcProbeDriver` ∪ **基类派生**（唯一不依赖命名的路径，排除基类自身）。
2. **挑选带优先级**：精确名 > `U` 前缀名 > 基类命中 > 其余名命中，非抽象优先；命中时打 `驱动类解析为 X（候选 N 个）`。
3. **失败即自曝**：把库里所有名字含 `GcProbe` 的类**连同父类与是否抽象**打进日志 ⇒ 一次失败即可定位"程序集未加载"还是"托管基类未解析"。
4. **拆掉一处断言风险**：原 `NewObject<UTcsDevGcProbeDriverBase>(Outer, Class)` 在候选类不匹配时走 `CastChecked` ⇒ **断言（可能崩）**；改为先 `NewObject<UObject>` 再安全 `Cast`，失败给可读错误。

> **方法论注**：这一轮是"**装置先于被测机制失败**"的典型——预注册预测 #1 的前提（装置能布置成功）当时并不成立。这也是"独立评审 + 首跑只读不判"的价值所在：没有把这次 FAIL 误读成"框架保活不成立"。

## 布置阶段实跑（2026-10-03 18:18，`Arm` 通过；**GC 与复核尚未跑**）

- **日志**：`Saved/Logs/LegendAutoChess_2.log`（本会话落在 `_2` —— 上一实例仍占用 `LegendAutoChess.log`，UE 自动改名）
- **命令锚点**：2401 `Cmd: Tcs.Test.Gc.Arm` · 2402 装置标题 · 2403 驱动类解析为 `TcsDevGcProbeDriver_C`（候选 2 个）
- **装置判定（2434-2437）**：
  - 2434 `[PASS] 基线：选中 2 个目标（预期 2）/ 扣血 7（预期 7）/ 脚本流程步骤调用 2（预期 2）`
  - 2435 `[PASS] 布置：四个槽位已挂进链/模板；驱动侧引用已清空，存活只取决于框架登记表`
  - 2436 `[提示] 已登记被测脚本对象 4 个；…`
  - 2437 `GC_READY：请在控制台执行 obj gc，然后跑 Tcs.Test.Gc.Verify`
- **与预注册预测的对照**：预测 #1、#2 **逐字命中**（含三个数值）。
- **四个槽位全部被抵达的直接证据**（同一轮基线跑）：
  | 槽位 | 日志证据 |
  |---|---|
  | 选择器 | `[TcsDevGcSelectorHost] ResolveTargets call=1 targets=2` |
  | 过滤器 | `[TcsDevGcTargetFilterHost] PassTarget candidate=1/2 pass=True call=1/2` |
  | 流程执行器 | `[TcsDevGcFlowExecutor] Execute marker=701/702 … call=1/2` |
  | 伤害委托 | `[TcsDevGcDamageFormula] **C# 公式被调用**：incoming=25 …` + `[伤害记录] #1/#2 … 最终 7.000 / 执行 7.000`（差额 −18 = 25−7 ⇒ 执行量确由 C# 公式决定） |
  | **两个新钩子（提案 B 检查点 4 的 C# 可达性）** | `[TcsDevGcDamageFormula] ResolveExecutedDamage 被抵达：target=1 candidate=7 absorbed=0` —— 该行只在 Execute 步的 `Delegate` 非空时才会出现（修订 E 的直接兑现） |
- **等待步按设计工作**：`WaitDelay[…] PC=2 挂起 0.250s` → `链 Probe.Gc.Slots 步 2 挂起` → 0.21 s 后 `链 Probe.Gc.Slots 唤醒（PC=2）` → `完成（共 3 步）` ⇒ 修订 A 生效（`GetRunTargets` 因此可读）。
- **本轮未完成的部分**：`obj gc` 与 `Verify` **均未执行**，且 PIE 在 `10.18.46` 被拆除 ⇒ 布置状态随世界销毁（装置的世界判据会拒绝跨会话复核，属设计如此）。**正例与负例 MUST 在同一次 PIE 会话内各自跑完 `Arm → obj gc → Verify`。**

## 实跑输出（2026-10-03 18:20，正例与负例**均通过**）

- **日志**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess_2.log`（本会话落在 `_2`——上一实例仍占用 `LegendAutoChess.log`，UE 自动改名）
- **快照口径（哈希已冻结）**：**366,232 字节 / 2,737 行**，mtime `2026-10-03 18:35:43`，**SHA-256 `3AA6774EFB6B70BBB843A2FBBBC682A7C2C616518842E3CBC9D6466D3205245C`**
  - 哈希口径：取证时 `Get-Process UnrealEditor*` 计数 = **0**（编辑器已退出，日志不再增长）⇒ 该哈希与下表行号锚点**严格属于同一快照**，无"哈希与行号非同时刻"的口径问题（对照 `EVID-2026-09-30-modifyflow-acceptance` 的干净口径）
- **命令锚点一览**：`2537` 正例 Arm · `2574` 正例 obj gc · `2577` 正例 Verify · `2613` 负例 Arm · `2622` 负例 obj gc · `2625` 负例 Verify（另：`2401` 为首轮失败尝试，见上节）

### 正例（`Arm` 10.20.15 → `obj gc` 10.20.19 → `Verify` 10.20.34）

| 锚点 | 原文 | 判定 |
|---:|---|---|
| 2537 | `Cmd: Tcs.Test.Gc.Arm` | — |
| 2539 | `UTcsDevGcProbe: 驱动类解析为 TcsDevGcProbeDriver_C（候选 2 个）` | 类查找正常 |
| 2567 | `[PASS] 基线：选中 2 个目标（预期 2）/ 扣血 7（预期 7）/ 脚本流程步骤调用 2（预期 2）` | **预测 #1 命中** |
| 2568 | `[PASS] 布置：四个槽位已挂进链/模板；驱动侧引用已清空，存活只取决于框架登记表` | **预测 #2 命中** |
| 2570 | `GC_READY：请在控制台执行 obj gc，然后跑 Tcs.Test.Gc.Verify` | — |
| 2574 | `Cmd: Obj GC` | **人工 GC 已执行**（D3 要求） |
| 2580 | `GC_CHECK_BEGIN：被测对象 4 个，存活 4 个（弱引用判定……）` | **预测 #3 命中（4/4）** |
| 2581-2584 | 四条 `槽位 …：存活`（选择器 / 过滤器 / 伤害委托 / 流程执行器） | **预测 #3 逐槽位命中** |
| 2604 | `[PASS] GC 存活性：被测脚本对象在真实原生 GC 后仍存活（装置侧弱引用仍可解析）` | — |
| 2605 | `[PASS] 选择器/过滤器槽位：GC 后仍选中 2 个目标（预期 2——脚本选择器与过滤器仍被抵达）` | **预测 #4 命中** |
| 2606 | `[PASS] 流程执行器槽位：脚本流程步骤调用增量 2（预期 2）` | **预测 #5 命中** |
| 2607 | `[PASS] 伤害委托槽位：扣血 7（预期 7——C# 公式仍被抵达）` | **预测 #6 命中** |
| 2608 | `[PASS] 正例通过：脚本对象在真实原生 GC 后仍存活，且各槽位行为与调用计数符合基线` | **预测 #7 命中** |
| 2609 | `GC_CHECK_END` | — |

### 负例（`Arm --negative` 10.20.46 → `obj gc` 10.20.51 → `Verify` 10.20.54）

| 锚点 | 原文 | 判定 |
|---:|---|---|
| 2613 | `Cmd: Tcs.Test.Gc.Arm --negative` | — |
| 2619 | `[PASS] 布置：负对照模式：四个夹具均未挂进链/模板，引擎侧零反射引用` | **预测 #8 命中** |
| 2622 | `Cmd: Obj GC` | 人工 GC 已执行 |
| 2628 | `GC_CHECK_BEGIN：被测对象 4 个，存活 0 个（弱引用判定……）` | **预测 #9 命中（0/4）** |
| 2629-2632 | 四条 `槽位 …：已回收` | 逐槽位命中 |
| 2633 | `[FAIL] GC 存活性：被测脚本对象已被真实原生 GC 回收（未通过存活）` | **负例必须失败——命中** |
| 2635 | `[PASS] 负对照如期失败：被测对象已被真实原生 GC 回收 ⇒ 本装置能观测到"失去保活"的后果（装置可信）` | **预测 #10 命中** |
| 2636 | `GC_CHECK_END` | — |

**预注册预测 10 条全部命中，无一条落空。**

## 结论与边界

**结论（本次实跑确立的事实）**：

1. **框架的四条保活路径全部成立**：四个宿主脚本槽位（选择器 / 过滤器 / 流程步骤执行器 / 伤害流程委托）在**真实原生 GC**（人工 `obj gc`，全量非增量）之后**仍存活且仍可被调用**——行为与计数（选中 2 目标 / 扣血 7 / 脚本步骤调用 +2）与 GC 前基线逐项一致。
2. **装置**能**报失败**，不是恒绿门禁**：负对照（夹具不挂任何引擎可见引用）在同一会话内 4/4 被回收，装置据此报出 `[FAIL]`，并按设计输出"装置可信"。
3. **两个"预测落空"分支都未发生**——这一条同样是结论：UnrealSharp **没有**在托管侧把所有 C# `UObject` 钉住（否则负例不会全灭），框架登记表**确实**是存活与否的决定因素（否则正例不会全活）。即"脚本插槽静默失效"这一顾虑**是真实的**，而本装置现在能把它变成可见数字。

**边界（MUST NOT 外推）**：

1. 只覆盖"**脚本对象作为宿主插槽被框架持有**"这一形态：不测 UnrealSharp 热重载存活性、不测 C++ 侧对象 GC 行为、不测性能/分配量（`design.md` Non-Goals）。
2. **`obj gc` 是人工步骤**：上述结论只在"日志确证执行过 `obj gc`"（锚点 2574 / 2622）的前提下成立。
3. **单进程单会话结论**：正例的"存活"依赖框架登记表在**该世界**内的持有；跨世界寿命语义不在本次覆盖内（另见注册表跨世界寿命的 `EVID-2026-09-29`）。
4. **负例的判据是弱引用死活**（不是"回收后再调用它"——那会是 UB，见上文 D6′）：因此本文件不构成"已回收对象被调用时的行为"的证据。

## 边界（MUST NOT 外推）

1. **只覆盖"脚本对象作为宿主插槽被框架持有"这一形态**：不测 UnrealSharp 的**热重载**存活性，不测 C++ 侧对象的 GC 行为，不测性能/分配量（`design.md` Non-Goals）。
2. **`obj gc` 是人工步骤**：`Verify` 的输出只有在此前确实执行过 `obj gc` 时才是 GC 证据（装置自带该诚实标注）；本文件不得被读成"未经 GC 也成立"。
3. **同一 PIE 会话内正例只能 `Arm` 一次**：`RegisterStepExecutor` 按 step struct 去重且无 Unregister API；新 PIE 会话重置世界子系统登记表。负例不受此限。
4. **证据落点说明**：本文件落在**插件仓**的证据目录，而提案 `tasks.md` 的 1.3 写"不动插件仓"——两者不矛盾：1.3 约束的是**插件仓的代码与规格**（本装置零插件代码改动），而证据目录是本仓与宿主共用的**跨仓记录位**（先例：`2026-09-28-host-scripting-e2e-pie.md`、`2026-09-28-selector-ref-pie.md` 均为宿主侧装置的证据）。
