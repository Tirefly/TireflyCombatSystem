## 1. 基线与入口盘点

- [x] 1.1 读取现有 `ATcsChainProbe`、`UTcsProbeDamageFormula`、TCS 生成 glue 和当前追踪矩阵，建立本提案的验证基线；确认不修改已归档提案。
- [x] 1.2 盘点 UnrealSharp 可用的 `[UStruct]`、`UObject` timer、`ResumeRun` 和 Unreal GC 触发/观测入口；将可用入口和受限入口写入设计注记。
- [x] 1.3 为新探针确定固定日志前缀、唯一 GameplayTag 和每个场景的通过判据；不得复用旧探针的成功 tag 导致幂等误判。
- [x] 1.4 修复 UnrealSharp glue generator 对 RefKind.Out 参数的调用顺序：初始化原生参数缓冲区、跳过 out 参数调用前序列化、native call 后回填并销毁容器临时值；重建 generator 后确认 C# 项目编译通过。

## 2. C# 验证夹具

- [x] 2.1 新增 `Script/LegendAutoChessCS/TcsProbe/TcsHostScriptingE2EProbe.cs`，负责实体注册、阶段调度、链/模板登记、断言和日志汇总。
- [x] 2.2 新增 `Script/LegendAutoChessCS/TcsProbe/TcsProbeEffectStep.cs`，声明 `[UStruct] FTcsProbeEffectStep`，至少包含可辨识的 `Marker` UPROPERTY，并确认 glue 生成真实字段读写。
- [x] 2.3 新增 selector/filter host 辅助类，按 UnrealSharp 的 UInterface 两段式规则实现 `ResolveTargets` / `PassTarget`；分别记录调用次数和固定输出。
- [x] 2.4 新增 `UTcsProbeEffectExecutor`，实现同步完成和首入 `TSR_Running`、timer 调 `ResumeRun`、二次进入完成；不得在持有运行态引用时启动新链。
- [x] 2.5 新增 `UTcsProbeFlowExecutor` 与 `FTcsProbeFlowStep`，实现可观察的流程继续/中止结果；通过 `RegisterStepExecutor` 绑定自定义流程步骤类型。
- [x] 2.6 managed build/glue 检查：自定义 USTRUCT、UObject 子类、接口覆写和 `FInstancedStruct.Make` 均生成真实代码；失败时保留失败证据，不勾后续行为任务。

## 3. Selector / Filter E2E

- [x] 3.1 注册至少两个实体，构造使用 `FTcsSelHostDelegate` 的链，验证 selector 输出目标集的顺序可由 `GetRunTargets` 读回。
- [x] 3.2 配置两个 filter：第一个对指定候选返回 false，第二个增加调用计数；验证 AND 短路使第二个 filter 不被调用。
- [x] 3.3 分别运行空 selector Host 和空 filter Host，验证“空集 + Warning”和“通过候选”语义；记录预期 Warning 与 Error/ensure 的区分。
- [x] 3.4 将 selector/filter 的 PIE 日志和结果写入追踪矩阵，未覆盖的实体查询能力继续标为 C++ 专用。

## 4. Effect 步骤执行器 E2E

- [x] 4.1 用 `FTcsProbeEffectStep` 通过 `UTcsEffectSubsystem.RegisterStepExecutor` 登记 `UTcsProbeEffectExecutor`，构造链并验证同步执行器写入的运行态标记可由访问器读回。
- [x] 4.2 运行挂起路径：首次执行返回 `TSR_Running`，链保持活动，timer 调 `ResumeRun`，二次执行返回 `TSR_Completed`，链最终完成。
- [x] 4.3 清空探针对执行器的字段引用，执行 Unreal GC 门控后再次运行同步步骤；若当前入口不能可靠触发/观测 Unreal GC，记录受限并转入台账。
- [x] 4.4 记录 `UTcsStepExecutor` 的同步、挂起/唤醒和 GC 结果；不得用一次 `TSR_Running` 日志替代完整唤醒证据。

## 5. Damage Flow 步骤执行器 E2E

- [x] 5.1 用同一或独立 `[UStruct]` 步骤类型通过 `UTcsDamageSubsystem.RegisterStepExecutor` 登记 `UTcsProbeFlowExecutor`，组装含该步骤的流程模板。
- [x] 5.2 运行模板并验证 C# 流程步骤被调用，返回值对流程继续/中止产生可观察结果；不得通过修改只读 `FTcsDamageFlowContextView` 伪造写回。
- [x] 5.3 清空探针对 Flow executor/delegate 的字段引用，执行 Unreal GC 门控后再次起流程；验证登记表的 GC 可见持有，或将受限结果转入台账。
- [x] 5.4 将流程步骤执行器的注册、执行、GC 结果写入追踪矩阵和 C# 调研文档。

## 6. 悬空句柄 E2E

- [x] 6.1 等待验证链自然完成并保留旧 `FTcsChainRunHandle`，调用所有按句柄读口，验证返回空值/无效句柄。
- [x] 6.2 调用 `SetRunTargets` / `SetRunVariable` 等写口，验证返回 `false + Warning`，日志没有 `ensure`、`Error` 或崩溃。
- [x] 6.3 记录代际失配与已释放句柄两类边界；不要把正常 Warning 清理路径写成失败。


> **2026-09-28 收到的 PIE 日志（第一批；日志正文未注明采集日期）**：`C:/Users/TireflyPC/.codex/attachments/a772bf8e-5cf7-44be-88d9-fd6a0cf4cddf/已粘贴的文本.txt`。日志 65–86 行：宿主 selector 产出 2 候选，两个 filter 对候选 2 短路、候选 3 全部通过；Effect 自定义步骤 `marker=17` 同步执行，`marker=99` 返回 `TSR_Running`，Timer 调 `ResumeRun` 后再次进入并完成。日志 93–99 行：已释放句柄读口返回空/无效，两个写口分别给出 `Warning + false`；过滤器调用计数符合短路。日志 100–116 行：Flow 执行器 `false` 时只调第 0 步，`true` 时两个步骤依序执行。该日志未包含 selector 未过滤时的目标身份和顺序、空 Host、同步步骤写入变量后的读回、GC、池槽代际复用。日志片段未见 `FAIL`、`Error`、`ensure` 或 `Fatal`；既有探针的清理 Warning 和本轮预期的中止/悬空 Warning 应分别标注，不推断为全部任务通过。
> **2026-09-28 第二批 PIE 日志**：`C:/Users/TireflyPC/.codex/attachments/1ab5b86e-9434-452e-85b8-0f8cea257090/已粘贴的文本.txt`。第 29–43 行：同步 Marker=17 写入后在下一步读回、运行态目标为指定 secondary，随后 `TSR_Running → ResumeRun → Completed`；第 51–56 行：旧 `{Index=0,Generation=1}` 在复用槽 `{Index=0,Generation=3}` 活动时不能读写新运行态；第 57–77 行：未过滤 selector 的目标顺序 `[1,2]`、空 selector Host 的空集 + Warning、空 filter Host 的候选全通过均已实证。第 80–94 行：Flow 中止与继续路径仍成立。第 116–117 行 `GC_READY` 与 `GC_CHECK_BEGIN` 之间，所附文本未包含原生 `obj gc` / `LogGarbage` 完成记录；第 127/135/145 行的 `POST_GC` 仅证明清空探针字段后仍可调用，**不得记为经过 Unreal GC 的保活证据**。第 139/146 行的 `No world was found` Warning 来自瞬态 Outer 的 `UTcsProbeDamageFormula` 打印日志，不影响本次伤害最终值，但应单独修整日志上下文。
> **2026-09-28 本机完整 Editor 日志补证**：持久化摘录 `Documents/combat-system-design/evidence/2026-09-28-host-scripting-e2e-pie.md`；原始 `E:/Projects_Dev/LegendAutoChess/Saved/Logs/LegendAutoChess.log:2656–2707`。第 2656 行 `GC_READY`，第 2663 行 `Cmd: Obj GC`，第 2664 行 `Collecting garbage and resetting GC timer.`，第 2665 行对象哈希表压缩；第 2667 行才出现 `GC_CHECK_BEGIN`。本机 UE 5.8 `Engine/Source/Runtime/Engine/Private/UnrealEngine.cpp:9338–9348` 对该命令同步执行 `CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true)`，因此此次不要求额外 `LogGarbage` Verbose 行。第 2677 行 selector/filter + Effect executor 实际再次调用，2685 行 Flow executor 双步骤调用，2693/2696 行 C# Damage delegate 后仍记 `Final=7`、Health 100→93。`LogScript: No world was found`（完整日志第 2689/2697 行）来自瞬态公式对象的 `PrintString`，不影响公式/GC 行为。跨 PIE 注册表的进程级寿命风险仍独立保留；此证据仅证明同一次 PIE 内的保活。
## 7. 跨语言盘点与文档收束

- [x] 7.1 盘点 AS、Luau、Puerts/TS 的实际插件、绑定产物和可运行测试入口；有入口则记录独立执行结果，无入口则明确写“未验证”。
> **跨语言入口盘点（2026-09-28）**：项目 `LegendAutoChess.uproject` 中启用 UnrealSharp；项目 `Plugins/` 下只有 `Tirefly/` 与 `UnrealSharp/`，`Script/LegendAutoChessCS/` 提供实际 C# 夹具。未发现项目级 AS/Luau/Puerts(TS) 插件、TCS 对应绑定产物或可运行 PIE 夹具，因此三者继续标记“未验证”，不能以 C# 的 UE 反射分发结果替代独立实测。`ITcsEntityQuery` 仍为 C++ 专用；跨 PIE 静态执行器注册表问题仍保留在 `reflection-backlog.md` R-2。
- [x] 7.2 更新 `Documents/combat-system-design/tcs-contract-traceability.md`：逐行写入各验证场景的代码、glue、PIE、GC 和未验证状态。
- [x] 7.3 更新 `Documents/combat-system-design/deferred-inputs-ledger.md`、`2026-09-23-csharp-tcs-logic-authoring-research.md` 和 `openspec/project.md` 的 S-8 Evidence Boundary；不得扩大跨语言承诺。
- [x] 7.4 运行 `openspec validate verify-tcs-host-scripting-e2e --strict --no-interactive`、`openspec validate --all --strict --no-interactive` 和 `git diff --check`。
- [x] 7.5 只有所有可运行场景有证据、受限场景有明确转台账记录、文档矩阵一致后，才归档本提案。




> **2026-09-28 归档门复核**：C# managed build 0 警告/0 错误；openspec validate verify-tcs-host-scripting-e2e --strict --no-interactive 通过；openspec validate --all --strict --no-interactive 24/24；主仓、TCS 与 UnrealSharp 的 git diff --check 无空白错误。行为证据已固化到 Documents/combat-system-design/evidence/2026-09-28-host-scripting-e2e-pie.md。跨 PIE 注册表缺陷由 R-2 继续跟踪，AS/Luau/TS 与 ITcsEntityQuery 均未冒充通过。
