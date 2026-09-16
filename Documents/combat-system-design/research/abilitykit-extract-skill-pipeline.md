# AbilityKit 对照调研提取：技能管线与原语（A 路）

> 性质：对照调研提取报告——为我方 FEffectStep 步骤链提供原语集提案 v1。
> 调研对象：AbilityKit（GitHub 快照 AbilityKit-master，2026-09-01 静态取证）报告 03/04/05/09 号 + vendored 源码（只读）。
> 证据标注沿用源报告：【源码】【文档】【推断】【未证实】及 E0-E5；源码路径前缀 `references/AbilityKit-master/`。
> 我方文档基线：`../../combat-system-state-tree-research/2026-08-31-combat-skill-system-carrier-discussion.md`（§6）+ 裁决 1/2 三份子文档。

## 目录

- 0 TL;DR
- 1 我方模型一句话回顾
- 2 Pipeline 模型（Phase 图 / 等待 / 分支 / 执行 / 取消 / 错误）
- 3 ActionSchema 强类型动作模式
- 4 combat 原语清单（targeting / projectile / damage / skilllibrary）
- 5 Pipeline vs FEffectStep 逐点对照表
- 6 FEffectStep 原语集提案 v1（重头）
- 7 否决与警示（含静默坑启示）
- 8 未证实与假设清单

---

## 0 TL;DR

AbilityKit Pipeline 是**解释执行**的阶段图：线性根列表 + 复合阶段递归嵌套（无节点级跳转/循环边），每 Tick 推进一个跨帧阶段、同帧连续跑完瞬时阶段；等待全靠计时与谓词**轮询**，无事件句柄等待；取消/打断/错误语义完整但分散且多处坑（Interrupt 不递归、Repeat 吞异常挂死、Conditional 的 Fail 不失败）。其阶段集合（Delay/WaitUntil/Conditional/Parallel/Repeat/Gate/Action）与我方 FEffectStep 控制流需求几乎一一对应，可直接映射。ActionSchema 强类型动作 = 稳定 ID + 委托注册表 + 参数 Schema（ParseArgs/TryValidateArgs）+ 校验器 + ID 代码生成，对我方"UObject 词汇类 + DefAsset instanced"是同构先行者，验证了该扩展面可行。combat 侧 targeting/projectile 原语参数面丰富（源码证实），damage 8 阶段管线有两个静默坑（Mixed 绕减免、Flags 无消费）。**产出：17 个 FEffectStep 独立原语提案 + 3 条"不做独立原语"裁决**（§6）。

## 1 我方模型一句话回顾

技能 = DefAsset 内 FEffectStep 步骤链（UObject 词汇类 instanced 定义 + 运行时 struct 实例 + 游标 + 等待句柄，事件驱动不逐帧空转，集中 tick 泵）；Buff = 触发器表（Event×Condition×Effect）+ 修饰器聚合；跨单位事件走总线 + 订阅表；泵 + 组件执行（TickableWorldSubsystem 管秩序、ActorComponent 管状态本地执行、词汇类管逻辑）。

---

## 2 Pipeline 模型

### 2.1 图结构：不是任意图，是"线性根列表 + 复合递归"

- Phase 图 = **线性根列表 + 复合阶段递归嵌套**，不是任意有向图；无节点级跳转/循环边（Repeat 只围绕单个子阶段/动作）【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.pipeline/Runtime/Core/Pipeline/AbilityPipeline.cs:24,91-105`（诊断图捕获也只有 Flow/Sequence/Parallel/Condition/Child 五种边）【03 §1】。
- 内建阶段 8 个：Action（瞬时委托）/Sequence/Parallel/Conditional/Delay/Repeat/WaitUntil/Gate + 条件节点 And/Or/Not；Timeline 内建实现**已停用**（文件仅 1 行注释），实际 Timeline 由 MOBA 侧自建【源码】`.../com.abilitykit.pipeline/Runtime/Phase/Timing/AbilityTimelinePhase.cs`【03 §4.5、§8-11】。
- 阶段协议 9 成员：`PhaseId/IsComplete/IsComposite/SubPhases/ShouldExecute/Execute/OnUpdate/Reset/HandleError`；四层基类 Instant（OnExecute 密封为执行+立即完成）/Durational（Duration 计时，-1 无限）/Interruptible（OnInterrupt=当帧退出）/Composite（不走模板，自驱动子阶段）【源码】`.../pipeline/Runtime/Phase/Core/AbilityPipelinePhaseBase.cs:139,175,250`【03 §3】。

### 2.2 等待/分支/并行如何表达

- **等待 = 计时 + 轮询，没有事件句柄**：Delay 是 Duration 别名；WaitUntil 每帧查谓词（谓词成立即完成，否则按 timeout 计时，`completeOnTimeout=false` 时超时后**回到未完成无限等**）【源码】`.../pipeline/Runtime/Phase/Timing/AbilityWaitUntilPhase.cs:35-72`【03 §4.5】。跨帧等待的另一半在 Triggering 侧：动作实现 `ITriggerRunningAction.Start()→IRunningAction` 交给 `TriggerActionRunner` 统一推进/按 owner 取消【源码】`.../com.abilitykit.ability/Runtime/Ability/Triggering/Runtime/TriggerActionRunner.cs:7-90`【05 §3.1】。
- **分支 = Conditional 多分支 + 检查策略**：`EConditionCheckStrategy`（OnEnter/Continuous/OnEvent——OnEvent 枚举存在但**包内零消费**，预留）；Continuous 时每帧复查可切分支（旧分支先 OnInterrupt）；无命中走 `ENoConditionBehavior`（Wait/Complete/Fail/Skip）——**其中 Fail 实际不失败**（见 §7）【源码】`.../pipeline/Runtime/Phase/Condition/AbilityConditionalPhase.cs:159-179`【03 §4.5、§8-2】。**Conditional 至少跨 1 Tick**（完成判定只在 OnUpdate，命中当帧 IsComplete 仍 false）【03 §8-4】。
- **并行 = Parallel 复合阶段**：逻辑并发、单线程（子阶段仍在调用方 Tick 线程内）；Execute 顺序启动全部，OnUpdate **逆序**遍历更新（移除安全），清空即完成【源码】`.../pipeline/Runtime/Phase/Composite/AbilityParallelPhase.cs:28-83`【03 §4.5】。
- **门控 = Gate 瞬时阶段**：条件不满足 → `context.IsAborted=true` → 同 Tick 整条管线 Fail（是"失败"不是"跳过"）【源码】`.../pipeline/Runtime/Phase/Condition/AbilityGatePhase.cs:30-36`【03 §4.5、§8-3】。

### 2.3 编译还是解释执行

**解释执行**：Run 每 Tick 驱动（`Tick(deltaTime)`），每 Tick 只对当前跨帧阶段调一次 OnUpdate，阶段完成后 while 循环（`ExecutePipeline`）**同帧连续推进所有可同步完成的后续阶段**直到遇到未完成者；`ShouldExecute=false` 的阶段跳过且不产生事件。无编译产物；编译思想只出现在 Triggering 侧（TriggerDef→TriggerInstance、RPN 表达式编译）【源码】`.../pipeline/Runtime/Core/Pipeline/AbilityPipeline.cs:421-470,514-562`【03 §4.1；05 §3.1】。

### 2.4 取消/打断/错误处理语义

| 操作 | 语义 | 边界 |
|---|---|---|
| Pause | 置 run+context 双布尔，发事件+trace | **State 保持 Executing**（Paused 枚举值从未赋值） |
| Resume | 清双布尔 | 非暂停态幂等 |
| Interrupt | 通知当前可中断阶段+其**直接**子阶段 → IsAborted → 立即 Fail | **无 Interrupted 终态**；通知**不递归**更深层（嵌套复合内层收不到） |
| Cancel | 仅置 `_isCancelled` | **下一次 Tick 才 Fail**；不再 Tick 则残留 Registry |
| context abort | Tick 开头/OnUpdate 后/ExecutePipeline 每轮三检查点 | 进 Failed（不发中断事件） |

【源码】`.../pipeline/Runtime/Core/Pipeline/AbilityPipeline.cs:472-512,644-692`【03 §4.2】
- 错误处理：阶段异常统一进 `HandlePhaseError`——先调阶段 `HandleError`（默认空实现），handler 再抛则聚合成 `AggregateException`（双失败聚合，有测试固化）；失败原因走 `Context.SetData("FailReason",…)` 共享键（MOBA 用它做规则门禁失败原因回传）【03 §4.3、§4.10；04 §4.10】。坑：事件委托无异常隔离、Repeat 的 action 异常被吞且管线**永久挂死**、Complete 路径事件抛错会跳过 Cleanup、Cleanup finally 内 Unregister 与池归还不彼此隔离【03 §4.3、§8-5】。
- 生命周期：定义实例与运行实例分离——`Start` 时经 `IAbilityPipelinePhaseInstanceFactory.CreateRunPhase()` 克隆（内建阶段全覆盖；**不实现工厂的阶段直接复用定义实例，并发不安全**）；克隆是"深树浅值"（结构递归克隆，委托/谓词/闭包按引用共享，闭包捕获可变状态仍跨 run 串扰）；阶段列表按 run 隔离且池化（容量 8/max 256）【源码】`.../pipeline/Runtime/Core/Pipeline/AbilityPipelinePhaseRuntime.cs:14-39`【03 §4.4、§8-12】。

---

## 3 ActionSchema 强类型动作模式

AbilityKit 的"强类型动作"分两层：**Plan 层**（triggering 包主线）与 **TriggerStrong 配置层**（ability 包）。注意：`com.abilitykit.actionschema` 包本身只是 Timeline DTO（SkillAssetDto/TrackDto/ClipDto），**不是**动作参数 Schema——命名易误导【05 §1.3】。

### 3.1 动作类型定义与注册

- **ID 体系**：`ActionId/FunctionId` 是 int 包装，由 `StableStringId.Get`（Fnv1a32 UTF-16 非负哈希，碰撞抛异常）从稳定字符串生成，跨版本不漂移；codegen 产物 `Generated/Triggering.GeneratedIds.cs` 维护 Id↔名称双向表（`GeneratedIdNames.RegisterAll`），供诊断把 id 格式化回名称【源码】`.../com.abilitykit.triggering/Runtime/Events/StableStringId.cs:12-32`、`.../Generated/Triggering.GeneratedIds.cs:22-29`【04 §3.2】。
- **委托注册表**：`ActionRegistry.Register<NamedAction0/1/2>(ActionId, action, isDeterministic)`——**(Id, 委托类型) 复合键**检索 + `IsDeterministic` 标记直接服务 `ExecPolicy.RequireDeterministic` 强制检查（确定性门禁在执行期校验）【源码】`.../triggering/Runtime/Registries/ActionRegistry.cs:10-72`【04 §3.2、§5】。
- **发现式模块**：`IPlanActionModule.Register(ActionRegistry, IWorldResolver)` + `AutoPlanAction` 抽象基类（ActionId/Order/ParseFrom/Execute 四件套），领域扫描批量注册【源码】`.../triggering/Runtime/Plans/Model/ActionArgs.cs:214-217`【04 §3.2】。

### 3.2 参数面与校验

- **参数 Schema**：`IActionSchema<TActionArgs,TCtx>` = `ActionId + ArgsType + ParseArgs(具名参数, ExecCtx) + TryValidateArgs`；注册到 `ActionSchemaRegistry`（静态注册表，带锁）【源码】`.../triggering/Runtime/Plans/Execution/ActionSchemaRegistry.cs:14-69`【04 §3.2、§5】。
- **动作调用计划 `ActionCallPlan`**：具名/位置双参数形态（Arity 超 2 时第 3 个起塞具名字典 `"__N"`）；`ActionArgValue` 四种 Kind（NumericValue/BlackboardTarget/BooleanValue/StringValue）；调度域 `ScheduleMode`（Immediate/Delayed/Periodic/Continuous/Timeline）+ MaxExecutions（-1 无限）+ CanBeInterrupted + ExecutionPolicy（含 WithRetry）+ 行为级 Cue【源码】`.../triggering/Runtime/Plans/Model/ActionCallPlan.cs:61-302`【04 §3.2】。
- **数值引用 `NumericValueRef`**：Const/Blackboard/PayloadField/Var/Expr 五源 + clamp/scale/offset/fallback 修饰——数值参数统一走引用而非裸值，是词缀数值组合的关键基建【源码】`.../triggering/Runtime/Plans/Model/IntValueRef.cs:14`【04 §3.1】。
- **校验器**：`ITriggerValidator` + `CompositeTriggerValidator.CreateMinimal()` 及七类专项（Reference/ExecutionRoot/CycleDetector/UgcLimits/ActionCallPlan/RuleSchedulePlan/TriggerPlanExecutable），装载期 `Validate(...).ThrowIfInvalid(...)`【源码】`.../triggering/Runtime/Plans/Validation/CompositeTriggerValidator.cs:153`【04 §5】。
- **代码生成**：Editor 菜单（`TriggerCodegenMenu`）生成稳定 ID 与参数 Schema 表（`Triggering.GeneratedIds.cs`）；MOBA 另有目标查询工厂清单生成（`MobaGeneratedTargetQueryFactoryManifest`）【04 §5；09 §5】。
- **TriggerStrong 配置镜像**（ability 包）：Runtime 9 动作（AddBuff/DebugLog/ExecuteEffect/LogAttackerName/PlayPresentation/Sequence/SetVar/ShootProjectile/SpawnSummon）+ 5 条件配置，`ActionConfigBase.ToActionDef()` 转定义；Editor 侧 17 文件 `*EditorConfig` 一一镜像【源码】`.../com.abilitykit.ability/Runtime/Ability/Config/TriggerStrong/`【05 §3.3、§5】。

### 3.3 对我方"UObject 词汇类 + DefAsset instanced"的借鉴与不适用点

**借鉴**：①定义/实例分离 + 工厂克隆的必要性被反复验证（不克隆则并发共享运行态——我方"DefAsset 定义 + struct 运行实例"天生规避，且比"深树浅值"克隆更干净，因我方运行态不内嵌定义对象）；②`IsDeterministic` 随注册标注、执行期强制校验的模式值得抄（我方词汇类 Execute 可带确定性标记服务未来回放）；③NumericValueRef 式"参数即引用"（Const/Var/Expr + clamp/scale）是词缀组合刚需，建议进 FEffectStep 参数面约定；④装载期校验器族 + 稳定 ID↔名称表（诊断可读性）低成本高收益。
**不适用**：①AbilityKit 动作是"无状态委托 + 字符串 ID"，参数是 `Dictionary<string,ActionArgValue>` 弱类型字典 + Schema 事后解析——我方 UObject 词汇类 instanced 直接把参数面做成 **UPROPERTY 强类型编辑器字段**，比"具名参数字典 + ParseArgs"少一层解析与校验，无需其 Schema 注册表与代码生成（代码生成只剩 ID↔名称表可选）；②`ExecCtx` 13 服务槽的宽上下文是 ECS-World 形态产物，我方泵+组件形态用"实例视图 + 事件"窄上下文即可；③坑：`StronglyTypedPayloads` 主线恒 null 未接线（强类型访问器注册表定义了但没接进 Runner）【04 §3.2、§8】——"定义了强类型面却没接主线"正是我方要靠"词汇类即定义"避免的脱节。

---

## 4 combat 原语清单

### 4.1 targeting（选目标模式全集）

管线 = Provider（候选源）→ Rule（过滤）→ Scorer（评分）→ Selector（选取）→ Mapper（映射），`ref struct` 链式装配【09 §2.1】。

| 模式 | 源码证实内容 |
|---|---|
| 形状 | `CircleShapeRule`(0x0101)、`SectorShapeRule`(0x0102，cos 用 CORDIC 确定性桥防跨端 1-ulp 漂移)；矩形/环形状**无**（Manual.md 记载的 OrientedRect 等系未落地 API）【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.targeting/Runtime/SearchTarget/Rules/CommonRules.cs:5-154`【09 §3.1、§8.1-2】 |
| 条件筛选 | 白名单/黑名单/排除实体/要求有效 ID/要求有位置 5 内建规则 + And/Or/Not 复合（构造时快照复制）；规则按声明顺序首个失败短路【源码】`.../Rules/CommonRules.cs`、`CompositeRules.cs:16-68`【09 §3.1】 |
| 评分排序 | 固定分/种子哈希随机/距离平方 3 评分器；`SearchOrder` 严格字典序 + 全同分稳定键决胜；**NaN 评分即淘汰**、无穷保留【源码】`.../Scorers/CommonScorers.cs:6-83`、`Queries/SearchOrdering.cs:115-131`【09 §3.1、§4.1】 |
| 选取 | Top-K 完整排序与流式 Top-K（融合路径 O(H×K×M)）+ `MaxCount` 截断 + 去重（**键只在规则+评分全部通过后才提交**——有契约测试固化）【源码】`.../Execution/TargetSearchEngine.cs:194-209`【09 §4.1、§7.1】 |
| 候选组合 | Concat/UnionDistinct/Intersect/Except 四种 Provider 集合运算【09 §3.1】 |

### 4.2 projectile（参数面）

`ProjectileSpawnParams`（readonly struct，float 边界输入）→ 世界内 Fixed64/Q32.32 定点积分（**文档仍按 float 描述，源码已定点化**——快照最重要的文档滞后）【09 §1.3、§8.1-1】。源码证实参数面【源码】`.../combat.projectile/Runtime/Projectile/Runtime/ProjectileSpawnParams.cs:11-114`【09 §3.2、§4.2】：

- 时空与运动：SpawnFrame/Position/Direction/Speed；**追踪**（TrackingTargetActorId + Provider，丢失→ExitReason.TrackingTargetLost）；**回旋**（ReturnAfterFrames/ReturnSpeed/ReturnStopDistance，先 Launcher 后 Root）。
- 生命周期：LifetimeFrames/MaxDistance/StartSuspended/TickIntervalFrames；ExitReason 8 值（Hit/Lifetime/MaxDistance/Manual/ReturnArrived/ReturnTargetLost/TrackingTargetLost/Unknown）。
- 碰撞：CollisionLayerMask/IgnoreCollider/CollisionHalfExtents（支持定向盒扫掠）；命中三态响应 `Ignore/Hit/Block`（Block=立即退出）；本帧命中去重缓冲（`HitColliderIdsThisTick[9]`）+ HitCooldownFrames 冷却 + **单帧最多 8 次命中**（maxHitsPerStep=8）。
- 命中策略：`ExitOnHit` / `Pierce`（maxHits<1 抛异常；hitsRemaining 耗尽自动重置 MaxHits）。
- 发射模式：SingleShot/Burst/Fan/Scatter（带 seed 保确定性）；调度 `ProjectileScheduleParams.Once/Repeat/Infinite`（count=-1 无限，到帧解析 pattern 可每帧动态换模式）。
- 区域：`AreaWorld` 静态**圆形**区域（float 状态、不进回滚快照），prev/curr 双缓冲差分产出 Enter/Exit 事件 + StayIntervalFrames 周期 Stay；到期先补 Exit 再发 Expire。
- 回滚：MemoryPack 快照 schema v7、Q32.32 raw long 字段；Import 后 HitFilter 回落默认、冷却清零、事件缓冲清空、**AreaWorld 不恢复**。

### 4.3 damage（伤害管线与两个静默坑）

`DamageCalculationPipeline.CreateDefault()` 8 阶段 Dataflow：Validate → Critical（critRoll<critChance 置 Critical 旗标+倍数；**随机值由上层注入 CritRoll 槽**保可回放）→ Base（Physical 加物攻/Magic 加法攻，再乘暴击倍数）→ Bonus（百分比+固定）→ Armor/MagicResist（有效防御=`max(0, def×(1-pen%)−flatPen)`，减免=`eff/(100+eff)`）→ Final（Floor）→ Overkill（区分 Overkill/Actual，护盾优先吸收）【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.damage/Runtime/Damage/Processor/DamageProcessors.cs:15-44,51-102`【09 §3.3、§4.3】。参数面：DamageRequest（Source/Attacker/Target:**object** 无类型约束/BaseValue/DamageType/Flags/SourceType）+ DamageSlots 10 个强类型槽（CritChance/CritMultiplier 默认 1.5/CritRoll 默认 1/BonusPercent/BonusFlat/双穿透%/TargetShield）+ DamageResult 10 个可观测阶段字段。

**两个静默坑（原委）**【源码】`DamageProcessors.cs:217-226,281,330`【09 §4.3、§8.2】：
1. **`DamageType.Mixed` 完全绕过减免与攻击加成**：护甲处理器首行 `if (DamageType != Physical) return`、魔抗处理器首行 `if (DamageType != Magic) return`——`Mixed = Physical|Magic` 两者都不匹配 → 无减免；基础伤害加成分支也只认 Physical/Magic → Mixed 无攻击力加成。原委：Flags 枚举语义（"既是又是"）与处理器"逐类型单分支"实现错位，无测试覆盖 Mixed 路径，静默通过。
2. **`DamageFlags` 11 个旗标中仅 Critical 被消费**：PenetrateArmor/PenetrateMagicResist/IgnoreShield/Lifesteal 等在 8 处理器中零消费逻辑（穿透实际由 DamageSlots 槽位承载）——旗标是"声明了语义但没人买单"的死参数，接入方依赖即踩坑。

### 4.4 skilllibrary（技能库组织）

`SkillLibrary<TKey,TData>` = 技能主数据字典 + 派生索引：主字典先提交再通知索引；`CreateDerivedKeyedIndex/CreateDerivedMultiKeyIndex` 用 selector 派生单键/多键索引（如按技能类型/职业分桶）；`Update` 用 old/new 双数据重算派生键。**E0+E1，全仓无任何生产消费方**（MOBA 用的是 entitymanager 而非 skilllibrary）——它只回答"条目如何索引"，不回答"技能条目内容如何组织"（内容组织在 MOBA 的 skills.json/TriggerPlan JSON，属示例层协议）【源码】`.../combat.skilllibrary/Runtime/SkillLibrary/SkillLibrary.cs:25-122`【09 §3.5、§6.2、§7.1】。对我方启示：DefAsset 本身就是 UE 资产库（PrimaryAssetId/AssetManager 可索引），无需再造 SkillLibrary——其"主存储先提交再通知"与 comparer 全链一致教训（内建索引用 Default comparer 与注入 comparer 分裂）值得记取【09 §4.4、§8.2】。

---

## 5 Pipeline vs FEffectStep 逐点对照表

| 维度 | AbilityKit Pipeline | 我方 FEffectStep | 判定 |
|---|---|---|---|
| 数据结构 | 阶段=类实例（接口 9 成员 + 四层基类），定义实例与运行实例经工厂克隆分离（深树浅值） | 步骤=USTRUCT 数据（DefAsset 内）+ UObject 词汇类 instanced 执行逻辑 + struct 运行实例（游标/等待句柄） | 同构且我方更彻底：定义是纯数据、逻辑在永生词汇类，无克隆语义可漏 |
| 图形状 | 线性根列表 + 复合递归嵌套；无节点级跳转/循环边 | 步骤链同构（线性 + 复合）；子链用 RunSubChain 引用 | 一致——两者都论证了"技能不需要任意图" |
| 推进模型 | 宿主每帧 Tick；每 Tick 推进 1 个跨帧阶段 + while 连续跑完瞬时阶段 | 集中 tick 泵 + 组件执行；游标推进 + 步进预算（单帧 N 步兜底） | 同构；我方多一层"泵不执行逻辑"的职责隔离与步进预算 |
| 等待语义 | **轮询**：Duration 计时 + WaitUntil 每帧查谓词；跨帧动作靠 Triggering 侧 IRunningAction/ActionScheduler 宿主推进 | **等待句柄**（定时/事件/动画帧），事件驱动不逐帧空转 | **能力差异（我方设计目标）**：AbilityKit 没有事件句柄等待原语，WaitUntil 轮询在 200 单位×高频链下是浪费；其替代方案是把等待拆到 Triggering 侧 running action——两套体系接缝成本高，佐证我方"等待句柄内建于步骤链"的取舍 |
| 分支表达 | Conditional 多分支 + CheckStrategy（Continuous 可切分支/至少跨 1 Tick）+ NoConditionBehavior（Fail 语义失真）+ Gate 硬门控 | Branch 步骤 + WaitUntil 条件；提案修正：真三态无命中策略、瞬时分支同帧完成 | 映射直接；其 Fail≠失败与跨 Tick 问题是现成反面教材 |
| 并行 | Parallel 复合阶段（逻辑并发单线程、逆序更新、全部完成才过） | Parallel 原语（提案加 All/Any 完成策略） | 同构，补完策略 |
| 取消/打断 | Pause（State 保持 Executing）/Cancel（下 Tick 生效）/Interrupt（立即 Fail+通知不递归+无独立终态） | 句柄 generation 失效即取消；打断=泵级统一语义（无需逐阶段 OnInterrupt 传播——struct 实例无对象身份，不存在"通知不到"） | 我方结构性规避其"中断不递归"坑：句柄失效是数据判定不是对象遍历 |
| 错误处理 | 阶段 HandleError 默认空 + 双失败聚合 + FailReason 共享键；但 Repeat 吞异常挂死、事件/trace 无隔离、Complete 跳过 Cleanup | 提案：泵级统一 try/catch 边界 + OnError 原语（FailChain/Continue/Retry）+ FailReason 等价通道 | 借鉴其聚合与 FailReason，规避其分散与吞异常 |
| 生命周期 | Run 一次性句柄 + Registry 注册表 + trace 事件流 + 池化 | 我方已有：实例池/句柄 + 总线单点可观测（事件分发文档 §8） | 对齐；其 trace 9 事件类型清单可作我方调试 overlay 的事件清单参照 |
| 与 Buff/事件关系 | Pipeline 不知战斗语义；Buff 归 EffectContainer/TriggerRunner（另一套体系），跨体系靠字符串共享键 | 我方 Buff=触发器表本就同总线同泵 | 我方一体性更好；AbilityKit 的 Pipeline/Triggering/Effect 三件套接缝（字符串键、ownerKey 协议）是反面参考 |

**AbilityKit 报告与我方文档不符/需注意的事实**：①我方讨论记录 §6.4 说"等待句柄，事件驱动不逐帧空转——GAS AbilityTask 思想"；AbilityKit 实证了**另一条路**（轮询+外部 running action）也能工作但接缝成本高，这不是矛盾而是佐证，采纳时需明确我方 WaitUntil 主路径是事件句柄、轮询仅兜底；②AbilityKit 无"每帧推进多阶段"以外的步进预算概念，我方 tick 泵文档的步进预算护栏是其缺失面，保留。

---

## 6 FEffectStep 原语集提案 v1（重头）

面向暗黑类 ARPG 词缀组合场景。判据——**做成独立原语**：需要专属运行时资源/容器、或词缀高频组合点且参数面不可再简化、或控制流语义无法用参数表达；**组合而成**：可由其他原语+参数表达（如 DoT、吸血、连锁）。参数面证据：〔源码〕=AbilityKit 源码证实；〔提案〕=我方新增/修正设计。

### 6.1 控制流原语（7）

| # | 原语 | 参数面 | 来源（对应物 + 源码路径） | 取舍理由 |
|---|---|---|---|---|
| 1 | `WaitDelay` | Duration〔提案：秒，ScaledDt 累积；≤0 配置校验拒绝——AbilityKit 的 ≤0 永不完成是坑〕 | ≈`AbilityDelayPhase`【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.pipeline/Runtime/Phase/Timing/AbilityDelayPhase.cs`【03 §4.5、§8-5】 | 最高频等待原语；词缀延迟触发基础件 |
| 2 | `WaitUntil` | 条件（事件 Tag 到达〔提案：主路径事件句柄〕/谓词〔提案：仅兜底轮询〕）+ Timeout + 超时策略（Complete/Fail/KeepWaiting〔源码证实 WaitUntil 有 timeout+completeOnTimeout〕） | ≈`AbilityWaitUntilPhase`【源码】`.../pipeline/Runtime/Phase/Timing/AbilityWaitUntilPhase.cs:35-72`【03 §4.5】 | 等事件/等命中/等死亡；我方等待句柄核心卖点在此落地 |
| 3 | `Branch` | 条件列表（首个命中，And/Or/Not 组合〔源码证实条件节点家族〕）→ 子链；无命中策略 Wait/Complete/**Fail（真失败）**〔提案：修正 AbilityKit Fail 失真〕；瞬时分支同帧完成〔提案：修正其至少跨 1 Tick〕 | ≈`AbilityConditionalPhase/AbilityConditionalBranch`【源码】`.../pipeline/Runtime/Phase/Condition/AbilityConditionalPhase.cs:159-179`【03 §4.5、§8-2/4】 | 词缀条件分支（如"目标血量<30% 则爆炸"）不可用参数表达 |
| 4 | `Parallel` | 子链组 + 完成策略 All〔源码证实〕/Any〔提案：任一完成即过，其余取消——暗黑"多段效果任一命中"需要〕 | ≈`AbilityParallelPhase`【源码】`.../pipeline/Runtime/Phase/Composite/AbilityParallelPhase.cs:28-83`【03 §4.5】 | 并发子链（伤害+挂状态+表现同时）；逻辑并发单线程即可 |
| 5 | `Repeat` | 子链 + Count（必须 >0 或显式 Infinite 标记〔提案：防 AbilityKit 无上界挂起〕）+ Interval + 每轮重置〔源码证实 RepeatCount/Interval/Reset 语义〕；异常上抛链级〔提案：修正其吞异常挂死〕 | ≈`AbilityRepeatPhase`【源码】`.../pipeline/Runtime/Phase/Timing/AbilityRepeatPhase.cs`【03 §4.5、§8-5】 | 多段攻击/脉冲 AOE；DoT 也可用它表达但首选 ApplyState 周期 |
| 6 | `Gate` | 条件 + 失败原因写入 FailReason 等价通道〔源码证实 Gate 失败=整链 Fail + FailReason 共享键〕 | ≈`AbilityGatePhase`【源码】`.../pipeline/Runtime/Phase/Condition/AbilityGatePhase.cs:30-36`【03 §4.5、§8-3】 | "需要目标在场才能继续"类硬门控；与 Branch 的区别是失败即终止整链 |
| 7 | `RunSubChain` | 子链定义引用 + 失败传播（FailParent/Continue〔提案〕）+ 上下文键隔离〔提案〕 | **我方新增**（AbilityKit 无子链引用原语；最近似物是 `ITriggerPlanExecutable` 节点树【源码】`.../triggering/Runtime/Plans/Executables/ITriggerPlanExecutable.cs:10-18`【04 §3.1】与 pipeline 嵌套组合） | 词缀库组合的基本手段：通用子链（如"一次武器挥击"）被多技能复用；保持定义层单事实源 |

### 6.2 战斗原语（7）

| # | 原语 | 参数面 | 来源 | 取舍理由 |
|---|---|---|---|---|
| 8 | `SelectTargets` | 候选源（阵营/范围单位）+ 形状（圆形/扇形〔源码证实 CircleShapeRule/SectorShapeRule〕；矩形/环形〔提案：源码无，ARPG 冲刺条/十字斩需要〕）+ 过滤（白/黑名单/排除自身/存活〔源码 5 规则〕）+ 排序（距离/固定种子随机〔源码 SeededHashRandomScorer〕/自定义评分）+ TopK/MaxCount + 去重〔源码证实全部〕；结果集写入上下文供后续步骤复用〔提案〕 | ≈combat.targeting 全管线【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.targeting/Runtime/SearchTarget/Execution/TargetSearchEngine.cs`、`Rules/CommonRules.cs`、`Scorers/CommonScorers.cs`【09 §3.1、§4.1】 | **形状判定独立成原语**（不内嵌进 Damage）：同一次选择的结果集要被伤害+挂状态+击退多步复用；targeting 与 damage 分离是 AbilityKit 证实正确的边界 |
| 9 | `Damage` | 目标集（来自 SelectTargets 或单目标）+ BaseValue（走 NumericValueRef 式引用〔源码证实 NumericValueRef 五源+clamp/scale〕）+ 类型（〔提案〕**单值** Physical/Magic/True/Mixed + 管线内 Mixed=两段分别结算——修正 Mixed 绕减免坑）+ 暴击（Chance/倍数/Roll 由泵注入种子随机〔源码证实 CritRoll 上层注入〕）+ 穿透（显式百分比/固定槽位〔源码证实槽位承载〕，**不做 Flags**）+ 护盾吸收/Overkill 区分〔源码证实〕+ 结果旗标进事件（击杀/暴击供 Buff 触发器消费〔提案〕） | ≈combat.damage 8 阶段管线【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.damage/Runtime/Damage/Processor/DamageProcessors.cs:15-44,51-102`【09 §3.3、§4.3】 | 词缀第一高频点；8 阶段顺序被源码+文档双重证实可直接借用；旗标教训见 §7 |
| 10 | `Heal` | BaseValue（引用）+ 受治疗加成钩子〔提案〕+ 溢出策略（截断/转护盾〔提案〕）+ 目标集 | **我方新增**（AbilityKit 无治疗原语；DamageResult 有 ShieldDamage 但无 HoT 管线）【09 全文无】 | 词缀高频点且与 Damage 对称；借 Damage 槽位模式做受治疗加成 |
| 11 | `ApplyState` | StateDef 引用 + 等级/层数 + DurationPolicy（Instant/Duration/Infinite〔源码证实枚举〕）+ 周期（Period + 首拍立即〔源码证实 ExecutePeriodicOnApply〕）+ **堆叠策略（Add/Refresh/Replace + 上限）〔提案：AbilityKit 无堆叠，StackCount 恒 1，我方暗黑类必须有〕** + Tag 门禁（Required/Blocked〔源码证实 ApplicationRequirements〕）+ 授予/回收 Tag 配对〔源码证实〕 | ≈`GameplayEffectSpec/EffectContainer.Apply`【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.ability/Runtime/Ability/Effect/EffectContainer.cs:22-73`、`GameplayEffectSpec.cs:9-43`【05 §3.2、§4.1】 | 挂状态=DoT/控制/增益的总入口；其 Apply 序（门禁→事件→授 Tag→组件→Cue）与到期/移除配对回收被源码证实，直接借用顺序 |
| 12 | `ModifyAttribute` | 属性 Tag + 运算（Add/Multiply〔提案枚举〕）+ 数值（引用）+ 时限（瞬时一次 / 跟随状态存在期——移除时配对清理〔源码证实：AttributeEffectComponent 移除时 ClearModifiers，sourceId 存实例 State〕） | ≈`AttributeEffectComponent`（ability 包）+ modifiers 包（02 报告未读，聚合语义另路调研）【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.ability/Runtime/Ability/Effect/Components/AttributeEffectComponent.cs:12-51`【05 §3.2】 | 词缀属性修改必须与来源生命周期配对，其 sourceId 配对清理模式源码证实可抄 |
| 13 | `SpawnProjectile` | 速度/方向 + 追踪（目标+丢失策略）+ 回旋（返程帧/速度/停止距离）+ 生命周期（寿命帧/最大距离）+ 碰撞层/尺寸 + 命中策略（ExitOnHit/Pierce+穿透次数〔源码证实〕）+ 三态过滤 Ignore/Hit/Block〔源码证实〕+ 发射模式（Single/Burst/Fan/Scatter+seed〔源码证实〕）+ 命中后动作子链〔提案：AbilityKit 命中只发事件，消费在项目侧〕 | ≈combat.projectile【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.projectile/Runtime/Projectile/Runtime/ProjectileSpawnParams.cs:11-114`、`ProjectileWorld.cs`【09 §3.2、§4.2】 | 参数面是五包中最完整的源码证实面，几乎可照抄为 UPROPERTY 字段；命中→事件→效果链的接缝沿用我方总线 |
| 14 | `SpawnArea` | 中心/半径 + 持续时间 + 进出/驻留事件间隔（StayInterval）+ 到期语义〔源码证实圆形 AreaWorld 全部〕；矩形/移动区域〔提案：源码仅静态圆形 float〕 | ≈`AreaWorld`【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.combat.projectile/Runtime/Projectile/Area/AreaWorld.cs:62-236`【09 §3.2、§4.2】 | 地面范围持续效果（法阵/毒圈）；enter/exit 差分+周期 Stay 的语义被证实 |

### 6.3 表现与元原语（3）

| # | 原语 | 参数面 | 来源 | 取舍理由 |
|---|---|---|---|---|
| 15 | `PlayCue` | CueTag + 时机（步骤前/后/命中点/状态期 OnActive/WhileActive/OnRemove〔源码证实双接口〕）+ 参数载荷 | ≈`IGameplayEffectCue`【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.ability/Runtime/Ability/Effect/IGameplayEffectCue.cs` + `ITriggerCue/TriggerCueDescriptor`【源码】`.../triggering/Runtime/Triggering/Contracts/ITriggerCue.cs`【05 §3.2；04 §3.1】 | 表现与逻辑分离是 GAS 正确遗产；词缀只改逻辑步不碰表现层 |
| 16 | `SetVar` | 键 + 值（NumericValueRef 五源：Const/Var/Expr/Blackboard/载荷 + clamp/scale/offset/fallback〔源码证实〕）；作用域（本链局部/单位级〔提案〕） | ≈`SetNumericVarAction` + `NumericValueRef`【源码】`references/AbilityKit-master/Unity/Packages/com.abilitykit.triggering/Runtime/Plans/Model/IntValueRef.cs:14`【04 §3.1；05 §3.1】 | 词缀组合的中间量传递（如"本次命中已弹射次数"）；引用化数值是组合而不硬编码的关键 |
| 17 | `OnError` | 包裹子链 + 策略（FailChain/Continue/Retry(N)〔提案〕）+ 失败原因写入 | **我方新增**（AbilityKit 无统一例外原语；异常语义分散且多处坑，见 §7）【03 §4.3、§8】 | 词缀组合链越长越需要局部容错；泵级统一 try/catch + 步级可配策略，FailReason 通道借其设计 |

### 6.4 不做成独立原语的裁决（3）

| 被否决项 | 裁决理由 | 组合方式 |
|---|---|---|
| `Schedule`（定时发射调度） | 仅 SpawnProjectile/Repeat 需要，做成两者参数面（Repeat 次数+间隔已覆盖 Once/Repeat/Infinite） | 对应物 `ProjectileScheduleParams.Once/Repeat/Infinite`【源码】`.../combat.projectile/Runtime/Projectile/Schedules/`、`ActionCallPlan.ScheduleMode`【04 §3.2】〔均源码证实〕 |
| `DamageInShape`（AOE 复合伤害） | 与 SelectTargets+Damage 两步组合完全重叠；独立会造出"形状参数双份漂移" | SelectTargets(形状)→Damage(目标集)；同结果集可再接 ApplyState/Knockback |
| `Timeline`（关键帧原语） | AbilityKit 内建 TimelinePhase 已停用、MOBA 自建才可用（文档仍画 Timeline 节点的教训）【03 §4.5、§8-11】；我方 ACT 帧判定已有 AnimNotify→事件路线 | 帧对齐需求 = WaitUntil(AnimNotify 事件)；真需求出现再立项 |

**组合示例（词缀 → 原语序列）**：燃烧 DoT = ApplyState(燃烧, Period) ；吸血 = Damage 后处理旗标 → Buff 触发器表（不占原语，走我方 Event×Condition×Effect）；连锁闪电 = Repeat{ SelectTargets(最近+排除已命中)→Damage }；暴击爆炸 = Branch(上次 Damage 结果.IsCritical){ SelectTargets(小圆)→Damage }。

---

## 7 否决与警示（含静默坑启示）

1. **不要照抄 DamageType[Flags]**：Mixed 绕减免+无攻击加成、Flags 仅 Critical 有消费者的原委是"枚举位语义"与"逐类型单分支实现"错位且无测试覆盖【09 §4.3、§8.2】。启示：**我方伤害类型用单值枚举；每声明一个参数/旗标必须指名其管线内消费者，无消费者的参数不进 UPROPERTY**——"参数面=被消费面"应成为原语评审门禁。
2. **不要让步骤自带错误处理默认值**：Repeat 吞异常挂死、HandleError 默认空实现、事件无隔离、Complete 跳过 Cleanup、Cleanup finally 不隔离【03 §4.3、§4.4、§8-5】。启示：异常边界收敛在泵级一处（对齐我方 tick 泵文档"单点可观测"），步骤级只做策略选择（OnError 原语）。
3. **不要做"定义即实例"的阶段对象**：不实现工厂克隆的阶段并发共享运行态、克隆深树浅值闭包串扰【03 §4.4、§8-12】。我方定义（数据）/实例（struct）/逻辑（词汇类）三分离结构性免疫。
4. **不要造第二个"动作/条件 ID 字符串协议"**：AbilityKit 因 Pipeline/Triggering/Effect 三体系并存付出字符串共享键（AbilityPipelineSharedKeys、effect.sourceContextId ownerKey、FailReason）与双 TriggerRunner/双 ConfigReloadBus 混淆代价【03 §4.5；05 §8.1-8.3】。我方总线+订阅表一体贯通，禁止步骤与 Buff 触发器之间出现私有字符串协议。
5. **轮询等待不要做主路径**：WaitUntil 每帧查谓词 + Conditional 至少跨 1 Tick 在高频链下是帧预算税【03 §4.5、§8-4】；我方等待句柄主路径事件驱动，轮询仅限无事件源的纯谓词。
6. **EffectSource 式"1115 行设计文档 vs 零实现"**：文档与源码脱节是 AbilityKit 系统性现象（15+ 报告有差异清单）【05 §8.1】。启示：我方设计文档落 §"决策状态"（已定/待定），未实现的设计不得写成既成事实。
7. **ImmediateAbility（同步链）与跨帧链分开两个执行入口**是 AbilityKit 刻意设计（Instant 管线 vs Tick 管线）【03 §1】——我方瞬时步骤同帧连续推进已覆盖，无需双入口，但"Execute 后未同步完成即失败"的强约束提示：**瞬时原语不允许内含等待**，这条校验应进装载期校验器。

---

## 8 未证实与假设清单

1. 【未证实】引用的测试通过数（targeting 67/67、projectile 8/8、damage 5/5、triggering 10/10 等）均为源报告转引文档，本轮未运行构建/测试（与源报告同口径）。
2. 【未证实】modifiers/attributes 包（02 报告）的修饰器聚合细节未读——ModifyAttribute 原语只锚定 ability 包 AttributeEffectComponent 源码点，聚合公式面待 B 路（Buff/修饰器）提取时补全。
3. 【未证实】`IActionSchema` 的运行期调用点密度（Schema 注册后在何处被 ParseArgs）未逐行追——04 报告仅证实注册面与校验器面。
4. 【推断】我方原语参数面中标注〔提案〕的项（堆叠策略、Any 并行、矩形/环形形状、Mixed 两段结算、命中后动作子链等）均为设计提案，无 AbilityKit 源码背书。
5. 【假设】我方 FEffectStep 以"步骤=数据+词汇类 Execute"对照 AbilityKit"阶段=类实例+工厂克隆"为同构映射；若我方后续引入步骤级并行执行器对象，对照结论需复核。
6. 【假设】SelectTargets 独立原语判据基于"结果集复用"需求；若实测发现绝大多数词缀是"选完即打"，可再加 Damage 内联目标参数面作语法糖（不改变两步正交的底层）。
