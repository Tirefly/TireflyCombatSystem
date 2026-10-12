# skill-cost Specification

## Purpose
定义技能消耗的**数据形状与运行态**：策略族 `ETcsCostPolicy` / `ETcsCostTiming` / `FTcsCostConfig`（形状），`FTcsCostOps` 的判定与提交（**fail-closed 的四类配置边界**、按**基值**直写的扣减、TOCTOU 重查）、**三个提交点**（激活期 / 终结期 / 手动）各自的落位判据，以及 `TcsEvent.Cast.CostApplied`——让「付不起」成为**具名、可判、可预查**的运行事实，而不是一个恒过的占位分支。
**框架只为"施法这个事务自身的边界"提供时机档**；"由某个特定项目事件触发消耗"属**项目词汇**，走 `ECT_Manual` 档 + `PayCost` 由宿主实现（`ECT_OnEvent` 档已于 2026-10-12 裁定删除，MUST NOT 补回）。

## Requirements

### Requirement: 消耗配置的数据形状（策略与三档时机）

`TcsSkill` MUST 提供消耗配置的**纯数据**形状（零行为）：

- `UENUM(BlueprintType) ETcsCostPolicy`（**三档**）：`ECP_None`（**默认**：恒可付、零日志）/
  `ECP_ResourceAttr`（走 M2 属性账本）/ `ECP_CustomFragment`（**本轮具名未实现**）；
- `UENUM(BlueprintType) ETcsCostTiming`（**三档**）：`ECT_OnCastStarted`（**默认**）/
  `ECT_OnCastCompleted` / `ECT_Manual`；
- `USTRUCT(BlueprintType) FTcsCostConfig{ ETcsCostPolicy Policy; ETcsCostTiming Timing;
  FGameplayTag ResourceAttr; FTcsParamValue Amount; }`。

**`Amount` MUST 是 `FTcsParamValue`（参数源载体）**（2026-10-12 用户裁定，台账 `COST-1` 闭合）：

- **判据 = 一个 `double` 同时承担了三件事，而三者都需要"可配置"**：
  ① **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"）；② **按来源配数值**（`HostDelegate` / `ParamRef`）；
  ③ **减免**——由契约键 `TcsStateParam.CostAmount` 上的修正器折叠给出。
  ⇒ 裸 `double` 三者都做不到；
- **`FTcsParamValue` 的默认构造装 `Literal(0)`** ⇒ **与换型前的 `double Amount = 0.0` 语义逐字相同**
  （既有资产零迁移；全仓 `.uasset` 扫该字段零命中）；
- **求值 MUST 在门禁⑤ 与每个提交点各做一次**，**MUST NOT** 求一次后跨"门禁 → 提交"复用
  （那会把 TOCTOU 窗口重新打开）；
- **求值结果 MUST 钳下界到 0**，但**调用方 MUST 能同时拿到"未钳的基数"**——
  判据见下方"负基数与减免过度是两件事"。

**`ETcsCostTiming` MUST NOT 含 `OnCastInterrupted` 档**——两条硬判据（非偏好）：

1. `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
   ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
2. 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**（那就是
   `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。

**`ETcsCostTiming` MUST NOT 含 `OnEvent` 档**（2026-10-12 用户裁定移除，**MUST NOT 补回**）：

- **判据**：框架只能认识"**施法这个事务自身的边界**"（开始 / 完成）——这两个时点在**每个项目里
  含义都相同**；而"**某个特定事件**"（挨打时付费 / 连击到第 3 段付费 / 血量低于 30% 付费……）
  是**项目词汇** ⇒ 框架内置它等于**让框架去猜项目的玩法**，且它提供的只是一个"等某个 tag"的
  空槽位（"这个 tag 是什么、什么时候发"全在宿主手里）⇒ **框架提供了一个自己解释不了、
  也无法校验的档位**。这与本仓主判据「**凡是需要宿主词汇的内置策略，框架一律不提供**」直接冲突；
- **宿主实现成本极低，且不需要任何新框架面**：监听自己关心的事件 → 调 `PayCost`（公开口）
  ——那正是 `ECT_Manual` 档 + `PayCost` 的既有组合；
- **伴随面 MUST 一并删除**（否则留下悬空契约）：`FTcsCostConfig::TriggerEventTag` 字段、
  `UTcsCostEventRelay` 整个类、门面的 `GetCostEventRelay()` 与 `CostEventRelay` 成员、
  以及三处撤销路径（`Revoke` / `RevokeBySource` / `UnregisterUnit`）的中继退订。
  **判据 = 「零消费者不预建」的反向使用**：消费者归零 ⇒ MUST 删掉；
- **`CostApplied` 事件本身 MUST 保留**（它与档位无关：任何提交路径都发它）；
- **`ETcsCooldownTiming` 不受本裁定影响**——冷却侧本来就没有 `OnEvent` 档
  （其四档是 `OnCastStarted` / `OnCastCompleted` / `OnCastInterrupted` / `Manual`）。

**`ResourceAttr` / `Amount` 的失效面 MUST fail-closed**（判据 = 本仓既有惯例，
`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
三处措辞几乎逐字相同："**配置写错**"与"**正确声明不消耗**"是两件事，混成一类会让前者
**表现成一切正常**、日志还清白）：

- `ECP_None` ⇒ 恒可付、**零日志**（正确声明"不消耗"）；
- `ECP_ResourceAttr` + `ResourceAttr` **无效** ⇒ **恒不可付** + `Warning`；
- `ECP_ResourceAttr` + **`Amount` 的求值结果为负** ⇒ **恒不可付** + `Warning`
  （负值会让技能**倒给**资源）；
- `ECP_ResourceAttr` + `Amount` 的求值结果为零 ⇒ **恒可付**、零日志（"消耗 0 点"就是不消耗）。

**★ 负基数与减免过度 MUST 是两件事（装置拒绝面检查 `d` 实测抓到的设计缺陷）**：

| 情形 | 判据依据 | 语义 | 处置 |
|---|---|---|---|
| **负基数**（作者把 `Amount` 填成 -50） | **未钳的基数** | **配置错误**、"倒给资源"的**漏洞形态** | **拦住** + `Warning` |
| **折叠后转负**（基数 30、减免 -100） | **钳后的生效量** | **减免过度** = 合法的"免费" | **钳 0**、**不报错** |

**判据（MUST）**：`ResolveEffectiveCost` MUST 输出**未钳的基数**（出参），
使上述两条判据各得其数。**MUST NOT** 只给一个钳过的结果
——那会让负基数被静默抹成 0、其守卫永远不触发（**该缺陷已在实施期被装置当场报出**）。

#### Scenario: 默认档为不消耗且恒可付

- **WHEN** 某技能的 `FTcsCostConfig` 取默认值
- **THEN** `Policy == ECP_None`、`Amount` 的内层源为 `Literal` 且值为 `0`、`ResourceAttr` 无效；
  该技能恒可付且**零日志**

#### Scenario: 三档时机且无打断档、无事件档

- **WHEN** 枚举 `ETcsCostTiming` 的全部取值被检查
- **THEN** 恰为 `OnCastStarted` / `OnCastCompleted` / `Manual` **三档**；
  **不含** `OnCastInterrupted`（判据：`D5-4` 已砍返还）也**不含** `OnEvent`
  （判据：需要宿主词汇的内置策略，框架一律不提供）

#### Scenario: 配置错误一律 fail-closed

- **WHEN** 一个"想配资源消耗但 `ResourceAttr` 未填 / `Amount` 的求值结果为负"的技能被判定
- **THEN** 判定为**不可付**并留**具名 `Warning`**——**MUST NOT** 静默按"不消耗"放过

#### Scenario: 负基数被拦而减免过度被钳（两件事可区分）

- **WHEN** 分别构造 ① `Amount` 字面量 = `-50`、② `Amount` 字面量 = `30` 且
  `CostAmount` 键上挂 `FlatAdd = -100`
- **THEN** ① 判**不可付** + `Warning`（负基数是漏洞形态）；
  ② 生效量**钳到 0**、判**恒可付**、**零红字**（减免过度是合法的"免费"）；
  且两者**不得因同一个收口而互相掩盖**（判据依赖"未钳的基数"出参）

#### Scenario: 自定义片段档具名降级

- **WHEN** 某技能配 `Policy = ECP_CustomFragment`
- **THEN** 判定为可付、**零扣减**，并留**具名** `Warning`（含档名）——**MUST NOT** 静默当作 `ECP_None`

### Requirement: 消耗的运行态（判定 / 提交 / 三个提交点）

`TcsSkill` MUST 提供消耗的**运行态**（`FTcsCostOps`），使 `FTcsCostConfig` 成为真的扣减机制。
**判据的唯一实现住本族函数**：门禁⑤（激活序列）与各提交点共用同一份，**MUST NOT 各写一份**。

- **门禁判定** `CanAffordCost(Subsystem, Unit, Config, OutReason)`——**七类分开处置**：

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
    ③ **`CHAIN-8` 已就同一问题给过判据**：修正器层**效力不等同**于直写基值——会被任一
    `TAO_Override` **整体抹掉**、参与 `PercentAdd`/`Mul` **复利**、可按来源摘除。
    **"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的**；
  - **MUST NOT 用属性侧 `BeginBatch`/`Commit` 造"事务"包裹**：`D5-10` 的"激活事务"指
    **逻辑上的一次激活**，不是 M2 的属性事务批——包起来会让本次扣减与其它属性改动
    **共享一个今天不存在**的提交点；
  - **提交失败 MUST 被报出**：`SetBaseValue` 自身对"单位未注册 / 属性名为空 / 未持有此属性"
    返回 `false`（含 `ensureMsgf`）⇒ MUST 检查返回值，**MUST NOT** 假设它一定成功
    （否则 `PayCost` 会报成功而资源没扣）。
- **`PayCostForEntry(Subsystem, Unit, EntryHandle)`**——**`ECT_Manual` 档的专用提交入口** + 宿主重试口：
  - **自动档 MUST NOT 走它**（`OnCastStarted` / `OnCastCompleted` 直调 `CommitCost`）：
    自动档的提交点**已经知道**自己该不该提交，走本口会多一层"按 Timing 判断"的分支
    ——那正是"同一件事两个入口"；
  - **内部 MUST 重查一次 `CanAffordCost`**（TOCTOU 防护：门禁⑤ 通过到此刻之间资源可能已被花掉），
    且判据与门禁**同一函数**（MUST NOT 各写一份）。
- **三个提交点的落位（MUST 各按其时机，MUST NOT 互相代劳）**：
  - `ECT_OnCastStarted` ⇒ 激活路径的"**门禁⑤ 之后、建 run 之前**"；
  - `ECT_OnCastCompleted` ⇒ 终结路径的 Timing 分派，**只在 `ECFR_Completed` 路触发**
    （被 `ECFR_Interrupted` / `ECFR_Replaced` 终结时**不提交**——该档无"打断时提交"语义）；
  - `ECT_Manual` ⇒ **框架零动作**（含日志）——宿主自己调 `PayCost`。
    **"由某个特定项目事件触发消耗" MUST 走这一档**（框架不为它内置时机，见数据形状需求）。
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

### Requirement: 消耗事件与 `CostApplied`

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
- **本事件 MUST NOT 依赖任何特定时机档**：它是"扣减发生了"的**事实广播**，
  `OnCastStarted` / `OnCastCompleted` / `Manual`（宿主调 `PayCost`）三条路径**都必须发它**。

#### Scenario: 提交成功后发 `CostApplied` 且载荷逐项吻合

- **WHEN** 一个 `ECP_ResourceAttr` 档技能成功扣减
- **THEN** 恰发 **1** 次 `CostApplied`，其 `EntryHandle` / `Unit` / `ResourceAttr` / `Amount` 与本次扣减一致

#### Scenario: 扣减失败不发事件

- **WHEN** 门禁⑤ 或提交点判定"付不起"
- **THEN** **不发** `CostApplied`（拒绝面由 `SAR_CannotAfford` 覆盖）

#### Scenario: 宿主手动 `PayCost` 也发本事件

- **WHEN** 宿主在自己的逻辑处（例如监听到某个项目事件之后）调 `PayCost` 且扣减成功
- **THEN** 恰发 **1** 次 `CostApplied`——**该路径与自动档逐字同源**
  （这正是"特定事件触发消耗"的宿主实现方式，框架不需要为此内置时机档）

### Requirement: `CostAmount` 消耗修正契约键与生效量求值

`TcsSkill` MUST 提供消耗的**生效量求值**，使"基数可配 + 减免可挂"两件事合成一个数：

- **契约键** `TcsStateParam.CostAmount`（原生 tag + 模块导出，与 `TcsStateParam.CooldownPct`
  同族）：**本键上的修正经五带折叠作用于 `Amount` 的求值结果**
  （**初值 = 字段求值结果，不是 0**）；
- **它与 `CooldownPct` 的刻意不对称（MUST 记，防按对称性误改）**：
  `CooldownPct` 是**纯系数**（`Duration × (1 − CDR)`、只按 `[0,1]` 钳制、**不做折叠**）；
  而本键走**带式折叠**——**因为"减固定值"用纯系数表达不了**。
  判据 = "**减免能不能用一条系数说清**"；
- **两种减免形态各占一带**（复用 `FoldTcsAttributeBands` 的既有算术，**零新机制**）；
- **求值 MUST 是唯一实现**（`FTcsCostOps::ResolveEffectiveCost`）：门禁⑤ 与**每个提交点**
  都调它，**MUST NOT 各算一份**（"门禁判的数"与"真扣的数"一致 = TOCTOU 防护的实质）；
- **`CostApplied` 载荷的 `Amount` MUST 取实际扣减量**（求值 ⊕ 减免之后），
  **MUST NOT 取配置基数**——否则"发出扣了 30 而实际扣了 21"会直接误导订阅者的 UI。

#### Scenario: 百分比减免生效

- **WHEN** `Amount` 基数求值为 `30`，且在 `CostAmount` 键上挂 `TAO_PercentAdd = -0.2`
- **THEN** 生效量为 `24.000`（`30 × (1 − 0.2)`）

#### Scenario: 固定值减免在百分比之后叠加

- **WHEN** 在上述之上再挂 `TAO_FlatAdd = -5`
- **THEN** 生效量为 `19.000`（`24 − 5`）——**MUST NOT 是 `20.000`**
  （那说明 `FlatAdd` 被算在缩放**之前**，与本仓属性/伤害的共用算术次序不符）

#### Scenario: 减免到免费是合法结果

- **WHEN** 减免把生效量折到 `0`（或折叠后转负被钳到 0）
- **THEN** 该技能**恒可付**、零扣减、零日志、**不发** `CostApplied`
  ——与"配了 0 点消耗"走同一条路（**MUST NOT** 报成配置错误）

#### Scenario: 减免按来源级联摘除

- **WHEN** 三条减免修正器同属一个来源，随后该来源被 `RemoveParamModifiersBySource` 摘除
- **THEN** 生效量**回到基数**（一次摘净）；**MUST NOT** 留下残余减免
  （成对性：与 `ApplyParamModifiers` 严格配对）

#### Scenario: `CostApplied` 载荷是实际扣减量而非基数

- **WHEN** 一次基数 `30`、减免后 `19` 的消耗提交成功
- **THEN** `CostApplied` 载荷的 `Amount == 19`（**MUST NOT 是 30**）
