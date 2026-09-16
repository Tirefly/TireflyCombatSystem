# M4（效果执行）决策点 v2 —— **已拍板 2026-09-02**（v1 经用户反馈修正：Scope/HandlerClass 砍除、Cues 改引用、EntrySelector 纠正）

- 日期：2026-09-02
- 状态：**决策点提案 v2**——M4a 事件与触发 / M4b 原语执行 / M4c 查询与投射
- 证据核验：2026-09-02 已对照 TCS 报告 09/01/ZZ。**核心事实：TCS 无触发行、无效果链、无前摇后摇**——触发寄生在 StateTree Task 的 EnterState（09:160），事件是组件多播无总线（ZZ:61），`SendStateTreeEvent` 是零调用死 API。M4a/M4b 是**净新增设计**，无继承包袱也无先例背书。

## M4a 决策点

### D4-1 触发行 12 字段（提案 v1 正式拍板）
事件→效果链映射的行数据（表行/Def 内联均可）。逐字段：

| 字段 | 一句话职责 |
|---|---|
| `EventTag` | 订阅哪个事件（Tag 路由，裁决 2a） |
| `EventPayloadFilter` | 载荷预筛（廉价的字段匹配，先于条件求值） |
| `Conditions` | 门禁条件（不求值即不触发；支持 `bConditionMissIsSilent`） |
| `Effects` | 触发后执行的 FEffectChain 引用/内联链 |
| `Priority` | 同 Tag 多行触发顺序 |
| `ExecutionGate` | 执行闸（如"仅权威侧""仅服务器"——网络姿态挂点） |
| `InterruptPriority` | 可打断哪些正在跑的链 |
| `GateTags` | 行级开关 Tag（运行时点灯控制整行） |
| `Cues` | **CueId 引用列表**（帧末通道）——引用而非配置：Niagara/贴花/MPC 参数映射住 M7 宿主适配；复杂参数走 SetVar 传递 |
| `bConditionMissIsSilent` | 条件未过时是否静默（false = 记录一条 Explain 线索） |

- **v2 修正（用户反馈 2026-09-02）**：①`Scope` 砍除——目标归属每效果步骤 `FTargetSelector`（一个技能多效果、各有目标：对己/对友/对敌），行级 Scope 冗余且冲突；②`HandlerClass` 砍除——触发器求值器本身就是全部行的共享 Handler（裁决 2a），定制钩子走行级 Custom Fragment 逃生口；③`Cues` 改为 CueId 引用而非效果配置。**12 → 10 字段。**
- 订阅生命周期：触发行注册于定义加载期，运行句柄与订阅配对（D0-2 机制复用）；来源注销自动退订。
- 理由：12 字段各自独立可空，空 = 默认行为；没有"策略类"。

### D4-2 事件分类默认表（通道由事件类型定，不给策划选）
| 事件 | 通道 | 理由 |
|---|---|---|
| 状态生命周期 5 事件 | 立即 | 同帧后续逻辑、关系表重评需要新值 |
| 属性变更（flush 时） | 立即 | 读侧依赖新值；UI 走帧末镜像 |
| 技能激活结果 | 立即 | 门禁链依赖结果 |
| Cue/表现类 | 帧末 | 帧末合并防广播风暴；预览走 PeekPending |
| 周期信号（泵类） | 帧末 | 无读依赖 |
| 统计/遥测 | 帧末 | 非关键 |

规则一句话：**"读侧依赖新值"→立即；"只是被告知"→帧末。** 默认表 v1；具体事件名随 M5/M6 词汇定稿补全。触发器行不提供通道选择（防误用）。

- **拍板后补充：Authoring 三层模型（用户问询定案）**——①原语集=插件解释器+类型库（C++，非配置）；②效果链+触发行=定义侧配置数据（DT 行/Def 内联双轨，Buff 链经触发行订阅 M3 生命周期挂接、Skill 链住 Skill Def）；③开发者代码三层：零代码（策划表行组合）/ Custom 逃逸位 Fragment（项目侧新类型）/ 新 FEffectStep 原语类型（刻意能力扩展）。**不存在脚本手动调用层**——类比 StateTree：原语=内置 Task 库，链=树，触发行=挂树机制；程序员提供词汇、策划用词汇写作。

## M4b 决策点

### D4-3 FEffectStep 原语集（终 15 原语；17+3 提案经三次修订收敛）
- **15 原语（终）**：控制流 6（WaitDelay/WaitEvent/Branch/Parallel/Repeat/RunSubChain）+ 战斗 6（SelectTargets/Damage/Heal/ApplyState/ModifyAttribute/ModifyFlow）+ 表现与元 3（PlayCue/SetVar/OnError）。修订史：WaitUntil→**WaitEvent**、Gate 并入开闸事件、**ModifyFlow 新增**（提交流程属性修正，见 09 文档）、**SpawnProjectile/SpawnArea 移除**（D4-16）。
- **步骤归属（D4-14 注册制分派）**：TcsEffect 只住本模块步骤 9（控制流 6 + ModifyAttribute/SetVar/OnError——ModifyAttribute 的域模块 TcsAttribute 在 Effect 下层，无法反向承载）；领域步骤 6 由领域模块定义 step struct + 执行器并自注册：Damage/Heal/ModifyFlow→TcsDamage、SelectTargets→TcsTargeting、ApplyState→TcsState、PlayCue→TcsCue。**15 是系统词汇数，不是 TcsEffect 模块私产。**
- 三个明确不做原语：Schedule（→参数）、DamageInShape（→Damage+SelectTargets 参数组合）、Timeline（否决）。
- 形态：每个原语 = 一个 FInstancedStruct 派生数据 struct；**原语集合的扩展 = 新增 step struct 类型（允许的 C++ 扩展点），Custom 逃逸位规约不适用于原语集合本身**（它管策略枚举，不管类型族）。
- 补充说明（用户问询）：①链是通用数据，Skill 与 Buff 共用同一原语集（仅触发上下文不同）；②Schedule 想表达「时刻 T 派发」，已被 `WaitDelay + RunSubChain` 组合覆盖，降级为等待步骤参数；③Timeline 否决理由：编辑器重资产越界、核心需求被 WaitDelay 序列+SetVar/ModifyAttribute 覆盖、演出级时间轴属表现层职责（LevelSequence/Niagara）；若未来需要「随时间插值属性」（蓄力类），以曲线参数形式回归 ModifyAttribute，而非 Timeline 原语。
- 解释器：逐步执行；即时步骤同步；等待步骤注册 M0 时钟泵；控制流调度子链；上下文（目标集/数值/变量）随链传递；Repeat 带**熔断**（单链单帧步数上限，AbilityKit 教训）。
- **异步语义定案（用户确认）**：RunSubChain 默认等待（bWait=false 才放支线）——子链完成唤醒父链；Parallel 默认不汇合（bJoin=true 才等全部）——JoinCount 计数统一唤醒机制；四种唤醒源（到期堆/事件匹配/子链完成/Gate 开闸事件）走统一挂起-恢复协议（FChainRun 池化保存 PC/上下文，代际校验防悬空）。
- 与 M3 边界（03 文档 §4）：链不进 State Def；触发行订阅 M3 生命周期事件来挂接。

## M4c 决策点

### D4-4 目标选择数据化
- `FTargetSelector` 数据 struct，**挂载在每个效果步骤上**（用户观点采纳：一个技能可能触发多个效果，对己/对友/对敌各有目标）。**纠正（用户指出）**：TCS 的 EntrySelector 不是目标选择器——它选择 SkillModifier 作用到哪些已学技能条目（DNF 式「某技能等级+1」），属 M5 域；新系统对应 `FEntrySelector` 数据 struct（All/ByTag/ById → 数据模式 + Custom 逃逸位，下轮拍板）。本节：
  - `Mode`：Self / Instigator / EventTarget / TagQuery / RadiusArea{Shape,Radius} / Custom(值 1 逃逸位)
  - **挂载位置**：SelectTargets 步骤显式持有；Damage/Heal/ApplyState 等战斗步骤内嵌一个
  - `Filter`：存活/阵营/Tag 排除组合（数据）
  - `Custom` → 决策 Fragment + Payload（规约；**形态 v3 已改：FInstancedStruct/TInstancedStruct 策略，见 D3-7 v3 与 10 文档 v3**）
- ~~SpawnProjectile/SpawnArea~~（D4-16 移除）：形状弹道参数化 struct 的思路随原语一并移出——未来 TcsProjectile/TcsArea 模块自带原语与形状注册；指示器渲染归宿主/LAC（D4-15）。

## M5 预告（下一轮正式拍板）

核验发现 **TCS 无前摇/后摇机制**（09 全文无任何记载；最接近物是 Def.StateTreeRef + ManualOnly TickPolicy 的"瞬发语义"，09:133,384）。M5 需要净新增：施法阶段数据（WindUp/Active/Recover + 各阶段可打断标记），由 M0 时钟泵驱动。双账本概念强支持继承（`RuntimeEntriesById` 主表 + 索引投影，09:247-269），冷却继承"默认无冷却"约定但改用到期堆（弃 0.1s Tick 递减，09:337）。

## 拍板方式

逐项回复或整批"按推荐"；每项完整取舍见本文档，证据行号已内联。

## D7 增补（2026-09-02，TcsDamage 独立模块轮，全部已拍板）
| D7-1 | TcsDamage 独立模块（九模块）：瞬时流程骨架（伤害/治疗同骨架）+ 每阶段事件实时收集修改器 + FDamageRecord 回放/统计流；公式零提供 | 已拍板 |
| D7-2 | Damage/Heal 原语语义 = 流程发起器（构造 FDamageFlowContext + 跑骨架）；公式 = 项目 IDamageFlowDelegate；Custom 原语路径仍可完全自定义 | 已拍板 |
| D7-3 | 流程属性（FormulaAttr）复用 M2 语义（键+修正链+封闭运算，作用域=流程）；瞬时角色属性修改 = 临时修正器 + 流程 Source（替代 CopyOnWrite）；v1 只做当前值通道 | 已拍板 |
| D7-4 | 消耗型修正器：{MaxUses/Cooldown/OnConsumed/SortKey} + Policy{Standard/CustomFragment(IConsumablePolicy)}；收集≠消费、成功执行才消费；多候选按 SortKey 裁决选一 | 已拍板 |
| D7-5 | **流程管线宿主化（M9 追问轮，用户否定预设阶段表——"不同项目有不同项目的说法"）**：流程=数据模板 FCombatFlowTemplate（有序步骤数组，FInstancedStruct 同 FEffectStep 形状）；TcsDamage 降为机制层（解释器+步骤注册表+自注册宏+黑板+收集协议+消耗裁决+记录流）；标准十阶段降级为内建步骤库+官方默认模板（宿主可整表替换）；通用数据步骤 `FlowModify`（数据化黑板写入）/`FlowDelegate`（数据化委托调用）+ 每步骤 Conditions（复用 D4-5）——**编辑器拼流程零 C++**；C++ 只入口新语义步骤/新公式；多预设=多模板（选择链：步骤配置→Def→全局默认） | 已拍板 2026-09-02 |
| D7-6 | **伤害修改器 = 触发行 + ModifyFlow（唯一通道）**：无独立修改器载体类型（D3-5 推论：状态行为=原语链）；文章"有状态/无状态修改器" = 带/不带条件的触发行（订阅随 Source 生命周期）；消耗策略挂提交（D7-4）；聚合走黑板键修正链（M2）；M8 修改器模板糖 | 已拍板 2026-09-02 |
| D7-7 | **流程模板重定向 FFlowRedirect**（用户拍板一步到位，否决缓增补）：状态/装备声明换流程模板，让渡点+重定向栈后挂/高优先+Source 级联回收——三粒度让渡模式升**四粒度**（参数→链→技能→流程） | 已拍板 2026-09-02 |

| D4-14 | **注册制分派 + 依赖层级反转（EffectStep 存废轮终定）**：领域步骤类型+执行器归领域模块（FStepDamage→TcsDamage、FStepApplyState→TcsState 等），TcsEffect 降为纯机制层（解释器+控制流+执行器注册表）；依赖链反转为 `Core←Attribute←Effect←{Damage,Targeting,State}←Skill`；反向依赖用注入接口击穿（IChainRedirectResolver/ICombatEntityQuery）；静态自注册宏 UE_DEFINE_EFFECT_STEP_EXECUTOR（重编译成本归属"定义新 C++ 类型"本身） | 已拍板 2026-09-02 |
| D4-15 | **TcsTargeting 模块成立**：FTargetSelector 数据化 + FTargetingShape 形状单一来源 + 实体查询注入（ICombatEntityQuery）；指示器渲染归宿主/LAC。**v2（2026-09-02 审阅轮 5 后，用户三拍板）**：①Selector/Filter **策略模式**取代枚举（策略基类抽象 + 内嵌配置；**载体 2026-09-10 经 D3-7 v3 修订为 FInstancedStruct/TInstancedStruct，见 10 文档 v3**；默认实现 Self/EventTarget；Filter 语义宿主实现——存活/敌对是宿主本体论）；②**D4-4 改口：战斗步骤不内嵌 selector**——目标消费 Context.Targets（Context 默认目标=事件目标，单步链零 SelectTargets 直接消费），TcsDamage 编译集 {Core,Attribute,Effect} 保持、TcsDamage→TcsTargeting 边消失；③**RadiusArea/FTargetingShape 后置**（竖切剧本单体选择无消费者实证）。规格见 10-module-targeting.md v2 | 已拍板 2026-09-02 |
| D4-16 | **SpawnProjectile/SpawnArea 原语移除**（17→15）：有的项目不需要投射物/区域实现；未来 TcsProjectile/TcsArea 自带原语注册——不需要的项目不引入 | 已拍板 2026-09-02 |

| D4-17 | **语言无关执行器终定**：插件不内嵌脚本引擎（宿主选择 AS/C#/TS）；注册双入口（C++ 静态宏 + 反射动态委托）；EStepResult{Completed,Running} 步内挂起协议扩展；蓝图理论可行不承诺；**R3 纯 C++ 无脚本集成** | 已拍板 2026-09-02 |
