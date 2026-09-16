# event-bus Specification

## Purpose
TBD - created by archiving change add-tcscore-event-bus. Update Purpose after archive.
## Requirements
### Requirement: 共享事件处理器

TcsCore MUST 提供 `UTcsEventHandler`（Abstract、Blueprintable）：处理入口 `HandleEvent(FGameplayTag EventTag, const FInstancedStruct& Payload)` 为 BlueprintNativeEvent（C++ 派生类覆写 `_Implementation`，蓝图子类可覆写同签名）；事件 struct 保持纯数据——struct 上不自绑 delegate（裁决 2a），行为集中在共享 Handler（无实例状态，通常传 CDO）；EventTag 随载荷一并传入（2026-09-11 用户拍板），使同一 Handler 可订阅多个 Tag 而不失真。

#### Scenario: 派发携带事件 Tag

- **WHEN** 某订阅命中并派发
- **THEN** Handler 收到发布时的 EventTag 与载荷（与 A' 动态多播参数形状对齐）

### Requirement: 双通道发布与订阅配对清理

`FTcsEventBus` 内核 MUST 提供 `Subscribe(Tag, Handler, Dispatch)` / `Unsubscribe(Handle)`（TMultiMap Tag 路由 + 订阅记录池化，句柄配对清理——悬空/无效句柄退订 ensure 并忽略），以及双通道发布：

- `PublishImmediate(Tag, Payload)`：发布当场同步派发（载荷 const 引用透传，零复制）；
- `PublishFrameEnd(Tag, Payload)`：入队（持有载荷副本），`FlushFrameEnd` **换出队列后**按入队序派发——冲洗期间新入队的事件归下一拍（防自激死循环）。

派发 MUST 先快照同 Tag 订阅句柄再逐目标代际校验（派发中的订阅/退订不影响本轮遍历）；Handler 以弱引用持有（不阻止 GC），失效订阅在派发时惰性摘除。

#### Scenario: 立即通道同步到达且通道隔离

- **WHEN** 同 Tag 各建立即/帧末两笔订阅后 `PublishImmediate`
- **THEN** 仅立即通道订阅者在调用栈内同步收到

#### Scenario: 帧末通道按入队序冲洗

- **WHEN** 依次 `PublishFrameEnd` 两笔后 `FlushFrameEnd`
- **THEN** 帧末订阅者按入队序收到两笔；冲洗期间新入队的第三笔留在队列归下一拍

#### Scenario: 退订后不再到达

- **WHEN** `Unsubscribe` 后发布
- **THEN** 该订阅不再收到事件，且退订为配对清理（路由条目与池槽位同步移除）

### Requirement: 世界级门面子系统

`UTcsEventBusSubsystem`（UTickableWorldSubsystem）MUST 仅在 Game / PIE / GamePreview 世界实例化（无跨 World 静态状态，PIE 安全）；帧末队列随子系统 Tick 冲洗（时机 = `UWorld::Tick` 尾部 `TickObjects`，晚于全部 Actor tick 组；时钟泵接管后须停用自 tick——plan1 Task 3 既定动作）；`Deinitialize` MUST 确定性清空订阅与队列（保留动态多播观察钩子）。

#### Scenario: 非游戏世界不创建

- **WHEN** 在编辑器预览/检查器世界解析该子系统
- **THEN** 子系统不存在

### Requirement: BP/CS 动态监听面

TcsCore MUST 提供 A' 反射面（Lyra GMS 形态）：动态多播 `FTcsOnCombatEvent(FGameplayTag EventTag, FInstancedStruct Payload)`（BlueprintAssignable，全量事件流——每笔派发均触发，绑定者按 Tag 自行过滤），以及 `UTcsAsyncAction_ListenForCombatEvent` 监听节点（"绑定即过滤"）：绑定门面多播后在回调内按 **Tag 匹配（精确/部分——层级含）+ PayloadType 结构类型匹配（空 = 不限；非空要求载荷结构为其自身或子类）**过滤，命中才广播节点自身的 `OnEvent` 子集流；Activate 绑定（重复激活先解绑再绑定）、BeginDestroy 退订。

#### Scenario: 监听节点输出子集流

- **WHEN** 以精确 Tag F 过滤激活节点，随后发布同 Tag 与异 Tag 事件各一笔
- **THEN** 节点自身 `OnEvent` 仅广播同 Tag 一笔（全量多播绑定者两笔都收）

#### Scenario: 部分匹配按 Tag 层级命中

- **WHEN** 以部分匹配（EMT_Partial）过滤父 Tag，发布子 Tag 事件
- **THEN** 节点命中并广播（精确匹配则不命中）

