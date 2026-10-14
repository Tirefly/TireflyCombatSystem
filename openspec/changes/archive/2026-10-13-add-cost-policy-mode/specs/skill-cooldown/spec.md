## MODIFIED Requirements

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

### Requirement: 冷却策略的数据形状（轨道与时机）

`TcsSkill` MUST 提供冷却策略的**纯数据**形状（零行为——同 `FTcsBoolSwitchRow` 的处置）：

- `UENUM(BlueprintType) ETcsSkillCooldownTiming`（**四档**）：
  `ECT_OnCastStarted`（**默认**：激活成功后立即起）/ `ECT_OnCastCompleted`（施法走完全部时段、
  **或瞬发当刻完成**时起）/ `ECT_OnCastInterrupted`（被打断**或被顶替**时起）/ `ECT_Manual`
  （**框架不自动起**，由宿主在自己的逻辑处调 `StartCooldown`）；
- `USTRUCT(BlueprintType) FTcsCooldownTrack{ FTcsParamValue Duration; FGameplayTag GroupTag; }`；
- `USTRUCT(BlueprintType) FTcsCooldownPolicy{ TArray<FTcsCooldownTrack> Tracks; ETcsSkillCooldownTiming Timing; }`。

**★ 类型名于 2026-10-12 由 `ETcsCooldownTiming` 改为 `ETcsSkillCooldownTiming`**（`PLN-R6.5-CC` 裁定 R12）：
与消耗侧的 `ETcsSkillCostTiming` **同批加域前缀**，以保持这一对姊妹枚举的对称。
**只改类型名——四档的枚举值与语义一律不动**；归档提案与历史日志里的旧字面保留作现场记录，
**MUST NOT 改写**（本仓既有"换根注记"纪律同款）。

**各字段的语义与判据（MUST 守死）**：

- **`GroupTag` 无效 = 自身轨、有效 = 单位级组槽**：这就是**真公共冷却（GCD）的纯数据表达**——
  "自己 8 秒不能再放"与"所有技能共享 1 秒公共冷却"形状完全相同，区别只在 `GroupTag`。
  ⇒ 不需要为两者各立一套类型（`D5-15`："Strategy 三档**溶解进轨道**"）。批次表 `:198` 的
  `GroupTag?` 问号**就此关闭**：字段**恒存在**，**MUST NOT** 另加一个"有没有组"的开关
  （无效 tag 已是"没有组"的天然表达，再加开关就是同一件事两个入口）；
- **空 `Tracks` = 永远可放**：这是 `D5-15` "默认无冷却"的数据表达，也是 Strategy 三档溶解之后的
  唯一"无"档 ⇒ **MUST NOT** 再引入一个 `None` 枚举值；
- **`Timing` 是策略级、不是每轨一个**（`D5-15` 明文）：时机回答"这次施法**什么时候**进冷却"，
  而"进冷却"对全部轨道是**同一个瞬间**；每轨各配一个时机只会造出"自身轨已起、组轨还没起"
  这种没有语义的中间态；
- **`Duration` MUST 是 `FTcsParamValue`（参数源载体）**（2026-10-12 用户裁定：
  `D5-15` 原话 `Duration(字面量|Param)` 的 `Param` 形态**就此落地**，台账 `COOLDOWN-1` 闭合）：
  - **判据**：裸 `double` 让"按时长等级表配冷却"与"按来源配时长"**都无法表达**，
    而两者都是真实内容需求；载体化之后**字面量源仍是默认形态**（`FTcsParamValue` 默认装 `Literal`）
    ⇒ 既有作者的写法零变化、资产零迁移；
  - **求值时点 = 起冷那一刻，且全场只求一次**：MUST 在**循环外**装配求值上下文、
    **逐轨各求一次会让"同一次冷却内部不自洽"**（与 CDR 快照同一条判据）；
  - **求值上下文 MUST 填 `EffectiveLevel`**：这是"按时长等级表配冷却"能成立的**唯一原因**
    ——`FTcsParamSource_StateLevelArray` / `_StateLevelMap` 读的是**基类**
    `FTcsParamEvaluateContext::EffectiveLevel`，而**不是**状态域派生字段；
  - **求值结果 MUST 钳下界到 0**：负时长没有语义（"立刻到期"应由 `Duration = 0` 或 `CDR = 1`
    表达）——与 `EffectiveDuration` 的既有钳制同款；
- **MUST NOT 声明 `CustomFragment` 字段、MUST NOT 建 `ITcsCooldownTimingFragment` 接口**
  （2026-10-11 用户裁定）：自定义时机 = "**不自动执行，让宿主在自己的确定逻辑处手动调用**"
  ⇒ 那正是 `ECT_Manual` 档。**判据**：时机是"何时做"的问题，而"何时"只有两种表达——
  **能被事件名说出来**，或**只能由代码在具体位置说出来**；**中间没有第三种形态** ⇒ 两种载体都不需要。
  载体存在的意义是"框架要按它替你决定做什么"，而手动档恰恰是"框架什么都不替你决定"。

#### Scenario: 四档各有时机语义且默认档最保守

- **WHEN** 新建一个 `FTcsCooldownPolicy` 且不显式设置 `Timing`
- **THEN** `Timing == ECT_OnCastStarted`、`Tracks` 为空数组；空 `Tracks` 下该技能**恒可放**

#### Scenario: 组轨与自身轨同形、靠 GroupTag 区分

- **WHEN** 检查 `FTcsCooldownTrack` 的字段列表
- **THEN** 它**恰好**含 `Duration` 与 `GroupTag` 两项，**不存在**"是不是组轨"的布尔字段或独立的组轨类型；`GroupTag` 无效即自身轨

#### Scenario: 自定义时机无任何载体

- **WHEN** 检查 `Source/TcsSkill/` 的类型清单与 `FTcsCooldownPolicy` 的字段列表
- **THEN** **不存在** `ITcsCooldownTimingFragment` 类型，也不存在名为 `CustomFragment` 的字段；`ECT_Manual` 档**零载体**（宿主直接调门面入口）

#### Scenario: 两个 Timing 枚举各自独立

- **WHEN** 比较 `ETcsSkillCooldownTiming` 与 `ETcsSkillCostTiming` 的档位集
- **THEN** 两者**不是同一个枚举**、也**不互相"覆盖"**：冷却侧**无** `None` 档与 `OnEvent` 档
  （冷却起因永远在本模块手里）、消耗侧**无** `OnCastInterrupted` 档
  （判据见 `skill-cost` 的同名需求）

#### Scenario: 时长是参数源载体且默认字面量零

- **WHEN** 新建一个 `FTcsCooldownTrack` 且不显式设置 `Duration`
- **THEN** `Duration` 是 `FTcsParamValue`，其内层源为 `Literal` 且值为 `0`
  （**与换型前的 `double Duration = 0.0` 语义逐字相同** ⇒ 既有资产零迁移）；
  该轨道起冷时长为 0 ⇒ 由 `EffectiveDuration` 的既有钳制表现为"起了但立刻清"

#### Scenario: 时长可按时长等级表取值

- **WHEN** 把 `Duration` 的源设为 `FTcsParamSource_StateLevelArray`（`Values = {8, 6}`）、
  并让该单位以等级 2 施放
- **THEN** 该轨生效时长基于 `6`（**求值上下文 MUST 已填 `EffectiveLevel = 2`**）；
  若实现未填该字段，等级表源会落各自 `Fallback` 而不是报错——**本 scenario 即钉住那个静默形态**

### Requirement: 冷却的终结侧触发点与时机分派

冷却的 `ETcsSkillCooldownTiming` MUST 接到**四路终结的唯一汇聚点**上：

- **挂点 = `FTcsCastOps::FinishCastRun`**（末段走完 / 瞬发当刻 / 打断 / 顶替四路**全部汇聚**到它，
  且其签名自带 `ECastFinishReason Reason`）⇒ **一处分派即可覆盖四路**。
  **MUST NOT** 去四个调用点各挂一份（那是"同一件事两个入口"，必然漂移）；
- **分派表**（`ECastFinishReason` 只有三档，且**瞬发没有自己的档**——瞬发走 `ECFR_Completed`）：

  | `Reason` | 起哪一档 |
  |---|---|
  | `ECFR_Interrupted` | `ECT_OnCastInterrupted`（真打断） |
  | `ECFR_Replaced` | `ECT_OnCastInterrupted`（**顶替 = 打断的一种终止形态**） |
  | `ECFR_Completed` | `ECT_OnCastCompleted`（**含瞬发**） |

- **`ECT_Manual` 在终结路径 MUST 零动作**（含日志）：它的定义就是"宿主自己调 `StartCooldown`"，
  框架做任何事都违背它；
- **触发点次序 MUST 排在"按 `RunSource` 回收"之后、归还池槽位之前**：
  - **之后**：回收会**广播属性变更事件**（M2 批提交重算 + 广播），排在回收之前会让
    回收尚未完成就可能触发新动作、且新条目与回收交错；**同族对照** = ⓪ 段（完成档起链）
    必须排在回收**之前**（否则新起的链挂的条目**当场被自己的终结例程摘掉**）
    ⇒ **两处次序相反、判据相同："别让自己的动作被自己的清理吃掉"**；
  - **之前（归还槽位）**：广播纪律是"先广播、后归还槽位"⇒ 订阅者拿到的载荷里若含 run 句柄，
    那一刻它 MUST 仍在册；
- **定义 MUST 在回收之后重新解析**：回收会广播，订阅者可在其中**注销技能定义**，
  而登记表是 `TUniquePtr` 持有、**注销即析构** ⇒ 沿用更早的 `Def` 指针是 use-after-free。

#### Scenario: 瞬发技能也按完成档起冷

- **WHEN** 一个**瞬发**（空 `Phases`）技能配 `ECT_OnCastCompleted` + 一条冷却轨，被激活
- **THEN** 激活当刻完成后该技能**进冷却**（`IsOnCooldown` 为真）
- **判据**：瞬发**没有时段推进**，若照"时段走完"接就永远不会触发——而它走的是
  `ECFR_Completed` ⇒ 挂在 `FinishCastRun` 上的分派**天然覆盖**它，**不需要额外挂点**

#### Scenario: 打断与顶替都走打断档

- **WHEN** 一个配 `ECT_OnCastInterrupted` 的技能分别被 `CancelCast` 打断、被同类技能顶替
- **THEN** **两种情形都起冷**（顶替是打断的一种终止形态）；而配 `ECT_OnCastCompleted` 的技能
  在被顶替时**不起冷**

#### Scenario: Manual 档终结路径零动作

- **WHEN** 一个配 `ECT_Manual` 的技能完成一次施法
- **THEN** 终结后该技能**没有冷却**（`IsOnCooldown` 为假）——框架 MUST NOT 替它起冷

