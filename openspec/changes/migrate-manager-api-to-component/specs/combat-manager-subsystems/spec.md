# 规范增量 —— combat-manager-subsystems

> **权威执行依据**：本 spec delta 定义 Subsystem 最终职责白名单与契约。具体 deprecated 标记清单、删除顺序与行号，以 `Plugins/TireflyCombatSystem/Documents/细化执行方案_ManagerAPI迁移到Component/05_PhaseFG_兼容层与最终硬删除.md` 为准。

## ADDED Requirements

### Requirement: State Manager Subsystem 的最终职责

迁移完成后，`UTcsStateManagerSubsystem` SHALL 严格只暴露以下职责，且不得多于这些：definition cache/load（`Initialize`、`LoadFromDeveloperSettings`、`LoadFromAssetManager`、`LoadStateOnDemand`、`PreloadAllStates`、`PreloadCommonStates`）、definition 查询（`GetStateDefinition`、`GetStateDefinitionByTag`、`GetStateSlotDefinition`、`GetStateSlotDefinitionByTag`、`GetAllStateDefNames`）、全局 state instance ID 工厂（`AllocateStateInstanceId`），以及跨 Actor 门面（`TryApplyStateToTarget`）。

#### Scenario: 枚举出的 public API 是穷尽集合

- **WHEN** 在归档后检查 `UTcsStateManagerSubsystem` 的 public 接口面时
- **THEN** 每一个声明出的 public 方法 MUST 属于上述四类之一；任何额外的 public API 都属于违约

#### Scenario: TryApplyStateToTarget 只做委托

- **WHEN** `TryApplyStateToTarget(TargetActor, StateDefId, Instigator, StateLevel, ParentSourceHandle)` is invoked
- **THEN** 它 MUST 只做四件事：(1) 校验 `TargetActor`，(2) 通过 `ITcsEntityInterface` 解析 `UTcsStateComponent`，(3) 调用 `StateComp->TryApplyState(...)`，(4) 返回结果；该方法中 MUST NOT 出现任何状态实例构造、参数求值或槽位逻辑



### Requirement: Attribute Manager Subsystem 的最终职责

迁移完成后，`UTcsAttributeManagerSubsystem` SHALL 严格只暴露以下职责，且不得多于这些：definition cache/load（`Initialize`、`LoadFromDeveloperSettings`、`LoadFromAssetManager`）、definition 与 tag 查询（`GetAttributeDefinition(FName)`、`GetModifierDefinition(FName)`、`TryResolveAttributeNameByTag`、`TryGetAttributeTagByName`）、全局 ID 工厂（`AllocateAttributeInstanceId`、`AllocateModifierInstanceId`、`AllocateModifierChangeBatchId`），以及 SourceHandle 工厂（`CreateSourceHandle`）。

#### Scenario: 枚举出的 public API 是穷尽集合

- **WHEN** 在归档后检查 `UTcsAttributeManagerSubsystem` 的 public 接口面时
- **THEN** 每一个声明出的 public 方法 MUST 属于上述四类之一

#### Scenario: ID 计数器只存在于 Subsystem 中

- **WHEN** 在 TCS 中搜索 `GlobalAttributeInstanceIdMgr` / `GlobalAttributeModifierInstanceIdMgr` / `GlobalAttributeModifierChangeBatchIdMgr`
- **THEN** 这些声明 MUST 只出现在 `UTcsAttributeManagerSubsystem` 中；任何 Component 或其他类都 MUST NOT 声明等价计数器



### Requirement: Subsystem 保持 GameInstanceSubsystem 作用域

`UTcsStateManagerSubsystem` 与 `UTcsAttributeManagerSubsystem` 二者都 SHALL 保持 `UGameInstanceSubsystem` 作用域。迁移过程中 MUST NOT 将它们迁移到 `UWorldSubsystem`、`UGameInstance`、`AGameMode` 或任何其他作用域。

#### Scenario: 初始化顺序保持不变

- **WHEN** PIE / Standalone / Server 启动流程进行时
- **THEN** 两个 Subsystem 的 `Initialize()` 都 MUST 在任何 Actor 的 `BeginPlay()` 之前完成，从而保证 Component 预热路径能成功执行



### Requirement: Subsystem 到 Component 不再保留 Friend 访问

迁移完成后，Component 类 SHALL NOT 再与 Manager Subsystem 声明 friendship。迁移过程中 MUST 从 `UTcsStateComponent` 中移除 `friend class UTcsStateManagerSubsystem`（以及任何类似声明）。

#### Scenario: 归档后 grep 结果为空

- **WHEN** `git grep "friend class UTcsStateManagerSubsystem" Plugins/TireflyCombatSystem/Source` executes post-archive
- **THEN** 该命令 MUST 不产生任何输出

#### Scenario: 不引入等价后门

- **WHEN** 审查迁移 PR 时
- **THEN** MUST NOT 为替代被删除的 friendship 再引入任何 `friend` 声明、任何为 Subsystem 暴露私有状态的 `public:` 区域，或任何通过子类中转的 `protected` 后门
