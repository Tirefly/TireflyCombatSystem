# M0-min + M2 决策点提案 v1（待逐项拍板）

- 日期：2026-09-02
- 状态：**决策点提案**——每项带 2–3 方案与推荐，逐项拍板后才进入对应设计文档
- 证据核验：2026-09-02 已对照 TCS 调研报告 04/05/07/13/ZZ 逐条核验并修正（要点：TCS 的"fixpoint"= 有界跳过-重试排除集循环，非数值收敛；默认提交尾行内 flush 而非统一帧末；依赖为求值期"读即登记"非声明式；句柄为 UObject+单调 int32，无索引+代际先例）
- 顺序无关性：以下决策点在实施顺序方案 A/B/C 下均需要，提前起草不依赖顺序拍板结果
- 已定裁决不重议（落地清单见文末）；本清单只列**真正开放**的问题

---

## M0-min（最小内核：句柄+池、事件总线、时钟泵）

### D0-1 确定性纪律（约束 M0 时钟与 M2 聚合，必须最先定）
- **A 不预留**：计时可用 wall-clock，遍历顺序随意。省事，但未来帧同步/回放/录像需重写核心。
- **B 完全确定性**：定点数、全排序、禁浮点聚合。成本大，当前无消费者，违反 YAGNI。
- **C 推荐：轻量确定性纪律**——计时一律走 ScaledDt（禁 wall-clock API，靠封装强制）；聚合公式顺序无关（已定裁决天然满足）；容器遍历用稳定序；随机数走显式注入的流。不做定点数、不实现同步。
- 影响：M2 聚合的纯函数化程度、M4 事件派发顺序约定。

### D0-2 句柄形态
- **A FGuid**：128bit、无池定位能力，需额外映射表。
- **B 推荐：索引+代际句柄**（如 `uint32 Index + uint32 Generation` 或打包 64bit）：O(1) 池定位，代际校验捕获悬空引用，是"USTRUCT+句柄+池"既定裁决的自然实现。**注意：净新增设计，TCS 无先例**（核验 2026-09-02：TCS 用 UObject 实例 + 弱引用 + 进程级单调 int32 Id，靠 UE GC 与每帧自愈清理兜底）。
- **C TCS 现状式**：UObject 实例 + TObjectPtr 弱持有 + 单调 int32 ID——已验证可行，但实例是 UObject（GC 压力、非池化）且与"USTRUCT+池"裁决不符；选 C 等于推翻既定裁决，需显式说明理由。
- 附加约束：**每实例类型独立强类型句柄**（`FStateHandle`/`FSkillHandle`…），禁止通用句柄互串（依赖铁律4 的句柄版）。

### D0-3 事件载荷的类型安全等级
- **A 纯 FInstancedStruct 载荷**：灵活、零 include 依赖，但字段访问无编译期检查（AbilityKit Dataflow 风格）。
- **B 每事件一个具体 FStruct + 强类型分发**：类型安全，但事件类型爆炸时头文件依赖膨胀（TCS 无总线时的问题换个形式回来）。
- **C 推荐：核心载荷用具体 FStruct（少量、稳定、核心词汇级）+ `FInstancedStruct` 扩展位承载域扩展载荷**；Handler UClass 统一入口（裁决2a），核心不 include 域扩展头。
- 关联：同步立即派发 vs 帧末泵的事件分类表是 M4a 决策点，但 M0 总线需先提供两种通道（立即 + 帧末队列）。

### D0-4 池与线程模型
- **A 线程安全池**（锁/无锁）：为未来多线程留路。
- **B 推荐：单游戏线程假设 + 断言**：战斗逻辑（含 Mass processor、AI tick）都在游戏线程；不加锁成本；越界即断言失败暴露架构违规。真需要并行时再升级（届时是局部改造而非重构）。

### D0-5 时钟泵归属与 ScaledDt 来源
- **A 各组件自行 Tick**：回到 TCS 模式，时序散乱。
- **B 推荐：单一 `UTickableWorldSubsystem` 泵（裁决2b）+ `FCombatClock` 封装**：内部默认 `World DeltaSeconds × TimeDilation`，时间源做成可注入接口（ITimeSource），宿主项目可替换（如回合制注入"回合数即时间"）。Tick 组与顺序：PrePhysics 泵逻辑、TG_PostUpdateWork 留给 M2 flush（见 D2-5）。

### D0-6 日志归 UE 原生分类（M9 收尾轮，三轮演化定稿）
- 演化：①初案注册制门面（注册标签+级别阈值表+命令行+屏显）；②用户细化砍自建级别（分级=UE 原生 Verbosity，默认 LogAll）；③用户最终拍板**撤销设施**——统一注册入口无消费者（注册表只剩无代码消费的清单）、自建转发反使 UE 日志查看器丢分类过滤、"统一显式口"实为测试装置需求而非插件机制需求。
- 终案：**日志归 UE 原生分类制**——每模块 `DECLARE_LOG_CATEGORY_EXTERN(LogTcs<模块名>, Log, All)`（命名规范入档），verbosity/console/过滤全套继承 UE；**屏显归测试装置/宿主**（直调 AddOnScreenDebugMessage），插件模块零屏显调用；showdebug 面板=未来接口位（届时日志路由随面板回归）。TcsCore 诊断退回：统计 CVar（池/堆）+ UTcsDeveloperSettings。**D0-6 v2（日志通道文件，审阅轮 5 用户新增规范）**：声明独立成 `Public/<模块名>LogChannel.h` + 定义 `Private/<模块名>LogChannel.cpp`——使用日志 include LogChannel 头而非 Module.h（模块壳保持最小）。

---

## M2（属性/数值：容器+Modifier+recalc+clamp+事务）

### D2-1 AttributeId 表示与策划 authoring 载体
- **A FGameplayTag 当属性 ID**：灵活但无编译期检查、字符串比较开销、易拼错。
- **B 推荐：强类型 UEnum（TEnumAsByte/FCombatAttributeId）为编译期词汇 + 注册表桥接 DataTable 行名**；属性表（策划载体）每行：Id、BaseValue、Min/Max 引用、依赖声明。UEnum 保证 key 空间隔离（铁律4），DataTable 保证策划友好。
- **C FName 注册表**：运行期注册、可热插，但失去编译期检查，为 YAGNI。

### D2-2 Modifier 模型与生命周期归属
- **A M2 内建定时移除**（modifier 带 duration，M2 自己计时）：M2 变复杂，且计时语义与 buff 生命周期重复。
- **B 推荐：M2 无计时器——modifier 生命周期完全由来源（buff/skill/装备的实例句柄）管理**，M2 只提供"来源句柄注销 → 级联移除其全部 modifier"的被动机制。时限、堆叠、刷新全部是 M3 的事。M2 保持纯函数聚合器。
- 字段集（提案）：`来源句柄、Tag（来源标签）、AttributeId、优先级带（Override/Add/PercentAdd/Mul）、值、作用通道（若有）`。

### D2-3 属性间依赖（派生属性）
- **A 不支持**：MaxHealth 等派生值由上层手动维护。简单，但必然长出野生的同步代码。
- **B 推荐：读即登记（TCS 已验证机制）**——求值期间读取其他属性 CurrentValue/数字参数时自动登记依赖边（TCS 实测口径：读 CurrentValue 登记、读 BaseValue 不登记）；图节点为 modifier 父实例，每轮重算即时重建；环检测用 Kahn + Tarjan SCC（skip 模式剔除重试 / 严格模式报错，思路直接继承）。策划**零声明负担**。
- **C 显式声明 DAG**：Def 上加 DependsOn 字段，构建期可校验、可视化友好（裁决3）；但属**净新增**（核验：TCS 无 DependsOn，全靠求值期收集），且声明与实际读取可能不一致（声明了没用/用了没声明），需额外一致性校验。
- 核验发现的两点注意：① TCS 深链 >8 跳会被 clamp 轮上限误判为环——新系统需把"重算轮数上限"与"环判定"解耦；② 若选 C，建议同时保留 B 的运行期环检测兜底。

### D2-4 clamp 的位置与形态
- **A 每 modifier 自带 clamp**：灵活但语义碎片化，聚合公式复杂化。
- **B 推荐：管线末端统一 clamp，边界三态可选（None / Static / Dynamic=另一属性）**——Dynamic 边界可被 ongoing modifier 影响、自引用禁止（TCS 已验证口径，如 HP≤MaxHP≤Level 链）；"最大生命上限降低"类 debuff 因此是纯数据行。持续生效规格继承 TCS：提交前有界 clamp 轮（TCS 上限 8 轮；其 >8 跳深链误判环的缺陷见 D2-3 注意①，新系统修正）。
- 附加：clamp 是聚合公式的最终一步，不参与 modifier 优先级排序。

### D2-5 脏标记、事务提交与 flush 时机
- **A 写时立即重算**：读永远新鲜，但一个 buff 挂 10 个 modifier 触发 10 次重算。
- **B 推荐：写时标脏 + 读时惰性重算（已定裁决）+ 单次管线事务提交**——事务边界 = 单次管线调用（挂/移除/改基值）内的候选集计算（求值→聚合→clamp 全在内存候选值上做），唯一提交点写回，失败零写入；**提交尾默认行内 flush 广播**（核验纠正 2026-09-02：TCS 默认即行内 flush，TG_PostUpdateWork 只是按需开启的帧末安全点，承接跨调用栈脏项与广播期延迟补发——"统一帧末 flush"是偏离 TCS 的新设计，不得冒充继承）。
- **开放子问题（拍板）**：① 事务期间读取返回旧值还是 pending 值？推荐**旧值 + 显式 `PeekPending`**（读旧值保证帧内一致性，peek 服务表现层预览）；② 是否保留帧末安全点机制？推荐**保留、默认关**（防重入与广播期重排的兜底，出问题再开）。

---

### D2-10 FlatAdd 加带（M9 收尾轮拍板，D2 修订）
- `ETcsAttributeOp` 追加 `TAO_FlatAdd`（第五带，带权 30）——**乘后平坦加**：`((Base+ΣAdd)×(1+ΣPercentAdd))×ΠMul + ΣFlatAdd`，不受 PercentAdd/Mul 缩放；Override 存在时被覆盖（"最强覆盖生效"不变）。
- 来源：GAS UE5.5 `EGameplayModOp::FixedAdd` 借鉴（shipped 先例=需求证据）；用户："挺常用的 AttrModOp"。命名 `FlatAdd` 用户定（弃 FixedAdd——中文易读成"固定不变"；弃 PostMulAdd——与桶序耦合）。交换性论证：全部缩放之后、clamp 之前，与既有带无交换歧义。
- 成本：枚举末尾追加（旧配置零迁移）+ 聚合尾段；~~M5 参数链不跟进（有序链式 SortKey 排序即可表达）~~——**2026-09-14 D5-5 v3 修订：M5 参数链改用与 M2 同一套五带（含 `FlatAdd`）与同一份带式折叠器（住 TcsAttribute，M2/M5/TcsDamage 流程属性三处共用），原"不跟进"口径作废**。R3 测试不消费，随计划一 Task 4/5 一并实现。

### D2-11 AttributeModOperand（M9 收尾轮拍板：主属性→派生属性载体）
- `FTcsAttrModOperand{Kind: OPK_Literal(默认)|OPK_AttributeScaled, Literal, Attribute, Coefficient}`——修正器 Operand 属性引用化：OPK_AttributeScaled 时 Operand = Coefficient × Current(Attribute)，聚合收集时求值并**读即登记依赖边**（D2-3 现成）。"1 力量=2 攻击力" = AttackPower 常驻修正器 `{TAO_Add, AttributeScaled(Strength, 2.0)}`；纯派生 = Base 0 + 仅此条；混合/纯派生统一，不需要"公式属性"概念。
- 取证：AbilityKit `MagnitudeSourceType`（ContextFloat/TimeDecay）同构【源码】；其表达式引擎（667 行 DSL）不采纳（封闭公式原则）；非线性派生走判定树①/宿主命令式。命名用户定：AttributeModOperand（非裸 AttributeOperand——语义=修正器的操作数）。
- 成本：operand 变体 + 求值挂接（计划一 Task 4/5）；流程属性黑板同构适用按需引入。
- **已考察并拒绝的候选（同轮入档，防回潮）**——拒绝理由收敛到三条不变式：**纯函数性**（操作复制+客户端重算地基）/ **域不透明**（M2 不认识时间·技能·表·世界）/ **封闭公式**（解释权出处唯一）：
  - ContextFloat（宿主上下文值，AbilityKit 同款）：破坏纯函数性——Context 值也得复制同步；宿主折算 Literal 替代
  - TimeDecay（随时间衰减，AbilityKit 同款）：M2 不认识时间（02 非目标）——时间衰减归 M3 时长/周期域
  - 随机 Roll：破坏确定性（D0-1）——随机上游掷好传 Literal
  - 技能参数引用 / Curve 等级表引用：跨域依赖爆炸 / D3-11 等级成长归项目——宿主查表折算 Literal
  - Custom Fragment（运行时自定义求值）：封闭公式/无运行时插点（D2-7 同款理由）——新运算走判定树②加带（D2-10 先例）
  - "% of 其他属性"：**已被 OPK_AttributeScaled 覆盖**（AttributeScaled(MaxHealth, 0.05) 即"+5% MaxHP"），非新 Kind
- 逃生口（统一形态）：宿主在 M2 外维护动态源，变化时更新 Literal（RemoveBySource+重挂/改槽位 Operand）——用宿主监听成本换重算一致性。能力对照诚实声明：AbilityKit 有 ContextFloat/TimeDecay 非其错误——其架构无"不复制数值、客户端重算"承诺（专门同步系统兜底），约束不同取舍不同。

### D2-12 FTcsParamScalar 统一载体（修正器模板轮拍板，R3 计划审阅轮 4）
- `FTcsParamScalar{Mode: Literal(默认)|Param, Literal, ParamKey}`（住 TcsCore，零战斗语义数据词汇）——全插件"字面量|Param"字段的统一类型载体：03 DurationTime、05 时段/冷却 Duration、04 链字段 Param()（§5 Authoring 纪律）、修正器 Operand.Literal（D2-11/模板资产）。
- **Literal 存规范值**（ValueConvention 转换在参数写入点，模板零变换逻辑——无表达式宪法）；Param 模式仅在**定义侧/模板侧**合法：物化点（apply/激活/宿主 MaterializeModifiers）解析一次后以 Literal 进账本——账本条目 Operand 的 Literal 恒为已解析规范值（物化器保证）。
- 解析 miss 兜底 Literal 默认值（对齐 D3-12"施加上下文覆盖值优先，Def 默认兜底"）；参数键空间=施加方参数表（模板只声明"我吃哪个键"——覆写=传参，无字段寻址机制，旧 TCS 参数覆写 OpenSpec 的"大"由此消化）。
- **2026-09-11 修订（PV 系列收束）**：本载体被 **`FTcsParamValue{TInstancedStruct<FTcsParamValueSource>}`** 取代（D3-7 v3 形态；基类虚函数 **Evaluate**——用户动词纪律：Resolve 仅句柄/Id→对象；内置 `FTcsParamSource_Literal/ParamRef`，等级表/属性源归各域模块）——详见 `2026-09-10-param-value-source-decision-points.md`；**D2-13 双形状不变式保留**（定义侧换载体，账本侧恒为已解析规范值、零膨胀）。

### D2-13 Operand 双形状（B 方案，用户拍板，R3 计划审阅轮 5）
- **定义侧** `FTcsAttrModOperandDef{Kind, Literal: FTcsParamScalar, Attribute, Coefficient}`——模板/Def 配置用：Literal 可声明"吃施加方参数表哪个键+默认规范值"；**运行侧** `FTcsAttrModOperand{Kind, Literal: double(已解析), Attribute, Coefficient}`——M2 账本 ModifierSlots 用：物化器保证已解析，**账本零膨胀**。
- OPK_AttributeScaled 分支两侧同形（Coefficient 留 double——参数化缩放系数未见需求，需要时判定树；live 求值不物化，读即登记不变）。物化器单点转换收敛漂移风险——同 D3-12 ParamSnapshot"定义参数的解析后副本"模式，非旧 TCS 双轨字段镜像（那是需人工同步的字段拷贝）。M5 `FTcsNumericParamModifier.Operand` 同构处理。

## 已定裁决落地清单（不重议，进设计文档）

| 裁决 | 落点 |
|---|---|
| 聚合公式 Override(0)>Add(10)>PercentAdd(15)>Mul(20)、组内顺序无关、脏标记、1e-5 | D2-5 的实现规格 |
| 事件总线 = Tag 键 + 订阅表 + 共享 Handler UClass；struct 不自绑 delegate | D0-3 的实现规格 |
| tick 泵 = TickableWorldSubsystem 泵 ScaledDt | D0-5 的实现规格 |
| 强类型 key 空间隔离（AttributeId/StateId/EffectId/CueId） | D2-1 的实现规格 |

## 拍板方式

逐项回复（如"D0-1 C，D0-2 B，…"）或整批"按推荐"。拍板通过后产出 `01-module-m0-core.md` 与 `02-module-attributes.md` 两份设计文档（类型词汇、入口服务、非目标、依据），进入下一对模块。

| D2-6 | **AttributeDef 值域模式终定**：`ValueDomain{Clamp(默认)/Wrap/CustomFragment(IValueDomainPolicy 只接管值域函数)}`；钳制反应走事件订阅（用户自定义 clamp 策略类失败的修正引入） | 已拍板 2026-09-02 |
| D2-7 | **Custom op 砍除终定**：M2 四带与参数账本的 Custom op 均砍除——计算在上游（链/伤害流程/项目代码）传入终值；Custom 逃逸位规约边界补全：适用于策略选择，不适用于公式/聚合本体 | 已拍板 2026-09-02 |

| D2-8 | **属性数据宿主终定**（用户指出的缺失节）：UCombatAttributeSubsystem(World)→FAttributeStore(句柄键控)→FAttributeInstance{Base/CachedCurrent/Dirty/Bounds/ModifierSlots}；Base=宿主升级事务写入（等级→数值归项目）；Current=派生缓存非权威；ModifierInstance 挂属性槽（自由链表，TCS/AbilityKit 同构）；三种修正器存放地总表（实体属性/技能参数/流程属性——同一 Source 级联形状）；03/06 注册表命名一致性待办 | 已拍板 2026-09-02 |

| D2-9 | **实例-定义引用规范终定**：权威=DefId（非指针）；热路径=解析缓存（const 指针+DefLibrary 版本校验，重定向/热重载自动失效）；**热路径零回查 Def**（信息进快照，否则=设计漏洞）；FAttributeName 稠密缓存覆盖词表 | 已拍板 2026-09-02 |
