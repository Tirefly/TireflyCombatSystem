# State 运行时 Bootstrap 约束

## 目的

本文记录 `UTcsStateComponent` 在 runtime bootstrap 流程中的当前硬约束，避免后续把“调用过初始化函数”误判为“State runtime 已可用”。

前置约束：一个 Entity 是否进入这套 bootstrap 流程，首先取决于开发者是否显式调用 `RegisterEntity()`。组件自身的注册 / 预热 / 报到，不等于该 Entity 已被激活为 TCS runtime entity。

如果调用方只需要判断某个 Entity 是否已被显式激活为 TCS runtime entity，应优先使用轻量注册态查询；只有在需要进一步诊断为什么尚未 ready 时，才进入完整的 `EvaluateEntityRuntimeState()` 结果。

## Runtime-ready 定义

`StateComponent` 对外的 runtime-ready 不是单一布尔缓存，而是以下条件的组合结果：

1. `PrepareStateRuntime()` 已成功完成。
2. 当前顶层 `StateTree` 已成功反推出该组件的 runtime slot 集合，并据此构建运行时数据。
3. 该 runtime slot 集合的 StateSlotMapping 已成功绑定到当前 `StateTree` 中真实存在的状态名。
4. `StartStateRuntime()` 已启动 `StateTree`。
5. `IsRunning()` 确认 `StateTree` 正在运行。

只有这些条件全部满足，`IsRuntimeReady()` 才允许返回 true。

## StateTree 强约束

当前设计不支持无 `StateTree` 的 `StateComponent` 进入 runtime-ready。单独存在的 `RuntimeStateSlots` 不具备当前设计意义，因为 Buff / Skill 依赖的是已经启动的外层 `StateTree` 与 StateSlotMapping 共同构成的 State runtime 环境。

因此：

- 缺少有效 `StateTree` 时，`PrepareStateRuntime()` 必须失败。
- `StateTree` 引用无效时，`PrepareStateRuntime()` 必须失败。
- 当前顶层 `StateTree` 若无法反推出任何有效 StateSlot 时，`PrepareStateRuntime()` 必须失败。
- 任一被当前 `StateTree` 命中的 `StateSlotDefinition` 无效时，`PrepareStateRuntime()` 必须失败。
- 任一被当前 `StateTree` 命中的 `StateSlotDefinition` 缺少 `StateTreeStateName`，或后续映射校验失败时，`PrepareStateRuntime()` 必须失败。

## StateSlot 子集配置

全局 `StateSlotDefinition` 是可用槽位全集，不是每个 Actor 的必需槽位集合。不同 Actor 类型可以拥有不同 StateSlot 子集。

当前实现不再由组件手工声明槽位子集，而是由当前顶层 `StateTree` 自动反推：

- 先读取当前组件绑定 `StateTree` 中真实存在的状态名。
- 遍历全局 `StateSlotDefinition`，只保留 `StateTreeStateName` 命中当前 `StateTree` 的定义。
- 只为这些被当前 `StateTree` 命中的槽位构建 `RuntimeStateSlots`。
- 未出现在当前顶层 `StateTree` 中的全局 StateSlotDefinition，不属于该组件的 runtime slot 集合，也不参与该组件 ready 判定。
- 如果当前 `StateTree` 无法反推出任何槽位，则视为配置/设计错误，不回退到全局全部槽位。

这意味着当前设计下，顶层 `StateTree` 既是 Gate 驱动器，也是当前 `StateComponent` runtime slot 集合的唯一声明源。

## Buff / Skill 放行条件

Bootstrap 只能在 `StateComponent->IsRuntimeReady()` 返回 true 后推进：

- `UTcsBuffComponent::PrepareBuffRuntime()`
- `UTcsSkillComponent::PrepareSkillRuntime()`

如果 StateTree 缺失、StateSlotMapping 失败，或 `StartStateRuntime()` 后 `IsRunning()` 为 false，`StateComponent` 必须保持 not ready，Buff / Skill 也必须保持 not ready。

## 继承启动路径封堵

`UTcsStateComponent` 继承自 `UStateTreeComponent`，但不能允许继承的自动启动路径绕过 TCS runtime bootstrap。因此当前实现要求：

- 构造、`InitializeComponent()` 与 `BeginPlay()` 阶段都会强制关闭 `bStartLogicAutomatically`。
- `TickComponent()` 在调用 `Super::TickComponent()` 之前先检查 `IsRuntimeReady()`，防止底层 StateTree 在 not ready 状态下 tick。
- 直接调用继承的 `StartLogic()` 会被拦截并输出日志。
- 直接调用继承的 `StopLogic()` 会被重定向到 `StopStateRuntime()`，以同步 `bStateRuntimeActive` 与 bootstrap ready 诊断；`StopStateRuntime()` 内部授权后才会调用底层 `Super::StopLogic()`。
- `RestartLogic()` 保留 BrainComponent 的重启语义，但必须先通过 TCS 前置条件检查，再走 TCS runtime restart 路径。
- `PauseLogic()` / `ResumeLogic()` 保留暂停 / 继续语义；它们不会推进 prepare 或首次 start，只在 ready / running 条件满足时转发到底层 StateTree。
- 只有 `StartStateRuntime()` 内部授权的启动流程可以调用底层 `Super::StartLogic()`。

这保证了外层 `StateTree` 的真实启动只能通过 bootstrap 控制的 runtime start 流程发生。

## 配置影响

`UTcsStateSlotDefinition::StateTreeStateName` 不再是可选项。编辑器数据验证会把空 `StateTreeStateName` 视为无效配置。对应的 `FTcsStateSlotDefRow::StateTreeStateName` 也应保持非空并与目标 `StateTree` 中实际状态名一致。

新增或修改顶层 `StateTree` 时，应同步确认：当前树中需要作为 runtime slot 的状态，都能通过 `StateSlotDefinition.StateTreeStateName` 命中到有效槽位定义；未命中的状态不会自动生成 runtime slot。
