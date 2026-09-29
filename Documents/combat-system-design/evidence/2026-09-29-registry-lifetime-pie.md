# TCS 注册表跨世界寿命修复：两连 PIE 行为证据

- **文档 ID**：`EVID-2026-09-29-registry-lifetime`
- **类型**：EVID / 证据
- **状态**：FROZEN
- **权威范围**：提案 `harden-registry-cross-world-lifetime` 的核心判据——**同一 Editor 进程内连续两次 PIE**，宿主脚本步骤执行器二次登记不再被拒
- **最后更新**：2026-09-29

- **被测对象**：`Source/TcsEffect`、`Source/TcsDamage` 的 4 张注册表（步骤执行器 / 流程步骤 / 条件求值器 / 载荷读取器）的**寿命语义**改造
- **验证范围**：TCS 插件 + LAC 宿主 C# 脚本插槽。地图 `L_UnrealSharpDev`，夹具 `BP_TcsHostScriptingE2EProbe`（`Script/LegendAutoChessCS/TcsProbe/TcsHostScriptingE2EProbe.cs`）——它在 `BeginPlay()` 里调 `RegisterStepExecutor` 登记 `FTcsProbeEffectStep` / `FTcsProbeFlowStep` 两个类型，**登记失败会提前 `return` 并打 `FAIL：… 登记失败`**，故"是否走到后续检查"即是登记成败的判据
- **完整来源**：`E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log`。取证时读取 **380465 字节 / 2806 行**（行号锚点均以该快照为准）。
  - **SHA-256 `1c61b8ae979ac4f2630cc38b5cde66f02b937c100a285d19a306b19381eabbce`** —— ⚠ **哈希口径说明**：该值由**共享读模式**取得，而取证时编辑器**仍在运行、日志仍在追加**，故它是"取证时刻"的哈希，**与上表中逐行锚点所在的那个 380465 字节快照并非严格同一时刻**。若要严格可比对，应在**关闭编辑器后**重新取哈希（届时文件不再增长）。此点如实标注，不外推为"哈希与行号严格对应"。
  - 编辑器重启后同名日志会被覆盖。

> **本证据来自重建后的复测**（2026-09-29 18:44–18:46）。首轮取证时的日志已被后续 PIE 覆盖；两侧数据一致（判据相同，且首轮另有 `Deinitialize` 诊断行可交叉印证生效机制），本条以**复测**为准——其测试条件更完整（**两次 PIE 都执行了 `obj gc`**）。

## 判据与结果（行号 = 复测日志快照）

| 序号 | 验证点 | 完整日志行 | 判定 |
|---:|---|---:|---|
| 1 | 第 1 次 PIE 起（世界建立） | 2402 | — |
| 2 | 第 1 次 PIE：探针走到 `RegisterStepExecutor` **之后**（实体准备完成） | 2412 | 登记成功 |
| 3 | 第 1 次 PIE：两个宿主执行器登记成功 | 2414（Effect）、2416（Damage） | 通过 |
| 4 | 第 1 次 PIE 内完整跑完 GC 三段门控（`obj gc` 已执行） | 2513 `GC_READY` → 2523 `GC_CHECK_BEGIN` → 2533/2541/2552 三项 POST_GC 通过 → 2563 `GC_CHECK_END` | 通过 |
| 5 | 第 1 次 PIE 结束：世界销毁 | 2571 | — |
| 6 | **同一 Editor 进程内紧接着起第 2 次 PIE** | 2581 `Repeating last play command` → 2644 世界建立 | 缺陷触发场景成立 |
| 7 | 第 2 次 PIE：探针再次走到登记之后（实体准备完成） | 2651 | **二次登记成功** |
| 8 | **第 2 次 PIE：同两个类型再次登记成功** | 2652（Effect）、2653（Damage） | **★ 核心判据通过** |
| 9 | 第 2 次 PIE 同样跑完 GC 三段门控 | 2750 → 2754 → 2764/2772/2783 → 2794 | 通过（与第 1 次同构） |
| 10 | 第 2 次 PIE 结束：世界销毁 | 2797 | — |

## 缺陷签名（应为零，实测零）

| 签名 | 命中 |
|---|---|
| `拒绝重复登记` | **0** |
| `已有执行器` | **0** |
| `保留首个` | **0** |
| `登记失败` | **0** |
| `Handled ensure` | **0** |

对照基线：**改动前**在同一场景下，第 2 次 PIE 的二次登记会命中"键已存在"分支，产出 `ensureMsgf(false, "…已有执行器——拒绝重复登记（保留首个）")` **红字**，并因保留首个而继续使用**旧世界的执行器指针**。

## 生效机制

**复测日志中不再包含** `Deinitialize` 诊断行（该行是首轮的临时插桩，**已按纪律从源码移除**——见 `tasks.md` §7/§9 的记录）。生效机制的判定依据改为两条：

1. **首轮诊断实测**（同一判据、同一装置）：`UTcsEffectSubsystem::Deinitialize: 持有执行器 1 个，注册表动态条目 1 个` ⇒ 动态条目**确实驻留在进程级注册表内**，并被 `Deinitialize` 清除；
2. **复测与首轮两侧均无**"替换为新登记"日志 ⇒ **生效的是"门面 `Deinitialize` 按世界显式撤销"这一半**，而非 `DiscardIfStale` 的替换路径（后者至今未被触发）。

这与设计意图一致——显式撤销是**整理手段**，`DiscardIfStale` 是**显式撤销失效时的保险**，正确性不依赖后者。

## 本轮环境噪声（非缺陷，逐项归类）

| 类别 | 数量 | 说明 |
|---|---:|---|
| `Error` | 2 | 引擎 `GameFeatureData` 资源管理器配置提示（`LogGameFeatures` / `LoadErrors`），每轮启动均有、与本批无关 |
| TCS/探针 `Warning` | 18 | **全部是装置刻意覆盖的降级路径**（各 8 条：`UnregisterTemplate 未登记`、`SetRunTargets/SetRunVariable 句柄悬空或已释放` ×3、`SelHostDelegate Host 未配置`、`RunTemplate 第 0 步中止` + `未走完`；再加 `No world was found` ×2 —— 后者是**已知夹具噪声**：瞬态 `TcsProbeDamageFormula` 对象无 World 时调 `PrintString`，见 `2026-09-28` 验证边界记录）。**两次 PIE 的 Warning 集合完全一致**，第二次无任何劣化迹象 |
| 引擎环境 `Failed to load` | 12 | `aqProf.dll` / `VtuneApi.dll` / `VtuneApi32e.dll` / `WinPixGpuCapturer.dll` / `Wintab32.dll` 未装；Android/iOS/Linux/Mac/TVOS 平台 SDK 未配置 |

> **注意**：`FAIL` 一词在此日志中共 14 处命中，**无一处属于 TCS 或探针**——全部是上表第三行的引擎环境加载失败。


## 证据边界

**本证据覆盖**：同一 Editor 进程、连续两次 PIE 下，**步骤执行器注册表**（Effect + Damage 两张）的动态登记不再被失效键拒绝；且 `Deinitialize` 确实观察到并清除了动态条目。

**本证据不覆盖**（MUST NOT 外推）：

1. **`Find` 侧的跨世界/已回收判定**与 **`DiscardIfStale` 的替换路径****均未被触发**——因为显式撤销总是先行清干净。若要单独验它们，需构造"条目未被撤销但对象已回收"的场景（例如宿主绕过子系统销毁直接重登记）。**按用户 2026-09-27 裁定"先实测但只做记录"，此处只记录，不因结果改变设计。**
2. **条件求值器 / 载荷读取器两张注册表**：本次装置未登记它们的动态条目（载荷读取器全项目登记数为 0），故其寿命语义同样**未经行为验证**（逻辑同构，属静态实现）。
3. **对象已被 GC 的路径**：**复测中两次 PIE 都执行了 `obj gc`**（第 1 次与第 2 次均含完整 GC 三段门控，见判据表第 4/9 行）——故"对象被回收后登记仍能成功"这条**已被本次覆盖**。仍不覆盖的是"条目未被撤销、对象已被回收"这一**构造性**场景（同上第 1 条）。
4. **AS / Luau / TS** 等其他脚本语言未独立往返。
5. **全新克隆从零构建**未执行。
6. **哈希与行号的严格同时性**：见开头"哈希口径说明"——取证时编辑器仍在写日志。

## 同批的编译判据（重建后实测）

| 配置 | 结果 | 规模 | error | warning |
|---|---|---|---|---|
| `LegendAutoChessEditor Win64 Development` | Succeeded | 30 action / 62.34 s | 0 | 0 |
| `LegendAutoChess Win64 Shipping` | Succeeded | 15 action / 97.75 s | 0 | 0 |

> **口径说明**：`LegendAutoChessEditor` **不支持 Shipping**（编辑器目标无该配置，UBT 原文 "LegendAutoChessEditor does not support the Shipping configuration"）。Shipping 验证 MUST 用 **Game 目标** `LegendAutoChess`。

## 伴随的既有回归网

`Tcs.Test.Slice.Run` 逐次跑到链执行完成、`Tcs.Test.Slice.Reject` 每次 `通过 3 / 失败 0`（检查 A/B/C 全 PASS）——**C++ 快路径行为不变**（双轨并存的预期）。


> **卡顿说明（供后续复核者）**：第 1 次 `Slice.Reject` 会出现约 **2.2 秒停顿**，来源是检查 B 的预期 `ensure` 触发 `FDebug::EnsureFailed`（`StackWalkAndDump` 0.749 s + `SendNewReport` 1.442 s）。**属预期行为**，并会生成一份 `Saved/Crashes/UECC-*` 报告（由 `SendNewReport` 产出，非崩溃）。同一调用点的 `ensure` 每会话只弹一次，故后续 Reject 无停顿。
