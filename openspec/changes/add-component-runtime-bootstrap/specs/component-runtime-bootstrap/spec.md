## ADDED Requirements

### Requirement: TCS runtime-ready 生命周期独立于 UE 组件注册生命周期
TCS SHALL 将 `Attribute`、`State`、`Buff`、`Skill` 四个运行时组件的业务可用性建模为独立的 runtime-ready 生命周期，而不是把 `OnRegister`、`BeginPlay` 或组件已存在本身视为业务 ready 信号。

#### Scenario: 组件已注册但尚未 ready 时不得执行业务逻辑
- **WHEN** 一个 TCS 组件已经完成 UE 注册，但其前置 subsystem 或前置组件尚未满足
- **THEN** 该组件 MUST 处于非 ready 状态
- **AND** 不得执行业务 Tick、业务事件绑定或真正的 public runtime API 主逻辑

#### Scenario: runtime-ready 与 UE 生命周期解耦
- **WHEN** 一个组件已经经历 `OnRegister` 或 `BeginPlay`
- **THEN** 这只能表明它已经进入 UE 生命周期
- **AND** 不得自动等价为 TCS runtime-ready

### Requirement: TCS 必须提供全局运行时编排子系统
TCS SHALL 提供一个 `UGameInstanceSubsystem` 级别的运行时编排器，用于统一协调战斗实体上的 `Attribute`、`State`、`Buff`、`Skill` 组件进入 ready 的顺序与重评估过程。

#### Scenario: bootstrap subsystem 推进预挂组件初始化
- **WHEN** 一个 Actor 在关卡启动时已预挂 `Attribute`、`State`、`Buff`、`Skill` 中的若干组件
- **THEN** bootstrap subsystem MUST 在所需 subsystem ready 后按依赖顺序推进这些组件进入 ready

#### Scenario: RegisterEntity 作为统一建档入口
- **WHEN** 一个实现了 `ITcsEntityInterface` 的 Actor 需要被纳入 TCS runtime 编排
- **THEN** bootstrap subsystem MUST 提供显式的 `RegisterEntity` 入口来启动即时评估，并在需要时建立 waiting 中的 pending registration 记录
- **AND** MUST NOT 把“第一个组件报到”作为唯一建档方式

#### Scenario: Entity 是否激活 TCS 由开发者显式决定
- **WHEN** 一个实现了 `ITcsEntityInterface` 的 Actor 尚未被上层系统显式调用 `RegisterEntity`
- **THEN** 该 Actor MUST NOT 仅因组件已经报到 bootstrap 就被视为已激活 TCS runtime bootstrap
- **AND** 组件报到本身 MUST NOT 替代开发者对激活时机的显式决定

#### Scenario: 已 ready 时 RegisterEntity 直接返回结果
- **WHEN** 调用 `RegisterEntity` 时该实体已经满足完整 ready 条件
- **THEN** `RegisterEntity` MUST 直接通过返回值表达 `ReadyNow` 或等价结果
- **AND** MUST NOT 为该实体建立长期 pending registration 记录

#### Scenario: 不引入初始化专用 ActorComponent
- **WHEN** 审查这套初始化编排能力的载体时
- **THEN** 它 MUST 由 bootstrap subsystem 承担
- **AND** MUST NOT 要求为每个战斗实体额外挂载一个只负责初始化的 ActorComponent

### Requirement: 组件发现复用 ITcsEntityInterface
TCS SHALL 继续复用 `ITcsEntityInterface` 作为四个运行时组件的统一发现面，而不是新增初始化专用接口。

#### Scenario: bootstrap subsystem 通过现有接口发现组件
- **WHEN** bootstrap subsystem 需要解析一个战斗实体上的 `Attribute`、`State`、`Buff`、`Skill` 组件
- **THEN** 它 MUST 通过 `ITcsEntityInterface` 的现有组件访问出口完成发现

#### Scenario: RegisterEntity 时重扫已有组件
- **WHEN** 外部系统调用 `RegisterEntity` 注册一个战斗实体
- **THEN** bootstrap subsystem MUST 立即通过 `ITcsEntityInterface` 重扫该实体当前已存在的 TCS 组件
- **AND** MUST NOT 假定这些组件一定会在注册之后重新报到

#### Scenario: 未显式注册时返回 NotRegistered 诊断
- **WHEN** 外部系统对一个实现了 `ITcsEntityInterface` 但尚未调用 `RegisterEntity` 的 Actor 执行 `EvaluateEntityRuntimeState`
- **THEN** 结果 MUST 表达该实体当前处于 waiting 或等价非 ready 主状态
- **AND** 阻塞原因 MUST 为 `NotRegistered` 或等价语义
- **AND** MUST NOT 直接下钻为 manager 或 component 级 waiting 原因

#### Scenario: 调用方可轻量查询显式注册态
- **WHEN** 外部系统只需要判断一个 Entity 是否已被开发者显式纳入 TCS runtime bootstrap
- **THEN** bootstrap subsystem SHOULD 提供轻量查询入口（如 `IsEntityRegisteredForRuntimeBootstrap`）
- **AND** 该入口 MUST 只表达显式注册态，不替代完整 ready 诊断

#### Scenario: 不新增初始化专用接口
- **WHEN** 审查这次变更引入的对外契约时
- **THEN** TCS MUST NOT 为初始化编排再新增一个独立接口层
- **AND** `ITcsEntityInterface` 继续只承担组件发现职责，而不承担 ready 状态表达职责

### Requirement: 组件必须提供最小注册协作
TCS SHALL 要求四个运行时组件向 bootstrap subsystem 提供最小注册 / 反注册协作，以覆盖当前提案范围内的预挂组件场景。

#### Scenario: 组件注册后可被纳入编排
- **WHEN** 一个 TCS 组件完成 `InitializeComponent()`
- **THEN** 它 MUST 能向 bootstrap subsystem 报到，使该 subsystem 能对所属 Actor 重新评估初始化顺序

#### Scenario: 组件报到不等于实体已激活
- **WHEN** 一个 TCS 组件已经向 bootstrap subsystem 报到，但所属 Actor 尚未调用 `RegisterEntity`
- **THEN** bootstrap subsystem MAY 记录该组件的可观察状态
- **AND** MUST NOT 因此把该 Actor 视为已激活的 TCS runtime entity

#### Scenario: prepare 完成后可触发重评估
- **WHEN** 一个 TCS 组件完成其显式 prepare 阶段
- **THEN** 它 MUST 能通知 bootstrap subsystem 重新评估所属 Actor 的 ready 条件
- **AND** 组件仅在完成 prepare 后才可被推进到真正 ready

#### Scenario: 等待中的实体拒绝重复注册
- **WHEN** 一个实体已经处于 waiting 的 `RegisterEntity` 流程中
- **THEN** 再次调用 `RegisterEntity` MUST 返回 `AlreadyWaiting` 或等价拒绝结果
- **AND** MUST NOT 为同一 waiting 实体重复追加 ready 回调

#### Scenario: 不把主协作钩子放在 OnRegister
- **WHEN** 审查组件与 bootstrap subsystem 的最小协作钩子时
- **THEN** 主注册 / 注销协作 SHOULD 以 `InitializeComponent()` / `UninitializeComponent()` 为准
- **AND** MUST NOT 仅依赖 `OnRegister()` / `OnUnregister()` 作为唯一的主初始化协作入口

### Requirement: 组件初始化顺序必须显式服从依赖图
TCS SHALL 将 `AttributeManagerSubsystem`、`StateManagerSubsystem`、`AttributeComponent`、`StateComponent`、`BuffComponent`、`SkillComponent` 的初始化顺序建模为显式依赖图，而不是依赖隐式注册先后。

#### Scenario: 基础层先于协作层进入 ready
- **WHEN** 一个 Actor 同时持有 `AttributeComponent`、`StateComponent`、`BuffComponent`、`SkillComponent`
- **THEN** `AttributeComponent` 与 `StateComponent` MUST 作为并行基础层先于 `BuffComponent` 与 `SkillComponent` 进入 ready

#### Scenario: State 只等待全局依赖，不等待 AttributeComponent
- **WHEN** `StateComponent` 需要进入 ready
- **THEN** 它 MUST 等待 `StateManagerSubsystem` ready
- **AND** MUST 等待 `AttributeManagerSubsystem` ready
- **AND** MUST NOT 把同 Actor 上的 `AttributeComponent` 作为前置依赖

#### Scenario: Attribute 与 State 可以并行进入基础层 ready
- **WHEN** 一个 Actor 同时持有 `AttributeComponent` 与 `StateComponent`
- **THEN** `AttributeComponent` 与 `StateComponent` MAY 在各自全局前置依赖满足后并行进入基础层 ready
- **AND** `StateComponent` 不得因为 `AttributeComponent` 尚未 ready 而被阻塞

#### Scenario: Buff 与 Skill 等待 State ready
- **WHEN** `BuffComponent` 或 `SkillComponent` 需要进入 ready
- **THEN** 它们 MUST 等待同 Actor 上的 `StateComponent` 进入 ready

### Requirement: StateComponent runtime-ready 必须包含有效 StateTree 与 StateSlotMapping
TCS SHALL 将 `StateComponent` 的 runtime-ready 定义为有效 `StateTree`、有效 StateSlot 运行时数据、有效 StateSlotMapping，以及 `StateTree` 已真实 running 的组合结果。

#### Scenario: 缺少 StateTree 时 State 不得 ready
- **WHEN** `StateComponent` 没有有效 `StateTree`
- **THEN** `StateComponent` 的 prepare MUST 失败或保持未完成
- **AND** `StateComponent` MUST NOT 进入 runtime-ready
- **AND** bootstrap subsystem MUST NOT 推进 `BuffComponent` 或 `SkillComponent` 进入 ready

#### Scenario: StateSlotMapping 构建失败时 State 不得 ready
- **WHEN** StateSlot 运行时数据无法成功构建，或 StateSlotMapping 无法绑定到实际 `StateTree` 状态
- **THEN** `StateComponent` MUST 保持 not ready
- **AND** 该失败 MUST 输出日志或等价诊断信息
- **AND** bootstrap subsystem MUST 把实体保持在 waiting 状态

#### Scenario: 单独存在的 StateSlot 运行时容器不等于 State ready
- **WHEN** StateSlot 运行时容器已经存在，但没有有效 `StateTree` 或有效 StateSlotMapping
- **THEN** TCS MUST NOT 把该状态视为 `StateComponent` runtime-ready
- **AND** MUST NOT 因此放行依赖 State runtime 的 `BuffComponent` 或 `SkillComponent`

#### Scenario: Buff 与 Skill 只在 StateTree running 后 ready
- **WHEN** bootstrap subsystem 准备推进 `BuffComponent` 或 `SkillComponent`
- **THEN** `StateComponent` MUST 已完成 StateSlotMapping 验证
- **AND** `StateComponent` MUST 已通过 runtime start 入口启动 `StateTree`
- **AND** `StateComponent` 的 running 状态 MUST 已由 `IsRunning()` 或等价判据确认

#### Scenario: runtime slot 集合由当前顶层 StateTree 自动反推
- **WHEN** `StateComponent` 准备构建 StateSlotMapping
- **THEN** 它 MUST 先读取当前顶层 `StateTree` 中真实存在的状态名
- **AND** MUST 只针对这些状态名能够命中的 `StateSlotDefinition` 构建运行时槽位与 StateTree 映射
- **AND** MUST NOT 默认要求当前 `StateTree` 绑定全局所有 StateSlotDefinition

#### Scenario: 当前顶层 StateTree 无法反推出任何槽位时 State 不得 ready
- **WHEN** 当前 `StateTree` 中没有任何状态名能命中 `StateSlotDefinition.StateTreeStateName`
- **THEN** `StateComponent` 的 prepare MUST 失败或保持未完成
- **AND** MUST 输出日志或等价诊断信息
- **AND** bootstrap subsystem MUST NOT 推进 `BuffComponent` 或 `SkillComponent` 进入 ready

#### Scenario: 被当前 StateTree 命中的槽位定义无效时 State 不得 ready
- **WHEN** 某个被当前 `StateTree` 命中的 `StateSlotDefinition` 无效，或缺少有效 `SlotTag`
- **THEN** `StateComponent` MUST 保持 not ready
- **AND** 该失败 MUST 输出日志或等价诊断信息

#### Scenario: 未出现在当前顶层 StateTree 中的槽位不属于 runtime 集合
- **WHEN** 某个 `StateSlotDefinition` 没有命中当前 `StateTree` 的任何真实状态名
- **THEN** 该槽位 MUST NOT 被纳入当前 `StateComponent` 的 runtime slot 集合
- **AND** TCS MUST NOT 要求它参与当前组件的 ready 判定

### Requirement: 预挂组件必须走统一初始化编排
TCS SHALL 让预挂组件复用 bootstrap 编排规则，而不是依赖隐式注册时机。

#### Scenario: 预挂组件复用同一 ready 流程
- **WHEN** 一个关卡内预挂 TCS 组件的 Actor 进入世界
- **THEN** 该 Actor MUST 复用统一的 subsystem-ready 与 component-ready 编排规则
- **AND** MUST NOT 依赖组件注册先后顺序来决定业务 ready

### Requirement: 未 ready 状态下的 Tick 与 public API 必须受保护
TCS SHALL 要求所有受 bootstrap 管理的运行时组件在未 ready 状态下对 Tick、事件与 public runtime API 执行统一保护行为。

#### Scenario: 未 ready 时 Tick 不执行业务逻辑
- **WHEN** 一个组件尚未进入 ready
- **THEN** 它的 Tick MUST 为 no-op，或组件 MUST 保持不执行该 Tick

#### Scenario: 未 ready 时命令型 API 必须拒绝执行
- **WHEN** 调用一个尚未 ready 组件的命令型 public runtime API（如 apply / add / remove / activate 一类入口）
- **THEN** 该调用 MUST 明确拒绝执行业务逻辑
- **AND** MUST 输出日志或等价诊断信息
- **AND** MUST 返回该 API 当前形态可表达的失败结果或 `NotReady`

#### Scenario: Skill 激活在 not-ready 时返回显式 NotReady
- **WHEN** 调用 `SkillComponent::ActivateSkill()` 时 `SkillComponent` 尚未满足 runtime-ready 条件
- **THEN** 该调用 MUST 返回 `NotReady` 或当前 API 可表达的等价结果
- **AND** MUST NOT 把“runtime 未 ready”压扁为普通 `ApplyFailed`

#### Scenario: 未 ready 时 public API 不得偷偷进入半初始化路径
- **WHEN** 调用一个尚未 ready 组件的 public runtime API
- **THEN** 该调用 MUST 明确失败或显式短路
- **AND** MUST NOT 在该入口内隐式推进半初始化业务主链

#### Scenario: 未 ready 时查询型 API 不得伪装成业务事实
- **WHEN** 调用一个尚未 ready 组件的查询型 API
- **THEN** 该调用 MAY 返回空结果、安全默认值或 `NotReady`
- **AND** MUST NOT 把“尚未 ready”错误地伪装成“业务上确实不存在该结果”

#### Scenario: 未 ready 时 public API 必须输出诊断
- **WHEN** 调用一个尚未 ready 组件的 public runtime API
- **THEN** 该调用 MUST 输出日志或等价诊断信息
- **AND** 调用方 MUST 能从返回值或结果枚举中观察到失败或 `NotReady`

### Requirement: bootstrap subsystem 必须提供轻量的 ready 结果与 pending 回调机制
TCS SHALL 让外界能够通过 `RegisterEntity` 返回值、显式检测结果和 waiting -> ready 的一次性回调理解当前状态，而不是要求 bootstrap subsystem 长期缓存所有已 ready entity 的状态。

#### Scenario: RegisterEntity 支持一次性 ready 回调
- **WHEN** 外部系统调用 `RegisterEntity` 并传入一个可选的 ready 回调，且该实体当前尚未 ready
- **THEN** bootstrap subsystem SHOULD 在该实体首次进入完全 ready 时触发该回调一次
- **AND** 在回调触发后删除对应的 pending registration 记录

#### Scenario: RegisterEntity 允许未绑定 OnReady
- **WHEN** 外部系统调用 `RegisterEntity` 时未提供已绑定的 `OnReady` delegate
- **THEN** 该调用 MUST 仍然有效
- **AND** bootstrap subsystem MUST NOT 因 delegate 未绑定而拒绝注册流程

#### Scenario: RegisterEntity 回调可用于蓝图流程
- **WHEN** 蓝图侧需要在实体完全 ready 后继续后续逻辑
- **THEN** `RegisterEntity` SHOULD 提供可被蓝图调用的入口与等价可绑定的 ready 回调能力

#### Scenario: 外界可通过显式检测获取当前阻塞原因
- **WHEN** 一个战斗实体尚未进入 ready
- **THEN** 外部系统 SHOULD 能通过 `EvaluateEntityRuntimeState` 或等价显式检测函数获取当前阻塞原因或诊断信息

#### Scenario: 轻量注册态查询不替代完整诊断
- **WHEN** 调用方已经通过轻量查询确认实体已显式注册，但仍需知道为什么尚未 ready
- **THEN** 调用方 SHOULD 继续使用 `EvaluateEntityRuntimeState`
- **AND** 轻量查询入口 MUST NOT 替代阻塞原因诊断职责

#### Scenario: 未显式注册时阻塞原因优先于普通 waiting 原因
- **WHEN** 一个 Actor 实现了 `ITcsEntityInterface`，其组件已经存在，但开发者尚未调用 `RegisterEntity`
- **THEN** `EvaluateEntityRuntimeState` MUST 优先返回 `NotRegistered` 或等价阻塞原因
- **AND** MUST NOT 先返回 `MissingAttributeComponent`、`StateManagerNotReady` 或其他后续编排阶段诊断

#### Scenario: EvaluateEntityRuntimeState 返回阻塞原因
- **WHEN** 外部系统调用 `EvaluateEntityRuntimeState` 检测一个实体的当前 runtime 状态
- **THEN** 该结果 SHOULD 同时包含总体状态结论与阻塞原因
- **AND** MUST NOT 退化为仅返回布尔值而丢失关键阻塞语义

#### Scenario: EvaluateEntityRuntimeState 的状态与阻塞原因职责分离
- **WHEN** 审查 `EvaluateEntityRuntimeState` 的结果结构时
- **THEN** 其中的总体状态枚举 SHOULD 保持轻量并表达 `Ready`、`Waiting`、`Invalid` 一类主结论
- **AND** 具体的等待或阻塞细分语义 SHOULD 通过独立的阻塞原因枚举表达，而不是把两层信息全部塞进状态枚举

#### Scenario: 不要求长期缓存 ready entity 状态
- **WHEN** 审查 bootstrap subsystem 的轻量方案时
- **THEN** 它 MUST NOT 以长期缓存所有已 ready entity 状态为前提
- **AND** 对已 ready 实体的结论 SHOULD 优先通过返回值或即时检测得到

### Requirement: StateComponent 的真实运行时启动必须服从 ready 屏障
TCS SHALL 区分 `StateComponent` 的预热动作与其真正进入业务运行态的时机，不得再把原生自动启动直接视为 runtime-ready。

#### Scenario: BeginPlay 预热不等于业务 ready
- **WHEN** `UTcsStateComponent` 在 `BeginPlay` 中完成缓存解析或其他预热动作
- **THEN** 这 MAY 作为性能优化存在
- **AND** MUST NOT 自动等价为 `StateComponent` 已经进入 runtime-ready

#### Scenario: State 的初始化应迁移到显式 prepare 流程
- **WHEN** 审查 `UTcsStateComponent` 的初始化职责时
- **THEN** 其现有 manager 解析、slot 初始化、缓存建立等关键初始化逻辑 SHOULD 从 `BeginPlay` 迁移到显式 prepare 阶段
- **AND** MUST NOT 继续依赖 `BeginPlay` 作为关键初始化主路径

#### Scenario: 真实运行时启动受 ready 屏障控制
- **WHEN** `StateComponent` 尚未满足其 subsystem 与组件依赖
- **THEN** 它的真实运行时逻辑与 `StateTree` 驱动 MUST NOT 提前启动

#### Scenario: StateTree 与 StateSlotMapping 未验证时不得启动真实运行时
- **WHEN** `StateComponent` 尚未验证有效 `StateTree` 与有效 StateSlotMapping
- **THEN** `StartStateRuntime()` 或等价真实启动入口 MUST 拒绝进入业务运行态
- **AND** MUST NOT 把仅完成缓存预热的状态标记为 runtime-ready

#### Scenario: State 的真实启动由 bootstrap 流程触发
- **WHEN** `StateComponent` 首次满足其 ready 条件
- **THEN** 它的真实运行时启动 MUST 由 bootstrap 流程统一触发
- **AND** MUST NOT 继续单纯依赖原生自动启动语义

#### Scenario: State 必须通过独立入口进入真正 runtime
- **WHEN** `StateComponent` 从预热态进入真正业务运行态
- **THEN** 它 MUST 通过独立的 runtime start / stop 入口完成生命周期切换
- **AND** 该入口 MUST 与 `BeginPlay`、subsystem 指针解析或缓存预热路径分离

#### Scenario: 直接 StartLogic 不得绕过 bootstrap
- **WHEN** 外部系统直接调用 `StateComponent` 继承自 `BrainComponent` 的 `StartLogic()`
- **THEN** 该调用 MUST NOT 绕过 TCS runtime bootstrap 启动底层 `StateTree`
- **AND** 应输出日志或等价诊断信息

#### Scenario: 直接 StopLogic 必须同步 runtime 状态
- **WHEN** 外部系统或 UE lifecycle 调用 `StateComponent` 继承自 `BrainComponent` 的 `StopLogic()`
- **THEN** 该调用 MUST 走 `StopStateRuntime()` 或等价的 TCS runtime stop 路径
- **AND** MUST 同步更新 State runtime active 标记与 bootstrap ready 诊断

#### Scenario: RestartLogic 保留重启语义但服从 ready 屏障
- **WHEN** 外部系统调用 `RestartLogic()`
- **THEN** `StateComponent` MUST 将其视为重启当前或曾经运行过的 StateTree 逻辑
- **AND** 只有在 TCS runtime 前置条件仍满足时才允许重启
- **AND** MUST NOT 把 `RestartLogic()` 当作 paused 状态的继续入口

#### Scenario: PauseLogic 和 ResumeLogic 保留暂停继续语义
- **WHEN** 外部系统调用 `PauseLogic()` 或 `ResumeLogic()`
- **THEN** `StateComponent` MUST 保留 BrainComponent 的暂停 / 继续语义
- **AND** 这些入口 MUST NOT 隐式推进 runtime prepare 或首次 start
- **AND** not ready 状态下必须拒绝执行并输出诊断
