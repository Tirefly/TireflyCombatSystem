# R6.5-1 实施计划：冷却与消耗（M5 技能层第二层）

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:executing-plans`（推荐，本任务强耦合 C++ 适合同会话内联执行）或 `superpowers:subagent-driven-development` 逐任务派发执行本计划。步骤使用 `- [ ]` 复选框跟踪。**每个步骤完成后必须用新鲜证据复核，再更新对应 OpenSpec 任务状态。**
>
> **★ 本仓 TDD 例外（MUST 读）**：`superpowers:writing-plans` 的默认形状是"先写失败测试"。**本仓 `AGENTS.md` 明令 Unreal 开发禁 TDD**（"绝对禁止"），且 `unreal-development-workflow` 规定验证 = **编译/构建 + 定向人工检查 + 项目既有装置取证**。故本计划**不使用 TDD 步骤**，其验证循环一律为：
> **实现 → 双配置 UBT 编译 → `TcsDev` 装置取证（两轮复现性）→ 文档回写**。
> 判据：技能推荐与用户级规范冲突时，**以用户级规范为准**（`AGENTS.md`「技能优先与流程纪律」明文）。

- **文档 ID**：`PLN-R6.5-1`
- **类型**：PLN / 计划
- **状态**：ACTIVE
- **权威范围**：`R6.5-1`（冷却与消耗，批次 `a`~`d`）当前实施计划——Task 骨架、口径裁决、验收面、取证纪律。**不含**批次 `e` / `g`（另轮；**★ 2026-10-13：原写 `e`/`f`/`g`，`f` 已删除**——与 `R5.5-f` 同一工作量，收敛为一个编号，见 `GLOSSARY` §2.1）；**不含** R6 已完成面与 R7 计划
- **最后更新**：2026-10-11（建立：开工收窄轮产出——批次 `a`~`d` 落成 Task 0~6；**四处口径当场裁定**（见 §0.2）；一处 R6 收束关账话术订正（见 §0.4））

**Goal:** 让「技能放不出来」的第二层理由变得可表达、可查询、可驱动——**冷却多轨道**（自身轨 / 共享组轨 → 真 GCD = 纯数据）与 **Cost 策略 × 时机**（付得起 / 何时付），两者都接到 R6 已落地的**六道具名门禁**与**到期堆**上，并把 R6 留的两道空门禁（`SAR_OnCooldown` / `SAR_CannotAfford`）从占位变成真分支。

**Architecture:** 全部改动住在 **`TcsSkill` 一个模块内**（`ECostPolicy` / `ECooldownTiming` / `FTcsCooldownTrack` 等新类型落 `Public/Def/`，运行态落 `Private/Skill/`）。**零新模块依赖边**：到期驱动复用 `TcsCore` 的 `UTcsClockSubsystem::PushExpiry` / `CancelExpiry`（该头 `:112` 注释原文即"M5 冷却的消费底座"）；属性读写复用 `TcsAttribute` 的 `EvaluateCurrent` / `ApplyModifier`（`TcsSkill` 已依赖两者）；事件复用 `TcsEvent.Cast` 根与 `TcsEventBus::Subscribe` / `PublishImmediate`。**冷却状态的真值住在账本条目**（`FTcsLearnedSkillEntry` 以 MODIFIED 回补 `CooldownTracks` 字段——R6 Task 2 曾因"类型不存在"刻意移除该字段，本轮类型与消费者同时到位）。

**Tech Stack:** UE 5.8 C++、GameplayTags（原生 tag + 模块导出宏）、`FInstancedStruct`（策略载体，`MEM-20260910-04`）、`TTcsInstancePool` / `FTcsExpiryHeap`、UBT（Development Editor + Game Shipping 双配置）、宿主侧装置（LAC `Source/TcsDev/`）。

**Spec:** `Documents/combat-system-design/spec/05-module-skill.md`（`SPEC-04-skill`）§3.5 冷却 / §3.6 Cost / §5 关键机制；决策原文 `Documents/combat-system-design/log/log-03-skill-m5.md` 的 `D5-10` / `D5-15` / `D5-16`。本计划是这批决策的**执行面**；`SPEC-04-skill` 是**语义面**；两者冲突时**以本计划 §0.2 的裁定为准**（本轮已就四处与正文失步处裁定，见 §0.4）。

---

## 0. 开工前置（先读：本轮输入、口径裁决、现状证据）

### 0.1 本轮输入（台账折入，逐条给落点）

| 台账条目 | 原归属 | 本计划落点 | 处置 |
|---|---|---|---|
| `WAIT-14`（`FTcsBoolSwitchRow::Mode` 零读取者） | 触发条件 | **不在本轮**（属批次 `e`） | 不动（本轮零布尔修改器） |
| `STAT-11`（`属性换算` 参数源全仓零实现者） | 触发条件 | **不在本轮**（本轮不碰参数源） | 不动 |
| `STAT-10`（类别标识容器 + `ByCategoryTags` 档） | 三项一并裁 | **不在本轮** | 不动 |
| `TRIG-5` 之 ⑤（`GateCheck` 读 `BoolSwitches`） | R6.5 候补 | **不在本轮**（批次 `e`） | 不动 |
| `SCRIPT-9`（属性写入面缺反射） | 触发条件 | ~~被本轮的 `ECostPolicy::ECP_CustomFragment` 档引用（见 §0.2 裁决 ④）~~ **★ 该引用已于 2026-10-12 改正：本档用不到 `SCRIPT-9`**（见 §0.2 裁决 ④ 的改正注记） | **不消费**；**本档的阻塞登记改指 `COST-3`**（新增条目） |

> **入册判据**：本轮新增的待办若**有 Task 归属**则不入册（`LEDGER-deferred`《入册判据》第 3 条）；只有指不出归属的才入册。本轮预期**零新增条目**。

### 0.2 本轮四条口径裁决（MUST 读——四处都改写了批次表/正文的原表述）

> **裁决来源**：2026-10-11 用户逐条拍板。**每条都关掉了一处"待定/二选一"**（判据 = `MEM-20261006-15`：计划里的"待定"是欠债标记，MUST 在计划期用硬约束关掉）。

**裁决 ①：`Custom` 时机档整体取消——不是"留形状报未实现"，是「从未需要」。**

用户的定义（原话意旨）：**Cost/Cooldown 的自定义时机 = 不自动执行，让宿主项目在自己的确定逻辑处手动调用** ⇒ **不需要任何 Payload / Fragment 载体**。

由此**三处连带作废**：

| 原表述 | 处置 | 判据 |
|---|---|---|
| `ITcsCooldownTimingFragment`（批次表 `c` 的第四档） | **整体删除**，不建该接口 | 见裁决 ③：`Manual` 档取代它，且**不需要载体** |
| `FCooldownPolicy.CustomFragment`（`D5-15` 字段表） | **删除该字段** | 同上 |
| `FCostConfig.Policy = CustomFragment`（`D5-10` 字段表） | **保留档位名，但本轮具名未实现 + 回退 `ECP_None`** | 它的消费者（宿主脚本写属性）今天物理不可达——见裁决 ④ |

**为什么这消解了 R6 留下的岔口**（`SPEC-04-skill` §3.3 实施注记）：R6 在 `CQM_Custom` 上留了一个**真实未决**——"`CastQueryFragment` 是 `FInstancedStruct`（策略结构体），而 `ITcsCastStateQuery` 是 UObject 接口，两者不是同一种载体"。**对「时机」这一维，该岔口不再成立**：时机是一个"**何时做**"的问题，而"何时"只有两种表达——**能被事件名说出来**（→ `OnEvent`）或**只能由代码在具体位置说出来**（→ `Manual`）。**中间没有第三种形态**，故两种载体都不需要。

**裁决 ②：`OnEvent` 档采纳（用户主动提出）。**

`D5-10` 的 Cost Timing 五档里原本就有 `OnEvent`，而批次表 `c` 把它漏掉了。**本轮采纳并给它形状**：

- 档位名 = `ECT_OnEvent`；
- 载体的**问题不在时机侧**——"等哪个事件"是**数据**，可写成 `FGameplayTag`（与 `FTcsEffectTriggerDef::EventTag` 同形，`TcsEffectTrigger.h:61`）；"事件到了做什么"是**行为**，走既有 `UTcsEventHandler` 派生 + `TcsEventBus::Subscribe`（`TcsEventBusSubsystem.h:83`，**裁决 2a：事件类型 → Handler CDO**）。
- **该 Tag 从哪来**：不新增字段于 policy —— 见裁决 ③ 的统一形状。

**裁决 ③：两个 Timing 枚举不是同一个（硬推导，非偏好）。**

`ECostTiming` **MUST NOT** 含 `OnCastInterrupted` 档。**判据**（两条硬约束，非"我觉得"）：

1. **`D5-4` 已把 `bRefundCostOnInterrupt` 砍除**，理由原文是"**返还是政策不是引擎语义**"；
2. 于是"打断时扣"这一档**没有任何独立含义**：说"施法开始时扣" ⇒ 被打断**也扣了**（那就是 `OnCastStarted`）；说"打断时扣" ⇒ 施法**成功完成就不扣**——**荒谬**。

⇒ 两个枚举**各有自己的档位集**（见 §2 落位表），MUST NOT 共用一个枚举、MUST NOT 用一个枚举"覆盖"另一个。

**裁决 ④：`CanAfford` 门禁恒在 `TryActivate`；提交点各自重查一次。**

- **门禁第 ⑤ 道（`CanAfford`）的调用点不变**——仍是 `TryActivate` 的六道门禁序列之一，答"**现在**付得起吗"。宿主据此可从返回值里看出为什么放不出来（R6 已为此落了具名枚举 `SAR_CannotAfford`）。
- **到期 / 事件 / 手动三个提交点各自重查一次**（TOCTOU 防护：门禁通过到提交点之间，资源可能已被花掉）。⇒ **一条判据两个调用点、语义一致**，MUST NOT 各写一份判断。
- **`ECP_CustomFragment` 档本轮具名未实现 + 回退 `ECP_None`**（记 `Warning`，MUST NOT 静默）。**★ 阻塞前置已于 2026-10-12 改正（原记"该档要求宿主脚本能写属性，而 `ApplyModifier` 的形参 `FTcsAttrModInstance` 是纯 C++ struct、非反射，`SCRIPT-9` / `R5.5-h` 均 ❌ 未开工 ⇒ 物理不可达"——该归因不成立，MUST 读）**：**真阻塞 = 扩展点形状从未裁定**（`D5-10` 只给了 `ICostPolicy` 这个 `USTRUCT` 策略基类形状，而该形状对脚本侧物理不可达 ⇒ 正确形态 = 宿主插槽家族的 `UINTERFACE` + `BlueprintNativeEvent`）⇒ 归台账 **`COST-3`**。**原记不成立的两个独立理由**：① 本档**用不到** `ApplyModifier`——Cost 侧明文 MUST NOT 用它、必须直写基值，而 `SetBaseValue` **有** `UFUNCTION`；② **宿主自定义消耗根本不经过 TCS 属性写入面**（在宿主自己的背包系统里扣）。**另记一处真实差异（勿类比）**：本档回退 `None` = **免费放**（**不是安全默认**），而 `CQM_Custom` 回退 `DefSwitches` 是**一个有定义的行为**。

**裁决 ⑤（连带落位，非独立裁决）：`Manual` 档需要两个新公开入口。**

`D5-16` 只承诺了 `AdjustCooldown` / `ResetCooldown`——**这两个是"改"已有冷却（`Delta` / `Clear`），都不是"开始冷却"**。同族地，Cost 的提交口也不在任何清单里。⇒ 本轮新增：

- `StartCooldown(Unit, EntryHandle)`——起该条目的**全部自身轨 + 所属组轨**，走与自动档**完全同一条路径**（MUST NOT 另写一份）；
- `PayCost(Unit, EntryHandle)`——按 `ECostPolicy` 提交 + 发 `OnCostApplied`，同样复用自动档路径。

### 0.3 现状证据表（逐条实测，非推断）

| 观察项 | 实测结果 | 命令/锚点 |
|---|---|---|
| 冷却/Cost 相关类型是否已落地 | **全部 0 命中**：`ECostPolicy` / `FCooldownTiming` / `FCooldownTrack` / `AdjustCooldown` / `ResetCooldown` / `OnCostApplied` | `Select-String Source\**\*.h,*.cpp -SimpleMatch` |
| 关键词命中皆注释/占位 | `Cooldown` 4 / `CanAfford` 3 / `GateCheck` 3 / `BoolSwitchMod` 6 / `SkillModDef` 2 | 同上；逐条读过，全在注释或空门禁 |
| 两道空门禁的插入点 | `TcsCastOps.cpp:163`（门禁③冷却）/ `:237`（门禁⑤ CanAfford），各带 `R6.5 占位` 注释 | 已读原文 |
| 到期堆公开口 | `UTcsClockSubsystem::PushExpiry(double, uint64, TFunction<void(uint64)>)` / `CancelExpiry(FTcsTimeEntryHandle)` | `TcsClockSubsystem.h:88/94`；`:112` 注释"M5 冷却的消费底座" |
| ⚠ `CancelExpiry` 对**悬空**句柄是 `ensureMsgf` | 留红字。而"到期回调正在跑"那一刻它自己的条目**仍有效**（`FireDueItem` 在回调返回后才释放） | `TcsExpiryHeap.h:92`、`:223-246` |
| ⚠ 池槽复用**不清零** | `TTcsInstancePool::Allocate` 复用空闲槽时**只改代际、不重建元素**（池的零策略纪律明文"Free 不清零槽位内容"） | `TcsCastOps.cpp:252-260` 的既有处置；**必须照抄"先整体重置再用"** |
| 事件订阅面 | `FTcsEventSubscriptionHandle Subscribe(FGameplayTag, UTcsEventHandler*, ETcsEventDispatch)` | `TcsEventBusSubsystem.h:83` |
| 事件广播面 | `PublishImmediate(FGameplayTag, const FInstancedStruct&)` / `PublishFrameEnd` | 同上 `:105/:113` |
| 四枚施法事件已落 | `TcsEvent.Cast.Started` / `.PhaseChanged` / `.Completed` / `.Interrupted` | `TcsCastEvents.cpp:8-18` |
| `CooldownPct` 全仓零落地 | 0 命中 ⇒ **现在就是定名时点** | 同上 |
| 全仓无任何 `enum class *Timing` | 0 命中 ⇒ 两个新枚举名无占用 | `enumerate` grep |
| `TcsStateParam` 根下"契约词原生声明"先例 | `Tag_TcsStateParam_Level`（三条判据注释齐备） | `TcsStateParamKeys.h` |
| 参数字典字段 | `FTcsStateDefBase::Params`（`TArray<FTcsNumericParamRow>`），技能 Def 继承它 | `TcsStateDefBase.h:70` |
| 属性读写口 | `EvaluateCurrent(Unit, Attribute) -> double`（读）/ `ApplyModifier(Unit, const FTcsAttrModInstance&)`（写） | `TcsAttributeSubsystem.h:219/241` |
| 属性面反射状态 | 两者皆 `UFUNCTION()` **无 specifier** ⇒ 脚本不可达；`FTcsAttrModInstance` 非反射 | `SCRIPT-9` 在册。**★ 2026-10-12 改正**：本行**与本轮的 `ECP_CustomFragment` 档无关**——本档不走 `ApplyModifier`（裁决：MUST NOT 用它，必须直写基值，而 `SetBaseValue` **有** `UFUNCTION`）；本档阻塞 = `COST-3` |

### 0.4 对本计划外两处文档的订正（MUST 同批回改）

**其一（R6 收束关账话术过期）**：`PLN-R6` 的《R6.5 批次表》关账块（`:206`）原话写"**R6.5 的开工条件已从'等 R6'变为'随时可开'**"。**该句对批次 `g` 不成立**（实测）：

- `R6.5-g`（多形态组合范式完整走查）依赖 **`R5.5-e`（关系表族）**，而 `R5.5-e` **❌ 未开工**，且它自己的第一步是"**字段语义拍板**（`Priority` 语义 / 互斥组结构 / 族规则表达式）"——`PLN-R5` §0.4 ② 的证据是 `DEC-2026-08-31-state-relation` 的待办行**至今未勾**、`SPEC-02-states` §3.4 **只给了形状**。〔**2026-10-13 追注**：**该段的前提已到期**——"语义拍板"这一步**已于 2026-10-13 完成**（`DEC-2026-10-13-relation-semantics`；`DEC-2026-08-31` 的待办行已由新载体接管 + 追加注记）。⇒ `g` 的依赖**分两半**（`e1` / `e2`），见上表追注。**原文保留记现场**（它描述的是 2026-10-11 的实况）。〕
- ⇒ `g` **不是"随时可开"**，其头部内容卡在一件从未做过的事上。**该订正 MUST 落在批次表关账块**（就地加实施注记，MUST NOT 抹除原文）。
- **同批订正**：`PLN-R6` §轮次切分表把 `R6.5` 描述为"**寄在 R6 与 R7 之间，不占正式轮**"。本轮实际开了**独立计划 + 独立 Task 骨架**，体量已不是"余项"那一档 ⇒ 该描述亦需就地注明（见 Task 6 Step 4）。

**其二（本计划的载体选择本身值得登记）**：`R4.5` 与 `R5.5` 两个非正式轮号**至今只有批次表、从未有过 Task 骨架**，八批全停在"❌ 未开工"。**根因不是它们不需要，而是"批次表长得像计划"**——批次表答"**还剩什么**"（台账形状），计划答"**怎么做、怎么验**"（执行形状）。本计划是该约定下**第一个拥有真实 Task 的非正式轮号**，MUST 在 `INDEX` §4.3 如实登记。〔**2026-10-13 追注**：本句写于 **2026-10-11**，"无 Task 骨架"这一**载体判据至今成立**（`R4.5` 的三批始终是并入 `PLN-R5` Task 3 / `PLN-R6` Task 0 落地，从未长出自己的 Task 骨架）。**但"八批全未开工"这半对 `R4.5` 已过期**——其 a/b/c 三批已于 2026-09-29 / 10-05 / 10-06 全部收口，`PLN-R4` 随而转 `FROZEN`；`R5.5` 那八批状态不变。原文保留不改写（记现场）。〕

### 0.5 非目标（本轮不做，逐条带归属）

| 面 | 归属 |
|---|---|
| **`FBoolSwitchModifier` + `GateCheck` 条件求值器**（含 `WAIT-14` 的 `Mode` 零读取者） | **批次 `e`**（另一轮） |
| **`UTcsSkillModDef` 技能侧参数行分派** | **批次 `f`** |
| **多形态组合范式完整走查 / `CQM_Custom` / `MCS_Custom` 两档真实现** | **批次 `g`**（~~被 `R5.5-e` 阻塞~~，见 §0.4 其一）〔**2026-10-13 追注**：`R5.5-e` 的**第一步"字段语义拍板"已于该日完成**（`DEC-2026-10-13-relation-semantics`）⇒ "被阻塞"已过期。**分两半读**：「关系字段门禁」那半依赖 `PLN-R5.5` Task 1（`e1`）；`CQM_Custom` / `MCS_Custom` 两档依赖 `PLN-R5.5` Task 2（`e2`）。原文保留记现场。〕 |
| **`ECostPolicy::ECP_CustomFragment` 真实现** | **触发条件**（**★ 2026-10-12 改正：阻塞 = `COST-3`（扩展点形状未裁）**，原记 `SCRIPT-9` / `R5.5-h` **不成立**，见 §0.2 裁决 ④） |
| **`ITcsCooldownTimingFragment`** | **整体删除**（裁决 ①——从未需要，MUST NOT 补回） |
| **打断来源优先级仲裁** | `R5.5-e`（R6 已如实登记未落，本轮不动） |
| **网络复制姿态** | `SPEC-04-skill` §6：接口位本期不实现 |
| **`OnCooldownUpdated` 的 UI 径向表现** | 不做 UI（§7 非目标）；本轮只保证"真值在 Entry、可查" |

### 0.6 本轮**不落**但已定位的缺口（登记 + 触发条件，MUST NOT 读成"未定"）

> **登记判据**（`LEDGER-deferred`《入册判据》第 3 条）：**有可指归属/触发条件的，入台账；只有指不出归属的才读成"未定"**。本节两条**均入台账**（触发条件型），此处同时留档以便后续轮次不再重新发现它们。

**缺口 ①：Cost 侧没有与 `CooldownPct` 对称的"折扣系数"通道（2026-10-11 用户提问暴露）。**

- **现象（一处真实的设计不对称）**：同族两处都是"**基数 × 系数**"的形状——

  | | 基数 | 系数通道 |
  |---|---|---|
  | 冷却 | `FTcsCooldownTrack::Duration` | **`CooldownPct`**（`D5-15` 明文走参数链、快照于触发时刻） |
  | Cost | `FTcsCostConfig::Amount` | ❌ **没有对手**（`D5-10` 字段表里从未有过对应的键） |

- **后果**：用户在 2026-10-11 提问"日后想实现特定条件下只 Commit 一半或 30%，该怎么实现"——**按本轮设计接不住**：`Amount` 是字面量 `double`（§2.2），且无折扣键可挂。
- **★ 缺的那处是遗漏，不是设计**（判据 = **对称性**，非偏好）：两者**没有任何实质区别**，都是"一个乘在数上的系数"；`Duration` 侧有系数通道，而 `Amount` 侧没有。
- **修法（三条路，代价已核）**：

  | 路 | 做法 | 代价 |
  |---|---|---|
  | **A（推荐）** | 照 `CooldownPct` 加同族契约键 `CostPct`，`CommitCost` 里乘上它 | **极小**——与 `CooldownPct` **逐字同构**，同一套参数链、同一套快照语义、**零新机制** |
  | B | `Amount` 改成参数源载体（`FInstancedStruct`，同 `Duration(字面量\|Param)` 的批注意图） | **大**——要动 Def 形状 + 参数源在技能侧的消费者问题（`STAT-11`：`AttributeScaled` 至今零实现者，同族） |
  | C | 宿主在提交点外自己扣、绕过 `CommitCost` | **MUST NOT**——会**破坏门禁⑤**：`CanAffordCost` 判的与真扣的数不一致，**这正是本计划 Task 2 Step 4 那条 TOCTOU 防护要防的形态** |

- **"特定条件"由谁判（MUST 写清，防走错路）**：**由宿主在挂修正器那一刻自己判**（`ModifierRows` 声明式 / `ApplyParamModifiers` 命令式都在宿主手上）——**条件系统是宿主的事，框架只负责"给一个能被修正的系数槽"**。这与 `CooldownPct` 的既有分工**逐字相同**。
- **本轮为何不做（边界判据，非"没时间"）**：`unreal-development-workflow` 的四条边界判据**第 3 条**——**取值归属 > 有无消费者**。加 `CostPct` **不越界**（只引用框架自身契约"属性值是个数"，不含宿主词汇）；**但"要不要有消耗折扣"是玩法结论**：`D5-15` 给冷却定 CDR 是**当时就拍了的**，而 Cost 侧**从未拍过**。⇒ 今天**没有真实内容需求**，现在加就是本计划反复拦的"**零消费者预建**"。
- **触发条件**：出现"**消耗需要按条件打折**"的真实内容需求（例如"装备减耗 30%" / "某状态下技能半价"）。
- **归属**：**触发条件型**（已入 `LEDGER-deferred`，见变更记录）。

**缺口 ②：`FTcsCooldownTrack::Duration` 的 `Param` 形态（批次表 `a` 的 `Duration(字面量\|Param)` 后半）。**

- 本轮只落**字面量** `double`；`Param` 形态要求 `Duration` 变成参数源载体——与缺口 ① 的"路 B"**是同一个技术问题**（参数源在技能侧的消费者缺失）。
- **⇒ 两条 MUST 同批裁**（任一落地时另一条的形状随之定），MUST NOT 分两次各造一套。
- **触发条件**：出现"**冷却时长要按参数算**"的真实内容需求。
- **归属**：**触发条件型**（已入 `LEDGER-deferred`）。

**缺口 ③（如实登记，非本轮职责）：`FTcsCostConfig::Amount` 只支持"单资源"**——`SPEC-04-skill` §7 非目标明文"不做多资源消耗（v1 单资源，多资源 = 未来新原语）"。MUST NOT 读成纰漏。

---

## 1. 文件结构（先锁分解，再切 Task）

**新建（`TcsSkill` 内部，全部落 `Public/Def/` 或 `Private/Skill/`）**：

| 文件 | 职责 |
|---|---|
| `Source/TcsSkill/Public/Def/TcsCooldownTypes.h` | 冷却策略族：`ETcsCooldownTiming`（四档）/ `FTcsCooldownTrack`（`Duration` 载体 + `GroupTag`）/ `FTcsCooldownPolicy`（`Tracks` 数组）。**纯数据、零行为**（同 `TcsBoolSwitchRow.h` 的处置） |
| `Source/TcsSkill/Public/Def/TcsCostTypes.h` | Cost 策略族：`ETcsCostPolicy`（三档）/ `ETcsCostTiming`（四档）/ `FTcsCostConfig`（`Policy` + `Timing` + `ResourceAttr` + `Amount`）。**同上** |
| `Source/TcsSkill/Public/Skill/TcsCooldownEvents.h` | 三枚冷却事件 Tag + 载荷 `FTcsCooldownEventPayload`（四枚共用一种形状，沿 `FTcsCastEventPayload` 先例） |
| `Source/TcsSkill/Public/Skill/TcsCostEvents.h` | `OnCostApplied` 事件 Tag + 载荷 `FTcsCostEventPayload` |
| `Source/TcsSkill/Private/Skill/TcsCooldownOps.cpp` / `.h`（头落 `Public/Skill/`） | 冷却运行态算子：起轨 / 查可放 / 改轨 / 清轨 / 到期回收。**单一真值入口** |
| `Source/TcsSkill/Private/Skill/TcsCooldownSubsystem_Tick.cpp` | 到期回调的落点（唤醒 → 改轨 → 发 `OnCooldownEnded`） |
| `Source/TcsSkill/Private/Skill/TcsCostOps.cpp` / `.h`（头落 `Public/Skill/`） | Cost 算子：`CanAfford` / `PayCost` 提交 / 发 `OnCostApplied`。**唯一实现**（门禁⑤ 与提交点都调它） |
| `Source/TcsSkill/Public/Skill/TcsCooldownQuery.h` | 只读查询契约（`GetCooldownRemaining` / `GetCooldownTotal` / `IsOnCooldown`）——**UI 轮询面**（`D5-15`："真值在 Entry，UI 轮询"） |
| `Source/TcsSkill/Private/Skill/TcsCooldownEventRelay.cpp` | `ECT_OnEvent` 档的事件订阅落点（一个 `UTcsEventHandler` 派生，按 Tag 订阅，到点起冷却 / 提交 Cost） |
| `Source/TcsSkill/Public/Param/TcsSkillParamKeys.h` | `CooldownPct` 契约键的原生声明（照 `TcsStateParamKeys.h` 先例，含三条判据注释） |

**修改**：

| 文件 | 改动 |
|---|---|
| `Source/TcsSkill/Public/Skill/TcsSkillRegistry.h` | `FTcsLearnedSkillEntry` **回补** `TArray<FTcsCooldownTrackState> CooldownTracks`（R6 Task 2 因"类型不存在"刻意移除，本轮类型与消费者同时到位——同 `ParamChain` 的回补先例） |
| `Source/TcsSkill/Public/Def/TcsSkillDefData.h` | 新增 `FTcsCooldownPolicy Cooldown` + `FTcsCostConfig Cost` 两字段（R6 §11 记为"明确不落的字段"，本轮正是它们的消费者到位之时） |
| `Source/TcsSkill/Public/Def/TcsSkillEnums.h` | **不改**（两个 Timing 枚举各有独立语义，各住自己的类型头；MUST NOT 挤进本文件造成"一个文件两套枚举"的误读） |
| `Source/TcsSkill/Private/Skill/TcsCastOps.cpp` | 门禁③（`:163`）与门禁⑤（`:237`）两处占位 → 真实现（调 `TcsCooldownOps` / `TcsCostOps`） |
| `Source/TcsSkill/Private/Skill/TcsCastOps_Phase.cpp` | 四路终结里挂冷却/Cost 的 Timing 触发点（`OnCastCompleted` / `OnCastInterrupted` 档） |
| `Source/TcsSkill/Public/TcsSkillSubsystem.h` | 新增公开口：`AdjustCooldown` / `ResetCooldown` / `StartCooldown` / `PayCost` / 三个查询口 + 单位级组槽（`TMap<FGameplayTag, FTcsCooldownGroupState>`） |
| **`Source/TcsAttribute/Public/TcsAttributeSubsystem.h`** + **`Private/TcsAttributeSubsystem.cpp`** | **本轮对 `TcsAttribute` 的唯一改动**：+1 个门面转发口 `GetBaseValue(Unit, Attribute)`——转发现有 `FTcsAttributeStore::GetBaseValue`（`TcsAttributeStore.h:61`），零新逻辑。**理由**：Cost 的 `ECP_ResourceAttr` 档必须"读基值 → 减 → 写回"，而门面今天只有 `EvaluateCurrent`（**折叠后**当前值）与 `PeekPending`，**都读不到基值**；从 `TcsSkill` 直伸 `ResolveStore` 会击穿 M2 门面 |
| `Source/TcsSkill/TcsSkill.Build.cs` | **不改**（零新依赖边——已含 `TcsCore` / `TcsAttribute` / `TcsEffect`） |

**宿主装置（LAC 仓 `Source/TcsDev/`）**：

| 文件 | 职责 |
|---|---|
| `Private/Dev/TcsDevSliceRig_CooldownCost.cpp`（新） | `Tcs.Test.CooldownCost.Run` / `.Reject`。**屏显段 `0x7C5D9000`**（`0x7C5D8000` 已被 R6 Task 6 占满） |
| `Public/Dev/TcsDevSliceRig_CooldownCost.h`（新） | 命令注册声明 |
| `Private/Dev/TcsDevSliceRig.cpp` | **+2 行**：include + `RegisterCommands()`（照 R6 Task 6 先例） |

---

## 2. 落位表（类型与签名——所有 Task 的 Interfaces 都引用本表，MUST NOT 各写一份）

### 2.1 枚举（两套，各自独立）

```cpp
// TcsCooldownTypes.h —— 冷却时机（四档；无 OnEvent：冷却由"自己起"或"宿主起"驱动）
UENUM(BlueprintType)
enum class ETcsCooldownTiming : uint8
{
    ECT_OnCastStarted     = 0,  // 默认：激活成功后立即起
    ECT_OnCastCompleted   = 1,  // 施法自然完成时起
    ECT_OnCastInterrupted = 2,  // 施法被打断时起（含顶替与瞬发？——见 Task 3 Step 2 的判据）
    ECT_Manual            = 3,  // 框架不自动起；宿主自行调 StartCooldown
};

// TcsCostTypes.h —— Cost 时机（四档；无 OnCastInterrupted，判据见 §0.2 裁决 ③）
UENUM(BlueprintType)
enum class ETcsCostTiming : uint8
{
    ECT_OnCastStarted   = 0,  // 默认：激活事务内提交
    ECT_OnCastCompleted = 1,  // 施法成功完成时提交
    ECT_OnEvent         = 2,  // 指定事件到达时提交（Tag 住 FTcsCostConfig::TriggerEventTag）
    ECT_Manual          = 3,  // 框架不自动提交；宿主自行调 PayCost
};

// TcsCostTypes.h —— Cost 策略（三档）
UENUM(BlueprintType)
enum class ETcsCostPolicy : uint8
{
    ECP_None           = 0,  // 默认：不消耗 ⇒ 恒可付
    ECP_ResourceAttr   = 1,  // 走 M2 属性账本（ResourceAttr 键 + Amount）
    ECP_CustomFragment = 2,  // 本轮具名未实现 + 回退 ECP_None（★ 阻塞 = COST-3；原记 SCRIPT-9 已于 2026-10-12 改正，见 §0.2 裁决 ④）
};
```

> **命名判据（`MEM-20261009-04`：同族对称 + 先扫字面占用）**：`ECost*` 族沿 R6 的 `ECastInstancing` / `ECastQueryMode` / `EMainChainStart` 三处**全部带前缀**的惯例取 `EC*` 前缀；`ETcsCooldownTiming` / `ETcsCostTiming` 带 `ETcs` 前缀。**实测这两个名字与 `ECost*` 全族在 `Source/` 均 0 命中**（§0.3），无占用冲突。
> **一处刻意的不对称（MUST NOT "修正"）**：冷却族用 `ETcsCooldown*` 而后缀族用 `ECost*`——因为 `ECast*` / `ECost*` 是 R6 已立的**短前缀惯例**（枚举名首词即域），而 "Cooldown" 不在该短前缀集内，硬取 `ECooldown*` 会造出第四个短前缀、且与 `ECast*` 视觉上只差一个字母（`ECast` vs `ECooldown` 不混淆，但 `EC*` 分族变模糊）。**这是一处"同族对称"与"字面占用"都满足后的取舍，不是遗漏。**

### 2.2 结构体

```cpp
// TcsCooldownTypes.h
USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCooldownTrack
{
    GENERATED_BODY()
    // 本轨时长（秒）。本轮只落字面量；`Duration` 用参数链取值的形态归后续轮（见 Task 1 Step 4 的边界）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) double Duration = 0.0;
    // 组轨身份：无效 = 自身轨；有效 = 写单位级组槽（同组所有条目共用一个槽）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag GroupTag;
};   // ★ GroupTag 的问号（批次表 `:198`）已关闭：字段恒存在，无效 tag = 自身轨

USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCooldownPolicy
{
    GENERATED_BODY()
    // 空数组 = 永远可放（None 档的数据表达；`D5-15`：Strategy 三档"溶解进轨道"）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FTcsCooldownTrack> Tracks;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ETcsCooldownTiming Timing = ETcsCooldownTiming::ECT_OnCastStarted;
};   // ★ 无 CustomFragment 字段（裁决 ①）；Timing 是 policy 级（`D5-15` 明文）

USTRUCT(BlueprintType)
struct TCSSKILL_API FTcsCostConfig
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ETcsCostPolicy Policy = ETcsCostPolicy::ECP_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ETcsCostTiming Timing = ETcsCostTiming::ECT_OnCastStarted;
    // 消耗哪个属性（仅 ECP_ResourceAttr 档有效）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag ResourceAttr;
    // 数量（仅 ECP_ResourceAttr 档有效；>0）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) double Amount = 0.0;
    // 仅 ECT_OnEvent 档有效：到哪个事件提交（无效 tag = 该档不成立，MUST 具名 Warning 并回退默认档）
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag TriggerEventTag;
};
```

> **★ `FTcsCostConfig::Timing` 的枚举名已定**（批次表 `:201` 原文写 `FCostConfig{Policy: ..., Timing}`，**未定枚举名**）⇒ 本轮定为 **`ETcsCostTiming`**，字段类型明确。**同一处 `Policy` 的枚举名定为 `ETcsCostPolicy`**（`SPEC-04-skill` §11.3 记 `ECostPolicy` "均未落地（正文提及处只作形状意图）"——现已落地，**MUST NOT 再引用 `ECostPolicy` 那个旧写法**）。

### 2.3 运行态（非反射，沿 `FTcsCastRun` 处置）

```cpp
// TcsCooldownOps.h
struct FTcsCooldownTrackState
{
    double Total = 0.0;                  // 起冷时冻结的时长（CDR 已作用）
    double EndTimeSeconds = 0.0;         // 到期绝对时刻（FTcsClock::Elapsed 时轴）
    FTcsTimeEntryHandle ExpiryEntry;     // 到期条目（改轨/清轨 MUST 先 CancelExpiry 再重挂）
};

// 单位级组槽（住 UTcsSkillSubsystem）：同组所有条目共用
struct FTcsCooldownGroupState
{
    double Total = 0.0;
    double EndTimeSeconds = 0.0;
    FTcsTimeEntryHandle ExpiryEntry;
    FGameplayTag GroupTag;
};
```

### 2.4 公开口（`UTcsSkillSubsystem`，全部 `UFUNCTION()` 无 specifier = 脚本可达、蓝图不可见）

```cpp
// —— 查询（UI 轮询面；`D5-15`"真值在 Entry，UI 轮询"）
UFUNCTION() bool  IsOnCooldown(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry);
UFUNCTION() double GetCooldownRemaining(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry, int32 TrackIndex);
UFUNCTION() double GetCooldownTotal(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry, int32 TrackIndex);
UFUNCTION() int32 GetCooldownTrackCount(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry);

// —— 可变性（`D5-16` 承诺的两个 + 本轮新增两个）
UFUNCTION() void AdjustCooldown(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry, int32 TrackIndex, double Delta);  // 发 OnCooldownUpdated
UFUNCTION() void ResetCooldown(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry, int32 TrackIndex);                // 清该轨（发 OnCooldownUpdated）
UFUNCTION() void StartCooldown(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry);                                   // ★ 新增：起全部自身轨 + 组轨
UFUNCTION() bool PayCost(FTcsCombatEntityHandle Unit, FTcsSkillEntryHandle Entry);                                         // ★ 新增：按 Policy 提交 + 发 OnCostApplied

// —— 门禁/提交共用（**唯一实现**，MUST NOT 各写一份）
// 住 TcsCostOps.h（模块内部头，不进 UFUNCTION 面）
bool CanAffordCost(UTcsSkillSubsystem& Subsystem, const FTcsCombatEntityHandle& Unit, const FTcsCostConfig& Config);
void CommitCost(UTcsSkillSubsystem& Subsystem, const FTcsCombatEntityHandle& Unit, FTcsSkillEntryHandle Entry, const FTcsCostConfig& Config);
```

### 2.5 事件（四枚，全部落既有 `TcsEvent.Cast` 根——MUST NOT 新增根）

| Tag | 产生者 | 载荷 |
|---|---|---|
| `TcsEvent.Cast.CooldownStarted` | `StartCooldown`（自动档与 Manual 档**同一条路径**） | `FTcsCooldownEventPayload` |
| `TcsEvent.Cast.CooldownUpdated` | `AdjustCooldown` / `ResetCooldown` | 同上 |
| `TcsEvent.Cast.CooldownEnded` | 到期堆回调（**权威侧**） | 同上 |
| `TcsEvent.Cast.CostApplied` | `CommitCost`（`ECP_ResourceAttr` 档） | `FTcsCostEventPayload` |

> **命名判据**：`D5-16` 原文用的是 `OnCooldownStarted` / `OnCooldownUpdated` / `OnCooldownEnded` / `OnCostApplied`。**落地名去掉 `On` 前缀**——`TcsEvent.Cast` 根下既有的四枚是 `Started` / `PhaseChanged` / `Completed` / `Interrupted`（**零 `On` 前缀**）。⇒ 按**同族对称**（`MEM-20261009-04`）取 `CooldownStarted` 等；且 "Cooldown" 段把三枚冷却事件与四枚施法事件区分开。**MUST NOT 照抄 `On*` 进 tag 名**（那会让同一根下两种命名风格并存）。
> **根数不变**：全部落 `TcsEvent` 根（域段 = `Cast`），`gameplay-tag-governance` 的 12 个根**不增**（"增根不增层"）。重叠判定需在 Task 5 Step 5 实测复核。
> **导出宏**：四枚皆 `extern TCSSKILL_API FNativeGameplayTag`（跨模块引用的实证要求见 `TcsCastEvents.h:28`——裸 `extern` 会 `LNK2001`）。

---

## 3. Task 骨架（Task 0 ~ 6）

> **Task 切分判据**（`superpowers:writing-plans` 的 right-sizing + 本仓先例）：**每个 Task 的交付物都能被独立拒绝**。Task 0 是硬前置（类型不存在则一切编译失败）；Task 1~2 是两条竖切（冷却 / Cost），各自可独立验收；Task 3 是接缝（终结路径的 Timing 触发点，冷却与 Cost 共用）；Task 4 是宿主装置与取证；Task 5 是事件词汇；Task 6 是收束。
> **★ 依赖顺序是硬的**：Task 1 与 Task 2 之间**无依赖**（可并行），但**两者都必须在 Task 3 之前**（Task 3 要调它们的终结入口）。

### Task 0 — 类型层落地（硬前置，零行为变更）

**目标**：把 §2.1 / §2.2 的类型全部落地并通过双配置编译。**本 Task 零行为变更**——只有类型与字段，无任何调用点（除 `FTcsLearnedSkillEntry` 的字段回补）。这是"类型先落地、行为后接"的既有节奏（同 R6 Task 1 Step 2 先例）。

**Files:**
- Create: `Source/TcsSkill/Public/Def/TcsCooldownTypes.h`
- Create: `Source/TcsSkill/Public/Def/TcsCostTypes.h`
- Create: `Source/TcsSkill/Public/Param/TcsSkillParamKeys.h`
- Modify: `Source/TcsSkill/Public/Def/TcsSkillDefData.h`（+2 字段）
- Modify: `Source/TcsSkill/Public/Skill/TcsSkillRegistry.h`（+1 字段 `CooldownTracks`）
- Modify: `Source/TcsSkill/Private/TcsSkillSubsystem.cpp`（`CooldownPct` tag 定义）

**Interfaces:**
- Consumes: `FTcsTimeEntryHandle`（`TcsExpiryHeap.h:21`）、`FGameplayTag`、`FTcsNumericParamRow`（`TcsStateDefBase.h:70`）
- Produces: §2.1 全部三枚举、§2.2 全部三结构体、§2.3 `FTcsCooldownTrackState`；`Tag_TcsSkillParam_CooldownPct`（`TcsSkillParamKeys.h`）

- [x] **Step 1：落 `TcsCooldownTypes.h`**——§2.1 的 `ETcsCooldownTiming` + §2.2 的 `FTcsCooldownTrack` / `FTcsCooldownPolicy`。文件头注释 MUST 写清 **`GroupTag` 的问号已关闭**（无效 tag = 自身轨）与 **`CustomFragment` 字段被裁决①删除、MUST NOT 补回**（判据见 `PLN-R6.5-1` §0.2 裁决 ①）。
- [x] **Step 2：落 `TcsCostTypes.h`**——§2.1 的 `ETcsCostPolicy` / `ETcsCostTiming` + §2.2 的 `FTcsCostConfig`。文件头注释 MUST 写清 **`ETcsCostTiming` 无 `OnCastInterrupted` 档的两条硬判据**（§0.2 裁决 ③）。
- [x] **Step 3：落 `TcsSkillParamKeys.h`**——`Tag_TcsSkillParam_CooldownPct`，值 `TcsStateParam.CooldownPct`。**声明归属判据 MUST 写成三条**（照 `TcsStateParamKeys.h` 的 `Level` 先例）：① **语义归属**——CDR 的公式（"作用于各轨 Duration"）写在 `D5-15`、由**技能冷却域**解释，状态侧无此概念；② **依赖方向**——`TcsSkill` 已依赖 `TcsState`，但反向不成立 ⇒ 住 `TcsSkill` **零成环风险**（与 `Level` 住 `TcsState` 是**同一判据的相反结论**，MUST 写出这个对照，防后人误以为不一致）；③ **声明方 = "谁解释那个词"**——宿主只是"往里挂修正器"。
- [x] **Step 4：`FTcsCooldownTrack::Duration` 的取值形态边界（如实登记）**——批次表 `a` 原文写 `FCooldownTrack{Duration, GroupTag?}`，`D5-15` 原话是 `Duration(字面量|Param)`。**本轮只落字面量** `double`：`Param` 形态要求 `Duration` 变成 `FInstancedStruct` 参数源载体，而**参数源族在技能侧尚无消费者**（R6 的 `AttributeScaled` 即因零实现者落空）。⇒ 本 Step 在字段注释里写明该边界与归属（触发条件 = 出现"冷却时长要按参数算"的真实内容）。
- [x] **Step 5：回补 `FTcsLearnedSkillEntry::CooldownTracks`**——`TArray<FTcsCooldownTrackState>`。注释 MUST 写明：**R6 Task 2 曾因"元素类型此刻全仓不存在 ⇒ 硬编译错误"刻意移除本字段**（`TcsSkillRegistry.h:30` 原文），**本轮类型（Task 0 Step 1）与消费者（Task 1）同时到位** ⇒ 回补成立，同 `ParamChain` 的回补先例。
- [x] **Step 6：`FTcsSkillDefData` 新增两字段**——`FTcsCooldownPolicy Cooldown` / `FTcsCostConfig Cost`。注释 MUST 说明"R6 §11.2 曾把'冷却策略 / Cost 配置'记为**明确不落的字段**（零消费者不预建），本轮消费者到位 ⇒ 字段随之落地"。
- [x] **Step 7：双配置编译**——`unreal-cpp-compile` 技能探测引擎路径后跑 **Development Editor + Game Shipping**。
  - 期望：`Result: Succeeded`，**0 error / 0 warning**。
  - 失败排查：新增 `UPROPERTY` 的 `TArray<T>` 元素类型**必须完整**（UHT 硬要求——`MEM-20261006-15` 的同一条判据）；若报"未声明标识符"，检查 include 链（`TcsCooldownTypes.h` 需先于 `TcsSkillRegistry.h` 被包含）。
- [x] **Step 8：归档前无损预检 + 提交**——本 Task 走 OpenSpec 提案 `add-cooldown-and-cost-types`（见 Task 6 Step 1 的提案清单）。

**非目标**：不做任何行为（无门禁、无到期堆、无事件）；不做 `Duration` 的 `Param` 形态；不做 `ECP_CustomFragment` 真实现。

---

### Task 1 — 冷却多轨道（批次 `a`）

**目标**：`FTcsCooldownPolicy` 的 Tracks 数组变成真的运行态——自身轨与共享组轨并存，到期堆驱动，CDR 快照于触发时刻，`IsOnCooldown` 可查、`StartCooldown` 可起。**一轮做完"能放不能放"这一件事**（净信号）。

**Files:**
- Create: `Source/TcsSkill/Public/Skill/TcsCooldownOps.h` + `Private/Skill/TcsCooldownOps.cpp`
- Create: `Source/TcsSkill/Private/Skill/TcsCooldownSubsystem_Tick.cpp`
- Create: `Source/TcsSkill/Public/Skill/TcsCooldownQuery.h`（查询面声明）
- Modify: `Source/TcsSkill/Public/TcsSkillSubsystem.h`（单位级组槽 + 四个公开口）
- Modify: `Source/TcsSkill/Private/Skill/TcsCastOps.cpp:163`（门禁③真实现）
- Modify: `Source/TcsSkill/Public/Def/TcsSkillActivateResult.h`（`SAR_OnCooldown` 的注释去掉"R6.5 占位"）

**Interfaces:**
- Consumes: Task 0 的全部类型；`UTcsClockSubsystem::PushExpiry` / `CancelExpiry`；`FTcsLearnedSkillEntry::CooldownTracks`；`Run->PhaseIndex`（终结路径用）
- Produces: `StartCooldown(Unit, Entry)`（公开口，Task 3 的 Manual 触发点调它）、`IsOnCooldown` / `GetCooldownRemaining` / `GetCooldownTotal` / `GetCooldownTrackCount`（Task 4 装置读数面）、`AdjustCooldown` / `ResetCooldown`（Task 5 的事件源）

- [x] **Step 1：落 `TcsCooldownOps` 的四个纯函数**——`StartCooldownOnEntry` / `IsEntryOnCooldown` / `AdjustTrack` / `ClearTrack`。签名住 `TcsCooldownOps.h`，**全部收 `UTcsSkillSubsystem&` 显式形参**（MUST NOT 用单例取子系统——既有算子的统一处置）。
- [x] **Step 2：起轨的核心算法**——对每条 `Track`：① 取 `Duration`；② 乘 CDR；③ 算 `EndTimeSeconds = Clock.GetClock().Elapsed + EffectiveDuration`；④ `PushExpiry(EndTimeSeconds, OwnerId, OnDue)`。
  - **★ 定位方式 MUST 照抄 R6 的既有先例，MUST NOT 发明"可逆编码"**（本计划自审抓到的一处初稿错误）。**先例原文**（`TcsCastOps_Phase.cpp:198-206`）：`OwnerId` 传**来源 Id、只作回调归属读数**（注释原文"堆零领域词汇，只作回调归属读数"），**真正的定位靠 lambda 捕获句柄**——`[WeakSubsystem, PhaseRunHandle](uint64 /*OwnerId*/){ ... }`。
    ⇒ 本 Task **同款**：捕获 `[WeakSubsystem, Unit, EntryHandle, TrackIndex]`，`OwnerId` 只填归属读数。**为什么冷却要多捕获 `Unit`**：冷却的真值住在**账本条目**上（不在 run 上），而 `FTcsSkillEntryHandle::Index` **只在单位桶内有意义**（`TcsSkillRegistry.h:53` 明文）⇒ 必须连同 `Unit` 一起捕获。**这正是 `FTcsCastRun::Unit` 存在的同一条理由**（`TcsCastRun.h:56-63`）。
    **MUST NOT 把定位职责压给 `OwnerId`**——那会与 R6 已确立的堆使用约定造出**第二真相**（同一个堆、两种回调约定）。
  - **组轨的处置**：`GroupTag` 有效时写**单位级组槽**（`TMap<FGameplayTag, FTcsCooldownGroupState>`），MUST NOT 重复写进每条目的轨道状态。**同组已有活槽时**：按 `D5-15`"公共冷却 = 共享组轨道"语义，**MUST 复用该槽**（不重起、不叠加）——判据见 Step 3。
- [x] **Step 3：`IsEntryOnCooldown` 的判据（`D5-15` 原文）**——"**可施放 = 全部自身轨道清空 + 所属组槽清空**"。MUST 写成一条短路遍历：任一自身轨未清 **或** 任一所属组槽未清 ⇒ `true`。
  - **边界（MUST 处理，不得崩）**：`Tracks` 为空 ⇒ **恒可放**（`D5-15`："默认无冷却（空 Tracks）"）；`GroupTag` 无效 ⇒ 该轨只在自己条目上算；组槽存在但**该组无任何条目登记**（陈旧槽）⇒ 视为已清并记 `Log`（MUST NOT 静默保留永久堵死）。
- [x] **Step 4：CDR 快照于触发时刻**——`CooldownPct` 从该条目的参数链取值，**在起冷那一刻取一次**并作用于各轨 `Duration`（`D5-15` 明文"快照于冷却触发时刻，中途变化不追溯"）。
  - **边界**：参数链上无 `CooldownPct` 键 ⇒ CDR = 0（无缩减）；`CooldownPct` 取到的值 MUST clamp（负值不得放大冷却；`>1` 不得产生负时长）——**clamp 的确切上下界 MUST 在证据里写出来**，不许"合理处理"。
- [x] **Step 5：到期回调（`TcsCooldownSubsystem_Tick.cpp`）**——到期 ⇒ ① 解码 `OwnerId` 定位条目与轨；② **先把该轨的 `ExpiryEntry` 复位**（见 Step 6 的 ensure 陷阱）；③ 清该轨；④ 若该条目**全部轨 + 组槽皆已清** ⇒ 允许再次激活（无需额外状态——门禁③ 每帧现算，不再有缓存态）；⑤ 发 `OnCooldownEnded`（Task 5 落，本 Step 先留调用点）。
- [x] **Step 6：★ 一处真实的 `ensure` 陷阱（MUST 照抄 R6 的既有处置）**——`CancelExpiry` 对**悬空**句柄是 `ensureMsgf`（留红字，`TcsExpiryHeap.h:92`）。而"到期回调正在跑"那一刻它自己的条目**仍然有效**（`FireDueItem` 在回调**返回之后**才释放，`:223-246`）⇒ **撤销必须发生在回调入口复位之后**：到期回调的**第一条语句**即把该轨的 `ExpiryEntry` 置空。**判据同`FTcsCastRun::PhaseExpiryEntry` 的既有注释**（`TcsCastRun.h:178-181`），MUST 在该处引用它。
- [x] **Step 7：池槽复用纪律（MUST 照抄）**——`FTcsCooldownTrackState` 若经池分配，**复用槽后 MUST 先整体重置**（`*State = FTcsCooldownTrackState();`），MUST NOT 逐字段赋值。判据见 `TcsCastOps.cpp:252-260`（R6 实测抓到的真实潜伏缺陷）。
- [x] **Step 8：`AdjustCooldown` / `ResetCooldown`**——改动轨状态后 **MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）；MUST NOT 只改 `EndTimeSeconds` 而不重挂（那会让堆里的旧到期时刻继续生效，症状 = "改了却不生效"或"提前到期"）。发 `OnCooldownUpdated`（Task 5 落，本 Step 留调用点）。
- [x] **Step 9：门禁③ 真实现**（`TcsCastOps.cpp:163`）——占位的两行注释替换为 `if (IsEntryOnCooldown(...)) { log 具名; return SAR_OnCooldown; }`。**MUST 复用 Step 3 的同一函数**（MUST NOT 内联一份判断）。日志 MUST 含**哪条轨/哪个组**（滞留在哪一轨是宿主最需要的读数）。
- [x] **Step 10：双配置编译** —— `Result: Succeeded`，0 error / 0 warning。
- [x] **Step 11：装置取证（Task 4 的装置就绪后执行；本 Task 先记录待验清单）**——待验项：自身轨起冷后 `SAR_OnCooldown`；组轨跨条目共享（A 起冷 ⇒ B 同组条目也放不出 = **真 GCD 的数据表达**）；CDR 快照不追溯（起冷后改参数 ⇒ Remaining 不变）；空 Tracks 恒可放；到期后可再放。

**非目标**：不做事件（Task 5）；不做 Cost（Task 2）；不做 Timing 四档的触发点分派（Task 3）；不做 `Duration` 的 `Param` 形态（Task 0 Step 4 已登记边界）。

---

### Task 2 — Cost 策略 × 时机（批次 `d`，与 Task 1 无依赖）

**目标**：`CanAfford` 门禁第 ⑤ 道真实现 + `PayCost` 提交 + `OnCostApplied`；`ECP_ResourceAttr` 档走 M2 属性账本；`ECP_CustomFragment` 档具名未实现 + 回退。

**Files:**
- Create: `Source/TcsSkill/Public/Skill/TcsCostOps.h` + `Private/Skill/TcsCostOps.cpp`
- **Modify: `Source/TcsAttribute/Public/TcsAttributeSubsystem.h`**（**本轮对 `TcsAttribute` 的唯一改动**：+1 个门面转发口 `GetBaseValue`，转发现有 `FTcsAttributeStore::GetBaseValue`，零新逻辑——判据见 Step 2）
- **Modify: `Source/TcsAttribute/Private/TcsAttributeSubsystem.cpp`**（同一入口的实现，一行转发）
- Modify: `Source/TcsSkill/Private/Skill/TcsCastOps.cpp:237`（门禁⑤真实现）
- Modify: `Source/TcsSkill/Public/TcsSkillSubsystem.h`（`PayCost` 公开口）
- Modify: `Source/TcsSkill/Public/Def/TcsSkillActivateResult.h`（`SAR_CannotAfford` 注释去掉"R6.5 占位"）

**Interfaces:**
- Consumes: Task 0 的 `FTcsCostConfig` / `ETcsCostPolicy` / `ETcsCostTiming`；`UTcsAttributeSubsystem::EvaluateCurrent`（读当前值，`:219`）、`SetBaseValue`（写基值，`:181`）
- Produces: `CanAffordCost(...)` / `CommitCost(...)`（**唯一实现**，Task 3 的终结触发点与 Manual 入口都调它）、`PayCost(Unit, Entry)` 公开口、**`UTcsAttributeSubsystem::GetBaseValue(Unit, Attribute)`**（新门面口）

- [x] **Step 1：`CanAffordCost`**——**★ 四类分开处置（MUST NOT 二选一，判据见下）**：

  | `Policy` | `ResourceAttr` | `Amount` | 返回 | 日志 |
  |---|---|---|---|---|
  | `ECP_None` | — | — | **恒 `true`** | **无**（正确声明"不消耗"） |
  | `ECP_CustomFragment` | — | — | 按 `ECP_None` 处理（恒 `true`） | **具名 `Warning`**（档名） |
  | `ECP_ResourceAttr` | **无效** | 任意 | **恒 `false`** | **`Warning`** |
  | `ECP_ResourceAttr` | 有效 | **`< 0`** | **恒 `false`** | **`Warning`** |
  | `ECP_ResourceAttr` | 有效 | `== 0` | **恒 `true`** | 无（"消耗 0 点"就是不消耗，非错误） |
  | `ECP_ResourceAttr` | 有效 | `> 0` | `GetBaseValue(Unit, ResourceAttr) >= Amount`，**且单位持有该属性** | 未持有时 `Warning` |

  - **★ 判据 = 本仓既有惯例，一律 fail-closed（MUST 读，我初稿在这里取反了）**：初稿写"配错 ⇒ 恒可付 + Warning"，理由是"不阻塞宿主"——**该理由是错的**，实测本仓对"配置写错"的处置**全部反着来**，措辞几乎逐字相同：
    - `TcsFlowStepConditions.h:56`——"**未知条件类型 → 视为不过 + Warning 日志**（**不静默通过**：静默会让'条件写错'表现成'步骤照跑'）"；
    - `TcsStepModifyFlow.cpp:36`——"载荷缺失或类型不符：不崩溃、**不静默通过**、不产生半笔提交"；
    - `TcsEffectAttributeAccess.h:72`——"条件求值要区分二者才能把'**键写错**'报出来（**MUST NOT 静默按 0.0 比较**）"。
    ⇒ 取"恒可付"**恰好制造本仓反复在拦的那类 bug**：技能明明配错，却照常放出来、**日志还清白**——"配置写错"表现成"一切正常"。**区分点 = "正确声明不消耗"（`ECP_None` / `Amount==0`，零日志）与"想配消耗但没配清"（无效 tag / 负值，拦住 + Warning）是两件事**，混成一类就会把后者伪装成前者。
  - **★ `Amount < 0` 为什么必须拦**：负的 `Amount` 经 `SetBaseValue(Unit, Attr, Base - Amount)` 会让技能**倒给**资源（`Base - (-50)` = `Base + 50`）——这是**可被当作漏洞利用**的形态，不是"配置不生效"。
  - **★ 单位未持有该属性 MUST 与"基值为 0"区分报**：`FTcsAttributeStore::GetBaseValue` 对未持有返回 `0.0`（`TcsAttributeStore.h:61`），与"确实持有且基值为 0"**不可区分** ⇒ `CanAffordCost` MUST 先经门面确认**持有**（`AddAttribute` 的既有语义面），否则"属性配错/没加"会表现成"付不起"——两种红字含义完全不同（`TcsEffectAttributeAccess.h:72` 是同一条判据的既有表述）。
  - **拒绝面形态**：本函数返回 `bool`，**MUST NOT ensure**（"付不起"是正常业务路径，不是契约违规）——与 `AttributeCompare` 条件的处置同款（"不通过 + 一条 Warning"）。
- [x] **Step 2：`CommitCost`**——按 Policy 提交：`ECP_ResourceAttr` ⇒ 走属性写侧扣减；`ECP_None` ⇒ 什么也不做；`ECP_CustomFragment` ⇒ 具名 `Warning` + 零提交。
  - **★ 写侧入口的判据（MUST 在此写清，MUST NOT 留到实施期）**：`ApplyModifier` 的形参 `FTcsAttrModInstance` 是**纯 C++ struct、非反射**（`SCRIPT-9` 在册）。本轮**插件内 C++ 调用完全可行**（`TcsSkill` 已依赖 `TcsAttribute`）⇒ `ECP_ResourceAttr` 档**今天就能真做**。~~受阻的只是**宿主脚本侧的 `CustomFragment` 档**（裁决 ④已推后）~~ —— **★ 2026-10-12 改正**：`CustomFragment` 档**并不受阻于这条**（它不走 `ApplyModifier`，且宿主自定义消耗根本不经过 TCS 属性写入面）⇒ 该档阻塞 = `COST-3`（扩展点形状未裁）。
  - **★ 扣减载体已裁定 = `SetBaseValue` 直写（读基值 → 减 → 写回），MUST NOT 用 `ApplyModifier` 挂修正器**（本计划自审时用硬约束关掉的一处"二选一"）。**判据三条，缺一都不成立**：
    1. **语义**：`SetBaseValue` 的注释原文即"**改写属性基础值（等级成长等宿主升级事务的落点）**"（`TcsAttributeSubsystem.h:171`），而"消耗资源"正是**永久改变基值**；挂修正器条目表达的是"**某个来源带来的增减**"，与"花掉了"不是一回事；
    2. **来源锚点的矛盾**：`ApplyModifier` 路径**必须**给 `Source`（`FTcsSourceHandle`），而资源扣减**不可**挂 `RunSource`（施法一终结 ⇒ 级联摘除 ⇒ 扣掉的血回来了）；挂"长寿命来源"则要**发明一个新发号器**，而该来源今天**零撤销消费者**（消耗本就不可撤销）⇒ 正是"零消费者不预建"；
    3. **`CHAIN-8` 已就同一问题给过判据**（台账原文）：修正器层**效力不等同**于直写基值——① 会被任一 `TAO_Override` **整体抹掉**（`TcsAttributeBandFold.h:161-164`）；② 参与 `PercentAdd` / `Mul` **复利**；③ 可按来源摘除。**"花掉 50 点蓝"若被一个 Override 抹掉就是荒谬的** ⇒ 必须直写基值。
  - **★ 一处实测事实（本计划自审抓到，MUST 记）**：`FTcsAttributeStore::GetBaseValue(const FGameplayTag&) const` **已经存在**（`TcsAttributeStore.h:61`，`FindInstance` 取 `BaseValue`、未持有返回 `0.0`），但**门面 `UTcsAttributeSubsystem` 没有转发它的公开口**（实测公开读口只有 `EvaluateCurrent`（折叠后当前值）与 `PeekPending`，**均读不到基值**）。
    ⇒ **本 Step MUST 先在 `TcsAttribute` 补一个门面转发口**（形如 `UFUNCTION() double GetBaseValue(FTcsCombatEntityHandle Unit, const FGameplayTag& Attribute)`），**转发现有实现、零新逻辑**。**这是本计划对 `TcsAttribute` 的唯一改动**，属"补一个已有能力的公开面"而非新机制。
    **为什么必须走门面、MUST NOT 从 `TcsSkill` 直接伸手 `ResolveStore` + `FindInstance`**：`ResolveStore` 虽在公开头，但其注释自述是"**门面与管线共用**的内部查询"（`:306-321`）；跨模块直读会**击穿 M2 门面**（属性存储的形状从此成为跨模块契约，M2 想改 `BaseValue` 的载体就得改两个模块）。**判据 = 门面是模块的对外唯一入口**（与 `TcsSkill` 自己的算子一律收 `UTcsSkillSubsystem&` 形参同一条纪律）。
  - **⇒ `CommitCost` 的 `ECP_ResourceAttr` 档调 `SetBaseValue(Unit, ResourceAttr, GetBaseValue(Unit, ResourceAttr) - Amount)`。**
  - **拒绝面**：`SetBaseValue` 自身对"单位未注册 / 属性名为空 / 该单位未持有此属性"返回 `false`（含 `ensureMsgf` 红字）⇒ `CommitCost` MUST 检查返回值并**向上报"提交失败"**，MUST NOT 假设它一定成功（否则 `PayCost` 会报成功而资源没扣）。
  - **另一处边界**：`GetBaseValue` 对**未持有该属性**返回 `0.0`（与"基值为 0"不可区分）⇒ `CanAffordCost`（Step 1）MUST 先确认该单位**持有**该属性，否则 `0 >= Amount` 的判定会把"属性不存在"表现成"付不起"（对于一个配错属性的技能，这两种红字含义完全不同）。
- [x] **Step 3：`PayCost(Unit, Entry)` 公开口**——解析条目 → 取其 Def 的 `Cost` → **重查一次 `CanAffordCost`**（裁决 ④的 TOCTOU 防护）→ 通过则 `CommitCost` + 发 `OnCostApplied`（Task 5）→ 返回是否成功。
  - **`PayCost` 的定位 MUST 写清**：它是 **`ECT_Manual` 档的专用提交入口**（并作宿主主动重试口）。**自动档（`OnCastStarted` / `OnCastCompleted` / `OnEvent`）MUST NOT 走它**——自动档直调 `CommitCost`，否则会多一层"按 Timing 判断要不要提交"的分支，而提交点应当**已经知道**自己该不该提交。
- [x] **Step 4：★ `ECT_OnCastStarted` 档的提交落点（2026-10-11 用户确认，MUST 按此实现）**——**门禁⑤（`CanAffordCost`）通过之后、建 run 之前**，调 `CommitCost`。
  - **落点精确位置**：`TcsCastOps.cpp` 的门禁⑤ 段（`:237`）**与**建运行态段（`:239` 起的 `CastRuns.Allocate()`）**之间**。
  - **为什么必须在"建 run 之前"（判据三条）**：① **语义**——`OnCastStarted` = "激活事务内提交"（`D5-10` 原文），而"激活成功"这件事的分界线就是**建 run**：建了 run 才存在"在飞的施法"，也才会有终结路径去回收它；② **失败面更干净**——若在建 run 之后提交且提交失败，就得**回滚一个刚建的 run**（池槽位、账本 `RunHandles` 回填、事件广播都要撤），而建 run 前失败只需**直接返回 `SAR_CannotAfford`**，零残留；③ **顺序与判据一致**——门禁⑤ 刚判"付得起"，紧接提交是**同一事实的两次使用**（判据 + 动作），中间隔得越远 TOCTOU 窗口越大。
  - **★ 一处 MUST NOT 的形状**：**MUST NOT 用属性侧的 `BeginBatch` / `Commit` 造一个"事务"把这个提交包起来**。判据：`D5-10` 的"激活事务"指的是**逻辑上的一次激活**，不是 M2 的属性事务批——包起来会让这次扣减与其它属性改动**共享一个提交点**，而那个提交点今天**不存在**（`TryActivate` 无事务边界，你已确认）。⇒ 直调 `CommitCost`（其内部 `SetBaseValue` 是"未开批 = 隐式批，立即生效"，`TcsAttributeSubsystem.h:171`），**MUST NOT 为此新引入事务语义**（那是 R7 或更后的题材）。
  - **★ 提交失败时的处置**：`CommitCost` 报告失败 ⇒ **返回 `SAR_CannotAfford` 且 MUST NOT 建 run**（与"门禁没过"同一返回值——对调用方而言两者是同件事："现在放不出来，因为付不起"）。日志 MUST 区分"门禁判不足"与"提交时失败"（后者是真实异常，前者是正常业务）。
- [x] **Step 5：门禁⑤ 真实现**（`TcsCastOps.cpp:237`）——占位注释替换为 `if (!CanAffordCost(...)) { log 具名; return SAR_CannotAfford; }`。**MUST 复用 Step 1 的同一函数**。
  - **裁决 ④的落点 MUST 在此注释写清**：门禁⑤ **恒在 `TryActivate`**（答"现在付得起吗"），**到期/事件/手动三个提交点各自重查一次** ⇒ **一条判据两个调用点、语义一致**。宿主从返回值即可看出为什么放不出来。
- [x] **Step 6：`ECP_CustomFragment` 档的具名降级**——MUST 记 `Warning`（含档名），**MUST NOT 静默当作 `ECP_None`**。~~降级注释 MUST 引用裁决 ④的阻塞前提（`SCRIPT-9` / `R5.5-h`）~~ —— **★ 2026-10-12 改正**：降级注释 MUST 引用**改正后的**阻塞前提（台账 **`COST-3`**，非 `SCRIPT-9`）；已按此改正 `TcsCostOps.cpp` 两处与 `TcsCostTypes.h` / `TcsSkillDefData.h`。
- [x] **Step 7：双配置编译** —— `Result: Succeeded`，0 error / 0 warning。
- [x] **Step 8：装置取证（待验清单）**——`CanAfford` 不足 ⇒ `SAR_CannotAfford`；足够 ⇒ 激活成功且资源正确扣减；`ECP_None` 恒过；`ECP_CustomFragment` 出 Warning 且按 None 走；**TOCTOU 场景**（门禁过 → 提交点前资源被花光 ⇒ 提交点拒绝，且**不产生半提交状态**）；**★ 四类配置边界逐格验**（Step 1 的表：无效 tag / 负 `Amount` / 零 `Amount` / 未持有属性——四者 MUST 各有一条读数，且**日志形态符合表格**）；**★ `OnCastStarted` 档的提交失败 ⇒ run 未建**（取证 MUST 验"没有半建的 run"：池占用数不变、账本 `RunHandles` 未回填）。

**非目标**：不做 `CustomFragment` 真实现；不做多资源（§7"v1 单资源"）；不做返还（`D5-4` 已砍除）。

---

### Task 3 — 终结路径的 Timing 触发点（接缝，冷热共用）

**目标**：把 `ETcsCooldownTiming` / `ETcsCostTiming` 的四档接到 R6 已落地的终结例程上。

**Files:**
- Modify: `Source/TcsSkill/Private/Skill/TcsCastOps.cpp`（**`FTcsCastOps::FinishCastRun`，`:444` 起**——挂触发点）
- Modify: `Source/TcsSkill/Public/Skill/TcsCooldownOps.h` / `TcsCostOps.h`（加"按 Timing 分派"的入口）
- Create: `Source/TcsSkill/Private/Skill/TcsCooldownEventRelay.cpp`（`ECT_OnEvent` 档的事件订阅）

**Interfaces:**
- Consumes: Task 1 的 `StartCooldownOnEntry`、Task 2 的 `CommitCost`、`ECastFinishReason`（`TcsCastFinishReason.h`）
- Produces: `DispatchCooldownTiming(Subsystem, RunCopy, Reason)` / `DispatchCostTiming(...)`——**唯一挂点**

> **★ 挂点已实测锁定（本计划自审抓到的初稿错误，MUST 读）**：初稿写"改 `TcsCastOps_Phase.cpp` 的四路终结处"，**那是错的**。实测：**四路终结全部汇聚到 `FTcsCastOps::FinishCastRun`**（**唯一例程**），四个调用点分别在 `TcsCastOps_Phase.cpp:279`（末段走完）/`:311`（瞬发当刻）/`TcsOps_Query.cpp:162`（打断）/`TcsCastOps.cpp:643`（顶替），而 `TcsCastOps.cpp:404` 那一处亦走它。**`FinishCastRun` 的签名自带 `ECastFinishReason Reason`**（`:447`）⇒ **一处分派即可覆盖四路，MUST NOT 去四个调用点各挂一份**（那正是本仓反复拦的"同一件事两个入口"）。
> **但 `TcsCastOps_Phase.cpp` 仍要改一处**（不是挂触发点）：见 Step 5 的瞬发接缝。

- [x] **Step 1：读全 `FinishCastRun` 的现状**（`TcsCastOps.cpp:444-670`，**MUST 逐段读完再动手**）——它的既有次序是：**⓪ 完成档起主链 → ① 取消时段到期锚点 → ② 按 `RunSource` 回收属性账本条目 → …**。本 Task 的触发点 MUST **插在正确的位置**（判定见 Step 2）。
  - **MUST NOT 凭 `SPEC-04-skill` 的描述推断形状**（那是语义面；形状以代码为准，`MEM-20261009-01`）。
  - **★ 一处必须继承的既有约束**：该例程有**重入窗口守卫**（`InFinishBroadcastHandles`，`:471-477`）与**"先按值拷出 `RunCopy` 再动池"**的纪律（`:479-485`）⇒ 本 Task 的全部新代码 **MUST 用 `RunCopy` 的拷贝字段**（`EntryHandle` / `Unit` / `RunSource`），**MUST NOT 再用 `Run` 指针**（池可扩容搬移）。
- [x] **Step 2：★ 触发点次序的裁定（本 Task 的核心判据，MUST 写清）**——冷却/Cost 的触发**MUST 排在"按 `RunSource` 回收"之后**。**判据**：回收会**广播属性变更事件**（`:464` 明文），而 `OnEvent` 档的 Cost 提交可能被那些广播触发；排在回收之前的话，回收尚未完成就可能提交消耗，且**新起的链/条目会与回收动作交错**。**同族先例** = ⓪ 段起链**必须**排在回收**之前**（`:489-492` 的反面理由：完成档起链排在回收后会"当场被自己的终结例程摘掉"）。⇒ **两处的次序理由相反、判据相同（"别让自己的动作被自己的清理吃掉"）**，MUST 在注释里把这个对照写出来。
  - **落地形状**：在 ② 段（回收）**之后**、且**在归还池槽位之前**插入 ③ 段。
  - **为什么必须在"归还槽位之前"**：广播纪律是"**先广播、后归还槽位**"（`FTcsCastEventPayload` 的类注释明文，`TcsCastEvents.h:49`）⇒ 订阅者拿到的冷却事件里若含 run 句柄，那一刻它 MUST 仍在册。
- [x] **Step 3：★ `ECT_OnCastInterrupted` 档的口径裁定**——"被打断时起冷"要回答：**顶替算不算？瞬发算不算？**
  - **硬约束**：`ECastFinishReason` 只有三档——`ECFR_Completed` / `ECFR_Interrupted` / `ECFR_Replaced`（**实测**，`TcsCastFinishReason.h`），而**瞬发没有自己的档**：瞬发走 `ECFR_Completed`（`:311`，ToolTip 原文"全时段走完，**或瞬发（空时段表）在激活当刻完成**"）。
  - **⇒ 分派表（MUST 写进注释，否则实施期必漏）**：
    | `Reason` | 是否触发 `ECT_OnCastInterrupted` |
    |---|---|
    | `ECFR_Interrupted` | **是**（真打断） |
    | `ECFR_Replaced` | **是**（`SPEC-04-skill` §11.5 裁决 1 明文"顶替 = 打断的一种终止形态，复用同一事件"） |
    | `ECFR_Completed` | **否** ⇒ 改判 `ECT_OnCastCompleted` 档（**含瞬发**——见 Step 5） |
- [x] **Step 4：`ECT_Manual` 档的"框架什么都不做"**——MUST NOT 在终结路径里为该档做任何事（含日志）。判据：Manual 的定义就是"宿主自己调"；框架做任何事都违背它。**MUST 写清"Manual 档下终结路径零动作"。**
- [x] **Step 5：★ 瞬发接缝（本 Task 最容易漏的一条）**——`ECT_OnCastCompleted` 档在**瞬发**（空 `Phases`）时必须也被触发。**实测的瞬发路径**：`TcsCastOps_Phase.cpp:311` 直接以 `ECFR_Completed` 调 `FinishCastRun` ⇒ **只要 Step 2 的触发点挂在 `FinishCastRun` 里按 `Reason` 分派，瞬发就自动被覆盖**（`Reason == ECFR_Completed`）。
  - **⇒ 本 Step 的实际动作 = 验证该覆盖成立，并在此写明"为什么不需要额外挂点"**。**若实施时发现瞬发走的是另一条不经 `FinishCastRun` 的路径，则本 Step MUST 补挂点**（届时如实改动本计划）。
  - **同族教训**：`SPEC-04-skill` §3.3 的实施注记已就"空 `Phases` 意味着没有任何时段到期 ⇒ 原句'全时段走完'对瞬发永不成立"留过同款教训；本轮 MUST NOT 复犯。
- [x] **Step 6：`ECT_OnEvent`（仅 Cost）的订阅落点**——`UTcsEventHandler` 派生类按 `FTcsCostConfig::TriggerEventTag` 订阅；事件到达 ⇒ 调 `CommitCost`（含 TOCTOU 重查）。
  - **订阅的寿命 MUST 处理**：条目被撤销（`RevokeSkill`）/ 定义被注销时 MUST 退订（否则回调里的条目指针悬空 = **use-after-free**）。**同族先例** = R6 `TcsCastOps.cpp:211-234` 的"任何可能广播的调用之后，手里的指针都可能失效"处置。**`TcsEventBusSubsystem::Unsubscribe(FTcsEventSubscriptionHandle)`（`:90`）是退订口。**
  - **`TriggerEventTag` 无效时的边界**：该档不成立 ⇒ 具名 `Warning` + 回退 `ECT_OnCastStarted`（MUST NOT 静默当默认档）。
- [x] **Step 7：双配置编译** —— `Result: Succeeded`，0 error / 0 warning。
- [x] **Step 8：装置取证（待验清单）**——三档 × {完成, 打断, 顶替, 瞬发} 的**真值表逐格验**（按 Step 3 的分派表）；Manual 档验"框架零动作"（终结后冷却是空的、资源没动）；`OnEvent` 档验事件到达才提交、**且撤销技能后订阅已退订**。

**非目标**：不做 `CQM_Custom` / `MCS_Custom`（批次 `g`）；不做打断来源优先级仲裁（`R5.5-e`）。

---

### Task 4 — 三枚冷却事件 + `OnCostApplied`（批次 `b` 的事件半）

**目标**：四枚事件落地并与可变性 API **成对**（`MEM-20260902-12` 的核心判据）。

**Files:**
- Create: `Source/TcsSkill/Public/Skill/TcsCooldownEvents.h` + `Private/Skill/TcsCooldownEvents.cpp`
- Create: `Source/TcsSkill/Public/Skill/TcsCostEvents.h` + `Private/Skill/TcsCostEvents.cpp`
- Modify: 四个调用点（Task 1 Step 5/8、Task 2 Step 3）

**Interfaces:**
- Consumes: `TcsEventBusSubsystem::PublishImmediate`（`:105`）、`FTcsCastEventPayload` 的形状先例
- Produces: 四枚 `extern TCSSKILL_API FNativeGameplayTag` + `FTcsCooldownEventPayload` / `FTcsCostEventPayload`

- [x] **Step 1：落 `FTcsCooldownEventPayload`**——`{EntryHandle, Unit, DefTag, TrackIndex, GroupTag, Total, Remaining, EndTimeSeconds}`（`D5-16` 的 payload 清单 + R6 `FTcsCastEventPayload` 的身份四件套）。
  - **`SkillId` 字段的处置（`D5-16` 原文有它，今天没有这个类型）**——MUST 用 `DefTag`（内容身份）取代，**MUST NOT 为它发明一个类型**。理由：`D5-9` 修订后 `EffectiveDefId` 已整体移除、"这个条目用哪份定义"的唯一答案就是 `DefTag`（`TcsSkillRegistry.h:42` 明文）。**这是 `MEM-20261006-16`「名字三类核实」的第三类（拟定未建）的正确处置 = 用现有的正确载体替代，不是补造。**
- [x] **Step 2：落 `FTcsCostEventPayload`**——`{EntryHandle, Unit, DefTag, ResourceAttr, Amount}`（`D5-16` 原文清单 + 身份三件套）。
- [x] **Step 3：落四枚 tag**——`TcsEvent.Cast.CooldownStarted` / `.CooldownUpdated` / `.CooldownEnded` / `.CostApplied`。全部 `extern TCSSKILL_API FNativeGameplayTag`（导出宏判据见 `TcsCastEvents.h:28`）。
- [x] **Step 4：接上四个调用点**——`StartCooldown` → `CooldownStarted`；`AdjustCooldown`/`ResetCooldown` → `CooldownUpdated`；到期回调 → `CooldownEnded`；`CommitCost` 成功后 → `CostApplied`。
  - **`CanAfford` 失败 MUST NOT 发 `CostApplied`**（`D5-16` 明文："CanAfford 失败不发——TryActivate 返回值覆盖"）。
  - **广播早于释放的纪律**（沿 R6 `FTcsCastEventPayload` 的注释）：订阅者在回调内可按句柄复核状态 ⇒ **先广播、后归还槽位**。
- [x] **Step 5：tag 治理复核**——新四枚落在既有 `TcsEvent` 根（**根数不增**，仍 12）。跑 `openspec/specs/gameplay-tag-governance` 的校验脚本，MUST 得 0 违规；**并复核根数声称的四处**（`INDEX` §5 新纪律：计数声称 MUST 全库同批回改）。
- [x] **Step 6：双配置编译** —— `Result: Succeeded`，0 error / 0 warning。
- [x] **Step 7：装置取证（待验清单）**——四枚事件各被订阅到且载荷字段正确；`AdjustCooldown` 后 `CooldownUpdated` 的 `Remaining` 与新值一致；**事件与可变性成对**（没有 API 的状态 MUST NOT 有事件、反之亦然——`MEM-20260902-12` 的三连自问）。

**非目标**：不做 UI/径向（§7）；不做网络复制（§6）。

---

### Task 5 — 宿主装置与端到端取证（LAC 仓）

**目标**：一条把「冷却多轨 + 组轨 GCD + CDR 快照 + Cost 门禁/提交/事件」串在**同一次施法**里的装置，两条路线都跑（无头 + 编辑器 PIE）。

**Files:**
- Create: `Source/TcsDev/Private/Dev/TcsDevSliceRig_CooldownCost.cpp`（屏显段 **`0x7C5D9000`**）
- Create: `Source/TcsDev/Public/Dev/TcsDevSliceRig_CooldownCost.h`
- Modify: `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（**+2 行**）
- Create: `Content/TcsDev/E2E/` 下的真内容资产（带冷却与 Cost 的技能 Def）

**Interfaces:**
- Consumes: Task 1~4 的全部公开口与事件
- Produces: `Tcs.Test.CooldownCost.Run` / `.Reject` 命令

- [x] **Step 1：装置骨架**——照 R6 Task 6 的 `TcsDevSliceRig_Skill.cpp` 先例（`TcsDevSliceRig.cpp:?` 的 `RegisterCommands()` 注册模式）。
- [x] **Step 2：★ 跨帧等待 MUST 挂被测物自己的时轴**——判据已在 R6 确立（`PLN-R6:601-621`）：**MUST NOT 用 `FTimerManager` 墙钟**、**MUST NOT 把判据执行体放进 `FTimerManager` 回调**（后者在 R6 实测产生 **use-after-free**：`Passed=-1514964509`）。复用 LAC 的宿主私有骨架 `TcsDevCombatClockWait.h`（R6 Task 6 交付）。
- [x] **Step 3：判据清单**（`Run`）——至少覆盖：① 空 Tracks 恒可放；② 自身轨起冷 ⇒ 门禁③ 拒；③ **组轨跨条目共享**（A 起冷 ⇒ 同组 B 条目也拒 = 真 GCD）；④ CDR 快照不追溯；⑤ 到期后恢复可放；⑥ Cost 不足 ⇒ `SAR_CannotAfford`；⑦ Cost 足够 ⇒ 扣减正确；⑧ 四枚事件各收到一次；⑨ Manual 档框架零动作；⑩ `OnEvent` 档事件到达才提交。
- [x] **Step 4：`.Reject` 判据**——非法输入必须被拒且**不产生半提交状态**：无效 `EntryHandle` / 未学技能 / `ResourceAttr` 无效 / `Amount` 负值 / 重复 `PayCost`。
- [x] **Step 5：两条路线都跑**——无头 `UnrealEditor-Cmd -game` + **编辑器 PIE**。判据：R6 实证"**无头跑过 ≠ 验证过**"（跨帧回调/捕获/持有期延长的改动，**编辑器路线不可省**）。
  - **★ PIE 面 2026-10-12 补取完成，且预判成立——PIE 抓出两处夹具缺陷**（详见证据档 §4.4）：
    - `P1`（**判据级**）跨帧谓词问的是**全局** `CooldownEndedCount`，而 CDR 那条 **0.5s** 轨与
      被测的 **1.0s** 自身轨同时刻起冷 ⇒ **CDR 先到期就把全局计数满足** ⇒ 等待提前落定
      ⇒ `I2` 误报 FAIL。**无头因首帧时钟大跳使两轨同帧到期**而恰好重合 ⇒ **假绿灯**。
      修法 = 谓词按**条目身份**筛。**复验硬证据**：`I1` 的 `ElapsedSeconds` **0.550s → 1.333s**（两轮）。
    - `P2`（**前提级**）检查 `e` 拿 `Attribute.Armor` 当"未持有"，而 `ApplySliceAttributes`
      **会授予 Armor** ⇒ "未持有"这个前提**只在无头下成立**（无头内容资产 0 条 ⇒ 属性表为空）。
      修法 = 另造**不装配**的裸单位取得真正的未持有态，并把该口改为**两向都验**。
  - **两处都是夹具缺陷，插件侧门禁/提交逻辑在两处都表现正确。**
  - 读数：`Run` 立即面 **18/0** + 尾段 **21/0**、`Reject` **7/0**，**两次独立 PIE 世界逐字相同**；
    跨 PIE 残留检查通过；`D1`/`F1`/`B1` 在 PIE 下全 `PASS` ⇒ ****该三条在 PIE 下亦 `PASS` ⇒ 无头对它们的 `PASS` 不是假绿灯**（⚠ **但两条路线用的夹具版本不同，见 `EVID-2026-10-11-r65-cooldown-cost` §1.1 ⇒ MUST NOT 读成"双路线一致通过"**）**。
  - **夹具热补丁手法**（可复用）：Ctrl+Alt+F11 是 `LiveCodingConsole` 用 `RegisterHotKey` 注册的
    **全局热键**，`PressKey` 够不到 ⇒ 用 `SendInput` 合成真实按键；**且给 `UCLASS` 加成员变量
    无法热补丁** ⇒ 夹具新增状态放**文件作用域**。
  - **⚠ 一处取证卫生缺陷（2026-10-12 自查所得，MUST 读）**：上述 PIE 读数是**在 Live Coding
    热补丁生效期间**取得的，而**编辑器基线 `TcsDev.dll` 比源码旧约 7 小时**
    ⇒ **该读数重启编辑器后不可复现**（会退回 `22/1 + 6/1`）。**这正是 `MEM-20260922-05`
    「取证期一律不用 Live Coding」警告的形态，本轮违反了它。**
    **受影响面按模块核定 = 仅宿主夹具 `TcsDev`**；`TcsSkill.dll` / `TcsAttribute.dll` 均**晚于各自最新源码**
    ⇒ **插件侧（真正的交付物）证据不受影响**。
    **⇒ ✅ 同日已闭合**：删陈旧 patch → 双配置全量 UBT（写回基线）→ 核对**各模块** DLL 晚于其源码
    → 重开编辑器（无 patch 加载）→ **两条路线都重跑** ⇒ 无头 **18/0 + 21/0 + 7/0**、
    PIE **18/0 + 21/0 + 7/0**，**逐字一致且同为新夹具**。全过程见证据档 §1.1。
    **两条可复用判据**：① **`UnrealEditor-Cmd` 与 PIE 共用同一个基础 DLL**（`Shipping.exe` 只用于
    编译验证）⇒ "重建了 Shipping"**不代表**无头路线的装置换了新代码，MUST 按**模块**核对 mtime；
    ② **写进证据档的读数，其来源 MUST 是"基线 DLL 且其时间戳晚于相关源码"**。
- [x] **Step 6：两轮复现性**——同配置连跑两轮，关键读数**逐字相同**（R6 的既有纪律）。
- [x] **Step 7：回归**——R6 的五个命令逐项不变：`SkillLayer.Run` 24/0、`.Reject` 7/0、`SkillCast.Run` 26/0、`.Reject` 16/0、`.Phase` 29/0 + `PhaseDeferred` 34/0、`SkillParamChain.Run` 26/0、`.Reject` 9/0。**任一变化即回归破坏，必须查明。**
- [x] **Step 8：屏显段分配复核**——`0x7C5D9000` 是否空闲（`0x7C5D8000` 已被 R6 Task 6 占满）。
- [x] **Step 9（同日追加，用户裁定）：删除 `ECT_OnEvent` 消耗档**——判据 = 框架只能认识"施法这个
  事务自身的边界"，"某个特定事件触发消耗"是**项目词汇** ⇒ 归宿主（`ECT_Manual` + `PayCost`）。
  删除 7 项伴随面（枚举值 / `TriggerEventTag` / 中继类 / 门面读口 / 三处撤销退订 / 装置 `H1`·`H2` /
  两条检查用 tag）；规格落点 = 提案 **`remove-cost-onevent-timing`**（3 REMOVED + 3 ADDED）。
  **分母变化**：`Run` 立即面 **20/0 → 18/0**、尾段 **23/0 → 21/0**（去掉 `H1`/`H2`）；
  `Reject` 恒 **7/0**。**MUST NOT 读成回归**——删的是**一个已不存在档位**的判据。
- [x] **Step 10（同日追加，用户裁定）：技能数值字段参数化 —— `COST-1` / `COOLDOWN-1` 同批闭合**——
  `FTcsCooldownTrack::Duration` 与 `FTcsCostConfig::Amount` 换 `FTcsParamValue`；
  新增契约键 `TcsStateParam.CostAmount`（**带式折叠**：`PercentAdd` = 百分比减免、
  `FlatAdd` = 固定值减免）；唯一求值实现 `FTcsCostOps::ResolveEffectiveCost`。
  **★ 形状与原记不同（MUST 记）**：台账 `COST-1` 原记"加系数键 `CostPct`（MUST 沿此走）"，
  **用户改点 B 案**——判据 = **纯系数表达不了固定值减免**。⇒ 已在台账该行记明这次改点。
  **规格落点** = 提案 **`parametrize-skill-numeric-fields`**（2 MODIFIED + 2 ADDED）。
  **分母变化**：新增装置判据 `J0`~`J5` ⇒ `Run` 立即面 **18/0 → 24/0**、尾段 **21/0 → 27/0**；
  `Reject` 恒 **7/0**；R6 回归 **24/0** 且摘要逐字同基线。
  **两类读数（无头 + PIE）逐字一致，且同一基线 DLL、无热补丁参与。**
  **★ 实施期抓到一处真实设计缺陷**（装置拒绝面检查 `d` 当场 FAIL）：初版"钳 0"把
  **负基数**（配置错误/漏洞）与**减免过度**（合法的免费）压成同一个值 ⇒ 前者守卫永不触发；
  修法 = `ResolveEffectiveCost` 增"未钳基数"出参。**编译与阅读都过、只有行为级取证抓得到。**
- [x] **Step 11（同日追加）：拓宽非目标 —— 多资源（成本组）入册**（用户裁定：本轮只做单资源）。
  形状 = `FTcsCostGroup{TArray<FTcsCostConfig>, ERequireMode}`（`All`/`Any`），
  触发条件 = "出现需要同时消耗两类以上资源的真实内容需求"（DNF 式"蓝+血+道具"）。
  **MUST 一并重审**两件事：① 成本提交的**原子性**（现行代码**明文禁止**用 `BeginBatch` 造事务）；
  ② `ECP_CustomFragment` 的**扩展点形状未裁**（台账 **`COST-3`**——**★ 2026-10-12 改正：原记 `SCRIPT-9` 不成立**；多资源里"道具"那一条依赖 `COST-3`）。
  **台账落点 = `COST-2`**（新增，触发条件型）。

**非目标**：不列入 `Saved/` 的产物（`Saved/` 是 gitignored，`.gitignore:68`——放那里的门禁不持久）。证据档落 `Documents/combat-system-design/evidence/`。

---

### Task 6 — 收束（提案归档 + 文档五面回写 + 台账对账）

**目标**：本轮收束，走 R6 Task 7 的同一套动作。

**Files:**（按 Step 展开）

- [x] **Step 1：提案归档**——本轮各 Task 的提案（预期 `add-cooldown-and-cost-types` / `add-cooldown-multitrack` / `add-skill-cost` / 取证提案）逐个 `openspec archive <id> --yes`。
  - **★ 归档前必做无损预检**（`TOOLS-9` 的同族教训）：备份 `specs/` 并逐条 diff 每个 MODIFIED delta 的**全部既有 requirement + scenario**。**归档器整体替换 requirement**，漏写的 scenario 会**静默丢失**。
  - **★ 新建能力的 `## Purpose` 是 `TBD` 占位**（`TOOLS-9` 已登记，本轮第 3~4 次命中）⇒ 归档后 MUST **手写每个新能力的 Purpose**，否则 `validate --all --strict` 变 `failed`。
- [x] **Step 2：`SPEC-04-skill` 回写**——§3.5 / §3.6 的落轮标注去掉"归 R6.5"；§11.2 的六行不落面**改判**（`a`~`d` 已落 ⇒ 移出该表；`e`/`f`/`g` 保留）；§11.3 的实现名对照补上本轮落地名（`ETcsCostPolicy` / `ETcsCooldownTiming` / `ETcsCostTiming` / `FTcsCooldownPolicy` / `FTcsCostConfig`）；新增 §12 或续 §11 记本轮实施范围。
  - **冻结档纪律**（`docs-convention` §7）：`SPEC-04-skill` 状态是 `ACTIVE`（非冻结）⇒ 可就地改；但**已加的实施注记 MUST 保留**（原文 + 注记并存）。
  - **★ `MUST NOT` 改动正文的语义**——本轮对 `D5-10` 的 `OnCastInterrupted` 档订正 MUST 用**就地实施注记**（保留原文），MUST NOT 直接删原文。
- [x] **Step 3：`SPEC-TRACE` 补行**——`tcs-contract-traceability.md` 补本轮 R6.5 行。
- [x] **Step 4：`PLN-R5` 路线图 + `PLN-R6` 批次表回写**——见 §0.4 两处订正：① `PLN-R6`《R6.5 批次表》关账块加实施注记（`g` 的依赖订正 + 本计划已开）；② `PLN-R5` 路线图加 `R6.5-1` 行。**`PLN-R6` 是 `ACTIVE`，可就地改**；但按 `INDEX` §5 的计数声称纪律，全库同批回改。
- [x] **Step 5：台账对账**——复核 `STAT-10` / `STAT-11` / `WAIT-14` 三条的归属是否受影响（预期**不受影响**，均属批次 `e`/触发条件）；本轮若新增待办，按《入册判据》第 3 条判是否入册（预期**零新增**）。
- [x] **Step 6：两册日志**——`log/decisions-log.md` 新增本轮条目（含 §0.2 五条裁决）；`log/log-03-skill-m5.md`（**FROZEN，只追加**）新增 `R6.5-1` 节。
- [x] **Step 7：`INDEX` 回写**——§4.3（新增 `PLN-R6.5-1` 行，含 §0.4 其二的"第一个拥有 Task 的非正式轮号"登记）、§4.4 路线图（`R6.5` 行拆分）、§4.5 台账计数（若变）、§4.6 EVID 计数（若新增）、§5（本轮若有新纪律）。
- [x] **Step 8：检查点卡**（经 `harness-retro` 门控）——R6.5-1 断点卡 **`MEM-20261012-01`** 已立
  （`status: task-open`；同日经用户门控入库，含**六类缺陷分列**与三条设计开口）。
  **本批（Step 10~11）后又更新一次**：补 `OnEvent` 删除裁定、双路线闭合读数、参数化批次结论。
- [ ] **Step 9：提交**——**须用户明确授权**（`~/.agents/AGENTS.md`「SourceControl 提交规则」最高优先级）。提交前展示摘要与影响范围。
  - **★ 两仓提交范围纪律（R6 实证，MUST 复用）**：TCS 经路径反查确认 100% 属本轮 ⇒ 可 `-A`；**LAC 是混合仓 MUST NOT `-A`** ⇒ 按路径逐个 `add`，跳过所有无关改动。**post-`add` MUST 跑路径正则安全检查**（R6 曾拦下一处上一次会话遗留的 TGFS 改名泄漏）。

**非目标**：不做批次 `e` / `g`（另轮；**`f` 已删除**，见 `GLOSSARY` §2.1）。

---

## 4. Review Focus（spec 隐含但没有任何 Task 的测试覆盖、最可能咬人的五类）

按 `superpowers:writing-plans` 要求：这是**规格是愿景文档、其对某输入沉默不等于该输入可以坏掉**的那一类。每条都已在拥有该代码的 Task 里加了对应步骤。

1. **到期堆的悬空句柄 `ensure`（Task 1 Step 6）**——`CancelExpiry` 对已释放句柄留红字。合理的人会期望"取消一个已到期的冷却"是安全的空操作，而实际会打红字。**已在 Task 1 Step 6 钉死"回调入口先复位"。**
2. **改了冷却时长但堆没重挂（Task 1 Step 8）**——合理的人会期望 `AdjustCooldown` 立刻生效；不重挂则堆里旧时刻继续生效，症状是"改了却不生效"或"提前到期"。**已在 Task 1 Step 8 钉死"先 Cancel 再 Push"。**
3. **TOCTOU：门禁通过后资源被花光（Task 2 Step 3）**——合理的人会期望"付不起就不放"；门禁通过到提交点之间资源可能已被花掉。**已在 Task 3 Step 4 与 Task 2 Step 3 钉死"提交点重查"，并在 Task 5 Step 3 的 ⑩ 项与 `.Reject` 的"不产生半提交状态"里加了测试。**
4. **瞬发技能漏接 `OnCastCompleted` 档（Task 3 Step 5）**——合理的人会期望"瞬发 = 成功完成 ⇒ 冷却按完成档起"；但瞬发**没有时段推进**，照"时段走完"接就永远不触发。**已在 Task 3 Step 5 单独钉死。**
5. **`OnEvent` 档的订阅寿命（Task 3 Step 4）**——合理的人会期望撤销技能后不再有回调；不退订则回调里条目指针悬空 = use-after-free。**已在 Task 3 Step 4 钉死"撤销/注销时 MUST 退订"。**

---

## 5. 变更记录

- **2026-10-11 建立**（R6.5-1 开工收窄轮）：批次 `a`~`d` 落成 **Task 0~6**（类型层 / 冷却多轨 / Cost 策略 × 时机 / 终结接缝 / 事件词汇 / 装置取证 / 收束）。**五条口径当场裁定**（§0.2）——其中三处**改写了上游文档的原表述**（`ITcsCooldownTimingFragment` 删除 / `OnEvent` 采纳 / `ECostTiming` 删 `OnCastInterrupted` 档 + 两个新公开口）。**一处上游订正**（§0.4 其一：`PLN-R6` 关账块的"随时可开"对批次 `g` 不成立）。**现状证据表 17 行逐条实测**（§0.3），其中**两条既有陷阱被判据化**（到期堆悬空 `ensure` / 池槽复用不清零）。

- **2026-10-11 自审（`superpowers:writing-plans` 的 Self-Review）抓到并已修的三处真问题**——三处都属"写计划时留下的欠债标记"，正是 `MEM-20261006-15` 点名禁止的形态。**MUST 记录，因为它们证明自审不是形式**：
  1. **★ 挂点写错了地方**：初稿写"改 `TcsCastOps_Phase.cpp` 的四路终结处"，**实测四路全部汇聚到 `FTcsCastOps::FinishCastRun`**（唯一例程，签名自带 `ECastFinishReason Reason`）⇒ 收敛为**一处分派**（Task 3 已改，并把"为何不去四个调用点各挂一份"写成判据）。**根因** = 我按 `SPEC-04-skill` 的**语义面**描述（"四路终结"）推断了**形状**，而形状以代码为准（`MEM-20261009-01`）。
  2. **★ 一处"推荐 = A 或 B"**：Task 2 的扣减载体初稿写"推荐用独立的消耗来源，**或**走 `SetBaseValue` 直写"——**这是二选一，是欠债标记**。已用三条硬判据关掉为 `SetBaseValue` 直写（语义 / 来源锚点矛盾 / `CHAIN-8` 已立"修正器层效力不等同"）。
  3. **★ 一处"若 X 则先补 Y"**：随上一处暴露——`EvaluateCurrent` 读的是**折叠后当前值**、`SetBaseValue` 写的是**基值**，"读现值再减"会在有修正器时**多减**。实测 `FTcsAttributeStore::GetBaseValue` **已存在**（`:61`）但**门面未转发** ⇒ 关掉为"**补一个门面转发口**"（零新逻辑），并把"为何 MUST 走门面、MUST NOT 直伸 `ResolveStore`"写成判据。**该改动已登记进 §1 文件表与 Task 2 的 Files**。

- **2026-10-11 用户复核轮的两处订正（MUST 记，两处都改变了实现）**：
  1. **★ `CanAffordCost` 的配置错误面取反（我初稿判据错误）**。初稿写"`ResourceAttr` 无效 或 `Amount <= 0` ⇒ 恒可付 + Warning"，理由是"不阻塞宿主"。**该理由是错的**：实测本仓对"配置写错"的处置**一律 fail-closed**，且措辞几乎逐字相同（`TcsFlowStepConditions.h:56` / `TcsStepModifyFlow.cpp:36` / `TcsEffectAttributeAccess.h:72`："**不静默通过**：静默会让'条件写错'表现成'步骤照跑'"）。取恒可付**恰好制造本仓反复在拦的那类 bug**——技能配错却照常放、日志还清白。**已改为四类分开处置**（Task 2 Step 1 的表）：`ECP_None` 与 `Amount==0` = 正确声明不消耗（零日志）；**`ResourceAttr` 无效 / `Amount<0` = 恒不可付 + Warning**。**另加一条**：`GetBaseValue` 对未持有返回 `0.0`，与"基值为 0"不可区分 ⇒ MUST 先确认持有，否则"属性没加"会表现成"付不起"。
     **为什么 `Amount<0` 尤其必须拦**：`SetBaseValue(Base - Amount)` 在负 `Amount` 下是 `Base + 50` ⇒ 技能**倒给**资源，属可被利用的形态，不是"配置不生效"。
  2. **★ `ECT_OnCastStarted` 档的提交落点已由用户确认 = "门禁后、建 run 前"**。原文只把它写在 Task 3 的取证清单里（不够显式），**已升格为 Task 2 的独立 Step 4**，并把三条判据写清（语义 = 建 run 才是"激活成功"的分界线 / 失败面 = 建 run 后失败要回滚一个刚建的 run / 顺序 = 紧接判据 TOCTOU 窗口最小）。**同批写明一处 MUST NOT**：**MUST NOT 用属性侧 `BeginBatch`/`Commit` 造"事务"包裹**（`D5-10` 的"激活事务"指逻辑上的一次激活，非 M2 属性事务批；包起来会让本次扣减与其它属性改动共享一个**今天不存在**的提交点）。

- **本轮刻意不做的事（`MEM-20261006-15` 的正面应用）**：批次表里的三处"待定"——`FCooldownTrack{Duration, GroupTag?}` 的问号、`FCostConfig{..., Timing}` 的枚举名未定、`ITcsCooldownTimingFragment` 的载体未定——**全部在本计划期用硬约束关掉**，MUST NOT 留到实施期。判据：提案期改的是一份未获批的 delta 稿（改稿成本），实施期改的是**已获批的规格面**（改规格 + 返工成本）。

- **★ 一处本计划自身的载体选择（MUST 登记）**：`R4.5` 与 `R5.5` 两个非正式轮号**至今只有批次表、从未有过 Task 骨架**，其八批全停在"❌ 未开工"。**根因不是它们不需要，而是"批次表长得像计划"**——批次表答"**还剩什么**"（台账形状），计划答"**怎么做、怎么验**"（执行形状），两者混同产出的东西**能列出来、无法执行**。**本计划是该约定下第一个拥有真实 Task 的非正式轮号**，已在 §0.4 其二登记，并将在 Task 6 Step 7 写入 `INDEX` §4.3。〔**2026-10-13 追注**：同 §0.4 其二——"无 Task 骨架"这半仍成立，**"`R4.5` 八批全未开工"这半已过期**（其三批已于 2026-09-29 / 10-05 / 10-06 收口，`PLN-R4` 转 `FROZEN`）；`R5.5` 不变。**"载体选择"这条判据本身不受影响**，故本计划的自陈仍然有效。〕
