# M5（技能/施法）决策点 v3 —— 全部拍板定稿

- **文档 ID**：`LOG-03-skill`
- **类型**：LOG / 决策日志
- **状态**：FROZEN
- **权威范围**：D5-1~D5-19、PV-10 与 D4-5 条件的定义处
- **最后更新**：2026-09-22

- 日期：2026-09-02
- 状态：**已拍板**（三轮问答；用户三次实质修正：施法阶段降级为查询契约、冷却策略×时机矩阵、参数任意键；OnInputReleased/OnStateEnded/vector 均被砍除）
- 证据核验：2026-09-02 对照 TCS 09/01/ZZ。用户补充关键设计意图：TCS 不做前后摇是有意的灵活性；职业公共冷却过时但机制可留。

## 定案总表

| 项 | 决定 |
|---|---|
| D4-5 | 条件最小集 = HasAllTags / AttributeCompare / VariableCompare / GateCheck(读 BoolSwitches) / Chance + Custom 逃逸位（值 1） |
| D5-1 | **施法阶段 = 查询契约三实现**：引擎只承诺 `IsInterruptibleNow()/CanMoveNow()` 查询；答案来源 = Def 简单开关（默认）/ 时段表（可选标准件：任意段数 `FPhaseSpan{起止, bInterruptible, bCanMove, Tag}`，"前摇/后摇"=项目命名惯例，瞬发=空表）/ Custom Fragment。**时段 Duration 支持 Param() 引用**——RPG 养成增减前后摇 = 参数修正链（PercentAdd——D5-5 v3 命名对齐），时段进入时快照 |
| D5-2 | 账本全 struct 化：`FLearnedSkillEntry`（中央注册表桶内）+ `FCastRun`（池化）；索引先只主表；弃 UObject Entry + 8 转发 override 壳税 |
| D5-3 | **冷却 = 策略 × 触发时机双枚举**：策略 `{None(默认) / Standard(固定时长) / SharedGroup(共享组 Tag) / CustomFragment}`；时机 `{OnCastStarted(默认) / OnCastCompleted / OnCastInterrupted / CustomFragment}`（OnInputReleased、OnStateEnded 砍除——前者项目输入定义不可猜，后者场景由 CustomFragment 覆盖）；CDR 走参数链 `CooldownPct`，**快照于触发时刻**（中途变化不追溯）；到期堆驱动；默认无冷却；**v2：多冷却轨道泛化，见 D5-15** |
| D5-4 | `bRefundCostOnInterrupt` 砍除——返还是政策不是引擎语义；`OnCastInterrupted` 事件照发，返还由响应链/Fragment 声明。其余继承：止于未来不追溯、单实例顶替 = bSingleInstance、取消三值对齐 M3 |
| D5-5 | **参数修正 = 任意 FName 键 + 每键修正链**（词汇归项目，与 D2-1 同构；TCS 参数链形态减策略类）；**封闭的是运算**：`Add / PercentAdd / Mul / FlatAdd / Override`（**D5-5 v3 修订 2026-09-14：与 M2 同一套五带、顺序无关带式聚合——见文末 v3 行**；原 `AddPct` 命名失步已修；**Custom op 砍除 2026-09-02**——计算在上游传入终值，一致性随 D2-7）；**值类型拆双表：NumericParameters（double 全运算）+ BoolSwitches（bool 仅 Override）**（2026-09-02 用户提案拍板；"Gate" 命名避让 M4 原语/GateTags/ExecutionGate；修改器命名失步后统一，见 D5-5 v2），vector 砍除（外源改向量场景几乎不存在，真出现走 Fragment 或拆分量）；Source 注销级联撤销；**特殊键 `Level`**：Entry 等级修正（EffectiveLevel=clamp(0,Base+Σ)，DNF 式+1=Add+1；等级→数值表归项目） |
| D5-6 | `FEntrySelector{Mode: All/ByTag/ById/Custom}` 数据化 |
| D5-7 | **起链解析 = 让渡点三实现**：全局定义（默认）→ 数据重定向行 `FChainRedirect{TargetChainId, ReplacementChainId}`（策划零代码，标准件保留）→ Custom Fragment（逻辑）；重定向声明在状态/装备 Def，Source 注销级联失效；**绝不修改共享 Def** |
| D5-8 | **链参数注入**：链步骤/时段数值字段可引用 `Param()`；解析走 D5-5 修正账本（Def 基值 + 修正链）；**链启动时快照**，步骤可显式 opt-in 实时解析。与 D5-7 分工：参数改数值、重定向换结构 |
| D5-9 | **技能级重定向**（2026-09-02 用户提案采纳）：`FSkillRedirect{TargetSkillId, ReplacementSkillId}` 声明在状态/装备 Def；Entry 的**有效 DefId** 解析走让渡点（重定向栈后挂/高优先→原 Def）；Source 级联回收=暂时性天然成立；冷却**键在 Entry**、策略/时长解析自生效 Def；等级/已学状态留 Entry；UI 图标解析生效 Def。**三粒度统一模式**：参数改数值（D5-5）→链重定向换流程（D5-7）→技能重定向换定义（D5-9），同一"让渡点+重定向栈+Source 级联回收"形状 |

## 关键机制串联（3 段斩→5 段斩的双解）

- 数值解：链 `Repeat{Times: Param("SlashCount")}`，基值 3，升级状态修正器 `SlashCount Add +2`（D5-8+D5-5）。
- 结构解：升级状态声明 `FChainRedirect{slash_combo → slash_combo_5}`（D5-7）。
- 整体解（D5-9）：形态/觉醒 = 技能级重定向——Entry 有效 DefId 换成形态技能 Def，参数/时段/冷却策略/链全套随生效 Def；与数值解/结构解构成三粒度统一模式。
- 两者都带 Source 级联回收，都是"State 影响另一个 State 执行流"的合法通道，且不触碰 Const Def。

## 非目标继承

无脚本调用层；施法段本体论不进引擎（三段/四段/无段全是数据）；引擎不内置职业/公共冷却概念。

## 依据

拍板：2026-09-02 三轮问答框（含用户修正：查询契约降级、策略×时机矩阵、OnInputReleased/OnStateEnded 砍除、vector 砍除、固定通道撤回）。证据：TCS 09/01/ZZ 核验（无前摇后摇系有意设计、双账本实证、参数链 09:262-263、冷却 0.1s Tick 递减被到期堆替代）。

| D5-10 | **Cost 策略×时机终定**：Policy{None/ResourceAttr/CustomFragment}×Timing{OnCastStarted/OnCastCompleted/OnEvent/OnCastInterrupted/Custom}；检查+提交成对；道具/弹药=宿主 Fragment；返还未设引擎字段 | 已拍板 2026-09-02 |
| D5-11 | **ECastInstancing 终定**：InstancePerExecution/InstancePerEntity（替换 bSingleInstance，Static 不做）；Level 双侧携带（Entry 持久+FCastRun 快照） | 已拍板 2026-09-02 |

| D5-12 | **StateParamSnapshot 终定**（Skill 侧）：FCastRun.ParamSnapshot——activate 时一次性解析全部生效参数（EffectiveLevel/Cost 实值/各时段 Duration/链参数），链与时段读快照；持续修正器 live 通道不受影响；opt-in 实时为唯一例外；收益=预演/可预测/回放输入 | 已拍板 2026-09-02 |

| D5-12 v2 | **双维度快照终定**（Skill 侧）：①参数表 Mode 列——Snapshot(默认)/Live（Live 实时走 Entry 账本求值）；②**~~AttrCapture 声明列表进 CastConfig~~**{AttrKey, From: Instigator/Target}——~~activate 时捕获进 FCastRun.Context.CapturedAttrs；属性读默认 Live、捕获命中读快照~~；"本次攻击+10%"双路径（~~捕获→改 Context~~ / 未捕获→FlowSource 临时修正器）。**★ 2026-10-08 第 ② 维整体删除（用户裁定，原文保留以记现场）**：该维判定为**零消费者 + 机制重叠 + 技能侧已被参数快照占满**（三条判据见 `SPEC-04-skill` §2 与 `PLN-R6` Task 3 Step 5）；**第 ① 维（参数表 Mode 列）不受影响、继续有效**。**边界**：伤害流程侧的 `CapturedAttrs` 归台账 `WAIT-7`，**不因此改变**。落点 = 提案 `add-cast-run-and-gates` | 已拍板 2026-09-02；**2026-10-08 第 ② 维删除** |

| D5-13 | **实例-定义引用规范终定**：Entry 权威=DefId（+EffectiveDefId）；GetDef() 解析缓存；**FCastRun 执行期零回查 Def**（ParamSnapshot 冻结一切） | 已拍板 2026-09-02 |

| D5-14 | **技能形态组终定**（用户亚托克斯 Q/鸣潮案例）：FFormGroup{Forms 各形态独立 Def/AdvancePolicy{OnCastCompleted,OnEvent,Custom}/ResetPolicy{Timeout,Interrupted,OtherCast,Custom}}；EffectiveDefId 解析链 = 重定向→形态→主 Def；形态状态 FCastSequenceState 存 Entry；学习身份单份；轮转=条件式特例 | 已拍板 2026-09-02 |

| D5-15 | **冷却多轨道泛化（D5-3 v2）**：`FCooldownPolicy{Tracks: TArray<FCooldownTrack{Duration(字面量\|Param), GroupTag?}>, CustomFragment}`——无 GroupTag=自身轨道，有=写单位级组槽（GroupTag 键）；可施放=全部自身轨道清空+所属组槽清空；原 Strategy{None/Standard/SharedGroup/CustomFragment} 溶解进轨道（None=空表/Standard=单条自身轨道/SharedGroup=带组轨道；自身+组并存合法→**真 GCD=纯数据**：自身 CD 轨+GCD 组轨并存）；Timing 枚举保留 policy 级；CDR `CooldownPct` 快照于触发时刻作用于各轨；Entry 冷却状态=轨道状态数组；原句"引擎不内置职业/公共冷却概念"改写为"不内置职业自动归类等游戏专属概念；公共冷却=共享组轨道的数据表达" | 已拍板 2026-09-02 |

| D5-16 | **冷却/消耗事件词汇**（撤回"冷却结束默认不发广播"——总线订阅制无噪音税）：总线 FStruct——`OnCooldownStarted`（Timing 触发点）/`OnCooldownUpdated`（运行中被 AdjustCooldown/ResetCooldown 改动）/`OnCooldownEnded`（到期堆触发，权威侧）；`OnCostApplied`（ResourceAttr 策略 Pay 提交后发，payload `{EntryHandle, SkillId, ResourceAttr, Amount}`；CanAfford 失败不发——TryActivate 返回值覆盖；CustomFragment 成本自行复用词汇）；冷却事件 payload `{EntryHandle, SkillId, Track, GroupTag?, Total, Remaining, EndTimeSeconds}`；UI 径向照旧轮询 Remaining/Total，事件供 AI/连招/音效触发式响应；**配套 API：AdjustCooldown/ResetCooldown（按轨道）**——事件与可变性成对存在 | 已拍板 2026-09-02 |

| D5-5 v2 | **BoolSwitch 命名统一**（用户抓出失步）：`FLogicGateModifier`→`FBoolSwitchModifier{SwitchKey, Value, Source}`；服务 `IsGateSet`→`IsSwitchSet`——D5-5"Gate 命名避让"当初只执行了一半（表名已避让、修改器结构名未跟），无特殊考虑；M4 条件原语 `GateCheck`（读 BoolSwitches）保留不动 | 已拍板 2026-09-02 |

| D5-17 | **描述文本与数值绑定**（M9 收尾轮）：SkillDef.`DescriptionTextKey`（StringTable 引用）+ 命名空间占位符 `{Attr:X}`/`{Param:X}`（FText::Format 原生语法，本地化参数重排白拿——弃自创 `\…\` 标记与自研解析器）；组装器按参数元数据**包富文本样式**（翻译者不碰标签，RichTextBlock 渲染归宿主 UI）；M8 占位符扫描（占位符 ∈ 参数键集/属性词表，拼错/孤儿加载期报错）；取值 = EffectiveDefId 解析后的 Entry 账本**现值**（含修正链——与逻辑同源不同时机：逻辑读 ParamSnapshot，UI 读账本） | 已拍板 2026-09-02 |
| D5-18 | **ValueConvention 值约定**（用户提案，GAS 扩展经验）：参数行 `ValueConvention` EnumFlags{None/Percent/OneMinus/Negate}，固定组合顺序 Percent→OneMinus→Negate；**写入点转规范值**（Base 注册/修正器 Apply——账本内永远规范值 0.85/0.75/-1000，链逻辑/ParamSnapshot/Explain 全消费规范值），UI 反变换显示（0.75→"25"+%样式）；Percent 与显示格式正交并存（排版小数位另列）。**升格独立模块 TcsNotation**（R0 §9 第十一模块，**与 TcsCore 平级互不依赖**——仅引擎基础；各域←Notation：所有玩家可见/开发者编辑的数值类配置通用——SkillDef/SkillModifierDef/宿主装备；装备配置的策划语言转换属宿主配置域，账本不做二次猜测） | 已拍板 2026-09-02 |

| D5-19 | **技能侧修正器模板 + CompeteGroup 终定**（R3 计划审阅轮 4）：FSkillDef 同形状 ModifierRows（D3-19）；**激活期挂载**（用户拍板）——激活时物化、Source=施法运行句柄、结算/打断级联摘除；物化上下文=FCastRun.ParamSnapshot（D5-12）；`ApplyParamModifiers` API 保留命令式入口；**优先级语义对号**（取证 TCS 调研 01/04/07）：旧 AttrModDef.Priority 管线排序不迁移（顺序无关五带已消灭）、SkillModDef.Priority→SortKey 承接、Exclusive（休眠池+候补复活）不迁移——**"取优先级最高"=Override 组取最大值**（与 M2 一致；**D5-5 v3 修订 2026-09-14 取代原 `Override+SortKey` 口径**）；**CompeteGroup 新原语**（用户定 GameplayTag 非 FName）：`FTcsNumericParamModifier`（原概念名 FNumericParamModifier）可选字段，空=不竞争；**读侧选优**——折叠前按组分桶、组内解析值最大者进折叠（账本全量保存、Source 撤销后下次折叠自动递补——无休眠池，递补语义保留而机制面只剩一字段+一次分桶）；同组跨 Op 校验器提示；**值竞争与优先级竞争可共存**（先分组选优后**按带**折叠）；不做取值 Fragment 策略（用户判定）；参数 clamp（叠加上限）留判定树候补——值域问题非选优问题 | 已拍板 2026-09-02 |

| D5-18 v2 | **ValueConvention/描述绑定 面补全**（R3 计划审阅轮 5，用户问"BuffDef 如何配置"）：**BuffDef 需要**——FStateDefBase 参数行 {Key, Base, ValueConvention}（镜像 SkillDef 行形状；快照构建写入点转换）+ DescriptionTextKey（buff tooltip 刚需；字段本身 FName 不需边，组装器在 Notation/宿主）；**UTcsAttrModDef 补约定列**（模板默认 Literal 策划语言书写、物化时 ConvertToCanonical——D5-18"注册/物化边界转换"模式延伸）；**依赖边补全**：TcsState←Notation、TcsAttribute←Notation（D5-18 原文"各域←Notation：所有玩家可见/开发者编辑的数值类配置通用"既录口径的**补全非翻转**——当时仅 SkillDef/SkillModDef 存在）；纯机制层（Effect/Damage/Targeting）不加边——运行时消费规范值、载体 FTcsParamScalar 归 Core；**"账本不做二次猜测"不变式不变**（转换责任在配置域/物化边界，账本与聚合运行时零约定逻辑） | 已拍板 2026-09-02 |

| D5-5 v3 | **参数链 = 带式聚合**（2026-09-14，用户拍板）：与 M2 属性聚合**完全同式、顺序无关**——五带 `Add/PercentAdd/Mul/FlatAdd/Override`；`Override` 存在取组内最大值（FlatAdd 一并被覆盖）；否则 `((初值 + ΣAdd) × (1 + ΣPercentAdd)) × ΠMul + ΣFlatAdd`；**SortKey 退化为带权、不再承担顺序语义**；**折叠初值 = 该键参数行的求值结果（无参数行则 0）**；CompeteGroup 选优后**按带**折叠；**折叠器单份住 TcsAttribute**，M2 属性聚合 / M5 参数链 / TcsDamage 流程属性三处共用。动机：原"有序链式"下 Mul 行写在 Add 行之前会让 `(0+120)×0.1` 变 `(0×0.1)+120`，策划无从察觉 | 已拍板 2026-09-14 |
| D5-17 v2 | **描述绑定词法族 + 绑定链**（2026-09-14）：词法族 `{Param:X}`（单值）/ `{ParamSeries:X}`（整表 + 当前档高亮，需 PV-10 可枚举能力）/ `{ParamRange:X}`（区间——在 LevelBase/MaxLevel 两点各求一次值，**零新能力**，曲线源也可用）/ `{Attr:X}`；**绑定靠键名 + 内容模块适配器 + 源虚分派——文本层不认识源类型**；取值回调四方法契约（`GetValue/GetSeries/GetRange/GetAttribute`，纯数据面）住 Notation；索引口径双处（技能面板=Entry 当前 EffectiveLevel / 实例 tooltip=该实例快照 Level）；当前格取账本现值、非当前格取表内基准值；**展示政策归开发者——TCS 只提供选项**（**token 载体次日被 v3 取代，见下**） | 已拍板 2026-09-14 |
| D5-17 v3 | **描述视图策略化**（2026-09-15，**取代 v2 的文本内嵌词法**——用户重设计）：①StringTable 回归**纯文案**（只含槽名，零语法——token 无法编辑器预警、视图无法携带自身参数、源与展示形态不可能一一匹配）；②**视图 = `FTcsParamView` USTRUCT 策略基类（`BuildText`/`IsCompatible` 虚函数）+ `TInstancedStruct` 持有**（D3-7 v3 同构——Kind+FInstancedStruct 与策略形态同物，取后者）；③**配置空间 = `Def.Descriptions: TArray<FTcsDescriptionEntry{DescriptionId, TextKey, Views[SlotName, View]}>`**（取代 DescriptionTextKey 单字段——多描述入口支撑"简单描述/复杂描述"多对多）；④内置 Value/Series/Range/Attribute（v2 词法转世，语义不变）；⑤数据面 = `FTcsViewBuildContext`（瞬时非反射：GetValue/TryGetSeries/EvaluateAtLevel/TryGetConvention——索引口径在适配器装配）+ `FTcsViewProbe`（纯数据能力探针）；⑥**编辑器预警四层**：源能力徽章 → IsCompatible 保存期拦截（错误挂配置元素+替换建议）→ 槽名交叉扫描（缺=错误/孤儿=警告）→ 运行期只降级；⑦**宿主自定义视图 = 一步、零公共代码**（扫描器永不改）；v2 的 GetView(Kind,Out) 回调与 FTcsDescriptionSource/SeriesView/RangeView 作废；保留：索引口径双处、当前格账本现值、约定白名单（D5-18 v3）、降级不变式、展示政策归开发者 | 已拍板 2026-09-15 |
| D5-18 v3 | **约定可配面**（2026-09-14）：约定列 = 行级"本行书写口径"；**可配白名单**——Literal/表型源可配，`ParamRef`（二次转换陷阱）/`AttributeScaled`（乘积语义歧义）禁配，M8 强制；**链步骤数值字段/修正行 Operand 补约定列**（PV-6 换型面扩大）；`Literal.Value` 定性为**配置态书写值**（无约定时即规范值——修正头文件注释措辞） | 已拍板 2026-09-14 |
| PV-10 | **源的可枚举能力**（2026-09-14）：`FTcsParamEnumerableSource : FTcsParamValueSource`（Core 可选能力基类）+ `Enumerate(Level, OutValues, OutCurrentIndex)` / `GetIndexForLevel`；**索引解析唯一真相在源、`Evaluate` 同源**；不可枚举源配序列展示 = M8 报错（载体 09-15 随 D5-17 v3 改 Series 视图 + IsCompatible 探针）；曲线源区间零能力可用、序列表待需求；随 TcsState 等级源同批落地 | 已拍板 2026-09-14 |

| **身份载体修订（2026-09-22）** | **本文档所有 `DefId` / "任意 FName 键" 应读作 tag 口径**：`FLearnedSkillEntry.DefId` → `DefTag: FGameplayTag`；D5-5 的"任意 FName 键 + 每键修正链" → **任意 `FGameplayTag` 键**；`ParamRef{Key: FName}` → `{Key: FGameplayTag}`；D5-9/D5-13/D5-14 里的 `EffectiveDefId` / `DefId` 一律读作身份 tag。原 `FName` 载体被**有据重开**（提案 `switch-identifiers-to-gameplay-tags`；理由见 m0min-m2 文档 D2-1 修订行）。**注意**：D5-17 v3 描述配置里的 `TextKey` / `DescriptionId` / `SlotName` **不在此列**（StringTable 键与展示槽名，非内容引用——口径见 11 文档）。 | 已拍板 2026-09-22 |

---

## ★ 附注：`D5-7` 链重定向的**不可替代内核**与它的**坏招牌**（2026-10-10 追加）

> **为什么有这一段**：2026-10-10 的讨论中出现一条会反复出现的疑问——**"『3 段斩→5 段斩』这种带职业判定与数值判定的需求，用 `BoolSwitch` + 现有功能也能做，`ChainRedirect` 是不是多余？"**
> 该疑问**当时的直觉是对的**，但它停在"没有可复核的事实清单"上。本段把**判据钉死**，
> **下次同一疑问出现时直接读本段，不必重新论证**（用户 2026-10-10 裁定：`D5-7` 与示例**先原样保留**，本段只作依据留档）。

### 一、该直觉成立在哪：`D5-7` 的招牌示例是**弱的**

本文档《关键机制串联（3 段斩→5 段斩的双解）》一节把 `D5-7` 的示例定为"段数 3 → 5"。
但按 `D5-8` 自己钉的分工——**"参数改数值、重定向换结构"**——**"段数"本身就是一个数值**；
而"剑魂职业 + 技能等级 > 5"是**谓词**。谓词 + 数值的天然归属是**条件**与**参数链**，不是映射表。

⇒ **该示例是 `D5-7` 的坏招牌，MUST NOT 被当作 `D5-7` 的代表用例**；`D5-7` 的正当内核见下节。

### 二、`D5-7` 不可替代的那**一条**能力

**让「外部的声明方」（状态 / 装备 / 天赋）在不触碰技能链资产的前提下换掉"这个技能走哪条链"，
并自带多来源仲裁与按来源成对摘除。**

对照"在链里放一个 `FTcsStepBranch`"：

| 维度 | 链内 `FTcsStepBranch` | `D5-7` 链重定向栈 |
|---|---|---|
| 谁做决定 | **链的作者**（须预先想到这个变化） | **外部声明方**（不改技能资产） |
| 多来源竞争 | 无（一条链一个分支） | `Priority` 大者胜、打平取**后挂者**（`FTcsChainRedirectRegistry::Resolve`） |
| 生命周期 | 随链 | **随来源**（`UnmountBySource` 级联摘除） |
| 前提 | 一个能表达该判定的**条件类型** | 一套**声明载体**（今天**没有**，见 §四） |

⇒ 它换的是「**条件表达力**」↔「**声明管线**」：**判定输入是谓词时它不划算**（§一）；
**要"多来源、免改资产、卸下即失效"时它是唯一形状**。

### 三、事实清单（全部可复核；格式按 `CONVENTION` §5 用 `路径:符号名`）

1. **今天链里连"职业判定"都做不了。** 链侧条件上下文的来源受限——`ExecuteStepBranch`
   （`TcsEffect/Private/Chain/TcsStepBranch.cpp`）只填 `Caster` 与 `World`；`TcsEffect/Public/Chain/TcsStepBranch.h`
   自述：`EventTag` / `ClassificationTags` **在链侧无来源** ⇒ **依赖它们的条件恒不通过**。
   「剑魂」是身份标签 ⇒ 要 `HasAllTags` ⇒ **在链里恒不通过**；而"技能等级 > 5"**没有任何条件类型能读**。
2. **内置条件只有三个**（`UE_DEFINE_TRIGGER_CONDITION_EVALUATOR`，`TcsTriggerCondition.cpp`）：
   `FTcsTriggerCondition_HasAllTags` / `_Chance` / `_AttributeCompare`。**没有**读 BoolSwitch 的 `GateCheck`、
   **没有**读 `Entry.Level` 的条件。
3. **注册的效果步骤只有十个**（`UE_DEFINE_EFFECT_STEP_EXECUTOR`，全仓）：`SelectTargets` / `ApplyState` /
   `Branch` / `ModifyAttribute` / `RunSubChain` / `SetVar` / `WaitDelay` / `WaitEvent` / `ModifyFlow` / `Damage`。
   **没有 `Repeat`** ⇒ §一的"数值解"今天同样走不通（`Repeat` 归 `R5.5-a`，见 [`LEDGER-deferred`](../ledger/deferred-inputs-ledger.md)）。
4. **被提议替代的 `BoolSwitch` 两半都不存在**：写侧 `FTcsBoolSwitchModifier`、读侧 `GateCheck` 条件——
   全仓 `Source/` **零命中**（唯一条提及是 `TcsNumericParamModifier.h` 的一句注释）。两者同归
   [`PLN-R6`](../plans/plan-r6-skill-layer.md) 的 `R6.5-e` 批次。
5. **重定向今天没有任何定义侧载体。** `FTcsChainRedirectEntry` 全仓只出现在三处：结构体定义、
   栈内记录 `FRecord`、门面形参 `UTcsSkillSubsystem::MountChainRedirect`。
   **没有任何 Def 持有它的字段**（`FTcsBuffDef` 与 `FTcsSkillDefData` 均无）⇒ `D5-7` 原词
   "**声明在状态/装备 Def**"那半**完全未落地**；`TcsChainRedirect.h` 自述"技能侧本轮零资产载体"，
   今天唯一挂载入口是 **C++ 形参**。
6. **已落地的部分**：`FTcsChainRedirectRegistry`（`Mount` / `UnmountBySource` / `Resolve` / `Num` / `Reset`）
   + 门面四个口 + 三级解析 `FTcsCastOps::ResolveCastChainId`，**已实现并已取证**——
   `EVID-2026-10-10-skill-cast-phases` 的 `P5` 五段读数（基线 / 挂上 / 高优先 / 后挂 / 摘除），
   挂摘成对 **3 → 0**。

### 四、今天**用不了**的三处（MUST NOT 读成"已可用"）

1. `EMainChainStart::MCS_Custom` 主链启动档——**具名未实现**：配了它报 `Warning` 且**不起链**（归 `R6.5-g`）。
2. `ITcsChainRedirectResolver`（Custom 重定向契约）——**"可声明、不可求值"**：注入实现后解析
   **不调用它**，只报 `Warning` 后取数据栈档（归 `R6.5-g`）。
3. **无资产载体**（§三·5）⇒ 今天只能 C++ 挂。

### 五、判别先例：同族的 `D5-9` 已被撤除

**技能级**重定向 `D5-9` 已于 **2026-09-11 撤除**，理由正是"**用另一条路更好**"
（`Entry↔Def` 一对一保证参数键空间纯净）。⇒ **"某条重定向该不该收窄"在本仓是有先例的问题类型**；
`D5-7` 若将来被裁，先例应引 `D5-9`（见 [`LOG-DECISIONS`](decisions-log.md) 与
[`SPEC-04-skill`](../spec/05-module-skill.md) §3.4）。

### 六、本段的**边界**（MUST NOT 外推）

- **不改任何决策**：`D5-7` 及其示例原样保留（用户 2026-10-10 裁定"先留着"）。
- **不含裁定结论**：本段只把「事实清单 + 不可替代内核 + 坏招牌」钉住，**为将来的裁定提供可复核依据**。
- **台账未登记条目**（用户裁定不登记）⇒ 本段是这条待裁项的**唯一载体**。

---

## ★ R6 轮（M5 技能层）实施期口径与收束（2026-10-11 追加；按 `docs-convention` §7「冻结」只追加、正文不改）

> **本节的地位**：`LOG-03-skill` 正文是 **D5-1~D5-19 的拍板定义处**（FROZEN）。本节只登记**实施期新定的口径**与**本轮收束结论**，**不改任何既有决策**。实施视角的完整收窄（落地 / 不落面带归属 / 实现名对照）住 **`SPEC-04-skill` §11**，本节只记**决策面**的增补。

### 一、实施期新定的三条口径（正文未覆盖）

1. **顶替语义三条**（2026-10-07，R6 Task 3 实施期定）：① 旧 run 发 `OnCastInterrupted`、**MUST NOT** 发 `OnCastCompleted`；② 顶替**受 `IsInterruptibleNow()` 门禁**（不可打断段 ⇒ 顶替被拒、返回 `SAR_AlreadyActive`）；③ 顶替 = **打断的一种终止形态**、复用同一枚事件，**MUST NOT** 新增第三枚终止事件。**与 GAS 的两处刻意分歧**：GAS 的 `bRetriggerInstancedAbility` 不查可打断性、且以 `EndAbility(bWasCancelled=false)` 结束（把"被顶替"报成"完成"）——**两处都 MUST NOT 照抄**。
2. **瞬发 = 空时段表 = 激活当刻完成**（2026-10-09 用户裁定**甲′**）⇒ 施法终结是**四路**（全时段走完 / **瞬发当刻完成** / 打断 / 顶替），非正文所暗示的三路。**为什么非补不可**：空 `Phases` 意味着**没有任何时段到期** ⇒ "全时段走完"对瞬发技能**永不成立**；缺第四路时瞬发只能靠**打断**结束（把一次成功施法报成"被打断"，且池槽位与账本 `RunHandles` 永久占用）。**纪律**：四路**汇聚同一个终结例程**，**只在"发哪一枚终止事件"处分岔**。
3. **`BoolSwitches` 落 `TcsSkill`**（2026-10-06 Q-8 改判）：`FTcsBoolSwitchRow` 与持有它的 Def **同域**；依据 = 四路 grep 实测其消费者**只有技能侧**（`SPEC-02-states` 全文零命中）。`ETcsParamMode` 仍从 `TcsState` 复用（零新编译边）。

### 二、三条口径裁决（收束轮复核，**均不改正文**）

1. **`STAT-1`（折叠器单份三处共用）✅ 已消费**（2026-10-09，R6 Task 4）：三处落点齐备——`TcsAttributePipeline.cpp`（M2，全仓首个消费者）/ `TcsFlowAttributes.cpp`（TcsDamage 流程属性黑板）/ **`TcsParamChain_Evaluate.cpp`（M5 技能参数链，本项）**。**收束自检**：`Source/` 全仓**真实调用点恰 3 处**（**MUST 数调用点，MUST NOT 数符号总命中**——注释会让计数虚高）。
2. **`CORE-1`（原生侧层级匹配）条件仍不成立**（2026-10-11 复核）：R6 的事件订阅方是**触发行**（`FTcsEffectTriggerDef.EventTag` **逐条具名**）+ 宿主脚本（动态委托，已支持部分匹配），**全库无任何代码路径需要订阅 `TcsEvent.Cast` 父 tag 收全部施法事件** ⇒ `TcsEventBus` 的**精确匹配语义本轮不动**（改它属跨模块 blast radius 且无真实消费者）。保持"待触发"。
3. **`WAIT-7`（AttrCapture）归属已改判且不因此关闭**（2026-10-08）：技能侧那半（`CastConfig.AttrCaptureList` / `FCastRun.CapturedAttrs`）**已整体删除**（三轮判据：零消费者 + 与流程属性黑板/`FlowModify` 机制重叠 + 技能侧已被参数快照占满）；**而**流程侧 `FTcsDamageFlowContext.CapturedAttrs` 仍是"字段就位、无填充者无读取者"，**触发条件不变**。**MUST NOT** 把技能侧的删除读成"`WAIT-7` 已关闭"。

### 三、R6 收束结论（2026-10-11）

- **计划 `PLN-R6` 的 Task 0~7 全部完成并取证**；**两个提案同日归档**（`add-cast-phases-and-interrupt` / `verify-skill-layer-e2e`）⇒ **`openspec/changes/` 归零**、**能力数 38 → 40**。
- **六份证据**：`EVID-2026-10-06-trigger-host-slots`（Task 0）/ `2026-10-07-skill-def-asset`（T1）/ `2026-10-08-skill-registry`（T2）/ `2026-10-08-skill-cast-runtime`（T3）/ `2026-10-09-skill-param-chain`（T4）/ `2026-10-10-skill-cast-phases`（T5）/ **`2026-10-11-skill-layer-e2e`（T6）**。
- **`D5-7`（技能级重定向）的现状**：D5-9 修订（2026-09-11）已把**技能级**重定向整体移除；**链级**重定向（`FTcsChainRedirect`）本轮落地并通过验收（含优先级与后挂者胜）。**本文 §《附注：`D5-7` 链重定向的不可替代内核与它的坏招牌》仍然是那条待裁项的唯一载体**（用户裁定不登记台账、不改 D5-7 本身）——**R6 收束 MUST NOT 被读成"那条待裁项已裁"**。
- **未落面改判 `R6.5`**（七个批次 `a`~`g` 全部未开工）：冷却多轨道与三事件 / `FCooldownTiming` 四档 / Cost 策略×时机 / `FBoolSwitchModifier` + `GateCheck` / `UTcsSkillModDef` 参数行分派 / 多形态组合范式完整走查。**《R6.5 批次表》的关账块**（`PLN-R6`）已补：依赖**从"等 R6"变为"随时可开"**，且 `CQM_Custom` / `MCS_Custom` 两档须**一起**补（本轮二者都是**具名未实现 + 回退默认档**）。
- **一条可复用的收束纪律（本轮沉淀，已写进 `INDEX` §5）**：**"计数声称"MUST 全库同批回改**——同一个数常在 **3~5 处**重复，且**没有校验器报警**。R6 收束实测两例：`gameplay-tag-governance` 的 `11 个根` 在 `SkillDef` 落地后**漏改三处**（由来表 / 度量 / scenario）、且"两个 `Def` 根"实为**三个**；`GLOSSARY` 的四个编号族计数各差 2~33。

---

### 四、R6.5-1 落地结论（2026-10-12）

> **本档 FROZEN，只追加**（`docs-convention` §7）。本节由 R6.5-1 执行轮追加。

- **`PLN-R6.5-1` 的 Task 0~5 全部完成并取证**（类型层 / 冷却多轨 / Cost 策略×时机 / 终结接缝 /
  四枚事件 / 宿主装置）；**Task 6 收束**进行中。
- **三个提案同日归档**（`add-cooldown-and-cost-types` / `add-cooldown-multitrack` / `add-skill-cost`）
  ⇒ 能力数 **40 → 42**（新增 `skill-cooldown` / `skill-cost`；另对 `skill-def-asset` / `skill-registry` /
  `attribute-pipeline` 各有增补）、`openspec validate --all --strict` = **42 passed / 0 failed**、
  `changes/` 归零。**归档时零 scenario 丢失**（两条 MODIFIED 需求各 +1 场景，其余原样）。
- **双配置编译** 0 error / 0 warning。**取证（双路线）**：无头 `Tcs.Test.CooldownCost.Run` **立即面 18/0 +
  含跨帧尾段 21/0**（两轮逐字相同）、`.Reject` **7/0**；**编辑器 PIE（2026-10-12 补取）同为 18/0 + 21/0 + 7/0，
  两轮独立 PIE 世界逐字相同**（✅ **缺口已闭合**：同日全量重建写回基线后两路线重跑，现同为
  "基线 DLL + 新夹具"，读数逐字一致，见 `EVID-2026-10-11-r65-cooldown-cost` §1.1）；**R6 回归 `SkillLayer.Run` 24/0 且复现性摘要
  与冻存基线逐字相同**（无头与 PIE 两侧均复取）⇒ 本轮对门禁③/⑤、撤销路径、`Deinitialize` 的改动**未破坏 R6 面**。
- **★ 装置抓到一个真实产品缺陷**：`ECT_OnCastStarted` 档**扣了资源却不发 `CostApplied`**
  （`CommitCost` 是纯扣减、发事件属调用方职责，而激活路径漏了那一行）⇒ 症状 = "资源扣了、
  订阅者收不到任何通知"，正是 `MEM-20260902-12`「事件与可变性 API 成对」点名的**静默不一致**。已修。
- **四处落地名/形状订正**（正文原文逐字保留，各就地注明）：
  ① 事件名**去掉 `On` 前缀**（`TcsEvent.Cast.CooldownStarted` 等——根下既有四枚零 `On` 前缀）；
  ② payload 的 `SkillId` 用 **`DefTag`** 取代（`EffectiveDefId` 已整体移除）；
  ③ **`ITcsCooldownTimingFragment` 整体删除、MUST NOT 补回**（自定义时机 = `ECT_Manual` + 宿主自调）；
  ④ **`ETcsCostTiming` 无 `OnCastInterrupted` 档**、**采纳 `OnEvent` 档**。
- **三处新增公开口**（`D5-16` 与 `D5-10` 的清单里都没有它们）：
  `StartCooldown`（"改"与"清"都不是"开始冷却"⇒ 无它则 `ECT_Manual` 档物理上无法兑现）/
  `PayCost`（手动提交口）/ `CanAfford`（纯查询，宿主可预灰按钮）。
- **★ 取证边界（**2026-10-12 补取完成，本注撤销**）**：编辑器 PIE 面**已取证**——
  `Run` 18/0 + 21/0、`.Reject` 7/0，**两次独立 PIE 世界逐字相同**；跨 PIE 残留检查通过；
  真 GCD / TOCTOU 在 PIE 下**均 `PASS`** ⇒ ****该三条在 PIE 下亦 `PASS` ⇒ 无头对它们的 `PASS` 不是假绿灯**（⚠ **但两条路线用的夹具版本不同，见 `EVID-2026-10-11-r65-cooldown-cost` §1.1 ⇒ MUST NOT 读成"双路线一致通过"**）**。**★ 补取证抓出两处夹具缺陷**
  （都不是插件缺陷，且**都在无头下拿到绿灯**）：① 跨帧谓词问的是**全局** `CooldownEndedCount`，
  而 CDR 的 0.5s 轨比被测的 1.0s 轨**先到期** ⇒ 等待提前 0.5s 落定 ⇒ `I2` 误报 FAIL
  （**`TcsDevCombatClockWait` 判据 2 第三次命中**：问对了"哪件事"却问错了"哪一条"）；
  ② 检查 `e` 的"未持有"前提**只在无头下成立**（无头内容资产 0 条 ⇒ 属性表为空；
  PIE 下 `ApplySliceAttributes` 会授予 `Attribute.Armor`）。两处均已修并复验，
  详见 `EVID-2026-10-11-r65-cooldown-cost` §4.4。
- **两条新台账条目**（均触发条件型、不绑轮次）：`COST-1`（Cost 侧缺折扣系数通道）、
  `COOLDOWN-1`（`Duration` 的 `Param` 形态未落）——**与既有 `STAT-11` 同一技术根因**
  （技能侧数值字段缺参数化载体），**MUST 同批裁**。
- **一条可复用的执行教训（本轮沉淀）**：**跨帧判据的谓词 MUST 直接问"被测事件本身"，
  MUST NOT 问时间派生量**。R6.5-1 的 `I3` 初版问 `GetTrackRemaining <= 0`（时间），而"到期事件已发"
  是另一件事——两者是**同一个到期堆**的条目，按 `(DueTime, Sequence)` 升序回调，等待件（+0.05）
  早于冷却件（+1.0）⇒ 无头首帧把时钟一次推到 2.79 使两者同帧到期，**谓词先真、事件后发**
  ⇒ 恒定假失败。**这与 R6 的"坑一"同类（问了一个相邻但不同的量），同一坑第二次命中**
  ⇒ `TcsDevCombatClockWait` 判据 2 的措辞需要更强的提醒（该头已写明，但仍被违反）。
