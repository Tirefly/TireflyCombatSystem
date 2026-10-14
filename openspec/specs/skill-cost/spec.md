# skill-cost Specification

## Purpose
定义技能消耗的**数据形状与运行态**：策略族 `ETcsSkillCostTiming` / `FTcsSkillCostConfig` / `FTcsSkillCostPolicy`（形状），`FTcsCostOps` 的判定与提交（**fail-closed 的配置边界**、按**基值**直写的扣减、TOCTOU 重查）、**三个提交点**（激活期 / 终结期 / 手动）各自的落位判据，以及 `TcsEvent.Cast.CostApplied`——让「付不起」成为**具名、可判、可预查**的运行事实，而不是一个恒过的占位分支。
**框架只为"施法这个事务自身的边界"提供时机档**；"由某个特定项目事件触发消耗"属**项目词汇**，走 `ECT_Manual` 档 + `PayCost` 由宿主实现（`ECT_OnEvent` 档已于 2026-10-12 裁定删除，MUST NOT 补回）。

## Requirements

### Requirement: 消耗配置的数据形状（策略与三档时机）

`TcsSkill` MUST 提供消耗配置的**纯数据**形状（零行为）：

- `UENUM(BlueprintType) ETcsSkillCostTiming`（**四档**）：`ECT_None`（**默认**：该技能一定没有消耗）
  / `ECT_OnCastStarted` / `ECT_OnCastCompleted` / `ECT_Manual`；
- `USTRUCT(BlueprintType) FTcsSkillCostConfig{ ETcsSkillCostTiming Timing; FInstancedStruct CostMode; }`；
- `USTRUCT(meta = (Hidden)) FTcsSkillCostPolicy`（消耗策略**抽象基类**：`CanAfford` / `CommitCost`
  两个虚函数，**中性默认实现** + `meta = (Hidden)`，**虚析构**）；
- `USTRUCT(BlueprintType) FTcsSkillCostPolicy_ResourceAttr : public FTcsSkillCostPolicy`
  （内建策略：`FGameplayTag ResourceAttr` + `FTcsParamValue Amount`）；
  **`BlueprintType` 是必需项**（**2026-10-13 订正**：本行原写裸 `USTRUCT()`，与代码不符）——
  判据 = `IsScriptExposedStruct`（`PyGenUtil.cpp:1560`）**沿父链**逐个查该元数据、
  **任一环有才导出**，而基类只带 `meta = (Hidden)` ⇒ 裸 `USTRUCT()` 的派生类**不进脚本层**
  ⇒ **内建策略脚本侧配不了**，与上述"内建也走同一套"及脚本面插槽目标直接冲突。
  **两条并列要求（MUST NOT 互相顶替）**：**具体策略类带 `BlueprintType`**（决定"脚本能不能配"）；
  **基类只带 `meta = (Hidden)`**（决定"picker 里能不能选到基类"——picker 的
  `FInstancedStructFilter::IsStructAllowed` **不读** `BlueprintType`）；
  故"编辑器里选得到"**推不出**"脚本里也配得了"。
- `USTRUCT(BlueprintType) FTcsSkillCostPolicy_HostDelegate : public FTcsSkillCostPolicy`
  （脚本面转发器：`TScriptInterface<ITcsSkillCostPolicyHost> Host`）；
- `UINTERFACE(MinimalAPI, Blueprintable) UTcsSkillCostPolicyHost` / `ITcsSkillCostPolicyHost`
  （宿主契约：`CanAfford` / `CommitCost`，均 `UFUNCTION(BlueprintNativeEvent)`）。

**★ 为什么是策略对象而不是档位枚举（2026-10-12 用户改点，MUST 读，防改回枚举）**：
原设计 `ETcsCostPolicy{None/ResourceAttr/CustomFragment}` 把"消耗什么"写死成**枚举档位**——
**档位枚举答"走哪个"、策略对象答"怎么做"，两者并存时"以哪个为准"就成了必须裁定的问题**
（同一件事两个入口）。改点后只剩**策略对象**一个入口。

**`CostMode` MUST 是裸 `FInstancedStruct`**（**MUST NOT** 用 `TInstancedStruct<T>`）：
后者在宿主脚本层**导出为空壳**（无字段读写代码）⇒ 脚本层配不了任何策略
（2026-09-24 换型口径）。类型收窄由 `meta = (BaseStruct = "...TcsSkillCostPolicy")` 给出。

**取内层 MUST 用单参数 `GetPtr<FTcsSkillCostPolicy>()`**：它在**空载荷**或**类型不符**时
**返回 `nullptr`** ⇒ 一个判据同时覆盖两种降级。**MUST NOT 用双模板参数重载**
`GetPtr<T, BaseT>()`——它在 `Shipping` 下**完全不校验**（引擎源码自认 "this may crash in shipping!"）。

**策略基类 MUST NOT 用纯虚 `= 0` 或 `PURE_VIRTUAL`**：UHT 为每个 USTRUCT **无条件**生成
`TCppStructOps<T>`（构造路径 `new T` 需可默认构造）⇒ 纯虚报 `C2259`；而 `PURE_VIRTUAL` 在
`CHECK_PUREVIRTUALS` 开启时展开为 `= 0`、关闭时才是致命错误体 ⇒ **同一份代码在 Development
编不过、Shipping 能过**（最坏的一类失败）。⇒ **中性默认实现 + `meta = (Hidden)`**
（picker 按 `Hidden` 过滤 ⇒ 基类不可被作者选中）。

**策略族 MUST 有虚析构**（多态基类的正确性要求，非洁癖）：派生策略持
`TArray` / `FTcsParamValue` 一类**非平凡析构**的成员；非虚析构下任何"经基类指针删除"
都是未定义行为（MSVC `C4265`）。

**`Timing == ECT_None` 是"不消耗"的唯一表达**（2026-10-12 用户裁定："只要 `CostTiming` 为 `None`，
那就不用管后面了，因为一定证明这个技能没有消耗"）：该档下框架**不看 `CostMode`**、
不扣减、**零日志**。原 `ECP_None` 枚举档**整体退役**。

**`Timing != ECT_None` 而 `CostMode` 为空 MUST fail-closed**（拦 + `Warning`）——
**MUST NOT** 静默按"不消耗"处理。判据：**"声明不消耗"（`Timing == None`，零日志）**与
**"想配消耗但没配清"（`Timing != None` 而策略为空，拦住 + `Warning`）是两件事**；
混成一类会让后者**表现成一切正常**（本仓反复在拦的那类 bug）。

**`Amount` MUST 是 `FTcsParamValue`（参数源载体）**（2026-10-12 用户裁定，台账 `COST-1` 闭合）：

- **判据 = 一个 `double` 同时承担了三件事，而三者都需要"可配置"**：
  ① **按等级配数值**（"1 级 10 蓝 / 2 级 15 蓝"）；② **按来源配数值**（`HostDelegate` / `ParamRef`）；
  ③ **减免**——由契约键 `TcsStateParam.CostAmount` 上的修正器折叠给出。
  ⇒ 裸 `double` 三者都做不到；
- **`FTcsParamValue` 的默认构造装 `Literal(0)`** ⇒ 与换型前的 `double Amount = 0.0` 语义逐字相同
  （既有资产零迁移；全仓 `.uasset` 扫该字段零命中）；
- **求值 MUST 在门禁⑤ 与每个提交点各做一次**，**MUST NOT** 求一次后跨"门禁 → 提交"复用
  （那会把 TOCTOU 窗口重新打开）；
- **求值结果 MUST 钳下界到 0**，但**调用方 MUST 能同时拿到"未钳的基数"**——
  判据见下方"负基数与减免过度是两件事"。

**`ETcsSkillCostTiming` MUST NOT 含 `OnCastInterrupted` 档**——两条硬判据（非偏好）：

1. `D5-4` 已把 `bRefundCostOnInterrupt` 砍除，理由原文是"**返还是政策不是引擎语义**"
   ⇒ 框架层面"打断 ⇒ 退钱"这条路已被明确关闭；
2. 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**（那就是
   `ECT_OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。

**`ETcsSkillCostTiming` MUST NOT 含 `OnEvent` 档**（2026-10-12 用户裁定移除，**MUST NOT 补回**）：

- **判据**：框架只能认识"**施法这个事务自身的边界**"（开始 / 完成）——这两个时点在**每个项目里
  含义都相同**；而"**某个特定事件**"（挨打时付费 / 连击到第 3 段付费 / 血量低于 30% 付费……）
  是**项目词汇** ⇒ 框架内置它等于**让框架去猜项目的玩法**，且它提供的只是一个"等某个 tag"的
  空槽位（"这个 tag 是什么、什么时候发"全在宿主手里）⇒ **框架提供了一个自己解释不了、
  也无法校验的档位**。这与本仓主判据「**凡是需要宿主词汇的内置策略，框架一律不提供**」直接冲突；
- **宿主实现成本极低，且不需要任何新框架面**：监听自己关心的事件 → 调 `PayCost`（公开口）
  ——那正是 `ECT_Manual` 档 + `PayCost` 的既有组合；
- **伴随面 MUST 一并删除**（否则留下悬空契约）：`TriggerEventTag` 字段、
  `UTcsCostEventRelay` 整个类、门面的 `GetCostEventRelay()` 与 `CostEventRelay` 成员、
  以及三处撤销路径（`Revoke` / `RevokeBySource` / `UnregisterEntity`）的中继退订。
  **判据 = 「零消费者不预建」的反向使用**：消费者归零 ⇒ MUST 删掉；
- **`CostApplied` 事件本身 MUST 保留**（它与档位无关：任何提交路径都发它）；
- **`ETcsSkillCooldownTiming` 不受本裁定影响**——冷却侧本来就没有 `OnEvent` 档
  （其四档是 `OnCastStarted` / `OnCastCompleted` / `OnCastInterrupted` / `Manual`）。

**内建策略的失效面 MUST fail-closed**（判据 = 本仓既有惯例，
`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
三处措辞几乎逐字相同："**配置写错**"与"**正确声明不消耗**"是两件事，混成一类会让前者
**表现成一切正常**、日志还清白）：

- `Timing == ECT_None` ⇒ 恒可付、**零日志**（正确声明"不消耗"）；
- `ResourceAttr` **无效** ⇒ **恒不可付** + `Warning`；
- **`Amount` 的求值结果为负** ⇒ **恒不可付** + `Warning`（负值会让技能**倒给**资源）；
- `Amount` 的求值结果为零 ⇒ **恒可付**、零日志（"消耗 0 点"就是不消耗）。

**★ 负基数与减免过度 MUST 是两件事（装置拒绝面检查 `d` 实测抓到的设计缺陷）**：

| 情形 | 判据依据 | 语义 | 处置 |
|---|---|---|---|
| **负基数**（作者把 `Amount` 填成 -50） | **未钳的基数** | **配置错误**、"倒给资源"的**漏洞形态** | **拦住** + `Warning` |
| **折叠后转负**（基数 30、减免 -100） | **钳后的生效量** | **减免过度** = 合法的"免费" | **钳 0**、**不报错** |

**判据（MUST）**：`ResolveEffectiveAmount` MUST 输出**未钳的基数**（出参），
使上述两条判据各得其数。**MUST NOT** 只给一个钳过的结果
——那会让负基数被静默抹成 0、其守卫永远不触发（**该缺陷已在实施期被装置当场报出**）。

#### Scenario: 默认档为不消耗且恒可付

- **WHEN** 某技能的 `FTcsSkillCostConfig` 取默认值
- **THEN** `Timing == ECT_None`、`CostMode` 为空载荷；该技能恒可付、零扣减且**零日志**

#### Scenario: 三档时机且无打断档、无事件档

> **场景标题是建立时（2026-10-12 早些时候）的轮次标签**——本仓裁定：`#### Scenario:` 标题在
> MODIFIED 语义下**不可改名**（先例 = `archive/2026-10-04-add-tcs-state-module/specs/plugin-descriptor/spec.md:18`；
> 又见 `skill-def-asset` 的"冷却与 Cost 字段不在本轮"同款处置）。
> **本场景的判据已随同日策略化改点现况化为"四档"**，见下。

- **WHEN** 枚举 `ETcsSkillCostTiming` 的全部取值被检查
- **THEN** 恰为 `None` / `OnCastStarted` / `OnCastCompleted` / `Manual` **四档**
  （`None` 为默认档，表"该技能一定没有消耗"）；
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

> **场景标题是建立时（2026-10-12 早些时候）的轮次标签**（同款处置见上）——
> **本场景的判据已随同日策略化改点整体反转**：`ECP_CustomFragment` 档**已随枚举退役而消失**，
> 那段"具名降级"正是被修掉的缺陷形态。**标题保留作现场痕迹，MUST NOT 据其派活。**

- **WHEN** 某技能配 `Timing != ECT_None` 但 `CostMode` 为空载荷（想配消耗却没接上策略）
- **THEN** `CanAffordCost` 判**不可付**、`CommitCost` **零提交**，并留**具名 `Warning`**
  ——**fail-closed**；**MUST NOT** 静默按"不消耗"放过，也 **MUST NOT** 免费放行
  （换型前的 `ECP_CustomFragment` 降级是"恒可付 + 零扣减"= **免费放**，
  与同文件"资源都没法查就放技能等于免费放"的判据自相矛盾 ⇒ 那是**已修正的缺陷形态**）

### Requirement: 消耗的运行态（判定 / 提交 / 三个提交点）

`TcsSkill` MUST 提供消耗的**运行态**（`FTcsCostOps`），使 `FTcsSkillCostConfig` 成为真的扣减机制。
**框架只做分派，"扣什么、怎么扣"归策略**。**判据的唯一实现住本族函数**：门禁⑤（激活序列）
与各提交点共用同一份，**MUST NOT 各写一份**。

- **策略解析** `ResolvePolicy(Config)` ⇒ `const FTcsSkillCostPolicy*`（**唯一解析点**；
  空载荷与类型不符都返回 `nullptr`）；
- **上下文装配** `MakeContext(Subsystem, Entity, Entry)` ⇒ `FTcsSkillCostContext`
  （`Subsystem` / `Entity` / `Entry` / `Level` 四件；**等级从条目取** ⇒ "按等级配消耗"的落点）；
- **门禁判定** `CanAffordCost(Subsystem, Entity, Config, OutReason, Entry)`——**三类分开处置**：

  | `Timing` | `CostMode` | 返回 | 日志 |
  |---|---|---|---|
  | `ECT_None` | 任意（**一概不看**） | `true` | **无**（正确声明"不消耗"） |
  | 非 `None` | **空 / 类型不符** | `false` | **`Warning`**（配置错误，fail-closed） |
  | 非 `None` | 有效 | 由**策略**答 | 由策略决定 |

  - **fail-closed 判据（MUST）**：本仓对"**配置写错**"的处置一律是"**拦住 + 报出**"，
    措辞见 `TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`
    ——"**不静默通过**：静默会让'条件写错'表现成'步骤照跑'"；
  - **内建策略的 `Amount < 0` MUST 拦**：扣减式是 `Base - Amount` ⇒ 负值会让技能**倒给**资源
    （`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，不是"配置不生效"；
  - **内建策略 MUST 先确认"持有"再比基值**：底层对**未持有**返回 `0.0`，与"持有且基值为 0"
    **不可区分** ⇒ 不先问持有，就会把"属性配错 / 压根没加"表现成"**付不起**"
    ——两种红字的含义完全不同（`TcsEffectAttributeAccess.h:72` 是同一条判据的既有表述）；
  - **MUST NOT ensure**：付不起是**正常业务路径**，不是契约违规（同 `AttributeCompare` 的处置）；
  - **纯查询语义**：MUST NOT 扣任何资源、MUST NOT 改账本、MUST NOT 发事件；
  - **属性门面不可得 ⇒ 判为不可付**（fail-closed）：资源都没法查就放技能，等于免费放
    ——那是"配置了消耗却静默不生效"的最坏形态。
- **提交** `CommitCost(Subsystem, Entity, Config, OutApplied, Entry)`——**唯一的提交实现**：
  - **框架 MUST 把策略的整个提交包进属性批**（`BeginBatch` / `Commit`，2026-10-12 用户裁定）；
    **批的边界 = 紧邻本策略的提交**，**MUST NOT** 圈进施法激活的其它属性改动
    （如 `MaterializeParamChainRows`）；
  - **★ 批给什么、不给什么（MUST 读，防误判）**：批**给**"重算与广播推迟到最外层 `Commit`"
    + "订阅者看不到中间态"（⇒ 关闭重入窗口）；**不给回滚**——`SetBaseValue` 的写入是**立即**的
    （`Instance->BaseValue = NewBaseValue;`），批只推迟 `Recalculate`。
    ⇒ **"全有全无"的实际保障 = 先判后扣**（调用方 MUST 先过 `CanAffordCost`），
    **不是批本身**。**MUST NOT** 把批当作事务/回滚来依赖；
  - **包批前 MUST 用不 ensure 的口探测实体注册态**：`BeginBatch` / `Commit` 对**未注册实体**
    触发 `ensureMsgf`（红字），而**宿主自定义策略**可能"没注册实体也判可付"
    ⇒ 框架包批会**替宿主多报一条 ensure**。未注册时**不包批**（策略照常提交，其自身失败面上报）；
  - **内建策略的扣减载体 = `SetBaseValue` 直写（读基值 → 减 → 写回）**，
    **MUST NOT 用 `ApplyModifier` 挂修正器**。判据三条，缺一都不成立：
    ① **语义**——`SetBaseValue` 的注释原文即"**改写属性基础值（等级成长等宿主升级事务的落点）**"，
    而"消耗资源"正是**永久改变基值**；挂修正器条目表达的是"**某个来源带来的增减**"，
    与"花掉了"不是一回事；
    ② **来源锚点的矛盾**——`ApplyModifier` 路径**必须**给 `Source`，而资源扣减**不可**挂施法运行态来源
    （施法一终结 ⇒ 级联摘除 ⇒ **扣掉的资源回来了**）；挂"长寿命来源"则要**发明一个新发号器**，
    而该来源今天**零撤销消费者**（消耗本就不可撤销）⇒ 正是"零消费者不预建"；
    ③ **`CHAIN-8` 已就同一问题给过判据**：修正器层**效力不等同**于直写基值——会被任一
    `TAO_Override` **整体抹掉**、参与 `PercentAdd`/`Mul` **复利**、可按来源摘除。
    **"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的**；
  - **提交失败 MUST 被报出**：策略内部的写口（如 `SetBaseValue`）自身对
    "实体未注册 / 属性名为空 / 未持有此属性"返回 `false`（含 `ensureMsgf`）
    ⇒ MUST 检查返回值，**MUST NOT** 假设它一定成功（否则 `PayCost` 会报成功而资源没扣）。
- **`PayCostForEntry(Subsystem, Entity, EntryHandle)`**——**`ECT_Manual` 档的专用提交入口** + 宿主重试口：
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

#### Scenario: 宿主自定义策略的检查与提交被真实调用

- **WHEN** 一个技能配 `CostMode` = 宿主自定义策略（C++ 派生 `FTcsSkillCostPolicy`，
  或脚本实现 `ITcsSkillCostPolicyHost` 经 `FTcsSkillCostPolicy_HostDelegate` 接入）
- **THEN** **框架 MUST 调到该策略的 `CanAfford` 与 `CommitCost`**，
  且**扣减量由策略决定**（**MUST NOT** 由框架按配置值代扣）
  ——判据 MUST 取"策略被调用"的**可观测证据**（如装置侧调用计数），
  并让策略返回一个**不等于配置基数**的值，以排除"框架走了别的分支恰好同值"

#### Scenario: 转发器空 Host 时 fail-closed

- **WHEN** `CostMode` 装了 `FTcsSkillCostPolicy_HostDelegate` 但 `Host` 为空
- **THEN** `CanAfford` 判**不可付**、`CommitCost` **零提交**（**MUST NOT** 返回中性值放行）
  ——空 Host 是**配置缺口**，与"策略缺失"同一条处置

#### Scenario: 不消耗档恒可付且零日志

- **WHEN** 一个 `Timing == ECT_None` 的技能被激活
- **THEN** 门禁⑤ 通过、激活成功，且**不产生任何消耗相关日志**（正确声明"不消耗"）

#### Scenario: 资源足够时按基值扣减

- **WHEN** 某实体持有资源属性（基值 100），一个配内建属性资源策略
  （`ResourceAttr` + `Amount = 30`）+ `OnCastStarted` 档的技能被激活
- **THEN** 激活成功，且该属性**基值**从 100 变为 70（**MUST NOT** 用 `EvaluateCurrent` 判读——
  它含修正器，与扣减口径不同层）

#### Scenario: 资源不足 ⇒ 具名拒绝

- **WHEN** 基值低于 `Amount`
- **THEN** 返回 `SAR_CannotAfford`，**资源一分未动**、**MUST NOT 建 run**

#### Scenario: 无效 ResourceAttr / 负 Amount 一律 fail-closed

- **WHEN** 分别配 `ResourceAttr` 无效、`Amount < 0`
- **THEN** 两者都判**不可付**并留 `Warning`——**MUST NOT** 静默按"恒可付"放过

#### Scenario: 未持有属性与"基值为 0"可区分

- **WHEN** 某技能配了一个该实体**未持有**的属性作为 `ResourceAttr`
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

#### Scenario: 策略提交后无残留批

- **WHEN** 一次消耗提交走完（含"框架包批"那条路）
- **THEN** 该实体的属性批深度归零、**无残留开启批**
  （判据：提交后的 `PeekPending` 与 `EvaluateCurrent` 相等——不等说明批没 `Commit`、
  后续属性广播会被吞）

### Requirement: 消耗事件与 `CostApplied`

- `TcsSkill` MUST 提供 `TcsEvent.Cast.CostApplied` + `FTcsCostEventPayload`
  （`{EntryHandle, Entity, DefTag, ResourceAttr, Amount}`）；
- **`D5-16` 的 `SkillId` 字段 MUST 用 `DefTag` 取代**（**MUST NOT 为它发明一个类型**）：
  `D5-9` 修订后 `EffectiveDefId` 已整体移除；
- **命名 MUST 去掉 `On` 前缀**（`D5-16` 原文写 `OnCostApplied`）：`TcsEvent.Cast` 根下既有四枚
  **零 `On` 前缀**，按同族对称取名；
- **导出宏**：MUST 为 `extern TCSSKILL_API FNativeGameplayTag`（裸 `extern` 会 `LNK2001`）；
- **`CanAfford` 失败 MUST NOT 发本事件**（`D5-16` 明文："CanAfford 失败不发——`TryActivate` 返回值覆盖"）：
  门禁拒绝面已有 `SAR_CannotAfford` 这个具名读数，再发一枚"没扣成"的事件纯属噪音；
- **只有真的扣了才发**（扣减量 `<= 0` ⇒ 零动作、零事件）；
- **本事件 MUST NOT 依赖任何特定时机档**：它是"扣减发生了"的**事实广播**，
  `OnCastStarted` / `OnCastCompleted` / `Manual`（宿主调 `PayCost`）三条路径**都必须发它**。

#### Scenario: 提交成功后发 `CostApplied` 且载荷逐项吻合

- **WHEN** 一个配内建属性资源策略的技能成功扣减
- **THEN** 恰发 **1** 次 `CostApplied`，其 `EntryHandle` / `Entity` / `ResourceAttr` / `Amount` 与本次扣减一致

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
- **求值 MUST 是唯一实现**（内建策略的 `ResolveEffectiveAmount`）：门禁⑤ 与**每个提交点**
  都调它，**MUST NOT 各算一份**（"门禁判的数"与"真扣的数"一致 = TOCTOU 防护的实质）；
- **求值 MUST 是策略自己的成员**（2026-10-12 策略化改点）：
  换型前它是 `FTcsCostOps` 的静态函数（框架代算），改点后归**内建策略**——
  因为"怎么算减免"是**该策略自己的语义**，宿主自定义策略可以有自己的算法；
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
