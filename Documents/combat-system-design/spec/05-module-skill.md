# [SPEC-04-skill](../spec/05-module-skill.md) — TcsSkill 技能/施法层设计（v2 定稿重写）

- **文档 ID**：`SPEC-04-skill`
- **类型**：SPEC / 模块规格
- **状态**：PENDING
- **权威范围**：TcsSkill（M5）账本/施法/冷却/Cost/参数链；理由住 LOG-03-skill 与 DEC-02-fold-display
- **最后更新**：2026-09-23

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

**FSkillDef 字段**（全部为施法语义；**无堆叠、无时值**——D3-10）：
- **参数双表 + Mode 列 + ValueConvention**（D5-5/D5-12 v2/D5-18）：`NumericParameters{Key, Base: FTcsParamValue, Mode: Snapshot(默认)|Live, ValueConvention}` 与 `BoolSwitches{Key, Base, Mode}`（**实现名 `FTcsNumericParamRow` / `FTcsBoolSwitchRow`，2026-09-14 命名批；**数值参数行住 `TcsState`、由 `FSkillDef` 继承白拿；布尔开关行住 `TcsSkill`（2026-10-06 R6 Task 1 Q-8 改判——`BoolSwitches` 的消费者只有技能侧，`SPEC-02-states` 全文零命中；`ETcsParamMode` 仍从 `TcsState` 复用）**）——Base 载体 PV 系列 2026-09-11 换型 TInstancedStruct<FTcsParamValueSource>——StateLevel/InstigatorLevel/AttributeScaled 等源落点（**TargetLevel 系暂不提供**——PV-4 评判轮）——Live 实时走 Entry 账本求值（仅 Skill 侧有意义）；ValueConvention（TcsNotation 层，D5-18）：Percent/OneMinus/Negate EnumFlags，**写入点转规范值**（账本内永远规范值 0.85/0.75/-1000），UI 反变换显示（记法层规格详见 [SPEC-07-notation](../spec/11-module-notation.md)）；
- **~~AttrCapture 声明列表（D5-12 v2）~~：`CastConfig{AttrKey, From: Instigator/Target}`——activate 时捕获进 `FCastRun.Context.CapturedAttrs`（属性读默认 Live、捕获命中读快照）；伤害流程侧另有步骤级 AttrCaptureList（09 文档，作用域=流程）——两层机制各自独立。**★ 2026-10-08 技能侧整体删除（用户裁定，原文保留以记现场）**：三条判据同时成立 ⇒ **零消费者**（技能侧 `CapturedAttrs` 全仓无任何类型读取）/ **机制重叠**（设计给捕获写的用途"改 `CapturedAttrs`、随流程消失、零账本污染"已被**流程属性黑板** `FTcsFlowAttributes`（作用域 = 流程用完即弃）+ `FlowModify` 数据步骤覆盖 ⇒ 同一个"流程局部可变值空间"存在两份）/ **技能侧已被参数快照占满**（技能自己的参数由 `FCastRun.ParamSnapshot` 在激活瞬间冻结，流程内工作值由黑板承担；剩下空间**说不出技能侧独有的业务场景**，设计语料也未给出该用例）。**删除面** = `FTcsCastAttrCapture` / `ETcsCastAttrCaptureFrom` 类型 + `FSkillDef.AttrCaptureList` 字段 + `FCastRun.CapturedAttrs`。**边界（MUST 记录）**：**只涉技能侧**——伤害流程侧的 `FTcsDamageFlowContext::CapturedAttrs` 归台账 `WAIT-7`，触发条件**不因此改变**，本删除 MUST NOT 被读成"关闭了 `WAIT-7`"。落点 = 提案 `add-cast-run-and-gates`；**MUST NOT 补回**。
- **描述文本绑定（D5-17 v3——视图策略化，2026-09-15）**：`SkillDef.Descriptions: TArray<FTcsDescriptionEntry{DescriptionId, TextKey, Views[]}>`（**取代单字段 DescriptionTextKey**——多描述入口：Tip 简述 / Codex 图鉴 / 升级预览各自独立，支撑"简单描述=当前档单值、复杂描述=全表+高亮"多对多）；**视图 = 结构化策略配置**：`Views: TArray<FTcsDescriptionViewSlot{SlotName, TInstancedStruct<FTcsParamView>}>`——内置 `Value`（单值）/`Series`（整表+当前档高亮）/`Range`（区间）/`Attribute`（属性现值），宿主 C++ 自定义视图一步接入（零公共代码）；**StringTable 原文只含槽名 `{Rate}`（零语法）**——机器语义全部进结构化配置，编辑器可校验（`IsCompatible` 探针 + 槽名交叉扫描，见 11/08）；组装器按 Entry 自身 Def 的账本**现值**取值（EffectiveDefId 已整体移除——D5-9/D5-14 重构 2026-09-11），RichTextBlock 渲染归宿主；技能面板索引口径 = 当前 `EffectiveLevel`（实例侧 = 快照 Level，见 03 §3.6）；
- **施法时段表**（可选）：`TArray<FPhaseSpan{Duration(FTcsParamValue——原"字面量|Param"，PV 系列 2026-09-11 换型), bInterruptible, bCanMove, Tag}>`——任意段数；"前摇/后摇"= 项目命名惯例；瞬发 = 空表；
- **冷却（D5-15 多轨道）**：`FCooldownPolicy{Tracks: TArray<FCooldownTrack{Duration(FTcsParamValue——同上换型), GroupTag?}>, CustomFragment}` × `FCooldownTiming{OnCastStarted(默认)/OnCastCompleted/OnCastInterrupted/Custom(ICooldownTimingFragment)}`——无 GroupTag=自身轨道，有=写单位级组槽；可施放=全部自身轨道+所属组槽清空；真 GCD=自身轨道+GCD 组轨道并存（纯数据）；
- **Cost（D5-10 v2）**：`FCostConfig{Policy: None(默认)/ResourceAttr(内建 M2 扣减)/CustomFragment(ICostPolicy), Timing{OnCastStarted(默认)/OnCastCompleted/OnEvent/OnCastInterrupted/Custom(ICostTimingFragment)}, ResourceAttr + CostParam}`——检查（CanAfford）与提交（Pay）成对走策略；**道具/弹药/意灵晶 = 宿主 CustomFragment（插件不提供实现）**；打断返还未设引擎字段（Timing 选择或 OnCastInterrupted 响应表达）；v1 单资源；
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
- 施法事件（核心词汇 FStruct）：`OnCastStarted / OnCastPhaseChanged / OnCastCompleted / OnCastInterrupted`。

### 3.4 参数修正与选择器（D5-5/D5-6/D5-9）
- **参数双表与修正器两结构**：`FTcsNumericParamModifier{ParamKey, Op: Add/PercentAdd/Mul/FlatAdd/Override（**D5-5 v3：与 M2 同一套五带**；原 `AddPct` 命名与 M2 `PercentAdd` 失步已修；Custom op 砍除 2026-09-02 不变——计算在上游传入终值）, Operand: FTcsParamValue（PV 系列 2026-09-11 换型——Literal/ParamRef/AttributeScaled 等源；**复合运算即参数链**：{Add, AttributeScaled(AttackPower)} + {Mul, ParamRef(DamageRate)} 两行折叠 = "攻击力×倍率"，PV-7）, Source, SortKey, CompeteGroup(GameplayTag, 可选), ValueConvention（D5-18 v3：链行 Operand 补约定列）}` 与 `FBoolSwitchModifier{SwitchKey, Value, Source}`（D5-5 v2 命名统一，原 FLogicGateModifier）；**参数链 = 带式聚合**（**D5-5 v3：与 M2 完全同式、顺序无关**——`Override` 存在取组内**最大值**直接作为结果（FlatAdd 一并被覆盖），否则 `((初值 + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；带权同 M2 `Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30`，**SortKey 退化为带权、不再承担排序语义**；**折叠初值 = 该键参数行的求值结果，无参数行则 0**；**折叠器单份住 TcsAttribute**，M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用——02 §2.2a"同一形状、作用域容器不同"的复用落点）；Source 注销级联撤销；**特殊键 `Level`**（EffectiveLevel 修正，DNF 式 +1 = Add+1）；**CompeteGroup（D5-19）**：读侧竞争组——折叠前按组分桶，组内解析值最大者进折叠（账本全量、Source 撤销自动递补，无休眠池）；"取优先级最高"=**Override 组取最大值**（与 M2 一致；D5-5 v3 取代原 `Override+SortKey` 口径），值竞争与优先级竞争可共存（先选优后**按带**折叠）；同组跨 Op 校验器提示。
- `FEntrySelector{Mode: All/ByTag/ById/Custom, Params}`——外部来源选择作用于哪些已学条目。
- **Def 自带参数修正行（PV-9，2026-09-11 采纳）**：`SkillDef.ParamChainRows`——声明作用域恒为本条目自身的 `FTcsNumericParamModifier` 行（可内联或引用 UTcsSkillModDef 模板），**无 FEntrySelector**（该选择器保持专属外部施加场景）；激活期物化进本条目参数链，Source=施法运行句柄，结算/打断级联摘除（与 ModifierRows 同生命周期语义）。**复合参数 = 链行带式折叠**（"攻击力×倍率 + y" = {Add, AttributeScaled(AttackPower)} + {Mul, ParamRef(DamageRate)} + {FlatAdd, ParamRef(DamageAddition)}，PV-7 + D5-5 v3 带式口径，任意书写顺序）；State/Buff 侧参数为快照单值、无账本链，暂不需要对应机制。
- **技能级替换（D5-9 修订 2026-09-11：移除 EffectiveDefId 技能级重定向）**：~~`FSkillRedirect` 重定向栈~~ 撤除——整体逻辑替换 = **直接换成新 Skill（新学习身份/Entry：Grant/Revoke，或 ReplaceSkill 链步骤原语判定树候补）**，不做同 Entry 跨 Def 漂移。动机（用户）：Entry↔Def 一对一后参数键空间绝对纯净，SkillDef 内 ParamRef 同域解析零歧义（PV-2.d 收束）；旧方案"保留身份、跨 Def 漂移"会让 Entry 上按旧 Def 键空间声明的参数修正器全部悬空。**让渡模式由三粒度收窄**：参数级（NumericSkillModifier/BoolSwitchModifier）→ 链级（链重定向栈，保留）→ 技能级 = 新 Skill。等级/冷却进度/形态进度不跨替换继承（需要则宿主显式迁移）。
- **多形态路由（D5-14 移除后的组合范式，2026-09-11）**：无内建形态解析——各形态是独立 Entry；"当前形态" = 阶段标识 StateInstance（Tag 门禁）；**ParamRef 恒解析于 Entry 自身 `DefTag` 参数表**（PV-2.d 终版钉死——无条件）。

### 3.5 冷却（D5-15/D5-16）
- 多轨道模型（D5-15）：Tracks 数组（自身轨道/共享组轨道），Timing 保留 policy 级；CDR = 参数链 `CooldownPct` 键，**快照于冷却触发时刻**（中途变化不追溯），作用于各轨 Duration；到期堆驱动；真值在 Entry（每轨 Remaining/Total 可查，UI 轮询）；默认无冷却（空 Tracks）。
- **冷却事件词汇（D5-16）**：`OnCooldownStarted`（Timing 触发点）/ `OnCooldownUpdated`（运行中被 AdjustCooldown/ResetCooldown 改动）/ `OnCooldownEnded`（到期堆触发，权威侧）——总线 FStruct，payload `{EntryHandle, SkillId, Track, GroupTag?, Total, Remaining, EndTimeSeconds}`；UI 径向照旧轮询，事件供 AI/连招/音效触发式响应。
- 引擎不内置职业自动归类等游戏专属概念；公共冷却 = 共享组轨道的数据表达（D5-15）。

### 3.6 Cost（D5-10）
- 策略×时机（见 §2）；门禁第 5 道 = CanAfford，提交按 Timing（激活事务内或事件点）；ResourceAttr 策略走 M2；v1 单资源；道具/弹药 = 宿主 Fragment；返还未设引擎字段；**OnCostApplied 事件（D5-16）**：ResourceAttr 策略 Pay 提交后发，payload `{EntryHandle, SkillId, ResourceAttr, Amount}`——CanAfford 失败不发（TryActivate 返回值覆盖），CustomFragment 成本自行决定是否发同词汇事件。

## 4. 入口服务

- `GrantSkill / RevokeSkill(unit, DefTag, Source)`
- `TryActivate(unit, SkillId, Context) -> ESkillActivateResult`：**显式门禁序列**（实体 Ready → 已学 → 冷却 → Instancing 顶替/并存判定 → CanAfford → Def 校验；每道门具名原因——修 TCS 门禁内联缺陷）
- `CancelCast(run, Reason)`；`ApplyParamModifiers(unit, FEntrySelector, TArrayView<FParamModifier>)`；`MaterializeModifiers(unit, TemplateIds, ParamContext, Source)`（D5-19 宿主命令式入口——与声明式 ModifierRows 共用同一物化器）；`AdjustCooldown / ResetCooldown(unit, EntryHandle, Track, Delta|Clear)`（D5-16 事件源——改动轨状态发 OnCooldownUpdated）；`GetNumericParam / IsSwitchSet / GetLevel`

## 5. 关键机制

- **门禁序列**：六道具名门（修 TCS 内联无钩子缺陷，TCS 报告 09，库外，已不在库内）。
- **时段驱动**：进入时段 → 到期堆注册（Duration 从 ParamSnapshot 取）→ `OnCastPhaseChanged`；`IsInterruptibleNow` 按三级实现解析；时段参数在快照构建时已定。
- **打断**：来源优先级 vs `IsInterruptibleNow()` → CancelCast(Cancelled) → `OnCastInterrupted`；止于未来不追溯（M4 约定）。
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

## 10. 验收钩子

M5 专项人工检查路径：门禁六态逐态触发；冷却策略/时机切换 + CDR 快照；参数链 Source 级联（装备卸下→修正消失）；**参数链带式折叠验一项：同一组链行打乱书写顺序，结果不变（Mul 写在 Add 之前仍是"先加后乘"）**；链重定向挂/摘 + 多形态组合范式（阶段标识 StateInstance + 门禁路由——"同样触发逻辑释放 B"）；ParamSnapshot 冻结验证（施法中途挂修正器不影响本次伤害）；Param() 时段时长被修正器改变（下一次激活生效）；冷却三事件触发序（Started/Updated/Ended）+ 多轨道并存与组槽联动（自身 CD+GCD 组轨）+ OnCostApplied 发送时机。
