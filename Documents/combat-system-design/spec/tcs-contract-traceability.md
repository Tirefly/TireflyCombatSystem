# TCS 契约追踪矩阵

- **文档 ID**：`SPEC-TRACE`
- **类型**：SPEC / 追踪矩阵
- **状态**：LIVING
- **权威范围**：OpenSpec 需求 → 代码证据 → 设计文档 → 提案任务的追踪与验证边界
- **最后更新**：2026-09-29

> 2026-09-26 收束轮建立。该矩阵区分代码静态证据、生成物证据、行为证据和未验证边界；“静态完成”不得替代 PIE/GC/跨语言行为验收。

> **2026-09-27 用户 PIE 复核补记**：用户在 `L_UnrealSharpDev` 中实际运行 `BP_TcsChainProbe`，SCRIPT-8 探针覆盖的 `RegisterTemplate`、C# 公式抵达、句柄访问器往返和挂起链唤醒全部通过；本补记不扩大到 GC、脚本选择器/过滤器、脚本步骤执行器、悬空句柄或其他脚本语言。

> **2026-09-28 `ref` 选择器复核**：新签名的 PIE 与原生 GC 证据见 `Documents/combat-system-design/evidence/2026-09-28-selector-ref-pie.md`；首轮完整日志快照行号为 2511–2661；编辑器重启后同名文件已变为第二轮日志，现有 `Saved/Logs/LegendAutoChess.log:2507–2657` 对应第二轮。首轮结论仅覆盖 C++ → C# 回调及原生容器回写；随后撤销 UnrealSharp 修补，重建原版生成器并发布 LAC 程序集，第二轮 PIE 再次通过（见同一证据文件第二轮记录）。全新克隆构建和 C# 反向主动调用仍未验证。

> **2026-09-28 SCRIPT-8 E2E 增量**：持久化摘录见 `Documents/combat-system-design/evidence/2026-09-28-host-scripting-e2e-pie.md`；原始 Editor 日志为 `Saved/Logs/LegendAutoChess.log:2656–2707`。顺序为 `GC_READY → Cmd: Obj GC → Collecting garbage → 对象哈希压缩 → GC_CHECK_BEGIN`。本机 UE 5.8 `UnrealEngine.cpp:9338–9348` 的 `Obj GC` 同步调用 `CollectGarbage(..., true)`；随后 C# selector/filter、Effect/Flow executor 和 Damage delegate 均再次执行，伤害结果为 `Final=7`、Health 100→93。只证明同一次 PIE 中探针字段已清空后的 Unreal GC 保活，不能外推跨 PIE/world 寿命。

| OpenSpec requirement | 当前代码证据 | 设计文档 | 提案任务 | 证据状态 |
|---|---|---|---|---|
| `plugin-descriptor / R3 模块物化声明` | `TireflyCombatSystem.uplugin`：7 个 Runtime 模块 | `openspec/project.md` Tech Stack / R3 context | `reconcile-tcs-contract-drift` 2.1–2.3 | 静态完成；无运行时需求 |
| `effect-chain / 链运行态句柄的反射性` | `Source/TcsEffect/Public/Chain/TcsChainRun.h`：展平 `Index`/`Generation`、`BlueprintType` | `04-module-effects.md` §5b；C# 调研 §12 | `reconcile-tcs-contract-drift` 3.1；SCRIPT-8 Task 6 | glue + C# PIE：活动句柄挂起/唤醒、完成后读写空值及同槽 Index=0 的 Generation 1→3 复用已验证；跨 PIE 生命周期另见 R-2。 |
| `effect-interpreter / 门面反射面` | `Source/TcsEffect/Public/TcsEffectSubsystem.h`：运行控制与 6 个访问器；`SetEntityQuery/GetEntityQuery` 保持 C++ 面 | `04-module-effects.md` §7；`10-module-targeting.md` §2.3 | `reconcile-tcs-contract-drift` 3.2；SCRIPT-8 Task 6 | C# PIE：活动运行态的目标/变量/施法者读写与悬空/代际失配的安全拒绝已验证；`ITcsEntityQuery` 仍为 C++ 专用，跨 PIE 生命周期另见 R-2。 |
| `damage-primitive / 流程委托契约` | `TcsDamageFlowDelegate.h`：`ContextView`、`BlueprintNativeEvent`、`Execute_*` 调用点 | `09-module-damage.md` §2.4 | `reconcile-tcs-contract-drift` 3.3；SCRIPT-8 Task 6 | C# PIE：模板登记、`Execute_*` 公式抵达、`Final=7`；同次 PIE 执行原生 `Obj GC` 后无探针字段引用的 delegate 仍使 Health 100→93；视图全字段及其他语言未验证。 |
| `damage-flow / 宿主脚本流程步骤登记` | `TcsDamageSubsystem` + `UTcsFlowStepExecutor`：脚本执行器注册入口与 GC 可见持有 | `09-module-damage.md` §2.4 | `add-host-scripting-slots` 4.5；`reconcile-tcs-contract-drift` 3.5 | C# PIE：Flow executor 返回 false 中止第 2 步、返回 true 依序执行 2 步；原生 GC 后继续执行两步。跨 PIE 寿命缺陷仍待独立处理。 |
| `effect-step-dispatch / 宿主脚本步骤执行器` | `TcsEffectSubsystem` + `UTcsStepExecutor`：UObject 执行器、注册入口、挂起返回 | `04-module-effects.md` §5b | `add-host-scripting-slots` 4.1–4.4；`reconcile-tcs-contract-drift` 3.5 | C# PIE：同步标记写入/读回、`TSR_Running → ResumeRun → Completed`；原生 GC 后同步步骤再次抵达。跨 PIE 注册表仍保留旧世界转发器，见 R-2。 |
| `targeting-strategy / 宿主脚本选择器插槽` | `TcsTargetSelectorHost.h`、`TcsSelHostDelegate.h` | `10-module-targeting.md` §2.1 | `reconcile-tcs-contract-drift` 3.4 | C# `ref IList` PIE：原生回调回写后未过滤顺序 `[1,2]`、空 Host 空集 + Warning、原生 GC 后 selector 再次执行均验证（`evidence/2026-09-28-selector-ref-pie.md`）；原版 UnrealSharp 生成器重建及第二轮 PIE 通过；全新克隆构建与 C# 主动调用反向回写未验证。 |
| `targeting-strategy / 宿主脚本过滤器插槽` | `TcsTargetFilterHost.h`、`TcsFilterHostDelegate.h` | `10-module-targeting.md` §2.2 | `reconcile-tcs-contract-drift` 3.4；SCRIPT-8 Task 6.4 | C# PIE：AND 短路、空 Host 全通过、原生 GC 后 filter 再次执行均验证。 |
| `entity-query-contract / 实体查询注入契约` | `TcsEntityQuery.h`：`TFunctionRef` 仍存在 | `10-module-targeting.md` §2.3；SCRIPT-5 台账 | `reconcile-tcs-contract-drift` 4.2；SCRIPT-8 Task 6.4 | C++ 专用；本次 SCRIPT-8 PIE 只验证运行态目标访问，不包含脚本遍历世界。**2026-09-29 定案**：`DEC-04` 裁定 ③ 判定"不换"——`TFunctionRef` 的语义/热路径/蓝图不承诺三条判据不随时间改变，本条自此不再是待办 |
| `effect-step-dispatch / 注册表寿命语义` | 4 张注册表：`Lifetimes` 表（对象/世界弱引用）+ 失效判据 + 拒绝门收窄 + `Unregister`/`GetDynamicKeys` + 门面 `Deinitialize` 按世界撤销 | `04-module-effects.md` §5b；`09-module-damage.md` §2.2 | `harden-registry-cross-world-lifetime`（2026-09-29） | **PIE 已验证（两连 PIE）**：同一 Editor 进程连续两次 PIE，Effect/Damage 两个宿主步骤执行器**二次登记均成功**，`拒绝重复登记` 等缺陷签名零命中（`evidence/2026-09-29-registry-lifetime-pie.md`）。**未覆盖**：`Find` 跨世界判定与 `DiscardIfStale` 替换路径未被触发；条件求值器/载荷读取器两张表无动态登记故未经行为验证；对象已 GC 路径未单独构造 |
| `damage-flow / 流程步骤注册表寿命语义` | `FTcsFlowStepExecutorRegistry`：与 Effect 侧同构同款改造 | `09-module-damage.md` §2.2 | `harden-registry-cross-world-lifetime`（2026-09-29） | 同上一行（本行只记落点，口径不重复定义） |

## 2026-09-28 验证边界与后续输入

- 本次手动 GC 命令和行为复核同处一个 PIE 世界；`Saved/Logs/LegendAutoChess.log:2663–2696` 为可复核依据。所贴的 155 行文本省略了命令/引擎输出，必须连完整日志一起引用。
- `UTcsProbeDamageFormula` 使用瞬态 Outer 调 `PrintString` 产生 `No world was found` Warning；公式仍被 C++ 抵达、实际扣除 7，属于验证装置日志上下文噪声。下次修改夹具时改用有 World 的日志上下文。
- Effect/Flow 动态执行器的进程级注册表跨 PIE 保留旧世界裸指针，此次单次 PIE 不覆盖；按 `reflection-backlog.md` R-2 中既有用户裁定，先记录，后续优先设计 TCS 自身兜底，A1 注销为退路。下一轮验证前须重启 Editor 进程。**★ 2026-09-29 已闭环**：提案 `harden-registry-cross-world-lifetime` 落地并经两连 PIE 实测——**"下一轮验证前须重启 Editor 进程"这条限制不再需要**（证据见 `evidence/2026-09-29-registry-lifetime-pie.md`；生效机制为门面 `Deinitialize` 按世界显式撤销，`DiscardIfStale` 替换路径仍未触发）。
- 本仓只有 UnrealSharp/C# 的项目级宿主和 TCS 插槽测试入口；AS、Luau、Puerts/TS 未提供可运行的独立行为证据，继续标记未验证。

## 2026-09-27 旧提案归档门槛（历史记录）

1. `add-host-scripting-slots` 的 PIE 公式抵达与访问器往返证据已记录；
2. GC 持有场景未由本次日志直接实证，但已在 SCRIPT-8 证据边界中明确转入后续验证；
3. 本矩阵每一行的状态与设计文档、台账和生效规格一致；
4. `openspec validate --all --strict --no-interactive` 通过；
5. 未完成项未被任务清单或项目上下文标为“已落地”。
