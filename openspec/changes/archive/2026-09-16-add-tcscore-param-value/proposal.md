# Change: 补录 TcsCore 参数载体能力规格（param-value）并收窄 cpp-module-structure

## Why
能力归属错位补正（2026-09-16 用户质询）：参数值来源策略族（载体/基类/内置源/上下文/参数表接口）是**正交能力**，此前寄居在归档提案的 `cpp-module-structure`「Task 0 载荷类型与设施」requirement 名下——后续增量（Task 4 的 AttributeScaled 源、PV-10 可枚举源、PV-1 上下文补齐）将没有自然归属地。本提案把 TcsCore 侧载体体系立为独立能力 `param-value`，并从 `cpp-module-structure` 移除该 requirement。

## What Changes
- **ADDED** 能力规格 `param-value`：统一数值配置载体 `FTcsParamValue`（含 Evaluate 便利转发——PV-1 增补）、抽象基类 `FTcsParamValueSource` 与内置源 Literal/ParamRef（PV-2）、反射可见求值上下文 `FTcsParamEvaluateContext`（含 Subject/EffectiveLevel 随 TcsState 等级源批次补齐的既定偏差记录）、`ITcsParamTableReader` 参数表只读接口。
- **REMOVED** `cpp-module-structure` 的「Task 0 载荷类型与设施」requirement：
  - TcsCore 部分迁入本提案的 `param-value` 能力；
  - TcsNotation 部分迁入姊妹提案 `add-tcsnotation-value-convention`；
  - `UTcsDeveloperSettings`（空壳）不再保留规格——待其长出真实配置项时随对应能力规格化（2026-09-16 用户拍板口径）。
- **BREAKING**：无（规格重组，行为即现状）。

## Impact
- Affected specs: `param-value`（新建）、`cpp-module-structure`（移除一条 requirement，其余两条目录/依赖规格不动）。
- Affected code: 无——实现已在 `Source/TcsCore/Public/Parameter/`（提交 ec08c1b + Evaluate 转发）。
- 未来落点注记（不进本轮规格）：`FTcsParamSource_AttributeScaled` 随 plan1 Task 4 落 `Source/TcsAttribute/Public/Attribute/`（PV-3，快照求值一次冻结）；`FTcsParamEnumerableSource`（PV-10）随 TcsState 等级源同批进 TcsCore；上下文 `Subject`（`FCombatEntityHandle`，Core 边界让步记录见 01 §2.1）与 `EffectiveLevel` 同批次补齐。
