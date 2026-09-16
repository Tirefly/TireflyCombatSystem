# AbilityKit 对照提取 B 路：触发器（Triggering）与属性修饰器（Attributes/Modifiers）

> 性质：对照调研提取（B 路：触发器与属性修饰器）→ 服务我方"Buff=触发器表（Event×Condition×Effect）+修饰器聚合"设计提案。
> 日期：2026-09-01 · 输入：ability-kit-research 技能 04/02 号报告全文、01 号报告事件/池/定时器/确定性小节、09 号报告坑证据；vendored 源码复核 ECompareOp/ETriggerExecutionMode/TriggerExecutionControlPlan 三处。
> 证据标注：沿用调研报告语义——【源码】=快照源码直接证实（附相对路径，默认省略 `Unity/Packages/com.abilitykit.triggering/Runtime/`，02 号报告路径相对仓库根）；【文档】=仓库内文档转引；【推断】/【未证实】=需复核。标注【源码·04§x】表示该结论由 04 号报告源码取证、本文转引，其余同理；【源码·本次复核】=本文在 vendored 源码亲验。
> 能力边界：AbilityKit 快照自述开发期（04§1），"存在实现"≠"生产就绪"。

## 0 TL;DR

AbilityKit 触发器与我方模型同构度高：`TriggerPlan` 是不可变计划（Phase/Priority/条件/动作/执行控制），`TriggerRunner` 纯事件驱动求值，条件三态（无/注册表函数/RPN 表达式），修饰器是纯计算内核（Add/PercentAdd/Mul/Override 分组公式 + 脏标记懒重算 + 属性依赖图）。与我方三处形态差异：① 默认 Immediate 同步直派而非帧末泵（Queued 为可选、Flush 环保护 MaxFlushPasses=1024）；② 逻辑住"委托注册表 + 复合键"而非共享 Handler UClass；③ 条件带 RPN 表达式与黑板/NumericVarDomain 服务面——自走棋规模用不上，是我方应克制的地方。两个已知坑原委清楚：`DamageType.Mixed` 因分支按"相等"而非"包含"判定而绕过减免；`ModifierKey`↔`AttributeId` 隐式共用整数空间且无防护代码。提案 v1：触发器行 **12 字段**，修饰器维持重算式并抄其分组公式。

## 1 我方模型一句话回顾

Buff = DefAsset 声明的触发器表（Event Tag × Condition × EffectChain）+ Tag 门控修饰器列表与值变更重算；事件 = 总线 + 订阅表（Tag 层级匹配，共享 Handler UClass，句柄校验后直调）；帧末泵防重入（事件分级：查询类同步、状态类帧末统一泵）（我方事件分发文档 §2-§5、tick 泵文档 §6）。

## 2 Trigger Plan 结构（数据面）

### 2.1 四层模型

计划模型层 `TriggerPlan<TArgs>`（不可变 readonly struct）→ 运行时层 `TriggerRunner<TCtx>`（订阅/排序/评估/执行）→ 装载层 `TriggerPlanJsonDatabase`（JSON 双格式）→ 领域协作层（MOBA 示例把计划接到技能/Buff/投射物）【文档·04§1.1，源码目录吻合】。包内零业务词汇：Buff/子弹/AOE 概念全部由项目层动作/条件/上下文接入【源码·04§1.2】——与我方"类做词汇、struct 做状态"同向。

### 2.2 TriggerPlan 字段清单（对照我方触发器行的直接输入）

【源码·04§3.2：`Plans/Model/TriggerPlan.cs:12-267`】

| 字段 | 语义 |
|---|---|
| `Phase` | 派发内分组，小者先执行 |
| `Priority` | 同 Phase 内排序，**大者先** |
| `TriggerId` | 稳定标识（调度分桶/直连触发的键） |
| `InterruptPriority` | Execute 成功后自动 `StopBelowPriority`，0=不自动打断 |
| `PredicateKind` | None / Function / Expr 三态 |
| `PredicateId` + `PredicateArg0/1` | 函数条件：最多 2 个位置参数 |
| `PredicateExpr` | 表达式条件：RPN 布尔节点数组 |
| `Actions[]` | `ActionCallPlan` 列表 |
| `Cue` | 表现回调描述 |
| `Schedule` | 计划内调度域 |
| `ExecutionControl` | `ETriggerExecutionMode`（Always/Once/Cooldown/Repeat）+ MaxExecutions + CooldownMs【源码·本次复核：`Plans/Model/TriggerExecutionControlPlan.cs:3-26`】 |

计划不可变：`AddActions`/`AsArgs<T>` 返回新副本，改字段=换实例【源码·04§3.2】。

### 2.3 事件源定义

- 事件键 `EventKey<TArgs>`：`(稳定 int Id, typeof(TArgs))` 复合键，同 Id 不同参数类型是不同通道【源码·01§3.1】。
- 稳定字符串→int：`StableStringId` 用 FNV-1a-32 UTF-16 哈希，碰撞抛异常【源码·04§4.1：`Events/StableStringId.cs:12-32`】；codegen 产物 `GeneratedIdNames.RegisterAll` 批量登记 Id↔名称双向表供诊断回显【源码·04§3.2】。
- 对照我方：我方用 GameplayTag 层级匹配做订阅降维（`Event.Combat.*` 命中一族）；AbilityKit 是"强类型通道 + 精确 Id"，无层级匹配，订阅粒度靠注册时逐 key 挂接【源码·04§4.2】。**两种降维互有得失**：Tag 层级换来订阅表容量收益，但丢掉"按参数类型分通道"的编译期检查——提案 v1 以 Tag 为主、参数类型放 PayloadFilter（§7）。

### 2.4 动作挂接与调度

- `ActionCallPlan`：位置参数 `Arg0/Arg1`（兼容）+ 具名参数字典双形态；`ScheduleMode`（Immediate/Delayed/Periodic/Continuous/Timeline）、`MaxExecutions`（-1 无限）、`CanBeInterrupted`、`ExecutionPolicy`（含 Retry）、行为级 Cue【源码·04§3.2：`Plans/Model/ActionCallPlan.cs:61-302`】。
- 动作注册表：`ActionRegistry` 以 `(ActionId, 委托类型)` 复合键检索 + `IsDeterministic` 标记；首次执行前一次性解析为委托槽位缓存【源码·04§4.6】。
- 计划内调度：非 Immediate 项按 TriggerId 分桶注册进 `ActionScheduler`（同槽位单活跃实例）；`ActionSchedulerManager.Update` **由宿主每帧推进，框架不自动驱动**【源码·04§4.7】。
- 与我方差异：我方效果链 = FEffectStep 序列（技能/Buff 共用，协程执行器推进）；AbilityKit 动作是平面列表 + 独立调度器 + 另一套可选"执行节点树"（Sequence/Selector/If/Repeat…，与 Actions 并存，执行优先级由领域执行器决定）【源码·04§4.8】——双轨并存的复杂度是反面教材，我方单轨效果链更干净。

### 2.5 装载与校验（反面清单）

JSON 双格式（运行时格式/源格式）；装载已知的三个坑：`LoadFromDto` 不重建 `_recordsByTriggerId` 派生索引（校验器拿到空值）；重复 TriggerId 字典静默覆盖而列表保留；`RegisterAll` 签名 void 丢弃可撤销 handle → 重复装载累积监听【源码·04§4.9/§8】。七类专项校验器（Reference/CycleDetector/UgcLimits…）存在，但 arity>2 的动作"配置期可构造、运行期才炸"仍是契约裂缝【源码·04§8.6】。

## 3 TriggerRunner/ExecCtx 执行模型

### 3.1 求值时机：事件驱动，无轮询

规则只在事件到达时求值（Register → `IEventBus.Subscribe`）；持续行为（光环/被动/周期）不靠 Runner 隐式持有状态，而是"组件存储 + 系统显式调和"【文档·04§1.2：`02-TriggeringSystem.md:425`】；周期/延迟动作经 ActionScheduler 由宿主 Update 推进（§2.4）。MOBA 侧持续订阅调和链：`OngoingTriggerPlansComponent(Revision)` → ReconcileSystem → SubscriptionService（按 ownerKey 持有 `Dictionary<int, IDisposable>` 注册表 + stale 清理）→ Runner【源码·04§4.10】。**对照我方**：同构（事件驱动 + 生命周期闭合），但 AbilityKit 的调和链要靠示例层自建 Revision 对账，我方 Buff Apply/Remove 直接同步注册/注销订阅（事件分发文档 §5 纪律 1）——更简单且无对账窗口。

### 3.2 排序与优先级

`TriggerRunnerEntryList.InsertSorted` 线性插入：Phase ↑ → 同 Phase Priority ↓ → 同优先级注册序早者先【源码·04§4.2：`TriggerRunnerEntry.cs:28-45`】。事件通道内 Handler 按注册 order 插入排序，通道层唯一短路条件是 `control.IsHardStopped`——优先级软过滤**完全下放给 Runner**【源码·04§4.1：`EventChannel.cs:84-88`】。注册返回 `IDisposable`，Dispose 摘除并按需反订阅【源码·04§4.2】——句柄式注销与我方订阅表一致。

### 3.3 中断语义（我方没有的一层）

【源码·04§3.2/§4.3：`Core/Execution/ExecutionControl.cs`】

- **硬停止**：`StopPropagation || Cancel || InterruptMode==All` → 全拦截并终止通道派发。
- **软中断**：`StopBelowPriority(targetPriority)` / BelowOrEqual → 后续条目按 `ShouldBlock(phase, priority)` 跳过（Cue OnSkipped）。
- **自动压制**：计划 `InterruptPriority` 非零时，动作执行完在 finally 后调 `StopBelowPriority`——"高优先级触发器得手后压制低优先级"（如"死亡触发打断后续掉落触发"）。
- **动作拒绝**：`RejectAction(reason)` 保留首个原因，只短路当前计划不杀通道。
- **Strict 策略**：Runner 构造可选，条件失败即短路整个派发（默认只跳过当前条目）。

### 3.4 ExecCtx 携带什么

readonly struct，13 槽：项目 `Context(TCtx)`、`EventBus`、`Functions`、`Actions`、`Blackboards`、`Payloads`、`IdNames`、`NumericDomains`、`NumericFunctions`、`Policy`（RequireDeterministic + Delta/TotalTimeMs）、`Control`、`ActionSchedulerManager`、`StronglyTypedPayloads`（主线恒 null，半落地状态）【源码·04§3.2：`ExecCtx.cs:11-108`】。每次派发重建：控制对象 Reset 复用，上下文从 `ITriggerContextSource` 取【源码·04§4.3】。对照我方 Handler 直调签名 `Execute(实例视图, 事件)`：AbilityKit 把"服务面"全部装进上下文对象，我方摊开在词汇类/容器 API——我方形态更薄，但 `Control`（中断状态）与 `Policy`（确定性门禁）两个槽位值得在提案中留位。

### 3.5 重入防护：与我方"帧末泵"的正面差异

- EventBus 双模式：**默认 Immediate 零缓冲同步直派**；Queued 需显式构造并配 Flush 驱动【源码·04§4.1：`EventBusOptions.cs`】。Flush 多轮清空直至无 pending，超过 `MaxFlushPasses`（默认 1024）抛 "Possible infinite event loop"——事件环保护【源码·04§4.1：`EventBus.cs:88-107`】。
- **坑**：Dispatch 遍历实时列表（无快照），回调中注册/注销改变同一次派发的可见性，文档要求隔离时应用层自复制【文档·04§8.4：`TriggerRunner.cs:164`】。
- **坑**：Cooldown 用 `ctx.Policy.TotalTimeMs`，Runner 不更新时间，宿主必须按派发批次刷新 ExecPolicy 否则失真【推断·04§8.8】。
- 对照我方：我方"状态类事件帧末统一泵 + 连锁新事件下一轮"是**结构性切断重入**；AbilityKit 默认同步直派、以环计数器兜底——我方更严格，代价是跨单位联动天然延迟一拍（自走棋可接受，AbilityKit 为动作游戏选了 Immediate）。**核实点**：我方事件分级清单（哪些同步哪些帧末）尚未裁决（事件分发文档 §9 待定），AbilityKit 的"查询类立即/状态类排队"思路与我方一致。

### 3.6 可观测四通道

Lifecycle 16 钩子 / Observer（携带 ExecCtx）/ Cue 两层（Trigger/Behavior，`NullTriggerCue` 单例早退零开销）/ Tracer（Evaluate/Execute/ShortCircuit 三类记录）【源码·04§4.11】。默认全关零成本——我方调试 overlay 单点插桩（事件分发文档 §8）可按此"空实现早退"模式做。

## 4 条件组合表达

### 4.1 三态与函数条件

`PredicateKind`：None（恒真）/ Function（`PredicateId` 查 `FunctionRegistry`，arity 0/1/2，其他抛异常）/ Expr（RPN 栈机）【源码·04§4.4】。函数条件回退顺序：先精确 `Predicate1<TArgs,TCtx>` 再 `Predicate1<object,TCtx>`【源码·04§4.4】。注册表带 `IsDeterministic`，`Policy.RequireDeterministic` 之下调用未标记函数直接抛异常——确定性是显式契约而非自觉【源码·04§4.4】。

### 4.2 表达式条件（RPN 栈机）

节点 Kind = Const/Not/And/Or/CompareNumeric/Function；≤64 节点 stackalloc、更长租 ArrayPool；求值结束栈深必须为 1【源码·04§4.4】。数值比较 6 算子【源码·本次复核：`Plans/Execution/PlannedTriggerPredicateEvaluator.cs:286-299`】：`Equal / NotEqual / GreaterThan / GreaterThanOrEqual / LessThan / LessThanOrEqual`。

### 4.3 值解析（数值参数化的通用面）

`NumericValueRef` 五源：Const → Blackboard（带 CanRead 校验）→ PayloadField → Var（NumericDomain）→ Expr（RPN 缓存编译）；统一策略修饰 Scale → Offset → Min/Max clamp；失败语义 Fallback/Required【源码·04§4.5】。参数四种 Kind：NumericValue/BlackboardTarget/BooleanValue/StringValue【源码·04§3.2】。

### 4.4 对我方条件词汇的建议

1. **条件 = 纯谓词**（bool 输出、无副作用），我方词汇类方法签名即满足；谓词参数用结构化 struct（`FConditionParams`）而非位置 int——AbilityKit arity>2"配置期可构造运行期才炸"的裂缝证明位置参数上限是脆弱契约（§2.5）。
2. **组合**：v1 用有序 AND 条件链（数组顺序即评估序）；Or/Not 由 v1 的"谓词 + 期望布尔取反"覆盖大半，复杂布尔推 v2 再加表达式层。RPN 栈机 + 黑板 + NumericVarDomain 是为"策划写任意逻辑"准备的服务面，自走棋 Buff 条件（血量阈值/Tag 状态/来源判定/概率）用谓词函数足够，**不抄**。
3. **抄 `IsDeterministic` 门禁**：谓词/动作注册时带确定性标记，回放/帧同步模式下未标记调用抛异常——正好服务自走棋回放确定性（tick 泵文档 §9 待定的定步长积累器未来受益）。
4. **抄值解析的"策略修饰"思想但简化**：效果数值在我方 FEffectStep 内已有 Clamp/Scale 需求时，挂-step 级修饰字段即可，不引入五源引用体系。

## 5 attributes/modifiers 聚合模型与两个坑

### 5.1 三层栈

modifiers（纯计算内核，asmdef `noEngineReferences:true`，零 Unity 依赖）→ attributes（属性引擎）→ gameplaytags（状态语义层）【源码·02§1.1/§1.3】。modifiers 不管存储、按 SourceId 批量移除、生命周期——全部归业务层【文档·02§1.1】。三列消费共享同一 `ModifierData/MagnitudeSource/ModifierCalculator` 内核，差异只在存储归谁（属性槽位/标签计数表/参数字典）【源码·02§1.4】——"计算内核与存储分离"正是我方"修饰器聚合 + 容器持有"的先例。

### 5.2 属性定义与实例

- 定义：`AttributeDef(Name/Group/DefaultBaseValue/Formula/Constraint/DependsOn)` 注册进 `AttributeRegistry` → `Freeze()` 建依赖图 + DFS 环检测（环抛异常）；冻结后注册/隐式创建抛异常【源码·02§3.2/§5.2】。`AttributeId` = 注册顺序下标，仅"同一注册表 + 相同注册顺序"内稳定【文档·02§1.2】。
- 实例：`AttributeGroup` 持 `AttributeSlot{BaseValue,Cached,Dirty}` 并行数组；`AttributeInstance` 持修改器槽位自由链表（`ModifierSlot{Handle,ModifierData,NextFree,Active}`）+ handle→槽位索引【源码·02§3.2】。

### 5.3 修饰器类型全集与组合公式

- `ModifierOp`：`Add=0 / Mul=1 / Override=2 / PercentAdd=3 / Custom=100+`【源码·02§3.3】。
- 组合策略按 `IModifierOperator.Priority` 分组：**Override(0, 终止) > Add(10) > PercentAdd(15) > Mul(20)**；`FinalValue = Override ? OverrideValue : (Base+AddSum) × PercentProduct × MulProduct`（PercentProduct 每项 `1+v`）；分组累积数学上可交换 → **顺序无关**；Override 无论位置都获胜【源码·02§4.5：`OperatorComposer.cs:104-159`】。
- 数值来源 `MagnitudeSource` 六类：Fixed/Scalable/Attribute/TimeDecay/Pipeline/ContextFloat；TimeDecay 按 `ElapsedTime/Duration` 归一化衰减【源码·02§3.3/§4.5】。
- 来源追踪：组合完成后按 Op 近似计算每条修饰器贡献写入 Recorder（Add=modValue、Mul=addBase×(v-1) 等）【源码·02§4.5】——调试面板"这条 Buff 贡献了多少"的现成公式。

### 5.4 堆叠与查询时机

- 堆叠：`ModifierStacking` 仅提供 Exclusive（同源替换）/Aggregate（层数叠加）计算逻辑，存储归业务【源码·02§3.3】——与我方"堆叠策略进 Buff DefAsset"同构。
- 查询时机：**脏标记懒重算**——读 `Value` 时脏才 `Recompute()`；`BaseValue` 变化阈值 1e-5 才置脏；`Changed` 事件同阈值【源码·02§4.4：`AttributeInstance.cs:232-262`】。
- 失效传播三条路：① 属性依赖图（Freeze 时建好，上游 Changed → 下游 MarkDirty）；② `ContextFloat` 键精确匹配置脏；③ 本属性写路径直接置脏 + 计算器失效【源码·02§4.4】。
- 缓存：`ModifierCache` 四重命中（修饰器数量 + 全量字段哈希 + BaseValue + 时变时比对 CurrentTime）；Recorder 非空强制绕过【源码·02§4.5】。

### 5.5 坑一原委：`DamageType.Mixed` 绕过减免

`DamageType` 是 `[Flags]`（Physical/Magic/True/Mixed=Physical|Magic）；护甲处理器首行 `if (input.DamageType != DamageType.Physical) return result;`（`DamageProcessors.cs:281`），魔抗处理器首行 `!= Magic` return（`:330`），基础伤害加成分支也只处理 Physical/Magic（`:217-226`）→ `Mixed` 两个相等判定都不满足 → **无减免、无攻击加成**，静默通过【源码·09§4.3/§8.2】。另 `DamageFlags` 11 个中仅 Critical 被内置 8 处理器消费，穿透等语义实际由 Dataflow 槽位承载【源码·09§3/§8.2】。**根因**：位标志组合值进了"相等"分支判定而非"包含"判定；消费端枚举语义与定义端不一致。对通用设计的启示见 §8.1。

### 5.6 坑二原委：ModifierKey↔AttributeId 隐式共用整数空间

`AttributeInstance` 把自身 `AttributeId.Id` 直接打包成 `ModifierKey.FromPacked((uint)id.Id)`，`AttributeContext.FindAttributeIdByKey` 反向用 `(int)key.Packed` 还原——**属性引用类修饰器（MagnitudeSourceType.Attribute）得以工作的前提就是这两个整数空间相同**【源码·02§4.6：`AttributeInstance.cs:52`、`AttributeContext.cs:365-369`】。但 `ModifierKey.Packed` 自有位布局 `[Reserved:8][Custom:8][SubCategory:8][Category:8]`，MOBA 又用 Categories（Skill=40/Projectile=30/AOE=31）定义参数键【源码·02§3.3/§4.6】——业务自定义分类键若与属性注册顺序 Id 撞车，`GetAttribute` 会错拿属性，**无防护代码**【源码·02§8.1.4；风险判定为推断】。启示见 §8.2。

### 5.7 重算式 vs 增量式的第三方实证

AbilityKit 是完整的**重算式**参照：脏标记懒重算 + 分组公式 + 单条目缓存 + 依赖图传播；它没有做 GAS 式"聚合器维护增量 sum"。其已知代价都在工程细节而非模型：热路径误用 public `GetActiveModifierData()` 每次 `new ModifierData[count]`（应走内部 scratch 版）；注入自定义组合策略时每次分配排序副本【源码·02§8.5】。这为 §7.3 的取舍提供了第三方证据。

## 6 逐项对照表

| # | 维度 | 我方模型 | AbilityKit | 判读 |
|---|---|---|---|---|
| 1 | 触发规则声明 | DefAsset `Triggers[{Event, Condition, EffectChain}]` | `TriggerPlan<TArgs>` 不可变计划（Phase/Priority/条件/动作/执行控制）+ JSON 装载 | 同构；AbilityKit 多 Phase/执行控制/Cue |
| 2 | 事件源 | GameplayTag 键 + 层级匹配订阅 | `(稳定int Id, typeof(TArgs))` 复合键 + codegen 稳定 ID，精确匹配 | 我方降维更强；AbilityKit 类型安全更强 |
| 3 | 条件 | 条件链（词汇类谓词） | 三态：None/函数(arity≤2)/RPN 表达式 + IsDeterministic 门禁 | 抄确定性门禁；表达式不抄 |
| 4 | 效果 | FEffectStep 链（协程执行器） | 平面 Actions + ActionScheduler + 可选执行节点树（双轨并存） | 我方单轨更干净 |
| 5 | 优先级 | 订阅表：注册序 + 优先级 | Phase ↑ → Priority ↓ → 注册序 ↑，通道层只认硬停止 | 基本一致 |
| 6 | 中断/压制 | 未定义 | 软中断 StopBelowPriority + InterruptPriority 自动压制 + Strict 短路 | **我方缺口**，提案补字段（§7） |
| 7 | 重入防护 | 帧末统一泵 + 连锁下一轮（结构性切断） | 默认 Immediate 同步直派 + MaxFlushPasses 环计数兜底；Dispatch 遍历实时列表 | 我方更严格；环计数上限值得抄做保险丝 |
| 8 | 上下文 | Handler 直调 `(实例视图, 事件)` | ExecCtx 13 服务槽（Control/Policy/Blackboards…） | 我方更薄；Control/Policy 概念留位 |
| 9 | 生命周期 | Apply/Remove 同步注册/注销订阅 | 注册返回 IDisposable；MOBA 示例层另建 Revision 调和链 | 我方更简；调和链为反面参考 |
| 10 | 属性聚合 | Tag 门控修饰器列表 + 值变更重算 | ModifierOp 四内置 + 分组公式（顺序无关）+ 脏标记懒重算 + 依赖图 | 高度同构；公式与阈值可直接采用 |
| 11 | 堆叠 | DefAsset 堆叠策略 | ModifierStacking 仅计算逻辑，存储归业务 | 同构 |
| 12 | 表现钩子 | 表现钩子待设计 | Cue 两层 + 空实现早退零开销 | 可抄结构 |
| 13 | 确定性 | dt 单点供给 + 定步长待定 | Fixed64 定时器内部定点 + RequireDeterministic 强制门禁 + DeterministicRandom | 门禁机制可抄；数值定点我方后置 |
| 14 | 可观测 | 总线单点插桩 | Lifecycle/Observer/Cue/Tracer 四通道 + 诊断聚合 | 结构可抄 |

## 7 提案 v1：触发器表字段设计 + 修饰器聚合取舍

### 7.1 触发器行字段（12 字段，Buff DefAsset 内 `Triggers[]` 一行）

| # | 字段（UE 命名） | 类型 | 语义 | AbilityKit 对应物 | 来源判定 |
|---|---|---|---|---|---|
| 1 | `EventTag` | `FGameplayTag` | 事件键，层级匹配订阅（`Event.Combat.*` 命中一族） | `EventKey<TArgs>`（Id+TArgs 复合键）+ StableStringId 哈希（04§2.3） | 对应物换形：Tag 层级替代类型通道 |
| 2 | `EventPayloadFilter` | `FInstancedStruct`（可选） | 事件参数预过滤——进条件评估前先按参数拦一道，省谓词调用 | 无（AbilityKit 靠条件函数读 TArgs） | **我方新增**：补 Tag 降维失去的参数维度 |
| 3 | `Conditions` | `TArray<FBuffConditionStep>` | 有序 AND 条件链；节点 = 谓词词汇类引用 + 结构化参数 + 期望布尔（取反即 Not） | `PredicateKind` + `PredicateId`+`PredicateArg0/1` / RPN 栈机（04§4.4） | 简化采用：AND 链替代表达式 |
| 4 | `Effects` | `TArray<FEffectStepRef>` | 效果链引用（复用技能侧 FEffectStep 序列，同一套词汇） | `Actions[]: ActionCallPlan`（位置/具名参数 + 调度域）（04§3.2） | 对应物：引用式效果链 |
| 5 | `Priority` | `int8` | 同一事件内行序，**大者先**，同优先级按声明序 | `TriggerRunnerEntry.Priority`（大者先）+ 注册序 tiebreak（04§4.2） | 对应物（方向语义显式写进表头注释） |
| 6 | `ExecutionGate` | `FBuffTriggerGate{Mode: Once/Cooldown/Repeat, MaxExecutions, CooldownSeconds}` | 执行门槛；Cooldown 记账用战斗泵时间轴（ScaledDt 累积），不用墙钟 | `ETriggerExecutionMode`(Always/Once/Cooldown/Repeat)+`MaxExecutions`+`CooldownMs`（本次复核 `TriggerExecutionControlPlan.cs:3-26`） | 对应物 + 修正：修掉"宿主忘刷新 TotalTimeMs"坑（04§8.8） |
| 7 | `InterruptPriority` | `int8`（0=不启用） | 本行执行成功后压制同事件内低于该值的后续行（如"死亡打断后续掉落触发"） | `TriggerPlan.InterruptPriority` → `StopBelowPriority` 自动压制（04§3.2/§4.3） | 对应物：补我方中断语义缺口 |
| 8 | `Scope` | `enum EBuffTriggerScope { Buff（默认）, Unit }` | 生命周期绑定：随 Buff Remove 注销订阅（默认），或随单位驻留（被动特性） | MOBA 持续订阅调和链 + 按 TriggerId 分桶调度（04§4.10） | **我方新增**：把示例层调和链收敛为一个枚举 + Apply/Remove 闭合纪律 |
| 9 | `GateTags` | `FGameplayTagRequirements`（可选） | 行级 Tag 门控（Activation/Ongoing 语义：激活需 Tag、持续需 Tag） | `GameplayTagRequirements` / `ContinuousTagRequirements` 四段式（02§3.1） | 对应物：与修饰器 Tag 门控同一词汇 |
| 10 | `HandlerClass` | `TSubclassOf<UBuffTriggerHandler>`（可选） | 特例触发器指定共享 Handler 类；缺省 = 按 Effects 生成的通用执行器 | 无（AbilityKit 全走委托注册表 + 计划解释器） | **我方新增**：事件分发文档已定的 Handler 形态，计划数据与直调代码的分界 |
| 11 | `Cues` | `TArray<FBuffCueBinding>` | 表现挂钩（ConditionPassed/Executed/Interrupted/Skipped 时机 → 表现键） | `ITriggerCue` 两层（Trigger/Behavior）+ `ECueLifecycleStage` + 空实现早退（04§4.11） | 对应物 |
| 12 | `bConditionMissIsSilent` | `bool`（默认 true） | 条件不满足 = 静默无事（联动触发），false = 记失败信号（供技能门禁/调试统计） | `MobaTriggerPlanExecutor.Execute` 的 `predicateMissIsSuccess` 双入口（04§4.10） | 对应物升格：示例策略进数据面 |

**不抄清单与理由**：① RPN 表达式条件——谓词 AND 链 + 结构化参数覆盖自走棋条件全集，表达式层带来装载/校验/调试三份复杂度（04§4.4 栈机 + 校验器七类的体量是前车之鉴）；② Blackboard/NumericVarDomain/五源 NumericValueRef——我方数据源是单位容器 + 事件参数，黑板是"服务面缺省项"而非 Buff 必需（04§3.2）；③ 执行节点树（Sequence/Selector/Parallel…）——与平面 Actions 双轨并存正是 AbilityKit 的复杂度教训（04§4.8），我方效果链单轨；④ Phase 分组——我方事件分级（同步查询类/帧末状态类）+ Priority 已覆盖，两组正交维度只留一个。

### 7.2 修饰器聚合取舍：重算式 vs 增量式

| 维度 | 重算式（AbilityKit 实证 + 我方现案） | 增量式（GAS 聚合器） |
|---|---|---|
| 写路径（挂/删修饰器） | O(1) 置脏 + 计算器失效 | O(1) 增量更新，但**删除时逆运算**（减法/除法）引入浮点误差或需维护逆序 |
| 读路径 | 脏时 O(k) 全量组合（k=活跃修饰器数），干净时读缓存 | O(1) 恒定 |
| 确定性 | 分组公式顺序无关（§5.3），同输入同输出；AddSum/乘积天然可交换 | 累积和依赖挂载顺序历史，回放/快照要额外封存聚合中间态 |
| 实现复杂度 | 低：置脏 + 一个纯函数组合器 + 阈值事件 | 高：聚合器不变量维护、删除补偿、与修饰器生命周期的一致性 |
| 适配的查询模式 | 写多读少（Buff 挂/删/刷新频繁，每帧读的属性有限） | 读多写少（属性面板常开、海量查询） |
| 规模判定 | 200 武将级 × 每单位 ~10-20 属性 × 活跃修饰器 ~10：单次重算百级浮点运算，懒重算天然合并同帧 N 次写 | 收益要在属性数/修饰器数再高一个量级才显 |

**结论：v1 维持重算式**，与既有决策（载体讨论 §6.2"比 GAS 增量聚合简单一档"）一致，AbilityKit 全框架采用重算式是第三方佐证（§5.7）。具体采用清单：

1. **组合公式照抄 AbilityKit**：`Override(终止) > (Base+AddSum)×PercentProduct×MulProduct`，Op 优先级组 Override=0/Add=10/PercentAdd=15/Mul=20（§5.3）——顺序无关免去排序正确性争论，比 GAS 逐条 fold 更可解释。
2. **ModifierOp 词汇**：四内置（Add/PercentAdd/Mul/Override）+ Custom 扩展槽；不上 Divide/FinalAdd 等（自走棋无需求，AbilityKit 四种撑住 MOBA 全部 Buff 实证有效）。
3. **重算细节**：脏标记懒重算 + BaseValue/结果阈值 1e-5 的 Changed 事件（§5.4）；组合走 scratch 缓冲零分配（避开 `new ModifierData[count]` 坑 §5.7）；可选"单条目缓存"后置——200 单位规模先不做，留测量后再加。
4. **失效传播**：v1 只做属性依赖图 + 写路径置脏两条；ContextFloat 式上下文依赖不引入（时变修饰器改由 Buff 的 Duration/周期 tick 驱动，见第 6 条）。
5. **时变修饰器**：不用"修饰器内部读 ElapsedTime"的 AbilityKit 形态（§8.6 的隐坑：context 缺失即永不衰减）；TimeDecay 类需求走 Buff 生命周期——到期/刷新时重挂修饰器，修饰器本体保持纯函数（base+列表+上下文快照 → 值）。
6. **来源与键空间**（吸收 §5.6 坑）：修饰器目标 = 强类型 `FCombatAttributeId`（枚举或 NameStamp，编译期隔离），**不走 packed int 复用整数空间**；来源 = `FBuffHandle`（generation 校验既有），来源清理 = 按 Handle 批量摘除 + 置脏——AbilityKit `AttributeContext.ApplyEffect` 上下文自增 SourceId 的不可跨上下文教训（02§8.1.5）一并规避。
7. **千人前瞻**：兵海层本就简化属性模型（载体讨论 §千人两层），重算式在武将层 ≤200 全功能单位没有可测瓶颈；增量式列入兵海层若需全功能属性时的再评估项，非当前承诺。

## 8 静默坑启示（对我方设计的具体动作）

1. **Mixed 绕过减免**（§5.5）：我方伤害类型的减免匹配必须用"包含"判定（`EnumHasAnyFlags`）或把 Mixed 在数据面拆成两条 Physical/Magic 结算——禁止在处理器分支里写 `!=` 相等比较。
2. **键空间共用**（§5.6）：一切"语义键"（修饰器目标/事件 Id/效果 Id）各用独立强类型，不做位压缩复用；确需压缩走显式编解码函数并加断言。
3. **arity 裂缝**（§2.5/§4.4）：条件与效果参数一律结构化 struct；装载期（DefAsset 加载/校验阶段）做完整性校验，杜绝"配置期可构造、运行期才炸"。
4. **重复键静默覆盖**（§2.5）：Buff/触发器 Id 装载期查重当错误；我方订阅表天然按实例注册（无全局 Id 桶），此坑形态不成立，但 DefAsset 目录级查重仍要做。
5. **注册句柄必须可回收**（§2.5）：订阅注册返回句柄且由 Buff 实例持有——我方 Apply/Remove 闭合已有（事件分发文档 §5），补一条"句柄由容器统一持有，禁止散落"。
6. **Cooldown 时间源**（§3.5）：执行门槛记账用战斗泵 ScaledDt 时间轴（tick 泵文档 dt 单点纪律的自然延伸），不碰墙钟。
7. **实时列表遍历**（§3.5）：我方帧末泵的 PendingEvents 队列已规避事件侧；订阅表本身的增删仍要"延迟到泵循环后处理"（tick 泵文档护栏 2 同款纪律，明确覆盖订阅表）。
8. **环保险丝**：即使帧末泵结构性防重入，仍抄 `MaxFlushPasses` 思路给泵加轮数上限 + 超限上报（我方无上限抛异常语义，改为日志+熔断该事件）。
9. **监听者异常策略**（01§4.1：EventDispatcher 静默吞异常 vs Context 异常隔离路由）：我方 Handler 直调要显式裁决——建议"吞 + 上报调试通道 + 不中断泵"，与 tick 泵护栏 3（无界循环禁令）配套。
10. **Apply/Remove 非对称**（02§8.4：`TryRemoveAllRefs` 清全部来源、模板不撤销授予）：我方一切清理按 BuffHandle 成对执行，禁"按 Tag 全清"式 API 进入 Buff 生命周期路径。

## 9 未证实与假设清单

- 【未证实】AbilityKit 全部结论来自静态只读调研（未运行 dotnet build/test/Unity），"测试通过"均为转引或静态对账（04§7.3/02§7.2）。
- 【未证实】MOBA Buff 配置（Luban 数据）到 Trigger Plan/修饰器的逐条映射——02§8.7 明示未展开，本文 §5.7/§7.2 的"撑住 MOBA"判断基于代码结构而非配置盘点【推断】。
- 【未证实】`TriggerPlanJsonDatabase` JSON 字段全集、模板实例机制细节——本文仅引用报告 §4.9 摘要，未逐一核对 DTO 全文。
- 【未证实】`ExecPolicy` 由宿主刷新的失真场景（04§8.8 标注为推断）；`StronglyTypedPayloads` 半落地状态的实际影响面。
- 【推断】我方触发器行 12 字段对暗黑类词缀组合的覆盖度——FEffectStep 原语集合未定稿（载体讨论 §6.5 待定项），条件链能否表达"击杀时 if 目标带 X 标签"之外的嵌套逻辑待验证。
- 【假设】事件键 = GameplayTag 且事件风暴规模下层级匹配可接受（我方事件分发文档 §9 待定项，AbilityKit 无对应实证——它用精确 Id，未给层级匹配的性能数据）。
- 【假设】200 单位 × 5-10 并发 Buff 的规模账（载体讨论 §6.2 数字沿用）；修饰器重算在此规模下的实测成本未做 benchmark。
- 与我方文档的差异声明：AbilityKit 默认 Immediate 直派（§3.5）**不构成对我方帧末泵的反证**——两者是同步性 tradeoff 的不同选择，但事件分级清单必须裁决，否则"查询类立即返回"在我方形态中无处安放。
