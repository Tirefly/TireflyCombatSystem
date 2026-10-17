## ADDED Requirements

### Requirement: 关系行与检查器形状

@TcsState@（共享层）MUST 提供关系行的**纯值类型** @FTcsRelationRow@ 与**静态函数族** @FTcsStateRelation@，供两个域共用：

- @FTcsRelationRow@ MUST 为纯值 @USTRUCT@，五字段：@FGameplayTag AppliesToTag@ + @FGameplayTagContainer RequiredTags@ / @ForbiddenTags@ / @TagsToBlock@ / @TagsToCancel@；
- 检查器形参 MUST **全是值**（主体词集 @FGameplayTagContainer@ + 客体词集 + 行视图），**MUST NOT** 出现任一域的 Def 类型或运行态记录类型——@TcsState@ 是共享层，认识域类型即制造包含环（同 @FTcsEntityAdmission@ 的既有处置：共享层定义它认得的**最小公共字段集**，域侧负责填）；
- 判定 MUST 只走 @FGameplayTag::MatchesTag@（**层级匹配**：父词命中子词，反向不成立）与 @FGameplayTagContainer::HasAny@；**MUST NOT 引入 @FGameplayTagQuery@**（Lyra 以同一需求证明「容器 + 层级匹配」已足够）；
- @AppliesToTag@ MUST 同时承载「具体定义」与「类别」两层含义，靠**层级匹配**统一（填具体词只作用它自己、填父词覆盖一族）——MUST NOT 为此发明通配语法；
- **两个方向 MUST 分开**：@RequiredTags@ / @ForbiddenTags@ 约束**自己**（受动·前置），@TagsToBlock@ / @TagsToCancel@ 约束**目标**（施动）。两者可从相反一侧表达同一互斥，**两条声明路径都合法**（选哪侧 = 作者声明权的选择，不是冗余）。

#### Scenario: 父词覆盖一族（层级匹配）

- **WHEN** 一条关系行的 @AppliesToTag@ 填某个父词，而客体词集中有它的子词
- **THEN** 该行命中（父词命中子词）；反向（@AppliesToTag@ 填子词、客体填父词）**不命中**

#### Scenario: 关系字段全空时零行为变化（阴性对照）

- **WHEN** 一个定义的五字段全为空容器（既有资产的现状）
- **THEN** 检查器的每一条判定都不命中 ⇒ 施加 / 取消 / 预防三处挂点**零行为变化**，与检查器落地前的读数**逐字一致**



### Requirement: 关系门的检查时机（apply 反向查找）

@ForbiddenTags@ 与 @TagsToBlock@ 可从**相反一侧**表达同一互斥 ⇒ 若互斥**只由施动侧声明**（@A.TagsToBlock@ 含 B 的主体词），则 B 的 apply 门禁**必须反向查找**才能拒绝。

- 本能力 MUST 取 **apply 时反向查找**（「是否存在存活实例的 @TagsToBlock@ 覆盖我」），**MUST NOT** 要求作者双向声明；**判据** = 作者负担是**永久**成本（每条互斥都要写两遍、漏写一侧即**静默失效**），而扫描是 @O(该实体在册实例数)@ 的一次遍历——与 apply 本身的量级同阶；单向声明即生效消除了「漏写一侧」这一整类静默缺陷；
- 扫描域 MUST 限于**该实体**的在册实例（该实体桶），**MUST NOT** 做全局扫描；
- 关系门判定 MUST **排在共存决策之前**（先过关系门、再看叠层）——判据：门禁是「能不能存在」，叠层是「以什么形态存在」，前者不成立时后者无意义；
- 拒绝 MUST 返回具名结果 + 具名 @Log@，且 **MUST NOT 产生任何事件 / MUST NOT 改动实例数**（拒绝路径零副作用）。

#### Scenario: 单向声明即生效（反向查找）

- **WHEN** 仅 @A.TagsToBlock@ 含 B 的主体词（B **未**声明 @ForbiddenTags@），B 后到并尝试 apply
- **THEN** B 被拒绝（反向查找命中 A 的存活实例）——**MUST NOT** 出现「A 在跑、B 也起来了」的窗口

#### Scenario: 拒绝路径零副作用

- **WHEN** 关系门拒绝一次施加
- **THEN** 该实体的在册实例数**不变**、**零事件**广播、账本与触发行**未改动**（只有一条具名 @Log@）



### Requirement: 取消与预防两个挂点

- **@TagsToCancel@（终止）**：施加成功路径 MUST 按它逐条移除既有实例，原因传 @EBuffRemoveCause::EBRC_Cancelled@——**该枚举首次有内建产生者**；复用既有 @RemoveState@ 撤销链，MUST NOT 另写一套移除逻辑；
- **@TagsToBlock@（预防）**：声明方在跑期间，命中的目标 MUST **无法开始且保持不能开始**。**两处时机 MUST 都覆盖**：① 目标尝试开始时（apply 门禁）；② **声明方开始时就已存在的目标**必须被**即时阻断**——只做 ① 会留下「声明方在跑、目标也已在跑」的窗口；
- 两处挂点 MUST 与 @TagsToCancel@ **区分清楚**：@TagsToCancel@ 是「把已在跑的**终止**掉」（事后），@TagsToBlock@ 是「让它**不能开始**」（事前）；同一目标可同时被两者覆盖，此时**取消先于阻断生效**（目标已被移除，无需再阻断）。

#### Scenario: 后到目标被拒（事前阻断）

- **WHEN** A 在跑（其 @TagsToBlock@ 覆盖 B），B 随后尝试 apply
- **THEN** B 被拒绝、实例数不变；**不产生任何 B 的生命周期事件**

#### Scenario: 已在跑目标被即时阻断（覆盖第二处时机）

- **WHEN** B 已在跑，随后 A 开始（A 的 @TagsToBlock@ 覆盖 B）
- **THEN** B 被即刻阻断（不再是有效在跑态）——**MUST NOT** 出现「A 在跑、B 也在跑」的存续窗口

#### Scenario: 取消产生 Cancelled 原因读数

- **WHEN** A 的 @TagsToCancel@ 覆盖一个正在跑的 B，施加 A
- **THEN** B 被移除、其实例数 -1，且移除事件载荷的 @Cause@ 读数 = @Cancelled@（复查：这是该枚举**首次**由框架产生，不是调用方传入）



### Requirement: 填法即策略（两种抢占配置）

同一互斥由**填哪几个字段**决定谁赢，**不需要 @Priority@ 数字**（@Priority@ 不属关系字段族，归槽位竞争表）：

| 配置 | 净语义（B 在时施加 A） | 谁赢 |
|---|---|---|
| 只 @A.TagsToCancel@ 含 B | A 上、B 被清 | **后到者赢** |
| @A.ForbiddenTags@ 含 B **+** @A.TagsToCancel@ 含 B | A 被拒（B 在）⇒ 只能等 B 自然消失 | **在位者赢** |

两种配置**同真且都合法**——它们是**两种不同的抢占策略**，不是一处错误。本能力 MUST NOT 对「@TagsToCancel@ 与 @ForbiddenTags@ 同时命中同一目标」报错（该组合**正是**在位者赢的表达方式）。

#### Scenario: 后到者赢（仅 TagsToCancel）

- **WHEN** 只配 @A.TagsToCancel@ 含 B，B 在跑时施加 A
- **THEN** A 施加成功、B 被移除（@Cancelled@）——**后到者赢**

#### Scenario: 在位者赢（ForbiddenTags + TagsToCancel）

- **WHEN** 同时配 @A.ForbiddenTags@ 与 @A.TagsToCancel@ 各含 B，B 在跑时施加 A
- **THEN** A 被**拒绝**（B 仍在跑）——**在位者赢**；MUST NOT 因两字段同时命中而报错



### Requirement: 关系表的作者期校验

两个 Def 的 @IsDataValid@ MUST 在既有规则之外追加两条**错误**：

- ① @A.TagsToCancel@ 与 @B.TagsToCancel@ **互相包含** ⇒ **双向强夺**、无确定赢家 ⇒ Error。**判据**：两条声明各自都是合法的独占策略，但**同时成立**时结果依赖施加顺序、作者无法预测；
- ② @A.ForbiddenTags@ **含自己的主体词**（自指）⇒ 自我否定的门禁、**永不可满足** ⇒ Error。**判据**：该行不可能被满足，属配置错误而非策略选择；
- **原「@A.ForbiddenTags@ 含 B + @A.TagsToCancel@ 含 B ⇒ 自相矛盾」这条规则 MUST NOT 存在**——按本能力的《填法即策略》需求，该组合是**在位者赢**的合法表达，报错即误报。

#### Scenario: 双向强夺被报错

- **WHEN** 构造两个定义，各自的 @TagsToCancel@ 互相包含对方的主体词
- **THEN** @IsDataValid@ 各报一条错误（指向具体字段），资产判为 @Invalid@

#### Scenario: 自指 ForbiddenTags 被报错

- **WHEN** 某定义的 @ForbiddenTags@ 含它自己的主体词
- **THEN** @IsDataValid@ 报出一条错误（该门禁永不可满足）

#### Scenario: 在位者赢组合不被报错（阴性对照）

- **WHEN** 某定义的 @ForbiddenTags@ 与 @TagsToCancel@ 同时含同一目标词
- **THEN** @IsDataValid@ **不报错**——这是合法策略，MUST NOT 误报

