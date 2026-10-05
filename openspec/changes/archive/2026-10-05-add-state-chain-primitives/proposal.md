# Change: 链原语与行为面接线（ApplyState / ModifyAttribute / AttributeCompare）

## Why

R5 的状态层至今**没有人能从链里施加它**：`FTcsStepApplyState` 不存在（`TcsState` 下零步骤 struct、零 `UE_DEFINE_EFFECT_STEP_EXECUTOR`），`D4-16` 十五原语里的 `ModifyAttribute` 也仍未落地（`04 §2.1` 已落 8 / 15）；触发行侧的两条内联位（`FTcsBuffDef::Triggers`，Task 1 只落形状）今天**零登记调用点**——"状态在，行为就在"没有接线处。链能改属性、能施加状态、buff 自己的触发行能在自己的生命周期事件上起链，是 R5 收口的最后两块拼图。

本变更同时解开两处**实施期核出的错前提**（裁定 2 / 3）：链运行态**根本没有来源位**（`FTcsChainRun` 全 11 字段、`FTcsEffectContext` 全 5 字段已逐字核过，`Self` 是运行态自身句柄、`Caster`/`Instigator` 是实体句柄，类型都换不成 `FTcsSourceHandle`），以及"同一个状态 = 同一个来源"在**一个来源施加多个不同定义**时不成立（会互相摘除，裁定 1）。

## What Changes

- **`FTcsStepApplyState`（新，住 TcsState）**：链里施加状态。字段 = `Target`（无效则取黑板 `Targets[0]`，仍无效 ⇒ 软失败）/ `DefTag` / `Overrides`；`Source` 取黑板 `RunSource`、`Instigator` 取黑板 `Instigator`。失败一律 `Warning` / `Error` + `TSR_Completed`（**不断链**）。
- **`FTcsStepModifyAttribute`（新，住 TcsEffect）**：把一条一次性 `FTcsAttrModInstance` 写进属性账本——`Op` 复用 `ETcsAttributeOp`（不新造词表）、运算数复用 `FTcsParamValue`，`Source` = 黑板 `RunSource`。这是 `TcsEffect` 对 `TcsAttribute` 的**首次真实使用**（编译边早已存在，代码里零 include）。
- **`AttributeCompare` 触发条件（新）**：条件数据 `{ Attribute, Comparison, Threshold }`，求值经属性门面 `EvaluateCurrent` 读触发上下文 `Caster` 的当前值；比较枚举 `ETcsAttributeComparison` 住 `TcsAttribute`（属性值域词汇），条件 struct 与 `HasAllTags` / `Chance` 同处。
- **链运行态的"身份 + 因果边"两个字段（新机制，裁定 2）**：`FTcsEffectContext` 补 `RunSource`（本次运行的来源锚点：`ExecuteChain` 在黑板未自带时**发放一枚**进程唯一新号 ⇒「同一次链运行 = 同一个来源锚点」，子链**不**继承、另发新号）与 `CausedBy`（**因果边**：启动本次运行的那个来源句柄——触发行起链时 = 该行的 `Source`，子链起链时 = 父链的 `RunSource`，宿主直调可留无效）。**边只作溯源读数，MUST NOT 参与撤销或共存判定**（撤销一律走状态实例的级联锚点）。
- **内联触发行接线（`TRIG-4` 的行为半）**：`Apply` 时逐条 `Def.Triggers` 以**该实例的级联锚点**为 `Source` 登记；`Remove` / 到期时按同一锚点级联退订。**登记排在 `Applied` 广播之前**（新登记的行要能看见自己的 `Applied`），**退订排在 `Removed` / `Expired` 广播之前**（它不该看见自己的死亡事件）。
- **级联锚点与施加方身份解耦（裁定 1）**：`FTcsStateInstance` 补 `CascadeAnchor`（每实例恒发新号）；修正器条目与触发行一律以它作锚点，`Source` 退回"谁施加的"（续杯 / 叠层判据 + 事件载荷）。
- **属性门面查找收敛为单载具（裁定 5）**：`UTcsAttributeSubsystem::Resolve(const UWorld*)` 成为唯一查找点；两模块各留自己的白名单薄壳。
- **四条既有规格要求的措辞订正**：`state-instance-lifecycle` 的实例字段表与"来源每次施加取新值"那句、`state-modifier-materialization` 的"按来源级联摘除"与"来源句柄的身份一致性"、`effect-trigger` 的"`Source` = 该状态实例句柄"。

## 待评审的裁定（逐条给判据）

1. **【用户已拍板 2026-10-05】级联锚点与施加方身份解耦**（甲案）。判据 = 现状下**一个来源施加两个不同定义**（一条技能给同一目标挂两个 buff）时，撤一个会按来源值把另一个的修正器与触发行一并摘掉，而那个实例还活着 ⇒ "状态在，修正器没了"的不一致态。解耦后锚点每实例唯一，撤销精确到实例；`Source` 保留原语义。**代价如实登记**：`FTcsStateInstance` +1 字段；两条规格 MODIFIED；`effect-trigger` 的"该状态实例句柄"精确化为"该实例的级联锚点"（措辞与语义对齐，非改名）。
2. **【已拍板 2026-10-05】身份与因果边分两个字段**（甲案；起因 = Q2-A 原描述里"把该行 `Source` 注进去"经核实有害：定义库登记的系统级触发行**全部共用一枚**来源句柄（`TcsDefinitionSubsystem.cpp:31` 一枚 `TriggerSourceRegistry.Allocate()` 供全部行），注进身份字段后所有系统级规则起出的链 `RunSource` 相同 ⇒ 链里 `ApplyState` **永远判"同来源"**，"受击叠一层"不可达）。⇒ **身份**（`RunSource`：每次运行发新号、子链另发）与**因果边**（`CausedBy`：装"谁启动了我"）分开；你原本要的"把该行 `Source` 注进去"**完整保留**——只是注到边字段。**判据链（三处独立证据一致指向"边落在对象上"）**：① Remake 定案 `SPEC-01` §2 逐字"只做归属，**不承载因果链**——因果链由**对象图天然边**承载（…每层对象自带 `Source` 字段，查询从明确起点沿对象逐级走）"；② 原始版自己的调研结论 `master:Documents/后续优化内容/AbilityKit设计借鉴/05-溯源树Trace与explain.md:136` 逐字"**不要**把血缘树塞进 `FTcsSourceHandle` 结构体本身…血缘树是上层诊断数据，应分开"；③ **原始版实践反证**（本轮 `master` 分支考古）：`CausalityChain` 是**只写不读**的字段——全仓 `CausalityChain` 命中只有三类（工厂拷贝父链+追加 / `NetSerialize` / `DebugString`），**零玩法读者**（`GetRootSource` / `GetParentSource` 有伪码无实现、`operator==` 只比 `Id`），且**建边路径在插件内从未被执行**（`CreateChildSourceHandle` 零调用方，`ApplyBuff` 传空 `FPrimaryAssetId()`），`master` 的 `Source/` 下连伤害模块都没有 ⇒ "技能 → Buff → 持续伤害"那条链在原始版里没有任何一次真实运行过。**结论**：原始版缺的正是"可读的边 + 明确的查询起点"，而不是"句柄里多一个数组"。
3. **`ApplyState` 的"断链"不可表达**（计划 Step 1 原文："仅 `DefTag` 不可解析时 ⇒ `Error` + 断链"）：`ETcsStepResult` 只有 `Completed` / `Running` 两档，**步骤无法中断链**——断链只发生在"执行器未登记"时（由门面自己做，`TcsEffectSubsystem_Run.cpp:205`）。⇒ 全部失败面统一 `Warning` / `Error` + `TSR_Completed`；真正的软失败接管归 `OnError`（R5.5-a）。
4. **`AttributeCompare` 的比较枚举住 `TcsAttribute`**：比较是**属性值域**的词汇（与 `ETcsAttributeOp` 同域），条件 struct 才是 TcsEffect 的配置面 ⇒ `ETcsAttributeComparison` 落 `TcsAttribute` 新头、`FTcsTriggerCondition_AttributeCompare` 落 `TcsTriggerCondition.h`（与另两条内置条件同处，走同一注册表）。
5. **【用户已拍板 2026-10-05】属性门面查找抬到 `TcsAttribute` 单载具**：`UTcsAttributeSubsystem::Resolve(const UWorld*)` 是唯一查找点（将来补注入契约**只换这一处**）；两模块各留自己的白名单薄壳（白名单是各自的审计面，不合并——TcsState 侧窄、TcsEffect 侧另需 `EvaluateCurrent`）。
6. **`FTcsStateAttributeAccess` 头注释订正**：它声称允许面含 `EvaluateCurrent`，实际未暴露该方法（查证 `TcsStateAttributeAccess.h:40-99` 无此方法）。本轮就地订正注释，不留"注释说谎"。
7. **`FTcsStepApplyState` 不声明参数表**：`ParamTable` 传空 ⇒ 引用类参数源落兜底（链侧参数表载体不存在，`ITcsParamTableReader` 的链侧实现是零消费者）。如实登记边界。
8. **内联触发行不存句柄**：`FTcsStateInstance` 明文禁持订阅句柄（D3-7 v2）⇒ 退订口径 = **按实例锚点级联**（`UnregisterTriggerRowsBySource(CascadeAnchor)`），MUST NOT 引入"逐条句柄"登记面。
9. **因果边 MUST NOT 成为撤销或正确性依赖**（沿用原始版自己的否决理由：`master:Documents/_archive/调研：SourceHandle机制扩展优化.md:203-204` 逐字"父级 SourceHandle 的生命周期可能先于子级结束…已确认 State 之间不应根据 SourceHandle 互相影响生命周期"）：`CausedBy` 只作溯源读数；状态撤销走 `CascadeAnchor`，属性条目与触发行撤销走各自的锚点/来源值——**任何按 `CausedBy` 级联撤销的写法都不允许**。
10. **溯源树（lineage tree）归 R8，本轮登记台账**：本轮只落"链运行态的第一段边"；成树遍历 + Explain 面板 + 回放归 R8（参考原始版的 AbilityKit 借鉴调研 `05-溯源树Trace与explain.md`：阶段 A = 只维护父子链接、阶段 B = explain 窗口、阶段 C = Snapshot 回放；建议的新能力名 `add-source-lineage`）。台账现无此行（考古确认零命中），本轮新增一行。
11. **时间条目锚点不动**：`PushExpiry` / `PushPeriod` 的 `OwnerId` 今天取 `Instance.Source.Id`（`TcsStateOps_Lifetime.cpp:51/75`）——它只是回调侧的身份读数、不参与匹配（撤销走条目句柄），本轮**不动**（最小改动；改它没有消费者）。

## Impact

- **Affected specs**（插件仓 TCS `openspec/`）：
  - `effect-chain`（**MODIFIED** 1 条：「效果链上下文（黑板）」补 `RunSource` 与 `CausedBy` 两个字段）
  - `effect-interpreter`（**ADDED** 2 条：「链运行态来源锚点与因果边」、「属性修改步骤（ModifyAttribute）」）
  - `effect-trigger`（**MODIFIED** 2 条：「触发行实例与级联退订」（锚点语义）、「触发条件最小集」（补第三条内置条件））
  - `state-step-library`（**ADDED**，新能力：「状态施加步骤（ApplyState）」）
  - `attribute-types`（**ADDED** 1 条：「属性值比较枚举」）
  - `attribute-pipeline`（**ADDED** 1 条：「属性门面的解析点」）
  - `state-instance-lifecycle`（**MODIFIED** 2 条：「池化状态实例记录」（+`CascadeAnchor` 与 `Source` 语义）、「广播点与阶段迁移」（撤销顺序补"退订触发行"））
  - `state-modifier-materialization`（**MODIFIED** 2 条：「移除与到期时按来源级联摘除」、「修正器来源句柄的身份一致性」——均改为按实例锚点）
- **Affected code**（插件仓 TCS）：
  - 新：`TcsAttribute/Public/Attribute/TcsAttributeComparison.h`、`TcsEffect/Public|Private/Chain/TcsStepModifyAttribute.*`、`TcsEffect/Private/Attribute/TcsEffectAttributeAccess.*`、`TcsState/Public|Private/Chain/TcsStepApplyState.*`、`TcsState/Private/State/TcsStateOps_Trigger.cpp`
  - 改：`TcsAttribute/Public/TcsAttributeSubsystem.h`（+`Resolve`）、`TcsEffect/Public/Trigger/TcsTriggerCondition.h` + `Private/Trigger/TcsTriggerCondition.cpp`（+条件）、`TcsEffect/Public/Chain/TcsEffectContext.h`（+`RunSource` 与 `CausedBy`）、`TcsEffect/Private/TcsEffectSubsystem_Run.cpp`（起链发放 `RunSource`）、`TcsEffect/Private/Trigger/TcsTriggerEvaluator.cpp`（起链时写 `CausedBy` = 该行的 `Source`）、`TcsEffect/Private/TcsEffectSubsystem_StepProtocol.cpp`（子链写 `CausedBy` = 父链 `RunSource`）、`TcsState/Public/State/TcsStateInstance.h`（+`CascadeAnchor`）、`TcsState/Private/State/TcsStateOps.cpp`（发放锚点 / 按锚点摘除 / 两处接线）、`TcsState/Private/State/TcsStateOps_Modifier.cpp`、`TcsStateModifierMaterializer.cpp`、`TcsState/Private/State/TcsStateAttributeAccess.h`（注释订正 + 共享查找点）
  - 规格同步（设计文档）：`SPEC-03-effects` §2.1 原语状态 8 → **10**、§2 "纯机制层"边界句改述为"不依赖**上层**领域模块"；`SPEC-02-states` §1 的 `FStepApplyState` 落地状态
- **Affected code**（宿主仓 LAC `Source/TcsDev/`）：`TcsDevSliceRig.cpp` 新增检查 **23a–23h**（正例面，零故意红字；拒绝面留 Task 7）；`Config/DefaultGameplayTags.ini` 补探针词
- **资产迁移**：无（新字段均有默认值；既有 `DA_Check_BuffDef` 不配 `Triggers` 时行为不变）
- **台账**：`TRIG-4`（行为半）结清、`DAMAGE-2` 的 `ModifyAttribute` 一支结清、`TRIG-5` 的 `AttributeCompare` 一支结清；新增实施期边界（链侧参数表缺位、`CHAIN-1` 补记"触发行来源在链内不可读"）

## 非目标

- `OnError` / `Parallel` / `Repeat`（R5.5-a）——链步骤的**中断与软失败接管**不在本轮，本轮步骤失败一律"不断链"。
- **行为 Fragment**：另开提案 `add-state-behavior-fragments`（本提案只做触发行接线）。
- `FTcsStepApplyState` 的操作复制 / 网络复制（R7）。
- 链侧参数表载体（`ITcsParamTableReader` 的链侧实现，零消费者）。
- 属性访问**写侧**注入契约（`ITcsAttributeAccess`，触发条件型 R5.5-g）。
- **溯源树 / 血缘遍历 / Explain 面板 / 按血缘回放**（裁定 10）：本轮只落第一段边，成树与可视化归 R8（新台账行）。
- 用量侧来源归属的其它缺口维持既有台账归属：`STAT-8`（逐来源退层）、`TRIG-3`（定义库来源句柄不外露）、`R-5`（`FTcsSourceHandle` 反射化，`reflection-backlog` 待调研）——本轮**不动**。
