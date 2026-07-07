## 背景

当前 `UTcsAttributeComponent`、`UTcsStateComponent`、`UTcsBuffComponent`、`UTcsSkillComponent` 的业务可用性仍部分依赖 UE 原生生命周期与局部补救逻辑，例如：

- `BeginPlay` 预热
- `OnRegister` 绑定
- API 内部临时查找组件
- `BuffComponent` 的懒解析补绑

这些做法在预挂组件 Spawn，以及 `StateComponent` 与 `StateTree` 启动时机对齐等场景下缺少统一保证。

## 目标

- 将 **UE 生命周期** 与 **TCS runtime-ready** 分层
- 明确四个组件的依赖顺序与统一编排流程
- 让预挂组件走统一的 runtime-ready 规则
- 将 `StateComponent` 现有依赖 `BeginPlay` 的初始化剥离到显式 prepare 流程
- 将“有效 StateTree + 有效 StateSlotMapping + StateTree running”纳入 `StateComponent` 对外 runtime-ready 的硬前置条件，确保 `BuffComponent` / `SkillComponent` 不会在 State 运行环境未真正可用时启动
- 支持不同 Actor 类型通过绑定不同顶层 `StateTree` 自动形成不同 StateSlot 子集，避免把全局 StateSlotDefinition 全部强加给每个 `StateComponent`

## 非目标

- 不改变 `Attribute / State / Buff / Skill` 的业务边界
- 不新增初始化专用 ActorComponent
- 不新增初始化专用接口，继续复用 `ITcsEntityInterface`
- 不引入四个组件的统一基类
- 不支持无 `StateTree` 的 `StateComponent` 进入 runtime-ready；没有 `StateTree` 时，单独构建 StateSlot 运行时容器不具备当前设计意义
- 不在本提案中引入完整 StateSlotProfile 资产，也不再引入组件侧手工声明的槽位子集数组；当前以顶层 `StateTree` 作为 runtime slot 集合的唯一事实源
- 暂不覆盖运行时动态创建且关键组件已齐备的 Actor
- 暂不覆盖 `RegisterEntity` 之后再动态补齐关键组件
- 暂不覆盖运行中移除关键组件后的状态回退

## 核心设计

### 1. `UTcsRuntimeBootstrapSubsystem`

采用 `UGameInstanceSubsystem` 级 bootstrap，由它：

- 通过 `InitializeDependency` 依赖：
  - `UTcsAttributeManagerSubsystem`
  - `UTcsStateManagerSubsystem`
- 接收 `RegisterEntity`
- 在组件报到 / 注销 / prepare 完成时执行重评估
- 按依赖顺序推进 ready

它**不**负责：

- 直接 `AddComponent`
- 承接 Actor 本地业务逻辑
- 长期缓存所有 ready Entity 的状态

当前方案 A 下，它只维护：

- 已被开发者显式纳入 bootstrap 的 entity 注册集合
- waiting 中的 pending registration
- 对应的 one-shot ready 回调

这里的“显式注册集合”只表达 **该 Entity 是否被上层激活 TCS 能力**，不表达 ready 事实，也不替代即时 runtime 检测。

### 2. `RegisterEntity`

`RegisterEntity` 是统一入口，不再把“第一个组件报到”当作唯一建档时机。

确认后的强约束：

- 一个 Entity 是否激活 TCS 能力、以及何时激活，完全由开发者显式调用 `RegisterEntity` 决定
- 组件向 bootstrap 的报到只表示“该组件可被观察 / 可参与重评估”，不等于“该 Entity 已纳入 bootstrap”
- 对实现了 `ITcsEntityInterface` 但尚未显式注册的 Actor，`EvaluateEntityRuntimeState` 必须返回 `Waiting + NotRegistered` 或等价诊断

推荐形态：

```cpp
UFUNCTION(BlueprintCallable, Category = "TCS|RuntimeBootstrap")
ETcsRegisterEntityResult RegisterEntity(
	AActor* Entity,
	FTcsOnEntityReadyDynamicDelegate OnReady);
```

约束：

- 参数使用 `AActor*`，函数内部检查 `ITcsEntityInterface`
- `OnReady` 可不绑定
- 若当前已 ready，返回 `ReadyNow`，不触发回调，不建立 pending 记录
- 若当前未 ready，返回 `RegisteredWaiting`，保存 one-shot 回调
- 若该实体已在 waiting 中，返回 `AlreadyWaiting`

推荐返回值：

```cpp
UENUM(BlueprintType)
enum class ETcsRegisterEntityResult : uint8
{
	ReadyNow,
	RegisteredWaiting,
	AlreadyWaiting,
	InvalidEntity,
};
```

### 3. `EvaluateEntityRuntimeState`

诊断入口不建议只返回 `bool`，应直接返回状态与阻塞原因。

推荐形态：

```cpp
UFUNCTION(BlueprintCallable, Category = "TCS|RuntimeBootstrap")
FTcsEntityRuntimeStateResult EvaluateEntityRuntimeState(AActor* Entity);
```

推荐结构：

```cpp
USTRUCT(BlueprintType)
struct FTcsEntityRuntimeStateResult
{
	ETcsEntityRuntimeState State;
	ETcsEntityRuntimeBlockReason BlockReason;
};
```

状态枚举保持轻量：

```cpp
UENUM(BlueprintType)
enum class ETcsEntityRuntimeState : uint8
{
	Ready,
	Waiting,
	Invalid,
};
```

阻塞原因承载细节：

```cpp
UENUM(BlueprintType)
enum class ETcsEntityRuntimeBlockReason : uint8
{
	None,
	NotRegistered,
	InvalidEntity,
	MissingEntityInterface,
	AttributeManagerNotReady,
	StateManagerNotReady,
	MissingAttributeComponent,
	MissingStateComponent,
	AttributeComponentNotPrepared,
	StateComponentNotPrepared,
	StateRuntimeNotStarted,
	BuffWaitingForState,
	SkillWaitingForState,
};
```

组合约束：

- `Ready -> None`
- `Waiting -> BlockReason != None`
- `Invalid -> InvalidEntity / MissingEntityInterface` 一类原因

额外约束：

- 一个实现了 `ITcsEntityInterface` 的合法 Actor，如果尚未调用 `RegisterEntity`，不得进入后续 manager / component waiting 原因分支；必须优先返回 `NotRegistered`

如果调用方只需要知道“该 Entity 是否已被开发者激活 TCS”，而不需要完整 ready 诊断，则 bootstrap subsystem 可以额外提供一个轻量查询（例如 `IsEntityRegisteredForRuntimeBootstrap`）。这个查询只回答显式注册态，不替代 `EvaluateEntityRuntimeState`。

### 4. 组件协作相位

四个组件不需要统一基类，但需要统一流程约定：

1. `InitializeComponent()`：向 bootstrap 报到
2. 显式 `prepare`：完成本地准备后通知 bootstrap
3. 进入 ready：在 bootstrap 重评估后放行
4. `UninitializeComponent()`：向 bootstrap 注销

关键点：

- `InitializeComponent()` 只表示“组件已向 bootstrap 报到并可被观察”，不等于实体已激活 TCS
- prepare 完成必须显式回报，否则 bootstrap 无法安全判定

### 5. 依赖顺序

确认后的依赖图如下：

1. `AttributeManagerSubsystem`
2. `StateManagerSubsystem`
3. `AttributeComponent` 与 `StateComponent`（并行基础层）
4. `BuffComponent` 与 `SkillComponent`（后置层）

补充约束：

- `StateComponent` 等待 `AttributeManagerSubsystem` 与 `StateManagerSubsystem`
- `StateComponent` **不等待** `AttributeComponent`
- `BuffComponent` / `SkillComponent` 都等待 `StateComponent` ready

`StateComponent` 的 ready 不只是“调用过 prepare”。在当前设计下，它必须同时满足：

- 存在有效 `StateTree`
- StateSlot 运行时数据构建成功
- StateSlotMapping 成功绑定到实际 `StateTree` 状态
- `StartStateRuntime()` 已启动 `StateTree`，且运行状态可由 `IsRunning()` 或等价判据确认

因此，bootstrap 只有在 `StateComponent->IsRuntimeReady()` 已经表达上述全部条件后，才能推进 `BuffComponent` 与 `SkillComponent`。

### 6. `StateComponent` 特殊处理

`StateComponent` 是唯一需要额外强调的组件。

要求：

- 现有位于 `BeginPlay` 的关键初始化逻辑迁移到显式 prepare 阶段
- `BeginPlay` 不再承载核心初始化职责
- 真正进入运行态由独立入口控制
- `StateComponent` 必须拥有有效 `StateTree` 才允许 prepare 成功
- StateSlot 运行时数据与 StateSlotMapping 必须在 prepare 阶段完成验证；如果 `StateTree` 缺失、槽位数据无法构建，或映射无法绑定到实际 `StateTree` 状态，prepare 必须失败并保持 not ready

推荐入口：

- `StartStateRuntime()`
- `StopStateRuntime()`

`StartStateRuntime()` 只负责：

- 校验前置条件
- 切到 runtime active
- 启动 `StateTree` 或等价逻辑

它不负责补做 prepare，也不负责搜索其他组件。

`StartStateRuntime()` 的成功结果必须以 `StateTree` 真实 running 为准。若 `StartLogic()` 后 `IsRunning()` 仍为 false，则 `StateComponent` 仍然不是 runtime-ready，bootstrap 不得继续推进 `BuffComponent` / `SkillComponent`。

### 6.1 StateSlotMapping 强约束

当前设计不允许“无 StateTree 但 StateSlot 运行时容器存在”的 ready 模式。StateSlotMapping 的意义是把 TCS StateSlot 与 `StateTree` 中真实状态建立绑定；如果没有有效 `StateTree`，或者当前组件声明的必需 StateSlot 无法绑定到目标状态，就不能声称 State runtime 已准备好。

全局 StateSlotDefinition 是“可用槽位全集”，不是每个 Actor 的必需槽位集合。不同 Actor 类型可以只使用其中一个子集，但这个子集不再由组件手工声明，而是由当前顶层 `StateTree` 自动反推：

- `StateComponent` 先读取当前绑定 `StateTree` 中真实存在的状态名集合
- 遍历全部 `StateSlotDefinition`，只保留 `StateTreeStateName` 命中当前 `StateTree` 的定义
- 这些命中的定义共同构成当前组件的 runtime slot 集合
- 未出现在当前顶层 `StateTree` 中的槽位，不属于该 `StateComponent` 的 runtime slot 集合
- 若没有任何 `StateSlotDefinition` 能命中当前 `StateTree`，prepare 失败，避免静默回退到全局全部槽位
- 每个命中的 `StateSlotDefinition` 仍必须是有效定义，并能完整参与后续 StateSlotMapping 验证

这意味着当前设计正式放弃“某个槽位可以独立于顶层 `StateTree` 存在，只是不参与 Gate 联动”的宽松语义；在本提案范围内，顶层 `StateTree` 既是 Gate 驱动器，也是当前 `StateComponent` runtime slot 集合的唯一声明源。

实现阶段应把当前 `void` 型的 StateSlotMapping 初始化流程改造成可被 `PrepareStateRuntime()` 判定成功 / 失败的流程。推荐方向：

- `InitStateSlotMappings()` 或其内部步骤返回 `bool`
- 缺少 `StateTree` 直接返回失败
- 根据当前 `StateTree` 自动反推出的 `RuntimeStateSlots` 为空，或命中定义无有效 SlotTag 时返回失败
- 需要绑定到 `StateTree` 的 StateSlot 如果无法匹配实际状态名，则返回失败
- 失败时 `bRuntimePrepared` 必须保持 false，并输出诊断日志

是否新增更细的 bootstrap 阻塞原因可以在实现阶段决定；但无论外部诊断枚举是否细分，`StateComponentNotReady` 都必须覆盖这些失败场景，且不得放行 Buff / Skill。

### 6.2 StateTree 继承运行态入口

`UTcsStateComponent` 继承自 `UStateTreeComponent` / `UBrainComponent`，但不能允许继承入口绕过 TCS runtime-ready 屏障。

入口语义如下：

- `StartLogic()`：表示从未运行到运行，必须由 `StartStateRuntime()` 统一控制；外部直接调用应被拒绝并输出诊断
- `StopLogic()`：表示停止当前运行逻辑；允许外部或 UE lifecycle 触发，但必须重定向到 `StopStateRuntime()` 或等价的 TCS runtime stop 路径，确保 `bStateRuntimeActive` 与 bootstrap ready 诊断同步更新
- `RestartLogic()`：表示重启当前或曾经运行过的逻辑，不是“继续”；允许外部调用，但必须先通过 TCS 前置条件检查，然后走 `StopStateRuntime()` + `StartStateRuntime()` 或等价的 TCS runtime restart 路径
- `PauseLogic()`：表示暂停运行中的 StateTree，不改变 runtime prepared / mapping ready 状态；仅在 runtime-ready 且 running 时转发给底层 StateTree
- `ResumeLogic()`：表示从 paused 状态继续运行；仅在 runtime-ready 的基础状态仍成立、StateTree 仍处于 running/paused 语义下时转发给底层 StateTree

`PauseLogic()` / `ResumeLogic()` 不应被误用为 bootstrap 初始化入口，也不应在 not ready 状态下隐式推进 prepare 或 start。

### 7. 未 ready 保护策略

统一规则：未 ready 不执行业务。

按入口类型收敛为：

| 类型 | 未 ready 时行为 |
|---|---|
| 命令型 API | 拒绝执行，返回失败 / `NotReady`，输出日志 |
| 查询型 API | 返回安全结果或 `NotReady`，不能伪装成业务事实 |
| Tick / 内部 step | no-op |

## 运行时流程摘要

1. Spawn Actor
2. 调用 `RegisterEntity`
3. bootstrap 扫描已有组件并做首次评估
4. 组件 `InitializeComponent()` 报到
5. 组件完成 prepare 后回报
6. bootstrap 按依赖顺序推进：
   - `AttributeComponent`
	- `StateComponent.PrepareStateRuntime()`：验证 `StateTree`、StateSlot 运行时数据与 StateSlotMapping
	- `StateComponent.StartStateRuntime()`：启动 `StateTree` 并确认 running
   - `Buff / Skill`
7. 若此前返回 `RegisteredWaiting`，ready 后触发 one-shot 回调并删除 pending 记录

## 当前范围边界

当前提案只覆盖：

- 预挂组件 Spawn
- 统一 ready 屏障、显式 prepare、以及 `StateComponent` 的 runtime start / stop

当前提案暂不覆盖：

- 运行时动态创建且关键组件已齐备的 Actor
- `RegisterEntity` 之后再动态补齐关键组件（原情景 B）
- 运行中移除关键组件后的下游状态回退

除此之外，这份设计已经足够进入实现规划，不应继续膨胀。
