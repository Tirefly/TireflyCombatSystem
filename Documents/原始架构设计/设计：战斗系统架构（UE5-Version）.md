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

1. SkillComponent 负责宿主级技能数据中心。
2. SkillInstance 表示 learned skill 的持久数据载体。
3. 具体技能可以通过派生 SkillInstance 声明自己的持久变量、复制规则和 `OnRep`。
4. 如果某个技能需要使用特定的 SkillInstance 派生类，配置入口应放在 SkillDef 上，而不是散落在组件侧或执行态侧。
5. SkillStateTree 负责消费和写入这些数据，但 SkillInstance 自身不承担玩法执行逻辑。
6. 每次技能激活时，仍然创建独立 StateInstance 进入 StateComponent 执行。

也就是说：

- Skill 拥有态是持久的。
- Skill 执行态是一次性的。
- Skill 的数据面可以因技能而异，但执行主链仍然统一走 StateComponent。

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
4. SkillComponent 持有 learned skill、技能修改器和技能实例集合，并在需要时向 StateComponent 发起技能执行请求。

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

### 6.1 SkillInstance 的定位

`UTcsSkillInstance` 当前版本表示：

- 某个 learned skill 的持久数据载体。

它可以作为具体技能的数据扩展点被继续派生，用来声明：

1. 技能自己的跨 Activation 持久变量。
2. 技能自己的参数结果与状态字段。
3. 技能自己的网络复制变量与复制策略。
4. 技能自己的 `OnRep` 事件分发表面。

这里还需要补一个配置落点约束：

- 如果框架需要允许每个技能指定不同的 SkillInstance 派生类，这个配置位置应放在 SkillDef 上。

原因是：

1. SkillDef 天然就是“这个技能该生成什么运行时数据载体”的静态配置入口。
2. 这类配置不应该放在 SkillComponent 上，否则会把技能定义期信息混进宿主级实例容器。
3. 这类配置也不应该放在 State 执行态侧，否则会把 learned skill 的拥有态配置错误下沉到一次 activation 的执行现场。

它不表示：

1. 一次技能执行实例。
2. 一个常驻运行中的 StateTree 容器。
3. 一次施法期间的临时执行态。
4. 真正执行业务逻辑的对象。

### 6.2 SkillComponent 的定位

`UTcsSkillComponent` 是宿主级技能数据中心。

它应该持久化持有：

1. 全部 learned skill 实例。
2. 宿主级 SkillModifiers。
3. 面向技能筛选、批量修改、装备/养成影响的统一缓存与入口。

SkillModifier 应归 SkillComponent 持有，而不是只绑定到单个 SkillInstance 上。

原因是：

1. 某些 SkillModifier 可能先于技能学习而存在。
2. 技能修改器往往需要按标签、等级段、技能槽、技能类型批量筛选目标技能。
3. 这些行为天然更适合放在技能数据中心，而不是散在每个技能对象内部。

当未来某个具体 SkillInstance 派生类需要网络复制时，SkillComponent 也应是更自然的实例宿主和复制挂载点，而不是把复制责任反向压回 State 执行态。

同时，SkillComponent 不应成为“选择 SkillInstance 派生类”的配置位置。更合理的职责划分是：

1. SkillDef 负责声明这个技能应使用哪种 SkillInstanceClass。
2. SkillComponent 负责实例化并持有这个 SkillInstance。
3. SkillStateTree 负责消费和写回这个 SkillInstance 上的数据。

### 6.3 Skill 数据面与复制面的归属

Skill 的单技能数据应归 SkillInstance 持有，但要继续区分“数据”和“逻辑”：

1. 数据字段放在 SkillInstance 派生类上。
2. 逐变量复制策略也定义在 SkillInstance 派生类上。
3. `OnRep` 最多用于事件分发、表现刷新、本地缓存失效通知等轻量处理。
4. 真正的玩法消费、读写与决策仍然交给 SkillStateTree、SkillComponent 或专用函数执行。
5. SkillInstance 派生类的选择来自 SkillDef，而不是由运行时链路临时决定。

这里可以借鉴 GAS 的地方，不是整套系统形态，而是：

- 真正需要逐字段复制控制的数据，应该被定义成真实反射属性，而不是强行塞进统一泛型容器后再试图让网络系统猜它的内部结构。

这意味着：

1. `FInstancedStruct`、`TMap` 或其他通用容器可以作为技能内部实现细节存在。
2. 但当某个技能需要 `RepNotify`、复制条件、PushModel 或其他逐字段网络策略时，主扩展面应该是 SkillInstance 派生类自己的真实属性。

### 6.4 技能执行态的定位

技能真正开始施放时，不是让 SkillInstance 自己进入执行态，而是：

1. SkillComponent 选定要激活的 SkillInstance。
2. 读取该 SkillInstance 当前的持久字段、参数结果和技能状态。
3. 创建本次执行对应的 StateInstance。
4. 把这次激活需要的输入、等级、快照值和临时参数写入 StateInstance。
5. 由 StateComponent 把该 StateInstance 放入运行态主链。

因此当前版本模型是：

```text
SkillComponent
  -> 按 SkillDef 配置创建并持有 SkillInstance (learned skill / persistent data)
  -> 激活时读取 SkillInstance 字段
  -> 创建独立 StateInstance
  -> StateInstance 进入 StateComponent 执行
```

### 6.5 跨 Activation 数据如何处理

需要跨 Activation 持久化的数据，不应该放在常驻 SkillStateTree runtime 中。

正确归属应是：

1. 宿主级公共数据 -> SkillComponent。
2. 单技能级持久数据、复制字段 -> SkillInstance 派生类（由 SkillDef 配置）。
3. 单次激活局部数据 -> 本次 StateInstance。

所以“技能需要跨 Activation 收集的数据”不是一个让 SkillInstance 继承 StateInstance 的理由。

真正应持久化的是：

- 技能自己的持久数据与复制字段。

而不是：

- 一次 activation 的 StateTree 执行现场。

### 6.6 SkillStateTree 与 SkillInstance 的协作边界

SkillStateTree 仍然是技能逻辑的主要消费端，但它和 SkillInstance 的边界需要保持清楚：

1. SkillStateTree 可以读取 SkillInstance 的持久字段。
2. SkillStateTree 可以在明确接口约束下写回 SkillInstance 的持久字段。
3. SkillInstance 自身不在 `OnRep`、Setter 或普通成员函数里执行业务效果。
4. 复制回调最多发出通知，再由 SkillStateTree、组件或上层系统决定如何消费。

这样做的目的是：

- 让 SkillInstance 继续保持“数据载体”职责。
- 让 SkillStateTree 保持“逻辑执行者”职责。
- 不把执行逻辑重新散落回数据对象本身。

### 6.7 Skill 网络同步策略

如果未来要支持联机，当前版本更推荐的 Skill 数据同步方向是：

1. 需要细粒度复制控制的字段，声明在 SkillInstance 派生类上。
2. 需要 `RepNotify`、复制条件、PushModel 的字段，也声明在 SkillInstance 派生类上。
3. `OnRep` 只做事件分发、状态通知、表现同步，不直接执行业务效果。
4. 真正的玩法状态流转，仍由服务器侧 SkillStateTree / State 主链决定。

这条策略的核心不是“让 SkillInstance 去执行业务逻辑”，而是：

- 把 Skill 的持久数据面与复制策略面，收敛到一个用户可派生、可强类型声明的对象上。
- 把“具体技能应该使用哪个 SkillInstance 派生类”的决定，收敛到 SkillDef 这个静态定义层上。

## 七、参数与快照边界

当前版本方向里，参数系统仍然是共享层能力。

`FTcsStateParameter` 的 `bIsSnapshot` 不应继续视为 Skill-only 语义。

它的正确定位是：

- 参数求值时机策略。

因此它既可以用于 Skill，也可以用于 Buff。

在 Skill 侧，推荐的参数流转关系是：

```text
SkillModifier
  -> 作用于 SkillComponent / SkillInstance
  -> 生成 SkillInstance 当前参数结果或持久字段
  -> 激活时按 snapshot 或非 snapshot 规则写入 StateInstance
  -> StateTree 执行消费 StateInstance 参数
```

## 八、重复应用与冲突策略边界

TCS 当前版本必须明确区分两种完全不同的问题：

1. Skill 重复激活。
2. Buff 运行态合并。

### 8.1 Skill 冲突处理回答的问题

Skill 侧关心的是：

1. 同一个 learned skill 能不能再次激活。
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
| `UTcsSkillInstance` | learned skill、单技能持久数据、用户可派生的数据/复制扩展面 | 一次技能执行态、直接玩法执行逻辑、常驻 StateTree runtime |
| `SkillDef` | 声明技能静态配置，包括 SkillInstanceClass 选择 | 宿主级实例容器、一次 activation 的执行现场 |
| `UTcsSkillComponent` | 宿主级技能数据中心与技能实例集合宿主 | 单次 activation 的 StateTree runtime |

## 十、当前落地状态

这份 UE5-Version 架构稿需要和当前项目状态一起看。

### 已经基本落地的部分

1. State Core / Buff 边界收口。
2. `UTcsStateDefinition` 抽象化方向。
3. `UTcsBuffDefinition` / `UTcsBuffInstance` / `UTcsBuffComponent` 的职责拆分。
4. Buff merge 改为 Buff 自己的语义链。

### 已经冻结方向、但尚未继续扩面实现的部分

1. Skill 仍保持轻骨架。
2. Skill 的主扩展面收敛为“可派生 SkillInstance”。
3. SkillDef 被收敛为 SkillInstance 派生类的配置入口。
4. 具体技能可以在 SkillInstance 派生类中声明持久字段、复制字段与 `OnRep`。
5. 技能执行仍通过独立 StateInstance 进入 `StateComponent`。
6. Skill 更细的复制宿主、接口规范、Task / Evaluator 适配仍留待后续独立议题。

## 十一、当前结论

TCS 当前版本架构可以用一句话概括：

- State Core 负责统一执行态框架。
- Buff 负责效果运行语义。
- Skill 负责拥有态、数据面与激活入口。

再翻译得更具体一点：

1. State 是唯一执行主链。
2. Buff 是挂在 State 上的语义扩展层。
3. SkillInstance 是 learned skill 的数据载体和可派生扩展面，不是执行态。
4. SkillDef 负责配置这个技能应使用的 SkillInstance 派生类。
5. SkillModifiers 归 SkillComponent。
6. 技能自己的持久字段、参数结果和复制策略，归 SkillInstance 派生类。
7. SkillStateTree 消费和写回 Skill 数据，但 SkillInstance 自身不承担玩法逻辑。
8. 技能每次 Activate 时，仍然创建独立 StateInstance 进入 StateComponent 执行。

如果后续设计不偏离这八条，TCS 的 State / Buff / Skill 三者边界仍然可以保持清晰，同时也为未来网络同步和技能自定义数据面预留出合理扩展点。