# TCS 契约追踪矩阵

> 2026-09-26 收束轮建立。该矩阵区分代码静态证据、生成物证据、行为证据和未验证边界；“静态完成”不得替代 PIE/GC/跨语言行为验收。

> **2026-09-27 用户 PIE 复核补记**：用户在 `L_UnrealSharpDev` 中实际运行 `BP_TcsChainProbe`，S-8 探针覆盖的 `RegisterTemplate`、C# 公式抵达、句柄访问器往返和挂起链唤醒全部通过；本补记不扩大到 GC、脚本选择器/过滤器、脚本步骤执行器、悬空句柄或其他脚本语言。

| OpenSpec requirement | 当前代码证据 | 设计文档 | 提案任务 | 证据状态 |
|---|---|---|---|---|
| `plugin-descriptor / R3 模块物化声明` | `TireflyCombatSystem.uplugin`：7 个 Runtime 模块 | `openspec/project.md` Tech Stack / R3 context | `reconcile-tcs-contract-drift` 2.1–2.3 | 静态完成；无运行时需求 |
| `effect-chain / 链运行态句柄的反射性` | `Source/TcsEffect/Public/Chain/TcsChainRun.h`：展平 `Index`/`Generation`、`BlueprintType` | `04-module-effects.md` §5b；C# 调研 §12 | `reconcile-tcs-contract-drift` 3.1；S-8 Task 6 | glue 静态核对 + C# PIE 已验证活动句柄与唤醒完成；GC 强保活及悬空句柄行为未单独实证 |
| `effect-interpreter / 门面反射面` | `Source/TcsEffect/Public/TcsEffectSubsystem.h`：运行控制与 6 个访问器；`SetEntityQuery/GetEntityQuery` 保持 C++ 面 | `04-module-effects.md` §7；`10-module-targeting.md` §2.3 | `reconcile-tcs-contract-drift` 3.2；S-8 Task 6 | 静态/glue 完成；C# PIE 已验证 `GetRunCaster`、`Set/TryGetRunVariable`、`GetRunTargets`；实体世界脚本访问、GC 与悬空句柄行为未实证 |
| `damage-primitive / 流程委托契约` | `TcsDamageFlowDelegate.h`：`ContextView`、`BlueprintNativeEvent`、`Execute_*` 调用点 | `09-module-damage.md` §2.4 | `reconcile-tcs-contract-drift` 3.3；S-8 Task 6 | glue 静态完成 + C# PIE 已验证模板登记、`Execute_*` 公式抵达及 `Final=7`；视图字段完整往返、GC 与其他语言未实证 |
| `damage-flow / 宿主脚本流程步骤登记` | `TcsDamageSubsystem` + `UTcsFlowStepExecutor`：脚本执行器注册入口与 GC 可见持有 | `09-module-damage.md` §2.4 | `add-host-scripting-slots` 4.5；`reconcile-tcs-contract-drift` 3.5 | 静态/glue 已覆盖；本次 C# PIE 未运行脚本步骤执行器，GC 强保活仍未单独实证 |
| `effect-step-dispatch / 宿主脚本步骤执行器` | `TcsEffectSubsystem` + `UTcsStepExecutor`：UObject 执行器、注册入口、挂起返回 | `04-module-effects.md` §5b | `add-host-scripting-slots` 4.1–4.4；`reconcile-tcs-contract-drift` 3.5 | 静态/glue 已覆盖；本次 C# PIE 未运行脚本步骤执行器，挂起返回与 GC 场景仍未实证 |
| `targeting-strategy / 宿主脚本选择器插槽` | `TcsTargetSelectorHost.h`、`TcsSelHostDelegate.h` | `10-module-targeting.md` §2.1 | `reconcile-tcs-contract-drift` 3.4 | 静态完成；脚本端到端行为待验证 |
| `targeting-strategy / 宿主脚本过滤器插槽` | `TcsTargetFilterHost.h`、`TcsFilterHostDelegate.h` | `10-module-targeting.md` §2.2 | `reconcile-tcs-contract-drift` 3.4；S-8 Task 6.4 | 静态完成；脚本端到端组合行为仍未验证 |
| `entity-query-contract / 实体查询注入契约` | `TcsEntityQuery.h`：`TFunctionRef` 仍存在 | `10-module-targeting.md` §2.3；S-5 台账 | `reconcile-tcs-contract-drift` 4.2；S-8 Task 6.4 | C++ 专用；本次 S-8 PIE 只验证运行态目标访问，不包含脚本遍历世界 |

## 归档门槛

1. `add-host-scripting-slots` 的 PIE 公式抵达与访问器往返证据已记录；
2. GC 持有场景未由本次日志直接实证，但已在 S-8 证据边界中明确转入后续验证；
3. 本矩阵每一行的状态与设计文档、台账和生效规格一致；
4. `openspec validate --all --strict --no-interactive` 通过；
5. 未完成项未被任务清单或项目上下文标为“已落地”。
