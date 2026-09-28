# Design: TCS 宿主脚本插槽端到端验证

## Context

TCS 的 S-8 插槽已经进入生效规格并完成 C# 伤害流程 PIE 子集。现有追踪矩阵把剩余风险分成四类：对象寿命、插槽行为、句柄竞态和跨语言边界。本提案只补验证装置与证据，不重新设计插槽接口。

当前工程已有 `ATcsChainProbe` 和 `UTcsProbeDamageFormula`。新验证装置与旧探针分离，避免把已通过的基线探针继续膨胀成不可维护的单体，同时复用现有实体注册、链登记、模板登记和日志约定。

## Goals / Non-Goals

### Goals

- 在同一套 PIE 夹具中得到可逐项审阅的 C# 行为证据。
- 让自定义步骤数据、selector/filter host、Effect/Flow executor 都由脚本层真实创建并登记。
- 对 `TSR_Running` 使用真实运行态句柄和唤醒回调，而不是只调用一次方法后宣称挂起可用。
- 对 GC 使用 Unreal GC 的人工门控流程，证明清空探针字段后登记表/门面仍能保活对象。
- 对悬空句柄记录返回值和日志，确认不升级为 `ensure`。

### Non-Goals

- 不把验证夹具变成生产插件 API。
- 不改变已有 C++ 注册表、双轨分派或访问器实现。
- 不把 C# 证据扩展解释为 AS/Luau/TS 的行为证据。

## Decisions

### D1：新建独立 C# PIE 验证装置

新增 `ATcsHostScriptingE2EProbe` 作为编排 Actor，保留既有 `ATcsChainProbe` 的历史基线职责。辅助类型按职责拆分：

- `FTcsProbeEffectStep` / `FTcsProbeFlowStep`：`[UStruct]` 自定义步骤数据，至少带一个可辨识的 `Marker` 字段；
- `UTcsProbeTargetSelectorHost` / `UTcsProbeTargetFilterHost`：实现 selector/filter 反射接口；
- `UTcsProbeEffectExecutor`：继承 `UTcsStepExecutor`，覆盖同步完成和挂起/唤醒路径；
- `UTcsProbeFlowExecutor`：继承 `UTcsFlowStepExecutor`，返回可观察的继续/中止结果；
- `UTcsProbeDamageFormula`：复用既有对象或抽取为共享辅助，不复制已有公式逻辑。

每个辅助对象只保存本次测试所需的最小状态，所有通过/失败均使用带固定前缀的 `PrintString` 日志。

### D2：步骤执行器挂起使用真实句柄唤醒

`UTcsProbeEffectExecutor` 第一次收到某个运行句柄时记录首入并返回 `TSR_Running`，同时保存句柄和门面引用。通过该 UObject 自身的 UnrealSharp timer 方法，在短延迟后调用 `UTcsEffectSubsystem.ResumeRun`；再次收到同一句柄时返回 `TSR_Completed`。这样验证的是解释器的真实 PC 停驻与重入，而不是伪造一个永远 Running 的执行器。

执行器不得在持有运行态引用期间启动新链；若需要登记结果，先通过句柄访问器写入变量，再结束当前调用。

### D3：selector/filter 使用可区分的实体序列

探针注册至少两个有效实体。selector 返回一个固定但非默认的顺序；filter 通过计数器和固定拒绝规则验证：

- selector 输出被转发器写入运行态目标集；
- filter 结果按 AND 组合；
- 前一个 filter 返回 false 后，后续 filter 不再被调用；
- 空 selector Host 返回空集并留 Warning；
- 空 filter Host 通过候选。

目标集最终通过 `GetRunTargets` 读取，不能只依赖 selector 自身日志。

### D4：GC 验证使用 Unreal GC 人工门

每个 GC 场景都遵循同一流程：

1. 创建并登记 delegate/executor；
2. 记录 `GC_READY`，清空探针 Actor 对该对象的强引用；
3. 在 PIE 控制台执行 Unreal `gc`；
4. 由不持有 executor 的 Actor timer 启动链或流程；
5. 以实际执行结果证明对象仍被门面或登记表保活。

只调用 `.NET GC.Collect()` 不得标记该任务通过。若当前 UnrealSharp 环境无法在不增加插件运行时 API 的前提下可靠触发或观测 Unreal GC，则记录为“测试入口受限”，转入台账，不伪造通过。

### D5：悬空句柄用已完成运行态验证

探针等待一个包含 `WaitDelay` 的链完成，再使用旧句柄调用：

- `GetRunTargets`；
- `GetRunCaster` / `GetRunInstigator`；
- `TryGetRunVariable`；
- `SetRunTargets` / `SetRunVariable`。

验收只看契约规定的空值/无效句柄、`false + Warning` 和无 `ensure`，不把 Warning 当失败。

### D6：跨语言采取能力盘点门

本提案强制验证 UnrealSharp/C#，对 AS/Luau/Puerts/TS 只做宿主、绑定和现有测试入口盘点。只有存在可运行的同等夹具并产生独立日志，才新增对应通过记录；否则保持“未验证”，不阻塞 C# 提案收束。

### D7：最小修复 UnrealSharp out 参数 glue 生成

现有 `ITcsTargetSelectorHost::ResolveTargets` 使用 `TArray<...>&`，C# glue 将其导出为 `out IList<...>`。当前生成器在调用 native 函数前错误地对该 `out` 变量执行 `ToNative`，会产生 CS0269/CS0177，阻断任何 C# selector 实现。验证提案允许在 `UnrealFunctionBase.ExportCallToNative` 中对含 `RefKind.Out` 的函数先初始化 UFunction 参数缓冲区、跳过 out 参数的调用前序列化，在 native call 后用 `ExportFromNative` 回填并销毁容器临时值；有返回值时最后再返回。该修复只影响 glue 生成顺序，不改变 UE 反射签名或 TCS 运行时语义。
### D8：同一编辑器进程重跑 PIE 的隔离门

当前 Effect/Flow 动态执行器注册表是进程级静态单例，转发器捕获执行器裸指针；门面只以本 PIE WorldSubsystem 的 `UPROPERTY` 数组保活，`Deinitialize` 不注销注册键。因此同一编辑器进程第二次 PIE 会拒绝同一步骤类型的重复注册，并可能继续使用上一世界的执行器对象。该缺口已登记在 `Documents/combat-system-design/reflection-backlog.md` 的 R-2 调研中，用户裁定先实测只记录，后续优先设计插件自身兜底，A1 注销方案作为退路。

本验证提案每次 PIE 只启动一次脚本执行器登记；下一轮重跑前必须退出并重新启动 Unreal Editor。停止 PIE 或触发 C# 热重载均不能替代进程重启。该临时隔离只保证验证夹具不主动触发重复注册；不代表跨 PIE 生命期缺陷已修复。Unreal GC 门控须在同一次 PIE 内完成，不能在退出该 PIE 后把旧结果当作 GC 证据。
## Risks / Trade-offs

| 风险 | 影响 | 处理 |
|---|---|---|
| Unreal GC 的手动门难以自动化 | GC 证据可能需要人工操作 | 固定 `GC_READY`/`GC_DONE` 日志和操作步骤；无法可靠执行时明确转台账 |
| C# 自定义 USTRUCT/glue 失败 | 无法登记自定义步骤 | 先做单独 glue 编译检查；失败则记录为绑定缺口，禁止改写成运行时通过 |
| `TSR_Running` timer 持有执行器 | 可能掩盖门面 GC 持有缺陷 | GC 场景和挂起场景分离；GC 场景使用不持有 executor 的外部启动器 |
| 空 Host 的 Warning 噪声 | 日志容易被误判为失败 | 日志前缀和验收表区分预期 Warning 与 Error/ensure |
| 其他语言没有宿主环境 | 无法给出跨语言行为证据 | 只完成能力盘点并保留未验证边界 |

## Migration Plan

1. 先添加 C# 验证夹具并通过 managed build/glue 检查。
2. 在 PIE 中按顺序运行基线、selector/filter、Effect executor、Flow executor、GC、悬空句柄。
3. 将每项结果写入追踪矩阵和台账，保持“静态 / glue / 行为 / 未验证”四级证据。
4. 若发现真实运行时缺陷，暂停本提案，另开实现提案；本提案只记录失败证据。
5. 全部验证项或明确的受限项落档后，运行 OpenSpec 严格校验并归档本提案。

## Open Questions

- 当前 UnrealSharp 项目是否能在不新增插件 C++ API 的情况下稳定触发并观测 Unreal `gc` 命令？任务 1 的入口盘点必须给出结论。
- AS/Luau/Puerts/TS 是否存在可直接复用的 TCS 插槽测试宿主？没有则不扩展本提案的实现范围。

