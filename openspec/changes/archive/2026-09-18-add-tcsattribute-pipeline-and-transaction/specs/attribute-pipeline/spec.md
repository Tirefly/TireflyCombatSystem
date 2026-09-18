## ADDED Requirements

### Requirement: 五带折叠纯函数（单份）

`TcsAttribute` MUST 以**一个纯函数**提供属性/参数/流程属性三处共用的带式折叠（D5-5 v3："运算符相同则实现必须单份"），住 `Public/Attribute/TcsAttributeBandFold.h`（**Public**——M5 参数链与 TcsDamage 流程属性容器要复用）：

- 签名 `FoldTcsAttributeBands(double BaseValue, TConstArrayView<FTcsAttributeBandEntry> Entries, ETcsAttrOverrideTieBreak OverrideTieBreak = OTB_Max)`，条目 `FTcsAttributeBandEntry{ ETcsAttributeOp Op; double Value; int32 OverridePriority; }`——第三个参数带默认值，**调用方无"同优先级策略"概念时可两参调用**（M5 参数链 / TcsDamage 流程属性容器即此情形）；
- **无 Override 时**：`Final = ((BaseValue + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；
- **存在 Override 时**：按"`OverridePriority` 大者胜 → 同优先级按 `OverrideTieBreak` 策略比较数值 → 有符号值大者"三级选出**一条**直接作为结果（`FlatAdd` 与其余带**一并被覆盖**——"最强覆盖生效"语义）；`OverridePriority` 仅对 `TAO_Override` 生效（其余带忽略，折叠 MUST NOT 因它改变任何其它带的算法）；
- **带序唯一真相在 `Op`**（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30）——`SortKey` 不参与折叠；组内与带间**顺序无关**（同一多重集的任意排列结果一致——含覆盖带：裁决关系 MUST 为全序，不得出现"同分打平、赢家取决于谁先被遍历到"）；
- **空集返回 `BaseValue`**；函数 MUST 纯函数（无引擎副作用、无时间/随机依赖，D0-1）——MUST NOT 另有并列的同义实现（三处消费者一律调用本函数）。

#### Scenario: 五带聚合顺序无关

- **WHEN** 以同一组条目（含 Add/PercentAdd/Mul/FlatAdd）按两种不同排列调用折叠
- **THEN** 两次结果完全一致，且等于 `((Base + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`

#### Scenario: Override 覆盖一切

- **WHEN** 条目中同时存在 Override（值 30）与 Add/PercentAdd/Mul/FlatAdd
- **THEN** 结果为 Override 组的胜出值（30），其余带（含 FlatAdd）不参与

#### Scenario: 覆盖带裁决与顺序无关

- **WHEN** 同一组覆盖条目（含不同优先级、以及"策略下幅度打平"的一对）按两种排列调用折叠
- **THEN** 两次选出同一条赢家（优先级 → 策略 → 有符号值的全序保证无平局歧义）

#### Scenario: 空集与初值

- **WHEN** 条目为空
- **THEN** 返回 `BaseValue`（参数链场景即"该键参数行的求值结果"，无参数行则为 0）

#### Scenario: SortKey 不改变结果

- **WHEN** 同一条目把 `SortKey` 置成任意值（折叠函数不接收该字段）
- **THEN** 结果不变（带序只由 `Op` 决定）

### Requirement: 按需重算与脏标记

管线 MUST 提供 `EvaluateCurrent(Unit, FTcsAttributeName)`（动词 = 求值，不是 `Resolve`——不从标识符取对象）：

- 目标属性**不脏** → 直接返回 `CachedCurrent`（零重算）；
- 目标属性**脏** → 重算：收集该属性的修正器（按 `Op` 分桶）→ 求值各操作数 → 折叠 → 值域收口 → 写回 `CachedCurrent` → 清脏标记；
- **管线是 `CachedCurrent` 的唯一生产者**（定义期初值与任何外部写入都不算）；`BaseValue` 变更、修正器挂/摘、依赖属性变更、**`AddAttribute` 新建实例（含"解冻时实例已带脏标记"）** MUST 标脏；
- **新建即脏的理由（2026-09-18 增补）**：结算会被推迟到"批外读取或提交"这一刻——此时单位上的属性图通常已搭好（动态边界指向的引用属性、同期挂上的修正器都在位），**定义/添加顺序不影响结果**；若在 `AddAttribute` 内立即结算，动态边界会读到尚未添加的引用属性（求值 0）→ 把原值钳成 0 且无人再标脏（静默错值）。代价：首次结算可能广播一次（旧值 = 原始基础值）——结算前的缓存值不对外承诺；
- 属性未定义时 MUST 返回 0 且不崩溃（与读侧契约一致）。

#### Scenario: 干净属性零重算

- **WHEN** 属性刚重算完（不脏）再次 `EvaluateCurrent`
- **THEN** 返回值不变且不触发收集/折叠路径（以装置计数或缓存值观察）

#### Scenario: 脏则重算并清脏

- **WHEN** 挂上一个 `Add` 修正器（标脏）后 `EvaluateCurrent`
- **THEN** 返回新值（旧值 + 修正量）、`CachedCurrent` 更新、`bDirty` 复位

### Requirement: 值域收口

重算 MUST 在折叠之后按实例的 `ValueDomain` 收口（D2-4/D2-6）：

- `AVD_Clamp`（默认）：越界钳到边界；
- `AVD_Wrap`：按值域跨度循环回卷——**跨度 = 两侧边界齐备且 `Max > Min` 时的 `Max − Min`**；跨度未成立（任一侧 `ABM_None`，或 `Max ≤ Min`）时 MUST 不回卷、返回聚合原值（行为确定、不 ensure；"Wrap 却没给跨度"属作者侧配置错误，不在热路径拦截，留给 M8 的定义校验矩阵）；
- `AVD_Custom`（逃逸位）：值域策略接口（`IValueDomainPolicy`）**不在 R3 范围**——命中时 MUST 记录显式提示（ensure 或警告日志，不得静默），并按 `AVD_Clamp` 收口（确定性优先于未实现策略）；
- 边界三态：`ABM_None` 该侧不限；`ABM_Static` 用静态值；`ABM_Dynamic` **先按本管线求值目标属性的当前值**再作边界（"HP ≤ MaxHP"形态）；`ABM_Dynamic` 的依赖边同样走"读即登记"（见下一条）；
- 自引用（属性以自身为边界）已在属性添加期拒绝（Task 4），本管线不再重复拦截。

#### Scenario: Clamp 收口

- **WHEN** 聚合结果 150、边界 `ABM_Static(0)..ABM_Static(100)`、`AVD_Clamp`
- **THEN** 结果为 100

#### Scenario: Wrap 收口

- **WHEN** 聚合结果 150、边界 `ABM_Static(0)..ABM_Static(100)`、`AVD_Wrap`
- **THEN** 结果为 50（按跨度 100 循环）

#### Scenario: Wrap 跨度未成立时退化为原值

- **WHEN** 聚合结果 150、仅上界 `ABM_Static(100)`（下界 `ABM_None`）、`AVD_Wrap`
- **THEN** 返回 150（不回卷、不钳制、不 ensure）——"只给一侧"没有跨度可言，框架不替作者猜一个 0

#### Scenario: Custom 未实现时显式提示并回落

- **WHEN** 实例 `ValueDomain = AVD_Custom`（策略接口未实现）
- **THEN** 按 Clamp 收口，且**同时**有显式提示（ensure 或警告日志）——不静默按某策略处理

#### Scenario: 动态边界先求值

- **WHEN** 属性 `Health` 的上边界为 `ABM_Dynamic(MaxHealth)`，`MaxHealth` 当前值 200，聚合结果 250
- **THEN** 收口结果为 200（动态边界按管线求值后再钳制）

### Requirement: 依赖登记与成环拒绝

管线 MUST 支持属性间依赖（D2-3），机制为**求值期读即登记**：

- 求值过程中读取其他属性的 `Current`（含 `OPK_AttributeScaled` 操作数的 `Coefficient × Current(Attribute)`、以及 `ABM_Dynamic` 边界属性的求值）→ MUST 登记一条依赖边（`被读者 → 读者`）；
- 依赖边登记后，**被读属性变化 MUST 把读者标脏**（派生属性零宿主维护："1 力量 = 2 攻击力"这类配置不需要宿主手工同步）；
- 环检测 MUST 用 **Tarjan SCC**（严格模式）：成环 MUST 拒绝该边并 ensure 提示（不静默重算到死）；
- **重算轮数上限 MUST 与环判定解耦**（旧 TCS 因"8 轮上限"把深链误判为环——本实现不得再以轮数作为环判据）；
- 图 MUST 每轮重建不缓存（脏标记已限流；热了再优化）。

#### Scenario: 读即登记使派生属性自动更新

- **WHEN** `AttackPower` 的修正器声明 `OPK_AttributeScaled(Strength, 2.0)`，先求值 `AttackPower`，再改 `Strength` 并求值 `AttackPower`
- **THEN** 第二次结果反映新的 `Strength`（读即登记生效，无需宿主干预）

#### Scenario: 成环被拒且求值有限

- **WHEN** 构造 A 依赖 B、B 依赖 A 的配置并求值
- **THEN** 成环被检测、**刚登记的那条边被拒绝**（读者在本次重算中使用被读者的上一缓存值）、ensure 命中，求值返回有限值且**不进入无限重算**（"拒绝该边"是设计既定语义；不是"整条属性零写入"）

### Requirement: 变更广播

重算产生**实质变化**时 MUST 经总线立即通道广播属性变更事件：

- 比较阈值 **epsilon = 1e-5**：`|NewValue - OldValue| <= 1e-5` 视为未变，**不广播**；
- 事件形状 `FTcsAttributeChangedEvent{ Unit, Attribute, OldValue, NewValue }`（核心词汇 FStruct，走 TcsCore 总线；`Reason` 字段待有消费者再加）——语义是**当前值（对外可读值）变了**，不是"某次写操作发生了"：改基值 / 挂摘修正器 / 依赖连带变化都只在**当前值确实动了**时产生这一条事件，因此它 MUST NOT 被当作"基础值变更日志"（基础值改了但被值域收口吃掉 = 对外没变 = 不广播，与"未变不广播"一致）；
- **事件 Tag 为原生 Tag `Tcs.Event.Attribute.ValueChanged`**（常量 `Tag_TcsEvent_Attribute_ValueChanged`，TcsAttribute 内声明——订阅方按它过滤）：命名公约 **`Tcs.Event.<域>.<事件名>`**（域段是真层级节点，同域后续事件如属性上线/下线挂 `Tcs.Event.Attribute` 下）；**原生**而非项目 Tag 表（插件自足、项目漏配不会静默丢事件）；由事件所属模块声明（TcsCore 不持战斗域词汇）。注意**原生订阅路径当前为精确匹配**——父标签订阅需等原生层级匹配落地（已列总线输入），期间原生消费者按叶子逐条订阅；BP/CS 动态层已支持部分匹配；
- 广播时机由事务控制（提交尾、行内 flush）——见 `attribute-transaction` 能力；同一提交内同一属性**最多广播一次**。

#### Scenario: 变化才广播

- **WHEN** 挂一个把属性值从 100 改成 100.000001（差 < 1e-5）的修正器，然后挂一个改成 110 的修正器（各自提交）
- **THEN** 第一次提交不广播、第二次提交广播一次（`OldValue=100`、`NewValue=110`）

### Requirement: 管线可见性与调用纪律

`FTcsAttributePipeline` 的**声明 MUST 住 `Public/Attribute/`**（跨模块消费者需要直调它驱动某单位的求值/事务，2026-09-18 用户拍板从 Private 移出），**实现 MUST 留 `Private/`**（`.cpp` 不随头公开）；类 MUST 带模块导出宏（有 out-of-line 成员，消费方否则 LNK2019）。

调用纪律（MUST）：
- **推荐路径是门面** `UTcsAttributeSubsystem`（转发同名入口）——游戏逻辑一律经门面，门面是唯一入口；
- 直调管线是**逃生口**（离屏/工具侧求值、无门面引用等场景）：`Read` / `Write` 两区的入口可直接调用；
- 两条路径操作**同一份状态**——MUST NOT 演化成两条记账路径（"管线是 `CachedCurrent` 唯一生产者"不得被破坏）；
- 机制面（`private:` 段：重算内核 / 依赖登记 / 求值栈等）**不构成消费契约**，调用方 MUST NOT 依赖，其变更不另行通知；
- **生命周期**：管线实例归门面所有、引用绑定该门面——调用方 **MUST NOT 跨帧持有**，或在其门面销毁后使用（World 切换 / `Deinitialize` 之后即悬空）。

#### Scenario: 跨模块消费者可直接驱动求值

- **WHEN** 另一模块持有某 World 的属性门面，并取得其管线引用
- **THEN** 可直接调用 `EvaluateCurrent` / `ApplyModifier` / `BeginBatch` / `Commit` 等公开入口，结果与经门面调用**逐位一致**（同一条记账路径）

#### Scenario: 消费者拿不到机制面

- **WHEN** 消费者尝试调用重算内核、依赖登记或求值栈等内部函数
- **THEN** 编译期不可见（`private:` 段）——实现细节不构成对外契约
