## MODIFIED Requirements

### Requirement: 世界级门面子系统

`UTcsEventBusSubsystem`（UTickableWorldSubsystem）MUST 仅在 Game / PIE / GamePreview 世界实例化（无跨 World 静态状态，PIE 安全）；帧末队列冲洗由时钟泵驱动（`UTcsClockSubsystem` 固定泵序第三步，时机 = `FWorldDelegates::OnWorldTickStart` 帧界泵点——早于全部 Actor tick 组；本子系统自 tick 停用：`SetTickableTickType(ETickableTickType::Never)`，不再于 `UWorld::Tick` 尾部 `TickObjects` 冲洗）；帧末通道可观测时机 = 发布帧的下一帧泵点（该帧游戏逻辑前）。`Deinitialize` MUST 确定性清空订阅与队列（保留动态多播观察钩子）。

#### Scenario: 非游戏世界不创建

- **WHEN** 在编辑器预览/检查器世界解析该子系统
- **THEN** 子系统不存在

#### Scenario: 帧末事件下一帧泵点到达

- **WHEN** 帧 N 游戏逻辑期间 `PublishFrameEnd`
- **THEN** 帧 N+1 泵点（该帧游戏逻辑前）派发；帧 N 末尾不派发
