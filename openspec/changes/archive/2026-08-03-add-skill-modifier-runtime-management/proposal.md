# 变更：补齐 SkillModifier 运行时管理与消费链路

## 背景

当前 TCS 已经具备 `UTcsSkillModifierDefinition`、typed `FStateParam*ModifierInstance`、`UTcsSkillEntrySelector` 与 `SkillModExecution` 执行器这些 authoring 与底层执行拼图，但还缺少真正可用的运行时主链。

目前的缺口不是“少几个 helper”，而是整条链路没有被统筹起来：
- `UTcsSkillComponent` 没有 SkillModifier 的权威账本、查询入口、批量移除入口和生命周期清理逻辑。
- `UTcsSkillEntry` / `UTcsSkillInstance` 没有和 SkillModifier 对接好的统一写入路径，只能各自初始化参数实例，不能稳定承接外部修改器。
- StateTree / Blueprint / C++ 侧没有收敛到同一套 SkillModifier 入口面，导致后续接入点很容易各写各的。
- 当前仓库已经明确 `UTcsSkillInstance` 的参数读取会透传到 `UTcsSkillEntry`，因此继续设计“独立的 SkillInstance 参数宿主”只会制造双写和回滚问题。

另外，本变更默认建立在 `add-def-strategy-defaults-and-validation` 这类 authoring 侧改造之上：typed evaluator、默认执行器和 DataTable authoring 继续由既有提案推进；本提案不重新发明一套 SkillModifier 定义结构，而是把**运行时统筹管理、使用消费与生命周期清理**补完整。

## 变更内容

- 让 `UTcsSkillComponent` 成为 SkillModifier 的唯一权威账本宿主，统一承担登记、查询、移除、恢复与生命周期清理职责。
- SkillModifier 的实际生效容器统一收敛到 `UTcsSkillEntry` 的 `StateParamInstances`（当前代码中的 typed 参数实例容器）；`UTcsSkillInstance` 继续透传读取 `SkillEntry` 的参数实例，不新增第二套目标作用域。
- SkillModifier 不再额外引入“来源保留期字段”或“Entry 作用域 / ActiveInstance 作用域”；所有 SkillModifier 都通过 `SourceHandle` 统一清理，来源存活期间写入的临时 modifier 对所有读取 `SkillEntry` 的地方可见。
- 固化 `Snapshot` 语义：它只冻结 Evaluator 的重新求值，不冻结 SkillModifier 链的增删改。
- 为 `UTcsSkillComponent` 设计统一的 SkillModifier 运行时实例、索引结构与公开入口，要求 C++ / Blueprint / StateTree 全部复用同一套核心路径，而不是手写 `SkillEntry` 容器。
- 设计来源结束、`ForgetSkill`、实例取消/结束后的自动清理与互斥恢复逻辑，确保 SkillModifier 不会永久污染 `SkillEntry`。
- **补齐 effective-value 统一消费契约**：SkillModifier 写入链已存在，但公开读取大多仍停在 base；本提案要求公开业务读取默认走 effective，并把 Attribute OperandBinding / 参数条件 / CD 进度等跨系统消费统一到同一读取口径。
- StateParamInstance SHALL 在宿主创建时绑定自身求值上下文，对外只公开无参 `GetModifiedValue()`；原 `GetValue()` 重命名为 `GetBaseValue()`，SkillModifier Evaluator 仅允许由 ParamInstance 内部调度。SkillEntry / SkillInstance 的参数上下文固定为所属 `SkillComponent` Owner，Skill 激活不接受外部 Instigator。

## 影响范围

- 受影响规范：`skill-runtime`、`state-parameter-management`、`attribute-modifier-runtime`
- 受影响代码：
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillComponent.h`
  - `Source/TireflyCombatSystem/Private/Skill/TcsSkillComponent.cpp`
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillEntry.h`
  - `Source/TireflyCombatSystem/Private/Skill/TcsSkillEntry.cpp`
  - `Source/TireflyCombatSystem/Public/Skill/TcsSkillModifierInstance.h`
  - `Source/TireflyCombatSystem/Public/State/TcsStateParamInstance.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateParamInstance.cpp`
  - `Source/TireflyCombatSystem/Public/State/TcsStateInstance.h`
  - `Source/TireflyCombatSystem/Private/State/TcsStateInstance_Parameters.cpp`
  - `Source/TireflyCombatSystem/Private/Attribute/TcsAttributeComponent_Calculation.cpp`
  - `Source/TireflyCombatSystem/Public/StateTree/Task/*SkillModifier*.h`
  - `Source/TireflyCombatSystem/Private/StateTree/Task/*SkillModifier*.cpp`
  - 相关 Skill / State 生命周期清理与调试查询逻辑
