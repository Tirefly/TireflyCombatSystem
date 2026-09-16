# Change: 补录 TcsNotation 值约期能力规格（value-convention）

## Why
能力归属错位补正（2026-09-16 用户质询）的 TcsNotation 侧：`ETcsValueConventionFlag` + `ConvertToCanonical`（D5-18，2026-09-11 落地、编译通过）此前寄居在归档提案的 `cpp-module-structure`「Task 0 载体类型与设施」requirement 名下——它是策划记法底座的**正交能力**，且后续增量（D5-18 v3 消费面：Task 4 `UTcsAttrModDef` 约定列校验、未来 `DecomposeFromCanonical` 反变换）需要自然归属地。本提案立独立能力 `value-convention`。

## What Changes
- **ADDED** 能力规格 `value-convention`：值约定标志位（VCF_ EnumFlags，固定组合顺序 Percent→OneMinus→Negate）与规范值转换助手 `ConvertToCanonical`（静态无状态，写入点调用；消费面恒规范值）。
- **BREAKING**：无（规格补录，行为即现状）。

## Impact
- Affected specs: `value-convention`（新建）。与姊妹提案 `add-tcscore-param-value` 对 `cpp-module-structure` 的 REMOVED delta 配合：两者归档后该 requirement 整体移除、内容分拆承接。
- Affected code: 无——实现已在 `Source/TcsNotation/Public/FTcsValueConvention.h`（提交 ec08c1b）。
- 未来落点注记（不进本轮规格）：`DecomposeFromCanonical`（UI 反变换，ConvertToCanonical 的逆序 Negate→OneMinus→×100）属 D5-17 v3 描述视图轮（TcsNotation 功能面随内容模块轮落地）；Task 4 的 `UTcsAttrModDef` 约定白名单校验（D5-18 v3：Literal/表型源可配，ParamRef/AttributeScaled 禁配）落 TcsAttribute 能力规格。
