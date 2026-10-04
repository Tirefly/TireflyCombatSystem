# Change: 参数求值上下文补齐主体与等级，并落地可枚举值来源基类

## Why

R5（M3 状态层）的三个前置契约里有两条落在 `TcsCore` 的参数面上：等级源需要"被施加方实体 + 生效等级"才能解析档位，而"索引解析的唯一真相在源"需要一个可枚举能力基类承载。两件事在 `param-value` 规格里一处已记为**既定偏差**（上下文字段）、一处**只在设计文档里存在**（PV-10 基类，openspec 侧零记载）——即规格与代码之间存在需要一次性拉平的落差，而下游（R5 Task 3 等级源、`ModifyAttribute`）即将消费它们。

## What Changes

- `FTcsParamEvaluateContext` 追加两字段：`Subject`（`FTcsCombatEntityHandle`，被施加方实体）与 `EffectiveLevel`（`int32`，生效等级）——**兑现**规格里"随 TcsState 等级源同批补齐"的既定偏差；**取值归调用方**（源不自己去查实例）。
- 新增可选能力基类 `FTcsParamEnumerableSource : FTcsParamValueSource`（USTRUCT + `meta = (Hidden)`）：**仅一个**中性默认实现 `GetIndexForLevel()`，**索引解析的唯一真相在源**。
- `TcsParamValueSource.h` 的头部口径注释由"M2/M5 轮补齐"改写为"随 TcsState 等级源同批补齐（2026-10-04，M2 已收束，原文过期）"。
- **不含**来源发号器进程唯一（台账 `WAIT-6`）：那是"恢复 `instance-handle-pool` 既有规格行为"的缺陷修复，按 OpenSpec 决策树（Bug 修复 → 直接修）免提案，落在 `PLN-R5` Task 0 Step 3。

## Impact

- Affected specs: `param-value`（MODIFIED「反射可见求值上下文」；ADDED「可枚举值来源基类」）
- Affected code:
  - `Source/TcsCore/Public/Parameter/TcsParamValueSource.h`（改：两字段 + 头注释口径）
  - `Source/TcsCore/Public/Parameter/TcsParamEnumerableSource.h`（新）
- 能力边界：`FTcsParamEnumerableSource` 的 **PV-10 原案 `Enumerate(Level, OutValues, OutCurrentIndex)` 本轮不建**——展示/视图层未落地，零消费者不预建（台账 `STAT-2` 本轮按"部分消费"登记）。
- 回归面：零行为变更（纯新增字段/类型），既有宿主验收命令 `Tcs.Test.Slice.Run` 输出应逐字不变。
