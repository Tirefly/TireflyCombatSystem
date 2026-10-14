# skill-cooldown Specification

## Purpose
定义技能冷却的**数据形状与运行态**：策略族 `ETcsSkillCooldownTiming` / `FTcsCooldownTrack` / `FTcsCooldownPolicy`（形状），`FTcsCooldownOps` 的多轨计时线（自身轨 + **实体级共享组轨 = 真公共冷却**）、CDR 契约键与起冷时刻快照、到期驱动与改轨/清轨、门禁③「冷却中」的**唯一判据实现**，以及终结路径上的时机分派与三枚事件——让「技能放不出来」的第二层理由（冷却）成为**可表达、可查询、可驱动**的运行事实，而不是一个恒过的占位分支。

## Requirements

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

- **`GroupTag` 无效 = 自身轨、有效 = 实体级组槽**：这就是**真公共冷却（GCD）的纯数据表达**——
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
  并让该实体以等级 2 施放
- **THEN** 该轨生效时长基于 `6`（**求值上下文 MUST 已填 `EffectiveLevel = 2`**）；
  若实现未填该字段，等级表源会落各自 `Fallback` 而不是报错——**本 scenario 即钉住那个静默形态**

### Requirement: `CooldownPct` 冷却缩减契约键的声明归属

`TcsSkill` MUST 提供原生 GameplayTag `TcsStateParam.CooldownPct`（常量名 `Tag_TcsSkillParam_CooldownPct`，
`Public/Param/TcsSkillParamKeys.h`，**带模块导出宏 `TCSSKILL_API`**），作为**冷却缩减（CDR）的框架契约键**：
挂在该键上的修正**不进普通参数折叠**，而是作为系数作用于该次冷却**各轨的 `Duration`**。

**为什么声明方是 `TcsSkill` 而不是 `TcsState`**（判据三条，与 `TcsStateParam.Level` 住 `TcsState` 是
**同一判据的相反结论**——该对照 MUST 被记录，防后人误以为两处不一致）：

1. **语义归属**：CDR 的公式（"作用于各轨 `Duration`"）写在 `D5-15`、由**技能冷却域**解释；
   状态侧**无此概念**——状态实例没有"冷却"，它的时长语义是"持续多久"（另一种东西）；
2. **依赖方向（决定性，与 `Level` 恰好相反）**：`TcsSkill` 的 `Build.cs` 含 `TcsState`（**已依赖**），
   而 `TcsState` **不依赖** `TcsSkill` ⇒ 声明在 `TcsSkill` **零成环风险**。`Level` 反过来
   （M5 没有自己的等级定义层，只是复用状态侧定的等级语义）故住 `TcsState`。
   **同一个词被多域消费是常规情形**（M2/M5/状态三域共用同一套五带即先例），
   声明方 MUST 取**语义所有者 + 依赖上游**的那一侧；
3. **声明方 = "谁解释那个词"**：宿主只是"往本键上挂修正器"（`ModifierRows` 声明式 /
   `ApplyParamModifiers` 命令式都在宿主手上），它**不解释**这个键。

**为什么 tag 文本仍在 `TcsStateParam` 根下**（与常量名住 `TcsSkill` 不矛盾）：本键是**技能参数表的键**，
而技能 Def 的数值参数表**继承**自 `FTcsStateDefBase::Params` ⇒ 键的词根由**参数表本身**决定
（宿主在同一根下声明自己的内容参数），而**声明方**由上述判据 ①②③ 决定。
两者是**两个独立问题**——`gameplay-tag-governance` 的「同一根共享」正是为"契约词 + 内容词并存"准备的。

**导出宏（跨模块实证）**：`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（无 `__declspec(dllexport)`）
⇒ 其它模块引用本变量会 `LNK2001`。本键的设计意图正是"供宿主与装置引用" ⇒ MUST 带模块导出宏。

#### Scenario: 契约键由插件原生声明且不需宿主配置

- **WHEN** 检查 `TcsStateParam.CooldownPct` 的声明方与宿主 `Config/DefaultGameplayTags.ini`
- **THEN** 它由**插件模块**原生声明（`TCSSKILL_API` 导出宏）、**不需要**宿主 ini 配置即生效；声明处住 `TcsSkill`（拥有冷却语义，且 `TcsState` 不依赖它 ⇒ 零成环）

#### Scenario: 契约键不进普通参数折叠

- **WHEN** 某条目的参数账本上挂一条 `ParamKey = TcsStateParam.CooldownPct` 的修正
- **THEN** 该修正**不改变**普通参数的折叠结果（它是冷却触发时刻读取的**系数**，不是参数表的值）；`TcsStateParam` 根下的宿主内容参数键仍由宿主 ini 声明

### Requirement: 冷却多轨运行态（起冷 / 判可放 / 改轨 / 到期驱动）

`TcsSkill` MUST 提供冷却的**运行态**（`FTcsCooldownOps`），使 `FTcsCooldownPolicy` 的 `Tracks`
数组成为真的计时线。**判据的唯一实现住本族函数**：门禁③（激活序列）与三个提交点共用同一份，
**MUST NOT 各写一份**。

- **起轨** `StartCooldownOnEntry(Subsystem, Entity, EntryHandle)`：
  - 逐轨取 `Duration` → 乘 CDR → 算 `EndTimeSeconds = Clock.GetClock().Elapsed + EffectiveDuration`
    → `PushExpiry`；返回**实际起的轨道条数**；
  - **CDR 快照于此刻**（`D5-15`："快照于冷却触发时刻，中途变化不追溯"）——
    开头取**一次**并作用于**全部**轨道（取多次会让同一批轨道拿到不同系数）；
  - **定位 MUST 靠 lambda 捕获**（`[WeakSubsystem, Entity, EntryHandle, TrackIndex]`），
    `OwnerId` **只作回调归属读数**——**MUST NOT** 把定位职责压给 `OwnerId`
    （那会与既有堆使用约定造出第二真相：同一个堆、两种回调约定）；
  - **组轨写实体级共享槽**（`GroupTag` 有效时）；**同组已有活槽 ⇒ 复用、不重起、不叠加**
    （重起会让"放完 A 立刻放 B"把 GCD 刷满，而 GCD 的意义恰恰是不许这样做）；
  - **零时长轨道视为"不起"**（不入一个立刻到期的条目——它只会在下一帧空跑一次）；
  - 条目悬空 / 定义取不到 / 时钟不可得 ⇒ 返回 0 并如实报出（**不 ensure**——时序或设施缺席）。
- **判可放** `IsEntryOnCooldown(...)`（`D5-15` 原文："**可施放 = 全部自身轨道清空 + 所属组槽清空**"）：
  - 写成**一条短路遍历**：任一自身轨未清 **或** 任一所属组槽未清 ⇒ `true`；
  - **判据 MUST 是"`EndTimeSeconds > Now`"（时轴上的事实），MUST NOT 是"`ExpiryEntry` 有效"**：
    只看句柄的话，一旦某条到期条目被别的路径撤掉而状态没清，该轨会**永久堵死**
    （症状 = 这个技能再也放不出来，且没有任何日志）；
  - **边界（MUST 处理，不得崩）**：`Tracks` 为空 ⇒ **恒可放**；`GroupTag` 无效 ⇒ 只算自身轨；
    条目状态数组比定义短（定义被热改）⇒ 缺的轨道按"未在跑"算，**MUST NOT 越界读**；
    组槽存在但已过期 ⇒ 不再阻拦；
  - **时钟不可得 ⇒ 判为不阻拦**（与"未注入实体查询 ⇒ 降级放行"同口径：设施缺席 MUST NOT 把一切拦死）。
- **CDR** `EvaluateCooldownPct(...)`：取 `TcsStateParam.CooldownPct` 契约键上的折叠结果；
  - **钳制上下界写死为 `[0, 1]`**（**MUST NOT** 写"合理处理"）：负值 ⇒ `0`
    （**负的 CDR 不得变成冷却延长**——那是"配了个负减免反而被罚"的静默怪相）；
    `> 1` ⇒ `1`（**不得产生负时长**）；非有限值（NaN/Inf）⇒ 记 `Warning` 并按 `0`（MUST NOT 静默）；
  - **生效式**：`EffectiveDuration = Duration × (1 - CDR)` ⇒ `0.3` = 时长打七折、`1` = 冷却为 0；
  - 该键**未命中 ⇒ CDR = 0**（无缩减），零日志（多数技能不会有 CDR 修正）。
- **改轨** `AdjustTrack(...)`：`Delta` 作用于**剩余**（正 = 延长、负 = 缩短）；
  **缩短到 `<= 0` ⇒ 等于清轨**（复用清轨路径，而非入一个立刻到期的条目）；
  `Total` **只增不减**（维持 `Remaining <= Total` 不变量——否则 UI 径向会画出超过 100% 的弧）；
  - **★ 改完 MUST 重挂到期堆**（`CancelExpiry` 旧的 + `PushExpiry` 新的）：
    **MUST NOT** 只改 `EndTimeSeconds` 而不重挂——堆里的旧到期时刻会继续生效，
    症状是"改了却不生效"或"提前到期"（两个方向都可能，取决于调整方向）。
- **清轨** `ClearTrack(...)`：撤锚点 + 状态**整体复位**（`= FTcsCooldownTrackState()`，非逐字段）。
- **到期回调** `ExpireTrack(...)` / `ExpireGroup(...)`：
  - **★ 第一条语句即把该轨状态整体复位**——这一步同时兑现两条纪律，**MUST NOT 拆开或挪到后面**：
    ① **防 `ensure` 红字**：`CancelExpiry` 对**悬空**句柄是 `ensureMsgf`，而"到期回调正在跑"那一刻
    它自己的条目**仍然有效**（`FireDueItem` 在回调**返回之后**才释放）⇒ 回调内若再撤本句柄就是
    "取消一个正在跑的条目"；复位后 `IsValid()` 为假，后续清理路径天然跳过
    （**同款先例** = `FTcsCastRun::PhaseExpiryEntry` 的既有注释、`FTcsCastOps::AdvancePhase` 的首条语句）；
    ② **池槽复用纪律**："整体重置而非逐字段赋值"——将来给该结构加字段时自动是干净的；
  - 组轨到期 = **整槽移除**（不需要逐条目遍历——这正是"组槽住实体"的收益）；
  - **无需额外的"允许再次激活"状态**：门禁③ 每次现算，不存在缓存态要清。
- **生命周期配对**（本仓"外部资源成对"纪律的落点）：
  - `ReleaseEntryCooldowns(...)` / `ReleaseEntityCooldowns(...)` MUST 存在——
    冷却是本仓**第一个在账本条目上挂外部资源**的机制（到期堆条目 + 实体级组槽），
    而账本条目此前的契约是"零外部资源 ⇒ 直接删桶即可"；该前提**已失效**；
  - 撤销路径 MUST 在**释放槽位之前**调用配对清理（槽位一放，条目内容整体重置 ⇒ 锚点找不回来）；
  - **组槽刻意不随条目撤销而摘**：组槽是**实体级**的、有自己的到期条目，会自行解除；
    替它解除会**替同组的其它技能**解除公共冷却（症状 = 撤销 A 之后同组的 B 立刻能放，而 GCD 本该还在）。
    ⇒ 组槽的生命周期归 `ExpireGroup` 与 `ReleaseEntityCooldowns`，**MUST NOT 在条目级越界清理**。

#### Scenario: 空 Tracks 恒可放（阴性对照）

- **WHEN** 某技能的定义**不含任何冷却轨道**，在其激活后查询 `IsOnCooldown`
- **THEN** 返回 `false`，且该技能能再次成功激活（**MUST NOT** 被门禁③ 拒）
- **判据的作用**：先证明"没有轨道的技能放得出来"，后面所有"冷却中 ⇒ 拒"才不是假绿灯
  （否则一个"恒拒"的实现会让全部正面判据都通过）

#### Scenario: 自身轨起冷后门禁③ 拒

- **WHEN** 一个配了单条自身轨（`GroupTag` 无效、时长 1.0s）的瞬发技能被激活成功，随即**再次**激活
- **THEN** 第二次返回 `SAR_OnCooldown`；`GetCooldownRemaining(...) > 0` 且 `<= GetCooldownTotal(...)`，
  且 `Total` 等于配置时长；`GetCooldownTrackCount(...)` 等于定义的轨道数

#### Scenario: 组轨跨条目共享（真 GCD）

- **WHEN** 两个*不同*的已学技能配了**同一个** `GroupTag`，其中 A 起冷
- **THEN** **B 也被拒**（`SAR_OnCooldown`），且两者查询到的剩余时长相**等**
- **判据**：组槽若被错写成"每条目各一份"，同组两个条目会各起各的线 ⇒ B 能放出来，本条即失败

#### Scenario: CDR 快照于触发时刻

- **WHEN** 一条 1.0s 的轨道配了 `CooldownPct = 0.5`，技能被激活
- **THEN** `GetCooldownTotal(...)` 为 **0.5**（`1.0 × (1 − 0.5)`）；起冷之后修改该键**不改变**本次总时长

#### Scenario: 到期后恢复可放

- **WHEN** 一条 1.0s 自身轨的冷却在**战斗时钟**上自然到期
- **THEN** 该技能重新可激活成功（**MUST NOT** 留下"永不恢复"的状态）

#### Scenario: 改轨重挂到期堆

- **WHEN** 对一条在跑的轨道调用 `AdjustCooldown(...)` 延长剩余
- **THEN** 新的剩余**立即**生效（读数随之改变），且**不出现"提前到期"**——判据 = 堆里的旧时刻已被撤销

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

### Requirement: 冷却事件（三枚）与可变性 API 成对

`TcsSkill` MUST 提供三枚冷却事件，且**与可变性 API 成对**（`MEM-20260902-12` 的核心判据：
有该状态改变就 MUST 有对应事件，反之亦然）：

| Tag | 产生者 |
|---|---|
| `TcsEvent.Cast.CooldownStarted` | 起冷成功（自动档与 `ECT_Manual` 档**同一条路径**） |
| `TcsEvent.Cast.CooldownUpdated` | `AdjustCooldown` / `ResetCooldown` |
| `TcsEvent.Cast.CooldownEnded` | 到期堆回调（**权威侧**——到期是时轴上的事实，不是"某个调用者说结束了"） |

- 三枚 MUST **共用一种载荷形状** `FTcsCooldownEventPayload`
  （`{EntryHandle, Entity, DefTag, TrackIndex, GroupTag, Total, Remaining, EndTimeSeconds}`）——
  订阅者拿到的永远是"哪个实体的哪条技能的哪条轨道"，各写一个 struct 会让订阅侧复制三份解析代码
  （先例 = `FTcsCastEventPayload` 的四枚共用）；
- **`D5-16` 的 `SkillId` 字段 MUST 用 `DefTag` 取代**（**MUST NOT 为它发明一个类型**）：
  `D5-9` 修订后 `EffectiveDefId` 已整体移除，"这个条目用哪份定义"的唯一答案就是 `DefTag`；
- **命名 MUST 去掉 `On` 前缀**（`D5-16` 原文写 `OnCooldownStarted` 等）：
  `TcsEvent.Cast` 根下既有的四枚是 `Started` / `PhaseChanged` / `Completed` / `Interrupted`
  （**零 `On` 前缀**）⇒ 按**同族对称**取名。**MUST NOT 照抄 `On*` 进 tag 名**
  （那会让同一根下两种命名风格并存）；
- **根数 MUST NOT 增加**：三枚落既有 `TcsEvent` 根（域段 `Cast`）——"增根不增层"；
- **导出宏**：三枚皆 MUST 为 `extern TCSSKILL_API FNativeGameplayTag`
  （`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`** ⇒ 跨模块引用会 `LNK2001`）；
- **一次起冷发一枚 `CooldownStarted`**（**MUST NOT** 逐轨发 N 枚）：订阅者的语义是
  "**这个技能**进冷却了"，不是"第 N 条轨进冷却了"；逐轨发 N 枚会让最简单的单轨用例收到 N 次
  而信息量完全相同。载荷的轨道读数取**第一条实际起的轨**，逐轨读数经查询口按 `TrackIndex` 取；
- **起冷事件 MUST 排在全部轨道写完槽之后**：订阅者在回调里查 `IsOnCooldown` 时应当能看到
  **完整**的冷却状态，而不是"第一条轨已起、后两条还没起"的中间态；
- **组轨到期事件 MUST 按"配了本组轨道的每个条目"各发一枚**：组槽是**实体级**的、
  **不记"谁起的它"**，而订阅者需要**自己的条目身份**才能刷新对应 UI 行
  （只发一枚无身份的组事件会让订阅者无从知道该更新哪一行）——这是"组槽住实体"的已知代价；
- **广播走立即通道**：冷却状态是"查询面可验证的事实"，发事件若走帧末，
  订阅者会在"事件还没到、但查询已经说在冷却"的窗口里看到不自洽的读数；
- **时钟不可得 / 世界无总线 ⇒ 静默跳过**（不 ensure、不红字——时序或设施缺席，非契约违规）。

#### Scenario: 一次起冷恰发一枚 `CooldownStarted` 且载荷身份吻合

- **WHEN** 一个配了**多条**轨道的技能起冷，订阅者计数
- **THEN** `CooldownStarted` 恰 **1** 次；载荷的 `EntryHandle` 与 `Entity` 与该条目一致

#### Scenario: 调轨与清轨各发 `CooldownUpdated`

- **WHEN** 依次调用 `AdjustCooldown(...)` 与 `ResetCooldown(...)`
- **THEN** 两次都发 `CooldownUpdated`，且调轨那次的 `Remaining` 与新值一致

#### Scenario: 到期发 `CooldownEnded` 且 `Remaining` 为 0

- **WHEN** 一条自身轨在战斗时钟上到期
- **THEN** 发出 `CooldownEnded`，其 `Remaining` 为 **0**（"剩余时间到点了"就是它的定义）

#### Scenario: 事件与可变性成对（无遗漏）

- **WHEN** 检查冷却侧的全部状态改变路径（起冷 / 调轨 / 清轨 / 到期）
- **THEN** 每条路径都有对应事件；且**不存在**"有事件但无对应状态改变"或反过来的情形
