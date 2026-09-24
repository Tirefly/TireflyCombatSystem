# 04-module-effects.md — TcsEffect 效果执行层设计（v2 定稿重写）

- 日期：2026-09-02
- 状态：**v2 定稿**——全部增补（D4 修订/D7 瞬时流程/Authoring 边界/端到端走查/伪代码）已折入正文；修订记录见文末
- 职责一句话：**"事件 → 触发行 → 效果链"的触发执行引擎 + 原语解释器——系统做流转的流转本体。**

## 1. 模块边界

- 消费者：M3（生命周期事件被订阅）、M5（技能编排链）、M6（权威/镜像接线）、TcsDamage/TcsTargeting/TcsCue（领域步骤注册进执行器注册表）。
- 依赖：TcsCore、TcsAttribute。**注册制分派（D4-14）**：本模块是纯机制层（解释器+控制流步骤+执行器注册表+自注册宏），不依赖任何领域模块；Damage/Heal/ModifyFlow 步骤+执行器住 TcsDamage（09 文档）、SelectTargets 住 TcsTargeting、ApplyState 住 TcsState、PlayCue 住 TcsCue；反向禁止。

## 2. 类型词汇（对外）

### 2.1 链与步骤（D4-3 终版：15 原语）
- `FEffectStep`：`FInstancedStruct` 容器；步骤类型 = **15 个数据 struct（D4-16 终版）**，按归属分两半——**本模块 9**：控制流 6（WaitDelay/**WaitEvent**/Branch/Parallel/Repeat/RunSubChain）+ ModifyAttribute + SetVar + OnError；**领域模块自注册 6（D4-14）**：Damage/Heal/ModifyFlow（TcsDamage）、SelectTargets（TcsTargeting）、ApplyState（TcsState）、PlayCue（TcsCue）。修订史：WaitUntil→WaitEvent（事件回调挂起替代条件轮询）、Gate 并入开闸事件、ModifyFlow 新增（提交流程属性修正，见 09 文档）、SpawnProjectile/SpawnArea 移除（D4-16）。
- `FEffectChain`：有序步骤数组 + 元数据（`MaxStepsPerFrame` 熔断上限，默认 CVar 可调）。
- `FEffectContext`（黑板，可池化）：`Caster / Instigator / EventPayload(FInstancedStruct) / Targets / Variables / **CapturedAttrs** / 注入引用（ICombatEntityQuery/IRelationResolver 等宿主能力契约，链构建时装配）`——黑板即上下文（与 09 文档同构）。
- **原语集合扩展 = 新增 step struct（Authoring 第三层）**；Custom 逃逸位规约管策略枚举，不管类型族。

### 2.2 触发行（D4-1 终定 10 字段）
`EventTag / EventPayloadFilter / Conditions / Effects / Priority / ExecutionGate / InterruptPriority / GateTags / Cues(CueId 引用) / bConditionMissIsSilent`。
- 全部独立可空、零策略类；CueId 是引用（Niagara/贴花/MPC 参数映射住 TcsCue/宿主）；~~Scope~~~~HandlerClass~~ 已砍除（目标归属 = SelectTargets 步骤显式解析或 Context 默认目标——D4-4 v2；求值器本身就是共享 Handler）。
- **条件最小集（D4-5 已拍板）**：`HasAllTags / AttributeCompare(Attr,Op,Value|Attr) / VariableCompare / GateCheck(读 BoolSwitches) / Chance(概率)` + Custom 逃逸位——无表达式语言。

### 2.3 目标选择（D4-4 终定；**v2 策略化**——D4-4 v2/D4-15 v2，规格详见 10-module-targeting.md）
- **策略模式（D4-4 v2，2026-09-02 审阅轮 5 后用户拍板；载体经 D3-7 v3 修订）**：`FTcsTargetSelectorStrategy` / `FTcsTargetFilterStrategy` USTRUCT 纯虚基类 + **裸 `FInstancedStruct`** 持有（2026-09-24 换型；StateTree 同构，规格见 10 文档 v3）；默认实现 Self/EventTarget；Filter 语义宿主实现（存活/敌对=宿主本体论）；RadiusArea/FTargetingShape 后置（竖切无消费者）。
- **战斗步骤不内嵌 selector（D4-4 v2 改口）**：目标消费 `Context.Targets`——**Context 默认目标初始化=事件目标**（单步链零 SelectTargets 直接消费）；"一个技能多效果、各有目标"用顺序 SelectTargets 步骤表达（目标集=链上显式数据流）。
- **归属（D4-15）**：策略契约+默认实现+SelectTargets 执行器住 TcsTargeting 模块（实体查询注入 ICombatEntityQuery）；指示器渲染归宿主/LAC。
- §9 走查样例中的 RadiusArea 等为**全量预演形态**（M5 世界）；R3 竖切目标选择形态见 10 文档 v2 验收钩子。

### 2.4 异步语义（2026-09-02 定案）
- **挂起-恢复协议**：`FChainRun`（池化）保存 PC/上下文/Repeat 游标/JoinCount——控制流状态不活在调用栈里；挂起类步骤让出控制权，唤醒源带句柄回来代际校验后从 PC 继续。
- **步内挂起（协议扩展，D4-17）**：执行器返回 `EStepResult{Completed, Running}`——Running 时 FChainRun 停在原 PC 每帧重入（脚本执行器/长步骤承载，状态存脚本侧），完成即续走；与 PC 挂起/事件订阅并列为第三种挂起形态。
- **四种唤醒源**：①到期堆（WaitDelay）；②事件匹配（WaitEvent：一次性订阅，命中即退订，Payload 写入 `LastEvent`）；③子链完成（RunSubChain 默认等待，bWait=false 放支线）；④开闸事件（GateTags 开闸发事件，WaitEvent 语法糖）。
- **Parallel**：默认不汇合（父继续）；`bJoin=true` 等全部子链完成（JoinCount 计数）。

## 3. 入口服务与关键机制

- `UCombatEffectSubsystem`：`RegisterTriggerRows(定义集)`（定义加载期，行句柄与总线订阅配对，来源注销自动退订）；`ExecuteChain(ChainId, Context) -> FChainRunHandle`；**执行器注册表（D4-14/17）**——领域步骤执行器经 `UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册（C++ 宏）或反射可达动态委托入口登记，本模块对步骤类型零硬编码 switch。
- **触发求值器**（总线共享 Handler，裁决 2a）：事件 → 匹配行（Priority 序）→ ExecutionGate（权威侧）→ GateTags → Conditions → 起链；条件未过按 `bConditionMissIsSilent` 决定是否写 Explain 线索（M8 数据源）。
- **解释器**：即时步骤同步执行；WaitDelay 注册到期堆；WaitEvent 订阅总线；RunSubChain 默认等待（子链完成唤醒父）；Repeat 带单链单帧熔断（AbilityKit 教训对症解）；Parallel 子上下文。
- **打断**：InterruptPriority 与运行中链比较 → Cancelled → 堆条目惰性失效 → 止于未来（已产生副作用不回滚——M5 打断复用此约定）。
- **OnError**：软失败 → OnError 步骤接管（降级/补偿），默认断链 + 日志 + Explain 线索。
- **起链解析（D5-7 三粒度让渡点之一）**：链 Id 解析 = 全局定义 → 重定向栈（状态/装备声明的 `FChainRedirect`，后挂/高优先）→ Custom Fragment；重定向栈解析经注入接口 `IChainRedirectResolver`（宿主/上层提供——D4-14 反向依赖击穿的既有接口之一）；Source 级联回收；绝不修改共享 Def。

## 4. Damage/Heal 原语：流程发起器（D7-2 终定）

- 原语**不算公式**——构造 `FDamageFlowContext`（黑板：攻击者/目标/分类 Tag/公式参数初值/流程属性黑板/FlowSource）→ `TcsDamage.DamageFlow.Run(模板)`（**流程=数据模板，D7-5**：默认模板=标准阶段组装 CollectStart→…→Completed，宿主可自定义模板/步骤/数据步骤；**插件零公式，公式=项目 IDamageFlowDelegate 或宿主自定义步骤**；FDamageRecord 记录流=回放/统计钩子）。完整规格见 09 文档。**Damage/Heal 步骤类型与执行器住 TcsDamage（D4-14）**——本节描述的是系统行为，不是模块归属。
- `ModifyFlow` 原语：提交流程属性修正（配合触发行订阅 Damage.Pre 等收集事件，实现"目标受火伤+20%"类修改器）。
- 瞬时角色属性修改（"本次攻击攻击力+10%"）双路径：属性已捕获 → 改 Context；未捕获 → FlowSource 临时修正器（09 文档 AttrCapture 节）。

## 5. Authoring 边界与代码路径（分离宪法 R0 §8 落地）

- **链的修改三通道**：①开启分支 = BoolSwitches/参数 → 链内 Branch/GateCheck；②幅度 = 字段写成 Param() → 参数账本；③结构变化 = 变体链（策划预创作）+ 链重定向选路。**不做运行时步骤级补丁**（Insert/Remove/PatchField）——链是 Const 共享数据，补丁合并语义是 bug 温床。
- **Authoring 纪律**：可预见会被修改的字段，创作时写成 ParamRef 源（载体 = **FTcsParamValue{FInstancedStruct}**，PV 系列 2026-09-11 取代 D2-12 FTcsParamScalar（载体 2026-09-24 换裸））——参数覆盖度是策划的创作自由度决策。
- **语言无关执行器（D4-17 终定）**：插件**不内嵌任何脚本引擎**（AngelScript/C#/TS 是宿主选择，插件不关注）；注册**双入口**——C++ 静态自注册宏 + **反射可达动态委托入口**（脚本层调用同一注册表）；蓝图理论可行（动态委托）不作为设计目标；**R3 纯 C++、无脚本集成**。
- **Skill Logic 需要代码的四条路径**（代码技能是一等公民）：①Custom 原语类型；②决策 Fragment；③链外代码 + 事件协作；④整技能代码化（单 Custom 步骤链，账本/冷却/门禁/打断照常）。框架不强制策划化。

## 6. 网络姿态落点（NET-1/2）

- 链执行**权威侧全量**；客户端镜像只跑 Cue 类步骤（复制的触发事件驱动，CueId+上下文快照）——接口位本期不实现；ExecutionGate 字段 = 策划可见的网络闸。

## 7. 非目标

不做 Timeline（曲线参数留活口）；不做图编辑器（M8 只读视图先行）；不做条件表达式语言；不做公式；**不内嵌任何脚本引擎**（宿主选择 AS/C#/TS）。

**脚本调用层（2026-09-24 更新）**：门面 API 的**反射面已落地**（提案 `add-scripting-reflection-surface`，PIE 实测通过）——`UTcsEffectSubsystem` 的链登记/触发登记/执行查询/点灯等门面方法、以及 `UTcsAttributeSubsystem` 的实体注册与属性读写，均以 `UFUNCTION()`（**无 specifier**）标记，宿主脚本层（UnrealSharp/C#）可直接调用。

- **无 specifier 是有意的**：形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等 `USTRUCT()` 非 `BlueprintType` 载体，加 `BlueprintCallable` 会被 UHT 的蓝图参数校验拒绝；无 specifier 时 UHT 不校验参数、脚本层照常可达。
- **蓝图侧仍不承诺**（R0 §9）：本批标记**不**扩大蓝图承诺面——这是"语言无关执行器"预留的兑现，不是蓝图支持。
- **仍未落地**：三张注册表的**反射注册入口**（`Register` 形参是 `TFunction`，不可反射——台账 S-2）；上下文/运行态反射化（台账 S-3，含 `FTcsDamageFlowContext` 深处 `TFunction` 的物理约束）；`ITcsEntityQuery` / `ITcsDamageFlowDelegate` 反射化（台账 S-5 / S-4）。**（台账 S-6 已消费：`TInstancedStruct<T>` 字段的脚本侧可配性已由 2026-09-24 换型解决）**

## 8. 依据

- 拍板：D4-1~D4-4 + D4-5 + D7-1~D7-4（2026-09-02 多轮问答框；用户贡献：Scope/HandlerClass 砍除、Cues 引用制、目标选择下沉每步骤、WaitEvent 提案、EntrySelector 纠正、Authoring 边界确认）+ Authoring 三层模型 + 分离宪法 R0 §8。
- 证据：TCS 09 核验（无触发行/无链/无前摇后摇——净新增；SourceHandle 回收链继承）；AbilityKit Pipeline/Triggering 对照；启发文档《伤害修改器设计》全文。

## 9. 端到端走查样例（2026-09-02）

总纲：策划写的是三张表的数据行（状态词表行/触发行/链）；运行时自动性来自三机制：生命周期与时钟泵按声明发事件、触发行求值器自动匹配、解释器按数据执行。定义加载期把触发行订阅挂上总线，之后事件驱动闭环，无策划调用。

### 例一 Buff「灼烧」（每 2s 对宿主 5% 最大生命火伤，10s，叠 3 层）
- 配置：StateDef_Burn（五轴 GroupBy=PerSource/Capacity→MaxStacks=3/RejectNew/AddValues/RefreshToTotal + Duration 10s + Period 2s）；触发行 TR_Burn_Tick（EventTag=`Tcs.Event.State.Periodic` + Filter DefTag=`<Burn 状态 tag>` + Effects=Chain_BurnTick + Cues）；链 Chain_BurnTick（单步 Damage{目标=EventTarget, 量=Param(BurnPct)×MaxHealth}）。**标识 2026-09-22 tag 化**：触发行 Filter 与链 id 均为 `FGameplayTag`（本节为走查预演形态，示意名不写全 tag 路径）。
- 运行：ApplyState（关系表→五轴→池分配→**快照构建**→时长/周期条目进到期堆→OnStateApplied 立即）→ 泵 2s 到期发 Periodic → 求值器四道门 → 起链 → Damage 步骤读 BurnPct（修正链）与 MaxHealth（M2 惰性重算）→ M2 事务扣血 → 提交尾 flush → Health 事件（立即）→ 死亡判定在宿主属性条件规则。10s 到 → Expire → 周期条目摘除 → 级联重评 → OnStateRemoved；悬空链醒后代际校验失效即取消（止于未来）。Cue 走帧末。

### 例二 Skill「旋风斩」（前摇 0.4s 可打断，4m 内 120% 攻击力，冷却 5s 挥砍组共享，消耗 20 怒气）
- 配置：SkillDef_Whirlwind（NumericParameters{DmgCoeff 1.2, Radius 400, Cost 20} + BoolSwitches + 时段表[{WindUp, 0.4, 可打断}] + 冷却共享组轨 5s/OnCastStarted + ECastInstancing=InstancePerEntity + CostConfig{ResourceAttr=Attr_Rage, CostParam=Cost, Timing=OnCastStarted} + CastChainId + MainChainStart=OnCastCompleted）；链 Chain_Whirlwind（SelectTargets{RadiusArea, 敌对存活} → Damage）；表现触发行订阅 Cast.Completed。
- 运行：宿主输入调 TryActivate → 六道门禁逐道（冷却组查/Instancing 顶替/CanAfford）→ FCastRun + **ParamSnapshot 构建** → OnCastStarted → CDR 快照 + 5s 冷却进堆 → WindUp 0.4s 进堆（打断=来源优先级 vs IsInterruptibleNow 查询契约）→ 到期 → MainChainStart=OnCastCompleted → 起链解析（重定向栈→全局）→ SelectTargets（RadiusArea 遍历中央注册表+位置过滤）→ Damage（委托 TcsDamage 骨架→项目公式 delegate→逐目标 M2 事务）→ flush → 击杀可连锁。冷却走 D5-16 三事件（Started/Updated/Ended）。

## 10. 核心伪代码索引（2026-09-02）

七段：A 触发行求值器（Tag 路由→四道门→条件 switch+Custom→起链）；B 链解释器（步骤分派/挂起到期堆/单帧熔断/代际唤醒；修订后 WaitEvent=事件一次性订阅挂起）；C Damage 步骤（委托 TcsDamage 骨架，公式在项目 delegate）；D SelectTargets（RadiusArea 遍历注册表+宿主注入 IRelationResolver）；E TryActivate 六道门禁；F 时段驱动+MainChainStart 起链时机；G Param 解析（字面量或 M5 账本快照/Live）。

共性：每台机器=封闭 switch，Custom 值（=1）分支接决策 Fragment（槽位规约：Custom 不放末位）；新增语义=加枚举值/新 step struct（Authoring 第三层）；宿主注入点=IRelationResolver.IsHostile（阵营）、死亡规则（Health 订阅，M2 零词汇）。

实现基线 = plan2 各任务 sketch（Task 1 解释器/Task 4 步骤库）；实现期与本节索引冲突时，以 plan sketch + 当时拍板为准。

## 11. 修订记录

- v1（2026-09-02）：初版决策折入（原语 17+3 前身、触发行 12 字段）。
- v2 增补（2026-09-02）：触发行 10 字段（砍 Scope/HandlerClass、Cues 引用制）、WaitEvent/Gate 并入、ModifyFlow 新增（17 原语）、异步四唤醒源协议、Authoring 边界与代码路径、Damage/Heal 委托化（TcsDamage）、D4-5 条件含 GateCheck。
- v2 定稿重写（2026-09-02）：全部增补折入正文（本文），端到端走查与伪代码索引保留为 §9/§10。
- v2 增补 2（2026-09-02，M9 追问轮同步）：折入 D4-14~16（注册制分派+依赖层级反转——依赖收窄为 Core/Attribute；15 原语终版与步骤归属；TcsTargeting；Spawn 残留清理）；§9 例二冷却表述对齐 D5-15/16。
- v2 增补 3（2026-09-23，标识体系 tag 化改造回写）：§9 例一触发行 Filter 与链 id 改 tag 口径（原 `Filter DefId=Burn`）；本节为走查预演形态，示意名不写全 tag 路径。落点 = 提案 `switch-identifiers-to-gameplay-tags`（2026-09-22 归档）。

## 12. 验收钩子

竖切验收：测试链 `WaitDelay → SelectTargets(单体策略) → Damage` 走 TcsDamage 骨架（10 文档 v2 验收钩子——§9 例二为全量预演形态）；熔断与打断、WaitEvent 挂起唤醒、重定向挂/摘在竖切后人工检查路径。
