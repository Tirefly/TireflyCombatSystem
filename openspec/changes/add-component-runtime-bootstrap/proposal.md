# 变更：为 TCS 运行时组件增加显式初始化编排层

## 背景

当前 `UTcsAttributeComponent`、`UTcsStateComponent`、`UTcsBuffComponent`、`UTcsSkillComponent` 的业务可用性仍然部分依赖 Unreal 原生 `ActorComponent` 生命周期（如 `OnRegister`、`BeginPlay`、懒解析、事件补绑）。这在以下场景中缺少强保证：

- 同 Actor 上多个 TCS 组件的注册先后顺序不可控
- `StateComponent` 的运行时逻辑与 `StateTree` 启动时机缺少统一的 ready 屏障
- `BuffComponent` 与 `SkillComponent` 依赖 `StateComponent` 已正式启动 `StateTree` 且已构建有效的 StateSlotMapping；单独存在的 StateSlot 运行时容器不能被视为有意义的 ready 状态

当前系统虽然依赖 `UGameInstanceSubsystem::Initialize()` 先于 Actor `BeginPlay()` 完成这一 UE 事实，但组件侧没有一套正式的 TCS 运行时就绪协议来表达“何时允许真正进入业务运行态”。

## 变更内容

- 新增一个独立的 **TCS 运行时初始化编排 capability**，把 “UE 组件已注册” 与 “TCS 组件已就绪” 明确分层
- 新增一个 `UGameInstanceSubsystem` 级别的运行时编排器（暂名 `UTcsRuntimeBootstrapSubsystem`），统一协调四个运行时组件的 ready 顺序，其中 `AttributeComponent` 与 `StateComponent` 作为并行基础层，`BuffComponent` 与 `SkillComponent` 作为后置协作层；该编排器不长期缓存已 ready 的实体，只维护等待中的注册项与一次性 ready 回调，并通过函数返回值或即时检测结果表达 entity 当前状态
- 要求该编排器在 `Initialize()` 中通过 `InitializeDependency` 显式依赖：
  - `UTcsAttributeManagerSubsystem`
  - `UTcsStateManagerSubsystem`
- 为 `AttributeManagerSubsystem` 与 `StateManagerSubsystem` 增加显式的 runtime-ready 契约，避免组件仅靠拿到指针就假定全局前置条件已满足
- 继续复用 `ITcsEntityInterface` 作为四个组件的统一发现面，而不是新增初始化专用接口
- 不新增专门用于初始化的 ActorComponent；组件初始化编排由新的 bootstrap subsystem 承担
- 要求 bootstrap subsystem 提供统一的 `RegisterEntity` 入口，用于显式把一个实现了 `ITcsEntityInterface` 的实体纳入运行时编排；该入口应支持可选的“entity 首次完全 ready 后”的一次性回调注册，并且应能被蓝图调用
- 明确一个 Entity 是否激活 TCS 能力、以及何时激活，完全由开发者显式调用 `RegisterEntity` 决定；组件报到本身不等于实体已激活 TCS runtime bootstrap
- 在保留完整 `EvaluateEntityRuntimeState` 诊断结果的同时，允许 bootstrap subsystem 提供轻量的“是否已显式注册”查询入口，避免调用方仅为判断激活态而被迫解析完整阻塞结果
- 要求四个 TCS 组件至少提供最小协作：在 `InitializeComponent()` / `UninitializeComponent()` 中向 bootstrap subsystem 注册 / 反注册，并暴露内部 prepare / ready / shutdown 入口
- 当前提案先覆盖以下场景：
  - 预挂组件
- 当前提案暂不覆盖以下场景：
  - 运行时动态创建且关键组件已齐备的 Actor
- 要求组件的 Tick、事件绑定和 public runtime API 只能在 ready 后进入真正业务路径；在未 ready 状态下必须按 API 类型执行统一保护策略：命令型 API 显式拒绝执行，查询型 API 按具体语义返回安全结果或 `NotReady`，Tick / 内部 step 直接 no-op；所有路径都必须输出日志，并通过 `RegisterEntity` 返回值或即时检测结果暴露当前状态
- 要求 bootstrap 诊断能够区分“实体不合法”和“实体尚未显式注册”；对实现了 `ITcsEntityInterface` 但尚未调用 `RegisterEntity` 的 Actor，应返回 `NotRegistered` 或等价阻塞原因，而不是把组件报到误判为已激活
- 其中 `SkillComponent::ActivateSkill()` 一类命令型入口，在 runtime 未 ready 时必须优先返回 `NotReady` 或当前 API 可表达的等价结果，而不是压扁为普通 `ApplyFailed`
- 要求 `StateComponent` 的真实运行时启动与 `StateTree` 驱动时机统一交给新的初始化流程控制，而不是仅依赖原生自动启动时序；现有依托于 `BeginPlay` 的初始化内容应剥离到显式的 prepare 流程中，再通过独立的 runtime start/stop 入口进入真实运行态（当前推荐命名：`StartStateRuntime()` / `StopStateRuntime()`）
- 要求 `StateComponent` 的 runtime-ready 硬前置条件包含：有效 `StateTree`、StateSlot 运行时数据构建成功、StateSlotMapping 成功绑定到实际 `StateTree` 状态，以及 `StartStateRuntime()` 后 `StateTree` 处于 running 状态；缺少任一条件时，`StateComponent` 必须保持 not ready，bootstrap 必须阻止 `BuffComponent` 与 `SkillComponent` 进入 ready
- 要求不同 Actor 类型可以因绑定不同顶层 `StateTree` 而拥有不同 StateSlot 子集；`StateComponent` 的 runtime slot 集合必须由当前顶层 `StateTree` 中真实出现的状态名自动反推，StateSlotMapping 只验证这些由 `StateTree` 反推出的槽位子集，不再把全局所有 StateSlotDefinition 默认视为每个 Actor 的必需槽位
- 要求继承自 `BrainComponent` / `StateTreeComponent` 的运行态控制入口服从 TCS ready 屏障：直接 `StartLogic()` 不得绕过 bootstrap；直接 `StopLogic()` 必须重定向到 TCS runtime stop 以同步 ready 标记与 bootstrap 诊断；`RestartLogic()` 表达重启语义并在前置条件满足时走 TCS runtime 重启路径；`PauseLogic()` / `ResumeLogic()` 表达暂停与继续语义并在 ready / running 条件下转发到底层 StateTree

## 影响范围

- 受影响规范：
  - **新增** `component-runtime-bootstrap`
  - **修改** `combat-manager-subsystems`
  - 后续讨论后可能继续影响：`state-management`、`buff-runtime`、`skill-runtime`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public/Attribute/*`
  - `Source/TireflyCombatSystem/Public/State/*`
  - `Source/TireflyCombatSystem/Public/Buff/*`
  - `Source/TireflyCombatSystem/Public/Skill/*`
  - `Source/TireflyCombatSystem/Public/TcsEntityInterface.h`
  - 对应 `Private/*` 实现文件

## 预期收益

- 初始化顺序从“隐式依赖注册/BeginPlay 时机”变为“显式依赖 ready 契约”
- `Buff` / `Skill` 对 `State` 的依赖不再靠碰巧的组件注册顺序成立
- 预挂组件初始化路径的一致性提升
- 复用现有 `ITcsEntityInterface`，避免引入额外的初始化专用接口层
- 为后续继续收敛 State / Buff / Skill 边界提供统一基础设施

## 风险与代价

- 这是一个横跨四个运行时模块的基础架构变更，不应并入现有 `add-skill-modifier-runtime-management`
- 如果 bootstrap subsystem 过度集中，容易演变成持有过多实体本地细节的“上帝对象”
- 如果把 ready 检查散落到每个 API / Tick / 事件入口而没有统一编排层，复杂度会快速失控
- `StateComponent` 与 `StateTree` 的启动时机改造风险较高，需要把“预热”和“可运行”分开表达
- 如果只调用 StateSlotMapping 构建流程但不验证构建结果，bootstrap 会错误地把“尝试过构建”当成“State runtime 已可用”，从而过早放行 Buff / Skill；实现阶段必须补强这个判据
- 如果继续按全局所有 StateSlotDefinition 验证每个 `StateComponent`，会误伤只需要部分 StateSlot 的 Actor 类型；实现阶段必须改为由当前顶层 `StateTree` 自动反推 runtime slot 子集

## 待审核重点

- bootstrap subsystem 的职责边界如何定义，才能既保持轻量 pending-registration 模型，又避免它回流 Actor 本地业务逻辑
- `RegisterEntity` 的返回值与 pending 回调语义如何收紧：已 ready 时只返回结果、不触发回调；等待中时才保存 one-shot 回调并在 ready 后删除记录
- 显式激活契约如何保持可观测：未调用 `RegisterEntity` 的合法 Entity 应如何通过 `EvaluateEntityRuntimeState` 暴露 `NotRegistered` 诊断，避免与普通 waiting 原因混淆
- `InitializeComponent()` / `UninitializeComponent()` 是否足以覆盖当前提案范围内的初始化与收尾路径
- `StateComponent` 现有 `BeginPlay` 逻辑如何完整迁移到显式 prepare 流程，而不残留隐式启动路径
- `StateComponent` 如何把“有效 StateTree + 有效 StateSlotMapping + StateTree running”合并为对外唯一的 runtime-ready 语义，并在缺失 StateTree 或映射失败时提供清晰诊断
- `StateComponent` 如何以当前顶层 `StateTree` 作为 runtime slot 集合的单一事实源，并如何处理“无匹配槽位”“命中定义无效”“树中存在未映射状态”等情况
- `StopLogic()`、`RestartLogic()`、`PauseLogic()`、`ResumeLogic()` 如何保留 UE BrainComponent 语义，同时不绕过 TCS runtime-ready 屏障
- 未 ready API 的分类保护策略是否足以覆盖代表性入口，还是需要再补一张实现期 API 对照表
