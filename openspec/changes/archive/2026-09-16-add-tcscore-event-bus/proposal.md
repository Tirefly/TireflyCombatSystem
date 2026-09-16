# Change: 补录 TcsCore 事件总线能力规格（event-bus）

## Why
plan1 Task 2 的事件总线实现（2026-09-11 落地、编译通过、用户 PIE 检查点验证）当时未走 OpenSpec 提案流程；按用户要求补录——本提案描述**已构建**的既有行为，使 `openspec/specs/` 与仓库实现同步。无任何代码改动。

## What Changes
- 新增能力规格 `event-bus`，覆盖四组既有设施（裁决 2a + D0-3 + M9 收尾轮 A' 反射面）：
  - `UTcsEventHandler` 共享事件处理器（BlueprintNativeEvent `HandleEvent(EventTag, Payload)`，事件 struct 纯数据不自绑 delegate）；
  - `FTcsEventBus` 内核：TMultiMap Tag 路由 + 池化订阅配对清理 + 双通道发布（立即同步 / 帧末入队冲洗）+ 快照派发与惰性摘除；
  - `UTcsEventBusSubsystem` 世界级门面（仅游戏世界实例化；帧末随子系统 Tick 冲洗——Task 3 时钟泵接管后停用自 tick）；
  - BP/CS 动态监听面：`FTcsOnCombatEvent` 全量多播 + `UTcsAsyncAction_ListenForCombatEvent` 绑定即过滤节点。
- **BREAKING**：无（纯规格补录，行为即现状）。

## Impact
- Affected specs: `event-bus`（新建能力）。
- Affected code: 无——实现已在 `Source/TcsCore/Public/EventBus/` + `Private/EventBus/`（提交 ec08c1b）。验证载体（`Private/Testing/` 临时装置）按用户决定不入库、不进规格。
