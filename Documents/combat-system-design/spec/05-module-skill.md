# [SPEC-04-skill](../spec/05-module-skill.md) — TcsSkill 技能/施法层设计（v2 定稿重写）

- **文档 ID**：`SPEC-04-skill`
- **类型**：SPEC / 模块规格
- **状态**：ACTIVE（**R6 核心已落地，2026-10-11 收束**——Task 0~7 全部完成并取证、两个提案已归档；落地范围与切分见 §11，**本轮不落**的条款逐条带归属于 §11.2，"不落"不等于"未定"）
- **权威范围**：TcsSkill（M5）账本/施法/冷却/Cost/参数链；理由住 LOG-03-skill 与 DEC-02-fold-display
- **最后更新**：2026-10-12（**`SCRIPT-9` 误归因改正 + `COST-3` 入册回写**：§3.6 与 §11 的 `CustomFragment` 阻塞登记由 `SCRIPT-9` 改指 **`COST-3`**（真因 = 扩展点形状未裁，非属性反射面缺失），§11.2 补一行扩展点批次行。**前次（同日）**：**R6.5-1 落地回写**：§3.5 / §3.6 各加落地注记（含四处落地名订正与三处新增公开口）；§11.1 门禁可拒性时态到期；§11.2 前四行加"处置"列；§11.3 补冷却/Cost/事件三行实现名；§9 加 v3 修订行。**前次** 2026-10-11（R6 收束：新增 §11 实施范围与切分——落地面 / 不落面带归属 / 实现名对照 / 四条错前提更正回写 / 三条口径裁决 / 两处正文就地订正 / 边界；身份块状态 PENDING → ACTIVE））

- 日期：2026-09-02
- 状态：**v2 定稿**——全部增补（D5-9~12、D3-10/11 Skill 侧等）已折入正文；修订记录见文末
- 职责一句话：**技能/施法的编排层——已学技能账本、施法运行、冷却策略、参数修正账本。技能是原语链的编排者，不是执行者（执行在 M4）。**

## 1. 模块边界

- 消费者：M6（输入/适配接线）、M8（编辑器）、宿主项目（GrantSkill/TryActivate API）。
- 依赖：TcsCore（句柄/总线/到期堆/时钟）、TcsNotation（ValueConvention/描述文本绑定，D5-18）、TcsAttribute（Cost ResourceAttr 扣减/属性读取——**AttrCapture 已于 2026-10-08 整体删除**，见 §2；伤害流程侧的步骤级捕获属 TcsDamage，与技能侧无关）、TcsState（FSkillDef 继承 FStateDefBase；关系字段 Block/Require/Cancels 执行）、TcsEffect（CastChain 起链/ChainRunHandle/打断协作）。**不依赖 TcsDamage/TcsTargeting**——链步骤是 FInstancedStruct 数据，运行时经执行器注册表分派（D4-14），本模块代码不具名任何领域步骤类型；"技能×伤害"的组合在 TcsIntegration/宿主完成。**FSkillDef 在本模块定义**（继承 M3 的 FStateDefBase）。**★ 2026-10-08 依赖面收窄**：AttrCapture 删除后，本模块对属性门面的**运行期**引用为零（`TcsAttribute` 依赖仅剩 **Cost ResourceAttr** 一路，而 Cost 归 R6.5）⇒ **MUST NOT** 为本模块新建属性访问白名单薄壳。
- **不依赖输入系统**（宿主/M6 职责）。

## 2. Def 类层级与 SkillDef（D3-10/D5-10）

```
FStateDefBase（抽象，M3 定义：词表/FragmentSet/通用默认参数 LevelBase/MaxLevel）
 └─ FSkillDef : FStateDefBase             ← 本模块定义（施法语义）
```

> **实施注记（2026-10-11，R6 收窄轮；正文不改、就地注明）**：本图三项已过期——实际数据基类 = **`FTcsStateDefBase`**（不是 `FStateDefBase`），本模块数据 = **`FTcsSkillDefData`**、资产 = **`UTcsSkillDef`**；基类**没有 `FragmentSet`**（行为 Fragment 是 `FTcsBuffDef.Fragments` 的**派生字段**）。`LevelBase` / `MaxLevel` 名称与落地一致，但**语义已收窄**（等级→数值表归参数源）。⇒ 完整对照见 **§11.3 / §11.4①**。

**FSkillDef 字段**（全部为施法语义；**无堆叠、无时值**——D3-10）：
- **参数双表 + Mode 列 + ValueConvention**（D5-5/D5-12 v2/D5-18）：`NumericParameters{Key, Base: FTcsParamValue, Mode: Snapshot(默认)|Live, ValueConvention}` 与 `BoolSwitches{Key, Base, Mode}`（**实现名 `FTcsNumericParamRow` / `FTcsBoolSwitchRow`，2026-09-14 命名批；**数值参数行住 `TcsState`、由 `FSkillDef` 继承白拿；布尔开关行住 `TcsSkill`（2026-10-06 R6 Task 1 Q-8 改判——`BoolSwitches` 的消费者只有技能侧，`SPEC-02-states` 全文零命中；`ETcsParamMode` 仍从 `TcsState` 复用）**）——Base 载体 PV 系列 2026-09-11 换型 TInstancedStruct<FTcsParamValueSource>——StateLevel/InstigatorLevel/AttributeScaled 等源落点（**TargetLevel 系暂不提供**——PV-4 评判轮）——Live 实时走 Entry 账本求值（仅 Skill 侧有意义）；ValueConvention（TcsNotation 层，D5-18）：Percent/OneMinus/Negate EnumFlags，**写入点转规范值**（账本内永远规范值 0.85/0.75/-1000），UI 反变换显示（记法层规格详见 [SPEC-07-notation](../spec/11-module-notation.md)）；
- **~~AttrCapture 声明列表（D5-12 v2）~~：`CastConfig{AttrKey, From: Instigator/Target}`——activate 时捕获进 `FCastRun.Context.CapturedAttrs`（属性读默认 Live、捕获命中读快照）；伤害流程侧另有步骤级 AttrCaptureList（09 文档，作用域=流程）——两层机制各自独立。**★ 2026-10-08 技能侧整体删除（用户裁定，原文保留以记现场）**：三条判据同时成立 ⇒ **零消费者**（技能侧 `CapturedAttrs` 全仓无任何类型读取）/ **机制重叠**（设计给捕获写的用途"改 `CapturedAttrs`、随流程消失、零账本污染"已被**流程属性黑板** `FTcsFlowAttributes`（作用域 = 流程用完即弃）+ `FlowModify` 数据步骤覆盖 ⇒ 同一个"流程局部可变值空间"存在两份）/ **技能侧已被参数快照占满**（技能自己的参数由 `FCastRun.ParamSnapshot` 在激活瞬间冻结，流程内工作值由黑板承担；剩下空间**说不出技能侧独有的业务场景**，设计语料也未给出该用例）。**删除面** = `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型 + `FSkillDef.AttrCaptureList` 字段 + `FCastRun.CapturedAttrs`。**边界（MUST 记录）**：**只涉技能侧**——伤害流程侧的 `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，触发条件**不因此改变**，本删除 MUST NOT 被读成"关闭了 `WAIT-7`"。落点 = 提案 `add-cast-run-and-gates`；**MUST NOT 补回**。
- **描述文本绑定（D5-17 v3——视图策略化，2026-09-15）**：`SkillDef.Descriptions: TArray<FTcsDescriptionEntry{DescriptionId, TextKey, Views[]}>`（**取代单字段 DescriptionTextKey**——多描述入口：Tip 简述 / Codex 图鉴 / 升级预览各自独立，支撑"简单描述=当前档单值、复杂描述=全表+高亮"多对多）；**视图 = 结构化策略配置**：`Views: TArray<FTcsDescriptionViewSlot{SlotName, TInstancedStruct<FTcsParamView>}>`——内置 `Value`（单值）/`Series`（整表+当前档高亮）/`Range`（区间）/`Attribute`（属性现值），宿主 C++ 自定义视图一步接入（零公共代码）；**StringTable 原文只含槽名 `{Rate}`（零语法）**——机器语义全部进结构化配置，编辑器可校验（`IsCompatible` 探针 + 槽名交叉扫描，见 11/08）；组装器按 Entry 自身 Def 的账本**现值**取值（EffectiveDefId 已整体移除——D5-9/D5-14 重构 2026-09-11），RichTextBlock 渲染归宿主；技能面板索引口径 = 当前 `EffectiveLevel`（实例侧 = 快照 Level，见 03 §3.6）；
- **施法时段表**（可选）：`TArray<FPhaseSpan{Duration(FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型), bInterruptible, bCanMove, Tag}>`——任意段数；"前摇/后摇"= 项目命名惯例；瞬发 = 空表；
- **冷却（D5-15 多轨道）**：`FCooldownPolicy{Tracks: TArray<FCooldownTrack{Duration(FTcsParamValue——同上换型), GroupTag?}>, CustomFragment}` × `FCooldownTiming{OnCastStarted(默认)/OnCastCompleted/OnCastInterrupted/Custom(ICooldownTimingFragment)}`——无 GroupTag=自身轨道，有=写单位级组槽；可施放=全部自身轨道+所属组槽清空；真 GCD=自身轨道+GCD 组轨道并存（纯数据）；

> **★ 2026-10-12 R6.5-1 参数化批次落地注记（上段原文逐字保留，MUST NOT 抹除）**：
> **两处数值字段换型落地**（台账 `COST-1` / `COOLDOWN-1` **同批闭合**，用户裁定）：
> - **`FTcsCooldownTrack::Duration`** → `FTcsParamValue`（求值在**起冷那一刻、全场一次**；
>   上下文 MUST 填 `EffectiveLevel` —— 这是"按时长等级表配冷却"能成立的唯一原因）；
> - **`FTcsCostConfig::Amount`** → `FTcsParamValue` + **新契约键 `TcsStateParam.CostAmount`**
>   （走**五带折叠**，初值 = `Amount` 求值结果）：`TAO_PercentAdd` = **百分比减免**、
>   `TAO_FlatAdd` = **固定值减免**；唯一求值实现 = `FTcsCostOps::ResolveEffectiveCost`。
>
> **★ 一处必须如实记的"文档先行、代码滞后"**：上段 §3.5 的 `FTcsCooldownTrack{Duration(FTcsParamValue
> ——同上换型)}` **在 2026-09-11 的 PV 系列换型时就已写成 `FTcsParamValue`**，
> 而**代码停在 `double Duration = 0.0`**（`PLN-R6.5-1` 按"只落字面量"实施并如实登记为 `COOLDOWN-1`）。
> ⇒ **本轮不是"给设计加能力"，而是"把代码补到设计早就写下的形态"**。
> **这条同时也是"计数/形状声称 MUST 全库回改"纪律的一个变体**：类型形状的声称也会
> **文档与代码分叉**，而 `validate` 不校验类型与文档是否一致 —— 只有**实现批次的逐处核对**能抓到。
>
> **与 Cost 侧的刻意不对称（MUST 记）**：CDR 是**纯系数**（`Duration × (1 − CDR)`、只钳 `[0,1]`、
> 不做折叠）；`CostAmount` 走**带式折叠** —— 因为"减固定值"用纯系数表达不了。
> **判据 = "减免能不能用一条系数说清"**。
>
> **实施期抓到一处真实设计缺陷**（装置拒绝面检查 `d` 当场 FAIL）：初版用"折叠后钳 `[0,+∞)`"作
> **唯一**收口 ⇒ 把**负基数**（配置错误/漏洞形态）与**减免过度**（合法的"免费"）**压成同一个值**
> ⇒ 前者被静默抹成 0、其守卫永不触发。**修法** = `ResolveEffectiveCost` 增"未钳基数"出参。
> **该缺陷编译与代码阅读都过，只有行为级取证才抓得到。**
>
> **落地名**：`FTcsParamValue`（两字段）/ `Tag_TcsSkillParam_CostAmount` /
> `FTcsCostOps::ResolveEffectiveCost` / `FTcsParamChainOps::FoldParamKeyWithBase`。
> **规格面落点** = 提案 `parametrize-skill-numeric-fields`；**装置读数** = `J0`~`J5`；
> **两路线读数** = 无头与 PIE 均 `Run 24/0 + 27/0`、`Reject 7/0`、R6 回归 24/0（摘要逐字同基线）。
- **Cost（D5-10 v2）**：`FCostConfig{Policy: None(默认)/ResourceAttr(内建 M2 扣减)/CustomFragment(ICostPolicy), Timing{OnCastStarted(默认)/OnCastCompleted/OnEvent/OnCastInterrupted/Custom(ICostTimingFragment)}, ResourceAttr + CostParam}`——检查（CanAfford）与提交（Pay）成对走策略；**道具/弹药/意灵晶 = 宿主 CustomFragment（插件不提供实现）**；打断返还未设引擎字段（Timing 选择或 OnCastInterrupted 响应表达）；v1 单资源；

> **★ 2026-10-12 R6.5-1 参数化批次落地注记（上段原文逐字保留；本段的 Cost 部分与 §3.5 的注记同批）**：
> - **`FTcsCostConfig::Amount`** → `FTcsParamValue`（原为裸 `double`）；
> - **新增契约键 `TcsStateParam.CostAmount`**（消耗修正键）：本键上的修正经**五带折叠**作用于
>   `Amount` 的求值结果（**初值 = 字段求值，不是 0**）——**百分比减免**用 `TAO_PercentAdd`、
>   **固定值减免**用 `TAO_FlatAdd`；
> - **唯一求值实现** = `FTcsCostOps::ResolveEffectiveCost`（门禁⑤ 与每个提交点各调一次，
>   **MUST NOT 各算一份**——"门禁判的数"与"真扣的数"一致正是 TOCTOU 防护的实质）；
> - **`CostApplied` 载荷的 `Amount` = 实际扣减量**（求值 ⊕ 减免之后），**MUST NOT 是配置基数**；
> - **负基数 vs 减免过度 MUST 分开**（实施期实测抓到的设计缺陷，详见 §3.5 注记）；
> - **`ECP_CustomFragment` 维持"具名未实现 + 回退 `None`"**（**阻塞登记已于 2026-10-12 改正：原记 `SCRIPT-9` 不成立 ⇒ 真阻塞 = 扩展点形状未裁，已入册 `COST-3`；详见 §3.6 同批注记**）；
> - **`v1 单资源` 维持**：多资源（DNF 式"蓝+血+道具"）**未做**，已入台账 **`COST-2`**
>   （触发条件型；届时形状 = 成本组 `FTcsCostGroup{TArray<FTcsCostConfig>, ERequireMode}`，
>   **且 MUST 一并重审**"成本提交的原子性"与 \(SPEC-04 明文的"MUST NOT 用 `BeginBatch` 造事务"\)）。
- **关系字段**（同 BuffDef 形状）：语义映射 Block=禁止激活（沉默/缴械）、Require=激活前提、Priority=顶替优先级、Cancels=激活时取消指定运行/状态（姿态切换）；
- **主链**：`CastChainId` + `MainChainStart{OnCastStarted(默认)|OnPhaseEnter(Tag)|OnCastCompleted|Custom}`（旋风斩类前摇技能配 OnCastCompleted）；
- `bSingleInstance` → **`ECastInstancing{InstancePerExecution / InstancePerEntity}`** + **`bRetriggerOnActive`**（D5-11，GAS 对齐；Static 不做）。**语义（2026-10-07 R6 裁定，原语料只给枚举名未给语义，此处补齐）**：`InstancePerExecution` = **并存**（同一施法者连点两次可各自推时段，互不干涉）；`InstancePerEntity` = **单一实例复用**（在飞时按 `bRetriggerOnActive` 分流：**默认 `false` ⇒ 驳回**，返具名原因 `AlreadyActive`；`true` ⇒ **顶替**）。**顶替 = 打断的一种终止形态**：旧运行态发 `OnCastInterrupted`（**MUST NOT** 发 `OnCastCompleted`）、**不起其主链**；且**受 `IsInterruptibleNow()` 门禁**（当前时段不可打断 ⇒ 顶替被拒）。**与 GAS 的两处刻意分歧**（MUST NOT 照抄）：① GAS 顶替走 `EndAbility(bWasCancelled = false)`，在 TCS 会让配 `OnCastCompleted` 起链的旧技能**在被顶替瞬间真打出主链**；② GAS 顶替**绕过** `CanBeCanceled`，本仓明确不采用。**依据**：`GameplayAbilityTypes.h:52-54`（并存原文）、`AbilitySystemComponent_Abilities.cpp:1831-1852`（驳回路径与日志原文）、`:1915-1918`（每次新建、无在飞检查）、`GameplayAbility.cpp:741-743` vs `:802-849`（顶替不经 `CanBeCanceled`）；
- **内联触发行**（**2026-10-07 R6 补入——原字段表遗漏，而 §2 组合范式已在使用"触发行"**）：`Triggers: TArray<FTcsEffectTriggerDef>`——"事件 → 效果链"的映射行，与 `FTcsBuffDef::Triggers`（`TcsBuffDef.h:86`）**同形且为平行的兄弟声明**（基类 `FTcsStateDefBase` **无**此字段，故各派生类自决）。**与 GAS 的关系**：与 `AbilityTriggers`（`GameplayAbility.h:725-727`）**同族**，且本仓 `FTcsEffectTriggerDef` **字段更厚**（9 个 vs GAS 的 3 个，多出 `Conditions` / `GateTags` / `Priority` / `ExecutionGate` 等）——**MUST NOT 表述为"对齐 GAS 的输入引导"**（GAS 的三档 `GameplayEvent`/`OwnedTagAdded`/`OwnedTagPresent` 无输入档，其输入走 `AbilityLocalInputPressed` 独立路径）。**本轮形态 = 零框架登记**：字段进资产、宿主可遍历读取；框架侧**不建登记表、不建生命周期**——**与状态侧的关键差异**：状态侧有明确登记时点（施加时逐行登记、`Source` = 实例级联锚点、移除/到期级联退订），技能侧**无对应时点**（激活由宿主主动调用）⇒ 本轮只落字段。**保留判据**：后续动作游戏的强输入引导（输入事件 → 条件门禁 → 起链）可由本字段零新机制表达；
- 继承自基类：`LevelBase / MaxLevel`（Level 语义见 §3.2）。
- **多形态/多段技能 = 组合范式（D5-14 移除，2026-09-11 用户拍板）**：~~FFormGroup{Forms, AdvancePolicy, ResetPolicy} 内建机制~~ 撤除——形态 = **多个 SkillDef 都学习**（各自独立 Entry/等级/冷却）；路由与推进 = 既有原语组合：阶段标识 **StateInstance**（纯状态标识，ApplyState 挂/到期摘）+ 关系字段门禁（阶段态 Require/Block 对应各形态 SkillDef）+ 触发行（OnCastCompleted → ApplyState 下一段）；"同样的触发逻辑释放 B" = 宿主输入路由（门禁失败原因具名化，逐条目可查询）。等级/冷却/进度跨形态同步 = 宿主职责（SkillModifier/状态标识——用户拍板）。动机：零新机制、Entry↔Def 一对一（EffectiveDefId 随之整体移除）、ParamRef 零歧义。
- **修正器引用行（D5-19）**：`ModifierRows` 同 FStateDefBase（D3-19 模板引用 UTcsAttrModDef/UTcsSkillModDef）——**激活期物化**，Source=施法运行句柄，结算/打断级联摘除；技能参数模板行经自带 FEntrySelector 作用于目标条目；物化上下文=FCastRun.ParamSnapshot。

## 3. 类型词汇（对外）

### 3.1 账本与运行（D5-2/D3-11/D5-12）
- `FLearnedSkillEntry`（struct，中央注册表桶内）：`DefTag / Level(持久) / LearnSource / 冷却轨道状态（每轨 Remaining/Total，D5-15）/ 运行句柄集 / 参数修正链集`。**实例-定义引用规范（D5-13；2026-09-11 随 D5-9 修订收窄；身份 2026-09-22 tag 化）**：权威=`DefTag: FGameplayTag`（原 `FName DefId`；EffectiveDefId 概念已整体移除）；`GetDef()` 解析缓存（const+版本校验）；**FCastRun 执行期零回查 Def**（ParamSnapshot 已冻结一切）。
- `FCastRun`（struct，池化）：`EntryHandle / Level(激活时快照 EffectiveLevel) / PhaseIndex / ChainRunHandle / **ParamSnapshot**`。**无 UObject 壳**（TCS 8 转发 override 壳税不迁移）；索引先只留主表（YAGNI）。
- **StateParamSnapshot（D5-12）**：activate 那一刻一次性解析全部生效参数（逐参数 Mode：Snapshot 走账本冻结 / Live 标记跳过）+ EffectiveLevel + 各时段 Duration（字段引用从快照解析）——链/时段/门禁读快照；**live 修正器通道不受影响**（持续效果实时）；opt-in 实时为唯一例外。

### 3.2 Level（D3-11 终定）
- `EffectiveLevel = clamp(0, LevelBase + Σ参数账本 Level 键修正)`；激活时快照进 FCastRun（运行中升级不追溯）；**等级→数值表进引擎（PV-4 2026-09-11 修订 D3-11；评判轮收束为四源）**：StateLevelArray/Map、InstigatorLevelArray/Map 源住 TcsState（经 `ITcsEntityLevelProvider::GetEntityLevel` 取实体级——**接口定义于 TcsState**、宿主实现，宿主可不经 TcsAttribute 承载等级；TargetLevel 系暂不提供——用户拍板），SkillDef 参数直配；宿主升级事务照旧（运行中升级不追溯）。

### 3.3 查询契约与施法事件（D5-1）
- `ICastStateQuery`：`IsInterruptibleNow() / CanMoveNow()`——打断结算与宿主移动系统询问的唯一合法入口。实现三级：Def 简单开关（默认）/ 时段表（当前时段字段）/ Custom Fragment。
  > **实施注记（2026-10-10，R6 Task 5 落地；保留原文、就地注明）**：实现名 = **`UTcsCastStateQuery` / `ITcsCastStateQuery`**
  > （`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`，形参 = **已反射句柄**
  > `FTcsCastRunHandle`；`FTcsCastRun` 自身非反射，作形参是 UHT 硬错误）。**三档分派按 `ECastQueryMode`**：
  > `CQM_DefSwitches`（默认）读 `bInterruptibleDefault` / `bCanMoveDefault`；
  > `CQM_PhaseTable` 读**当前时段** `Phases[PhaseIndex]` 的字段；`CQM_Custom` 求值 `CastQueryFragment`。
  > **一处降级 MUST 写明**：`PhaseTable` 档**无当前时段**（`Phases` 空或下标越界）时 MUST
  > **具名 `Log` 并回退 `DefSwitches` 档**——**MUST NOT** 越界读、**MUST NOT** 返回无条件"永不可打断"
  > （后者是一处无声的行为变更）。
  > **★ 本轮 `Custom` 档具名未实现并回退 `DefSwitches` 档（归 `R6.5-g`）**：MUST 记具名 `Warning`
  > （含档名），**MUST NOT** 静默当作默认档。**接口此刻"可声明、不可求值"**——
  > `CastQueryFragment` 是 `FInstancedStruct`（可编的**策略结构体**），而本接口是 **UObject 接口**
  > （可被既有类实现），两者**不是同一种载体**；"到底由宿主注入实现对象、还是让片段持一个指向实现
  > 对象的引用"属 `R6.5-g` 的契约裁决 ⇒ **本接口本轮零调用点，是需求明文允许的形态**
  > （同族先例 = `ITcsChainRedirectResolver` 的"注入点可留形状，求值 MUST 具名报未实现"）。
  > 取证：`EVID-2026-10-10-skill-cast-phases` §1.3（`P3` 三档互异）+ §1.2（`R9` 具名 Warning 与回退）。
- 施法事件（核心词汇 FStruct）：`OnCastStarted / OnCastPhaseChanged / OnCastCompleted / OnCastInterrupted`。

### 3.4 参数修正与选择器（D5-5/D5-6/D5-9）
- **参数双表与修正器两结构**：`FTcsNumericParamModifier{ParamKey, Op: Add/PercentAdd/Mul/FlatAdd/Override（**D5-5 v3：与 M2 同一套五带**；原 `AddPct` 命名与 M2 `PercentAdd` 失步已修；Custom op 砍除 2026-09-02 不变——计算在上游传入终值）, Operand: FTcsParamValue（PV 系列 2026-09-11 换型——Literal/ParamRef/AttributeScaled 等源；**复合运算即参数链**：{Add, AttributeScaled(AttackPower)} + {Mul, ParamRef(DamageRate)} 两行折叠 = "攻击力×倍率"，PV-7）, Source, ~~SortKey~~, CompeteGroup(GameplayTag, 可选), ValueConvention（D5-18 v3：链行 Operand 补约定列）}` 与 `FBoolSwitchModifier{SwitchKey, Value, Source}`（D5-5 v2 命名统一，原 FLogicGateModifier）；**参数链 = 带式聚合**（**D5-5 v3：与 M2 完全同式、顺序无关**——~~`Override` 存在取组内**最大值**直接作为结果~~（FlatAdd 一并被覆盖），否则 `((初值 + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；带权同 M2 `Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30`，~~**SortKey 退化为带权、不再承担排序语义**~~；**折叠初值 = 该键参数行的求值结果，无参数行则 0**；**折叠器单份住 TcsAttribute**，M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用——02 §2.2a"同一形状、作用域容器不同"的复用落点）；Source 注销级联撤销；**特殊键 `Level`**（EffectiveLevel 修正，DNF 式 +1 = Add+1；**该键由 TcsSkill 原生声明**为 `TcsStateParam.Level`，见下方订正 ③）；**CompeteGroup（D5-19）**：读侧竞争组——折叠前按组分桶，组内解析值最大者进折叠（账本全量、Source 撤销自动递补，无休眠池）；~~"取优先级最高"=**Override 组取最大值**~~（与 M2 一致；D5-5 v3 取代原 `Override+SortKey` 口径），值竞争与优先级竞争可共存（先选优后**按带**折叠）；同组跨 Op 校验器提示。

> ⑥ **定义侧 `FTcsNumericParamModifier` MUST NOT 携带 `Source`**（2026-10-09 用户质疑后订正）：
> 上一段字段表里的 `Source` **是假字段**——`FTcsSourceHandle` 是**非反射纯 C++ struct**
> （`TcsSourceHandle.h:20` 裸 `struct`）⇒ 不能作 `UPROPERTY`（UHT 产物实测：该类型反射属性
> **无 `Source`**）⇒ **既不进细节面板、也不进资产**；且定义侧物化路径盖的是 `FTcsCastRun.RunSource`、
> **根本不读行上的值**（全库唯一读 `Row.Source` 的是外部施加路径）。**语义根因**：
> **"源"从来不是"这一行"的属性**——同一个 `ParamChainRows[0]`，甲玩家靠装备给、乙玩家靠天赋给
> ⇒ **来源因实例而异，属实例而非 Def**。M2 的形状即此（`FTcsAttrModDefTableRow` 无 `Source`，
> 它是 `MakeFromDef(Row, Operand, InSource)` 的**形参**）。⇒ `Source` 只住账本侧，由**施加方**给出：
> 定义侧物化传 `RunSource`，外部施加经 `ApplyParamModifiers` 的 **`Source` 形参**；
> 且**来源无效时 MUST 拒绝施加**（无锚点的条目摘不掉 ⇒ 违反成对性判据）。
> **账本侧命名同批订正**：`FTcsNumericParamInstance` → **`FTcsNumericParamModInstance`**
> （与 M2 `FTcsAttr**Mod**Instance` 逐字同构；命名规则 = **`[表词] + Mod + 面后缀`**；
> 布尔侧表词 = `BoolSwitch` ⇒ 将来是 `FTcsBoolSwitchModifier` / `FTcsBoolSwitchModInstance`）。

> **★ 就地订正（2026-10-09，TCS R6 Task 4 落地时；MUST 保留原文，勿抹除本注）**——上一段有三处
> 与**实现事实**失步，三处**同时**存在且互相矛盾，故按"保留原文 + 就地注明"处置
> （判据 = 本仓 `MEM-20261009-01`「设计文本的字段表是意图速写，MUST NOT 当形状真相」）：
>
> ① **`SortKey` 与 `OverridePriority` 是两个字段，不是"退化"关系**。原文只列 `SortKey` 并说它
> "退化为带权、不再承担排序语义"——**同一句又说 Override"取组内最大值"**，而最大值口径本该由谁
> 承担完全没有字段。实现（照 M2）是**两字段并存**：`OverridePriority` = 仅 `TAO_Override` 带读的
> 强弱排座次（**折叠器读它**）；`SortKey` = 同带内展示/审计位（**折叠器不读**，带序唯一真相在 `Op`）。
> 依据 `TcsAttrModDef.h:79/:88`、`TcsAttributeBandFold.h:134/:137`、`:17` 明文"SortKey 不参与折叠"。
> **MUST NOT 令 `OverridePriority = SortKey`**——那会让 M5 的 Override 胜负判据与 M2 **不同**。
>
> ② **Override 不是"取组内最大值"，而是三级比较**：`优先级大者胜 → 优先级打平才按策略比数值 →
> 仍打平比有符号值`（`IsStrongerTcsOverride`，`TcsAttributeBandFold.h:71-89`）。"取最大值"只是
> **默认策略**（`OTB_Max`）下的表现形式。技能参数**没有"属性定义"这一层**，故 M5 一律取折叠器
> 默认策略（先例 = 同族第三处消费者 TcsDamage 的 `TcsFlowAttributes.cpp:45` 两实参调用）。
>
> ③ **特殊键 `Level` 的"词"由框架原生声明，且声明方 = `TcsState`**（2026-10-09 用户裁定，其后经用户
> 追问再订正声明模块）：`TcsStateParam` 根的**一般键**归宿主 ini（内容归属），但 `Level` 是**框架自己
> 解释的词**（`clamp(0, LevelBase + Σ)` 这条公式写在 D3-11 里）⇒ 按 `gameplay-tag-governance`
> 「框架契约词 MUST 由插件模块原生声明」落为原生 tag `TcsStateParam.Level`
> （`TcsState/Public/Param/TcsStateParamKeys.h`）。**为什么住 `TcsState` 而不是 `TcsSkill`**：
> ① `LevelBase` / `MaxLevel` 两个字段声明在 `FTcsStateDefBase`（技能 Def **继承**它）——**M5 没有自己的
> "等级定义"层**，等级本体论在状态侧；② **依赖方向**：`TcsSkill` 已依赖 `TcsState`，声明在上游则两侧
> 消费零成本，若声明在 `TcsSkill` 则状态侧将来消费就要**反向依赖、成环**。
> 若下放宿主配置，宿主漏配 ⇒ `Level` 修正**静默失效**（数值照进账本、等级永不动、零报错）。
>
> **另两处同批订正（形状层，非本节原文）**：④ `FTcsEntrySelector` 的 `EMode` 需要自己的
> `ETcsEntrySelectorMode` 枚举（全仓无现成枚举可复用）——"零新枚举"的口径只覆盖**修正器的三个
> 字段类型**（`Op`/`Operand`/`ValueConvention` 复用既有），不覆盖选择器；⑤ `ParamChainRows`
> 的**求值参数表** = 本次施法运行态的参数快照（`ParamRef` 类操作数从它取值），而
> `GetNumericParam` 走**实时**通道（参数行初值 + 账本槽位折叠）——快照**不是**公共读口的冻结来源
> （判据：快照住每次 run，而读口签名无 run 段，且同一 Entry 可有两个 run 并存 ⇒ "读哪一次"无唯一答案）。

- ~~`FEntrySelector{Mode: All/ByTag/ById/Custom, Params}`~~——外部来源选择作用于哪些已学条目。
  **★ 就地订正（2026-10-09 用户裁定，MUST 保留原文）**：上句的档位集合**已作废**，实现名 = `FTcsSkillEntrySelector`
  （**名字带 `Skill` 段**：它只服务技能账本条目）。三处修正：
  - **`ByTag` → `ByDefTag`**：原档名含混——它读的是条目的 `DefTag`（内容身份），而"Tag"在 TCS 里同时指
    属性词/状态词/事件词/参数键等七八种东西 ⇒ 档名 MUST 与字段名 `DefTag` 对齐。
  - **原 `ById` 档删除**（删的理由 MUST 记录，防后人"照旧稿加回来"）：它按 `FTcsSkillEntryHandle`
    （`Index` + `Generation`）精确命中，而该句柄是**运行时产物**——编辑器里没有、也无法预先填出合法值
    ⇒ 对"以配置为入口的外部施加"而言**这一档等于不存在**（只有代码能传）。且"按 Id 筛"的**历史由来**
    （Def 资产身份原为 `FName DefId`）**已于 2026-09-22 全量迁移到 `GameplayTag`（`DefTag`）**
    ⇒ 其正当形态就是**按 tag 筛**，已被 `ByDefTag` 覆盖（同一份内容身份不需要两档）。
  - **新增 `ByCategoryTags` 空壳**（承接删除位，**本轮只留壳不落机制**）：目标形态 = 按定义上的
    **类别标识容器**（`FGameplayTagContainer`——如一个 Buff 同具"火属性伤害 + 异常状态"，一个技能同具
    "左手/右手/双手释放（三选一）+ 投掷类 + 引导类"）筛选，规则**倾向复用引擎 `FGameplayTagQuery`**
    （`HasAll` / `HasAny` 等表达式）。**只留壳的判据**：① **载体不存在**——`StateDef` 侧的类别标识容器
    字段尚未新增（用户裁定"所有 StateDef 都应新增"，属**独立变更**）；② **规则与档名未拍板**——用户明示
    "后续可能改名 `ByCategoryTag` / `ByCategoryTags`，**还要讨论**"；③ **可能另立模块**——用户指出 GameplayTag
    筛选机制在 TCS 里将是重要角色、**甚至可能单独开一个 `TcsGameplayTag` 模块** ⇒ 现在写进 `TcsSkill`
    内部将来极可能要搬家。⇒ 本档行为与 `Custom` 档同款：**具名报出未实现 + 零修正被施加**。
  - **台账登记**：`STAT-10`（类别标识载体 + 筛选规则 + 模块归属，三项一并裁）。
- **Def 自带参数修正行（PV-9，2026-09-11 采纳）**：`SkillDef.ParamChainRows`——声明作用域恒为本条目自身的 `FTcsNumericParamModifier` 行（可内联或引用 UTcsSkillModDef 模板），**无 FEntrySelector**（该选择器保持专属外部施加场景）；激活期物化进本条目参数链，Source=施法运行句柄，结算/打断级联摘除（与 ModifierRows 同生命周期语义）。**复合参数 = 链行带式折叠**（"攻击力×倍率 + y" = {Add, AttributeScaled(AttackPower)} + {Mul, ParamRef(DamageRate)} + {FlatAdd, ParamRef(DamageAddition)}，PV-7 + D5-5 v3 带式口径，任意书写顺序）；State/Buff 侧参数为快照单值、无账本链，暂不需要对应机制。
- **技能级替换（D5-9 修订 2026-09-11：移除 EffectiveDefId 技能级重定向）**：~~`FSkillRedirect` 重定向栈~~ 撤除——整体逻辑替换 = **直接换成新 Skill（新学习身份/Entry：Grant/Revoke，或 ReplaceSkill 链步骤原语判定树候补）**，不做同 Entry 跨 Def 漂移。动机（用户）：Entry↔Def 一对一后参数键空间绝对纯净，SkillDef 内 ParamRef 同域解析零歧义（PV-2.d 收束）；旧方案"保留身份、跨 Def 漂移"会让 Entry 上按旧 Def 键空间声明的参数修正器全部悬空。**让渡模式由三粒度收窄**：参数级（NumericSkillModifier/BoolSwitchModifier）→ 链级（链重定向栈，保留）→ 技能级 = 新 Skill。等级/冷却进度/形态进度不跨替换继承（需要则宿主显式迁移）。
- **多形态路由（D5-14 移除后的组合范式，2026-09-11）**：无内建形态解析——各形态是独立 Entry；"当前形态" = 阶段标识 StateInstance（Tag 门禁）；**ParamRef 恒解析于 Entry 自身 `DefTag` 参数表**（PV-2.d 终版钉死——无条件）。

### 3.5 冷却（D5-15/D5-16）
- 多轨道模型（D5-15）：Tracks 数组（自身轨道/共享组轨道），Timing 保留 policy 级；CDR = 参数链 `CooldownPct` 键，**快照于冷却触发时刻**（中途变化不追溯），作用于各轨 Duration；到期堆驱动；真值在 Entry（每轨 Remaining/Total 可查，UI 轮询）；默认无冷却（空 Tracks）。
- **冷却事件词汇（D5-16）**：`OnCooldownStarted`（Timing 触发点）/ `OnCooldownUpdated`（运行中被 AdjustCooldown/ResetCooldown 改动）/ `OnCooldownEnded`（到期堆触发，权威侧）——总线 FStruct，payload `{EntryHandle, SkillId, Track, GroupTag?, Total, Remaining, EndTimeSeconds}`；UI 径向照旧轮询，事件供 AI/连招/音效触发式响应。
- 引擎不内置职业自动归类等游戏专属概念；公共冷却 = 共享组轨道的数据表达（D5-15）。

> **★ 2026-10-12 R6.5-1 落地注记（原文逐字保留，MUST NOT 抹除上三行；落地名与三处订正见下）**：
> 上段已**全部落地**（`PLN-R6.5-1` Task 0~5，提案三份同日归档 ⇒ 能力数 40 → 42，新增
> `skill-cooldown`）。**落地实现名**：`ETcsCooldownTiming`（四档）/ `FTcsCooldownTrack` /
> `FTcsCooldownPolicy` / `FTcsCooldownTrackState` / `FTcsCooldownGroupState` / `FTcsCooldownOps` /
> `FTcsCooldownEventPayload`。**三处与正文的订正**：
> ① **落地事件名去掉 `On` 前缀**——`TcsEvent.Cast.CooldownStarted` / `.CooldownUpdated` /
> `.CooldownEnded`（判据：`TcsEvent.Cast` 根下既有四枚 `Started`/`PhaseChanged`/`Completed`/
> `Interrupted` **零 `On` 前缀**，按同族对称；**MUST NOT 照抄 `On*` 进 tag 名**）；
> ② **payload 的 `SkillId` 用 `DefTag` 取代**——`D5-9` 修订后 `EffectiveDefId` 已整体移除，
> "用哪份定义"的唯一答案就是 `DefTag`（**MUST NOT 为它发明一个类型**）；
> ③ **`Custom(ITcsCooldownTimingFragment)` 第四档整体删除、MUST NOT 补回**——自定义时机 =
> "不自动执行，让宿主在自己的确定逻辑处手动调用"，那正是 `ECT_Manual` 档，**不需要任何载体**
> （判据见 `PLN-R6.5-1` §0.2 裁决 ①）。**另新增第三枚可变性口 `StartCooldown`**——
> `D5-16` 只承诺了"改"（`AdjustCooldown`）与"清"（`ResetCooldown`），**都不是"开始冷却"**
> ⇒ 无它则 `ECT_Manual` 档物理上无法兑现。
> **事件与可变性成对**（`MEM-20260902-12`）：三枚事件分别对应起冷 / 改轨清轨 / 到期，
> 且**不存在**"有事件无状态改变"或反过来的情形。**根数不增**（仍 12——落既有 `TcsEvent` 根）。

### 3.6 Cost（D5-10）
- 策略×时机（见 §2）；门禁第 5 道 = CanAfford，提交按 Timing（激活事务内或事件点）；ResourceAttr 策略走 M2；v1 单资源；道具/弹药 = 宿主 Fragment；返还未设引擎字段；**OnCostApplied 事件（D5-16）**：ResourceAttr 策略 Pay 提交后发，payload `{EntryHandle, SkillId, ResourceAttr, Amount}`——CanAfford 失败不发（TryActivate 返回值覆盖），CustomFragment 成本自行决定是否发同词汇事件。

> **★ 2026-10-12 R6.5-1 落地注记（原文逐字保留，MUST NOT 抹除上段；落地名与四处订正见下）**：
> 上段已**全部落地**（`PLN-R6.5-1` Task 2~4）。**落地实现名**：`ETcsCostPolicy`（三档）/
> `ETcsCostTiming`（**三档**，2026-10-12 修订前为四档——见上方裁定段）/ `FTcsCostConfig` / `FTcsCostOps` / `FTcsCostEventPayload`。
> **四处与正文的订正**（③ 的后半**已被上方裁定段取代**，此处按原样保留以记现场）：
> ① **落地事件名去掉 `On` 前缀** = `TcsEvent.Cast.CostApplied`（同 §3.5 的理由）；
> ② **payload 的 `SkillId` 用 `DefTag` 取代**（同 §3.5）；
> ③ **`ETcsCostTiming` 无 `OnCastInterrupted` 档**（`D5-4` 已砍返还 ⇒ 该档两种读法一为重复一为荒谬）；
> ~~**`OnEvent` 档采纳**~~（~~`D5-10` 原本就有；"等哪个事件"是数据 `TriggerEventTag`、"到了做什么"是行为
> 走 `UTcsCostEventHandler` 订阅 ⇒ 不需要策略片段载体~~）——**2026-10-12 裁定删除该档，本半句作废**；
> ④ **扣减载体 = `SetBaseValue` 直写**（读基值 → 减 → 写回），**MUST NOT 用 `ApplyModifier` 挂修正器**
> ——判据三条见 `add-skill-cost`（语义 / 来源锚点矛盾 / `CHAIN-8` 已立"修正器层效力不等同"）。
> **判定一律 fail-closed**：无效 `ResourceAttr` / `Amount < 0` / 未持有属性 ⇒ **拦住 + `Warning`**；
> `ECP_None` 与 `Amount == 0` ⇒ 恒可付、**零日志**（"正确声明不消耗"与"想配消耗但没配清"是两件事）。
> **门禁⑤ 恒在 `TryActivate`，提交点各自重查一次**（TOCTOU 防护）。
> **`CustomFragment` 档具名未实现 + 回退 `ECP_None`**。**★ 阻塞登记已于 2026-10-12 改正（原记"= `SCRIPT-9`：属性写入面的脚本可达性"——该归因不成立，MUST 读）**：该档**不依赖属性写入面的反射**，两个独立理由各自都足以推翻原记——① Cost 侧明文 **MUST NOT 用 `ApplyModifier`**、必须直写基值，而 `SetBaseValue` **有** `UFUNCTION` ⇒ 需要的那一半**早已可达**（`SCRIPT-9` 缺的是 `ApplyModifier` / `RemoveBySource`，**与本档无关**）；② **更根本：宿主自定义消耗根本不经过 TCS 属性写入面**（扣"无色小晶块"是在宿主自己的背包系统里减数量）。**真阻塞 = 扩展点形状从未裁定**：`D5-10` 只给了形状名 `ICostPolicy`（`USTRUCT` 策略基类 + C++ 虚分派），而该形状**对脚本侧物理不可达**（同 `FTcsParamValueSource` 的实测判据）⇒ 正确形态 = **宿主插槽家族**（`UINTERFACE` + `BlueprintNativeEvent` + 转发器），本模块已有同型先例 `UTcsCastStateQuery`（`CQM_Custom` 档）。**已入册 `COST-3`**（触发条件型）。**一处签名硬约束**：`FTcsLearnedSkillEntry` 非反射 ⇒ MUST 传已反射的 `FTcsSkillEntryHandle`。

> **★ 2026-10-12 裁定修订（R6.5-1 同日，用户裁定；上两段的原文与落地注记**逐字保留**，
> MUST NOT 抹除——本段是**对它们的修订**，不是覆盖）**：
>
> **`ETcsCostTiming` 由四档降为三档**：`OnCastStarted` / `OnCastCompleted` / `Manual`。
> **`ECT_OnEvent` 档整体删除、MUST NOT 补回**（`D5-10` 原文确实列了它 ⇒ 这是**对设计决策的
> 修订**，处置口径同 `D5-4` 砍返还 / `D5-15` 砍 `Custom` 档）。
>
> **判据**：框架只能认识"**施法这个事务自身的边界**"（开始 / 完成）——这两个时点在**每个项目里
> 含义都相同**；而"**某个特定事件**"（挨打时付费 / 连击到第 3 段付费 / 血量低于 30% 付费……）
> 是**项目词汇** ⇒ 框架内置它等于**让框架去猜项目的玩法**，且它给出的只是一个"等某个 tag"的
> 空槽位（"这个 tag 是什么、什么时候发"全在宿主手里）⇒ **框架提供了一个自己解释不了、
> 也无法校验的档位**。与主判据「**凡是需要宿主词汇的内置策略，框架一律不提供**」直接冲突。
>
> **宿主实现成本极低、零新框架面**：监听自己关心的事件 → 调 `PayCost`（公开口）。
> 那正是 **`ECT_Manual` 档 + `PayCost`** 的既有组合。
>
> **同批删除的伴随面**（判据 = 「零消费者不预建」的**反向**使用：消费者归零 ⇒ MUST 删掉）：
> `FTcsCostConfig::TriggerEventTag` / `UTcsCostEventRelay` 整个类 / 门面 `GetCostEventRelay()`
> 与 `CostEventRelay` 成员 / 三处撤销路径的中继退订 / 装置的两个检查 `H1`·`H2` /
> 两条检查用 tag（`SkillDef.Check.R65OnEvent`、`TcsEvent.Check.R65Trigger`）。
>
> **保留不动**：`CostApplied` 事件本身（与档位无关，任何提交路径都发它）、
> `ECT_Manual` 档 + `PayCost`、`ECP_CustomFragment` 的具名降级。
> **`ETcsCooldownTiming` 不受影响**——**冷却侧本来就没有 `OnEvent` 档**
> （其四档是 Started / Completed / Interrupted / Manual）⇒ 用户"Cooldown/Cost 都删"的意图
> 在冷却侧**已是现状**。
>
> **规格面落点** = 提案 `remove-cost-onevent-timing`（`skill-cost`：3 REMOVED + 3 ADDED）；
> **影响面实测 = 0**（全仓 `.uasset` 扫 `ECT_OnEvent` / `TriggerEventTag` **零命中**）。

## 4. 入口服务

- `GrantSkill / RevokeSkill(unit, DefTag, Source)`
- `TryActivate(unit, SkillId, Context) -> ESkillActivateResult`：**显式门禁序列**（实体 Ready → 已学 → 冷却 → Instancing 顶替/并存判定 → CanAfford → Def 校验；每道门具名原因——修 TCS 门禁内联缺陷）
- `CancelCast(run, Reason)`；~~`ApplyParamModifiers(unit, FEntrySelector, TArrayView<FParamModifier>)`~~（**实现名 `FTcsSkillEntrySelector`**，见 §3.4 的就地订正）；`MaterializeModifiers(unit, TemplateIds, ParamContext, Source)`（D5-19 宿主命令式入口——与声明式 ModifierRows 共用同一物化器）；`AdjustCooldown / ResetCooldown(unit, EntryHandle, Track, Delta|Clear)`（D5-16 事件源——改动轨状态发 `OnCooldownUpdated`）；`GetNumericParam / IsSwitchSet / GetLevel`

## 5. 关键机制

- **门禁序列**：六道具名门（修 TCS 内联无钩子缺陷，TCS 报告 09，库外，已不在库内）。
- **时段驱动**：进入时段 → 到期堆注册（Duration 从 ParamSnapshot 取）→ `OnCastPhaseChanged`；`IsInterruptibleNow` 按三级实现解析；时段参数在快照构建时已定。
  > **实施注记（2026-10-10，R6 Task 5 落地；按 `docs-convention` §7 保留原文、就地注明）**：
  > 上句**未写完成路径**——本轮补上，且**终结点是四路而非三路**：
  > ① 全时段走完、② 打断、③ 顶替、④ **瞬发（空时段表）= 激活当刻完成**。
  > **为什么必须有第四路**（`PLN-R6` Task 5 计划未定义、经用户 2026-10-09 裁定甲′补入）：
  > 空 `Phases` 意味着**没有任何时段到期** ⇒ 原句"全时段走完 → `OnCastCompleted`"
  > **对瞬发技能是一条永不成立的规则**。而"瞬发 = 空表"是 `skill-def-asset` 规格**钉死**的表达
  > ⇒ 缺第四路时，瞬发技能**只能靠打断结束**（把一次成功施法报成"被打断"，且池槽位与账本
  > `RunHandles` 永久占用）。**落地形状**：四路**汇聚同一个终结例程**（`FinishCastRun`），
  > **四路只在"发哪一枚终止事件"这一处分岔**（完成 → `Completed`；打断/顶替 → `Interrupted`）。
  > **判据** = `PhaseIndex < Phases.Num()`（"还有没有待推进时段"），**MUST NOT** 写成
  > `Phases.IsEmpty()` 空表判定——那会让"已走到末尾"与"空表"走两条路（同一件事两个入口）。
  > 取证：`EVID-2026-10-10-skill-cast-phases` §1.3（`P2` ×4 / `P8` ×9）。
- **打断**：来源优先级 vs `IsInterruptibleNow()` → CancelCast(Cancelled) → `OnCastInterrupted`；止于未来不追溯（M4 约定）。
  > **实施注记（2026-10-10，R6 Task 5 落地）**：上句**未写归一例程**——本轮补上：
  > 打断**与完成/顶替共用一个终结例程**（**MUST NOT 各写一份清理**；多份必然漂移，
  > 后果是**静默泄漏**：条目留在账本里、属性不回退、无人报错）。
  > 结算序 = **先过查询契约**：`IsInterruptibleNow()` 判可打断性 → 不可打断 ⇒ **拒绝**
  > （运行态原样继续、零事件、零账本改动）→ 可打断 ⇒ 走归一例程 + 记原因为"打断"。
  > **另如实登记一处未落面**：上句的"**来源优先级**"**本轮未落**——全仓**无任何类型**声明
  > "打断来源的优先级"（技能关系字段 `Priority` 的语义属 `R5.5-e` 关系字段检查器，未定稿）
  > ⇒ 本轮**只落可打断性裁决**，**MUST NOT** 读成"优先级仲裁已实现"（也不为它发明零消费者的形参）。
  > 取证：`EVID-2026-10-10-skill-cast-phases` §1.3（`P4`/`P4b` 成对）、§6.2 边界 6。
- **冷却触发（D5-15/D5-16）**：Timing 枚举到达 → CDR 快照 → 各轨到期堆注册（组轨写单位级组槽）→ Entry 轨道状态翻转 → `OnCooldownStarted`；`AdjustCooldown/ResetCooldown` 改动轨状态 → 重挂到期堆 → `OnCooldownUpdated`；到期出队 → `OnCooldownEnded`。
- **起链解析**（D5-7；链级让渡保留）：`CastChainId` 解析 = 全局定义 → 链重定向栈 → Custom Fragment；生效 Def（= Entry 自身 `DefTag`，EffectiveDefId 已移除）决定用哪套参数。
- **ParamSnapshot 构建时序**：门禁全过 → BuildSkillSnapshot（按 Entry 自身 Def 账本求值一次）→ FCastRun 绑定 → 后续一切读快照。

## 6. 网络姿态落点（NET-1/2）

- 权威侧跑门禁/施法/账本/冷却；**操作复制**：Grant/Revoke/ParamModifier/施法事件流 + ParamSnapshot；客户端观感预测仅限本地施法表现（施法条/动画）——结算以权威为准；接口位本期不实现。

## 7. 非目标

不做输入系统对接；不做 UI/图标；不做动画状态机耦合；无脚本层；~~不做等级→数值表~~（PV-4 2026-09-11 修订：等级表参数源进引擎归 TcsState；宿主升级事务仍归项目）；不做多资源消耗（v1 单资源，多资源=未来新原语）；不做 Mass（未来 TcsMass）。

## 8. 依据

- 拍板：D5-1~D5-16（2026-09-02 三轮问答框+多轮增补；用户贡献：施法阶段降级为查询契约、冷却策略×时机矩阵、OnInputReleased/OnStateEnded 砍除、vector 砍除、固定通道撤回→任意键、Cost 策略×时机、Instancing 二分、Level 双侧携带、StateParamSnapshot 参照引入、技能级重定向提案、冷却三事件+OnCostApplied（D5-16）、冷却多轨道泛化（D5-15）、BoolSwitch 命名失步抓出（D5-5 v2））。
- 证据：TCS 09/01/ZZ 核验（门禁内联、冷却 0.1s Tick、双账本与参数链、单实例顶替、无前摇后摇系有意设计——MEM-20260902-04）；MEM-20260819-07 三问标尺；MEM-20260902-04/06。

## 9. 修订记录

- v1（2026-09-02）：初版决策折入。
- v2（2026-09-02）：折入 D5-9（技能级重定向）、D5-10（Cost 策略×时机）、D5-11（ECastInstancing）、D5-12（StateParamSnapshot，Skill 侧）、D3-10/11（Skill 侧：类层级/Level 双侧）、参数双表+Mode 列、关系字段映射、MainChainStart 字段化。
- v2 增补（2026-09-02）：折入 D5-14（技能形态组 FFormGroup——亚托克斯 Q/鸣潮案例）——EffectiveDefId 解析链扩展形态来源；学习身份单份；轮转/条件=政策数据。
- v2 增补 2（2026-09-02）：折入 D5-15（冷却多轨道泛化——Strategy 枚举溶解进轨道，真 GCD=纯数据）、D5-16（冷却三事件+OnCostApplied 词汇+AdjustCooldown/ResetCooldown API）、D5-5 v2（FBoolSwitchModifier/IsSwitchSet 命名统一）。
- v2 增补 3（2026-09-02，M9 收尾轮）：折入 D5-17（描述文本与数值绑定——DescriptionTextKey/命名空间占位符/组装器包样式/M8 占位符扫描）、D5-18（ValueConvention 值约定——写入点转规范值+UI 反变换；升格 TcsNotation 第十一模块）；§1 依赖加 TcsNotation。
- v2 增补 4（2026-09-02，R3 计划审阅轮 4）：折入 D5-19 技能侧修正器模板（激活期物化、Source=施法运行句柄、MaterializeModifiers）+ CompeteGroup（GameplayTag 竞争组读侧选优）+ 优先级语义对号（Override+SortKey=取优先级最高、Exclusive 休眠池不迁移）。
- v2 增补 5（2026-09-11，PV 系列）：参数行 Base/时段/冷却/参数链 Operand 全量换型 **FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}**（D2-12 FTcsParamScalar 被取代）；D3-11 修订——等级表源进引擎（StateLevel/InstigatorLevel × Array/Map 归 TcsState，`ITcsEntityLevelProvider` 定义于 TcsState；TargetLevel 系暂不提供——评判轮用户拍板）；**PV-7 复合运算=参数链**（"攻击力×倍率" = Add(AttributeScaled)+Mul(ParamRef) 两行折叠，结果输入伤害流程；链行来源 = **`SkillDef.ParamChainRows`**——PV-9 已采纳）。
- v2 增补 6（2026-09-11，D5-9 修订——用户拍板）：**技能级重定向（FSkillRedirect/EffectiveDefId 重定向栈）移除**——整体逻辑替换 = 直接换成新 Skill（新学习身份/Entry；ReplaceSkill 链步骤原语列判定树候补）；动机 = Entry↔Def 一对一保证 ParamRef 同域解析零污染（PV-2.d 随之收束：ParamRef 恒解析于 Entry 自身 DefId 参数表，形态组场景=当前生效形态 Def）；三粒度让渡收窄为参数级+链级；等级/冷却进度不跨替换继承。
- v2 增补 7（2026-09-11，D5-14 移除 + EffectiveDefId 移除——用户拍板）：形态组内建机制（FFormGroup/FCastSequenceState/AdvancePolicy/ResetPolicy）撤除——**多形态 = 多学习 Entry + 阶段标识 StateInstance + 关系字段门禁 + 触发行推进**（零新机制组合；等级/冷却/进度跨形态同步归宿主：SkillModifier/状态标识）；**EffectiveDefId 概念整体移除**（重定向已撤、形态解析归组合——无消费者）；描述绑定/ParamSnapshot/ParamRef 全部按 Entry 自身 Def。
- v2 增补 8（2026-09-14，参数折叠与展示轮）：折入 **D5-5 v3**（§3.4 参数链 = 带式聚合——五带同 M2、顺序无关、SortKey 退化为带权、折叠初值 = 参数行求值结果否则 0、Override 组取最大值、CompeteGroup 选优后按带折叠、折叠器单份住 TcsAttribute 三处共用）、**D5-18 v3**（链行 Operand 补约定列）、**D5-17 v2**（§2 描述绑定词法族 + 索引口径 + 绑定链）、命名批（`FTcsNumericParamModifier`/`FTcsNumericParamRow`）；§2 参数行源落点删去"TargetLevel"残留（PV-4 评判轮暂不提供）。
- v2 增补 9（2026-09-15，D5-17 v3 描述视图策略化——用户重设计）：§2 描述绑定改**视图策略体系**——`Descriptions: TArray<FTcsDescriptionEntry>` 取代单字段 DescriptionTextKey；StringTable 只含槽名（零语法）；视图 = `FTcsParamView` 策略基类 + `TInstancedStruct` 持有（内置 Value/Series/Range/Attribute——v2 词法转世）；编辑器预警四层（探针拦截挂配置元素/槽名交叉扫描/运行期降级）；索引口径与展示政策归开发者原则不变。
- v2 增补 10（2026-09-23，标识体系 tag 化改造回写）：§3.1 Entry 身份 `FName DefId` → **`FGameplayTag DefTag`**；§3.4/§4/§5 同步（ParamRef 解析锚点、`GrantSkill/RevokeSkill` 签名、起链解析生效 Def）。落点 = 提案 `switch-identifiers-to-gameplay-tags`（2026-09-22 归档）。§2 参数行 Key 与冷却 GroupTag 本就是 tag/词表口径，未动。
- **v3（2026-10-12，R6.5-1 落地回写）**：§3.5 / §3.6 **各加一段落地注记**（原文逐字保留）——冷却与消耗**运行态已落地**（`PLN-R6.5-1` Task 0~5），含**四处落地名订正**（事件名去 `On` 前缀 / payload 的 `SkillId` 用 `DefTag` / `ITcsCooldownTimingFragment` 整体删除 / `ETcsCostTiming` 无 `OnCastInterrupted` 档 + 采纳 `OnEvent` 档）与**三处新增公开口**（`StartCooldown` / `PayCost` / `CanAfford`）；§11.1 的"门禁只 4 道可拒"**时态到期**（六道全可拒）；§11.2 前四行**已兑现**（加"处置"列，原文保留）；§11.3 补三行实现名对照（冷却族 / Cost 族 / 事件名与 `SkillId`）。**落点 = 三份提案同日归档**（`add-cooldown-and-cost-types` / `add-cooldown-multitrack` / `add-skill-cost`）⇒ 能力数 **40 → 42**（新增 `skill-cooldown` / `skill-cost`）、`validate --all --strict` = **42 passed / 0 failed**。**证据** = `EVID-2026-10-11-r65-cooldown-cost`（**无头与编辑器 PIE 双路线均已取证**：`Run` 立即面 **18/0** + 尾段 **21/0**、`.Reject` **7/0**，两轮独立 PIE 世界逐字相同（✅ 两条路线现已同版本：基线 DLL + 新夹具，缺口已闭合，见同档 §1.1）；补取 PIE 时**抓出两处夹具缺陷**并修复，见该档 §4.4）。

## 10. 验收钩子

M5 专项人工检查路径：门禁六态逐态触发；冷却策略/时机切换 + CDR 快照；参数链 Source 级联（装备卸下→修正消失）；**参数链带式折叠验一项：同一组链行打乱书写顺序，结果不变（Mul 写在 Add 之前仍是"先加后乘"）**；链重定向挂/摘 + 多形态组合范式（阶段标识 StateInstance + 门禁路由——"同样触发逻辑释放 B"）；ParamSnapshot 冻结验证（施法中途挂修正器不影响本次伤害）；Param() 时段时长被修正器改变（下一次激活生效）；冷却三事件触发序（Started/Updated/Ended）+ 多轨道并存与组槽联动（自身 CD+GCD 组轨）+ OnCostApplied 发送时机。

---

## 11. R6 实施范围与切分（2026-10-11 收窄轮）

> **体例同 `SPEC-02-states` §12**：本文正文是**定稿设计**（"应该是什么"），本节是**实施视角的收窄**（"实际落了什么 / 什么没落 / 没落归谁"）。**正文只作两处订正**（见 §11.6），其余一字不改；**凡正文与本节冲突，以本节为实施实况**。
> **本轮 = R6（M5 技能层）**，计划 `PLN-R6`；七个 Task（0~7）全部完成并取证，两个提案均已归档。

### 11.1 本轮落地（R6，Task 0~7）——`Source/TcsSkill/` 47 文件

| # | 面 | 落地内容 | 证据 |
|---|---|---|---|
| 1 | **模块与 Def 资产族** | `TcsSkill` 成第 9 个模块（`TcsCore ← TcsNotation/TcsAttribute ← TcsEffect ← TcsSkill`，**不依赖 `TcsDamage`/`TcsTargeting`/`TcsIntegration`**）；`UTcsSkillDef` 资产 + `FTcsSkillDefData` 数据（继承 `FTcsStateDefBase`）+ `FTcsBoolSwitchRow` / `FTcsPhaseSpan` / `FTcsSkillActivateResult`（`SAR_` 前缀）/ `FTcsSkillDefTableRow` / `TcsSkillEnums`；定义库**第五条按类发现路径**（`DiscoverSkillDefs` / `ResolveSkillDef`） | `EVID-2026-10-07-skill-def-asset` |
| 2 | **账本与门禁第一道** | `FTcsLearnedSkillEntry` + per-unit 桶与**槽位代际**（`FTcsSkillRegistry`）+ `FTcsSkillOps`（授予/撤销/按来源级联/解析/读取）；`GrantSkill` / `RevokeSkill` / `RevokeSkillBySource` / `UnregisterUnit`；`ITcsEntityQuery::IsEntityReady` 作门禁第一道（与 `IsAlive` **正交**） | `EVID-2026-10-08-skill-registry` |
| 3 | **施法运行态与六道门禁** | `FTcsCastRun` 池化运行态 + `FTcsCastRunHandle`（含 `GetInner`/`SetInner` 转换对）+ `TryActivate` **六道具名门禁**（实体 Ready / 已学 / 冷却占位 / Instancing 四路分流 / CanAfford 占位 / 定义可解析）；激活期 `ParamSnapshot`；四枚 `TcsEvent.Cast.*` 事件与载荷 | `EVID-2026-10-08-skill-cast-runtime` |
| 4 | **参数链（带式折叠）** | 双形状修正器（定义侧 `FTcsNumericParamModifier` / 账本侧 `FTcsNumericParamModInstance`）+ 每条目 `NumericParamModInstances` + **五带折叠收束到唯一 `FoldTcsAttributeBands` 调用点**（台账 `STAT-1` 全三处收束）+ `FTcsSkillEntrySelector` 四档 + `Level` 特殊键（`TcsStateParam.Level` 由 `TcsState` 原生声明）+ 激活期物化与终结**按 `RunSource` 级联摘除** + `Mode` 列分流（`EPM_Live` 不进快照数值面） | `EVID-2026-10-09-skill-param-chain` |
| 5 | **时段、查询契约、四路终结、链重定向** | `FTcsPhaseSpan` 时段表按**战斗时钟到期堆**逐段推进 + `TcsEvent.Cast.PhaseChanged`；`ICastStateQuery` 三档（`CQM_DefSwitches` / `CQM_PhaseTable` / `CQM_Custom`）；`CancelCast` **先过查询契约**；**四路终结汇聚同一例程**（全时段走完 / **瞬发当刻完成** / 打断 / 顶替，只在"发哪一枚终止事件"处分岔）；`FTcsChainRedirect` 挂摘 + 优先级 + 后挂者胜 | `EVID-2026-10-10-skill-cast-phases` |
| 6 | **端到端竖切** | 内容资产驱动的全链验收 + **`CHAIN-7` 完整三路（完成/打断/顶替）读数**（属性现值 + 账本条目数**成对**，含**同一次施法内的阴性对照**）+ 跨帧时轴纪律落成宿主私有骨架 | `EVID-2026-10-11-skill-layer-e2e` |
| 7 | **宿主脚本插槽** | `ITcsTriggerConditionEvaluator` / `ITcsTriggerPayloadReader` 两张注册表（`UINTERFACE(MinimalAPI, Blueprintable)`）——`R-2` 后段闭环 | `EVID-2026-10-06-trigger-host-slots` |

**门禁六道的可拒性实测**：**只 4 道可拒**（①②④⑥），③（冷却）与⑤（CanAfford）本轮落"**具名拒绝 + 空实现占位**"（空轨道恒可放 / 不消耗档恒可付）⇒ 序列结构完整可测，非默认分支归 `R6.5`。

> **★ 2026-10-12 R6.5-1 落地注记（原文逐字保留）**：上句的**时态已到期**——`R6.5-1` Task 1/2 已把
> **③（冷却）与⑤（CanAfford）两道转真**，门禁**六道全部可拒**（实测：门禁③ 起冷后返回
> `SAR_OnCooldown`、门禁⑤ 付不起返回 `SAR_CannotAfford`）。上句保留作 R6 收束时的如实记录。

### 11.2 本轮**不落**（带归属，MUST NOT 读成"未定"）

> **★ 2026-10-12 R6.5-1 落地注记（原文逐行保留；下表四行已由 `R6.5-1` 兑现 ⇒ 见"处置"列）**：
> `R6.5-a` / `b` / `c` / `d` **四批已落地并取证**（2026-10-12；提案三份同日归档）。
> **下表前四行的归属列因此已兑现，不再是"不落"**——保留原行以维持"当时确实是这么切的"这一事实。

| 面 | 正文条款 | 归属 | 2026-10-12 处置 |
|---|---|---|---|
| **冷却多轨道 / 三事件 / `AdjustCooldown`·`ResetCooldown`** | §2「冷却（D5-15 多轨道）」、§3.5 | **`R6.5-a` / `R6.5-b`** | ✅ **已落**（另+`StartCooldown`） |
| **`FCooldownTiming` 四档** | §2 同上 | **`R6.5-c`** | ✅ **已落**（第四档 `Custom` 删除） |
| **Cost 策略 × 时机 + `CanAfford` 真实现 + Pay 提交 + `OnCostApplied`** | §2「Cost（D5-10 v2）」、§3.6 | **`R6.5-d`** | ✅ **已落**（另+`CanAfford` 查询口） |
| **`FBoolSwitchModifier` + `GateCheck` 条件求值器** | §2 参数双表的 `BoolSwitches` 运行期通道 | **`R6.5-e`**（形状已落，消费者未落） | ❌ 未落（不变） |
| **`UTcsSkillModDef` 技能侧参数行分派** | §2「修正器引用行（D5-19）」的技能行那半 | **`R6.5-f`**（`ParamChainRows` 本轮**只落内联形态**） | ❌ 未落（不变） |
| **`CQM_Custom` 查询档 / `MCS_Custom` 起链档** | §3.3 三级实现的第三级 / §2 主链四档 | **`R6.5-g`**（本轮**具名未实现 + 回退默认档**，MUST 记 `Warning`） | ❌ 未落（依赖 `R5.5-e` 仍未满足） |
| **关系字段检查器（`Blocks` / `Requires` / `Priority` / `Cancels`）** | §2「关系字段」的**语义**（形状已落） | **`R5.5-e`**（技能与状态共用同一检查器） | ❌ 未落（不变） |
| **多形态组合范式的完整走查** | §2「多形态/多段技能 = 组合范式」 | **`R6.5-g`**（本轮只落可运行基座） | ❌ 未落（不变） |
| **`FTcsParamView` 视图策略族 + 技能面板** | §2「描述文本绑定（D5-17 v3）」的视图那半 | **`R8`**（`state-def-asset` 已钉死"本轮 MUST NOT 建视图策略族"） | ❌ 未落（不变） |
| **内联触发行的框架登记与生命周期** | §2「内联触发行」 | **字段已交付、本轮零框架登记**（与状态侧的差异见该条；消费者 = 后续强输入引导） | ❌ 未落（不变） |
| **`AttributeScaled` 参数源** | §3.4 参数链的四源之一 | **全仓无实现者**（要求宿主提供 `ITcsAttributeProvider`，今天为零）⇒ 配了只落兜底值；归**触发条件** | ❌ 未落（不变） |
| **Cost 减免通道** | §3.6 的 `Amount` | **触发条件型**（台账 `COST-1`） | ✅ **已落（2026-10-12）**——但**形状与原记不同**：原记"加系数键 `CostPct`"，**用户改点 B 案 = `Amount` 换 `FTcsParamValue` + 契约键 `CostAmount` 走带式折叠**（判据：纯系数表达不了**固定值减免**）。见 `parametrize-skill-numeric-fields` |
| **`FTcsCooldownTrack::Duration` 的 `Param` 形态** | §3.5 的 `Duration(字面量\|Param)` | **触发条件型**（台账 `COOLDOWN-1`） | ✅ **已落（2026-10-12）**——`FTcsParamValue`；**★ 注意：§3.5 正文早在 2026-09-11 就写着这个形态，代码滞后至此才补上**（见 §3.5 注记的"文档先行、代码滞后"） |
| **多资源消耗（成本组）** | §3.6 的 `v1 单资源` | **触发条件型**（台账 `COST-2`，2026-10-12 新登记） | ❌ 未落——DNF 式"蓝+血+道具"；届时形状 = 成本组，且 MUST 一并重审原子性 |
| **消耗的"检查 / 提交"扩展点（`ECP_CustomFragment` 真实现）** | §2「Cost（D5-10 v2）」的"检查（CanAfford）与提交（Pay）**成对走策略**"、§3.6 的 `ICostPolicy` | **触发条件型**（台账 `COST-3`，2026-10-12 新登记；**原把该档阻塞记为 `SCRIPT-9`，2026-10-12 改正**） | ❌ 未落——今天 `CanAffordCost` / `CommitCost` 写死在框架内；**正确形态 = 宿主插槽家族**（非 `D5-10` 原写的 `ICostPolicy` `USTRUCT` 基类——那条对脚本侧不可达）。**与 `COST-2` 的关系 = 可分离**：`COST-2` 的"道具"项依赖本条，本条不依赖 `COST-2` |

### 11.3 实现名对照（正文概念名 → 落地实现名，MUST 按右侧引用）

| 正文概念名 | 落地实现名 |
|---|---|
| `FStateDefBase` / `FSkillDef` | **`FTcsStateDefBase`**（基类）/ **`FTcsSkillDefData`**（数据）；资产 = `UTcsStateDef` / **`UTcsSkillDef`** |
| `FLearnedSkillEntry` | **`FTcsLearnedSkillEntry`** |
| `FCastRun` | **`FTcsCastRun`** |
| `ICastStateQuery` | **`ITcsCastStateQuery`**（契约）+ **`UTcsCastStateQuery`**（U 类） |
| `FPhaseSpan` | **`FTcsPhaseSpan`** |
| `NumericParameters` / `BoolSwitches` | **`Params`**（`FTcsNumericParamRow`）/ **`BoolSwitches`**（`FTcsBoolSwitchRow`） |
| `FCooldownTrack` / `FCooldownPolicy` / `FCooldownTiming` | **`FTcsCooldownTrack`** / **`FTcsCooldownPolicy`** / **`ETcsCooldownTiming`**（2026-10-12 `R6.5-1` 落地；运行态 = `FTcsCooldownTrackState` / `FTcsCooldownGroupState` / `FTcsCooldownOps`） |
| `ECostPolicy` / `FCostConfig` | **`ETcsCostPolicy`** / **`FTcsCostConfig`**；**`Timing` 的枚举名定为 `ETcsCostTiming`**（正文原本未定名）——**MUST NOT 再引用 `ECostPolicy` 那个旧写法**（2026-10-12 `R6.5-1` 落地；运行态 = `FTcsCostOps` / `UTcsCostEventRelay`） |
| `ITcsCooldownTimingFragment` | ❌ **整体删除、MUST NOT 补回**（自定义时机 = `ECT_Manual` 档 + 宿主自调 `StartCooldown`，**不需要载体**；判据见 `PLN-R6.5-1` §0.2 裁决 ①） |
| `OnCooldownStarted` / `OnCooldownUpdated` / `OnCooldownEnded` / `OnCostApplied`（事件名） | **`TcsEvent.Cast.CooldownStarted`** / **`.CooldownUpdated`** / **`.CooldownEnded`** / **`.CostApplied`**——**落地名去掉 `On` 前缀**（`TcsEvent.Cast` 根下既有四枚零 `On` 前缀，按同族对称；MUST NOT 照抄 `On*`） |
| payload 的 `SkillId` 字段 | **`DefTag`**（`EffectiveDefId` 已整体移除，"用哪份定义"的唯一答案就是它；**MUST NOT 为 `SkillId` 发明类型**） |
| `bSingleInstance` | **`ECastInstancing`** + **`bRetriggerOnActive`** |
| `ParamSlots` / `ModifierSlots` | **`NumericParamModInstances`** / **`AttrModInstances`**（2026-10-09 改名：容器字段名 = 元素类型名复数；`Slot` 保留给"可寻址可复用带代际的空位"） |
| `FSkillRedirect` / `EffectiveDefId` | **已整体移除**（D5-9 修订 2026-09-11；重定向走链级 `FTcsChainRedirect`） |
| `FFormGroup` / `FCastSequenceState` | **已整体移除**（D5-14，2026-09-11） |
| `FCastAttrCapture` / `ETcsCastAttrCaptureFrom` | **已整体删除**（2026-10-08；判据见正文该条，**MUST NOT 补回**） |
| `FTcsCastAttrCapture` 等 | 同上 |
| `MaterializeModifiers` | **`MaterializeParamChainRows`** |
| `ECostPolicy` / `FCooldownTrack` / `ITcsCooldownTimingFragment` | **前两者已落地**（2026-10-12 `R6.5-1`）⇒ 见上方新增的三行对照（**`ETcsCostPolicy` / `FTcsCooldownTrack`**）；**`ITcsCooldownTimingFragment` 整体删除、MUST NOT 补回**。原判"均未落地（`R6.5` 批次；正文提及处只作形状意图）"**已到期**，保留作 R6 收束时的如实记录 |

### 11.4 §0.4 四条错前提的更正回写（正文 §2 的对应处已就地注明）

| # | 错前提 | 实况 | 落点 |
|---|---|---|---|
| ① | 正文 §2 层级图写 `FStateDefBase` / `FSkillDef`，并称基类含 **`FragmentSet`** | 实际数据基类 = **`FTcsStateDefBase`**；**基类无 `FragmentSet`**（行为 Fragment 是 `FTcsBuffDef.Fragments` 的**派生字段**）；`LevelBase` / `MaxLevel` 名称一致但语义已收窄（等级→数值表归参数源） | 本节 §11.3 + 正文 §2 已加实施注记 |
| ② | "`WAIT-7` AttrCapture 等 M5 技能账本轮" | **过期归属**，且**技能侧那半已整体删除**（2026-10-08 用户裁定）⇒ 两层机制的"独立性"表述**现在只剩一层**（伤害流程侧独存） | `WAIT-7` 归属措辞已改（`LEDGER-deferred`）；**技能侧 MUST NOT 补回** |
| ③ | "`CORE-1` 若全域订阅需求成立" | **条件不成立（已核）**——事件订阅方是触发行（逐条具名）+ 宿主脚本（动态委托），**全库无任何路径需要订阅 `TcsEvent.Cast` 父 tag** ⇒ 保持"待触发"，`TcsEventBus` 的**精确匹配语义本轮不动** | `LEDGER-deferred` `CORE-1` 行已注明本轮复核结论 |
| ④ | "Def 资产撞名时 **MUST** 加 `Asset` 后缀"（被当成硬规则） | **实为历史产物**：git 考古显示三族给出**两种相反答案**（属性族资产先占裸名 ⇒ **数据**让位加 `Data`；触发族/Buff 族 ⇒ **资产**让位加 `Asset`）⇒ 真正判据 = **谁的名字已有文档在位** | `openspec/project.md` 的限定语已改为**二选一判据**（Task 1 Step 7 交付） |

### 11.5 本轮三条口径裁决（正文未覆盖，实施期定的）

1. **顶替语义三条**（2026-10-07）：① 旧 run 发 `OnCastInterrupted`、**MUST NOT** 发 `OnCastCompleted`；② 顶替**受 `IsInterruptibleNow()` 门禁**（不可打断段 ⇒ 顶替被拒）；③ 顶替 = 打断的**一种终止形态**，复用同一事件，**MUST NOT** 新增第三枚终止事件。**与 GAS 的两处刻意分歧**（MUST NOT 照抄）见正文 §2。
2. **瞬发 = 空时段表 = 激活当刻完成**（2026-10-09 裁定甲′）⇒ 终结路径是**四路**而非三路。**为什么非补不可**：空 `Phases` 意味着**没有任何时段到期** ⇒ "全时段走完"对瞬发**永不成立**，缺第四路时瞬发只能靠**打断**结束（把一次成功施法报成"被打断"，且池槽位与账本 `RunHandles` 永久占用）。
3. **`BoolSwitches` 落 `TcsSkill`**（2026-10-06 Q-8）：`FTcsBoolSwitchRow` 与持有它的 Def **同域**；依据 = 四路 grep 实测其消费者**只有技能侧**（`SPEC-02-states` 全文零命中）；`ETcsParamMode` 仍从 `TcsState` 复用（零新编译边）。

### 11.6 本轮对正文的两处就地订正（只此两处）

1. **§2 层级图**：`FStateDefBase` / `FSkillDef` 及"`FragmentSet`"三项描述**已过期** ⇒ 就地加实施注记（指向 §11.3 / §11.4①）。
2. **§2 `AttrCapture` 条目**：技能侧**已整体删除** ⇒ **原文保留以记现场** + 就地注明删除判据与"`WAIT-7` 不被关闭"的边界。

### 11.7 边界（如实，MUST NOT 读成"全绿"）

- **门禁只 4 道可拒**（③⑤ 为占位）；**`CQM_Custom` / `MCS_Custom` 具名未实现**；
- **`AttributeScaled` 参数源全仓零实现者** ⇒ 配了只落兜底值（本轮实测可用的是**字面量 / 参数引用 / 等级表**三条）；
- **`ResolveSkillDef` 的登记边界**：技能定义**只进缓存、不参与世界装配**（与状态/修正器模板同款）；
- **关系字段只有形状、零检查器**（`R5.5-e`）；
- **内联触发行零框架登记**（字段可读、无生命周期）；
- **冷却与 Cost 的空实现是真占位**：`FCooldownTrack` 空 Tracks = 恒可放、`FCostConfig.Policy = None` = 恒可付；
- **单机 / 单世界**：网络姿态只落"接口位不实现"（§6）；
- **验收装置住宿主仓**（LAC `Source/TcsDev/`），属**宿主验收装置**；跨帧等待骨架亦**宿主私有**、**未提升为插件契约**。
