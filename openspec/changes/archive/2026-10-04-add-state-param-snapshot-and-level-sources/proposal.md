# Change: 状态参数快照、等级源与 Duration-Period 到期堆（R5 Task 3）

## Why

`TcsState` 今天只有"身份骨架"：实例建得出来、查得到、删得掉，但**没有一个数值属于它**——`DurationRemaining` / `PeriodRemaining` 恒为 0（施加时刻意不编造），`FTcsStateInstance::ParamSnapshot` 按注释留白，`TcsEvent.State.Periodic` 只有声明、零广播，`ExtendDuration` / `SetRemaining` 只改字段、不碰到期堆。buff 之所以是 buff 的两条腿——**"施加瞬间冻结的数值"**与**"到期/周期两个时间语义"**——都还没长出来。

本轮把这两条腿接上：施加时把 `Def.Params` 逐行求值冻结成快照（施加方覆盖优先、`ValueConvention` 转规范值），时值/周期落进 M0 的到期堆并按代际校验回调，等级类参数源（State/Instigator × Array/Map 四型）获得真实的 `EffectiveLevel` 与宿主等级读口。

## What Changes

- **新增快照能力**：`FTcsParamSnapshot` / `FTcsParamSnapshotEntry`（已解析规范值 + 源引用位）+ 构建规则（`Overrides` 优先、`Def.Base` 兜底、`ValueConvention` 经 `FTcsValueConvention::ConvertToCanonical` **首次点亮**）+ 读取适配器 `FTcsStateParamTableReader`（把快照当参数表暴露给 `ParamRef` 与 Task 4 的修正器物化）。
- **`FTcsStateInstance` 补 `ParamSnapshot` 字段**（Task 2 留白的落地）+ **新增 `PeriodEntry` 字段**（周期条目独立锚点——`ExpiryEntry` 只有一个，时长与周期是两个语义）。
- **新的求值上下文供料面**：`ApplyState` 增加两个**可选**形参（`Instigator` 与 `ParamTable`）、新增派生上下文 `FTcsStateEvaluateContext`（在 Core 上下文之上补宿主等级读口 `LevelProvider`——域读口不进 Core）、`UTcsStateSubsystem` 增加 `SetEntityLevelProvider` / `GetEntityLevelProvider`——三处都是"**调用方填写、源只读上下文**"这同一条取值归属规则的落地。
- **四个等级源**：`FTcsParamSource_StateLevelArray` / `_StateLevelMap`（读 `Context.EffectiveLevel`）/ `_InstigatorLevelArray` / `_InstigatorLevelMap`（读 `Context.Instigator` → `ITcsEntityLevelProvider` → level），四型继承 `FTcsParamEnumerableSource`（Task 0 Step 2 落地的基类首次有实现方）。
- **Duration / Period 落堆**：`Finite` 入堆、`Infinite` 不入堆；`Period > 0` 走**重复到期条目**（每次回调广播 `TcsEvent.State.Periodic` 后重新入堆，默认等首个周期）；`PeriodRefresh`（Keep / Reset / Immediate）作用于刷新路径；`ExtendDuration` / `SetRemaining` 补"撤销旧条目 + 按新余量重入堆"；到期回调经 `UWorld` 弱引用 + 句柄代际校验落 `ExpireState`。
- **补齐 Task 2 留下的三处待办**：`Deinitialize` 撤销全部待撤销条目、生命周期操作同步到期堆 + `Infinite` 拒绝面实测、`ParamSnapshot` 字段落地。
- **撤销顺序扩展（硬约束的延伸）**：移除路径 MUST 先撤销该实例的到期/周期条目，再走 `Expiring → 广播 → 归还槽位`——条目持句柄、槽位复用后回调仍会到达，靠代际校验兜底是"能成立"，但**先撤条目**才是"不产生无谓回调"。

## Impact

- Affected specs：`state-param-snapshot`（新增能力）、`state-instance-lifecycle`（MODIFIED ×4：池化实例字段 / 门面周期与到期堆同步 / 无限时长拒绝面口径 / 世界反初始化清空）、`state-def-asset`（ADDED ×1：`ValueConvention` 白名单校验收口）
- Affected code（插件仓）：
  - 新增：`Source/TcsState/Public/State/TcsStateSnapshot.h`、`Source/TcsState/Public/State/TcsStateParamTableReader.h`（+ `.cpp`）、`Source/TcsState/Public/Host/TcsEntityLevelProvider.h`（含派生上下文 `FTcsStateEvaluateContext`）、`Source/TcsState/Public/Param/TcsParamSource_StateLevel.h`、`Source/TcsState/Public/Param/TcsParamSource_InstigatorLevel.h`、`Source/TcsState/Public/State/TcsStateDuration.h`、`Source/TcsState/Private/State/TcsStateOps_Lifetime.cpp`、`Source/TcsState/Private/State/TcsStateOps_Snapshot.cpp`
  - 修改：`State/TcsStateInstance.h`（补两字段）、`State/TcsStateOps.h` / `TcsStateOps.cpp`（撤销顺序 + 快照构建挂点）、`TcsStateSubsystem.h/.cpp`（等级读口 + 两个可选形参 + 两个时长操作 + `Deinitialize`）
- Affected code（宿主仓）：`Source/TcsDev/` 装置扩检查块 + 一个最小 `ITcsEntityLevelProvider` 实现（等级源真配置演示的宿主半边）
- **非目标**：参数链聚合（R6 `STAT-1`）；快照 Live 化（`Live` 模式本轮只在 `ETcsParamMode` 上留值）；修正器物化（Task 4——本轮的快照与读取适配器是它的输入口）；五轴堆叠与刷新政策（Task 5——`PeriodRefresh` 的 Keep/Reset/Immediate 本轮只作用于"同单位 + 同 `DefTag`"这一暂用判据）
