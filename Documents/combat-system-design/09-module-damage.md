# 09-module-damage.md — TcsDamage 瞬时流程层设计（伤害/治疗）

- 日期：2026-09-02
- 状态：**v4（PV 系列 D7-2 收窄：基础伤害值=参数账本解算输入、流程零计算）**（编译层模块名 **TcsDamage**，R0 §9；依据 D7-1~D7-7 已拍板 + 启发文档《伤害修改器设计》全文 + PV 系列 2026-09-11；**插件零公式、流程零预设**）
- 职责一句话：**瞬时流程的机制层——流程解释器 + 步骤注册表 + 流程属性黑板 + 收集事件协议 + 消耗型裁决 + 记录流。流程=数据模板（宿主组装）；标准阶段只是插件自带的步骤库与官方默认模板。**

## 1. 模块边界

- 依赖：TcsCore（句柄/Source/总线/到期堆）、TcsAttribute（provider 接口、聚合语义复用、事务）、**TcsEffect（FEffectStep 基类型+执行器注册宏+链解释器——D4-14 注册制分派）**。
- 被依赖：TcsEditor（组合层）、宿主（delegate 实现、统计/回放消费）——**R3 竖切例外**：TcsIntegration slice 编译依赖本模块（plan2）；全设计口径 TcsIntegration{Skill, Cue}。**TcsSkill 不依赖本模块**——链步骤运行时经注册表分派，编排层不具名领域步骤类型（D4-14）；"技能×伤害"组合在 TcsIntegration/宿主完成。
- 层级：`Core ← Attribute ← Effect ← {Damage, Targeting, State} ← Skill`（**R0 §9 原式，层级序=方向许可**——**Damage/Heal/ModifyFlow 步骤类型+执行器住本模块**，经 UE_DEFINE_EFFECT_STEP_EXECUTOR 自注册；TcsEffect 不依赖本模块）。用户 M9 追问轮确认此方向：真正消费流程的是宿主项目，不需要伤害的项目可不引入本模块，TcsEffect 照常运行。
- **流程管线宿主化（D7-5，用户 M9 轮拍板）**：本模块之于伤害流程 = TcsEffect 之于效果链——流程=数据模板，**阶段构成本身是项目知识**（"不同项目有不同项目的说法"，用户原话），插件不预设；标准十阶段降级为内建步骤库 + 官方默认模板（可整表替换）。

## 2. 类型词汇（对外）

### 2.1 流程上下文与三层值空间（M9 轮澄清）
- `FDamageFlowContext`（黑板，可池化）：`Attacker / Instigator / Targets / 分类 Tag 集 / 公式参数初值 / 流程属性黑板 / FlowSource / **CapturedAttrs**`。**黑板即上下文**——天然同构于 FEffectContext。
- **三层值空间**：
  1. **技能参数**（账本，Entry 级持久）——SkillDef 参数+修正链求值，不进流程；
  2. **公式参数初值**：`TMap<FGameplayTag, FTcsParamValue>`（数值为主 + FInstancedStruct 扩展位）——Damage 步骤配置的值（**FTcsParamValue**，PV 系列 2026-09-11 换型——Literal/ParamRef 等源，原 Param() 引用技能参数归 ParamRef）注入，**只读原料**，delegate 按名取用；键=项目词表 tag，**永不用下标**（**2026-09-22 tag 化：原 `TMap<FName, FFlowParamValue>`**）；
  3. **流程属性黑板**（容器=`FTcsFlowAttributes`：键+每键修正链+封闭五带运算（同 M2，Custom op 砍除——D2-7；值域策略逃逸走 IValueDomainPolicy 同款形态），M2 语义复用——**折叠统一调用 TcsAttribute 共享带式折叠纯函数（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器三处共用，勿私建第二份）**，作用域=流程用完即弃）——**伤害计算的工作值**（BaseHitRate/BaseDamage/FinalDamage 等就是临时变量）；内建步骤读写的键名是**标准步骤库的契约**（M8 校验/Explain 认识）——**契约键归框架**（`Tcs.Flow.Key.*` 原生声明，2026-09-22 tag 化），**项目键自由 tag、无注册表**；也是 ModifyFlow 提交的落点。
- **分类 Tag 集**：来源标签（流程启动时由 Damage 步骤配置写入）+ 元素标签（Element 步骤由 delegate 解析后写入，修改器可改）——修改器匹配键（"受火伤+20%"按 Tag 过滤收集事件）、免疫/减伤候选匹配（"免疫火焰"）、FDamageRecord 记录维度；词表归项目，插件只搬运与匹配。
- **AttrCapture**：`FDamageFlowConfig.AttrCaptureList{AttrKey, From: Instigator/Target}`——FlowStart 时从 M2 捕获进 CapturedAttrs 快照；属性读默认 Live、捕获命中读快照（**流程内读取一致性**——中途 buff 过期不追溯）；"本次攻击攻击力+10%"双路径：捕获命中→改 CapturedAttrs（随流程消失，零账本污染）／未捕获→FlowSource 临时修正器挂真实账本。
- `FlowSource`：**每流程唯一 FSourceHandle——作用域修改器的归属锚点**："本次攻击+X%"类临时修改挂 M2 账本、Source=FlowSource，流程结束 `RemoveBySource` 级联摘除（O(1)，复用 D2-2）；替代文章 CopyOnWrite（零拷贝）。

### 2.2 流程模板：标准步骤库 + 官方默认模板（D7-5）
- **流程模板** `FTcsDamageFlowTemplate`（概念名 `FCombatFlowTemplate`；实现名 2026-09-21 定 `FTcsFlowTemplate`，T-8 资产化轮改现名）：有序步骤数组（FInstancedStruct，同 FEffectStep 形状）+ `TemplateId: FGameplayTag`（2026-09-22 tag 化）；**多预设 = 多模板资产**（不同游戏模式/角色配不同模板）。模板选择链：Damage 步骤配置指定 → Def 覆盖 → 全局默认（官方默认 = `Tcs.Flow.Template.Default`）。
- **模板重定向** `FFlowRedirect{TargetTemplateId, ReplacementTemplateId}`（D7-7，用户拍板一步到位；字段为 tag）：状态/装备声明换流程，让渡点+重定向栈后挂/高优先+Source 级联回收——**三粒度让渡模式升四粒度**（参数→链→技能→流程）。
- **标准步骤库**（内建 struct+执行器，自注册；官方默认模板 = 下表组装，宿主可整表替换）：

| 标准步骤 | 引擎行为 | 项目挂点 |
|---|---|---|
| CollectStart | 发流程开始事件，重置收集 | — |
| PreHit | 发收集事件 → 应用 | 修改命中率/注册免疫候选 |
| Hit | delegate 基础命中率 → 修改器 → 判定 Hit/Miss/Invalid | `ICombatDamageFlowDelegate.GetBaseHitRate` |
| Crit | delegate 基础暴击率 → 修改器 → 判定 | `GetBaseCritRate` |
| Element | delegate 解析元素 → 修改器可改 → 元素标签写入分类 Tag 集 | `ResolveElement` |
| BaseDamage | **接收输入值**（链步骤 DamageBase = 参数账本解算结果——PV-7 2026-09-11：流程零基础值计算）→ 修改器可改 | `CalculateBaseDamage`（**降级逃生口**——D7-2 收窄，宿主特殊公式才实现） |
| AfterDamage | 发收集事件 → 应用（"伤害+50"） | 修改 FinalDamage |
| PreExecute | 发收集事件 → 收集免疫/减伤候选（**只收集不消费**） | 提交消耗型候选 |
| Execute | 免疫/减伤裁决（SortKey 选一）→ 宿主护盾 hook → M2 事务扣血（键=步骤配置的项目词表 AttrKey）→ **成功才消费** | `ModifyShield`；OnConsumed 回调 |
| Completed | 发完成事件 + **FDamageRecord 入记录流** | 统计/回放消费 |

- **通用数据步骤（编辑器拼流程零 C++）**：`FlowModify{TargetKey, Op, Operand(FTcsParamValue——Literal/ParamRef 等源，PV 系列 2026-09-11 换型；黑板键引用保留为流程域自身 Operand 选项), SortKey}`（数据化黑板写入——"破甲阶段"=一个 FlowModify）与 `FlowDelegate{TargetKey, Delegate 接口位}`（数据化委托调用，轻量公式挂法）。**每步骤可带 Conditions**（复用 D4-5 条件最小集+Custom Fragment，对流程上下文求值，不过则跳过——"嗜血才跑的额外步骤"用条件表达）。
- **自定义步骤的 C++ 边界（宪法 R0 §8）**：仅两种——①新语义步骤（读宿主独有游戏状态：部位判定/连击计数），新词汇归 C++、自注册宏登记；②新公式（D7-2 插件零公式，delegate=宿主）。除此之外流程组装纯数据。
- 判定代码（Hit/Crit 基础值与判定）= 标准步骤库；**基础伤害值来源 = 链步骤数据配置（参数账本解算——PV-7 2026-09-11，流程零计算）**；命中/暴击基础率等仍为 delegate 挂点——或宿主写自定义步骤整段替代。

### 2.3 伤害修改器 = 触发行 + ModifyFlow（唯一通道，D7-6）
- **修改器没有独立载体类型**（D3-5 推论：状态行为=原语链）：一个"伤害修改器" = 状态/装备的触发行订阅步骤收集事件 → 条件 → `ModifyFlow` 提交。文章"有状态/无状态修改器" = 带/不带条件的触发行；订阅随 Source 生命周期自动退订——"状态在，修改器就在"天然成立。
- 消耗型：提交携带 `{MaxUses/Cooldown/OnConsumed/SortKey}`；Execute 步骤裁决 SortKey 选一、成功执行才消费、未选中者完全不动（D7-4）。
- 聚合：提交落进黑板对应键的修正链，读时按 M2 同式求值——**折叠统一走 TcsAttribute 共享带式折叠纯函数（D5-5 v3，2026-09-14：M2 属性聚合 / M5 参数链 / 本容器三处共用；SortKey 不参与顺序语义，带序由 Op 决定）**。
- 创作糖：一条修改器 = 触发行+单步链（比文章一行重）——M8 编辑器提供"伤害修改器"模板自动生成，**机制唯一、界面给糖**。

### 2.4 接口与记录
- `ICombatDamageFlowDelegate`（纯 C++ 接口，宿主实现）：`GetBaseHitRate / GetBaseCritRate / ResolveElement / CalculateBaseDamage（**降级逃生口**——PV-7 D7-2 收窄后仅宿主特殊公式实现，普通项目零 delegate） / ModifyShield`。
- 治疗流程：同骨架精简版（无 Hit/免疫；Crit 可选），`IHealFlowDelegate.GetBaseHeal`。
- `FDamageRecord`：`FlowId / Source / Target / 元素 / Hit / Crit / Base / Final / Executed / Absorbed / Kill / 序号 / 时刻`——完成/打断时发 `Damage.Record` 事件（立即，回放依赖序）+ 环形缓冲（统计）。
- 提交原语：**`ModifyFlow{键, 运算, 操作数, 消耗策略?}`（战斗组；步骤类型+执行器住本模块——D4-14）**——触发行订阅阶段收集事件后在响应里提交流程属性修正；`消耗策略`（MaxUses/Cooldown/OnConsumed）挂在提交上，实现"免疫一次"类效果。

## 3. 关键机制

- **收集 ≠ 消费**（D7-4）：阶段内只收集候选；Execute 裁决（SortKey 选一）→ 成功执行才调 OnConsumed；流程失败/打断 → 无人消费。
- **瞬时角色属性修改**（"本次攻击攻击力+10%"）：临时修正器 + **FlowSource**（每流程唯一 FSourceHandle）——O(1) 挂账本、流程结束 `RemoveBySource` 级联摘除；替代文章的 CopyOnWrite。v1 只做当前值通道。
- **流程同步性**：瞬时流程单帧同步完成——**无挂起/唤醒**（区别于链的 WaitEvent）；消耗型 CD 走到期堆（流程外）。
- **免疫多候选裁决**：SortKey/Priority 选一；未选中候选完全不动（文章"三个免疫只消费一个"）。

## 4. 网络姿态落点（NET-1/2）

- 权威侧跑流程；`FDamageRecord` 事件流复制到客户端（战斗日志/统计/UI 数据源）——服务器权威、客户端零预测（瞬时流程无预演问题，文章同款结论）。

## 5. 非目标

不做任何公式（命中/暴击/元素=delegate 挂点或宿主步骤；**基础伤害值=链步骤参数解算输入——PV-7 2026-09-11，流程零计算，流程中途公式贡献=伤害修改器自实现（D7-6 通道）**）；**不预设流程阶段构成**（流程=数据模板，D7-5——标准阶段只是官方默认模板）；不做元素克制表；不做护盾系统（宿主 hook）；不做伤害数字 UI/统计报表（只供数据流与记录）；**不做生产期过程级溯源**（FDamageRecord 是结果快照，不含每键修改器贡献清单——"这个数怎么算出来的"走 M8 Explain 开发期回放，08 §4；千人战场每笔伤害带贡献清单是量灾难）；不做 DoT（那是 M3 周期 + 链，不走瞬时流程）。

## 6. 依据

- 启发文档《伤害修改器设计》（用户提供全文，wx_article.txt）：阶段化骨架、事件实时收集、流程由流程选择执行修改器、流程属性（FormulaAttr）、瞬间属性修改、有状态修改器、黑板即上下文——七个机制全部落位（四个同构、一个被句柄设计超越、一个真新增）。
- 拍板：D7-1~D7-4（2026-09-02 问答框，含"插件零公式"原则——用户："具体项目中如何实现 damage/heal 还都不确定，插件直接提供 effect 流程不合适"）；**D7-5~D7-7（2026-09-02 M9 追问轮）**：用户否定预设阶段表（"Damage 阶段具体都有什么，不同项目有不同项目的说法"）→ 流程管线宿主化；伤害修改器唯一通道确认（用户追问后统一模型拍板）；模板重定向用户拍板一步到位（否决 YAGNI 缓增补）；**D5-5 v3（2026-09-14：流程属性容器 = TcsAttribute 共享带式折叠器三处共用之一，折叠勿私建）**。

## 7. 验收钩子

竖切扩展：火球术链 → Damage 原语 → **官方默认模板** → 项目 delegate（一条简单公式）→ Health 事务 → Record 事件可见；三免疫实例注入后仅消费一个；临时修正器（本次攻击 +10% 攻击力）流程后还原。M9 轮新增人工检查：数据步骤（FlowModify 实现破甲）零 C++ 拼流程；模板重定向挂/摘（状态声明 FFlowRedirect）；修改器模板生成的触发行+单步链生效。

## 8. 修订记录

- v1（2026-09-02）：初版（D7-1~4 折入）。
- v2（2026-09-02，M9 追问轮）：§1 依赖方向对齐 D4-14 翻转后形状（本模块依赖 TcsEffect，Damage/Heal/ModifyFlow 步骤类型+执行器住本模块）；ModifyFlow 归属标注。
- v3（2026-09-02，M9 追问轮重构）：**流程管线宿主化**（D7-5——阶段表降级为标准步骤库+官方默认模板，流程=数据模板；通用数据步骤 FlowModify/FlowDelegate + 每步骤 Conditions）；§2.1 重写为三层值空间（技能参数/公式参数初值 Map/流程属性黑板契约键名）+ 分类 Tag 集/FlowSource/CapturedAttrs 定位澄清；§2.3 修改器唯一通道（D7-6）；D7-7 模板重定向（四粒度让渡）。
- v4（2026-09-11，PV 系列 D7-2 收窄）：**基础伤害值来源 = 链步骤数据配置（StateParam 参数账本解算——复合运算由参数链承载），结果输入流程，流程零基础值计算**（用户否决流程域拼装/表达式源/双字段三案）；`CalculateBaseDamage` 从 ★主公式 **降级逃生口**（普通项目零 delegate）；公式参数初值/FlowModify Operand 换型 FTcsParamValue（黑板键引用保留为流程域自身选项）；流程中途公式贡献 = 伤害修改器自实现（D7-6 通道，不开 BaseDamage 公式口）。
- v4 增补（2026-09-15，D5-5 v3 交叉引用补齐）：§2.1/§2.3 流程属性黑板的折叠口径显式化——统一调用 TcsAttribute 共享带式折叠纯函数（三处共用之一），实现落点指向补齐；顺带移除无出处的旧词"三域三制"。
- v4 增补 2（2026-09-23，标识体系 tag 化改造回写）：§2.1 公式参数初值键 `TMap<FName, …>` → **`TMap<FGameplayTag, FTcsParamValue>`**；黑板容器名对齐实现（`FFlowAttributes` → **`FTcsFlowAttributes`**）；**契约键归框架**（`Tcs.Flow.Key.*` 原生声明）与项目键自由 tag 的分工写明；§2.2 模板身份补 `TemplateId: FGameplayTag`、模板类型名对齐实现（`FTcsDamageFlowTemplate`，T-8 轮改名）；重定向字段为 tag。落点 = 提案 `switch-identifiers-to-gameplay-tags`（2026-09-22 归档）。
