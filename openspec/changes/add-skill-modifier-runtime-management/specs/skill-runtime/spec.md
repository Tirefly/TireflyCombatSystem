## ADDED Requirements
### Requirement: SkillComponent 承载 SkillModifier 权威账本

TCS SHALL 让 `UTcsSkillComponent` 成为 SkillModifier 的唯一权威账本宿主，并通过统一组件入口承担登记、查询、移除、恢复与生命周期清理职责。

#### Scenario: 通过组件入口应用 SkillModifier
- **WHEN** 任意 C++、Blueprint 或 StateTree 调用面要对技能应用一个 SkillModifier
- **THEN** 它 MUST 通过 `UTcsSkillComponent` 的统一组件入口完成
- **AND** 它 MUST NOT 直接手写 `UTcsSkillEntry` 的参数实例容器

#### Scenario: 通过来源批量清理 SkillModifier
- **WHEN** 一个 `SourceHandle` 对应的来源结束
- **THEN** `UTcsSkillComponent` MUST 能按该 `SourceHandle` 找到并移除全部相关 SkillModifier
- **AND** 该移除流程 MUST 不依赖对全部 learned skill 做无界全表扫描

#### Scenario: 单次 apply 写入失败时回滚本次新增项
- **WHEN** 一次 SkillModifier apply 调用已经向部分目标 `SkillEntry` 成功写入 runtime entry
- **AND** 后续目标写入失败
- **THEN** TCS MUST 回滚该次调用已经成功写入的 runtime entry 与参数实例链
- **AND** 系统 MUST NOT 留下半成功的脏状态

### Requirement: SkillModifier 直接写入 SkillEntry 参数实例链

TCS SHALL 让 SkillModifier 的唯一生效容器落在 `UTcsSkillEntry` 的 typed `StateParamInstances` 上。`UTcsSkillInstance` SHALL 继续通过既有透传访问器读取 `SkillEntry` 的参数实例，不新增独立的 SkillModifier 目标作用域。

#### Scenario: SkillEntry 立即看到新写入的 SkillModifier
- **WHEN** `UTcsSkillComponent` 成功将一个 SkillModifier 应用到某个 `UTcsSkillEntry`
- **THEN** 目标参数对应的 typed `StateParamInstance.ModifierInstances` MUST 立即包含该 modifier
- **AND** 后续通过 `SkillEntry` 读取该参数时 MUST 立即看到修正后的值

#### Scenario: 多目标 selector 会展开为多条 runtime records
- **WHEN** 一个 SkillModifier 的 `EntrySelector` 在一次 apply 中命中多个 `UTcsSkillEntry`
- **THEN** TCS MUST 为每个命中的 `UTcsSkillEntry` 分别创建独立的 runtime record
- **AND** 系统 MUST NOT 把多个目标折叠进同一条 runtime record

#### Scenario: 单条 runtime record 只对应一个目标落点
- **WHEN** TCS 创建一条 SkillModifier runtime record
- **THEN** 该记录 MUST 只对应一个具体 `TargetSkillEntry`
- **AND** 它 MUST 只对应一个具体目标参数落点，而不是“多个目标的聚合请求对象”

#### Scenario: SkillInstance 继续透传读取同一参数链
- **WHEN** 某个活跃 `UTcsSkillInstance` 读取它对应 `SkillEntry` 上的目标参数
- **THEN** 它 MUST 读到与 `SkillEntry` 相同的 SkillModifier 链结果
- **AND** TCS MUST NOT 因 SkillModifier 引入第二套仅供 `SkillInstance` 使用的目标容器

#### Scenario: 来源存活期间写入的临时 SkillModifier 对全部读取者可见
- **WHEN** 一个来源在其存活期间向目标 `SkillEntry` 写入临时 SkillModifier
- **THEN** 任何在该期间读取该 `SkillEntry` 参数的调用者 MUST 看到这份临时修正
- **AND** 该共享可见性 SHALL 被视为本提案接受的运行时语义

### Requirement: Snapshot 只冻结 Evaluator 重求值

TCS SHALL 将 `Snapshot` 语义限定为“禁止 Evaluator 重新求值”，而不是“冻结 SkillModifier 链”。即使目标参数已经 snapshot，SkillModifier 仍然可以增删改对应的 modifier 实例链。

#### Scenario: Snapshot 参数在首次求值后仍可写入 SkillModifier
- **WHEN** 一个 `bIsSnapshot == true` 的参数已经完成首次求值
- **AND** 后续有 SkillModifier 被应用到该参数
- **THEN** TCS MUST 允许该 SkillModifier 写入参数实例链
- **AND** 读取结果 MUST 体现该 SkillModifier 的修正

#### Scenario: Snapshot 只阻止再次调用 Evaluator
- **WHEN** 一个已 snapshot 的参数被重复读取
- **THEN** 它 MUST 不因为再次读取而重新调用 Evaluator
- **AND** 但它 MAY 因 SkillModifier 链变化而返回不同的最终值

### Requirement: SkillModifier 生命周期必须可按来源与目标清理

TCS SHALL 为 SkillModifier 提供可预测的生命周期清理语义，至少覆盖 `SourceHandle` 结束、`ForgetSkill` 和 Exclusive 恢复三个方向。

#### Scenario: 来源结束时按 SourceHandle 移除全部 SkillModifier
- **WHEN** 一个来源的 `SourceHandle` 生命周期结束
- **THEN** `UTcsSkillComponent` MUST 移除该来源写入的全部 SkillModifier
- **AND** 被移除后对应 `SkillEntry` 的参数实例链 MUST 不再保留这些 modifier

#### Scenario: ForgetSkill 时清理目标 SkillEntry 的 SkillModifier 账本记录
- **WHEN** `UTcsSkillComponent::ForgetSkill` 移除某个 learned skill
- **THEN** 与该 `SkillEntry` 关联的 SkillModifier 账本记录和索引 MUST 一并清理
- **AND** 后续查询 MUST 不再返回指向已遗忘 `SkillEntry` 的 SkillModifier

#### Scenario: 技能实例结束时清理该实例来源的 SkillModifier
- **WHEN** 一个 `UTcsSkillInstance` 生命周期结束，且它对应的 `SourceHandle` 曾写入 SkillModifier
- **THEN** TCS MUST 通过该 `SourceHandle` 清理这些 SkillModifier
- **AND** 该清理路径 SHOULD 复用统一的按来源移除逻辑，而不是复制第二套实例结束专用算法

#### Scenario: Exclusive SkillModifier 在高优先级移除后恢复次高候选
- **WHEN** 同组 `ModifierId` 的 Exclusive SkillModifier 中，当前最高优先级实例被移除
- **THEN** TCS MUST 恢复该组内剩余最高优先级候选的 `bActive`
- **AND** 后续参数读取 MUST 反映恢复后的有效实例

### Requirement: SkillModifier 外部入口必须统一路由到组件核心

TCS SHALL 为 SkillModifier 提供统一的组件级外部入口，并让 StateTree、Blueprint 与 C++ 共用同一条核心逻辑路径。

#### Scenario: StateTree 任务应用 SkillModifier 时附带来源句柄
- **WHEN** 一个 StateTree 任务在运行时对目标 `SkillComponent` 应用 SkillModifier
- **THEN** 它 MUST 将来源 `SourceHandle` 一并传入 `UTcsSkillComponent` 的组件入口
- **AND** 后续清理路径 MUST 能依赖该 `SourceHandle` 自动移除对应 SkillModifier

#### Scenario: 组件入口返回展开后的平铺结果
- **WHEN** 组件入口对一个或多个 `ModifierId` 执行 apply
- **THEN** 返回给调用方的 runtime records MUST 是展开后的平铺结果集合
- **AND** TCS MUST NOT 为此强制引入额外的 request、batch 或 group 级运行时对象

#### Scenario: Blueprint 与 C++ 不直接操作 SkillEntry 容器
- **WHEN** Blueprint 或 C++ 调用面要新增、查询或移除 SkillModifier
- **THEN** 它们 MUST 调用 `UTcsSkillComponent` 暴露的统一运行时入口
- **AND** 它们 MUST NOT 绕过组件直接改写 `SkillEntry.StateParamInstances`

### Requirement: 公开参数读取默认返回 effective value

TCS SHALL 让业务可见的 Skill / State 参数读取默认返回 effective value（base + 激活中的 SkillModifier 链结果）。无参 `GetModifiedValue()` 与任何默认参数读取 API SHALL 使用 ParamInstance 自身绑定的上下文；`GetBaseValue()` 与任何显式 Base API SHALL 仅表示未经 SkillModifier 链改写的 base value。跨系统消费（含 Attribute OperandBinding、参数条件、冷却进度）MUST 复用同一 effective 读取口径，而不是各自读取 base 字段。

#### Scenario: Get*ParamByTag 默认返回 SkillModifier 修正后的值
- **WHEN** 目标 `SkillEntry` 参数上已存在激活中的 SkillModifier
- **AND** 调用方通过 `UTcsStateInstance` / `UTcsSkillInstance` 的 `Get*ParamByTag` 读取该参数
- **THEN** 返回值 MUST 等于无参 `GetModifiedValue()` 的结果
- **AND** 返回值 MUST NOT 仅等于 base `GetBaseValue()`

#### Scenario: SkillInstance 读取真实宿主参数而不是本地空容器
- **WHEN** `UTcsSkillInstance` 调用 `Get*ParamByTag`
- **THEN** 它 MUST 通过 virtual `Get*ParamInstance` 定位到 `SkillEntry` 上的参数实例
- **AND** MUST NOT 因读取本地空容器而返回缺失或 base 默认值

#### Scenario: 需要 base 时必须走显式 Base API
- **WHEN** 调用方明确只需要未经 SkillModifier 修正的基础参数值
- **THEN** 它 MUST 调用显式 Base 读取入口（如 `Get*BaseParamByTag`）
- **AND** 默认 `Get*ParamByTag` MUST 继续返回 effective

#### Scenario: 冷却进度分母使用 effective 冷却时长
- **WHEN** 技能冷却已启动，且冷却参数上存在激活中的 SkillModifier
- **THEN** `GetRemainingCooldownRatio` 使用的冷却分母 MUST 为 effective 冷却时长
- **AND** MUST NOT 继续使用 base `GetBaseValue()` 作为分母

#### Scenario: 新增公开参数读取 API 不得绕过 effective 契约
- **WHEN** 后续新增任何公开的、面向业务的 StateParam 解析值读取 API
- **THEN** 该 API MUST 默认返回 effective value
- **AND** 若接口要暴露 base，MUST 在 API 名称中明确声明 Base
- **AND** MUST NOT 直接把 `NumericValue` / `GetBaseValue()` 作为业务默认返回

### Requirement: 技能激活固定使用所属实体

TCS SHALL 将 `UTcsSkillComponent::ActivateSkill` 的公开输入收敛为 `SkillDefId`。SkillComponent Owner MUST 同时作为创建出的 SkillInstance 的 Owner、Instigator、SourceHandle Instigator，以及其 SkillEntry 参数的固定求值 Instigator。

#### Scenario: ActivateSkill 不接受外部 Instigator
- **WHEN** 检查 `UTcsSkillComponent::ActivateSkill` 的 public API
- **THEN** 它 MUST 仅接收 `SkillDefId`
- **AND** MUST NOT 允许调用方传入与 SkillComponent Owner 不同的 Instigator

#### Scenario: SkillEntry 参数上下文绑定所属实体
- **WHEN** SkillComponent 为所属实体创建一个 SkillEntry
- **THEN** 该 Entry 的全部 StateParamInstance MUST 绑定该 Entry 与所属实体作为内部求值上下文
- **AND** 后续激活该技能 MUST NOT 重写这些参数的 Instigator 上下文
