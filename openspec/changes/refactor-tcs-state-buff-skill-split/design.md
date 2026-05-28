## 背景

TCS 当前的 `State` 运行时已经是完整主链路，但它并不是一个纯粹的共享层：

- `UTcsStateDefinition` 同时持有共享字段和 Buff 专属字段。
- `UTcsStateInstance` / `UTcsStateComponent` 已经内建了叠层、合并、持续时间等 Buff 运行时语义。
- 技能侧仍然缺少真正落地的定义、实例和冷却模型，同时当前 `State` 体系里还挂着一部分实验性质的 Skill 引用与上下文暴露实现。

同时，活跃 change `migrate-manager-api-to-component` 已经完成了本轮代码改写，当前只剩测试与归档收尾。也就是说，当前 `UTcsStateComponent` / `UTcsStateManagerSubsystem` 的代码形态可以视为冻结基线；这个基线改善了运行时宿主职责，但并没有解决状态核心、Buff 与 Skill 之间的数据模型边界问题。

本设计在此基础上进一步收口：

- 状态核心只保留共享宿主与生命周期框架
- `Buff` 拿回持续时间与效果运行语义
- `Skill` 在当前阶段保持现状，不在本提案里继续扩面实现

并且当前可以先固定的，不是最终 Skill 命名，而只是两条阶段性实现事实：

- 当前代码中的 learned-skill 数据对象仍使用 `UTcsSkillInstance` 命名，但这不是长期命名结论
- 技能释放时，当前仍由 Skill 侧向 `UTcsStateComponent` 申请运行态进入主链；后续 Skill-specific runtime 收敛由独立 change 处理

## 目标 / 非目标

- 目标：
  - 把 `UTcsStateDefinition` 收敛为只包含共享字段的抽象基类
  - 把 `DurationType` / `Duration` 与 Stack / Merge 一并迁移到 Buff 模块
  - 保持 `UTcsStateComponent` 为统一具体宿主，避免平行宿主框架重复
  - 为后续 Skill 独立议题预留清晰边界
- 非目标：
  - 本提案不要求第一阶段立即把 TCS 物理拆成多个 `.Build.cs` 运行时模块；这里的“模块”首先指职责边界和代码组织边界
  - 本提案不覆盖对外玩法功能扩面，只聚焦已有状态/技能/Buff 语义的重新归位
  - 本提案不在同一 change 中实现 `UTcsSkillDefinition`、`UTcsSkillEntry`、新的 `UTcsSkillInstance` 或 Skill 专用 schema；这些 follow-up 由独立 change 推进
  - 本提案不直接实现编辑器资产创建流程、`UFactory`、`UAssetDefinition` 或编辑器侧注册表调整；这些改动应留在 `add-tcs-def-editor-authoring`
  - 本提案不替代 `migrate-manager-api-to-component`；两者需要顺序衔接，而不是并行各改一半

## 决策

- 决策：`UTcsStateDefinition` 改为抽象共享基类
  - 原因：当前共享基类已经混入 Buff 和 Skill 专属字段；继续允许直接创建裸 `StateDefinition` 只会放大混杂。
  - 备选方案：
    - 保留具体 `UTcsStateDefinition` 并靠 `StateType` 区分：拒绝，因为这会继续把子类语义压回基类。

- 决策：删除 `ETcsStateType` / `StateType`
  - 原因：当前枚举几乎不承担真实行为分流，只是在共享基类上额外挂了一层类型标签；在抽象基类 + 具体定义类型的方向下，它已经没有继续保留的必要。
  - 备选方案：
    - 保留枚举仅用于兼容诊断：暂不采用，因为这会延长共享基类继续携带子类分类信息的寿命。

- 决策：状态核心只保留共享宿主职责
  - 原因：Apply / Remove / Query / Slot / StateTree / 生命周期调度是所有运行态共享的最小公共框架。
  - 备选方案：
    - 为 Buff 再复制一套平行宿主链：拒绝，因为会把职责混杂替换为框架重复。

- 决策：`DurationType` / `Duration` 归 Buff 模块
  - 原因：本轮已确认 Skill 的时间语义回到 Skill 侧；共享基类不再允许保留子类专属时间字段。
  - 备选方案：
    - 在状态核心保留 Duration：拒绝，因为这会让抽象基类继续承载 Buff-only 字段。

- 决策：当前 `UTcsStateMerger` / `MergerType` 先随 Buff 语义一起迁移
  - 原因：现有合并链路里，`MergerType` 与 `MaxStackCount` 紧密耦合，`StackDirectly` / `StackByInstigator` 明确依赖叠层语义，而 `MergeStateGroup()` 也是围绕同 Def 状态合并与淘汰展开；如果只移动 `MaxStackCount` 而让 `MergerType` 继续停留在抽象 `StateDefinition`，会形成一半在 Buff、一半在状态核心的断裂设计。
  - 备选方案：
    - 让 `MergerType` 继续留在抽象 `StateDefinition`：拒绝，因为这会保留 Buff-only 配置在共享基类。
    - 立即把所有 `UTcsStateMerger` 一次性重构成状态核心的全新冲突策略抽象：暂不采用，因为当前需求首先是把 Buff 语义从共享基类中拿出去，没必要在同一阶段再扩成一轮新的抽象设计。

- 决策：若未来确实存在非 Buff 的“同 Def 冲突处理”需求，再把当前 `UTcsStateMerger` 一分为二
  - 原因：现有 `UseNewest` / `UseOldest` / `NoMerge` 从语义上看可以被解释为更通用的重复态冲突处理，但当前项目里它们仍然服务于现有 State/Buff 合并链路，没有足够证据要求现在就把它们提升为状态核心正式契约。
  - 备选方案：
    - 现在就把 `UseNewest` / `UseOldest` / `NoMerge` 抽成通用状态核心策略：可以做，但会让当前提案从“剥离 Buff”扩大成“重新设计冲突策略体系”，范围过宽。

- 决策：若未来 Skill 需要处理“同 Def 重复应用”，应由 Skill 自己定义冲突策略，而不是复用 Buff 合并器
  - 原因：Skill 侧关心的是“能否重复激活、重复激活后刷新/拒绝/覆盖/排队”的拥有态与执行态策略；Buff merger 关心的是“已产生的运行中效果如何叠层、保留、淘汰”。两者虽然都面对“重复应用”，但问题域不同。
  - 备选方案：
    - 未来让 Skill 直接复用 `UTcsStateMerger`：拒绝，因为这会把 Buff 的叠层/合并语义错误套用到 Skill 的激活冲突语义上。

- 决策：当前阶段冻结本提案内部的 Skill 实现范围
  - 原因：现阶段的首要目标是先把状态核心与 Buff 的边界收敛；若同时补齐 Skill 定义、实例、冷却和激活模型，会把 change 重新扩大成第二个系统重做。
  - 备选方案：
    - 同步推进 Skill 收敛：拒绝，因为这会把当前 change 从“先清理共享基类和 Buff 语义”扩大成“同时重写 Skill 系统”，回归范围过大。

- 决策：本提案不把当前 `UTcsSkillInstance` 命名冻结成长期契约
  - 原因：本提案的职责是收敛 `State Core` / `Buff` 边界，而不是在同一阶段把 Skill 的 owned-data 对象名与 activation runtime 名最终定型。
  - 原因：真正需要冻结的是“已学会技能持有态”和“技能执行态”必须分离，而不是强行把当前代码里的旧名字声明成永久结论。
  - 备选方案：
    - 继续把当前 `UTcsSkillInstance` 旧名写成“当前与后续方向都成立”：拒绝，因为这会和后续独立 Skill/runtime 收敛 change 正面冲突。

- 决策：同 Def Skill 重复激活规则延后到按技能类型调研后再定
  - 原因：不同技能对重复激活的合理处理方式可能完全不同，例如拒绝、刷新、覆盖、排队、并行；当前没有足够证据用一条统一规则覆盖全部技能类型。
  - 备选方案：
    - 现在先给所有 Skill 规定统一重复激活规则：拒绝，因为这会在缺乏技能类型调研的情况下过早冻结错误契约。

- 决策：`bIsSnapshot` 不再视为 Skill-only 语义
  - 原因：快照与否本质上是参数求值时机策略，不只技能会需要，周期 Buff 也同样可能需要“生成时快照”与“每次重算”两种模式。
  - 备选方案：
    - 把 `bIsSnapshot` 彻底迁到 Skill 模块：拒绝，因为这会错误地把一个可能被 Buff 复用的参数评估策略绑定成技能专属能力。

- 决策：`UTcsStateInstance` 暴露给 `StateTreeSchema` 的内容集暂不固化
  - 原因：当前 `OwnerSkillCmp` / `InstigatorSkillCmp` 等上下文接入是实验性质实现，尚不足以直接升格为长期契约。
  - 备选方案：
    - 直接以当前 Schema 暴露面作为最终设计：拒绝，因为这会把实验实现过早固化成架构约束。

- 决策：以 `migrate-manager-api-to-component` 的当前结果作为冻结基线
  - 原因：该议题虽然尚未归档，但当前剩余工作是测试与归档，不再预计继续修改运行时代码；本提案可以直接以现有代码形态为起点，而不是等待未来 rebase。
  - 备选方案：
    - 继续把它当成未来还会变动的前置代码面：拒绝，因为这会放大不必要的流程耦合，和当前事实不符。

- 决策：编辑器创作与 Factory/注册表改动不放进本提案
  - 原因：`UTcsBuffDefinition` 可能需要新的资产创建入口，但这属于已存在的 `add-tcs-def-editor-authoring` 边界，更适合在那里补齐而不是在运行时重构 change 中并行落地。
  - 备选方案：
    - 在本提案里顺手修改编辑器创作流程：拒绝，因为会让运行时重构和编辑器创作 change 再次耦合。

## 边界：Skill 重复应用与 Buff 合并

未来如果 Skill 模块也出现“同 Def 被再次应用”的场景，不应直接照搬当前 Buff merge 体系，而应先区分这两个问题发生在不同层：

- Skill 侧处理“同一个已学会技能能不能再次激活，以及再次激活后怎么处理”
- Buff 侧处理“技能再次激活后产出的运行中效果如何合并、叠层、保留或淘汰”

这里真正应该固定的前提，是“重复激活策略面对的是 learned-skill 持有态，而不是两个执行态实例互相 merge”。在本 change 实施时，当前代码里的该对象名仍是 `UTcsSkillInstance`；后续如果独立 change 把它翻名为 `UTcsSkillEntry` 并引入新的执行态 `UTcsSkillInstance`，这条边界依然成立。

### 命名草案

如果将来需要给 Skill 侧引入独立策略抽象，当前更推荐这类命名方向：

- `UTcsSkillActivationConflictPolicy`
- `UTcsSkillDuplicateApplyPolicy`
- `UTcsSkillExecutionConflictPolicy`

而当前 Buff 侧现有 `UTcsStateMerger` 在本提案方向下，更接近下面这类语义：

- `UTcsBuffMerger`
- `UTcsBuffMergePolicy`

本提案当前不会立即改这些类名，但后续实现时应按这个语义边界思考，而不是继续把两类策略都塞进同一个 merger 概念里。

### 对比

| 维度 | Skill 重复应用策略 | Buff merge 策略 |
|---|---|---|
| 处理对象 | 已学会技能持有对象对应的技能激活请求 | 已经存在的 Buff / State 运行态实例 |
| 关心的问题 | 能否重复激活、重复激活后如何处理 | 已存在效果如何叠层、刷新、保留、淘汰 |
| 典型结果 | 拒绝、刷新执行、覆盖旧执行、排队、允许并行、是否重置冷却 | `UseNewest`、`UseOldest`、`NoMerge`、叠层累加、按施法者分组叠层 |
| 所属模块 | Skill | Buff |
| 是否应该复用当前 `UTcsStateMerger` | 否 | 是，至少在当前阶段先作为 Buff 语义承载 |
| 与 `StateComponent` 的关系 | 先由 Skill 决定是否再次激活，再决定是否向 `StateComponent` 申请运行态 | 已经进入 `StateComponent` 的运行态之后，再由 merge 策略处理 |

### 实践规则

后续实现阶段可以用一句话卡住边界：

- “技能能不能再放一次”，由 Skill 策略决定
- “再放一次后产生的效果怎么并”，由 Buff 合并决定

## 风险 / 取舍

- 现有 Def 资产与作者流程会被打断，需要提供迁移路径或一次性升级说明。
- 现有运行时代码大量集中在 `Public/State/**` 和 `Private/State/**`，拆分时很容易出现“名义拆分，实际逻辑仍留在状态核心”的半吊子状态。
- `migrate-manager-api-to-component` 虽然代码已冻结，但尚未完成测试与归档；流程上仍需要明确两者的责任边界，避免后续归档说明互相覆盖。
- 由于本阶段保留 Skill 现状，`State` 体系里已有的 Skill 引用接入点不会一次性清零，需要明确它们是后续独立 change 的债务，而不是继续被当作共享设计。
- 当前 `UTcsStateInstance` / `StateTreeSchema` 的上下文暴露面仍是实验基线，若实现阶段不先收敛最小共享面，容易把试验字段误固化为正式契约。

## 迁移计划

1. 以 `migrate-manager-api-to-component` 当前已冻结的代码结果作为实现基线；其测试与归档可独立继续，不再阻塞本提案的设计收敛。
2. 收敛 Definition 层：抽象 `UTcsStateDefinition`，引入 `UTcsBuffDefinition`，把 Buff-only 字段从共享基类迁出。
3. 删除 `ETcsStateType` / `StateType`，不再依赖共享基类枚举做类型表达。
4. 把 `MergerType` 与当前 `UTcsStateMerger` 链路一起迁到 Buff 侧，先作为 Buff 的同 Def 合并/叠层策略处理。
5. 从 `State` 侧剥离 Buff 运行时数据与逻辑：Duration、Stack、Merge、Period、Refresh、Expire、Buff 专属事件。
6. 对现有 Skill 引用接入点做清单化记录和兼容冻结，本阶段不扩展本 change 内的 Skill 模块实现。
7. 重新定义共享参数系统与 `StateInstance -> StateTreeSchema` 的最小暴露面，把快照视为共享参数策略，并把实验性质的上下文字段与正式契约拆开。
8. 记录对编辑器创作的影响说明；若 `UTcsBuffDefinition` 需要新的资产创建入口，由 `add-tcs-def-editor-authoring` 补充处理，而不是在本提案中直接改 Factory/注册表。
9. 在后续独立议题中，再决定 Skill 模块的 `UTcsSkillDefinition`、`UTcsSkillEntry`、新的 `UTcsSkillInstance`、Skill schema 与桥接细节，以及是否需要从 Buff 中再抽出通用重复态冲突策略抽象。
10. 对“同 Def 技能重复激活”的处理规则按技能类型做单独调研，再决定 Skill 是否需要独立冲突策略抽象及其默认策略集合。

## 开放问题

- 当前 `UseNewest` / `UseOldest` / `NoMerge` 是否值得在未来从 Buff 合并体系中再抽成共享状态核心的冲突策略抽象。
- 同 Def 技能重复激活时，哪些技能类型应拒绝、刷新、覆盖、排队或允许并行，需要在后续 Skill 议题中如何分类调研。
- `UTcsSkillDefinition` 最终是直接引用单一执行态定义，还是组合一个或多个 State/Buff 定义来表达技能执行结果，仍需后续独立议题细化。