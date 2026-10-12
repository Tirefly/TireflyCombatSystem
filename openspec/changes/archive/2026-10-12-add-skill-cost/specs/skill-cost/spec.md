## ADDED Requirements

### Requirement: 消耗的运行态（判定 / 提交 / 四个提交点）

`TcsSkill` MUST 提供消耗的**运行态**（`FTcsCostOps`），使 `FTcsCostConfig` 成为真的扣减机制。
**判据的唯一实现住本族函数**：门禁⑤（激活序列）与各提交点共用同一份，**MUST NOT 各写一份**。

- **门禁判定** `CanAffordCost(Subsystem, Unit, Config, OutReason)`——**五类分开处置**：

  | `Policy` | 条件 | 返回 | 日志 |
  |---|---|---|---|
  | `ECP_None` | — | `true` | **无**（正确声明"不消耗"） |
  | `ECP_CustomFragment` | — | 按 `None` 处理 ⇒ `true` | **具名 `Warning`**（含档名） |
  | `ECP_ResourceAttr` | `ResourceAttr` 无效 | `false` | **`Warning`** |
  | `ECP_ResourceAttr` | `Amount < 0` | `false` | **`Warning`** |
  | `ECP_ResourceAttr` | `Amount == 0` | `true` | 无 |
  | `ECP_ResourceAttr` | 单位**未持有**该属性 | `false` | **`Warning`**（与"不足"可区分） |
  | `ECP_ResourceAttr` | 正常 | `基值 >= Amount` | 不足时 `Log`（正常业务路径） |

  - **fail-closed 判据（MUST）**：本仓对"**配置写错**"的处置一律是"**拦住 + 报出**"，
    措辞见 `TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
    ——"**不静默通过**：静默会让'条件写错'表现成'步骤照跑'"。
    **区分点 = "正确声明不消耗"（`ECP_None` / `Amount == 0`，零日志）与"想配消耗但没配清"
    （无效 tag / 负值 / 未持有属性，拦住 + `Warning`）是两件事**；混成一类会把后者**伪装成前者**
    （技能明明配错却照常放出来、**日志还清白**）；
  - **`Amount < 0` MUST 拦**：扣减式是 `Base - Amount` ⇒ 负值会让技能**倒给**资源
    （`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，不是"配置不生效"；
  - **MUST 先确认"持有"再比基值**：底层对**未持有**返回 `0.0`，与"持有且基值为 0"**不可区分**
    ⇒ 不先问持有，就会把"属性配错 / 压根没加"表现成"**付不起**"——两种红字的含义完全不同
    （`TcsEffectAttributeAccess.h:72` 是同一条判据的既有表述）；
  - **MUST NOT ensure**：付不起是**正常业务路径**，不是契约违规（同 `AttributeCompare` 的处置）；
  - **纯查询语义**：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件；
  - **时钟/属性门面不可得 ⇒ 判为不可付**（fail-closed）：资源都没法查就放技能，等于免费放
    ——那是"配置了消耗却静默不生效"的最坏形态。
- **提交** `CommitCost(Subsystem, Unit, Config, OutApplied)`——**唯一的扣减实现**：
  - **扣减载体 = `SetBaseValue` 直写（读基值 → 减 → 写回）**，**MUST NOT 用 `ApplyModifier` 挂修正器**。
    判据三条，缺一都不成立：
    ① **语义**——`SetBaseValue` 的注释原文即"**改写属性基础值（等级成长等宿主升级事务的落点）**"，
    而"消耗资源"正是**永久改变基值**；挂修正器条目表达的是"**某个来源带来的增减**"，
    与"花掉了"不是一回事；
    ② **来源锚点的矛盾**——`ApplyModifier` 路径**必须**给 `Source`，而资源扣减**不可**挂施法运行态来源
    （施法一终结 ⇒ 级联摘除 ⇒ **扣掉的资源回来了**）；挂"长寿命来源"则要**发明一个新发号器**，
    而该来源今天**零撤销消费者**（消耗本就不可撤销）⇒ 正是"零消费者不预建"；
    ③ **`CHAIN-8` 已就同一问题给过判据**：修正器层**效力不等同**于直写基值——
    会被任一 `TAO_Override` **整体抹掉**、参与 `PercentAdd`/`Mul` **复利**、可按来源摘除。
    **"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的**；
  - **MUST NOT 用属性侧 `BeginBatch`/`Commit` 造"事务"包裹**：`D5-10` 的"激活事务"指
    **逻辑上的一次激活**，不是 M2 的属性事务批——包起来会让本次扣减与其它属性改动
    **共享一个今天不存在**的提交点；
  - **提交失败 MUST 被报出**：`SetBaseValue` 自身对"单位未注册 / 属性名为空 / 未持有此属性"
    返回 `false`（含 `ensureMsgf`）⇒ MUST 检查返回值，**MUST NOT** 假设它一定成功
    （否则 `PayCost` 会报成功而资源没扣）。
- **`PayCostForEntry(Subsystem, Unit, EntryHandle)`**——**`ECT_Manual` 档的专用提交入口** + 宿主重试口：
  - **自动档 MUST NOT 走它**（`OnCastStarted` / `OnCastCompleted` / `OnEvent` 直调 `CommitCost`）：
    自动档的提交点**已经知道**自己该不该提交，走本口会多一层"按 Timing 判断"的分支
    ——那正是"同一件事两个入口"；
  - **内部 MUST 重查一次 `CanAffordCost`**（TOCTOU 防护：门禁⑤ 通过到此刻之间资源可能已被花掉），
    且判据与门禁**同一函数**（MUST NOT 各写一份）。
- **四个提交点的落位（MUST 各按其时机，MUST NOT 互相代劳）**：
  - `ECT_OnCastStarted` ⇒ 激活路径的"**门禁⑤ 之后、建 run 之前**"；
  - `ECT_OnCastCompleted` ⇒ 终结路径的 Timing 分派，**只在 `ECFR_Completed` 路触发**
    （被 `ECFR_Interrupted` / `ECFR_Replaced` 终结时**不提交**——该档无"打断时提交"语义）；
  - `ECT_OnEvent` ⇒ 总线订阅（见下一需求）；
  - `ECT_Manual` ⇒ **框架零动作**（含日志）——宿主自己调 `PayCost`。
- **`ECT_OnCastStarted` 的落点判据（MUST）**：① **语义**——"激活成功"的分界线是**建 run**
  （建了 run 才存在"在飞的施法"，也才会有终结路径去回收它）；
  ② **失败面更干净**——建 run 之后提交失败要**回滚一个刚建的 run**（池槽位、账本 `RunHandles` 回填、
  事件广播都要撤），而建 run 前失败只需直接返回，零残留；
  ③ **顺序与判据一致**——门禁⑤ 刚判"付得起"，紧接提交是同一事实的两次使用，TOCTOU 窗口最小；
  - **提交失败 ⇒ 返回 `SAR_CannotAfford` 且 MUST NOT 建 run**（与"门禁没过"同一返回值）；
    日志 MUST 区分二者：**门禁判不足是正常业务**，**提交时失败是真实异常**。

#### Scenario: 不消耗档恒可付且零日志

- **WHEN** 一个 `Policy = ECP_None` 的技能被激活
- **THEN** 门禁⑤ 通过、激活成功，且**不产生任何消耗相关日志**（正确声明"不消耗"）

#### Scenario: 资源足够时按基值扣减

- **WHEN** 某单位持有资源属性（基值 100），一个配 `ECP_ResourceAttr` + `Amount = 30` + `OnCastStarted` 档的技能被激活
- **THEN** 激活成功，且该属性**基值**从 100 变为 70（**MUST NOT** 用 `EvaluateCurrent` 判读——
  它含修正器，与扣减口径不同层）

#### Scenario: 资源不足 ⇒ 具名拒绝

- **WHEN** 基值低于 `Amount`
- **THEN** 返回 `SAR_CannotAfford`，**资源一分未动**、**MUST NOT 建 run**

#### Scenario: 无效 ResourceAttr / 负 Amount 一律 fail-closed

- **WHEN** 分别配 `ResourceAttr` 无效、`Amount < 0`
- **THEN** 两者都判**不可付**并留 `Warning`——**MUST NOT** 静默按"恒可付"放过

#### Scenario: 未持有属性与"基值为 0"可区分

- **WHEN** 某技能配了一个该单位**未持有**的属性作为 `ResourceAttr`
- **THEN** 判定为不可付并留 `Warning`（指明"未持有"），**MUST NOT** 表现成"资源不足"——
  两者是含义完全不同的红字

#### Scenario: 提交点重查（TOCTOU 防护）

- **WHEN** 门禁⑤ 通过之后、提交点之前，资源被另一条路径花光
- **THEN** 提交点**重查**后拒绝，且**不产生半提交状态**（资源与账本都不留下中间态）

#### Scenario: `OnCastStarted` 档提交失败 ⇒ 不建运行态

- **WHEN** 一个配 `ECT_OnCastStarted` 的技能在**提交时**失败（例如属性在门禁后被移除）
- **THEN** 返回 `SAR_CannotAfford`，且**没有半建的 run**（池占用数不变、账本 `RunHandles` 未回填）

#### Scenario: 重复 `PayCost` 第二次被拒且零半提交

- **WHEN** 资源只够付一次，连续调用两次 `PayCost`
- **THEN** 首次成功、**第二次被拒**（重查未过），且资源余额**保持不变**（不出现负值或部分扣减）

### Requirement: 消耗事件与 `OnEvent` 中继

- `TcsSkill` MUST 提供 `TcsEvent.Cast.CostApplied` + `FTcsCostEventPayload`
  （`{EntryHandle, Unit, DefTag, ResourceAttr, Amount}`）；
- **`D5-16` 的 `SkillId` 字段 MUST 用 `DefTag` 取代**（**MUST NOT 为它发明一个类型**）：
  `D5-9` 修订后 `EffectiveDefId` 已整体移除；
- **命名 MUST 去掉 `On` 前缀**（`D5-16` 原文写 `OnCostApplied`）：`TcsEvent.Cast` 根下既有四枚
  **零 `On` 前缀**，按同族对称取名；
- **导出宏**：MUST 为 `extern TCSSKILL_API FNativeGameplayTag`（裸 `extern` 会 `LNK2001`）；
- **`CanAfford` 失败 MUST NOT 发本事件**（`D5-16` 明文："CanAfford 失败不发——`TryActivate` 返回值覆盖"）：
  门禁拒绝面已有 `SAR_CannotAfford` 这个具名读数，再发一枚"没扣成"的事件纯属噪音；
- **只有真的扣了才发**（`Amount <= 0` / `Policy != ECP_ResourceAttr` ⇒ 零动作、零事件）；
- **`ECT_OnEvent` 档的中继**（`UTcsCostEventRelay`，`UTcsEventHandler` 派生——裁决 2a）：
  - **一个共享 CDO + 一张等待登记簿**：订阅数 = **不同 tag 数**（通常 1~3），**不是条目数**
    ——每条目一个实例会造出 N 个行为完全相同的对象；
  - **订阅寿命（MUST，本档最危险的一处）**：条目被撤销 / 单位被注销 MUST **退订**，
    否则回调里的条目句柄悬空 = **use-after-free**；某个 tag 下**最后一个**等待者被摘掉时 MUST 退订；
  - **`TriggerEventTag` 无效 ⇒ 该档不成立** ⇒ 具名 `Warning` + **回退 `ECT_OnCastStarted`**。
    **回退 MUST NOT 取"什么都不做"**：那会让配错的技能**永远不扣费**（免费放技能），
    是比"提前扣"严重得多的静默漏洞；
  - **处理 MUST 两段式**：回调内会经 `SetBaseValue` 触发 M2 管线重算并**广播属性变更事件**
    ⇒ 订阅者可在其中重入、甚至撤销条目 ⇒ **先拷贝一份待处理句柄，再逐个处理**；
  - **到点后 MUST 走 `PayCostForEntry`**（自带 `CanAffordCost` 重查＝TOCTOU 防护），
    **MUST NOT** 在中继里另写一份判据。

#### Scenario: 提交成功后发 `CostApplied` 且载荷逐项吻合

- **WHEN** 一个 `ECP_ResourceAttr` 档技能成功扣减
- **THEN** 恰发 **1** 次 `CostApplied`，其 `EntryHandle` / `Unit` / `ResourceAttr` / `Amount` 与本次扣减一致

#### Scenario: 扣减失败不发事件

- **WHEN** 门禁⑤ 或提交点判定"付不起"
- **THEN** **不发** `CostApplied`（拒绝面由 `SAR_CannotAfford` 覆盖）

#### Scenario: `OnEvent` 档激活时不扣、事件到达才扣

- **WHEN** 一个配 `ECT_OnEvent` 的技能被激活
- **THEN** 激活当时资源**不变**；直到 `TriggerEventTag` 指定的事件被发布，才发生扣减并发 `CostApplied`

#### Scenario: `OnEvent` 档的事件标签无效 ⇒ 回退而非免费

- **WHEN** 一个配 `ECT_OnEvent` 但 `TriggerEventTag` **无效**的技能被激活
- **THEN** 留**具名** `Warning`，并按 `ECT_OnCastStarted` **当场提交**（**MUST NOT** 什么都不做
  ——那会让配错的技能永远免费）

#### Scenario: 撤销技能后订阅已注销

- **WHEN** 一个配 `ECT_OnEvent` 的技能其条目被撤销，随后该事件被发布
- **THEN** **不再发生**任何提交（订阅已注销）——MUST NOT 出现"回调里拿着已撤销条目的句柄"（use-after-free）

#### Scenario: `CustomFragment` 档具名降级且零扣减

- **WHEN** 某技能配 `Policy = ECP_CustomFragment`
- **THEN** 判定为可付、**零扣减**，并留**具名** `Warning`（含档名）——**MUST NOT** 静默当作 `ECP_None`
