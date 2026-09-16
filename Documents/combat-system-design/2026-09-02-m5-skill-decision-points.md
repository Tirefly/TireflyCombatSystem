# M5（技能/施法）决策点 v3 —— 全部拍板定稿

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

| D5-12 v2 | **双维度快照终定**（Skill 侧）：①参数表 Mode 列——Snapshot(默认)/Live（Live 实时走 Entry 账本求值）；②**AttrCapture 声明列表进 CastConfig**{AttrKey, From: Instigator/Target}——activate 时捕获进 FCastRun.Context.CapturedAttrs；属性读默认 Live、捕获命中读快照；"本次攻击+10%"双路径（捕获→改 Context / 未捕获→FlowSource 临时修正器） | 已拍板 2026-09-02 |

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
