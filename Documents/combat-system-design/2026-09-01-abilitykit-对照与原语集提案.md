# AbilityKit 对照调研综合与原语集提案

> 性质：对照调研**综合文档**——三路提取报告为证据库，本篇只做裁决与提案；未拍板项明确标注。
> 日期：2026-09-01 · 调研对象：AbilityKit（GitHub 快照 AbilityKit-master，Unity+.NET 纯 C# 可组合战斗工具集，92 包 / 4835 cs，MIT，开发期自述）。
> 证据库（本目录 research/ 下，全部带证据等级与源码路径）：
> - A 路 `research/abilitykit-extract-skill-pipeline.md`（Pipeline/ActionSchema/combat 原语/FEffectStep 原语集提案 v1）
> - B 路 `research/abilitykit-extract-triggering-attributes.md`（TriggerPlan/TriggerRunner/条件/修饰器聚合/触发器行 12 字段提案）
> - C 路 `research/abilitykit-extract-editor-determinism.md`（actioneditor 图编辑器实况/确定性栈/Record 回放/HFSM）
> 关联：裁决 1（状态关系归属）、裁决 2（事件分发 + tick 泵）、裁决 3（可视化范式）三份已定文档 + 总日志 §6。

## 1. 调研范围与方法

三路并行提取（各路先读我方 4 份已定设计文档建立模型，再读 ability-kit-research 对应报告 + vendored 源码只读复核），我方综合裁决。能力边界：全部静态取证未运行构建/测试；AbilityKit 文档滞后于源码是系统性现象，引用以其报告差异清单为准。

## 2. 总对照结论表（我方待定项 → AbilityKit 形态 → 裁决）

| # | 我方待定项 | AbilityKit 形态 | 裁决 |
|---|---|---|---|
| 1 | **FEffectStep 原语集**（暗黑类词缀最小集） | Pipeline 8 内建阶段 + combat 原语参数面（源码证实丰富） | **17 原语提案 v1 + 3 不立项裁决**（A 路 §6，见本篇 §3）——采纳为基线待拍板 |
| 2 | **触发器表字段** | TriggerPlan 不可变计划（Phase/Priority/条件/动作/执行控制） | **12 字段提案 v1**（B 路 §7.1，见本篇 §4）——采纳为基线待拍板 |
| 3 | 修饰器聚合：重算 vs 增量 | 全框架重算式（分组公式顺序无关 + 脏标记懒重算 + 依赖图） | **维持重算式**，公式/阈值/键空间隔离照抄清单（B 路 §7.2，见本篇 §5）——第三方实证关闭此项 |
| 4 | 事件分级清单（同步立即 vs 帧末泵） | AbilityKit 默认 Immediate 同步直派 + MaxFlushPasses 环保险丝 | Immediate 直派**不构成帧末泵反证**（同步性 tradeoff），但**分级清单升级为必答题**——否则"查询类立即返回"无处安放（B 路 §3.5/§9） |
| 5 | 订阅降维（Tag 层级匹配） | 精确 Id + 参数类型复合通道（无层级匹配） | Tag 层级为主 + `EventPayloadFilter` 字段补参数维度（B 路 §7.1 字段 2）——两种降维互有得失的取舍已落 |
| 6 | 中断/压制语义（事件分发文档未定义） | 软中断 StopBelowPriority + InterruptPriority 自动压制 + Strict 短路 | **我方缺口确认**，补进触发器行字段 7（B 路 §3.3） |
| 7 | 定步长积累器（tick 泵待定） | `FixedStepReplayClock`（≈9 行：acc 累加→扣步→frame++） | **现成最小参照**；引入判据=先回答"逻辑帧与渲染帧是否解耦"，200 单位同帧顺序 tick 暂不需要（C 路 §4.5） |
| 8 | 自走棋战斗回放 | FrameRecordFile 三轨（Meta(TickRate+RandomSeed)/输入帧/周期状态哈希 + 可选快照）+ diff 对账工具 + CLI 门禁 | **最小可行形态已给出**（C 路 §5.4，见本篇 §6）——仍属待定，优先级后置 |
| 9 | 条件表达式（RPN/黑板） | RPN 栈机 + Blackboard + NumericVarDomain | **不抄**——谓词 AND 链 + 结构化参数覆盖自走棋条件全集，表达式层是三份复杂度（B 路 §4.4/§7.1 不抄清单） |
| 10 | 裁决 3 自研图对照素材 | actioneditor 实为 NBC.ActionEditor 时间轴（**非节点图**）；pipeline 包无图编辑器（Phase 编排靠代码） | **裁决 3 对照预期落空但收获更准**：exec-pin 图维持 UEdGraph/SGraphPanel（K2 排除被印证）；ACT 时间轴 lane 拿到 Track/Clip 四层先例（C 路 §2/§3） |

三路共同的大验证：**"类做词汇、struct 做状态、系统做流转"三分离在 AbilityKit 有全面对应物**——其"定义实例/运行实例工厂克隆（深树浅值）"的并发坑，被我方三分离结构性免疫【源码调研】；其 Pipeline/Triggering/Effect 三体系间的字符串共享键接缝，被我方"总线+订阅表一体贯通"规避。等待语义上，AbilityKit 轮询+外部 running action 的接缝成本反证了我方"等待句柄内建步骤链"的取舍（A 路 §5）。

## 3. FEffectStep 原语集提案 v1（17 原语 + 3 不立项）

> 细节（每原语参数面、〔源码证实〕/〔提案〕标注、来源路径）见 A 路报告 §6；此处为裁决级清单。**状态：提案，待用户拍板。**

**判据**：需要专属运行时资源/容器、或词缀高频组合点且参数面不可再简化、或控制流语义无法用参数表达 → 独立原语；可由其他原语+参数表达 → 组合而成。

**控制流（7）**：`WaitDelay`（ScaledDt 累积，≤0 配置拒绝）、`WaitUntil`（主路径事件句柄、谓词仅兜底轮询 + 超时策略）、`Branch`（首个命中条件→子链；无命中真三态且 Fail 真失败；瞬时分支同帧完成）、`Parallel`（子链组 + All/Any 完成策略）、`Repeat`（Count>0 或显式 Infinite + Interval；异常上抛）、`Gate`（硬门控，失败即终止整链 + FailReason 通道）、`RunSubChain`（子链引用复用，词缀组合基本手段——AbilityKit 无此原语，我方新增）。

**战斗（7）**：`SelectTargets`（形状独立成原语：圆/扇【源码证实】+ 矩/环【提案】+ 过滤/评分/TopK/去重，结果集供多步复用）、`Damage`（8 阶段管线借用；数值走引用化；类型单值枚举且 Mixed 管线内两段结算——修正其绕减免坑；穿透走槽位不做 Flags；暴击 Roll 由泵注入种子随机）、`Heal`（我方新增，与 Damage 对称 + 溢出策略）、`ApplyState`（挂状态总入口：DurationPolicy/周期/堆叠策略【我方新增，AbilityKit 无】/Tag 门禁/授予回收 Tag 配对——Apply 序直接借用）、`ModifyAttribute`（属性修改与来源生命周期配对清理，sourceId 模式源码证实）、`SpawnProjectile`（参数面为源码最完整面可照抄为 UPROPERTY 字段；命中→事件→效果链接缝沿用我方总线）、`SpawnArea`（圆形进出/驻留事件源码证实；矩形/移动区域提案）。

**表现与元（3）**：`PlayCue`（表现与逻辑分离，GAS 正确遗产）、`SetVar`（词缀中间量传递；数值引用化是组合而非硬编码的关键——NumericValueRef 思想简化采用）、`OnError`（包裹子链 + FailChain/Continue/Retry 策略——我方新增，AbilityKit 异常语义分散且多处坑）。

**不立项（3）**：`Schedule` 并入 SpawnProjectile/Repeat 参数面；`DamageInShape`（AOE 复合伤害）= SelectTargets+Damage 两步组合，独立会造形状参数双份漂移；`Timeline` 不立项（AbilityKit 内建 TimelinePhase 已停用是教训；帧对齐 = WaitUntil(AnimNotify 事件)，真需求再立项）。

**组合示例**：燃烧 DoT=ApplyState(燃烧,Period)；吸血=Damage 旗标→Buff 触发器（不占原语）；连锁闪电=Repeat{SelectTargets(最近+排除已命中)→Damage}；暴击爆炸=Branch(上次 Damage.IsCritical){SelectTargets(小圆)→Damage}。

## 4. 触发器行字段提案 v1（12 字段）

> 细节（每字段类型/语义/对应物）见 B 路报告 §7.1。**状态：提案，待用户拍板。**

`EventTag`（层级匹配事件键）/ `EventPayloadFilter`（参数预过滤，补 Tag 降维失去的参数维度——我方新增）/ `Conditions`（有序 AND 谓词链 + 期望布尔取反）/ `Effects`（FEffectStep 链引用，技能 Buff 同一词汇）/ `Priority`（大者先，声明序 tiebreak）/ `ExecutionGate`（Once/Cooldown/Repeat + Cooldown 记账走战斗泵 ScaledDt 时间轴，不碰墙钟）/ `InterruptPriority`（执行成功后压制低优先级行——补我方缺口）/ `Scope`（Buff 随 Remove 注销 / Unit 驻留——把示例层调和链收敛为枚举）/ `GateTags`（行级 Tag 门控，与修饰器同词汇）/ `HandlerClass`（特例触发器指定共享 Handler，缺省通用执行器）/ `Cues`（表现挂钩时机）/ `bConditionMissIsSilent`（联动 vs 记失败信号）。

**不抄**：RPN 表达式、Blackboard/NumericVarDomain、执行节点树双轨、Phase 分组（理由见 B 路 §7.1）。

## 5. 修饰器聚合：重算式维持（第三方实证关闭）

结论与采用清单（B 路 §7.2）：

1. **组合公式照抄**：`Override(终止) > (Base+AddSum)×PercentProduct×MulProduct`，组优先级 Override=0/Add=10/PercentAdd=15/Mul=20——顺序无关免去排序正确性争论。
2. **ModifierOp**：四内置 + Custom 扩展槽；不上 Divide/FinalAdd（AbilityKit 四种撑住 MOBA 全部 Buff）。
3. **重算细节**：脏标记懒重算 + 1e-5 阈值 Changed 事件 + scratch 缓冲零分配（避开其热路径 new 数组坑）；单条目缓存后置到测量后再加。
4. **失效传播 v1**：属性依赖图 + 写路径置脏两条；时变修饰器不走"内部读 ElapsedTime"形态（其 context 缺失即永不衰减的隐坑），走 Buff 生命周期到期/刷新重挂。
5. **键空间强类型隔离**：修饰器目标用 `FCombatAttributeId`，来源用 `FBuffHandle` 批量摘除——规避其 ModifierKey↔AttributeId 隐式共用整数空间无防护的坑。
6. **千人前瞻**：兵海层简化属性模型，重算式在武将层无瓶颈；增量式列为兵海层若需全功能属性时的再评估项。

## 6. 确定性/回放：自走棋最小形态建议（C 路，待定优先级后置）

- **定步长积累器**：`FixedStepReplayClock` 9 行为最小参照（变速=改 Speed，暂停=不喂 dt）；**引入判据**=逻辑帧与渲染帧解耦需求出现时，在 pump 外包一层，组件仍收逻辑 dt；届时护栏 1 升级为"逻辑 dt=定点 raw，ScaledDt 仅表现"。
- **回放最小录制集**：战斗头（TickRate+RandomSeed+内容/代码版本 hash）+ 每帧指令轨（自走棋输入稀疏，近零成本）+ 周期状态哈希轨；快照轨留调试 seek。回放=换输入源的重演，录制端不加执行器。
- **前置条件**：逻辑侧确定性清单先闭合（定点时间、种子化随机 xoroshiro128+ 式、稳定遍历序、dt 单点），否则回放只能"表演"不能"对账"。**"帧时钟确定 ≠ 全战斗确定"**。
- **对账工具**：保守 diff（同帧+非零哈希才可比）+ CLI 退出码进门禁。

## 7. 对裁决 3 的重要校准（预期落空与替代收获）

- **落空**：actioneditor 不是节点图编辑器——是 vendored NBC.ActionEditor 2.0.0 的 **IMGUI 自绘时间轴**（Track/Clip 四层轨道，全包 0 处 GraphView/NodeGraph）；pipeline 包**没有任何图编辑器**（Phase 编排靠代码构建）。"对照 Pipeline Phase 图现成实现"的预期不成立。
- **替代收获**：①NBC 时间轴是裁决 3 §5"管线图+时间轴"混合视图**时间轴半边的现成先例**（分区结构/Clip 拖拽/播放指针/按类型注册预览器）；②"Phase 壳 + Timeline 内容"桥接（阶段编排与轨道编排分层）是可借鉴结构；③**印证 K2 排除**（不碰蓝图 VM 也能做编辑器），且全自绘成本高反证 UEdGraph/SGraphPanel 复用路线划算；④"编辑器能画 ≠ 运行时能跑"（11 Clip 仅 2 个可导出）——我方步骤原语必须一次定全导出契约。
- 三层工具链分离（authoring 资产包/编辑器包/DTO 契约包）可借鉴——正好支撑"列表编辑器先行、图编辑器后置"的节奏。

## 8. 通用设计门禁（三路静默坑启示合并，进原语/触发器评审门）

1. **参数面 = 被消费面**：每声明一个参数/旗标必须指名管线内消费者，无消费者的参数不进 UPROPERTY（DamageFlags 11 个仅 1 个被消费的教训）。
2. **语义键强类型隔离**：修饰器目标/事件 Id/效果 Id 各用独立强类型，禁止 packed int 复用整数空间（ModifierKey 坑）。
3. **枚举判定用"包含"不用"相等"**：位标志组合值进相等分支 = Mixed 绕减免式静默漏洞；无测试覆盖的组合路径必补。
4. **参数一律结构化 struct**：位置参数上限是脆弱契约（arity>2 配置期可构造运行期才炸）；装载期完整性校验强制。
5. **订阅句柄统一持有**：由容器持有，禁止散落；注册/注销与 Buff Apply/Remove 成对闭合。
6. **时间轴单一来源**：Cooldown/Duration/周期记账一律走战斗泵 ScaledDt 时间轴（ AbilityKit 定时器链路墙钟驱动、Cooldown 依赖宿主刷新 ExecPolicy 的坑）。
7. **泵/遍历的增删延迟**：订阅表与 Tickable 注册表的增删延迟到循环后处理。
8. **环保险丝**：帧末泵抄 MaxFlushPasses 思路加轮数上限，超限日志+熔断该事件（不抛异常）。
9. **瞬时原语禁内含等待**：装载期校验器强制（AbilityKit Instant/Tick 双入口的强约束提示）。
10. **确定性是显式契约**：谓词/动作注册带 IsDeterministic 标记，回放模式下未标记调用即异常（自走棋回放待定项的前置基建）。
11. **清理按句柄成对执行**：禁"按 Tag 全清"式 API 进入 Buff 生命周期路径（TryRemoveAllRefs 教训）。
12. **文档纪律**：未实现的设计不得写成既成事实（AbilityKit EffectSource 1115 行文档 vs 零实现的系统性脱节教训）——我方设计文档已用"已定/待定"结构对齐此纪律。

## 9. 已定文档待定项状态更新

- 裁决 3 文档待定项「ability-kit-research Pipeline Phase 图对照调研」→ **已执行并校准**（§7）：预期落空，替代收获三条；该文档其余待定项不变。
- 事件分发文档待定项「事件分级清单」→ **升级为必答题**（§2 行 4）：下一轮应逐事件裁决同步/帧末。
- tick 泵文档待定项「定步长积累器」→ **有现成 9 行参照与引入判据**（§2 行 7/§6）：维持"需要时再定"，不需再调研。
- 载体讨论 §6.5 待定项「FEffectStep 原语集」→ **提案 v1 已出**（§3）：待拍板后关闭。

## 10. 决策状态

**已定**（2026-09-01，本篇为综合裁决）：
- 修饰器聚合维持重算式 + 采用清单（§5）——关闭载体讨论 §6.2 的悬置。
- 通用设计门禁 12 条（§8）——进入后续原语/触发器评审。
- 裁决 3 对照校准（§7）；确定性/回放最小形态建议（§6，优先级后置）。
- 条件表达式/执行节点树/Phase 分组不抄清单（§2 行 9、§4）。

**待拍板（提案 → 决策）**：
- [ ] **FEffectStep 原语集 17+3 清单**（§3）——拍板后即冻结原语命名空间，进入参数面细化。
- [ ] **触发器行 12 字段**（§4）——与原语集联合拍板（Effects 字段引用原语集）。
- [ ] 事件分级清单（同步立即 vs 帧末泵，逐事件裁决）。
- [ ] 中断/压制语义（InterruptPriority 采纳与否——建议随触发器表一并定）。

**未证实与假设（全局）**：三路报告均未运行构建/测试，"测试通过"皆转引；规模账（200 单位×5-10 Buff）未做 benchmark；层级匹配事件订阅在事件风暴下的性能无实证（AbilityKit 无对应数据）；提案类参数面（堆叠策略、Any 并行、矩/环形状等）无源码背书，均为设计提案。
