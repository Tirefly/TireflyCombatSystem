# R0 立场文档：为什么从零重建战斗系统插件（草稿 v1）

- 日期：2026-09-02
- 状态：**v1 已确认**（MD-1/2/3 于 2026-09-02 问答框拍板时随案确认；§2 怀疑清单用户未提出异议，后续如有新怀疑可增补）
- 依据：tcs-legacy-research（16 文件）、ability-kit-research、combat-system-design 既有裁决、MEM-20260819-07 / MEM-20260821-01 / MEM-20260819-09
- 用户既定意图原话："完全从零开始搭建一个新的基于 UE5.8 的高扩展性、高复用性、高内聚、低耦合、数据驱动、策划友好的战斗系统插件"

---

## 1. 决定与处置

- **决定**：新起一个战斗系统插件（名称/前缀/存放位置待拍板，工程约定：`<项目>/Plugins/Tirefly/<插件名>`，MEM-20260821-01）。
- **TCS 处置（已拍板，MD-3）**：推荐**冻结**——保留仓库作为对照参考与数据结构翻译源，不再演进新功能；两个悬空 OpenSpec 提案（`add-state-fragment-mechanism`、`add-tcs-turn-based-extension`）随之冻结，Fragment 机制思路迁入新系统 M3 重审。
- **重建的是代码与结构，不是认知**：三份调研的结论层全部继承（§3），实现层不搬运（§4）。

## 2. 为什么重建：怀疑清单化

"陷入怀疑"落到七个有证据的结构性问题上。每条给出：现象 → 为何在 TCS 内修不划算 → 新系统的回答。**若用户实际怀疑不在此清单内，须补充后重写本节再动工。**

| # | 现象（调研证据） | 为何不在 TCS 内修 | 新系统的回答 |
|---|---|---|---|
| 1 | 策略类爆炸：~40 策略类（TSubclassOf×17 / FInstancedStruct×6），大量单一实现的分支 | 逐类重构动的面比重建还大 | 策略类仅在"行为真正多态且策划需要换挡"处保留；其余用数据规则表达。判据：单一实现且无扩展消费者的策略 → 降级为函数或数据行 |
| 2 | 无中心事件总线，组件间直接调用/查找，隐性耦合 | 事件通道是骨架级改动 | M0 总线+订阅表（裁决2a）；跨域只走两扇门（总线或显式接口） |
| 3 | 无 ScaledDt：既无 TimerManager 也无缩放处理，计时语义悬空 | 同上，全局时序骨架 | M0 时钟泵（TickableWorldSubsystem，裁决2b）；一切计时走 ScaledDt |
| 4 | 组件中心（UTcsStateComponent 挂 Actor）与千人 Mass 战场冲突 | 骨架假设错了，修不动 | 核心可脱离 Actor 运行；M6 两级适配（军官组件 / 小兵 Mass fragment+processor） |
| 5 | 高风险三连：orphan-scan 子目录误删；ReadyNow 分支跳过 OnReady；ready 不等 DefAsset 预加载 | 引导时序散落各处 | M1 定义注册表显式状态机重做；三连修复列为引导模块验收项 |
| 6 | Fragment 机制悬空（提案未落地），生命周期扩展点缺失 | 提案建在旧骨架上 | M3 内建通用 Fragment 机制；三问标尺（机制原生/配置内聚/本体论不进核心）作为扩展验收尺 |
| 7 | 幽灵依赖与死代码（GameplayMessageRouter 等） | 存量包袱 | 干净起点，不迁移存量 |

**反面平衡**：重建不是否定 TCS 全部。属性管线（fixpoint 过滤 + 拓扑排序 + 单事务提交 + TG_PostUpdateWork flush + ongoing clamp）是调研确认"做对了"的部分——证明三层分拆方向可行，思路全收、代码重写。

## 3. 继承什么（结论层，直接有效）

| 结论 | 来源 | 去处 |
|---|---|---|
| 约束 → Tag 关系表 `FUnitStatusRule{Status,Blocks,Requires,Priority}` | 裁决1 | M3 |
| 事件 = 总线 + 订阅表 + 共享 Handler UClass；struct 上不自绑 delegate | 裁决2a | M0 / M4a |
| tick = TickableWorldSubsystem 泵 ScaledDt | 裁决2b | M0 |
| 定义=UObject DefAsset / 实例=USTRUCT+句柄+池 / 执行=集中 tick 三层分拆 | 载体讨论 | M1 / M3 / M0 |
| 两级单位：Mass 小兵 + 全功能军官 | 载体讨论 | M6 |
| 修饰符聚合 recalc 式：Override(0)>Add(10)>PercentAdd(15)>Mul(20)、脏标记惰性重算、1e-5 阈值 | 对照提案 | M2 |
| 属性管线思路：有界跳过-重试过滤（TCS 的"fixpoint"实为排除集重试循环，非数值迭代收敛）、拓扑排序、单事务提交、提交尾行内 flush + 帧末安全点、ongoing clamp（有界轮数） | TCS 04/05（2026-09-02 核验） | M2（重写） |
| StateTree 只做决策/编排，不做 buff/skill 执行 | unreal-state-tree 调研 | M6 |
| DataTable 双轨 authoring；Tag 宿主透明不预设分类学 | TCS 03 / MEM-20260821-01 | M1 |
| FEffectStep 17+3 原语集、触发行 12 字段 | 提案 v1 | M4（正式拍板后生效） |
| 12 条通用设计门（参数=消费面、包含非相等、handle 配对清理、ScaledDt、延迟泵变更、循环熔断等） | AbilityKit 教训 | 全模块设计门 |

## 4. 放弃什么（实现层）

1. **不搬运任何 TCS C++ 实现**——结构性问题在骨不在肉，搬运即把骨架带过来。全部重写，逐模块对照 TCS 同域实现做"它怎么做/我们为什么不同"注释。
2. TCS 仓库保留三个用途：同域对照参考、迁移期数据结构翻译源、DataTable 兼容格式参考（是否做导入器另议，默认不做）。
3. 两个 OpenSpec 提案冻结（见 §1），Fragment 思路迁 M3 重审后以新系统提案形式重新落地。
4. 不做 TCS→新系统的运行时兼容层（两系统不并存于同一项目运行时；若 LAC 过渡期需要，属项目侧适配，非插件职责）。

## 5. 非目标（第二系统效应防线）

- 不做通用 ECS、不做反射工具箱、不做序列化框架。
- 不实现网络同步；确定性预留（禁 wall-clock、聚合顺序无关）在 M0 决策点单独拍板。
- 不做 K2 节点、不做通用图编辑器框架（裁决3：可视化跟随数据形状）。
- 不做 GAS 兼容层。
- 单插件、模块数 ≤ 10（M0–M8）；AbilityKit 的 92 包是规模反面参照。（**已被 §9 修订取代**：TcsDamage/TcsTargeting/TcsNotation 增补后为十一模块）

## 6. 验收标准（整个重建的"完成"定义）

1. **数据驱动验收线**：策划新增一个 buff/技能 = 纯数据资产 + 表格行，零 C++ 改动。
2. **五条依赖铁律**可 grep 检查（依赖只向下；跨域只走总线/接口；实例不持 UCLASS 强引用；key 空间隔离；数据驱动线）——见模块地图提案 §1。
3. **竖切验收**：R3 节点一条伤害链打通（定义→属性→效果→**屏显验收信号**——测试装置直调 AddOnScreenDebugMessage；TcsCue 移出 R3，M9 收尾轮拍板），PIE 定向人工检查。
4. 每模块能用一句话说清对外词汇与服务；设计文档齐备（每模块一份 NN-module-*.md）。

## 7. 待拍板遗留（滚动清单）

| # | 事项 | 状态 |
|---|---|---|
| 1 | 模块地图 M0–M8 + 实施顺序方案 C | 本轮主拍板（提案文档 §5） |
| 2 | TCS 冻结处置 + 两提案冻结 | 随本轮拍板默认执行，可异议 |
| 3 | ~~插件名称/前缀/存放~~ **已定 2026-09-02**：显示名沿用 TireflyCombatSystem；模块前缀 Tcs（R0 §9）；存放 <项目>/Plugins/Tirefly/ | 已定 |
| 4 | 网络同步/确定性预留 | M0 决策点已拍板（D0-1/D0-4/NET 姿态落点） |
| 5 | FEffectStep 原语集、触发行（v2 定为 10 字段）、事件分类表 | M4a/M4b 轮已拍板（D4-1~D4-3；终版 15 原语见 D4-16） |

---

## 8. 分离宪法（锋利边界 + 酸测四问）

> 缘起：2026-09-02 M4 轮用户质询"链和触发放进 Def 与 TCS 的 Data-Behavior Separation 冲突"。结论：**分离不但要坚持，而且新系统是它更纯粹的实现**——边界从"资产 vs 类"挪到"词汇 vs 句子"。
>
> **两种读法**：TCS 的操作版（数据=Def 资产 / 行为=C++ 策略类）与实质版（语义=词汇 / 内容=句子）。TCS 混淆了它们——把句子放进类（策略类爆炸的根因）；而它自己每 buff 内嵌 StateTree 本就是"行为作为数据资产"。
>
> **锋利边界**：
>
> | 归属 | 侧 | 理由 |
> |---|---|---|
> | 原语语义（Damage 怎么结算）、聚合管线、事务不变式、解释器 | C++（行为） | 编译期保证、测试覆盖、性能 |
> | 链、触发行、策略轴取值、条件参数 | 数据（内容） | 天天变、纯配置、加载期校验即可 |
> | 新增语义（"重力场拉扯"） | C++（Authoring 第三层） | **无脚本层**——数据永远长不出新语义，边界不糊掉的保险栓 |
> | 封闭语义内表达不了的策略 | Custom 逃逸位 → Fragment 类（C++） | 显式、可审计的让渡门 |
>
> **酸测四问**：①随内容天天变吗→数据；②需引擎保证不变式吗→代码；③是"一个词"还是"一句话"→词汇=代码、句子=数据；④出错时期望编译期拦截还是加载期报错→语义错误必须编译期。
>
> **候选姿态**：方案一 = 解释器模式 + 锋利边界 + Custom 让渡门（**采纳**）；方案二 = TCS 原样策略类/StateTree（否决：策略类爆炸实证）；方案三 = 链内嵌脚本/表达式（排除：加载期校验/网络重放/Explain 全失效）。
>
> 防策划拼出弗兰肯斯坦逻辑的四重兜底：封闭原语集、Repeat 熔断、Explain 线索、条件无表达式。
>
> 回链：`04-module-effects.md` §6；后续各模块设计文档的数据/代码边界判定均引用本节。

## 9. 模块物化规定（2026-09-02 死规定，用户拍板）

> 编译层 UE 模块按依赖层级固化，UBT 强制依赖方向——铁律 1 从 grep 纪律升级为编译器背书。

| 模块 | 内容 | 依赖 |
|---|---|---|
| TcsCore | 句柄机制（**FTcsSourceHandle 归属来源标识归此**：系统级通用归属机制，非战斗本体论）、池、事件总线（含 BP/CS 动态监听层）、时钟/到期堆、诊断、词表/DefLibrary 机制、**数值配置载体 FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}（PV 系列 2026-09-11 取代 D2-12 FTcsParamScalar；Evaluate 虚分派，Literal/ParamRef 内置）** | 引擎基础 |
| TcsNotation | 策划记法与文本绑定层（M9 收尾轮新增，第十一模块）：**FTcsValueConvention 值约定**（Percent/OneMinus/Negate EnumFlags，固定顺序组合；写入点→规范值，UI 反变换显示）+ 描述文本绑定（StringTable 命名空间占位符 + FText::Format 组装便利 + 组装器按参数元数据包富文本样式）+ 占位符扫描接口（M8 消费）；渲染本体（RichTextBlock）归宿主 UI；零战斗语义——所有玩家可见/开发者编辑的数值类配置通用（SkillDef/SkillModifierDef/**StateDef 参数行/AttrModDef**/宿主装备） | 仅引擎基础（**与 TcsCore 平级互不依赖**：Core=运行时机制底座，Notation=策划记法底座；宿主非战斗配置可独立复用） |
| TcsAttribute | M2 属性管线 + **修正器模板 UTcsAttrModDef（D3-19：纯模板+约定列，物化执行器住 TcsState）** | Core, **Notation**（D5-18 v2） |
| TcsEffect | M4 触发行/链/解释器——**纯机制层（D4-14 注册制分派）**：只住控制流 6 + ModifyAttribute/SetVar/OnError 步骤、执行器注册表 + 自注册宏；领域步骤归领域模块 | Core, Attribute |
| TcsDamage | 瞬时流程骨架（伤害/治疗）：阶段化事件实时收集修改器、流程属性（M2 语义复用）、临时修正器（流程 Source）、消耗型修正器、FDamageRecord 回放/统计流；**公式=项目 delegate，插件零公式**；**Damage/Heal/ModifyFlow 步骤+执行器住本模块** | Core, Attribute, Effect |
| TcsTargeting | M4c 目标选择（D4-15；**v2 策略化**；**载体 D3-7 v3**——FTcsTargetSelectorStrategy/FTcsTargetFilterStrategy USTRUCT 纯虚基类+`TInstancedStruct` 持有，默认 Self/EventTarget；战斗步骤不内嵌 selector 读 Context.Targets；RadiusArea/FTargetingShape 后置）；实体查询注入（ICombatEntityQuery，定义在 TcsEffect）；指示器渲染归宿主/LAC | Core, Effect |
| TcsState | M3 状态（**Buff 子域，不独立**——TCS 双账本发散教训）；**ApplyState 步骤+执行器住本模块**；BuffDef 参数行 ValueConvention/描述绑定（D5-18 v2）；**`ITcsEntityLevelProvider`（GetEntityLevel 实体等级契约，宿主实现）+ StateLevel/InstigatorLevel × Array/Map 等级表源（PV-4，2026-09-11；TargetLevel 系暂不提供）** | **Core**, **Notation**（D5-18 v2）, Attribute, Effect |
| TcsSkill | M5 账本/施法/冷却/参数账本/重定向；参数行 ValueConvention（记法层） | Core, Notation, Attribute, State, Effect（**不含 Damage/Targeting**——链步骤是 FInstancedStruct 数据经注册表分派，编排层不具名领域步骤类型） |
| TcsCue | M7 插件默认表现适配：ICuePresenter 默认实现 + Cue 定义资产 + 可复用 VFX/SFX 经验封装；宿主可整表替换；**PlayCue 步骤+执行器住本模块** | Effect, 引擎表现模块 |
| TcsIntegration | M6 集成（**全插件唯一依赖 GameplayStateTree 的模块**） | Skill, Cue |
| TcsEditor | M8 工具，editor-only | 全部 |

- **表列依赖 = 该模块代码实际 include 的最小编译集**；上文依赖链 `Core←Attribute←Effect←{Damage,Targeting,State}←Skill`（D4-14）是**方向许可**（层级序：谁在谁之上、允许依赖谁），不是强制边清单——跨模块组合（技能×伤害×目标选择）发生在 TcsIntegration/宿主。

- **语言无关规定（2026-09-02 增补）**：插件不内嵌任何脚本引擎（AS/C#/TS 是宿主选择）；执行器注册双入口（C++ 静态宏 + 反射动态委托）保证任何语言/项目可用战斗系统；蓝图理论可行不承诺；R3 纯 C++。
- **基础设施内置规定（2026-09-02 增补）**：事件总线与对象池内置 TcsCore，不自立插件、不依赖/不替换 Lyra GameplayMessageRouter（Handler 模型/双通道/稳定序三不匹配；宿主出站桥自建）；提炼时机 = rule of two（第二个消费者出现再议 TcsFoundation）。
- **模块设置约定（2026-09-10 增补）**：DevSettings 按需生长——模块出现首个真实配置需求才建 `UTcs<模块名>Settings : UDeveloperSettings`（`GetCategoryName` 返回 `"Tcs"` 归拢同分类、SectionName 区分模块），不预建空壳；TcsCore 的 `UTcsDeveloperSettings` 为 "Tcs" 分类锚点保留；**红线：Settings 值不得影响模拟语义**——模拟可调量走 Def 或代码常量，观测走 CVar/日志分类（D0-6）。
- **死规定**：域依赖只允许沿层级向下（2026-09-02 增补：十一模块——TcsDamage 瞬时流程骨架+记录流、TcsTargeting 目标选择、TcsNotation 策划记法层，公式归项目；**D4-14 依赖翻转：TcsEffect 纯机制层只依赖 Core/Attribute，领域步骤类型+执行器归领域模块并自注册**）（UBT 强制）；禁止反向 include与跨层旁路；新增横切能力要么下沉 Core、要么立新模块，不许在既有模块间开侧门；新代码归属判定 = 看它依赖谁、被谁依赖。
- **命名**：模块前缀 Tcs（用户拍板；插件显示名仍开放，遗留拍板项 #3 收窄为仅显示名）；单复数随域名单数（TcsAttribute/TcsState/TcsEffect/TcsSkill）。
- **未来扩展**：TcsMass（Mass 适配，核心零改动前提已由 D3-1 保证）；宿主侧表现替换层。
