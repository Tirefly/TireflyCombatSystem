## 1. 规范与设计
- [x] 1.1 明确 TCS runtime-ready 与 UE 生命周期的分层契约
- [x] 1.2 确认 `UTcsRuntimeBootstrapSubsystem` 只暴露 entity 级状态 / 原因 / 事件的职责边界，以及它与四个运行时组件的最小协作面
- [x] 1.3 明确 `UTcsDefinitionManagerSubsystem` 的实例依赖边界与 `DefinitionManagerNotReady` 诊断面；全局预加载完成状态不作为单一组件的 ready 前置条件
- [x] 1.4 明确 `Attribute` 与 `State` 的并行基础层关系，以及 `Buff`、`Skill` 的后置依赖与失败策略
- [x] 1.5 明确未 ready API 的分类矩阵（命令型 / 查询型 / Tick）与统一保护策略
- [x] 1.6 明确 `StateComponent` 的 runtime start/stop 入口命名与职责边界
- [x] 1.7 明确 `RegisterEntity` 的返回值语义、等待中重复注册拒绝规则、可空 `OnReady` one-shot 回调语义，以及蓝图调用面
- [x] 1.8 明确 `EvaluateEntityRuntimeState` 的返回结构、显式注册契约与阻塞原因表达
- [x] 1.9 明确 `ETcsEntityRuntimeState` 与 `ETcsEntityRuntimeBlockReason` 的枚举草案、`NotRegistered` 诊断及组合约束
- [x] 1.10 覆盖当前提案场景：预挂组件
- [x] 1.11 明确 `StateComponent` runtime-ready 的硬约束：有效 `StateTree`、有效 StateSlot 运行时数据、有效 StateSlotMapping、以及 `StateTree` running 缺一不可
- [x] 1.12 明确不同 Actor 类型可因绑定不同顶层 `StateTree` 而拥有不同 StateSlot 子集，StateSlotMapping 只验证由当前 `StateTree` 自动反推出的槽位
- [x] 1.13 明确 `StopLogic()` 需要同步 TCS runtime stop，`RestartLogic()` 是重启语义，`PauseLogic()` / `ResumeLogic()` 是暂停 / 继续语义，且这些入口都必须服从 TCS ready 屏障

## 2. bootstrap 基础设施
- [x] 2.1 引入 `UTcsRuntimeBootstrapSubsystem : UGameInstanceSubsystem`
- [x] 2.2 在 bootstrap subsystem 中通过 `InitializeDependency` 显式依赖 `UTcsDefinitionManagerSubsystem`
- [x] 2.3 提供统一的 `RegisterEntity` 入口，并在注册时重扫已有组件
- [x] 2.4 复用 `ITcsEntityInterface` 解析四个组件，并支持按 Actor 做即时依赖评估与 waiting registration 跟踪
- [x] 2.5 建立轻量的显式注册集合、瞬时 ready 检测结果与 waiting 中 pending registration 记录，而不是长期缓存已 ready entity
- [x] 2.6 提供统一的组件注册、反注册、重评估与诊断输出入口

## 3. DefinitionManager 依赖边界
- [x] 3.1 bootstrap 仅检查 `UTcsDefinitionManagerSubsystem` 实例是否可用，不将全局预加载完成标记当作单一组件 ready 前置条件
- [x] 3.2 `StateComponent` / `AttributeComponent` 按自身必需 Definition 域查询准备，不等待无关域预加载
- [x] 3.3 保持 DefinitionManager 与 bootstrap subsystem 都只承担全局职责，不回流 Actor 本地业务逻辑

## 4. 四个组件接入 bootstrap
- [x] 4.1 `UTcsAttributeComponent` 接入 bootstrap，并把本地业务 ready 与 UE 生命周期解耦
- [x] 4.2 `UTcsStateComponent` 接入 bootstrap，并把现有 `BeginPlay` 初始化迁移到显式 prepare 流程，再通过独立的 runtime start/stop 入口收敛真实运行时启动时机
- [x] 4.2.1 将 StateSlotMapping 初始化流程改为可被 `PrepareStateRuntime()` 判定成功 / 失败，缺少有效 `StateTree` 时必须失败
- [x] 4.2.2 在 `PrepareStateRuntime()` 中验证 StateSlot 运行时数据与 StateSlotMapping；失败时保持 not ready，并输出诊断
- [x] 4.2.3 确保 `StartStateRuntime()` 只在 prepare 已验证成功后启动 `StateTree`，并以 `IsRunning()` 或等价判据作为 ready 结果
- [x] 4.2.4 封堵继承自 `UStateTreeComponent` 的自动启动、直接 `StartLogic()` / `StopLogic()` / `RestartLogic()` 与 base tick 绕过路径
- [x] 4.2.5 移除 `StateComponent` 手工 StateSlot 子集配置，改为由当前顶层 `StateTree` 自动反推并只构建 / 验证命中的槽位子集
- [x] 4.2.6 调整 `StopLogic()`、`RestartLogic()`、`PauseLogic()`、`ResumeLogic()`，保留 UE BrainComponent 语义但加上 TCS ready guard
- [x] 4.3 `UTcsBuffComponent` 接入 bootstrap，改为服从统一 ready 屏障
- [x] 4.4 `UTcsSkillComponent` 接入 bootstrap，改为服从统一 ready 屏障
- [x] 4.5 四个组件在 `InitializeComponent()` / `UninitializeComponent()` 中提供最小注册 / 反注册协作，并补充显式 prepare 完成回报，不新增初始化专用接口或统一基类

## 5. 入口面与运行时约束
- [x] 5.1 统一约束未 ready 状态下的 Tick 行为
- [x] 5.2 按 API 类型统一约束未 ready 状态下的 public runtime API 行为（命令型 / 查询型）
- [x] 5.3 为 bootstrap subsystem 增加 `RegisterEntity` 返回值、`EvaluateEntityRuntimeState` 阻塞原因诊断（含 `NotRegistered`），以及 waiting -> ready 的一次性回调支持
- [x] 5.3.1 为调用方提供轻量的显式注册态查询入口，避免仅为判断激活态而依赖完整 `EvaluateEntityRuntimeState` 结果
- [x] 5.3.2 收束 `SkillComponent::ActivateSkill()` 的 not-ready 返回语义，避免压扁为普通 `ApplyFailed`
- [x] 5.4 确认缺少 `StateTree`、StateSlot 运行时数据无效、StateSlotMapping 失败时，bootstrap 诊断与组件日志不会错误放行 Buff / Skill
- [x] 5.5 `EvaluateEntityRuntimeState` 实时查询组件 ready 状态，避免 StateTree stopped 后继续使用 stale tracked ready 结果
- [x] 5.6 确认当前 `StateTree` 无法反推出任何槽位、命中定义无效、或命中槽位映射失败时，State 保持 not ready 且 Buff / Skill 不启动

## 6. 验证
- 当前已完成编译验证与 OpenSpec 校验；场景级手工验证仍待补齐。
- 运行时动态创建且关键组件已齐备的 Actor 场景已移出当前提案范围。
- [ ] 6.1 覆盖预挂组件场景的初始化顺序验证
- [ ] 6.2 覆盖未 ready 时 Tick / 事件 / public API 的保护行为验证
- [x] 6.3 执行 `openspec validate add-component-runtime-bootstrap --strict --no-interactive`
- [ ] 6.4 覆盖缺少 `StateTree` 或 StateSlotMapping 无效时，`StateComponent` 保持 not ready 且 Buff / Skill 不启动的验证
- [ ] 6.5 覆盖不同 Actor 类型绑定不同顶层 `StateTree` 时，只验证各自自动反推出的 runtime slot 集合且互不误伤的验证
- [ ] 6.6 覆盖 `StopLogic()`、`RestartLogic()`、`PauseLogic()`、`ResumeLogic()` 在 ready / not ready / paused 状态下的行为验证
