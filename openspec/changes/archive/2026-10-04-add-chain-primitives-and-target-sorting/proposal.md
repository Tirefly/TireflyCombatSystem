# Change: 追加四个链原语 + 目标排序相位（P-A）

## Why

R4 的前两个 Task 打通了"**事件 → 触发行 → 效果链**"的**订阅侧**，Task 3 又补齐了**提交侧**（`ModifyFlow`）。但链里**只有三个原语**（`WaitDelay` / `SelectTargets` / `Damage`）——**能触发，却做不了什么**：没有变量、没有分支、没有子链、没有事件等待，链的"句子"能力停在最小样例上。本变更补的就是这一批原语（`SetVar` / `Branch` / `RunSubChain` / `WaitEvent`）。

同一批并入 **P-A 排序相位**（用户 2026-09-30 拍板并批，理由 = **改动点复用**：本批本就要写提案、本就要改 `TcsChainRun` 结构、本就必须补跑 Shipping）。P-A 的动机来自 AbilityKit 吸收调研：TCS 的目标管线是"**单相**"的——`ITcsEntityQuery` 取数 → `FTcsTargetFilterStrategy` 资格 → `FTcsTargetSelectorStrategy` 选择——**只能表达"谁合法"，不能表达"谁更优"**。后果是顺序逻辑散落在各个选择器实现里：不可复用、无确定性约束、不可组合（"先按威胁、同威胁按距离"声明不出来）。补上排序相位后，目标管线变成"**选择 → 过滤 → 排序 → 取前 K**"，且并列时有唯一的确定性裁定者。

三处**计划没预料到**的发现直接改变了本提案的形状，逐条记在下方「与计划的偏离」。

## What Changes

### 一、四个链原语（`effect-interpreter`）

| 原语 | 形状 | 挂起 |
|---|---|---|
| `FTcsStepSetVar` | `{ VarKey: FGameplayTag; Value: FTcsParamValue }` | 即时 |
| `FTcsStepRunSubChain` | `{ ChainId: FGameplayTag; bWait = true }` | `bWait` 且子链挂起时 |
| `FTcsStepBranch` | `{ Conditions: TArray<FInstancedStruct>; ThenChainId; ElseChainId; bWait = true }` | 同上 |
| `FTcsStepWaitEvent` | `{ EventTag: FGameplayTag; TimeoutSeconds = 0.0 }` | 首入即挂起 |

- **`SetVar` 是 `FTcsEffectContext::Variables` 的首个写入方**——该字段此前"R3 无写入方，留位"的注记随之作废。`Value` **MUST** 用 `FTcsParamValue`（退化成裸 `double` 会让"变量只能写字面量"成为隐式限制）。`VarKey` **MUST** 与门面 `SetRunVariable` / `TryGetRunVariable` 共用 `EffectChainRunVar` 这一个键空间（分根会导致"步骤写的变量脚本读不到"）。
- **`WaitEvent` 落地第三种挂起形态的第二、三个唤醒源**：①到期堆（`WaitDelay` 已实证，`WaitEvent` 的超时复用）②**事件匹配** ③**子链完成**。三个源各在运行态上留可配平的锚，且**运行态释放即解锚**。
- **`Branch` 的条件复用触发行那张条件注册表**（一套条件类型两处可用，零重复实现）；链侧求值上下文只有 `Caster` 有来源，`EventTag` / `ClassificationTags` **本轮无来源** ⇒ 依赖它们的条件在链侧恒不通过。**这是明示接受的限制**，登记进台账。

### 二、熔断护栏（`effect-interpreter`，本批必须补的两条）

- **预算按"最外层进入"计**：`MaxStepsPerFrame` 从此是一次最外层进入的**总**预算，**嵌套起链共用父链预算**（子链步数计入父链）。原实现里它是 `RunFrom` 的**局部**变量 ⇒ 每进一条链各拿 64 步 ⇒ `A→B→A→B` 自激**既绕过预算、又直接吃调用栈**（这不是性能问题，是崩溃）。
- **嵌套深度 MUST 另有固定常量上限**：预算由链定义给出、**作者可配到很大**，故它 **MUST NOT** 作为调用栈深度的唯一护栏。该上限是**调用栈护栏而非行为语义**，MUST NOT 做成链定义字段。

### 三、P-A 排序相位（`targeting-strategy`）

- **新增 `FTcsTargetScorerStrategy`**（虚分派 USTRUCT 基类，中性默认实现 + `meta=(Hidden)`，**禁纯虚**）+ **排序项** `FTcsTargetSortItem{ Scorer; Direction }`：`SortItems` 是**有序比较项数组**，按**严格字典序**逐项裁定，每项方向独立；
- **稳定键决胜**：全部排序项相等时按 **`FTcsCombatEntityHandle::Id` 升序**（详见「与计划的偏离」第 2 条的事实更正）；
- **浮点边界**：任一排序项返回 **NaN ⇒ 该候选被排除**（MUST NOT 排末尾）；**±Inf 保留**参与排序；**"无法评分"MUST 用 NaN 表达，MUST NOT 用 `-Inf`**（`-Inf` 在升序下会变成"排最前"——外部参照实现里实测存在的陷阱，规格明文禁止照抄）；
- **资格 MUST NOT 伪装成低分**：`Scorer` 只表达"谁更优"；"死亡 / 满血 / 不可选中"归 Filter。这条写进类型注释与规格两处；
- **唯一内置评分器 = 距离**（`FTcsScorerDistance`，只依赖 `ITcsEntityQuery::GetLocation`，**零宿主语义**）。判据 = 用户 2026-09-30 拍板的原则：*"TCS 属于可复用插件，理论上不应该直接干涉宿主项目生命值属性的配置方法"* ⇒ 泛化为"**凡是需要宿主词汇的内置策略，插件一律不提供**"；
- **评分器宿主插槽**（`ITcsTargetScorerHost` + `FTcsScorerHostDelegate` 转发器）——**第三个插槽家族**，补齐选择器/过滤器两族之外的缺口；没有它，"血量最低"这类条件无处安放，宿主只能去写 C++ 策略类型。规格 SHOULD 注明"**排序评分优先 C++ 策略、脚本插槽用于低频/非关键排序**"（调用频次 = 逐候选 × 逐排序项，高于过滤器，跨语言反射在热路径上不便宜）；
- **候选去重**（本轮补掉的**无人防守的真实缺口**）：当前完全没有去重概念，宿主来源重叠时**下游伤害步骤会对同一目标打两次**。去重键 = 实体句柄身份，发生在**过滤之后、取前 K 之前**；**执行器**负责，`Resolve` 契约一字未改；
- **`MaxCount`（K）语义**：按**完整字典序**取前 K，`0` = 不限。**MUST NOT** 理解成"按某一个排序项截断"。

### 四、文档与注释回写（本轮必须同批，否则库内自相矛盾）

- **`SPEC-06-targeting` §5「非目标」删去"不做目标评分/权重排序（未见需求）"**——该句在本变更落地后即为假陈述（`decisions-log` 2026-09-30 ⑦ 已预告"两项既有非目标需改口"，本提案处理其中与排序相关的一项）；
- **`SPEC-06-targeting` §3 补排序契约**（严格字典序 + 稳定键决胜 + NaN 排除 + 确定性的同进程口径）；`§2.2` 的过滤器条目并补"资格不得伪装成低分"这条分工纪律；
- `FTcsEffectContext::Variables` 的"R3 无写入方，留位"注记改为"首个写入方 = `SetVar`"；
- `TcsChainRun.h` 的运行态纪律块补"嵌套起链共用预算"与"深度上限"两条。

## Impact

- **Affected specs**：`effect-interpreter`（MODIFIED × 2 / ADDED × 3）、`targeting-strategy`（MODIFIED × 1 / ADDED × 3）。
- **Affected code**：
  - `TcsEffect`：新建 `Chain/TcsStepSetVar`、`Chain/TcsStepBranch`、`Chain/TcsStepRunSubChain`、`Chain/TcsStepWaitEvent`（各 `.h` + `.cpp`）；改 `Chain/TcsChainRun.h`（挂起锚 + 等待方字段）、`Chain/TcsEffectContext.h`（注记）、`TcsEffectSubsystem.h` / `.cpp`（等待表 + 共享 Handler + 预算/深度计数 + `ReleaseRun` 解锚与唤醒等待方），新 `.cpp` 按 300 行纪律拆（照 `TcsEffectSubsystem_Trigger.cpp` 的先例）；
  - `TcsTargeting`：新建 `Targeting/TcsTargetScorerStrategy.h`、`Targeting/TcsScorerDistance.h/.cpp`、`Targeting/TcsScorerHostDelegate.h/.cpp`、`Host/TcsTargetScorerHost.h`；改 `Chain/TcsStepSelectTargets.h/.cpp`（排序 / 去重 / 取 K）；
  - **`TcsEffect` 零新增模块依赖**：四个原语全住 `TcsEffect`，`ITcsEntityQuery` / 事件总线都是既有依赖；`TcsTargeting` 仍只依赖 `TcsCore` + `TcsEffect`。
- **BREAKING**：无（四个原语为新类型；`FTcsStepSelectTargets` 只**加**字段，既有链资产不配 `SortItems` 时**顺序**不变——排序项为空即保持既有"保序"）。**但有一处无条件的行为变化必须点名 = 候选去重**：它不随字段门控（去重是执行器的固定相位），故既有链若其选择器产出**重复候选**，结果集会变少（同一目标只打一次）——这是本轮**刻意补的缺口**（原状为"宿主来源重叠时下游对同一目标打两次"），且"重复候选被保留"从未写进任何规格（§5 非目标与选择器契约都只说"只填充不清空"），故不算破坏契约，但**不是零影响**。
- **不入库的宿主侧**：本轮不改 `Source/TcsDev/` 与 `Content/`（Task 4 才动）。

## 与计划的偏离（逐条留痕，供计划回写）

| # | 计划原文 | 实际 / 裁定 | 依据 |
|---|---|---|---|
| 1 | Task 3.5 Step 1：`effect-chain` MODIFIED：追加四个原语需求 | **`effect-chain` 一字不改**，四个原语全部落 `effect-interpreter` | `effect-chain` 的「效果链上下文（黑板）」早已写明 `Variables` 是"SetVar/Branch 类步骤的载体"；`WaitDelay` 这个同族步骤也住 `effect-interpreter`（该能力已持"等待步骤"条目）。往 `effect-chain` 塞步骤需求会与既有分工打架 |
| 2 | 调研稿 §2.1 / 计划第 502 行：稳定键用 `FTcsCombatEntityHandle` 的**（Index, Generation）**升序 | **事实更正**：该句柄**只有 `int64 Id`**，头注释明文"本句柄**无代际段**——Id 单调递增且永不复用"；（Index, Generation）是 `TTcsInstanceHandle<T>` 的字段。**改判为 `Id` 升序**（更强：永不复用 ⇒ 无回收槽误命中） | `TcsCombatEntityHandle.h` 头注释与字段定义（源码核实） |
| 3 | 计划第 490 行：建议 `FTcsChainRun` 加 `FTcsEventSubscriptionHandle PendingSubscription`（每运行态一条订阅） | **改为共享订阅**：同一 `EventTag` 的多个等待者**共用一条总线订阅**（计数配对），运行态只持"在等哪个 tag" | ①既有明文纪律（`FTcsTriggerRegistry`："**MUST NOT** 每行各订一次：那会让总线订阅表随行数膨胀且退订易漏"）；②**总线派发不把订阅句柄交给 Handler**（只传 `(EventTag, Payload)`）⇒ 每等待一条订阅既无收益、回调又无法自辨身份 |
| 4 | 计划第 492 行：熔断自检"`MaxStepsPerFrame` 是否够" | **不是"够不够"**：预算原为 `RunFrom` 局部变量 ⇒ 嵌套起链各拿新预算。**补两条**：共享预算 + 固定深度上限 | `RunFrom` 实现（`MaxStepsPerFrame` 是局部变量）；`FTcsStepExecute` 以 `FTcsChainRun&` 引用传池元素 |
| 5 | 计划未登记 | **`SPEC-06-targeting` §5 的"不做目标评分/权重排序（未见需求）"必须同批改口** | 该句与本提案直接冲突；`decisions-log` 2026-09-30 ⑦ 预告"两项既有非目标需改口，提案时处理" |
| 6 | 计划第 504 行 MUST NOT 只点名"Top-K 的**流式**实现" | **K 语义进本批**（字段 + 按完整字典序取前 K），只做**非流式** | 用户 2026-10-03 裁定：不做 K 则排序相位**没有可用消费者**（`Context.Targets` 排完序仍含全部候选，下游会打全体），且用户 2026-09-30 原话"先只内置一个**距离最近**"本身隐含 K=1 |
| 7 | R4.5-b / R4.5-c 与 P-A 评分器"可并批"（Q-7，未裁定） | **不并批**：本批只落 P-A 评分器插槽，R-1 / R-2 各留独立提案 | 用户 2026-10-03 裁定。理由：并批的主要收益（一套验证装置）**已由既有 `TcsDevGcFixtures` 四槽位夹具兑现**；R-1/R-2 结论冻结于 09-24/09-27 而 P-A 契约今日才定形，捆一批会让任一处改动都得重开整批 |
