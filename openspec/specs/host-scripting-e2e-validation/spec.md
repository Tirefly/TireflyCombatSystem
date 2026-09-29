# host-scripting-e2e-validation Specification

## Purpose
定义宿主脚本插槽的端到端验证矩阵与其证据边界：插槽对象的 GC 保活证据、悬空句柄安全证据，以及跨语言承诺的登记。
## Requirements
### Requirement: SCRIPT-8 宿主脚本端到端验证矩阵

项目 MUST 提供一套可在 UnrealSharp/C# PIE 中运行的宿主脚本插槽验证装置，逐项覆盖已归档 SCRIPT-8 契约的行为证据：伤害流程 delegate 回调、按句柄访问器、selector/filter 转发、Effect 步骤执行器同步与挂起/唤醒、Damage Flow 步骤执行器和悬空句柄访问。验证装置 MUST 为每个场景输出可检索的通过/失败日志，并将静态实现、glue、PIE、GC 和未验证边界分开记录。

#### Scenario: 既有 C# 伤害流程回归

- **WHEN** 在 PIE 中运行验证装置的伤害流程回归场景
- **THEN** 脚本登记模板、C++ `Execute_*` 抵达 C# delegate、访问器往返和挂起链唤醒均有独立日志，结果与已归档基线一致

#### Scenario: 宿主 selector/filter 转发

- **WHEN** 脚本对象通过 `FTcsSelHostDelegate` 和 `FTcsFilterHostDelegate` 参与目标选择
- **THEN** 目标集顺序、过滤 AND/短路、空 selector Host 与空 filter Host 的结果和 Warning 语义均可由 PIE 结果确认

#### Scenario: 宿主 Effect/Flow 步骤执行器

- **WHEN** 脚本定义的自定义步骤数据经 `RegisterStepExecutor` 登记到 Effect 或 Damage Flow 门面并执行
- **THEN** Effect 执行器的同步完成、`TSR_Running` 后 `ResumeRun` 重入完成，以及 Flow 执行器的继续/中止结果均能从链/流程结果和日志观察到

### Requirement: 插槽对象 GC 保活证据

验证装置 MUST 能在清空探针侧强引用并执行 Unreal GC 后，再次触发已登记的 Effect executor、Flow executor 或流程 delegate；若当前宿主无法可靠触发或观测 Unreal GC，验证记录 MUST 明确标记为受限并转入台账，不得以 .NET GC 结果替代。

#### Scenario: 登记的 Effect executor 经 Unreal GC 仍可调用

- **WHEN** 执行器已登记、探针清空自身字段、PIE 控制台执行 Unreal `gc`，随后启动含该步骤的链
- **THEN** 脚本执行器仍被调用并产生预期结果，或记录明确的入口受限证据

#### Scenario: 登记的 Flow executor/delegate 经 Unreal GC 仍可调用

- **WHEN** 流程模板已登记、探针清空自身字段、PIE 控制台执行 Unreal `gc`，随后启动含该流程的链
- **THEN** Flow executor/delegate 仍被调用并产生预期结果，或记录明确的入口受限证据

### Requirement: 悬空句柄安全证据

验证装置 MUST 在链自然完成或代际失配后调用全部按句柄访问器，并记录读写结果和日志级别。

#### Scenario: 已完成句柄读口

- **WHEN** 使用已释放的 `FTcsChainRunHandle` 调用目标、施法者、发起者和变量读口
- **THEN** 返回空值/无效句柄，不触发 `ensure`，并保留必要的 Warning

#### Scenario: 已完成句柄写口

- **WHEN** 使用已释放的 `FTcsChainRunHandle` 调用目标集或变量写口
- **THEN** 返回 `false + Warning`，不写入任何新运行态，不触发 `ensure`

### Requirement: 跨语言证据边界登记

验证报告 MUST 按语言分别记录宿主存在性、绑定入口和行为证据。C# 通过不得推导 AS、Luau、Puerts/TS 或蓝图的端到端通过；没有可运行宿主时 MUST 保留为未验证边界。

#### Scenario: 仅有 C# 宿主

- **WHEN** 当前仓库只有 UnrealSharp/C# 可运行验证入口
- **THEN** C# 场景可单独收束，其他语言记录为未验证，不阻塞本提案的 C# 证据归档

#### Scenario: 其他语言存在可运行入口

- **WHEN** 某其他脚本语言具有真实 TCS 插槽绑定和可运行 PIE 夹具
- **THEN** 该语言必须使用独立日志和独立结果记录，不能复用 C# 日志作为证据
