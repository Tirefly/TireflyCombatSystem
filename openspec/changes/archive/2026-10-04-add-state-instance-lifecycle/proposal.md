# Change: 状态实例、per-unit 注册表、施加门面与生命周期事件

## Why

R5 Task 1 让"状态定义"成为可发现、可校验、可解析的内容资产，但**定义还不能变成运行实例**：全库没有任何 `FStateInstance` 的宿主、没有按单位归置的状态桶、没有施加/移除入口，也没有一条状态生命周期事件——R4 打通的"事件 → 触发行 → 效果链"因此仍**没有状态侧的事件可订阅**。
本变更把 M3 的运行态骨架立起来：实例能被创建、查到、移除，全生命周期按 D3-7 广播到总线，后续 Task（参数快照 / 修正器物化 / 五轴堆叠 / `ApplyState` 链原语）在这套骨架上续写。

## What Changes

- **NEW** 状态实例句柄 `FTcsStateHandle`（`USTRUCT(BlueprintType)`，`Index` + `Generation`；`Index < 0` 或 `Generation <= 0` 判无效）——句柄**相对本门面**有义，`Index` / `Generation` 的语义由**桶**定义（与链运行句柄 / 触发实例句柄同款：展平 `int32` 两字段，供反射与宿主脚本往返，不外传对象）。
- **NEW** 池化实例记录 `FTcsStateInstance`（**纯数据**）：`DefTag` / `Handle` / `Source` / `Instigator` / `Stacks` / `Level` / `Phase` / 剩余时值 / 周期计时 / 到期条目锚点。**零策略、零载荷、零订阅句柄、零 `UObject` 引用**（D3-7 v2 纪律）。
- **NEW** per-unit 桶注册表 `FTcsStateRegistry`：`FCombatEntityHandle → 桶`（`TUniquePtr` 间接层，`TMap` 扩容不搬移桶地址），桶内 = 槽位数组 + 代际数组 + 空闲槽栈（照 `FTcsTriggerRegistry` 手法）；脏句柄（代际失配 / 下标越界）一律**拒绝 + `Warning`**，不 `ensure`。
- **NEW** 施加门面 `UTcsStateSubsystem : UWorldSubsystem`（仅 Game / PIE / GamePreview；`Deinitialize` 全量清理）：`ApplyState` / `GetState` / `ForEachState` / `RemoveState` / `ExpireState` / `UnregisterUnit` / `ExtendDuration` / `SetRemaining`，以及定义登记口 `RegisterStateDef` / `UnregisterStateDef` / `GetRegisteredStateDef`。
- **NEW** 状态定义到运行态的**单向注入路径**：定义库（`UTcsDefinitionSubsystem`，GameInstance 级、在 `TcsIntegration`）在装配世界时把缓存的状态定义登记进该世界的状态门面。**TcsState MUST NOT 反向依赖 `TcsIntegration`**（依赖是单向的：定义库写进门的登记口，不是门去反查定义库；同款先例 = 定义库把触发行写进 `UTcsEffectSubsystem::RegisterTriggerRow`）。本条**不为状态定义单独发号**——状态实例的来源句柄由状态门面在每次施加时经统一发号器发放。
- **NEW** 生命周期事件六枚原生 tag（`TcsEvent.State.Applied` / `Refreshed` / `StackChanged` / `Expired` / `Removed` / `Periodic`）+ 反射载荷 `FTcsStateEventPayload`；`ApplyState` 的共存决策结果**恰广播其中一枚**（`Applied` / `Refreshed` / `StackChanged`），`ExpireState` 广播 `Expired`，`RemoveState` 广播 `Removed` 并携带原因。
- **NEW** 阶段机 `EStatePhase{ Inactive / Active / Expiring }` 与迁移校验：非法迁移 = `ensure`（配置错误语义，与脏句柄的竞态口径分开）。
- **MODIFIED** `integration-entity`：状态定义由"**只进缓存、不被装配到世界**"改为"**缓存 + 逐世界登记进状态门面**"——Task 1 那一步只做到"进缓存"是因为当时还没有消费方；今天消费方（`UTcsStateSubsystem`）已存在，定义的运行期副本按同族既有形态（链 / 触发行都在世界侧有逐世界副本）落位。
- **NOT BREAKING**：无既有代码引用这些新类型；插件模块清单不变（仍八个）；无 tag 词表新增根（六枚事件词落在既有 `TcsEvent` 根下）。

## Impact

- Affected specs: `state-instance-lifecycle`（**ADDED**，新能力）、`integration-entity`（**MODIFIED**）
- Affected code:
  - 新增：`Source/TcsState/Public/State/{TcsStateHandle.h, TcsStateInstance.h, TcsStateRegistry.h, TcsStateOps.h, TcsStateEvents.h}`、`Source/TcsState/Private/State/{TcsStateRegistry.cpp, TcsStateOps.cpp, TcsStateOps_Query.cpp, TcsStateOps_Events.cpp}`、`Source/TcsState/Public/{TcsStateSubsystem.h}`、`Source/TcsState/Private/{TcsStateSubsystem.cpp}`
  - 修改：`Source/TcsState/Public/State/TcsStateEnums.h`（补三枚举）、`Source/TcsIntegration/{Public/TcsDefinitionSubsystem.h, Private/TcsDefinitionSubsystem.cpp, Private/TcsDefinitionSubsystem_State.cpp}`（逐世界登记状态定义）
  - 宿主装置（**LAC 仓**，同批交付）：`Source/TcsDev/.../TcsDevSliceRig.cpp` 扩"施加 / 订阅 / 移除"检查块
- **不在本变更内**（各自提案）：参数快照与等级源（Task 3）、修正器物化（Task 4）、五轴共存决策（Task 5）、`ApplyState` 链原语与行为 Fragment（Task 6）
