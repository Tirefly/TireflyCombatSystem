# Change: 验证 TCS 宿主脚本插槽端到端闭环

## Why

`add-host-scripting-slots` 已经完成静态实现、glue 核对和 UnrealSharp/C# 的伤害流程 PIE 子集验证，但追踪矩阵仍明确列出 GC 保活、脚本选择器/过滤器、脚本步骤执行器、悬空句柄和其他脚本语言边界未被单独实证。当前代码契约已经归档，下一步需要一个独立的验证提案把这些行为证据补齐，避免重新打开已经归档的实现提案。

## What Changes

- 新增 LAC 侧 C# 宿主脚本 E2E 验证装置，覆盖现有 S-8 插槽契约，不修改插件运行时分派语义。
- 验证 C# 宿主选择器和过滤器的目标输出、AND 短路、空 Host 降级与保序语义。
- 验证 `UTcsStepExecutor` 的同步完成、`TSR_Running` 挂起/唤醒以及 `UTcsFlowStepExecutor` 的流程登记与执行。
- 验证执行器、流程 delegate 在清空探针侧强引用并执行 Unreal GC 后仍由门面/登记表保活；`.NET GC.Collect()` 不作为 Unreal GC 证据。
- 验证活动链完成后的悬空句柄访问器语义：读口为空/无效，写口 `false + Warning`，不产生 `ensure`。
- 将每项结果写入追踪矩阵、遗留台账、C# 调研与 S-8 证据边界；对 AS/Luau/TS 只记录当前宿主可用性和是否具备测试条件，不从 C# 结果外推跨语言支持。

## Impact

- Affected specs: 新增 `host-scripting-e2e-validation` 验证能力规格；现有 `effect-step-dispatch`、`damage-flow`、`targeting-strategy`、`effect-interpreter` 的运行时契约不改，只补行为证据映射。
- Affected code: `Script/LegendAutoChessCS/TcsProbe/` 下新增或扩展验证装置；原则上不修改 TCS 插件 C++ 运行时。为支持 `BlueprintNativeEvent` 的 `out` 数组接口，本提案允许对本地 UnrealSharp glue generator 做最小工具链修复；该修复不改变 TCS 运行时契约。如验证暴露真实 TCS 运行时缺陷，必须另开实现提案，不在本提案中静默修复。
- Affected documents: `tcs-contract-traceability.md`、`deferred-inputs-ledger.md`、`2026-09-23-csharp-tcs-logic-authoring-research.md`、`openspec/project.md`。
- Verification: Unreal Engine 5.8 PIE、UnrealSharp C# glue、定向日志检查、`openspec validate --all --strict --no-interactive`。

## Non-Goals

- 不重新打开或改写已归档的 `add-host-scripting-slots`、`reconcile-tcs-contract-drift`。
- 不改变 `UTcsStepExecutor`、`UTcsFlowStepExecutor`、selector/filter、句柄访问器或 GC 持有的运行时契约。
- 不反射化 `ITcsEntityQuery`，不把脚本遍历世界、取位置、判存活写成已完成能力。
- 不嵌入 AS、Luau、Puerts/TS 或其他脚本引擎；没有可运行宿主时只登记为未验证边界。
- 不把人工执行的 Unreal 控制台 `gc` 与 .NET `GC.Collect()` 混为同一种证据。

