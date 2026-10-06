# [SPEC-03-effects](../spec/04-module-effects.md) — TcsEffect 效果执行层设计（v2 定稿重写）

- **文档 ID**：`SPEC-03-effects`
- **类型**：SPEC / 模块规格
- **状态**：ACTIVE
- **权威范围**：TcsEffect（M4）链与步骤、触发行、目标选择、宿主插槽；理由住 LOG-02-effects
- **最后更新**：2026-10-06

> **换根注记（2026-10-01）**：本文 tag 名已随提案 `reroot-gameplay-tag-vocabulary` 换根——旧前缀 `Tcs.Event.*` / `Tcs.Flow.Key.*` / `Tcs.Flow.Template.*` / `Tcs.Attr.*` / `Tcs.Chain.*` 依次成为 `TcsEvent.*` / `DamageFlowKey.*` / `DamageFlowTemplate.*` / `Attribute.*` / `EffectChain.*`；本文正文一律用新名，旧名仅存于本注记与 `log/`、`ledger/`、`evidence/` 等历史文件。

- 日期：2026-09-02
- 状态：**v2 定稿**——全部增补（D4 修订/D7 瞬时流程/Authoring 边界/端到端走查/伪代码）已折入正文；修订记录见文末
- 职责一句话：**"事件 → 触发行 → 效果链"的触发执行引擎 + 原语解释器——系统做流转的流转本体。**

## 1. 模块边界

- 消费者：M3（生命周期事件被订阅）、M5（技能编排链）、M6（权威/镜像接线）、TcsDamage/TcsTargeting/TcsCue（领域步骤注册进执行器注册表）。
- 依赖：TcsCore、TcsAttribute。**注册制分派（D4-14）**：本模块是纯机制层（解释器+控制流步骤+执行器注册表+自注册宏），**不依赖上层领域模块**（依赖铁律 `Core ← {Notation, Attribute} ← Effect ← {Damage, Targeting, State} ← Skill`；`TcsAttribute` 在它**下层**，允许——**2026-10-05 首次真实使用这条边**：`ModifyAttribute` 步骤与 `AttributeCompare` 条件经 `FTcsEffectAttributeAccess` 取属性门面）；Damage/Heal/ModifyFlow 步骤+执行器住 TcsDamage（09 文档）、SelectTargets 住 TcsTargeting、ApplyState 住 TcsState、PlayCue 住 TcsCue；反向禁止。

## 2. 类型词汇（对外）

### 2.1 链与步骤（D4-3 终版：15 原语）
- `FEffectStep`：`FInstancedStruct` 容器；步骤类型 = **15 个数据 struct（D4-16 终版）**，按归属分两半——**本模块 9**：控制流 6（WaitDelay/**WaitEvent**/Branch/Parallel/Repeat/RunSubChain）+ ModifyAttribute + SetVar + OnError；**领域模块自注册 6（D4-14）**：Damage/Heal/ModifyFlow（TcsDamage）、SelectTargets（TcsTargeting）、ApplyState（TcsState）、PlayCue（TcsCue）。修订史：WaitUntil→WaitEvent（事件回调挂起替代条件轮询）、Gate 并入开闸事件、ModifyFlow 新增（提交流程属性修正，见 09 文档）、SpawnProjectile/SpawnArea 移除（D4-16）。
- **原语实现状态（2026-10-05，R5 Task 6 回写）：15 个中已落地 10 个** —— 本模块 6（`WaitDelay`（R3）/ `WaitEvent` / `Branch` / `RunSubChain` / `SetVar`（四者 R4 Task 3.5，提案 `add-chain-primitives-and-target-sorting`，证据 `EVID-2026-10-04-chains-primitives`）/ **`ModifyAttribute`（R5 Task 6，2026-10-05；本模块对下层 `TcsAttribute` 的首次真实使用）**）+ 领域侧 4（`Damage`（R3）/ `ModifyFlow`（R4 Task 3，端到端见 `EVID-2026-10-04-modifier-channel`）/ `SelectTargets`（R3）/ **`ApplyState`（R5 Task 6，2026-10-05，住 TcsState）**）。**余 5 个**：`Parallel` / `Repeat` / `OnError`（R5.5-a）、`Heal`（R5.5-b）、`PlayCue`（R8，随 TcsCue）。**节内其余描述仍是设计意图**——"这一轮到哪儿"的权威登记在 `plans/plan-r5-state-layer.md` 注记与 `LEDGER-deferred`，本节只标已落地项，MUST NOT 当作"全部已实现"来读。
- `FEffectChain`：有序步骤数组 + 元数据（`MaxStepsPerFrame` 熔断上限，默认 CVar 可调）。
- `FEffectContext`（黑板，可池化）：`Caster / Instigator / EventPayload(FInstancedStruct) / Targets / Variables / **CapturedAttrs** / 注入引用（ICombatEntityQuery/IRelationResolver 等宿主能力契约，链构建时装配）`——黑板即上下文（与 09 文档同构）。
- **原语集合扩展 = 新增 step struct（Authoring 第三层）**；Custom 逃逸位规约管策略枚举，不管类型族。

### 2.2 触发行（D4-1 终定 10 字段）
`EventTag / EventPayloadFilter / Conditions / Effects / Priority / ExecutionGate / InterruptPriority / GateTags / Cues(CueId 引用) / bConditionMissIsSilent`。
- 全部独立可空、零策略类；CueId 是引用（Niagara/贴花/MPC 参数映射住 TcsCue/宿主）；~~Scope~~~~HandlerClass~~ 已砍除（目标归属 = SelectTargets 步骤显式解析或 Context 默认目标——D4-4 v2；求值器本身就是共享 Handler）。
- **条件最小集（D4-5 已拍板）**：`HasAllTags / AttributeCompare(Attr,Op,Value|Attr) / VariableCompare / GateCheck(读 BoolSwitches) / Chance(概率)` + Custom 逃逸位——无表达式语言。

**实现状态（2026-10-04，R4 Task 1 / 2 / 2.5 / 4 收束回写）——字段级"落地 / 留位"对照**：

| 字段（终定名） | R4 状态 | 说明 |
|---|---|---|
| `EventTag` | ✅ 活字段 | 总线按 Tag 路由（门①）；同 Tag 多行**共用一个订阅**（计数配对） |
| `Conditions` | ✅ 活字段 | 走条件注册表 `FTcsTriggerConditionRegistry`（门④）；未登记类型 ⇒ 不通过 + 日志 |
| `EffectChainId` | ✅ 活字段（**改名**） | 设计期名 `Effects`，Task 1 形态精修改为 `EffectChainId`（用户拍板"更直观"）；命中后 `ExecuteChain`，链未登记由门面既有拒绝面兜 |
| `Priority` | ✅ 活字段 | 大者先；同级按登记序（显式全序 `(Priority 降序, 槽位下标升序)`，不依赖输入序） |
| `ExecutionGate` | ✅ 活字段（**仅两枚举值同判**） | 门②；R4 单机形态下本地即权威 ⇒ `TEG_Always` 与 `TEG_AuthorityOnly` 同判，真正的权威闸归网络姿态轮 |
| `GateTags` | ✅ 活字段 | 门③；非空要求**全亮**（`SetTriggerGateTag`），空数组恒通过 |
| `bConditionMissIsSilent` | ✅ 活字段 | 条件未过：`true` 静默跳过 / `false` 记 `Verbose`（MUST NOT Warning） |
| `EventPayloadFilter` | ⏸ **留位（只存不裁）** | 零消费者；等首个带可筛字段的载荷类型 |
| `InterruptPriority` | ⏸ **留位（只存不裁）** | 零消费者；等链打断语义轮 |
| `Cues` | ❌ **已删除** | Task 1 形态精修删除（TcsCue 模块未敲定，留字段 = 给策划假控件）；TcsCue 落地时加回 |

- **载体（R4 Task 2.5）**：独立资产 `UTcsEffectTriggerDefAsset`（住 TcsIntegration，`TriggerTag` 为内容身份）经 DefLibrary 发现并**装配为触发行**（`Source` = 定义库来源句柄）；**内联位**（`SkillDef` / `BuffDef` 的 `TArray<FTcsEffectTriggerDef>`）待 R5/R6。
- **条件落地面（2026-10-05 R5 收束订正）**：**三条**已实现——`HasAllTags` / `Chance`（R4）+ **`AttributeCompare`（R5 Task 6，第三条内置条件，本模块对下层 `TcsAttribute` 经 `FTcsEffectAttributeAccess` 取门面；实测能门控起链，见 `EVID-2026-10-05-state-chain-primitives` 检查 23f）**——**内置条件与自定义条件走同一注册表**（不分内外两套路径）；`TcsTriggerCondition.cpp` 现有三条自注册宏与该清单逐条对应。**未落地**：`VariableCompare`（等变量存储消费者）/ `GateCheck`（读 M5 `BoolSwitches`，归 R6）/ Custom 逃逸位——登记在台账 `TRIG-5`。
- **求值顺序 = 四道门**（R4 Task 2）：`事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`——顺序有意义：网络闸最廉价先判，条件最贵最后判。
- **载荷读取器**（Task 2 机制 + Task 3 首个属主登记 + **R-2 后段宿主插槽 2026-10-06**）：`FTcsTriggerPayloadReaderRegistry` 由**载荷类型的属主模块**自登记（TcsDamage 的收集事件读取器给出 `Caster ← Attacker`、分类标签直通）——机制层不认识任何领域载荷类型，这是依赖铁律的兑现形态。
- **★ 两张注册表的宿主脚本插槽（`R-2` 后段，2026-10-06 落地，`PLN-R6` Task 0）**：条件求值器与载荷读取器此前只有 C++ 静态自注册路径（宿主专属条件类型 / 专属载荷读取器无法用脚本定义）。现补宿主口：**宿主契约** = `UTcsTriggerConditionEvaluator`/`ITcsTriggerConditionEvaluator`（`UINTERFACE(MinimalAPI, Blueprintable)`，`bool Test(const FInstancedStruct&, const FTcsTriggerContext&, double)`）与 `UTcsTriggerPayloadReader`/`ITcsTriggerPayloadReader`（`FTcsTriggerPayloadInfo Read(const FInstancedStruct&)`）；**门面四口** = `RegisterConditionEvaluator` / `UnregisterConditionEvaluator` / `RegisterPayloadReader` / `UnregisterPayloadReader`（均 `UFUNCTION()`，返回 `bool`）。**注册值仍为 `TFunction`、无转发器**——门面把 `TScriptInterface` 包成 `TFunction` 转发进既有注册表，**键与查表逻辑零改动**。**"每事件一次"语义保住**（读取点在行循环外，`TcsTriggerEvaluator.cpp:43`）。**拒绝面判在门面**（`Register` 返回 `void` ⇒ 门面须自行 `Find` 判同键，否则调用侧分不出拒绝与成功）；**强持有** = 门面 `UPROPERTY` 数组（`TScriptInterface` 本身不构成 GC 强引用），与 2026-09-29 的寿命弱引用表互补成两半。`FTcsTriggerContext` / `FTcsTriggerPayloadInfo` 为此升格 `BlueprintType`（承诺面零代价：消费它们的门面方法仍是无 specifier 的 `UFUNCTION()`）。**准入状态**：双配置编译 0/0 已闭环，**运行期往返取证待执行**。
- **留位项与剩余条件的唯一待办登记** = `LEDGER-deferred` 的 `TRIG-5`（2026-10-04 补登）。

### 2.3 目标选择（D4-4 终定；**v2 策略化**——D4-4 v2/D4-15 v2，规格详见 [SPEC-06-targeting](../spec/10-module-targeting.md)）
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
- **语言无关执行器（D4-17 终定）**：插件**不内嵌任何脚本引擎**（AngelScript/C#/TS 是宿主选择，插件不关注）；注册**双入口**——C++ 静态自注册宏 + **反射可达动态委托入口**（脚本层调用同一注册表）；蓝图理论可行（动态委托）不作为设计目标。
- **Skill Logic 需要代码的四条路径**（代码技能是一等公民）：①Custom 原语类型；②决策 Fragment；③链外代码 + 事件协作；④整技能代码化（单 Custom 步骤链，账本/冷却/门禁/打断照常）。框架不强制策划化。

## 5b. 宿主扩展路线：插槽（2026-09-24 确立并落地，台账 SCRIPT-8）

> **证据状态（2026-09-28 增量）**：原 SCRIPT-8 静态/glue 与 C# 公式子集证据仍成立；本次单次 PIE 另验证 Effect 脚本步骤同步标记读回、`TSR_Running → ResumeRun → Completed`、selector/filter、悬空及代际失配句柄，并在原生 `Obj GC` 后再次执行宿主对象（见 `evidence/2026-09-28-host-scripting-e2e-pie.md`）。进程级步骤注册表跨 PIE 保留旧世界转发器的问题仍由 `reflection-backlog.md` R-2 追踪；AS/Luau/TS 未独立实测。

**问题**：客制化、只服务宿主业务、不值得进插件的语义（"选最近的敌人"、宿主专属伤害公式、读宿主独有状态的新步骤）——**不该写 C++ 进插件仓**，但框架必须给它们一个入口。

**方案：框架只提供"插座"抽象，宿主用任意 UE 脚本语言填实现。**

- **机制 = UObject 接口 + `UFUNCTION(BlueprintNativeEvent)`**——这是 **UE 原生反射分发**（`UFunction::Invoke`），**不是 C# 专属**：AngelScript / Luau / Puerts(TS) / 蓝图全部支持它 ⇒ **天然语言无关**，正是 D4-17"宿主选择脚本引擎"承诺的落地形态。
- **本项目已有活先例**（非推测）：`UTcsEventHandler`（C# 覆写 `HandleEvent_Implementation` 生效）与 `ITcsAttributeProvider`（3 个 `BlueprintNativeEvent`，C++ 经 `Execute_GetCurrentValue` 调到）——**同代码库内已实测**。
- **★ 扩展点的脚本可达性由分发机制决定，三类分明**（本路线的方法论内核）：

| 机制 | 现状用于 | 脚本可达 | 处置 |
|---|---|---|---|
| **虚分派（vtable）** | 选择器 / 过滤器 / 参数源 | ❌ **物理不可达**（脚本 struct 无 C++ 类型 ⇒ `CppStructOps == nullptr` ⇒ vtable 位为 0 ⇒ 野调用；**引擎层面无解**） | 加"宿主委托"插槽类型**转发** |
| **注册表 + `TFunction`** | 步骤执行器 / 条件求值器 / 载荷读取器 | ⚠️ **差一层签名**（键可反射、值不可） | ~~加 UObject 执行器基类替代注册值~~ → **订正（2026-10-06，`R-2` 后段落地）**：**保留 `TFunction` 注册值**，改在门面加"键 + `TScriptInterface`"登记口，由门面把脚本对象包成 `TFunction` 转发进既有注册表；宿主契约用 `UINTERFACE(MinimalAPI, Blueprintable)` + `BlueprintNativeEvent`。**理由**：语言可达性只由"宿主面对的契约是否为 `UObject` 反射类型"决定，**与注册值载体无关**；而 `TFunction` 注册值**没有策略基类可继承 ⇒ 转发器无对象可接**，走"替换注册值"就得上 `USTRUCT` 策略基类 = SCRIPT-2 路线（SCRIPT-8 已明写"改动面大得多"而刻意绕开）。**依据与四处先例见 `LEDGER-reflection`《宿主插槽家族的耦合关系》** |
| **UObject + `BlueprintNativeEvent`** | `UTcsEventHandler` / `ITcsAttributeProvider` / **条件求值器 / 载荷读取器（2026-10-06 起）** | ✅ **可达**（已实测） | **插槽路线的底座** |

- **三类插槽（已落地，台账 SCRIPT-8）**：
  - **①目标选择/过滤**：`ITcsTargetSelectorHost` / `ITcsTargetFilterHost`（`UINTERFACE + Blueprintable`）+ 转发器 `FTcsSelHostDelegate` / `FTcsFilterHostDelegate`（USTRUCT 策略子类，持 `TScriptInterface` 纯转发）。**转发是必需的**——选择器族走虚分派，脚本物理不可达。
  - **②步骤执行器**：`UTcsStepExecutor`（`UCLASS(Abstract, Blueprintable)`，`BlueprintNativeEvent Execute(FGameplayTag, FTcsChainRunHandle, const FInstancedStruct&)`）——门面 `RegisterStepExecutor` 把它包成 `TFunction` 转发进既有注册表（**键与查表逻辑零改动**）。流程侧同款：`UTcsFlowStepExecutor`。
  - **③伤害流程**：`ITcsDamageFlowDelegate` 5 方法换签名（上下文 → `FTcsDamageFlowContextView` 反射视图）+ `BlueprintNativeEvent` + C++ 调用点改 `Execute_`；流程模板登记面（`RegisterTemplate`）反射化。
- **★ 关键设计手法：传句柄、不传上下文**——插槽签名用 `FTcsChainRunHandle` 等**句柄**，上下文经**门面按句柄访问器**（`GetRunTargets` / `SetRunTargets` / `TryGetRunVariable` / `SetRunVariable` / `GetRunCaster` / `GetRunInstigator`）读写。**收益**：绕开"上下文反射化"（`FTcsDamageFlowContext` 深处嵌 `TFunction`，**物理不可能整体反射化**）——台账 SCRIPT-3 因此从必经之路降为可选。
- **双轨并存**：框架内置语义走 **C++ 快路径**（`TFunction` / 虚分派），插槽只服务**宿主扩展**。理由：`UFunction::Invoke` 比 `TFunction` 慢，而宿主客制化语义不在热路径上。
- **★ 注册表的寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤；修反射册 R-2 跨世界寿命缺陷）**：4 张注册表（步骤执行器 / 流程步骤 / 条件求值器 / 载荷读取器）**仍是进程级单例**（静态自注册纪律不变），但**动态登记项**必须能判"还属不属于当前世界"：
  - **登记**：动态入口一并记录**宿主对象弱引用** + **登记世界弱引用**；静态自注册的纯函数项不建寿命条目（永不过期）；
  - **失效判据**（任一即失效）：对象弱引用为空（已被 GC）/ 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询**：`Find` 带可选世界入参（调用方持有世界时 MUST 显式传入）；跨世界失效 MUST 视为未命中并**移除条目 + 留 Warning**（MUST NOT 静默按"未登记"处理——那会把"世界已更换"表现成"类型写漏"）；
  - **拒绝门 = "同世界活对象重复"**：既有条目**已失效** ⇒ **替换**（不 ensure）；**有效且同世界**（或静态自注册项）⇒ 沿用拒绝（ensure + 保留首个）。这条替代了原先"只看键重复"的口径——旧口径会让失效条目毒化后续所有 PIE；
  - **撤销**：4 张注册表提供 `Unregister` + `GetDynamicKeys`（静态项 MUST NOT 可移除）；门面 `Deinitialize` 按世界撤销本世界登记的动态条目。**但它是整理手段而非正确性前提**——失效判据自足，即使从不调用也不会解引用已失效对象（对应用户 2026-09-27 裁定"TCS 侧兜底优先，注销作退路"）；
  - **`UPROPERTY` 强持有 MUST 保留**：与弱引用是互补的两半——强持有管"别被收走"，弱引用管"收走了/换世界了就失效"，缺任一半都有洞（先例：WAIT-8 的静默回收）。

**判据（哪些写 C++、哪些走插槽）**：

| 情形 | 归属 |
|---|---|
| 框架内置语义 / 热路径 / 需引擎级不变式 | **C++**（进插件） |
| 客制化 / 只服务宿主业务 / 不值得进插件 / 非热路径 | **插槽**（宿主脚本） |

**约束（实施期两条硬约束 + 一条 GC 纪律）**：

- 插槽接口的所有形参 MUST 全反射（"C# 四问"硬约束——这也是为什么传句柄而非 struct）；`BlueprintNativeEvent` 会触发 UHT 的蓝图参数校验（`UhtFunction.cs:UhtFunction.Validate`）⇒ 形参/返回**连句柄都必须 `BlueprintType`**（故 `FTcsChainRunHandle` 于 2026-09-24 放宽——理由与零代价论证见 `effect-chain` 规格）。
- **非反射纯 C++ struct 不能作 `UFUNCTION` 形参**（UHT 报 `Unable to find 'struct'`）⇒ 需要"上下文"时必须做**反射视图**（`FTcsDamageFlowContextView` 只摘可反射数据面；黑板因含 `TFunction` 物理不可反射而**不在**视图内）。**这修正了台账 SCRIPT-8 的原设想**（原写"`ITcsDamageFlowDelegate` 5 方法补 `UFUNCTION`"——做不到，必须换签名）。
- **GC 可见持有**：门面 MUST 以 `UPROPERTY` 数组持有已登记的脚本执行器——裸 C++ 注册表不经 GC 的 `RefLink`，不持有则脚本执行器被**静默回收**，表现为"步骤不生效"而非崩溃（与模板登记表的 WAIT-8 缺陷同款形态）。
- 插槽**不扩大蓝图承诺面**（蓝图恰好也能用，但不是承诺项，R0 §9）。

**★ C# 侧覆写写法（实测，两类不同）**：

| 插槽载体 | C# 写法 |
|---|---|
| **UINTERFACE**（`ITcsDamageFlowDelegate` / `ITcsTargetSelectorHost` / …） | 写**两半**：①签名分部声明（无体、**无 attribute**）②`<名>_Implementation` 分部方法（带体）。给①加 `[UFunction]` 会让生成器再发一份 ⇒ CS0102/CS0111 |
| **UCLASS**（`UTcsStepExecutor` / `UTcsFlowStepExecutor`） | 直接 `override` 生成器发出的 `public virtual` 方法 |

## 6. 网络姿态落点（NET-1/2）

- 链执行**权威侧全量**；客户端镜像只跑 Cue 类步骤（复制的触发事件驱动，CueId+上下文快照）——接口位本期不实现；ExecutionGate 字段 = 策划可见的网络闸。

## 7. 非目标

不做 Timeline（曲线参数留活口）；不做图编辑器（M8 只读视图先行）；不做条件表达式语言；不做公式；**不内嵌任何脚本引擎**（宿主选择 AS/C#/TS）。

**脚本调用层（2026-09-24 更新）**：门面 API 的**反射面已落地**（提案 `add-scripting-reflection-surface`，PIE 实测通过）——`UTcsEffectSubsystem` 的链登记/触发登记/执行查询/点灯等门面方法、以及 `UTcsAttributeSubsystem` 的实体注册与属性读写，均以 `UFUNCTION()`（**无 specifier**）标记，宿主脚本层（UnrealSharp/C#）可直接调用。

- **无 specifier 是有意的**：形参含 `FTcsEffectChain` / `FTcsEffectTriggerInstance` 等 `USTRUCT()` 非 `BlueprintType` 载体，加 `BlueprintCallable` 会被 UHT 的蓝图参数校验拒绝；无 specifier 时 UHT 不校验参数、脚本层照常可达。
- **蓝图侧仍不承诺**（R0 §9）：本批标记**不**扩大蓝图承诺面——这是"语言无关执行器"预留的兑现，不是蓝图支持。
- **证据分级（2026-09-27，见 §5b 与台账 S 系列）**：
  - **静态实现 + glue 已完成**：SCRIPT-8 三类插槽、反射视图、门面 6 个按句柄访问器和流程模板登记面均已落地；`TcsDamageFlowDelegate.generated.cs` 已从零方法空壳变为 5 个可覆写方法。
  - **C# PIE 已验证子集**：脚本模板登记、`CalculateBaseDamage` 经 `Execute_*` 抵达、句柄访问器往返以及挂起链唤醒完成。
  - **仍未由本次证据覆盖**：GC 强保活、脚本选择器/过滤器、脚本步骤执行器、悬空句柄安全语义和 AS/Luau/TS 往返；不得把“插槽已实现”外推成上述行为均已 E2E 验证。
  - **定位**：SCRIPT-3 上下文反射化降为可选；SCRIPT-2 三张注册表换签名低于 SCRIPT-8；SCRIPT-5 的“脚本遍历全世界”仍是 C++ 专用边界；SCRIPT-1、SCRIPT-4、SCRIPT-6、SCRIPT-7 已消费。

## 8. 依据

- 拍板：D4-1~D4-4 + D4-5 + D7-1~D7-4（2026-09-02 多轮问答框；用户贡献：Scope/HandlerClass 砍除、Cues 引用制、目标选择下沉每步骤、WaitEvent 提案、EntrySelector 纠正、Authoring 边界确认）+ Authoring 三层模型 + 分离宪法 R0 §8。
- 证据：TCS 09 核验（无触发行/无链/无前摇后摇——净新增；SourceHandle 回收链继承）；AbilityKit Pipeline/Triggering 对照；启发文档《伤害修改器设计》全文。

## 9. 端到端走查样例（2026-09-02）

总纲：策划写的是三张表的数据行（状态词表行/触发行/链）；运行时自动性来自三机制：生命周期与时钟泵按声明发事件、触发行求值器自动匹配、解释器按数据执行。定义加载期把触发行订阅挂上总线，之后事件驱动闭环，无策划调用。

### 例一 Buff「灼烧」（每 2s 对宿主 5% 最大生命火伤，10s，叠 3 层）
- 配置：StateDef_Burn（五轴 GroupBy=PerSource/Capacity→MaxStacks=3/RejectNew/AddValues/RefreshToTotal + Duration 10s + Period 2s）；触发行 TR_Burn_Tick（EventTag=`TcsEvent.State.Periodic` + Filter DefTag=`<Burn 状态 tag>` + Effects=Chain_BurnTick + Cues）；链 Chain_BurnTick（单步 Damage{目标=EventTarget, 量=Param(BurnPct)×MaxHealth}）。**标识 2026-09-22 tag 化**：触发行 Filter 与链 id 均为 `FGameplayTag`（本节为走查预演形态，示意名不写全 tag 路径）。
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
- v2 增补 4（2026-10-04，R4 收束回写）：**§2.1 补原语实现状态**（15 个中已落地 8 个 + 余 7 个的归属轮次）；**§2.2 补字段级"落地 / 留位"对照表**（七个活字段 / 两个留位字段 / `Cues` 已删除）+ 载体与条件落地面 + 四道门顺序 + 载荷读取器机制；**§12 补 R4 验收实证与留白**。落点 = R4 轮收束（`PLN-R4` Task 5；证据 `EVID-2026-10-04-trigger-def-asset` / `-chains-primitives` / `-modifier-channel`）。
- v2 增补 5（2026-10-05，R5 收束回写）：**§2.1 原语实现状态 8 → 10**（+`ModifyAttribute` / `ApplyState`，两者均 R5 Task 6）；**§2.2 条件落地面订正**（`AttributeCompare` 由"未落地"改为已实现——该行自 R4 起即过期）；**§12 补 R5 增量对账表**（`ApplyState` / `ModifyAttribute` / `AttributeCompare` / 内联触发行 / 链运行态可读面 五项 ✅，`Parallel`/`Repeat`/`OnError` 与打断仍 ⛔）。落点 = R5 轮收束（`PLN-R5` Task 8；证据 `EVID-2026-10-05-state-chain-primitives` / `-state-layer-pie`）。
- v2 增补 6（2026-10-06，R6 Task 0 = `R-2` 后段落地回写）：**§2.2 载荷读取器段补"宿主脚本插槽"条**（两契约 + 门面四口 + 无转发器 + 每事件一次 + 拒绝判在门面 + 强持有/弱引用两半）；**§5b 三类插槽的机制表订正两行**——"注册表 + `TFunction`"行的处置由 **"加 UObject 执行器基类替代注册值"** 改为 **"保留 `TFunction` + 门面加登记口"**（原字面被 `R-2` 形态裁定取代，**保留原文并划改**），并把条件求值器 / 载荷读取器并入"UObject + `BlueprintNativeEvent`"那一行的"现状用于"列（它们自本轮起确属该机制）。**同批**：`最后更新` → 2026-10-06。落点 = `PLN-R6` Task 0；形态依据见 `LEDGER-reflection`《宿主插槽家族的耦合关系》；**准入状态 = 编译面已闭环、运行期往返取证待执行**。

## 12. 验收钩子

竖切验收：测试链 `WaitDelay → SelectTargets(单体策略) → Damage` 走 TcsDamage 骨架（10 文档 v2 验收钩子——§9 例二为全量预演形态）；熔断与打断、WaitEvent 挂起唤醒、重定向挂/摘在竖切后人工检查路径。

**R4 收束时的实证状态（2026-10-04）**——本节各钩子逐条对账，**未打勾的仍是留白，不得读成已验**：

| 验收钩子 | 状态 | 证据 / 留白 |
|---|---|---|
| **"事件 → 触发行 → 效果链"闭环** | ✅ **已实证** | 内容资产（`UTcsEffectTriggerDefAsset`）→ 定义库发现与装配 → 事件命中 → 挂起 → 唤醒 → 完成，四轮 PIE 含失败面与引用链预检；`EVID-2026-10-04-trigger-def-asset` |
| **伤害修改器唯一通道（D7-6）端到端** | ✅ **已实证** | 破甲单步链 `ModifyFlow{Op=Mul 0.5}` 提交 `DamageFlowKey.BaseDamage` ⇒ 30 → 15，含对照组 / 摘行还原 / 按来源级联摘除 / 载荷读取器 `Caster ← Attacker`；`EVID-2026-10-04-modifier-channel`（两轮 19/0 逐字一致 + `.Reject` 4/0） |
| **WaitEvent 挂起唤醒** | ✅ **已实证** | 共享订阅 + 门面等待表；订阅计数配对与解锚范围见 `EVID-2026-10-04-chains-primitives` |
| **熔断（单帧步数 / 嵌套深度）** | 🟡 **部分** | 嵌套深度上限（16）已实测：17 层起链 ⇒ 深度 17 熔断 ⇒ 16 层父链逐一回卷完成。**单帧步数预算本身未被触发**（实测累计步数 17，未达共享预算 64）；**"三条降级路径"（无时钟 / 无总线 / 未注入查询）留白**，归 M5 |
| **打断** | ⛔ **未验** | 无公开取消/释放入口（台账 `CHAIN-6`）⇒"运行态释放即解锚"的事件锚分支不可达；"打断与取消"归 M5 轮 |
| **重定向挂/摘** | ⛔ **未验** | `FFlowRedirect` 模板重定向整套未落地（台账 `STAT-3`，归 R5） |
| **GateTags 开闸事件**（§2.4 唤醒源④） | 🟡 **部分** | 门③（`GateTags` 全亮才通过）已实证，并成为内容行的启停控制面；**"开闸即发事件 + `WaitEvent` 语法糖"未实现** |

**R5 收束时的增量对账（2026-10-05）**——本节在 R4 表之上补 R5 触及的钩子，**未打勾的仍是留白**：

| 验收钩子 | 状态 | 证据 / 留白 |
|---|---|---|
| **`ApplyState` 领域步骤端到端** | ✅ **已实证** | 链里首次能施加状态：`FTcsStepApplyState` 自注册生效、`Source` = 黑板 `RunSource`；端到端（内容资产驱动的链 → 状态实例 → 全生命周期）见 `EVID-2026-10-05-state-layer-pie`；单元面见 `EVID-2026-10-05-state-chain-primitives`（23a/23h） |
| **`ModifyAttribute` 链原语（机制侧私有写入原语）** | ✅ **已实证** | 本模块对下层 `TcsAttribute` 的**首次真实使用**（经 `FTcsEffectAttributeAccess`）；`Source` = `RunSource`。**边界**：它落的是**修正器层**，效力**不等同** GAS Instant（会被 `Override` 整体抹掉、参与复利、可按来源摘除）⇒ 台账 `CHAIN-8`；且链挂条目**无框架侧回收触发点**（`CHAIN-7`） |
| **`AttributeCompare` 条件门控** | ✅ **已实证** | 检查 23f：同一份含 `ModifyAttribute` 的链体，条件过则改得动账本、不过则账本不动；`FTcsTriggerContext` 补 `World`（求值器填入）——**该字段的落地同时暴露一处跨文件缺陷**（`TcsStepBranch` 也构造该上下文却未填 `World` ⇒ 条件会静默恒不通过） |
| **内联触发行（Def 内联位）** | ✅ **已实证** | 施加时以实例 `CascadeAnchor` 登记（排在 `Applied` 广播之前）、移除/到期按同一锚点级联退订（排在广播之前）；`UnregisterTriggerRowsBySource` 单点调用。**边界**：本轮只验内联位，独立资产位在 R4 已验，两者共用登记机制 |
| **链运行态身份与因果边的可读面** | ✅ **已实证** | `RunSource`（每运行一枚新号、子链另发）+ `CausedBy`（触发行起链 = 该行 `Source`）；两个只读读数口 `GetRunSource` / `GetRunCausedBy`。**未覆盖**：**溯源树**（祖先遍历 / Explain 面板 / 按血缘回放）归 R8（台账 `TOOLS-7`）。**因果边 MUST NOT 参与撤销或共存判定** |
| **`Parallel` / `Repeat` / `OnError`** | ⛔ **未验** | 三者结构体存在但执行器未实现（归 R5.5-a）；`OnError` 的"Explain 线索"产出面另归 M8（`TOOLS-6`） |
| **打断 / 取消** | ⛔ **未验** | 与 R4 同（无公开取消入口，台账 `CHAIN-6`）；R5 未改变这一点 |
