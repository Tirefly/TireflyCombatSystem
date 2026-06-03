# TCS 架构设计（UE5-Version）

## 文档定位

本文用于记录 TireflyCombatSystem 在 UE5 版本阶段当前已经收敛下来的架构主张与边界。

它不是最终冻结定义，也不是单纯描述“当前代码已经如何实现”的实现真相文档。本文关注的是：

1. TCS 当前版本希望稳定成什么样的模块边界。
2. 各对象在当前阶段分别承担什么职责。
3. 哪些能力属于共享 State Core，哪些属于 Buff，哪些属于 Skill。
4. 当前哪些部分已经落地，哪些部分只是方向收敛，还没有继续扩面实现。

## 配套文档

为了避免把“架构方向”、“当前实现”和“迁移执行事项”继续混在一份文档里，当前版本请配合下面三份文档一起看：

1. `文档：State模块与Buff模块核心函数流程详解（当前实现）.md`
  - 用来回答“现在代码到底怎么跑”。
2. `文档：StateCore-Buff-Skill迁移清单与资产迁移策略.md`
  - 用来回答“State 层现在还残留了哪些 Buff / Skill 债务，以及旧资产应该怎么迁”。
3. `文档：TCS手工验证指南（StateCore-Buff-Skill）.md`
  - 用来回答“当前阶段手工验证应该怎么做，重点看哪些行为边界”。

当前需要先明确的两点是：

- State 与 Buff 的边界已经基本收口完成。
- Skill 的长期方向已经明显收敛，但运行时细化仍然保留到后续独立议题。

## 一、总设计原则

TCS 当前版本架构遵循五条原则：

1. 共享宿主唯一。
2. 运行态和拥有态分离。
3. Buff 专属语义不再污染 State Core。
4. Skill 不直接复用 Buff 的叠层/合并抽象。
5. Skill 的持久数据面与网络复制面应尽量显式、强类型，而不是把所有变量都压进统一泛型容器里。

把这五条翻译成更直白的话就是：

1. Actor 身上只有一套统一的 State 运行时主链。
2. Buff 和 Skill 都不能再各自复制一套平行宿主框架。
3. Buff 的时长、叠层、合并、Period 必须回到 Buff 模块自己管理。
4. Skill 的“我学会了什么”和“这一次技能正在执行什么”必须拆开。
5. 如果某个技能真的需要逐变量复制策略，就应该让引擎直接看到这些真实字段，而不是强行把它们藏进统一数据袋里。

## 二、系统总览

TCS 当前版本按职责分成三个核心层：

1. State Core
2. Buff
3. Skill

同时再配合两个基础支撑系统：

1. Attribute
2. SourceHandle / 因果链

### 2.1 State Core 是什么

State Core 是所有运行态共享的最小公共框架，它负责：

1. 状态定义解析与实例创建。
2. 参数求值与写入。
3. 激活条件检查。
4. 槽位归属与激活裁决。
5. StateTree 生命周期调度。
6. 移除链统一收敛。

State Core 不再负责：

1. Buff 的持续时间语义。
2. Buff 的叠层语义。
3. Buff 的同 Def 合并语义。
4. Skill 的 learned-skill 拥有态。

### 2.2 Buff 是什么

Buff 不是第二套状态宿主，而是：

- 挂在 Actor 身上的语义扩展层。

Buff 模块借助 State 主链提供的扩展点，接管 Buff 专属语义：

1. Duration
2. Stack
3. Merge
4. Period
5. Refresh / Expire
6. Buff 专属事件

### 2.3 Skill 是什么

Skill 也不应该成为第二套执行宿主。

Skill 在当前版本中的定位是：

1. SkillComponent 仍然是宿主级技能入口与未来数据中心的方向，但当前代码本体仍是轻骨架。
2. `UTcsSkillEntry` 表示 learned skill 的拥有态 / 持久数据对象。
3. `UTcsSkillInstance` 表示一次技能激活进入 State 主链后的执行态，而不再承担 learned skill 拥有态。
4. `UTcsSkillDefinition` 当前同时提供 `SkillEntryClass` 与 `SkillInstanceClass` 两个静态配置入口。
5. 当前已经落地的 SkillStateTree schema 会同时暴露 `SkillEntry` 与 `SkillInstance` 两个上下文，而不是只暴露单一 Skill 数据对象。
6. 每次技能激活时，执行态仍然统一借道 `UTcsStateComponent` 进入共享状态主链。

也就是说：

- Skill 拥有态是持久的。
- Skill 执行态是一次性的。
- Skill 当前已经显式拆成 `SkillEntry` 与 `SkillInstance` 两层，而执行主链仍然统一走 StateComponent。

## 三、战斗实体与组件拓扑

一个战斗实体通过实体接口暴露它的战斗能力组件。

当前稳定的组件拓扑方向是：

1. AttributeComponent
2. StateComponent
3. BuffComponent
4. SkillComponent

它们的关系不是并列互相接管，而是：

1. AttributeComponent 负责属性与属性修改器。
2. StateComponent 负责统一运行态主链。
3. BuffComponent 挂接到 StateComponent 的扩展点上，处理 Buff 专属语义。
4. SkillComponent 仍然是技能入口与未来数据中心的预留宿主，但当前代码还没有完整落地 learned skill 集合、SkillModifier 容器和技能激活请求主链。

## 四、State Core 架构方向

### 4.1 Definition 层

State Core 只保留抽象共享定义：

- `UTcsStateDefinition`

它只应该承载所有运行态共享的字段，例如：

1. StateSlotType
2. Priority
3. CategoryTags / FunctionTags
4. StateTreeRef
5. ActiveConditions
6. Parameters / TagParameters

它不再承载：

1. DurationType
2. Duration
3. MaxStackCount
4. MergerType
5. Skill-only 字段

### 4.2 Runtime 层

State Core 的共享运行态对象是：

- `UTcsStateInstance`

它负责：

1. 绑定 Owner / Instigator / 组件上下文。
2. 持有共享参数。
3. 持有 ApplyTimestamp。
4. 持有当前 Stage。
5. 驱动该实例自己的 StateTree runtime。

它不承载 Buff-only runtime 数据，也不承载 learned skill 持久数据。

### 4.3 宿主层

Actor 身上的统一运行时宿主是：

- `UTcsStateComponent`

它负责：

1. TryApplyState / Remove / Query。
2. RuntimeStateSlots 的维护。
3. 槽位 Gate 联动。
4. 激活模式裁决。
5. StateTree Tick 调度。
6. 所有状态实例的统一移除收敛。

这里有一个必须保持的架构结论：

- 无论 Buff 还是 Skill，真正的执行态最终都必须借道 `UTcsStateComponent` 进入系统。

## 五、Buff 架构方向

### 5.1 Buff Definition

Buff 使用独立具体定义类型：

- `UTcsBuffDefinition`

它承载 Buff-only 配置，包括：

1. DurationType
2. Duration
3. Period
4. MaxStackCount
5. MergerType
6. 后续增量反应策略

### 5.2 Buff Runtime Instance

Buff 的运行态实例是：

- `UTcsBuffInstance : UTcsStateInstance`

它的定位不是 learned object，而是：

- 一个已经进入 State 主流程的 Buff 运行实例。

它持有 Buff 专属运行时字段，例如：

1. TotalDuration
2. RemainingDuration
3. Period
4. MaxStackCount
5. StackCount

如果未来确实需要 UpdateTimestamp，也应该优先加在 BuffInstance 自己，而不是回流污染 StateInstance 基类。

### 5.3 Buff 宿主语义层

Buff 的语义层组件是：

- `UTcsBuffComponent`

它负责：

1. Buff 申请入口。
2. 有限时长 Buff 的时长跟踪。
3. Buff 合并编排。
4. Merge 淘汰者移除。
5. Buff 专属事件广播。
6. 后续 Period / Refresh / Expire / 增量反应执行。

它不负责：

1. 自己创建第二套运行态容器。
2. 自己绕开 State 主链直接管理状态生死。

### 5.4 BuffMerger 的职责

`UTcsBuffMerger` 的职责必须收紧成：

1. 决定谁被保留。
2. 决定谁被淘汰。
3. 决定 survivor 的 stack/max stack 如何收敛。

它不应该继续负责：

1. Duration 刷新策略。
2. Period 重置或补触发策略。
3. 时长归零后的掉层或整 Buff 清除策略。

这些后续增量反应应由 `UTcsBuffComponent` 统一执行，而不是分散在每个 merger 实现里。

## 六、Skill 架构方向

Skill 是当前最容易重新混乱的地方，所以这里需要写得最明确。

### 6.1 SkillEntry 与 SkillInstance 的定位

当前代码已经不再采用“`UTcsSkillInstance` 同时承担 learned skill 拥有态与执行态”的旧方案。

当前的最小落地骨架是：

1. `UTcsSkillEntry`
  - 表示 learned skill 的拥有态 / 数据对象。
  - 当前最小实现只记录其绑定的 `UTcsSkillDefinition`。
  - 当前阶段不承载一次技能激活的运行时执行态。
2. `UTcsSkillInstance : UTcsStateInstance`
  - 表示一次技能激活进入 State 主链后的执行态。
  - 当前通过成员引用回指 `UTcsSkillEntry`。
  - 当前已经作为 `UTcsSkillDefinition::ResolveStateInstanceClass()` 的技能执行态配置结果参与状态实例创建。

这意味着当前架构里已经冻结的一点是：

- learned skill 拥有态与单次技能执行态是两个不同对象，不应再混回同一类型里。

关于“单技能持久字段、复制字段最终由哪一层承载”，当前代码还没有扩面到足以冻结最终答案。至少在现阶段，旧文档里把这些能力默认压到 `UTcsSkillInstance` 上，已经属于陈旧表述。

### 6.2 SkillDefinition 的配置职责

`UTcsSkillDefinition` 当前已经同时提供两类运行时类型配置：

1. `SkillEntryClass`
  - 用来决定 learned skill 拥有态对象的类型。
2. `SkillInstanceClass`
  - 用来决定一次技能激活进入 State 主链后的执行态类型。

因此当前版本里，SkillDef 的职责不再只是“选择一个 SkillInstance 派生类”，而是：

1. 决定 learned skill 应生成什么 `UTcsSkillEntry` 派生类。
2. 决定技能激活时应生成什么 `UTcsSkillInstance` 派生类。

### 6.3 SkillComponent 的定位

`UTcsSkillComponent` 仍然应被视为宿主级技能入口和未来数据中心的方向。

但必须直说的是：

1. 当前代码中的 `UTcsSkillComponent` 仍然只有基础生命周期骨架。
2. learned skill 集合、SkillModifier 容器、完整激活 / 取消 / 查询 API 目前都还没有在组件内落地成稳定实现。
3. 因此旧文档里把这些能力写成“当前版本已经由 SkillComponent 承担”的表述，已经过时。

更准确的说法应当是：

1. SkillComponent 是后续继续承接这些能力的预留宿主。
2. 当前代码只完成了 Skill 类型拆分与 StateTree 上下文骨架，没有完成完整技能子系统。

### 6.4 SkillStateTree 的当前边界

当前 Skill 侧已经落地的一条关键实现事实是：

- `UTcsSTSchema_Skill` 会同时向 SkillStateTree 暴露 `UTcsSkillInstance` 和 `UTcsSkillEntry` 两个上下文。

这意味着当前 SkillStateTree 的最小边界已经从旧文档里的“只围绕单一 SkillInstance 数据对象运转”，更新为：

1. 读取本次激活执行态：`UTcsSkillInstance`
2. 读取 learned skill 拥有态数据：`UTcsSkillEntry`
3. 必要时继续把 `UTcsSkillInstance` 当作 `UTcsStateInstance` 使用，复用共享 State 主链

因此旧文档里这类说法都已经不准确：

1. “SkillStateTree 只消费单一 SkillInstance 持久数据对象”
2. “SkillInstance 本身不是执行态”

### 6.5 技能执行态的定位

技能真正开始施放时，当前已经与代码对齐的描述应当是：

1. 最终进入 `UTcsStateComponent` 共享执行主链的，是某个 `UTcsSkillInstance`（或其派生类）。
2. 这个对象本质上仍然是 `UTcsStateInstance` 的技能特化执行态。
3. 它在运行时持有对 `UTcsSkillEntry` 的引用，从而把一次激活和 learned skill 拥有态连接起来。

所以这里必须修正文档里的旧说法：

- 旧说法“技能激活时创建一个独立 `StateInstance`，而 `SkillInstance` 只做 learned skill 数据载体”已经不再符合当前代码。

更贴近当前代码的模型是：

```text
SkillComponent（当前仍是轻骨架，后续承接技能入口）
  -> learned skill 对象：UTcsSkillEntry / SkillEntryClass
  -> 技能激活执行态：UTcsSkillInstance / SkillInstanceClass
  -> UTcsSkillInstance 进入 UTcsStateComponent 共享主链执行
```

### 6.6 跨 Activation 数据如何处理

需要跨 Activation 持久化的数据，当前至少不应再写成“默认由 `UTcsSkillInstance` 承担”。

当前代码已经说明：

1. `UTcsSkillInstance` 已经是单次技能激活执行态。
2. `UTcsSkillEntry` 才是当前 learned skill 拥有态骨架。

因此更稳妥的当前结论是：

1. 跨 Activation 的 learned skill 数据不应继续写进 `UTcsSkillInstance` 这类执行态对象。
2. learned skill 侧更细的数据承载方案，应围绕 `UTcsSkillEntry` 或其同层对象继续收敛。
3. 这部分还没有在当前代码里继续扩面实现，不应在本文里把未来方案写成既成事实。

### 6.7 Skill 网络同步策略

这一节在旧文档里默认把“单技能持久字段、复制字段、`OnRep` 扩展面”归到 `UTcsSkillInstance` 上，这已经与当前代码的对象拆分相冲突。

当前更准确的说法只能收敛到下面这一级：

1. learned skill 级持久字段与网络复制策略，不应继续默认绑定到 `UTcsSkillInstance`。
2. 这类能力未来若要继续扩面，必须先明确 `UTcsSkillEntry`、`UTcsSkillComponent` 与复制宿主之间的关系。
3. 在这件事正式冻结之前，本文不再把任何一条具体复制方案写死成“当前版本既定架构”。

## 七、参数与快照边界

当前版本方向里，参数系统仍然是共享层能力。

`FTcsStateParameter` 的 `bIsSnapshot` 不应继续视为 Skill-only 语义。

它的正确定位是：

- 参数求值时机策略。

因此它既可以用于 Skill，也可以用于 Buff。

在 Skill 侧，当前更稳妥的参数流转关系应写成：

```text
SkillModifier（后续议题）
  -> 作用于 SkillComponent / SkillEntry 侧数据
  -> 生成 learned skill 侧参数结果或持久字段
  -> 激活时按 snapshot 或非 snapshot 规则写入本次 SkillInstance
  -> StateTree 执行消费 SkillInstance / StateInstance 参数
```

## 八、重复应用与冲突策略边界

TCS 当前版本必须明确区分两种完全不同的问题：

1. Skill 重复激活。
2. Buff 运行态合并。

### 8.1 Skill 冲突处理回答的问题

Skill 侧关心的是：

1. 同一个 learned skill 记录（当前可理解为同一个 `UTcsSkillEntry`）能不能再次激活。
2. 再次激活后是拒绝、刷新、覆盖、排队还是允许并行。
3. 是否重置冷却、是否生成新的执行态。

### 8.2 Buff merge 回答的问题

Buff 侧关心的是：

1. 已经进入系统的同 Def 运行态如何保留或淘汰。
2. stack 如何累加或覆盖。
3. 合并后的时长、层数、Period 如何收敛。

因此当前原则必须是：

- Skill 重复激活策略不能复用 Buff merger。
- Buff merger 也不能承担 Skill 激活冲突职责。

## 九、对象职责总表

| 对象 | 当前职责 | 不该承担的职责 |
|---|---|---|
| `UTcsStateDefinition` | 共享状态定义 | Buff-only / Skill-only 字段 |
| `UTcsBuffDefinition` | Buff 专属配置 | 共享宿主职责 |
| `UTcsStateInstance` | 共享执行态实例 | Buff-only runtime，learned skill 拥有态 |
| `UTcsBuffInstance` | Buff 运行时实例 | learned skill 持久数据 |
| `UTcsStateComponent` | 统一执行态宿主 | Buff 专属语义、Skill 拥有态 |
| `UTcsBuffComponent` | Buff 语义扩展层 | 第二套状态宿主 |
| `UTcsSkillEntry` | learned skill 拥有态 / 数据对象骨架 | 单次技能执行态 |
| `UTcsSkillInstance` | 一次技能激活的执行态，且通过 `SkillEntry` 连接拥有态数据 | learned skill 持久数据宿主 |
| `SkillDef` | 声明技能静态配置，包括 `SkillEntryClass` 与 `SkillInstanceClass` | 宿主级实例容器、一次 activation 的执行现场 |
| `UTcsSkillComponent` | 技能入口与未来数据中心的预留宿主；当前仍是轻骨架 | 被误写成已经完整落地的 learned skill / SkillModifier 管理器 |

## 十、当前落地状态

这份 UE5-Version 架构稿需要和当前项目状态一起看。

### 已经基本落地的部分

1. State Core / Buff 边界收口。
2. `UTcsStateDefinition` 抽象化方向。
3. `UTcsBuffDefinition` / `UTcsBuffInstance` / `UTcsBuffComponent` 的职责拆分。
4. Buff merge 改为 Buff 自己的语义链。

### 已经冻结方向、但尚未继续扩面实现的部分

1. Skill 仍保持轻骨架。
2. Skill 代码层已经显式拆出 `UTcsSkillEntry`（learned skill 拥有态）与 `UTcsSkillInstance`（单次激活执行态）。
3. `UTcsSkillDefinition` 已经提供 `SkillEntryClass` 与 `SkillInstanceClass` 两个配置入口。
4. `UTcsSTSchema_Skill` 已经同时向 SkillStateTree 暴露 `SkillEntry` 和 `SkillInstance` 双上下文。
5. `UTcsSkillComponent` 的 learned skill 容器、SkillModifier、完整激活 / 取消 / 查询主链仍未继续扩面。
6. Skill 更细的持久字段宿主、复制宿主、接口规范与任务节点适配仍留待后续独立议题。

## 十一、当前结论

TCS 当前版本架构可以用一句话概括：

- State Core 负责统一执行态框架。
- Buff 负责效果运行语义。
- Skill 负责拥有态、数据面与激活入口。

再翻译得更具体一点：

1. State 是唯一执行主链。
2. Buff 是挂在 State 上的语义扩展层。
3. `UTcsSkillEntry` 是当前 learned skill 的拥有态 / 数据对象骨架。
4. `UTcsSkillInstance` 是一次技能激活执行态，并通过 `UTcsStateComponent` 进入共享状态主链。
5. SkillDef 当前负责同时配置 `SkillEntryClass` 与 `SkillInstanceClass`。
6. SkillStateTree 当前通过双上下文同时消费 `SkillEntry` 与 `SkillInstance`。
7. `UTcsSkillComponent` 仍是后续技能数据中心方向，但当前实现还没有完整扩面到 learned skill / SkillModifier 主链。
8. 更细的持久字段宿主与网络复制策略，仍需围绕 `SkillEntry` / `SkillComponent` 关系继续单独冻结，本文不再把旧方案写成当前事实。

如果后续设计不偏离这八条，TCS 的 State / Buff / Skill 三者边界仍然可以保持清晰，同时也为未来网络同步和技能自定义数据面预留出合理扩展点。