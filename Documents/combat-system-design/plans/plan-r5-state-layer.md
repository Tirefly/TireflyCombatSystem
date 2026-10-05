# R5 实施计划：M3 状态层（`TcsState` 全生命周期竖切）

- **文档 ID**：`PLN-R5`
- **类型**：PLN / 计划
- **状态**：ACTIVE
- **权威范围**：R5 当前实施计划；含 **R5–R8 轮次路线图（现行排期真相源）** 与《R5.5 批次表（非正式轮号）》。R4 及以前的实施记录不住这里（见 `PLN-R4` / `PLN-R3-1` / `PLN-R3-2`）
- **最后更新**：2026-10-05（Task 6 两半（6a / 6b）落地）

> **换根注记（2026-10-01）**：本文 tag 名一律用**换根后**的新名——`TcsEvent.*` / `DamageFlowKey.*` / `DamageFlowTemplate.*` / `Attribute.*` / `EffectChain.*`。旧名只存于 `log/`、`ledger/`、`evidence/` 等历史文件（本仓历史留痕不改写）。

> **路线图移交**：自本轮起《轮次路线图》的**真相源 = 本文档**（原住 `PLN-R4`，该文档已改为只承载《R4.5 批次表》）。

**Goal:** 让"施加态"获得完整生命周期——链里能**施加**一个状态（`FTcsStepApplyState`），状态带着**施加瞬间冻结的参数快照**与**实时生效的持续修正器**（真实改属性账本），行为由**触发行订阅状态生命周期事件**挂接（buff 自带链），到期或驱散后按来源**干净回收**。R4 打通的"事件 → 触发行 → 效果链"闭环，本轮获得它的**第一个非伤害领域消费者**。

**Architecture:** 新模块 `TcsState` 落在依赖链第四层——`TcsCore ← {TcsNotation, TcsAttribute} ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。注册表宿主 = `UTcsStateSubsystem : UWorldSubsystem`（per-unit 状态桶；与既有的 `UTcsAttributeSubsystem` 同构）。实例 = 池化 `FStateInstance`（**纯数据**：无策略、无载荷、无订阅句柄）。行为三路外置：① 链——经触发行订阅生命周期事件；② 行为 Fragment——兴趣 Tag + 单一泛化回调；③ 修正器——经 M2 账本按状态句柄级联。

**Tech Stack:** UE 5.8 C++、GameplayTags（原生 tag + 模块导出宏）、`FInstancedStruct` 策略载体、UBT（Development Editor + Shipping 双配置）、宿主侧装置（LAC `Source/TcsDev/`）。

---

## 0. 开工前置（先读：本轮输入、收窄结论、已拍板口径）

### 0.1 本轮输入（台账折入，逐条给落点）

| 台账条目 | 原归属 | 本计划落点 | 处置 |
|---|---|---|---|
| `STAT-2`（`FTcsParamEnumerableSource`） | R5 | **Task 0** Step 2（基类住 TcsCore） | 消费 |
| `DAMAGE-5`（求值上下文补 `Subject` + `EffectiveLevel`） | R5 | **Task 0** Step 1 | 消费 |
| `WAIT-6`（来源发号器应进程唯一） | 原 M6，2026-10-04 归属提前 | **Task 0** Step 3（**本轮升为硬前置**） | 消费 |
| `TRIG-4`（`SkillDef`/`BuffDef` 内联触发行） | R5/R6 | **Task 1**（字段）+ **Task 6**（施加时登记/级联退订） | 消费 |
| `TRIG-5` 之 `AttributeCompare` 条件 | R5 可随 | **Task 6** Step 4 | 部分消费 |
| `DAMAGE-2` 余 4 原语之 `ModifyAttribute` | R5 | **Task 6** Step 2 | 部分消费 |
| `DAMAGE-2` 余 3 原语（`Parallel` / `Repeat` / `OnError`） | R5 | **R5.5**（见 §轮次路线图） | 改判归属 |
| `DAMAGE-3` 余 `Heal` | R5 | **R5.5** | 改判归属 |
| `DAMAGE-4`（消费动作 + 候选裁决） | R5 | **R5.5** | 改判归属 |
| `STAT-3`（`FFlowRedirect` 模板重定向栈） | R5 | **R5.5**（首版竖切无消费者） | 改判归属 |
| `DAMAGE-1` 之"载荷 → `Context.Targets`" | R4 已部分消费 | 不在本轮（等首个带目标字段的载荷类型） | 不动 |

### 0.2 收窄轮的口径裁决（2026-10-04，用户逐项拍板）

| # | 议题 | 裁定 | 影响面 |
|---|---|---|---|
| Q-1 | M3 中央注册表的宿主类（`SPEC-02-states` 写 `UCombatStateRegistry`、`SPEC-05-integration` 写 `UCombatWorldRegistrySubsystem`，两处不一致且都不符代码族 `UTcs*Subsystem`） | 建 **`UTcsStateSubsystem : UWorldSubsystem`**（per-unit 状态桶）；`SPEC-05-integration` 的 `UCombatWorldRegistrySubsystem`（实体状态机 + 泵接线 + DefLibrary 门禁）**留 R7/M6** | `SPEC-02-states` §3.1、`SPEC-01-attributes` §2 一致性待办行、`SPEC-05-integration` §2.1 三处同步 |
| Q-2 | `ModifyAttribute` 的"属性访问注入位" | **直接依赖 `UTcsAttributeSubsystem`**（`TcsEffect.Build.cs` 早有该编译边；注入位是给**宿主词汇**用的）。三条缓解：① 钉死用到的三个方法 + 集中一个解析点；② `D4-14` 的"纯机制层"改述为"**不依赖上层领域模块**"；③ 台账登记将来补注入契约的触发条件 | 路线图旧措辞作废；`SPEC-03-effects` 边界句精确化 |
| Q-3 | 来源发号器统一（`WAIT-6`） | **提到 R5 作硬前置**（Task 0 Step 3） | 插件内两个存量发号器 + 宿主装置直写高位 Id 的规避手法 |
| Q-4 | 数据 struct 与资产类同名撞车（UHT 去前缀判重：`FTcsBuffDef` vs `UTcsBuffDef` 必炸） | **甲案**：数据 = `FTcsStateDefBase` / `FTcsBuffDef`；资产 = `UTcsStateDef` / **`UTcsBuffDefAsset`**（适用 `openspec/project.md` 既定的 `Asset` 后缀补救条款） | `SPEC-02-states` §2 的资产名（`UTcsBuffDef` → `UTcsBuffDefAsset`） |
| Q-5 | 关系表检查器是否留 R5 | **建议一并推 R5.5**（证据见 §0.4 ②）；若坚持留 R5，须先加一步"字段语义拍板" | `SPEC-02-states` §3.4 的字段语义 |
| Q-6 | 五轴 Custom 策略与"行为 Fragment / 决策 Fragment"在 R5 的落点 | 只落**有消费者的两种**：五轴共存决策（Task 5）+ 行为 Fragment（Task 6）；`IValueDomainPolicy` / `ICostPolicy` 等**零消费者不预建** | 策略载体形态沿用 D3-7 v3（USTRUCT 反射基类 + `TInstancedStruct`） |

### 0.3 现状证据（2026-10-04 双路源码核查，逐条已核）

| 事实 | 证据 | 对计划的影响 |
|---|---|---|
| `Source/` 只有 **7 个模块**，无 `TcsState` | 模块目录清单；`TireflyCombatSystem.uplugin` 的 `Modules` 数组 | Task 1 是从零建模块 |
| `FTcsParamSnapshot` / `FTcsParamSnapshotEntry` / `FTcsNumericParamRow` / `ETcsParamMode` / `ITcsEntityLevelProvider` / `FTcsParamEnumerableSource` **代码里全不存在**（只活在规格与决策文档） | 全插件 grep 零命中（`Source/` 侧） | Task 0/3 是**造类型**不是接线；形状必须在计划里给草稿。**★ 2026-10-04 Task 0 更新：`FTcsParamEnumerableSource` 已存在**（`TcsCore/Public/Parameter/TcsParamEnumerableSource.h`），其余五项仍缺 |
| `FTcsValueConvention::ConvertToCanonical` **零调用点**（`TcsNotation` 里躺着） | `TcsValueConvention.h` 只有定义；全库无调用 | Task 3 会点亮它（正面信号：存量设计对了） |
| 全仓**从未构造过一个 `FTcsAttrModInstance`**，`Materialize*` 零命中 | 唯一写入点 `UTcsAttributeSubsystem::ApplyModifier` 由调用方自带实例 | Task 4 的物化器是修正器管线的**第一个真实消费者** |
| `FTcsSourceHandleRegistry` 计数器是**每实例**的（`NextId` 非 static，首个发 1），而级联摘除按值比较 `Source` | `TcsSourceHandle.h`；插件内两个发号器（定义库 `TriggerSourceRegistry` / 伤害门面 `FlowSourceRegistry`）**头号都是 1** | ✅ 坐实 `WAIT-6` = **真实缺陷**（状态实例来源会与定义库装配行互相误摘）→ Task 0 Step 3 |
| 触发行登记面已齐备：`RegisterTriggerRow(const FTcsEffectTriggerInstance&)` / `UnregisterTriggerRow` / `UnregisterTriggerRowsBySource`（**无 `UFUNCTION`**）/ `SetTriggerGateTag` / `GetTriggerRowCount` | `TcsEffectSubsystem.h` | Task 6 直接复用，零机制新增 |
| `FTcsEffectTriggerDef` 实存 **9 字段**，今天**零内联用法** | `TcsEffectTrigger.h`；`TArray<FTcsEffectTriggerDef>` 在 `Source/` 零命中 | Task 1 加内联字段即首个用法 |
| 规格已钉死本轮语义："施加状态时登记 ⇒ `Source` = 该状态实例句柄"；"状态移除 ⇒ 级联退订后不再触发" | `openspec/specs/effect-trigger/spec.md` | Task 6 **有规格背书**，不需新决策 |
| 步骤执行器签名 `TFunction<ETcsStepResult(const FInstancedStruct&, FTcsEffectContext&, FTcsChainRun&)>`——**形参里没有 World/子系统**，一律经 `Run.Owner`（`TWeakObjectPtr<UTcsEffectSubsystem>`）反查 | `TcsEffectStepExecutor.h`；先例 `TcsStepWaitDelay.cpp`（时钟）、`TcsStepSelectTargets.cpp`（实体查询） | Task 6 的 `ApplyState` / `ModifyAttribute` 照此形态 |
| 定义库发现走 `IAssetRegistry::GetAssetsByClass`（**不依赖** `PrimaryAssetTypesToScan`）；装配在 `SeedWorld`，重启世界幂等 | `TcsDefinitionSubsystem.cpp` | 本轮**不碰** `INTEG-3`（AssetManager 切换） |
| 链资产 `IsDataValid` **缺** `NotValidated → Valid` 提升段（触发定义资产有） | 同族两处已修，链资产是例外（= 台账 `TOOLS-2` ③） | Task 1 的 `UTcsBuffDefAsset` **一次写对**，不复制该缺陷 |
| `openspec/specs/plugin-descriptor/spec.md` 把模块清单钉成"**恰好七个**"，且明写 `TcsState` 属目标模块、当前不得出现 | 该规格；`openspec/project.md`（当前 7 / 目标 11） | Task 1 的提案 **MUST MODIFY 该规格**（7 → 8） |
| 宿主装置在**宿主仓**：`E:\Projects_Dev\LegendAutoChess\Source\TcsDev\`（`TcsDevSliceRig.cpp`） | 文件系统实测 | Task 7 的装置改动是**跨仓交付**（同 R4 Task 4） |

### 0.4 计划不得建在错前提上的两条更正

① **"`ModifyAttribute` 需要属性访问注入位"= 过期前提**（原写进 `PLN-R4` 路线图 R5 行）。实测：`TcsEffect.Build.cs` **早已依赖 `TcsAttribute`**（`Core/GameCore/Engine/GameplayTags/TcsAttribute/TcsCore`），只是代码里从未 include 过它。**注入位是给宿主词汇用的**（实体查询 / 阵营 / 等级），而属性账本是 TCS 自家实现 ⇒ 直接依赖，零新编译边。已按 Q-2 裁定，路线图措辞同步改掉。

② **关系表字段语义从未定稿**（Q-5 的证据）：`DEC-2026-08-31-state-relation` 的待办行至今写着"关系表字段细节（Priority 语义、互斥组结构、族规则表达式）"未勾；`SPEC-02-states` §3.4 只给了**形状** `{Status, Blocks, Requires, Priority, Cancels}` 与**执行时机**。故"检查器"不是"实现已定稿的设计"，而是"**先要拍板语义**"——这超出"该轮设计已按实施视角收窄"的前置条件（台账 §"与计划的分工"），故建议并入 R5.5。**本轮只落字段（形状），不落检查器**；字段零消费者这一点按纪律登记台账。

---

## Global Constraints

- **禁 TDD**（用户级最高纪律）：无失败测试步骤；每任务验证 = UBT 编译通过 + 定向人工检查。
- **提交纪律**：任何 `git commit` 仅在用户明确授权后执行；计划不含自动提交步骤，任务边界即"停点待检查"。**跨仓注意**：插件仓（TCS）与宿主仓（LAC）各自独立提交。
- **风格**：创建/修改 C++ 前执行 `unreal-cpp-style` 技能——Tab 缩进、UTF-8 无 BOM、LF；`.h` 用 region、`.cpp` 扁平；单 `.cpp` ≤300 行，超出按 `<Name>_<Feature>.cpp` 拆分；文件头 `// Copyright Tirefly. All Rights Reserved.`。
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名；枚举值 = 枚举名缩写全大写；API 导出宏 `TCS<模块>_API`（跨模块消费面 MUST 带——台账 `WAIT-9`）；动词纪律（`Resolve` 仅用于句柄/Id→对象）。
- **UHT 两条硬门槛（2026-10-04 双实证）**：① **全项目头文件名唯一**（同名不同目录照样 UHT 失败）；② **反射类型去 `U/A/F/E/I/T` 前缀后不得同名**（`FTcsBuffDef` vs `UTcsBuffDef` 必炸 ⇒ Q-4 甲案）。
- **依赖铁律**：`TcsCore ← {TcsNotation, TcsAttribute} ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。**TcsEffect MUST NOT include 上层领域模块**（Attribute 在它下层，允许——`ModifyAttribute` 与 `AttributeCompare` 即住 TcsEffect）。
- **日志**：归 UE 原生分类制（D0-6 v2）——使用日志 include `<模块>LogChannel.h`；**常规验收命令 MUST 零红字**（故意的失败输出独立成 `.Reject` 命令）。
- **编译**：每任务以 UBT **Development Editor** 编译通过为完成门槛；**改动含 `WITH_EDITOR` 面或新 `UPROPERTY` 时须补跑 Shipping**。
- **规格先行**：每个 Task 的实施以 OpenSpec 提案开道（`openspec validate <id> --strict --no-interactive` 通过 → 实施 → `openspec archive <id> --yes`）；归档时 MODIFIED 需求的标题 MUST 与主规格逐字一致；改名需求用 `## RENAMED Requirements`。
- **过网结构纪律**：MUST 纯反射数据——禁 `TFunction`、禁 `TMap`/`TSet` 作 `UPROPERTY`；`FInstancedStruct` 内层须可复制。
- **反射取内层**：一律单参数 `GetPtr<T>()` / `GetMutablePtr<T>()`（双参数重载在 Shipping 下**不校验**，是类型混淆陷阱）。
- **USTRUCT 抽象手法**：中性默认实现 + `meta = (Hidden)`；**禁 `=0` 与 `PURE_VIRTUAL`**（`PURE_VIRTUAL` 在 Development/Shipping 行为不同）。
- **`IsDataValid` 收尾**：无错无警时 MUST 把基类返回的 `NotValidated` 提升为 `Valid`（否则编辑器显示"未验证"、装置断言失效）。
- **unity 合并撞名**：同模块跨 `.cpp` 的 file-local 符号 MUST 带文件/装置前缀（UBT unity 合并会跨文件撞名，报错位置误导性极强）。
- **GC 纪律**：登记表若持**非 `UPROPERTY` 容器**且元素含对象引用，MUST 覆写 `AddReferencedObjects` 补引用；注入的 `TScriptInterface` MUST 由 `UPROPERTY` 持有。
- **零消费者不预建**：字段可先就位（设计承诺），但**不得为无消费者的字段造机制**；未落地面 MUST 写进计划注记 + `LEDGER-deferred`。
- **属性访问最小面（Q-2 缓解①）**：本轮允许 `TcsEffect` 触碰的属性 API **只有三个**——`UTcsAttributeSubsystem::ApplyModifier` / `RemoveBySource` / `EvaluateCurrent`；且 MUST 集中在一个解析点（`FTcsAttributeAccess::Resolve(World)` 一类），将来补注入契约时只换这一处。
- 设计规格来源：`Documents/combat-system-design/`（`[`SPEC-02-states`](../spec/03-module-states.md)`、`[`SPEC-01-attributes`](../spec/02-module-attributes.md)`、`[`SPEC-03-effects`](../spec/04-module-effects.md)`、`[`SPEC-08-damage`](../spec/09-module-damage.md)`、`LOG-01-states`、`DEC-01-pv`、`DEC-02-fold-display`）；冲突时以设计文档 + 用户拍板为准。

---

## 轮次路线图（R5–R8）

> 自本轮起本表为**现行排期真相源**（原住 `PLN-R4`）。`PLN-R4` 保留《R4.5 批次表》直至 b/c 收口。

| 轮次 | 主题 | 主要交付 | 前置 | 台账消费 |
|---|---|---|---|---|
| **R5（本计划）** | **M3 状态层（`TcsState`）** | 前置契约（求值上下文 / 可枚举源 / **发号器统一**）/ 模块与 Def 资产 / 实例·注册表·门面·生命周期事件 / 参数快照 + 等级源 + Duration-Period-到期堆 / **修正器物化（D3-19）** / 五轴堆叠 + Custom 决策 Fragment / **`ApplyState` + `ModifyAttribute`** + 内联触发行 + 行为 Fragment / 端到端验收 / 收束 | R4 已收束 | `STAT-2`、`DAMAGE-5`、`WAIT-6`、`TRIG-4`、`TRIG-5`（部分）、`DAMAGE-2`（`ModifyAttribute` 一支） |
| **R4.5**（**非正式轮号**，2026-10-04 重排后**仅剩两处**） | **脚本通道收口**（不占正式轮） | ①注册表跨世界寿命缺陷修复（**✅ 已闭环 2026-09-29**）；②**`R-1` 参数源族宿主插槽 → 并入本计划 Task 3**（同族同验证面）；③**`R-2` 两张注册表宿主脚本插槽 → R6 开工前**（消费者 = 宿主专属条件 / SkillCost） | — | 不消费台账条目；权威登记 = `LEDGER-reflection` 的 `R-1` / `R-2` 行 |
| **R5.5**（**非正式轮号**） | **原语与消费余额**（寄在 R5 与 R6 之间，不占正式轮） | 见下方批次表 | R5 核心落地 | `DAMAGE-2` 余 3 / `DAMAGE-3` 余 / `DAMAGE-4` / `STAT-3` + 本轮新增的关系表族 |
| **R6** | **M5 技能层（`TcsSkill`）** | `UTcsSkillDef`（继承 `FStateDefBase`）/ 账本 `FLearnedSkillEntry` / 六道门禁 / 时段驱动 `FPhaseSpan` + 打断 / 冷却多轨道 + 三事件 / Cost 策略 / 参数链（折叠器复用）/ 链重定向栈 / `FEntrySelector` | R5（`FSkillDef` 继承 + 快照类型复用） | `STAT-1`、`CORE-1`（若全域订阅需求成立） |
| **R7** | **M6 集成层** | 两级单位（Mass 小兵 + 全功能军官）/ `UCombatWorldRegistrySubsystem`（实体状态机 + 泵接线 + DefLibrary 门禁）/ StateTree 接线 / AttributeSet 全套 / `PrimaryAssetTypesToScan` + 发现机制切换 AssetManager + 加载层三策略 | R6 | `INTEG-1`、`INTEG-2`、`INTEG-3`、`WAIT-2`、`WAIT-5`、`WAIT-8`（模板资产化） |
| **R8** | **M7 表现 + M8 编辑器与工具** | `TcsCue`（Cue 契约 + `PlayCue` + 默认适配）/ 校验矩阵（四联 + 定义校验）/ Def 双轨同步器 / Explain 调试面板 / K2 强引脚 | R7 | `PRES-1`、`TOOLS-1`~`TOOLS-6`、`WAIT-1`、`WAIT-4`、`WAIT-9` 全量审计、`WAIT-10` |

**说明**：

- **R5 切分的理由 = 验收信号纯净**：`Heal`（需治疗模板 + `IHealFlowDelegate`）、`Parallel`/`Repeat`/`OnError`（TcsEffect 侧，与 M3 零耦合）、`DAMAGE-4`（TcsDamage 侧）、`STAT-3`（消费者是"状态/装备声明换流程"，首版竖切用不上）四项与 M3 无依赖关系；拖着它们会把"一条 buff 从施加到驱散全通"这个信号稀释成两件事。
- **关系表族（检查器 + 级联重评 + 槽位竞争）整体归 R5.5**——理由见 §0.4 ②：**语义未定稿**，它不是"已收窄的设计"。
- **技能参数行分派（D3-19 的另一半）归 R6**：`UTcsSkillModDef` 与 `TcsSkill` 今天都不存在，本轮只落**引用位 + 属性行**。
- **弹性**：R6 可能拆两轮（账本/施法与时段/冷却）；`INTEG-3` 的加载层若 R7 超载可拆独立轮。

### R5.5「原语与消费余额」批次表（非正式轮号）

> **立表原因与 `R4.5` 同款**：非正式轮号若只住在会话里，会被上下文压缩整条丢掉（2026-09-30 实证）⇒ MUST 落纸。**权威登记仍在 `LEDGER-deferred`**，本表只登记批次与状态。

| 批次 | 内容 | 状态 | 依赖 / 触发 |
|---|---|---|---|
| **R5.5-a** | `DAMAGE-2` 余 3 原语：`Parallel`（`bJoin` + `JoinCount` 汇合）/ `Repeat`（单链单帧熔断游标）/ `OnError`（软失败接管） | ❌ 未开工 | 与 M3 零耦合；`OnError` 的 Explain 线索产出面另归 R8 `TOOLS-6` |
| **R5.5-b** | `DAMAGE-3` 余 `Heal` 执行器（需治疗流程模板 + `IHealFlowDelegate`，09 §2.4"同骨架精简版"） | ❌ 未开工 | 需先定治疗模板与 delegate 形状 |
| **R5.5-c** | `DAMAGE-4` 消费动作（扣 `MaxUses` / 起 `Cooldown` / 发 `TcsEvent.Damage.ModifierConsumed`）+ `Execute` 的候选裁决通道（`DamageFlowKey.ExecuteCandidates` + `SortKey` 选一） | ❌ 未开工 | 需"收集型候选"的真实生产者 |
| **R5.5-d** | `STAT-3` `FFlowRedirect` 模板重定向栈（四粒度让渡：参数 → 链 → 技能 → **流程**） | ❌ 未开工 | 需"状态/装备声明换流程"的真实内容 |
| **R5.5-e** | 关系表族：**字段语义拍板**（Priority 语义 / 互斥组结构 / 族规则表达式）→ 检查器 → 移除级联重评（脏标记批量合并）→ 槽位竞争表（主体 = 施法运行，R6 后） | ❌ 未开工 | 语义拍板是第一步（见 §0.4 ②） |
| **R5.5-f** | 技能参数行分派（`UTcsSkillModDef` 经注册制分派到 TcsSkill 执行器物化） | ❌ 未开工 | R6（`TcsSkill` 落地后） |
| **R5.5-g** | **属性访问写侧插槽**（`ITcsAttributeAccess` + 门面注入）——Q-2 的 B 案，本轮只登记 | ❌ 未开工 | 触发条件：出现"宿主替换属性存储"或"脚本直写属性"的真实需求 |
| **R5.5-h** | 属性写入面脚本可达（`ApplyModifier` / `RemoveBySource` 补 `UFUNCTION` + glue）——本轮登记为 `SCRIPT-9` | ❌ 未开工 | 触发条件：出现"脚本不经链直接施加减益"的真实需求 |

---

## Task 0 — 前置契约（零行为变更）

**目标**：把三处 R5 硬前置一次性补齐——参数求值上下文缺字段、参数源缺可枚举基类、来源句柄发号器不唯一。三处都**不改现有行为**（纯新增字段/类型/计数器语义），可用既有装置回归。

**交付物**（全部修改既有模块，无新模块）：

- `Source/TcsCore/Public/Parameter/TcsParamValueSource.h`（改）
- `Source/TcsCore/Public/Parameter/TcsParamEnumerableSource.h`（新）
- `Source/TcsCore/Public/Handle/TcsSourceHandle.h`（改）
- 宿主装置 `Source/TcsDev/.../TcsDevSliceRig.cpp`（**LAC 仓**，撤"直写高位 Id"规避）

**提案**：`extend-param-evaluate-context`（TcsCore）+ `unify-source-handle-issuer`（TcsCore）

**依赖**：无（本轮第一个 Task）

- [x] **Step 1: `FTcsParamEvaluateContext` 补两个字段（`DAMAGE-5`）**——**✅ 2026-10-04 落地**（两字段 + 头注释口径改毕；UHT 产物 `NewProp_Subject`（`FStructProperty` 绑 `FTcsCombatEntityHandle`）与 `NewProp_EffectiveLevel`（`FIntProperty`）已进反射表，偏移正确）——在 `TcsParamValueSource.h` 的既有唯一字段 `ParamTable` 之后追加：
  `UPROPERTY(BlueprintReadWrite) FTcsCombatEntityHandle Subject;`（被施加方实体——状态/技能实例所依附的单位）
  `UPROPERTY(BlueprintReadWrite) int32 EffectiveLevel = 0;`（生效等级；0 = 无等级语义；**取值归调用方**，源不自己去查实例）
  头注释第 15 行的"M2/M5 轮补齐"口径 MUST 改为"**随 TcsState 等级源同批补齐（2026-10-04，M2 已收束，原文过期）**"。
- [x] **Step 2: `FTcsParamEnumerableSource` 基类（`STAT-2`）**——**✅ 2026-10-04 落地**（新文件 `Source/TcsCore/Public/Parameter/TcsParamEnumerableSource.h`，UHT 已产出 `.generated.h` + `.gen.cpp`；PV-10 原案的 `Enumerate` **未建**——展示层零消费者，台账按"部分消费"记）——新文件，形状：
  `USTRUCT(meta = (Hidden)) struct TCSCORE_API FTcsParamEnumerableSource : public FTcsParamValueSource`
  成员：`virtual int32 GetIndexForLevel(int32 Level) const { return INDEX_NONE; }`。**提交前修正（2026-10-04）**：原写的 `GetIndexCount()` 已删——它**全库无设计出处**（PV-10 各处只写 `Enumerate` + `GetIndexForLevel`）、且 **Series 视图要的是整表数值**（`FTcsViewBuildContext::TryGetSeries`），"表项总数"渲染不出任何一档 ⇒ 无消费者；PV-10 的 `Enumerate` 与其派生形态一并留白到展示层（`FTcsParamView_Series`，R8）落地时。
  **纪律**：只放"表项总数 + 等级→下标"两个中性默认实现（**索引解析的唯一真相在源**，见 `[`SPEC-00-core`](../spec/01-module-m0-core.md)` ④）；MUST NOT 预建 `IValueDomainPolicy` 一类策略接口（零消费者）。
- [x] **Step 3: 来源发号器统一（`WAIT-6`，本轮硬前置）**——把 `FTcsSourceHandleRegistry` 的计数器改为**进程唯一**（`Allocate()` 语义不变：返回非 0、全局唯一）。**✅ 2026-10-04 落地，含一处必要偏离**：
  - **偏离内容**：计划原写"`NextId` 由实例成员改为 `static std::atomic<uint64>`"，照字面改**修不掉缺陷**——本类是跨模块头内联的（`Allocate()` 内联、实例分住 TcsIntegration / TcsDamage / TcsDev 三个不同镜像），MSVC 的 COMDAT 折叠**不跨 DLL 合并** ⇒ `static` 成员/内联变量在每个镜像各留一份计数器、全都从 1 开始（而 monolithic Shipping 又会合并成一份 ⇒ 症状随配置漂移、Editor 下照旧撞号）。
  - **实际落法**：故改为**出线**：`Allocate()` 的定义移入新文件 `Source/TcsCore/Private/Handle/TcsSourceHandle.cpp`（计数器 = 该文件匿名 namespace 变量），结构体按本仓导出宏纪律（"有 out-of-line 成员或反射符号才带宏"，先例 `TcsCoreStats`）改带 `TCSCORE_API`。**导出生效的硬证据** = 三个消费模块全部链接成功（未导出必 LNK2019）。
  连带：① 插件内两个发号器（`UTcsDefinitionSubsystem::TriggerSourceRegistry` / `UTcsDamageSubsystem::FlowSourceRegistry`）**零改动**（自动获得全局唯一性）✅；② 宿主装置 `TcsDevSliceRig.cpp` 撤掉"直写高位 Id"的规避常量，改走正常发号（**LAC 仓改动**，与插件同批交付）✅；③ 台账 `WAIT-6` 标"已消费"✅。
- [x] **Step 4: 编译与回归**——**两半均 ✅ 2026-10-04**。**编译半边**：`-projectfiles` 刷新 + UBT Development Editor + Game Shipping **双配置均 Succeeded**，全日志**零 warning 零 error**；
  - **回归半边**：用户于 PIE 内跑 `Tcs.Test.Slice.Run` ⇒ **通过 19 / 失败 0**、延迟判定 **7b PASS**、区间**零红字**（`Error:` / `Warning:` / `Ensure condition` / `Fatal error` / `[FAIL]` 全零命中）。**关键行为读点 = 检查 14「按来源级联摘除」摘掉恰 1 条**——装置已改走正常发号，仍不误摘定义库来源 ⇒ Step 3 的**行为级**实证（不只编译级）。证据 = `EVID-2026-10-04-param-context-and-source-issuer`（活动日志区段 L2413–L2698 的逐项锚点 + 区段哈希）。

**验收信号**：双配置 0 error / 0 warning；`Tcs.Test.Slice.Run` 输出与 R4 收束时逐字一致（除"按来源级联摘除"检查改为走正常发号）。

**非目标**：不引入 `ITcsAttributeAccess`（Q-2 裁定 A 案）；不碰 `PrimaryAssetTypesToScan`。

---

## Task 1 — `TcsState` 模块落地 + Def 形状与资产

**目标**：让"buff 定义"成为可发现、可校验、可解析的内容资产；模块进入 `uplugin` 与规格的模块表。

**交付物**：

- `TireflyCombatSystem.uplugin`（改——`Modules` 插一条，位置在 `TcsEffect` 之后、`TcsTargeting` 之前）
- `Source/TcsState/TcsState.Build.cs`（新）
- `Source/TcsState/TcsStateModule.h` / `.cpp`（新，模块壳**不注册任何东西**）
- `Source/TcsState/Public/TcsStateLogChannel.h` + `Private/TcsStateLogChannel.cpp`（新，`LogTcsState`）
- `Source/TcsState/Public/Def/TcsParamRow.h`（新：`FTcsNumericParamRow` / `ETcsParamMode`）
- `Source/TcsState/Public/Def/TcsStateDefBase.h`（新：`FTcsStateDefBase`）
- `Source/TcsState/Public/Def/TcsBuffDef.h`（新：`FTcsBuffDef`）
- `Source/TcsState/Public/Def/TcsBuffDefTableRow.h`（新：`FTcsBuffDefTableRow`）
- `Source/TcsState/Public/Def/TcsStateDef.h`（新：`UTcsStateDef`）+ `TcsBuffDefAsset.h`（新：`UTcsBuffDefAsset`）
- `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` + `Private/TcsDefinitionSubsystem_State.cpp`（新）（改：加 `DiscoverStateDefs()` / `ResolveStateDef(DefTag)`；`TcsIntegration.Build.cs` 加 `TcsState` 依赖）
- `openspec/specs/plugin-descriptor/spec.md`（**MUST MODIFY**：模块清单 7 → 8）+ `openspec/project.md`（7/11 → 8/11）

**提案**：`add-tcs-state-module`

**依赖**：Task 0（求值上下文类型在 TcsCore，Def 行要引用它）

- [x] **Step 1: 模块骨架**（2026-10-04 ✅）——照 `TcsDamage` 六件套（`.uplugin` 条目 / `Build.cs` / 壳 `.h`+`.cpp` / 日志通道两件 / `Public`+`Private` 目录）。`Build.cs` 的 `PublicDependencyModuleNames` = `Core` / `CoreUObject` / `Engine` / `GameplayTags` / `TcsCore` / `TcsNotation` / `TcsAttribute` / `TcsEffect`。壳类名 `FTcsStateModule : public IModuleInterface`（空体）。
- [x] **Step 2: 参数行与 Def 形状**（2026-10-04 ✅）——`ETcsParamMode { EPM_Snapshot = 0, EPM_Live = 1 }`（规格：Snapshot 默认 / Live 仅 Skill 侧用）；`FTcsNumericParamRow { FGameplayTag Key; FTcsParamValue Base; ETcsParamMode Mode = EPM_Snapshot; ETcsValueConventionFlag ValueConvention = VCF_None; }`。
  `FTcsStateDefBase`（抽象、编辑器隐藏）：`FGameplayTag StatusTag` / `int32 LevelBase = 1` / `int32 MaxLevel = 0` / `TArray<FTcsNumericParamRow> Params` / `TArray<FTcsDescriptionEntry> Descriptions` / `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows`（**R5 只用属性行**）。
  `FTcsBuffDef : FTcsStateDefBase`：时值三件（`EDurationPolicy DurationPolicy = EDP_Finite` / `FTcsParamValue DurationTime` / `double Period = 0.0` / `ETcsPeriodRefresh PeriodRefresh = EPR_Keep`）、堆叠（`FStateStackPolicy StackPolicy`）、
  行为（`TArray<FTcsEffectTriggerDef> Triggers`——**`TRIG-4` 的内联位**）、关系字段（`TArray<FGameplayTag> Blocks` / `Requires` / `int32 Priority = 0` / `TArray<FGameplayTag> Cancels`——**只落字段，检查器归 R5.5**）。
  **不落项（登记台账）**：`FStateDefBase` 的"生命周期事件词汇"字段**无消费者**（行为 Fragment 的兴趣 Tag 已是声明面）⇒ 本轮不预建；`Descriptions` 只落字段与 `IsDataValid` 引号校验，**渲染归 R8**。
- [x] **Step 3: 资产两个**（2026-10-04 ✅）——`UTcsStateDef : UPrimaryDataAsset`（`DefTag: FGameplayTag`；`static const FPrimaryAssetType PrimaryAssetType` 取值 = 类名 `"TcsStateDef"`；`GetPrimaryAssetId()` 名取 `DefTag.GetTagName()`）；`UTcsBuffDefAsset : UTcsStateDef`（`BuffDef: FTcsBuffDef`；`PrimaryAssetType` = `"TcsBuffDefAsset"`）。
  `IsDataValid` 校验（**末尾 MUST 把 `NotValidated` 提升为 `Valid`**）：`DefTag` 空 ⇒ Error；`Params` 键重复 ⇒ Error；`ModifierRows` 空引用 ⇒ Error；`Triggers` 内 `EventTag` / `EffectChainId` 空 ⇒ Error；`DurationPolicy == Finite` 且 `DurationTime` 未配 ⇒ Error；`Period < 0` 或 `PeriodRefresh != Keep` 而 `Period == 0` ⇒ Warning。
- [x] **Step 4: 表行（编辑期载体）**（2026-10-04 ✅）——`FTcsBuffDefTableRow : FTableRowBase`（`DefTag` + `BuffDef`）；**运行期零 DataTable 加载路径**（双轨制：表 = 编辑期、资产 = 运行期）。
- [x] **Step 5: 定义库发现与解析**（2026-10-04 ✅）——`DiscoverStateDefs()` 照 `DiscoverTriggerDefs()` 形状（`IAssetRegistry::GetAssetsByClass(UTcsBuffDefAsset::StaticClass()->GetClassPathName(), ...)` + `IsLoadingAssets()` 就绪门 + 四条校验进 `FailureList`）；`ResolveStateDef(FGameplayTag) -> const FTcsBuffDef*`。**不做世界装配**（状态 Def 由 `UTcsStateSubsystem` 按需解析，无"每世界登记一次"的语义）。
- [x] **Step 6: 规格同步**（2026-10-04 ✅）——`plugin-descriptor` 的"恰好七个模块"改 8 并补 `TcsState` 行；`openspec/project.md` 同步；`[`SPEC-02-states`](../spec/03-module-states.md)` 的资产名 `UTcsBuffDef` → `UTcsBuffDefAsset`（Q-4 甲案）与 §1 依赖行同步。
- [x] **Step 7: 编译 + 资产发现实测**——**两半均 ✅ 2026-10-04**。**编译半边**：Development Editor 与 Game Shipping 均 `Result: Succeeded`，两日志 `error` / `warning` 命中数全 0（含宿主装置改动后的复编）；产出 `UnrealEditor-TcsState.dll`。**资产半边**：编辑器内建 `/Game/TcsDev/Checks/DA_Check_BuffDef`（`DefTag` = `Buff.Def.StatusTag` = `StateDef.Check.Burn`）并存盘；PIE 跑 `Tcs.Test.Slice.Run` ⇒ **通过 20 / 失败 0**（原 19 项 + 新增**检查 18**）、延迟判定 **7b PASS**、区间零红字；**检查 18 四项判据全过**（按 `DefTag` 解析成功 / `StatusTag` 一致 / `IsDataValid == Valid 1/1` / 失败清单状态定义条目 0 条）。证据 = `EVID-2026-10-04-tcs-state-def-asset`（区段 L2909–L3168、区段哈希 `0dd2bb9a…`）。双配置编译；装置侧断言"发现 N 个 `UTcsBuffDefAsset`、`IsDataValid == Valid`、按 `DefTag` 可解析"（**注意**：`Valid` 判据依赖 Step 3 的提升段）。

> **Task 1 落地记录（2026-10-04）**——实际交付面比交付物列表多三项，逐条留痕：
> - **提案 delta 从一份变四份**：除 `plugin-descriptor`（MUST MODIFY）外，另补 ① `integration-entity` MODIFIED——Step 5 一落地，现行规格里"**两条**按类发现路径"这句事实即失效，delta 是合规不是扩张；② `state-def-asset` ADDED——新 Def 族的形状/身份/校验需要规格落点（对齐 `effect-trigger-asset` 先例）；③ `gameplay-tag-governance` MODIFIED——见下条。
> - **描述载体随之落**（用户 2026-10-04 裁定甲案）：`FTcsDescriptionEntry` / `FTcsDescriptionViewSlot` 两个纯数据载体——`FTcsStateDefBase.Descriptions` 的类型**全库不存在**（TcsNotation 只有值约定 + 日志通道），不补则编译不过，而 §12.4 已裁定"本轮只落字段与作者侧校验"；视图策略族仍归 R8。
> - **`StateDef` tag 根（用户 2026-10-04 裁定甲案）**：Step 7 要真资产就必须有身份词，而根表 9 个根里**没有任何一个承载"状态定义资产身份解析"**（`TcsStateParam` 是参数表读取角色，不是 Def 身份）⇒ 新增 `StateDef.<名>` 根（根表 9 → 10）。**`Def.StatusTag` 的根归属本轮不裁定**（零解析消费者 ⇒ 不预建根），验收资产复用同一个 `StateDef.*` 词，归属待 R5.5-e 关系表族拍板。
> - **两处类型提前落**（详见 `SPEC-02-states` §12.5）：`EDurationPolicy` / `ETcsPeriodRefresh`（住 `Public/State/TcsStateEnums.h`）与 `FStateStackPolicy` 形状（住 `Public/State/TcsStateStackPolicy.h`）随本 Task 落——字段要先有类型；Task 2 / Task 5 只在其上续写，计划里那两项交付物名随之让位。
> - **宿主侧连带（LAC 仓）**：`TcsDev.Build.cs` 显式加 `TcsState` 依赖——**传递依赖只给头文件路径、不给导入库**：TcsDev 靠 `TcsIntegration` 的 public 依赖能 include 到 `Def/TcsBuffDefAsset.h`，但链接期取不到 `Z_Construct_UClass_UTcsBuffDefAsset`（实测 `LNK2019` + `LNK1120`）⇒ **直接使用某模块类型者 MUST 自己声明依赖**（这条是本节新记录的引擎/UBT 事实）。
> - **描述载体归属订正**：`SPEC-02-states` §10 v2 增补 9 原写"类型住 TcsNotation"，与 `DEC-02-fold-display` v3 命名批"住哪"表冲突 ⇒ 改判 **TcsState**（§12.5 第 4 条留痕）。

**验收信号**：新模块编入且 `uplugin` 模块序正确；一个 buff 资产被定义库扫到、校验回执为 `Valid`、按 tag 解析成功。

**非目标**：不做槽位竞争/关系表组织（R5.5-e）；不做 `PrimaryAssetTypesToScan`（R7 `INTEG-3`）；不做 Def 双轨同步器（R8 `TOOLS-3`）。

---

## Task 2 — 实例 / 注册表 / 门面 / 生命周期事件

**目标**：状态能被创建、查到、移除，并把全生命周期广播到总线（R4 的触发行才有东西可订阅）。

**交付物**：

- `Source/TcsState/Public/State/TcsStateEnums.h`（新：`EDurationPolicy` / `ETcsPeriodRefresh` / `EStatePhase` / `EStateRemoveCause` / `EApplyResult`）
- `Source/TcsState/Public/State/TcsStateHandle.h`（新：`FTcsStateHandle`，`USTRUCT(BlueprintType)`，`int32 Index` + `int32 Generation`）
- `Source/TcsState/Public/State/TcsStateInstance.h`（新：`FStateInstance`）
- `Source/TcsState/Public/State/TcsStateRegistry.h` + `Private/State/TcsStateRegistry.cpp`（新：per-unit 桶 + 槽位代际）
- `Source/TcsState/Public/TcsStateSubsystem.h` + `Private/TcsStateSubsystem.cpp`（新：`UTcsStateSubsystem : UWorldSubsystem`）
- `Source/TcsState/Public/State/TcsStateOps.h` + `Private/State/TcsStateOps*.cpp`（新：`FStateOps` 引擎函数）
- `Source/TcsState/Public/State/TcsStateEvents.h` + `Private/State/TcsStateEvents.cpp`（新：原生 tag + 载荷 struct）

**提案**：`add-state-instance-lifecycle`

**依赖**：Task 1

> **提案面（2026-10-04，二份 delta）**：`state-instance-lifecycle`（**新能力** ADDED 七条：句柄 / 池化实例 /
> per-unit 桶与代际 / 世界门面 / 定义登记口 / 六枚事件与载荷 / 广播点与阶段机）+ `integration-entity`
> MODIFIED 一条（状态定义从"只进缓存"扩为"缓存 + 逐世界登记进状态门面"）。
>
> **三处实施期裁定（当日拍板，均已在代码与规格里留痕）**：
> ① **身份归资产、登记口显式收身份**——`FTcsBuffDef` 是**定义内容**，`DefTag` 是**资产身份**
> （`UTcsStateDef::DefTag`，`GetPrimaryAssetId()` 的取值来源）。故 `RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef&)`
> 收两个形参；**不往 `FTcsStateDefBase` 加 `DefTag`**（否则策划在同一资产里手填两遍同一个 tag = 双真相）。
> ② **实例自持 `Unit`**（宿主单位）——句柄里没有单位段，不自持就只能"遍历所有桶找句柄"（O(桶数)）；
> 且 R5 Task 4 的修正器物化要用它把修正器挂到该单位的属性账本上（同一份信息，不是重复真相）。
> ③ **注册表自建槽位、不复用 `TTcsInstancePool`**——那个池是"一个池 = 一个句柄空间"，状态要的是
> **每单位一个句柄空间**（桶）；套用只有"一桶一池"（语义绕）或"全局一池"（句柄与单位解耦）两条更差的路。
> 手法照 `FTcsTriggerRegistry` 的槽位 + 代际（语义刻意与池一致：分配优先复用空闲槽、释放使代际 +1、新槽从 1 起）。
>
> **归档器硬约束（第三次实证，已写进 delta 头部）**：`### Requirement:` 与 `#### Scenario:` 的**标题**在
> MODIFIED 块里**都不可改、不可删**——改名或删除会被 `validate` 拒为 `omits scenario(s)`，故 Task 1 那条
> "状态定义只进缓存、不被装配到世界"的场景标题**原样保留**（它描述"缓存那一步"、今日仍成立），
> 另**新增**一条场景陈述"被逐世界登记"，两步关系写进需求正文。

- [x] **Step 1: 句柄与枚举**——**✅ 2026-10-04 落地**。`TcsStateEnums.h` 补 `EStatePhase{Inactive/Active/Expiring}`（三态由来：`Expiring` 是"先广播后释放槽位"能成立的前提）、`EStateRemoveCause{Expired/Removed/Cancelled}`、`EApplyResult{Applied/Refreshed/Stacked/Rejected}`（后两档本轮不可达，各自注释写明归属）；新增 `Public/State/TcsStateHandle.h`：`USTRUCT(BlueprintType)` 展平 `int32 Index` + `int32 Generation`，`IsValid()` = `Index >= 0 && Generation > 0`（**判据是"代际非 0"而非"下标非 0"——下标 0 是合法槽位**），相等比较 + `GetTypeHash`（含 `Templates/TypeHash.h`，`HashCombine` 不在 `CoreMinimal` 传递闭包内）。
- [x] **Step 2: `FStateInstance`（池化，纯数据）**——**✅ 2026-10-04 落地，两处对计划文本的修正**：① **实现名 `FTcsStateInstance`**（计划写 `FStateInstance`；`TcsState` 前缀与同族 `FTcsStateDefBase` / `FTcsStateEventPayload` 成族）；② **新增字段 `Unit`（宿主单位）**——计划未列，但句柄里没有单位段，不自持就只能"遍历所有桶找句柄"（O(桶数)），且 Task 4 的修正器物化要用它把修正器挂到该单位账本上（同一份信息）。字段 = 身份（`DefTag`/`Handle`/`Unit`/`Source`/`Instigator`）+ 数值阶段（`Stacks`/`Level`/`Phase`）+ 时间（`DurationRemaining`/`PeriodRemaining`/`ExpiryEntry`）。**`ParamSnapshot` 字段本轮按注释留白**：快照类型 `FTcsParamSnapshot` 全库不存在（Task 3 落），写占位类型只会造一个假类型。**非反射结构体**（无 `USTRUCT` 宏——它只在 C++ 侧流转，进总线的载荷是 `FTcsStateEventPayload`）。
- [x] **Step 3: 注册表与桶**——**✅ 2026-10-04 落地，含一处对计划文本的"为什么"补全**。`Public/State/TcsStateRegistry.h` + `Private/State/TcsStateRegistry.cpp`：`FTcsStateRegistry` 持 `TMap<FTcsCombatEntityHandle, TUniquePtr<FStateBucket>>`（间接层使**桶地址不随 `TMap` 扩容失效**，遍历中为别的单位建桶安全）；`FStateBucket` = 槽位数组 + 代际数组 + 空闲槽栈，`MakeHandle` 是**唯一拼装点**。**为什么不用 TcsCore 的 `TTcsInstancePool`**（计划未写理由）：那个池是"一池一句柄空间"，而状态要的是**每单位一个句柄空间**；套用只有"一桶一池"（同一件事两套身份 + 每单位多一份池与统计）或"全局一池"（句柄与单位解耦 ⇒ `GetState(Handle)` 无法直接定位）两条更差的路。槽位语义**刻意与池一致**（分配优先复用空闲槽、释放使代际 +1、新槽从 1 起、奇偶簿记），使"代际失配 = 悬空"在全插件只有一种读法。脏句柄一律**拒绝 + Warning、不 ensure**（时序竞态语义）。
- [x] **Step 4: 门面 `UTcsStateSubsystem`**——**✅ 2026-10-04 落地，含一处实施期裁定**。世界过滤三型 + `Deinitialize` 全量清理（登记表复位 + 全部桶清空；**发号器不复位**——`Id` 契约是"进程内永不复用"，复位会让新世界与上一世界撞号 = `WAIT-6` 那个缺陷形态）；申请/查询/移除/生命周期操作全套按计划落地，另加 `IsStateActive` / `GetStateCount` / `GetTotalStateCount` 三个观测口（装置断言与将来的统计面）。**实施期裁定 = 定义登记口的身份处理**：首次编译炸 `C2039: 'DefTag' is not a member of 'FTcsBuffDef'`——身份归**资产**（`UTcsStateDef::DefTag`：资产身份、`GetPrimaryAssetId()` 取值来源、作者期校验对象），而 `FTcsBuffDef` 是**内容**。故 `RegisterStateDef(FGameplayTag DefTag, const FTcsBuffDef&)` **收两个形参**；**不往 `FTcsStateDefBase` 加 `DefTag`**（否则策划在同一资产里手填两遍同一个 tag = 双真相）。附带 `AddReferencedObjects`：登记表是 `TUniquePtr` 容器、GC 看不见——本轮定义内容里通常没有对象引用（大概率空跑），但**缺一段 ARO 是静默失败类缺陷、修复成本为零**，故补上。实现文件按功能拆分（`TcsStateSubsystem.cpp` / `TcsStateSubsystem_Definition.cpp` + `State/TcsStateOps*.cpp` 三拆：主流程 / 查询 / 事件）。
- [x] **Step 5: 生命周期事件**——**✅ 2026-10-04 落地**。`Public/State/TcsStateEvents.h`：六枚原生 tag 带 `TCSSTATE_API`（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为裸 `extern` ⇒ 不带宏必 `LNK2001`），词全落既有 `TcsEvent` 根下（**零新增根**）；载荷 `FTcsStateEventPayload` = `Handle` / `DefTag` / `Source` / `Instigator` / `Stacks` / `Level` / `Cause`，`Cause` 注释写明"**仅 `Expired` / `Removed` 有意义**"。广播走 `PublishImmediate`（同一提交内到达 ⇒ 订阅者可在广播者那句日志之前读到实例，装置也能同帧断言）。
- [x] **Step 6: 广播点与 Phase 迁移**——**✅ 2026-10-04 落地**。施加（新实例）⇒ `Applied`；命中同组 ⇒ `Refreshed`（本轮判据 = 同单位 + 同 `DefTag`，**已在代码注释里标为 Task 5 的替换点**，"同组"三字旁边写着五轴）；到期 ⇒ `Expired`；显式移除 ⇒ `Removed`（原因由调用方给）；`UnregisterUnit` 逐条 `Removed` 后删桶。**撤销顺序 = 硬约束**：`Expiring` → 广播 → 归还槽位（先释放则订阅者拿到已清零的载荷）。阶段迁移矩阵四条合法边（含 `Expiring → Active`：刷新是"过渡后挂回"），非法迁移 `ensure`（配置错误语义）。
- [x] **Step 7: 编译 + 冒烟**——**✅ 2026-10-04 两半均过**。**编译半边**：Development Editor 与 Game Shipping **双配置 `Result: Succeeded`**、日志零 error 零 warning（修 `DefTag` 与 `ForEachBucket` 两轮后）。**冒烟半边**：宿主装置扩**检查 19a–19f**（常规命令）+ **检查 E/F/G**（拒绝面），PIE 实测 `Tcs.Test.Slice.Run` **26/0 + 7b PASS + 零红字**（唯一 `Warning:` 命中即 19f 的预期拒绝日志）、`Tcs.Test.Slice.Reject` **7/7**。证据 = `EVID-2026-10-04-state-instance-lifecycle`（双区段哈希 + 复算脚本 + 七条边界）。

**验收信号**：实例可建可查可删；六个事件 tag 至少三个（Applied / StackChanged / Removed）有真实广播与订阅者回执。

> **Task 2 落地结果（2026-10-04）**：实例可建可查可删 ✓；事件面实测**四枚有真实广播与订阅者回执**（`Applied` / `Refreshed` / `Expired` 路径同款 / `Removed`——超计划要求的三枚）；`StackChanged` 与 `Periodic` 两枚**只有声明、零广播**（分别随 Task 5 / Task 3，已在 `EVID-2026-10-04-state-instance-lifecycle` §6 边界⑦如实记）。
> **两处与计划文本的差异（留痕）**：① `FStateInstance` → **`FTcsStateInstance`**（命名成族）+ **新增 `Unit` 字段**（理由见 Step 2）；② 拒绝面探针**多扩两条**（计划只要求"装置侧调 ApplyState 建实例"）——`E/F/G` 三条状态面检查落进 `.Reject` 命令，其中 **G 用真实路径造陈旧句柄**（代际只在槽位释放时 +1，无法凭空构造）。

**非目标**：不做就绪状态机（`INTEG-3`/R7）；不做操作复制（`WAIT-3`）；不做查询门面 `UCombatEntityComponent` 侧接线（R7）。

---

## Task 3 — 参数快照 / 等级源 / Duration-Period-到期堆

**目标**：状态带"施加瞬间冻结的数值"与"到期/周期"两个时间语义——这是 buff 之所以是 buff 的两条腿。

**交付物**：

- `Source/TcsState/Public/State/TcsStateSnapshot.h`（新：`FTcsParamSnapshot` / `FTcsParamSnapshotEntry`）
- `Source/TcsState/Private/State/TcsStateSnapshotReader.h/.cpp`（新：`ITcsParamTableReader` 的快照适配器）
- `Source/TcsState/Public/Host/TcsEntityLevelProvider.h`（新：`ITcsEntityLevelProvider`）
- `Source/TcsState/Public/Param/TcsParamSource_StateLevel.h`（新：Array/Map 两型）
- `Source/TcsState/Public/Param/TcsParamSource_InstigatorLevel.h`（新：Array/Map 两型）
- `Source/TcsState/Public/State/TcsStateDuration.h` + `Private/State/TcsStateDuration.cpp`（新：到期/周期驱动）
- `Source/TcsCore/Public/Parameter/TcsParamSourceHost.h`（新，**R4.5-b**：`ITcsParamSourceHost`）
- `Source/TcsCore/Public/Parameter/TcsParamSource_HostDelegate.h` + `Private/Parameter/TcsParamSource_HostDelegate.cpp`（新，**R4.5-b**：宿主源转发器）

**提案**：`add-state-param-snapshot-and-level-sources` + `add-param-source-host-slot`（R4.5-b，可同批归档）

**依赖**：Task 2

> **提案面（2026-10-05，两份提案同时起草并获批；`openspec validate --strict` 两份均绿）**：
> - **`add-state-param-snapshot-and-level-sources`**：新能力 `state-param-snapshot` ADDED 六条（快照与构建规则 / 读取适配器 / 等级来源与读口 / 时值与周期语义 / 时长操作的堆同步 / 生命期与撤销顺序）+ `state-instance-lifecycle` MODIFIED 三条 + `state-def-asset` ADDED 一条（值约定白名单校验）。
> - **`add-param-source-host-slot`**：`param-value` MODIFIED 一条（补 `Instigator`）+ ADDED 一条（宿主参数源插槽）。
>
> **四处实施期裁定（当日拍板，逐条已在代码与规格里留痕）**：
> ① **等级读口不进 Core 上下文**——调研 R-1 原设想把 `ITcsEntityLevelProvider` 加成 `FTcsParamEvaluateContext` 的第三字段；**不采用**（Core 不该持有只有上层使用的域读口）。改用**已立规的扩展机制**：`TcsState` 的派生上下文 `FTcsStateEvaluateContext` 持该字段，域侧源以 `GetScriptStruct()->IsChildOf(...)` 判定后取值（先例 = 同族 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`）。**连带**：接口住 `TcsState/Public/Host/`（读口随消费者所属的领域层），不是计划原写的 `TcsCore/`。
> ② **`Instigator` 反而进基础上下文**——实施期编译暴露它**不在** Task 0 补的两个字段里（那次只补了 `Subject` 与 `EffectiveLevel`），而发起者等级源必须读它。裁定的**分界判据**：跨域通用的主体身份（`Subject` / `Instigator` / `EffectiveLevel`）进基础；某域的读口进该域派生上下文。
> ③ **快照类型名取设计名**——实现名 = `FTcsParamSnapshot` / `FTcsParamSnapshotEntry`（`SPEC-02-states` §3.6 的 2026-09-14 命名批），**不是** Task 2 注释里预留的 `FTcsStateParamSnapshot`：快照是**参数域**概念（修正器物化与 R6 技能侧参数行都要读它），不是状态模块专属。Task 2 那条注释随之作废（评审时若倾向成族命名，改这一处即可，代价 = 三个文件）。
> ④ **读取适配器是纯 C++ 类**——`FTcsStateParamTableReader` 本轮**不派生 `UObject`、也不实现 `ITcsParamTableReader`**：唯一消费方（Task 4 的物化器）是 C++ 直调，而该接口**不是 `Blueprintable`**（宿主脚本实现不了）⇒ 造 `UObject` 壳属"零消费者预建"，且会把快照指针的寿命绑到 GC 上。继承接口的时机 = 出现第一个要把快照当参数表挂进上下文的反射消费方。
> ⑤ **Step 9 的文件面收窄**：接口与转发器**同住一个头**（`TcsParamSourceHost.h`）——同族先例 `TcsParamSource_AttributeScaled.h`（接口 + 派生上下文 + 源三者同处一文件）；计划原写的 `TcsParamSource_HostDelegate.h/.cpp` 两个文件**不建**（拆开只多一跳）。连带补 `FTcsParamValueSource` 的**虚析构**（`C4265` 的根因修复：多态基类 + 派生源持非平凡析构成员 = 经基类指针删除是 UB）。
>
> **一处实施期发现（引擎/UBT 事实，供后续轮参考）**：（`TMap` 元素含 `TUniquePtr`）的类型**不能**加模块导出宏 `TCSSTATE_API`——MSVC 会强制实例化 `TMap` 的复制路径并报 `C2280`（`TSparseSetElement` 复制到已删除的函数）。跨模块可达性改由**门面**承担（`UTcsStateSubsystem` 带宏、它作为成员持有登记表）。
>
> **另一条（同批踩到，装置侧）**：引擎 `TimerManager.h` 的 `SetTimer` 句柄形参是**非 const 左值引用** ⇒ 传临时量（`FTimerHandle()`）或**捕获副本**（lambda 按值捕获进来的成员默认 const）都绑不上（`C2665`）。延迟探针的写法 = **句柄用回调体内的局部左值**；且**不要**把裸指针捕获的 `World` 当 weak lambda 的宿主（`CreateWeakLambda` 的宿主必须是捕获得来的**弱引用**或对象指针本身）。

- [x] **Step 1: 快照类型**——`FTcsParamSnapshotEntry{ FGameplayTag Key; double Value; FInstancedStruct SourceRef; }`（**源引用位**供 Debug 与将来 Live 化；**禁用 `Resolved*` 命名**——动词纪律）；`FTcsParamSnapshot{ TArray<FTcsParamSnapshotEntry> Entries; }` + `TryGetNumericParam(FGameplayTag, double&) const`。
- [x] **Step 2: 快照构建**——`Apply` 时对 `Def.Params` 逐行求值：`Overrides`（施加方覆盖）**优先**，否则 `Def` 默认值；求值上下文 = `FTcsParamEvaluateContext{ ParamTable = 施加方参数表, Subject = Target, EffectiveLevel = Level }`；**`ValueConvention` 转换在此写入点发生**（`FTcsValueConvention::ConvertToCanonical`——**全库首次点亮**），快照内**永远规范值**。
- [x] **Step 3: 快照读取适配器**——`FTcsStateParamTableReader : public ITcsParamTableReader`（把 `FTcsParamSnapshot` 当参数表暴露给 `FTcsParamSource_ParamRef` 与修正器物化）——**这是 Task 4 物化器的输入口**，本步先落类型与 `TryGetNumericParam`。
- [x] **Step 4: 等级源与宿主契约**——`ITcsEntityLevelProvider`：`UINTERFACE(Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) int32 GetEntityLevel(FTcsCombatEntityHandle Entity) const`（**不带 `const` 在 `BlueprintNativeEvent` 上是硬规则**——按 UHT 要求去掉 `const` 并补 `_Implementation` 声明），宿主实现；门面加 `SetEntityLevelProvider(TScriptInterface<...>)` / `GetEntityLevelProvider()`（`UPROPERTY` 持有；未注入 = `nullptr`，是**配置状态**不是错误）。
  四个源：`FTcsParamSource_StateLevelArray{ TArray<double> Values; }`（读 `Context.EffectiveLevel` → 下标，越界落最后一档）/ `_StateLevelMap{ TMap<int32,double> Values; }` / `_InstigatorLevelArray` / `_InstigatorLevelMap`（读 `Context.Subject` → provider → level）。四型均继承 `FTcsParamEnumerableSource`（Task 0 Step 2），`GetIndexForLevel` 即"索引解析唯一真相在源"。
  上下文补第三个字段：`UPROPERTY(BlueprintReadWrite) TScriptInterface<ITcsEntityLevelProvider> LevelProvider;`（同 `ParamTable` 先例——`DAMAGE-5` 只记了两个字段，实现期按需补第三个，**记为对台账条目的增量**）。
- [x] **Step 5: Duration 与 Period 落堆**——`Finite`：`PushExpiry(Clock->GetClock().Elapsed + DurationTime, OwnerId, 回调)`；`Infinite`：不入堆、永不过期。`Period > 0`：**重复到期条目**——每次周期回调广播 `TcsEvent.State.Periodic`（载荷带当前 `Stacks` + `Level`）后**重新入堆**；默认**等首个周期**（"施加即生效"由 apply 响应链表达）。
- [x] **Step 6: `PeriodRefresh` 与生命周期操作**——刷新（Refresh/StackChange）时按 `EPR_Keep`（不动）/ `EPR_Reset`（重建周期条目）/ `EPR_Immediate`（立即执行一次并重置）；`ExtendDuration(handle, Δ)` / `SetRemaining(handle, T)` = 撤销旧条目 + 按新余量重入堆（**句柄配对清理**），`Infinite` 上调这些口 = `Warning` + 无操作。
- [x] **Step 7: 到期路径**——到期回调（`UWorld` 弱引用 + 句柄代际校验）→ `ExpireState` → `EStateRemoveCause::Expired` → Task 2 的广播与回收链。
- [x] **Step 8: 编译 + 冒烟**——Finite + Period buff：施加 → 若干个 `Periodic` → 到期 `Expired` + 实例回收（**装置逐 tick 轮询观察**，勿用"下一 tick"假定）。

- [x] **Step 9: R4.5-b 参数源族宿主插槽（同批落地——同族同验证面）**——`ITcsParamSourceHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + 两个 `UFUNCTION(BlueprintNativeEvent)`：`double Evaluate(const FTcsParamEvaluateContext& Context)` / `bool AllowsValueConvention()`）+ `FTcsParamSource_HostDelegate : FTcsParamValueSource`（持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`；两个虚函数**纯转发** + 空 Host 守卫 = 返回 0 / true）。
  两处都住 `TcsCore/Public/Parameter/`（`ITcsParamTableReader` 已在此；转发器 MUST 与基类同模块）；**载体与调用点零改动**（`FTcsParamValue.Source` 是裸 `FInstancedStruct`，照装）。
  **两个虚函数都要转发**（R-1 调研的硬判据）——漏 `AllowsValueConvention` 会让宿主新源在本轮 Task 3 的 `ValueConvention` 白名单上拿到错误的默认能力位。
  验证（R-1 调研已定的流程）：UBT 编译 + **glue 产物核验**（"能导出 ≠ 能往返"——确认 `Evaluate` 生成真实方法体，非空壳）+ **C# 实现实测**（配到 `FTcsParamValue.Source` 后求值走脚本；**复用 SCRIPT-8 的探针形态，不得重写**）。规格 delta = `param-value` 能力 ADDED 一条"宿主参数源插槽"需求 + 场景。
- [x] **Step 10: 编译 + 冒烟收口**——本节全部落地后重跑双配置编译 + Task 3 的冒烟（快照/等级源/周期），确认 Step 9 未动既有求值路径。

**验收信号**：快照数值随 `Overrides` / Def 默认 / 等级源三者组合正确变化；周期事件计数与到期时刻可复现（同一 Seed 下逐字一致）；**C# 自定义源能在链里求值**（R4.5-b 判据）。

**非目标**：不做参数链（R6 `STAT-1`）；不做快照 Live 化（`Live` 模式本轮只在 `ETcsParamMode` 上留值）。

> **Task 3 落地结果（2026-10-04）**：Step 1–10 **全勾**；两份提案归档（`add-state-param-snapshot-and-level-sources`：`state-param-snapshot` **新能力 ×6** + `state-instance-lifecycle` ×3 MODIFIED + `state-def-asset` ×1 ADDED；`add-param-source-host-slot`：`param-value` ×1 MODIFIED + ×1 ADDED），`openspec validate --all --strict` = **29 passed / 0 failed**、`changes/` 零活动提案。
> - **验收读数**：双配置编译**零 warning / 零 error**；`Tcs.Test.Slice.Run` **即时 32/0 + 延迟段 39/0**（新增 `20a`–`20e` 数值面、`20f`–`20l` 时间面、一条"夹具就绪"判定）；`Tcs.Test.Slice.Reject` **9/0**（新增 H/I）。证据 = `EVID-2026-10-04-state-param-snapshot-and-level-sources`（区段 L2416–L2760、345 行 / 49,691 字节 / SHA-256 `7b82a338…`、复算脚本、**9 条边界**）。
> - **四处实施期裁定（详见 Task 3 头部的《提案面》块）**：① 等级读口不进 Core 上下文（落 `FTcsStateEvaluateContext`）；② `Instigator` 反而进基础上下文（分界判据 = 通用主体身份 vs 域读口）；③ 快照取设计名 `FTcsParamSnapshot`（Task 2 预留的 `FTcsStateParamSnapshot` 作废）；④ 读取适配器是**纯 C++ 类**（`ITcsParamTableReader` 不是 `Blueprintable`，不预建 `UObject` 壳）。
> - **两处交付物面收缩**：Step 9 的接口与转发器**同住一个头**（`TcsParamSourceHost.h`，同族先例 `TcsParamSource_AttributeScaled.h`）；计划的 `TcsStateDuration.h/.cpp` **未建**——入堆/撤堆集中在 `TcsStateOps_Lifetime.cpp`（"时长驱动"的全部逻辑就是那四个函数，另开一层的抽象成本大于收益）。
> - **三处同类判据（本轮装置侧踩坑沉淀，全部非机制问题）**：探针词未声明时的**静默无效 tag**、漏施加探针导致四条断言在空夹具上说谎、`ForEachState` 未按定义过滤取到"桶里第一个实例"、定时器绑装置 Actor + 采样延时拿世界时钟反算得到负数——四条都记进证据 §4，判据两条：**夹具不完整会让断言说谎**、**拿世界时钟当探针相对表必错**。
> - **两处如实边界（交 Task 7 / 内容侧）**：① 验收资产 `DA_Check_BuffDef` 是 `Finite` 而 `DurationTime` 求值为 0（4 条 `状态时值非正` Warning 的来源，不影响断言但让"零红字"严格不成立）；② **R4.5-b 的 C# 侧实测未做**（静态面 + glue 核验已过），归 Task 7 端到端或单独补探针。
>
> **Task 3 收口后的两笔补账（2026-10-04 当晚，均已闭环）**：
> - **① 验收资产的时值改为 3600**（走编辑器侧资产通道：读 → 整块回写 → 回读 → `save_assets` 落盘）⇒ 4 条"时值非正"Warning 消失、台账 **`Task-3-1` 关闭**；"Finite 且在测试窗口内真的到期"转为**明确未覆盖**（证据 §5 边界④）。
> - **② R4.5-b 的 C# 侧行为面补齐**：新夹具 `UTcsDevGcParamSourceHost`（脚本参数源，返回 13）+ 探针扩成**五槽位**（链里加第二发 `FTcsStepDamage`，其 `DamageBase.Source` 装 `FTcsParamSource_HostDelegate`）。读数 = `已登记被测脚本对象 5 个`、`槽位 参数源槽位：存活`、GC 前后各见 `call=N incoming=13`、`[PASS] 基线 … 扣血 14（预期 14 = 每发 7 × 2 发）`、`[PASS] 正例通过`。证据 §5 边界⑦由"只到静态面"改写为"静态 + 行为双证"。
>   - **一处判据订正（留档）**：第一版把"扣量"当参数源槽位的判据（写成"公式 7 + 参数源 13 = 20"），实测翻车——**两发 Damage 走同一流程模板**，模板里的公式把 `incomingBase` 一律覆写成 7 ⇒ 扣量恒为 7 × 2。正确落点是**调用参数**（`call=2 incoming=13`），判据因此改挂到"公式那一行的入参"并给公式补了调用序号。**判据落点选错会让"通过"与"不通过"都失去意义**。

---

## Task 4 — 修正器物化（D3-19）

**目标**：让 buff **真的改数值**——`ModifierRows` 在 apply 时物化成 M2 账本条目（`Source` = 状态句柄），移除时按来源级联摘除。这是本轮第一个"肉眼可见"的硬信号。

**交付物**：

- `Source/TcsState/Public/State/TcsStateModifierMaterializer.h` + `Private/State/TcsStateModifierMaterializer.cpp`（新）
- `Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h`（改：补一个**显式构造/填充入口**——今天"全仓从未构造过该结构"）
- `Source/TcsState/Private/State/TcsStateOps_Modifier.cpp`（新：apply/remove 两个挂点）

**提案**：`add-modifier-materialization`

**依赖**：Task 3（快照 = 物化输入）

- [x] **Step 1: 物化入口形状**——**✅ 2026-10-04 落地，含一处签名订正**。实际签名 = `FTcsStateModifierMaterializer::Materialize(UTcsStateSubsystem& Subsystem, const FTcsBuffDef& Def, const FTcsStateInstance& Instance, TArray<FTcsAttrModInstance>& Out)`——**收实例而不是散字段**（计划原写的 `Snapshot` / `Source` / `Target` 三个形参在"实例自持 `Unit`/`Source`/`Level`/`ParamSnapshot`"之后全是冗余，且多形参只会让"某处少填一个"成为可能，而少填在源侧表现为**静默落兜底**而不是报错）；上下文经 `FTcsStateOps::MakeContext` **同一装配点**获得（与快照构建同源）。
  逐条流水：`ModifierRows` → 解析 `UTcsAttrModDef`（`Get()` 优先、未加载才 `LoadSynchronous()`）→ `FTcsAttrModOperandDef` 转运行侧 `FTcsAttrModOperand`（`OPK_Literal` 经上下文求值；`Kind == OPK_AttributeScaled` **不物化**、原样过）→ `ValueConvention` 规范转换（能力位为假则不转）→ 经 `FTcsAttrModInstance::MakeFromDef` 装配（`Source` = 状态实例来源句柄）。
- [x] **Step 2: apply 挂点**——`ApplyState` 流程里排在**快照构建之后、`Applied` 广播之前**：物化 + 逐条 `ApplyModifier`，外层 `BeginBatch` / 末尾 `Commit` 包裹（M2 事务语义：多条修正器 = 一次重算 + 一次广播）。**并加一处重入纪律**：提交会广播 ⇒ 之后 MUST 重新定位实例与桶（订阅者可在提交的广播里重入状态操作）。
- [x] **Step 3: remove 挂点**——`Remove/Expire` 流程里 `RemoveBySource(Target, 实例来源句柄)`（**按来源级联摘除**，与将来的触发行退订同一来源句柄），**批内提交**；顺序 = **摘除 → 撤时间条目 → `Expiring` → 广播 → 归还槽位**，且摘除**排在"取实例指针"之前**（理由同 Step 2 的重入纪律；既有守卫天然接住重入造成的失效）。
- [x] **Step 4: 属性访问解析点（Q-2 缓解①）**——实际文件 = `Private/State/TcsStateAttributeAccess.h`（**改名以避让 Task 6 的同名头**：UHT 要求全项目头文件名唯一，计划原文与 Task 6 计划项都写作 `TcsAttributeAccess.h`）；实现体落 `Private/State/TcsStateOps_Modifier.cpp`（该文件是 TcsState 内**唯一** include 属性门面头的地方）。本类的形状即纪律：**薄包装**（暴露面 = 白名单方法，不把门面指针交出去），本轮暴露 `ApplyModifier` / `RemoveBySource` / `BeginBatch` / `Commit` + 一个存在性判据 `IsLedgerReady`（只答"账本认不认识这个单位"）；头注释写明"将来补 `ITcsAttributeAccess` 注入契约时**只换这一处**"。
- [x] **Step 5: 编译 + 冒烟**——**两半均 ✅ 2026-10-04**。**编译半边**：Development Editor 与 Game Shipping **双配置 `Result: Succeeded`**、日志零 error 零 warning。**冒烟半边**：装置扩**检查 21a–21h**（常规命令）+ **检查 J**（拒绝面），PIE 实测 `Tcs.Test.Slice.Run` **即时 40/0 + 延迟累计 47/0**、`Tcs.Test.Slice.Reject` **10/0**；读数 = 账本条目 `0→1`（两属性各一条）/ 护甲 `5→-15`（快照 `-20`，兜底 `-999` 未被取用）/ 攻击 `30→30.5`（写 50 的百分比约定）/ 定序"属性变更 37 < `Applied` 38" / 刷新后条目仍 `1/1` 且值随覆盖 `-35` / 移除后条目 `0/0` 且两值归位。证据 = `EVID-2026-10-04-state-modifier-materialization`（双区段哈希 + 复算脚本 + 十条边界 + 一处装置缺陷留痕）。

**验收信号**：**属性数值前后可观测**（两条观测面：`EvaluateCurrent` 读数 + `FTcsAttributeChangedEvent` 广播）——✅ 两半都实测：`EvaluateCurrent` 前后可分辨（21b/21c/21e/21f），属性变更广播到达且**早于** `Applied`（21d 的定序读数）。

**非目标**：技能参数行分派（R5.5-f）；关系表检查器（R5.5-e）；参数链式聚合（R6）。

> **Task 4 落地结果（2026-10-04 / 归档于 2026-10-05 零点后）**：Step 1–5 **全勾**；提案 `add-modifier-materialization` 归档为 `2026-10-05-add-modifier-materialization`（**新能力 `state-modifier-materialization` ×7 ADDED** + `state-param-snapshot` ×1 MODIFIED），`openspec validate --all --strict --no-interactive` = **30 passed / 0 failed**、`changes/` 零活动提案。
> - **验收读数**：双配置编译**零 warning / 零 error**；`Tcs.Test.Slice.Run` **即时 40/0 + 延迟累计 47/0**（新增 21a–21h）、`Tcs.Test.Slice.Reject` **10/0**（新增 J）。证据 = `EVID-2026-10-04-state-modifier-materialization`（双区段哈希 `5dedd64b…` / `eeb895d5…`、复算脚本、**十条边界**、一处装置缺陷留痕）。
> - **一处裁定修订（重要，替代 Task 3 的推论文本）**：Task 3 裁定"读取适配器是纯 C++ 类、**不造 UObject 壳**"，理由是"唯一消费方是 C++ 直调"。本轮落地时该**推论**被推翻——物化求值要经**反射上下文**把"该实例的快照"交给参数源，而"读参数表"这件事**已有反射契约**（`FTcsParamEvaluateContext::ParamTable`）；若为它另开一条纯 C++ 表通道，同一问题（"本次求值的参数表是哪张"）会出现两个不一致的答案。故按甲案落地：**纯类本体不动**（仍是查找语义的唯一实现），**另加薄壳** `UTcsStateParamTableReader`（只委托）+ 门面单例 + `FTcsStateSnapshotScope`（**RAII 栈式绑定**，嵌套按栈恢复）。判据链与两条被否方案见 `LOG-DECISIONS`。
> - **四处对计划文本的订正（逐条留痕）**：① `Materialize` 签名**收实例**而非散字段（`Snapshot`/`Source`/`Target` 三形参在实例自持后冗余，且多形参让"少填一个"成为可能——少填在源侧表现为静默落兜底）；② 属性访问解析点文件名改 `Private/State/TcsStateAttributeAccess.h`（计划原文与 Task 6 计划项同名，撞 UHT"全项目头文件名唯一"）；③ 允许触碰的属性 API 上限从"三个"改述为"**语义面三个 + 事务对（`BeginBatch`/`Commit`）+ 一个存在性判据（`IsLedgerReady`）**"——计划 Step 2 要求批包裹而 Step 4 只许三个，是自相矛盾，以"能站住的纪律"为准；④ 解析点实现体落 `Private/State/TcsStateOps_Modifier.cpp`（该文件是 TcsState 内唯一 include 属性门面头处，越界在编译期就不可达）。
> - **一处交付物面收缩**：反射壳与纯类本体**同住一个头**（`Public/State/TcsStateParamTableReader.h`，同族先例 = R4.5-b 的接口 + 转发器同头），未新开文件。
> - **一处装置缺陷（已修，留痕）**：21h 首版夹具用"真造单位 + 手动注销属性账本"构造"无账本单位"，导致拆除时组件 `EndPlay` 二次注销而 **ensure** ⇒ 判据 = **夹具的形状自己造出红字，与被测机制无关**；改用"从未注册的句柄"。含缺陷那一轮的区段哈希记进证据 §4.1（**不作验收锚点**）。
> - **一处口径订正（影响后续所有轮）**：`Run` 命令**本来就不是字面"零红字"**（19f、20j 是故意红字，本轮实测恰 3 条 Warning）⇒ "零红字"的准确读法是"**零非预期红字**"，且装置头部 MUST 逐条列出预期红字与归属（本轮已改装置两处头部文案并重编重跑，使日志区段与源码严格一致）。
> - **两条边界入册台账**：`ATTR-1`（修正器模板身份词 `TemplateTag` 的根归属未定——`ModifierRows` 走资产直引用，该词零解析消费者）、`STAT-5`（**状态移除的广播窗口内重入移除同一句柄 ⇒ 双次归还槽位**——既有窗口，本轮只闭合了自己引入的两处）。

---

## Task 5 — 五轴堆叠与刷新政策（含 Custom 决策 Fragment）

**目标**：同一 buff 的重复施加有确定行为——层数、分组、溢出、数值叠加、刷新对时长的影响。

**交付物**：

- `Source/TcsState/Public/State/TcsStateStackPolicy.h` + `Private/State/TcsStateStackPolicy.cpp`（新：五轴枚举 + `FStateStackPolicy` + 决策 Fragment 基类）
- `Source/TcsState/Private/State/TcsStateOps_Stack.cpp`（新：共存决策与刷新）
- `Source/TcsState/Public/State/TcsStateStackFragment.h`（新：`FTcsStateStackDecisionFragment` 反射基类）

**提案**：`add-state-stacking-policies`

**依赖**：Task 4

- [x] **Step 1: 五轴定义**——`EGroupByPolicy{ EGB_None = 0, EGB_PerSource = 1, EGB_PerInstigator = 2, EGB_PerTag = 3, EGB_Custom = 1<<4 }`（**值 0 = 默认/None、Custom 走逃逸位**：0 与 Custom 位各自的语义 MUST 在枚举注释里写清）；`MaxStacks`（`≤0` = 无限）；`EOverflowPolicy{ RejectNew, ReplaceOldest, ReplaceNewest }`；`EValueStackPolicy{ KeepMax, AddValues, PerStackValue }`；`EStackDurationPolicy{ None, RefreshRemainingToTotal }`。
- [x] **Step 2: 组键与共存决策**——组键 = `GroupBy` 四态的确定性组合（`PerSource` → 来源句柄；`PerInstigator` → 发起者句柄；`PerTag` → 自定义分组 Tag）；命中组内按 `MaxStacks` 与 `Overflow` 决策：`RejectNew`（拒绝并返回 `EAR_Rejected`）/ `ReplaceOldest` / `ReplaceNewest`（移除旧实例 + 新施加 + 广播 `StackChanged`）；未满仓则 `Stacks++` + 广播。
- [x] **Step 3: 刷新语义**——同组同来源的重新施加 = `Refresh`：按 `ValueStack` 更新数值（`KeepMax` 取大 / `AddValues` 累加 / `PerStackValue` 按层取值）→ **快照重建**（新 payload 覆盖，D3-12）→ 按 `StackDurationPolicy` 决定剩余时长（`RefreshRemainingToTotal` = 回到满额）→ `EPR_*` 作用于周期条目 → 广播 `TcsEvent.State.Refreshed`。
- [x] **Step 4: Custom 决策 Fragment（唯一的策略落点）**——`FTcsStateStackDecisionFragment`（`USTRUCT(meta = (Hidden))`，USTRUCT 反射基类 + 虚分派 + 中性默认实现）：`virtual bool ShouldAccept(...)` / `virtual int32 ResolveStacks(...)` 一类**按决策种类**的抽象；`FStateStackPolicy` 以 `TInstancedStruct<FTcsStateStackDecisionFragment>` 持有一个 Custom 位；**策略实例归 Def 资产持有**（单字段暴露，类/载荷配对 bug 类消失）。
  **MUST NOT** 预建 `IValueDomainPolicy` / `ICostPolicy`（零消费者，Q-6）。
- [x] **Step 5: 编译 + 冒烟**——四种组合各跑一遍：`None + ∞`（`NoMerge` 等价）/ `MaxStacks=1 + ReplaceOldest`（`UseNewest` 等价）/ `MaxStacks=1 + RejectNew`（`UseOldest` 等价）/ `PerInstigator + AddValues`（`StackByInstigator` 等价）——**组合覆盖 TCS 四个旧 Merger 的行为**（规格 §3.2 的等价表即验收清单）；另跑一个 Custom 策略样本。

**验收信号**：五轴组合下 `Stacks` / 数值 / 剩余时长的行为与规格 §3.2 等价表逐条一致；Custom 策略被真实调用（日志可证）。

**非目标**：不内建 Overflow 升级替换（规格明文：走"`StackChanged` 满仓触发行 → `ApplyState` 强力 → `RemoveState` 原"三步组合）；不做状态级重定向（R5.5-d 邻域）。

> **Task 5 落地结果（2026-10-05）**：Step 1–5 **全勾**；提案 `add-state-stacking-policies` 归档为 `2026-10-05-add-state-stacking-policies`（**新能力 `state-stacking-policies` ×6 ADDED** + `state-instance-lifecycle` ×2 MODIFIED），`openspec validate --all --strict --no-interactive` = **31 passed / 0 failed**、`changes/` 零活动提案。
> - **验收读数**：双配置编译**零 warning / 零 error**；`Tcs.Test.Slice.Run` **即时 53/0 + 延迟累计 60/0**（新增 22a–22m）、`Tcs.Test.Slice.Reject` **11/0**（新增 K）。证据 = `EVID-2026-10-05-state-stacking-policies`（双区段哈希 `0841f12a…` / `dc3917a9…`、复算脚本、**十二条边界**）。
> - **五处对计划文本的订正**：① Step 1 的 `GroupBy` 四态 ⇒ **三态 + Custom**（`EGB_PerTag` 与 `GroupTag` 裁撤：组键基座含 `DefTag`、分组词取自定义自身 ⇒ 该档与 `None` 完全同义；用户当场拍板）；② Step 1 的 `EOverflowPolicy` 三档 ⇒ **两档**（替换最旧 / 替换最新在「一组一条实例」下同义）；③ Step 2 括号注（替换也播 `StackChanged`）⇒ **只在层数真变化时播**（以事件词语义为准）；④ Step 4 的 `TInstancedStruct<…>` ⇒ **裸 `FInstancedStruct` + 手写 `BaseStruct` 元数据**（全仓 2026-09-24 换型口径）；⑤ Step 5 首行等价表 `NoMerge = None + ∞` ⇒ 加**近似**注（真正「每次施加独立成实例」改用 Custom 决策 Fragment 表达——本轮装置样本即此用法）。
> - **一处实现缺陷（本轮发现并修）**：`ScheduleTime` 在刷新路径上**无条件回满额** ⇒ `EStackDurationPolicy::ESD_None`（不动时长）这一档**在实现上从未存在**；本轮改为在时间重挂点按轴读，两档读数可分（回满额 8.000 / 保留 3.000）。
> - **一处实施期口径（超出计划）**：**未声明来源按「同来源」处理**（否则不关心来源的调用方在默认策略下**每次施加都叠一层**）；副作用 = 既有检查 19c / 21e **无需改动**即可继续成立。
> - **一处交付物面收缩**：计划写的 `Private/State/TcsStateStackPolicy.cpp` **未建**（策略是纯数据头、无实现体）；决策词汇（`FTcsStateStackRequest` / `FTcsStateStackDecision`）与 Fragment 基类**同住一个头**（`TcsStateStackFragment.h`；同族先例 = Task 4 的反射壳与纯类本体同头）。
> - **三条边界入册台账**（53 ⇒ 56）：`STAT-6`（`PerStackValue` 零行为）/ `STAT-7`（跨定义共享层数组）/ `STAT-8`（逐层来源归属）。
> - **一处非本轮引入的文案偏差（登记、未改）**：拒绝面检查 I 的内联文案写「预期 2 条 Warning」，实测该检查只产生 1 条——它不是验收锚点（头部状态面总数 9 与实测一致），交 Task 8 对账一趟处理。

---

## Task 6 — 链原语与行为面接线

**目标**：把前面全部接进链里——链能施加状态、能写属性，buff 自己挂的触发行能在自己的生命周期事件上起链。**R5 的收口动作。**

**交付物**：

- `Source/TcsState/Public/Chain/TcsStepApplyState.h` + `Private/Chain/TcsStepApplyState.cpp`（新：`FTcsStepApplyState` + 自注册）
- `Source/TcsEffect/Public/Chain/TcsStepModifyAttribute.h` + `Private/Chain/TcsStepModifyAttribute.cpp`（新：`FTcsStepModifyAttribute` + 自注册）
- `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.cpp`（改：补 `AttributeCompare` 条件求值器 + 自注册）
- `Source/TcsEffect/Private/Chain/TcsAttributeAccess.h/.cpp`（新：属性访问解析点，**TcsEffect 侧副本**——与 Task 4 的 TcsState 侧同名不同文件，MUST NOT 同名头文件）
- `Source/TcsState/Public/State/TcsStateBehaviorFragment.h`（新）+ `Private/State/TcsStateOps_Behavior.cpp`（新）
- `Source/TcsState/Private/State/TcsStateOps_Trigger.cpp`（新：内联触发行登记/退订）

**提案**：`add-state-chain-primitives` + `add-state-behavior-fragments`（同 Task 内可合并归档）

**依赖**：Task 5

- [x] **Step 1: `FTcsStepApplyState`（TcsState 领域步骤）**——字段：`FTcsCombatEntityHandle Target`（无效则取 `Context.Targets[0]`，仍无效 ⇒ 软失败）/ `FGameplayTag DefTag` / `TMap<FGameplayTag, double> Overrides`。
  执行体照 `FTcsStepSetVar` 形状（即时步骤、无挂起）：经 `Run.Owner->GetWorld()` 取 `UTcsStateSubsystem` → `ApplyState(...)`。
  **来源句柄 = 本次链运行态**（`Run.Self` 或链级来源位——实施时按"来源应表达谁施加的"定，并在头注释写明）；失败 ⇒ `Warning` + `TSR_Completed`（**不断链**），仅 `DefTag` 不可解析时 ⇒ `Error` + 断链。
- [x] **Step 2: `FTcsStepModifyAttribute`（TcsEffect 机制原语）**——字段：`FTcsCombatEntityHandle Target` / `FGameplayTag Attribute` / `ETcsAttributeOp Op`（复用 M2 枚举，**不新造词表**）/ `FTcsParamValue Operand`。
  执行体经 `Run.Owner->GetWorld()` + **属性访问解析点** 取 `UTcsAttributeSubsystem` → 构造一次性 `FTcsAttrModInstance`（`Source` = 链运行态来源）→ `ApplyModifier`。
  **边界**：`TcsEffect` 本轮首次 include `TcsAttribute`——头注释 MUST 写明"允许依赖**下层**领域模块（Attribute），MUST NOT 依赖上层（Damage/Targeting/State/Skill）"。
- [x] **Step 3: `AttributeCompare` 触发条件**——求值器注册进 `FTcsTriggerConditionRegistry`：条件数据 `{ FGameplayTag Attribute; ETcsAttributeComparison Comparison; double Threshold; }`（`Greater/Less/GreaterOrEqual/LessOrEqual`）；求值经属性访问解析点读 `EvaluateCurrent`（**与 Step 2 共用同一解析点**）。头注释写明"依赖属性读取注入位（本轮 = 直接依赖 `UTcsAttributeSubsystem`）"。
- [x] **Step 4: 内联触发行（`TRIG-4`）**——`Apply` 时逐条 `Def.Triggers` 构造 `FTcsEffectTriggerInstance{ Def, Source = 状态实例来源句柄 }` → `UTcsEffectSubsystem::RegisterTriggerRow(...)`（**来源句柄取自 Task 0 的统一发号器**，与修正器物化**共用同一个来源句柄**——同一个状态 = 同一个来源，摘除一次全清）；`Remove/Expire` 时 `UnregisterTriggerRowsBySource(同一句柄)`。规格背书：`openspec/specs/effect-trigger/spec.md`「施加状态时登记 ⇒ `Source` = 状态实例句柄」。
- [x] **Step 5: 行为 Fragment 订阅挂接**——`FTcsStateBehaviorFragment`（`USTRUCT(meta=(Hidden))`，反射基类 + 中性默认实现）：`TArray<FGameplayTag> Interests` + `virtual void OnStateEvent(const FGameplayTag& EventTag, const FInstancedStruct& Payload, const FStateBehaviorContext& Ctx)`；`Apply` 时按 `Interests` 向总线订阅（`Source` = 状态句柄，复用 D4-1 的订阅配对 + 来源级联退订机制），`Remove/Expire` 自动退订。**`FragmentSet` 归 Def 持有**（实例零策略）。
- [x] **Step 6: 编译 + 冒烟**——一条链：`ApplyState(buff)` → buff 的 `Triggers` 订阅 `TcsEvent.State.Applied` → 起一条行为链 → 行为链里 `ModifyAttribute` 改属性 → `RemoveState` 后触发行级联退订、修正器级联摘除。
- [x] **Step 7: 规格同步**——`SPEC-03-effects` 的"纯机制层"边界句改述为"**不依赖上层领域模块**"（Q-2 缓解②）；`SPEC-02-states` §1 的 `FStepApplyState` 落地状态更新。

**验收信号**：链里施加状态成功；buff 自带行为链被自己的生命周期事件驱动；来源级联一次摘干净（触发行 + 修正器同源）。

**非目标**：`Heal` / `Parallel` / `Repeat` / `OnError`（R5.5-a/b）；链侧中断/取消（M5/R6）；`ApplyState` 的操作复制（R7）。

> **Task 6 落地结果（6a 部分，2026-10-05）**：
> - **新增类型面**：`FTcsStepApplyState`（TcsState，`Public|Private/Chain/` 新目录 + 自注册）· `FTcsStepModifyAttribute`（TcsEffect + 自注册）· `ETcsAttributeComparison`（TcsAttribute 新头）+ `UTcsAttributeSubsystem::Resolve`（**唯一门面查找点**）+ `FTcsEffectAttributeAccess`（TcsEffect 侧白名单薄壳，`Private/Attribute/`）· `FTcsTriggerCondition_AttributeCompare`（第三条内置条件：数据住 `Public/Trigger/TcsTriggerCondition.h`、求值器 + 自注册住 `Private/Trigger/TcsTriggerCondition.cpp`）· `TcsStateOps_Trigger.cpp`（内联触发行登记 / 退订；TcsState 内**唯一** include `TcsEffectSubsystem.h` 处）· `FTcsStateInstance::CascadeAnchor`（级联锚点，与 `Source` 解耦）· `FTcsEffectContext::RunSource` / `CausedBy`（链运行态"身份 + 因果边"）+ `UTcsEffectSubsystem::GetRunSource` / `GetRunCausedBy`（只读观测面，**非 `UFUNCTION`**）。
> - **七处计划文本订正（逐条留痕）**：① Step 1 的"来源句柄 = 本次链运行态（`Run.Self` 或链级来源位）"**无落点**——`FTcsChainRun`（11 字段）与 `FTcsEffectContext` 本无来源位，`Self` 是运行态自身句柄且类型换不成 `FTcsSourceHandle` ⇒ 改为**黑板新增 `RunSource`**（起链发新号、**子链另发**）。② Step 1 的"仅 `DefTag` 不可解析时 ⇒ `Error` + **断链**"**不可表达**——`ETcsStepResult` 只有 `Completed` / `Running` 两档，**步骤无法中断链**（断链只发生在"执行器未登记"时，由门面自己做）⇒ 统一为 `Warning` / `Error` + `TSR_Completed`，软失败接管归 `OnError`（R5.5-a）。③ Step 3 写的 `Public/Trigger/TcsTriggerCondition.cpp` **不存在该形态**（条件求值器住 `Private/Trigger/`）。④ Step 4 的"与修正器物化**共用同一个来源句柄**"改为共用同一**锚点**——`Source` 已退回"施加方身份"（一个来源可施加多个定义，按它撤销会互相误摘 ⇒ 走 `CascadeAnchor`）。⑤ Step 5 的 `OnStateEvent` **补 `const`**（片段住 Def、解析出来是 `const FTcsBuffDef*`）+ 订阅承接形态补明（**每兴趣 Tag 一条订阅 + 共享 Handler**，先例 `FTcsChainEventWaitRegistry`；**不**搞每实例 UObject）——**该步属 6b，当时未实施**（**同日稍后已落地**，见下方《Task 6 落地结果（6b 部分）》）。⑥ Step 2 的计划文件 `Private/Chain/TcsAttributeAccess.h/.cpp` 改为"**单载具查找点**（`UTcsAttributeSubsystem::Resolve`，用户 2026-10-05 拍板）+ **两模块各自白名单薄壳**"（TcsState 侧不动、TcsEffect 侧新增 `Private/Attribute/TcsEffectAttributeAccess.*`）。⑦ `FTcsTriggerContext` 需**补 `World`**（求值器签名拿不到世界），**且两个构造点都必须填**——本轮实测 `TcsStepBranch` 也构造该上下文，漏填会让链里的 `AttributeCompare` **静默恒不通过**（无红字、无日志）。
> - **验收读数**（PIE 全绿；双配置编译均 `Result: Succeeded`）：即时 **61/0**（新增 23a–23h）、延迟段收束 **68/0**、`.Reject` **11/0**；`Run` 区段红字**恰 3 条** = 装置头部既有清单（19f ×1 / 20j ×2）。关键读数：23a `Source.Id=115` / `CascadeAnchor.Id=116` **互异**；23b 自己的 `Applied` 起链一次（护甲 5.000→20.000）；23c 自己的 `Removed` **不起链**（计数 1→1、回退 20.000→10.000）；23d 登记表 **2→4→2**；23e 10.000→17.000；23f ①② 不变 / ③ 80.000→87.000；23g 同来源两实例增量 **25.000**、**移除只掉 10.000**；23h 两次运行 `RunSource` 互异且各自 = 实例 `Source`、触发行起链 `CausedBy.Id=116` = 主探针实例 `CascadeAnchor.Id`。证据 = `EVID-2026-10-05-state-chain-primitives`（双区段哈希 `2cfa09fe…f801f` / `7458a999…632f34`）。
> - **一处模型级发现（首轮实测，已入台账 `TRIG-6`）**：内联触发行是"**事件 Tag 级**"规则、**不过滤载荷与单位** ⇒ 一个实例登记的行会被世界里**任何**同 Tag 事件引爆（含别的实例的 `Applied`）；首轮 23g 因此得 30.000 / 计数 2（即时 `60/1`），修法 = **调整夹具施加顺序**（次定义先 ⇒ 串扰窗口为零），期望值一行未改。
> - **交付面收缩（两处）**：`TcsState/Public|Private/Chain/` 为**本轮新建目录**；Step 5 的 `TcsStateBehaviorFragment.h` 与 `TcsStateOps_Behavior.cpp` **未建**（6b 范围）。
> - **6b（`add-state-behavior-fragments`）当时未实施**：行为 Fragment 契约 / Def 载体 `Fragments` / 订阅表 + 共享 Handler / 两处接线 / 检查 23i–23l **全部待做**；其提案已过门禁并留在 `changes/` 里。（**2026-10-05 当日已实施并归档**——见下方《Task 6 落地结果（6b 部分）》。）

> **Task 6 落地结果（6b 部分，2026-10-05）**——提案 `add-state-behavior-fragments`（**1 新能力 + 3 改能力 / 4 个 delta**），当日实施 + 双配置编译 + 两连 PIE + 归档 `2026-10-05-add-state-behavior-fragments`：
> - **新增类型面（插件仓 5 新 + 9 改）**：`FTcsStateBehaviorFragment`（`USTRUCT(meta=(Hidden))`：`Interests: TArray<FGameplayTag>` + `virtual void OnStateEvent(const FGameplayTag&, const FInstancedStruct&, const FTcsStateBehaviorContext&) const` + **中性空实现**，并补 `virtual ~FTcsStateBehaviorFragment() = default;`——见下方第 5 条）· `FTcsStateBehaviorContext`（纯 C++ 值语义：`Subsystem` / `Handle` / `Unit` / `DefTag` / `Stacks` / `Level`，**不含实例指针**）· `FTcsStateBehaviorRegistry`（订阅表：`SetHandler` / `Reset(Bus)` / `AddInstance` / `RemoveInstance` / `CollectInterests` / `IsInterested` / 两个计数口；**每兴趣 Tag 一条订阅 + 计数配对**）· `UTcsStateBehaviorHandler`（`UTcsEventHandler` 派生、**弱引用**回指门面、强引用归门面 `UPROPERTY`）· `TcsStateOps_Behavior.cpp`（`WireBehaviors` / `UnwireBehaviors`，`Fragments` 空则**直接返回**——不建 Handler、不订阅、零开销）。
> - **改动面**：`FTcsBuffDef` + `Fragments: TArray<FInstancedStruct>`（裸载体 + 手写 `meta=(BaseStruct="/Script/TcsState.TcsStateBehaviorFragment")`）· `TcsBuffDefAsset::IsDataValid` + 两条作者期规则（空载荷 / 非派生类型 ⇒ Error，指名条目下标）· `UTcsStateSubsystem` + 注册表与 Handler 成员 + `BehaviorHandler` + `InRemovalBroadcastHandles` 旁表 · `TcsStateOps.cpp` / `TcsStateOps_Stack.cpp` / `TcsStateOps_Lifetime.cpp` 的**广播后按句柄重查** · `TcsStateSubsystem.cpp` 的 `Deinitialize` 全量退订（先重置行为注册表并打印 `1→0 / 1→0`）。
> - **两处接线时机（两侧都是硬约束）**：`Apply` 新实例路径的接线**排在 `Applied` 广播之前**（订阅者能在自己的 `Applied` 回调里跑行为）；移除 / 到期路径的退订**排在 `Removed` / `Expired` 广播之前**（"状态走了，行为先走"）；`RefreshStacked` 路径**不重订阅**。
> - **`TRIG-6` 甲案（本轮第二刀，用户拍板）**：补**首个状态载荷读取器**（`TcsStateEvents_ReadTriggerPayload` + `UE_DEFINE_TRIGGER_PAYLOAD_READER(FTcsStateEventPayload, …)`；注意 `TcsDamage` 早有读取器，故这是**首个状态**读取器而**不是**注册表的首个实现）+ **两侧泛型主体匹配**（载荷信息与触发行实例各持反射 `FInstancedStruct Subject`；状态行登记时绑定实例 `Handle`；两侧均有效时须精确 `UScriptStruct` 一致 + `CompareScriptStruct(A, B, 0)` 值相等；未绑定的全局行与外部无 `Subject` 事件**保留原 Tag 路由**）+ 载荷**新增反射字段 `Unit`**（`Caster` = 有效 `Instigator` 否则 `Unit`；原载荷无 `Unit` ⇒ `Caster` 只能取常无效的 `Instigator`）。⇒ **23g 恢复"先主后次"顺序**后仍为 `+25.000 / 计数增量 1`，即 6a 那条串扰语义**在不再依赖夹具顺序**的前提下被消除。
> - **身份修复（裁定 8 甲案，本轮第一刀）**：`FTcsStateRegistry` 出线**进程唯一**发号器（每次分配取**不复用的正奇数**、释放 `+1` 置偶数、**世界拆解不复位**、`int32` 正空间耗尽 `Fatal` 不回绕）——修掉"各桶各自从代际 1 起发 ⇒ 两单位首实例同为 `0/1`，而 `GetState` 扫全桶 ⇒ 命中错单位"的缺陷。**被否乙案**（句柄加 `Unit`）需迁移反射面与全部消费者；本轮 `Unit` 只加到**事件载荷**上。
> - **`STAT-5` 有限重入守卫（✅ 已消费）**：`UTcsStateSubsystem::InRemovalBroadcastHandles` **只**在**真正的移除广播窗口**成对标记 / 解除 ⇒ 递归 `Remove` 同句柄 `false` 静默、不重复广播与释放。**登记时设想的"按 `Phase == ESP_Expiring` 判拒"被否**——刷新路径（`TcsStateOps_Stack.cpp:235–249`）**也临时用 `Expiring`** ⇒ 按阶段判会**静默拒绝**合法的 `Refreshed` 自移除。**同时闭合六处"广播 / 副作用后仍读旧指针"**（`Apply` / `RefreshStacked` / `EPR_Immediate` / `PushPeriod` / `TcsStepApplyState` / `TcsStepModifyAttribute`）⇒ 统一"**副作用前取值快照、副作用后按句柄重查**"。
> - **一处学科性订正（任务文本）**：任务 0.1 与提案原文写"该注册表属主模块自登记读取器场景至今**零实现方**"——**事实错误**（`TcsDamage` 早已登记 `FTcsDamageFlowCollectEvent` 的读取器）⇒ 全文改为"首个**状态**读取器"。
> - **验收读数**（PIE 全绿；双配置编译均 `Result: Succeeded` 且**零 warning / 零 error**）：即时 **65/0**（新增 23i–23l）、延迟段收束 **72/0**、`.Reject` **11/0**；`Run` 区段红字**恰 3 条**（与 6a 相同，**本轮未新增任何故意红字**）。关键读数：23i 自身 `Applied` 一次 + `subs=4 / instances=1` + 读取器 `Caster=Instigator/退Unit` 且 `Subject=Handle`；23j 两定义两单位无串扰、`刷新A/B=1/0`、`订阅4→4不重挂`、空外部载荷扇出 `1/1`、重复兴趣去重 `subs4/instances2`，重入子读数 `Applied/Refreshed 自移除=成/成` 且 `旧句柄0/57拒绝`；23k 先退订后 `Removed`（`片段A/B=0/0`）+ 外部重入 `次数=1 / 返回=false` + 作者期两条规则各报 1 条 Error；23l **两连 PIE**（首轮 `0/0` 起、故意留哨兵 `0/59`；第二轮 `旧句柄拒绝=是`、`0/0` 起、新哨兵 `0/135`）+ 两次 `状态行为反初始化：订阅 1→0 实例 1→0`。证据 = `EVID-2026-10-05-state-behavior-fragments`（三段区段哈希 `9c8ade40…` / `8692f61f…` / `11324a20…`，冻结快照 `05218d83…`）。
> - **一处评审发现（未修，已入台账 `STAT-9`）**：终止广播窗口内对同 `Target` + 同 `DefTag` 重施，可能刷新尚未释放的旧实例、遗留修正器与时间条目 ⇒ **`STAT-5` 的有限修复 MUST NOT 被外推**为该组合已安全；复活 / 排队 / 拒绝三种策略留待真实内容触发后裁定。
> - **一处工程事实（首编两处 warning，已修）**：`FTcsStateBehaviorFragment` 引入虚函数 ⇒ C4265（有虚函数但析构非虚），补 `virtual ~…() = default;`；`FGCObject::AddReferencedObjects` 的裸 `UTcsStateSubsystem*` 成员在增量 GC 口径下收窄为 `TObjectPtr`。

---

## Task 7 — 端到端验收与证据

**目标**：一条竖切把 R5 全部面串起来，产出可复核证据与**如实边界清单**。

**交付物**：

- 宿主装置（**LAC 仓** `Source/TcsDev/`）：装置检查块 + 新命令 `Tcs.Test.State.Run`（零红字）/ `Tcs.Test.State.Reject`（拒绝面，自带屏显声明）。**实际落点由"`TcsDevSliceRig.cpp` 扩检查块"改为独立 TU**（`TcsDevSliceRig_State.cpp` + 公共面抽出 `TcsDevSliceRig_Internal.h`）——本体已 3500 行级，状态装置 803 行再摊进去会既超 `.cpp` 300 行约定、又让"屏显段键位游标"两套数字挤在一个文件里
- `Documents/combat-system-design/evidence/2026-10-05-state-layer-pie.md`（新，`EVID-2026-10-05-state-layer-pie`；**日期由 `2026-10-04` 顺延为 `2026-10-05`**——本轮实际收束日）
- 内容资产：**四枚**（计划写"一个 buff + 一条施加链"）——`DA_ModDef_E2E` 修正器模板 + `DA_State_E2E` buff（Finite 2.00s + Period 0.50s + 修正器行 + 内联触发行）+ `DA_Chain_E2E_Apply` 施加链 + `DA_Chain_E2E_Behavior` 行为链。**多出的两枚不是冗余**：修正器模板与行为链各自都需要**独立的 tag 身份载体**（前者是台账 `ATTR-1` 的闭合依据、后者要能被内联触发行按 `ChainId` 起链），计划把它们当成"buff 自带"的隐含物，实施期证明不成立

**提案**：`verify-state-layer-e2e`（宿主侧装置为主，插件侧零行为改动）

**依赖**：Task 6

- [x] **Step 1: 竖切脚本**——✅ 落地（**链路与计划文本有四处偏离，逐条见下**）：`ApplyState(buff)` → 快照冻结（`Overrides` 覆盖一项、等级源驱动一项）→ 修正器物化 → `TcsEvent.State.Applied` → buff 自带触发行起行为链 → 行为链 `ModifyAttribute` 写属性 → 周期 `Periodic` → 到期 `Expired` → 属性归位 + 触发行级联退订 + 实例回收。**偏离**：① ~~"灼烧类 buff"~~ ——本轮是**机制验收**，属性取 `Armor`（状态修正器，会回退）/ `Attack`（行为链，常驻）两个**互为对照**的通道，不引入火抗/灼烧这类**题材语义**；② ~~"减火抗 `X → Y`"~~ ——修正器是 `TAO_Add` + 覆盖值 25.0，`Armor 5.000→30.000`；③ **行为链落点 = `Attribute.Attack`（不是被施加的属性本身）**——见裁定 D10 与下方落地结果块；④ **周期面判据写成 `≥1` 而非定值**：`DurationTime` 2.00s / `Period` 0.50s，而 S11 在 **0.60s 那一拍**采样 ⇒ 此刻刚好只落了 **1** 次周期（实测 `PeriodicDelta=1`）；到 S12（2.60s）时累计已 **3** 次（`0.65s / 1.31s / 1.98s` 三拍，随后到期）。**同一物理量在两个采样点读数不同 ⇒ 判据 MUST 是下界**（装置源码即 `PeriodicDelta >= 1`），写等式会随帧率与采样时刻假失败。
- [x] **Step 2: 观测面（双证据）**——✅ 落地（**一处收缩 + 一处订正**）：① 属性面 = `EvaluateCurrent` ✅（Armor / Attack 双读数）；~~`PeekPending`~~ **未用**——该口属 `UTcsAttributeSubsystem` 的待结算查询（`TcsAttributeSubsystem.cpp:255`），而本轮竖切全程**即时结算**（无待结算窗口）⇒ 用它会读到恒空，属**无对象读数**，故不写。② 事件面 = **四个槽**（`Observer->StateEventCounts` 的 `Applied`/`Removed`/`Expired`/`Periodic`，索引 0/2/3/4），**不是"六个状态事件的广播计数"**——观测者虽订阅六枚（`TcsDevBootstrap.cpp:345-359`），但本轮竖切的可判读数只有这四枚；`Refreshed` 在本路径**不发生**（非叠层刷新路径）、`StackChanged` 属 Task 5 面。~~`FTcsAttributeChangedEvent` 计数~~ **未用**——属性面已有 `EvaluateCurrent` 前后读数，再加事件计数是**同因双证**（同一物化写点），而**定序**这一本来需要它的判据本轮**零读数**（见落地结果块"未覆盖"条）。③ **可复现性由"每轮读数逐字一致"收缩为"复现性摘要行逐字一致 + 各检查结论相同"**——见落地结果块（进程内单调量与帧驱动量按构造不可复现）。
- [x] **Step 3: 拒绝面独立命令**——✅ 落地，七条（a–g）**与计划的五条不是同一份清单**：计划写的"不可解析 `DefTag` / 无效目标 / 脏句柄 / 库未就绪 / `ExtendDuration` 作用于 `Infinite`"中，**三条落地、两条被替换**。落地 = ①不可解析/未登记 `DefTag`（检查 a）②无效目标句柄（b）③"脏句柄"实为**悬空句柄访问**——由"真施加→真移除→旧句柄再访问"三面构成，比"脏句柄"更可判（c）④~~库未就绪~~ **未做独立拒绝检查**——定义库未就绪在装置里表现为**夹具直接失败退出**（`TcsDevSliceRig_State.cpp` 内的前置守卫），不是一条可断言的"操作被拒"路径 ⇒ 该条**降级为前置守卫**、不占检查位 ⑤`ExtendDuration` 作用于 `Infinite`（f，同条并测 `SetRemaining`）。**新增三条**：⑥`RegisterStateDef` 无效身份 / 重复登记 ⇒ **Error**（e，级别本身即被测语义）⑦未登记 `ChainId` 起链（g）⑧未登记身份**查询**落 `nullptr` 且**零红字**（d，negative control——证明前六条的红字来自"拒绝"而非"任何异常都红"）。全批实测**恰 6 Warning + 3 Error**。约定守住：**全部红字集中在此命令**，`Tcs.Test.State.Run` 区段红字 = **0**。
- [x] **Step 4: 边界清单（如实记）**——✅ 落地，证据 §6 共 **20 条**（计划预声明的 5 条里 **3 条落、2 条未落**）：① 单机单世界单 PIE 进程 ✅（§6.1，并明确含"Shipping 只验编译、无运行期"）② `StateLevel*` 两型 ~~只测两型~~ **实际只测数组型 `StateLevelArray`**、`StateLevelMap` **零读数**；`InstigatorLevel*` 两型本轮未被内容资产消费（§6.2，判据比计划更严）③ ~~关系表字段零消费者~~ **未记入本轮 §6**——该边界属 Task 5 面（叠层/五轴），本竖切不触达；如实说明 = 本条**不在本轮证据的覆盖声明范围内**，见下方落地结果块"五条计划预期的对账"④ ~~`Cues` / `EventPayloadFilter` / `InterruptPriority` 仍留位~~ **未记入本轮 §6**——同理属状态 Def 的**字段留位**边界（Task 1 面），本轮内容资产未配这些字段；⑤ **跨 PIE 残留检查 ✅**（§1.7 + S1：停止 PIE 后无残留、重进起点归零；与本条**同源**的是 S1 而非计划设想的独立检查）。**逐条判定小结 = 3 条落（①⑤ 全落、② 落但收窄为一型）、2 条未落（③④）**。**另有 15 条计划未预声明的边界**（含"`ResolveAttrModDef` 运行期调用者为零"、"`FTcsStepModifyAttribute` 不等同 GAS Instant"、"链挂条目按设计常驻"）——**边界清单的价值恰恰在这些实施期才显形的条目上**。
- [x] **Step 5: 复核**——✅ 落地：冻结快照 4,824 行 / 666,087 字节 / SHA-256 `db630575…`（**先冻结再切区段**，因日志跨五个 PIE 世界且随编辑器重启轮转）；五个区段各有行号 / 行数 / 字节 / SHA-256（`f4c37829…` / `1deb56be…` / `23887a8f…` / `3d7a890e…` / `c0f4ef36…`）；§1 三类读数**逐条对应日志一行**（42 处 `L####` 引用**全部按内容重定位**，不是按偏移量推算——见落地结果块"一处本文件自身缺陷"）；§3 七个复算脚本逐字可跑且**跑过**。

**验收信号**：`Tcs.Test.State.Run` **两轮各 15/0**（全绿）且区段**零红字**；`.Reject` **7/0** 且红字恰为装置头部枚举的那一批（6 Warning + 3 Error）；证据文档 §6 的 20 条边界**逐条有实测支撑**，无一条以推断冒充。

**非目标**：不做竞技级压测；不做 Mass 路径；不做网络复制。

> **Task 7 落地结果（2026-10-05）**：Step 1–5 **全勾**；提案 `verify-state-layer-e2e`（**1 新能力 + 3 改能力**：`state-layer-e2e-validation` ADDED ×4、`gameplay-tag-governance` MODIFIED ×3、`integration-entity` MODIFIED ×1、`attribute-types` ADDED ×1）**待归档**（§8 Step 1）；`openspec validate verify-state-layer-e2e --strict --no-interactive` = **valid**（exit 0）、`openspec list` = **54/58 tasks**（余 §8）。
> - **验收读数（全部取自交付二进制）**：取证口径 = **先关编辑器 → 完整双配置 UBT 构建**（Editor `7.96s` / Shipping `28.16s`，各 `Result: Succeeded`、零 warning 零 error）**→ 重开编辑器 → 重跑全部验收**。理由见下方"陈旧 DLL"条。结果 = `Tcs.Test.State.Run` **两轮各 15/0**（区段零红字、零 `[FAIL]`、复现性摘要行 `-ceq` 逐字相同）+ `Tcs.Test.State.Reject` **7/0**（红字恰 **6 Warning + 3 Error**）+ §5 回归**逐字不变**（`Tcs.Test.Slice.Run` 即时 **65/0** + 延迟 **72/0**、`Tcs.Test.Slice.Reject` **11/0**）；两处**预声明**的分母移动如期出现（检查 18 `IsDataValid == Valid 2/2 个`、检查 19a 门面内 `2 条`）。
> - **15 条检查的实测标题（逐字取自日志，勿按计划文本转述）**：S1 跨 PIE 无残留 · S2 内容身份解析 · S3 `ResolveAttrModDef` 身份一致（同一指针）· S4 属性账本就绪 · S5 内容链施加建实例 · S6 内联触发行已随实例登记 · S7 快照三键 · S8 真资产修正器物化 · S9 内联触发行起了行为链 · S10 **对照单位零波及** · S11 在飞状态与周期回调 · S12 到期回收 · S13 内联触发行按锚点退订 · S14 状态修正器按锚点摘净 / Armor 复原 · S15 行为链条目按设计常驻。**S10 是"对照组未被波及"**（`ControlUnit` 在册状态 = 0）——它证的是**读数不是"世界上任何东西变了"的假阳性**，与 S4 的"账本就绪"是两件事。
> - **本 Task 最值钱的自家读数 = S7 的"三键两两可分"**：覆盖键 `25.000` / 定义字面量 `10.000` / 兜底 `-999.000` 三值两两不同，**外加一个负对照键**（未进 `Overrides` ⇒ 实测保定义值 `10.000`）⇒ 同时证明"跨资产 `ParamRef` 通道真的通了"与"覆盖是**按 `Row.Key` 逐键命中**、不是整表替换"。等级键实测 `30.000`（装置把施加者等级设为 2 ⇒ 若该源读 `LevelProvider` 会得 `60.000`）⇒ 顺带钉死"`StateLevelArray` 只读 `Context.EffectiveLevel`"。
> - **一处裁定（D10，用户拍板甲案）——行为链落 `Attribute.Attack`**：状态修正器目标 `Attribute.Armor` 挂 `Instance.CascadeAnchor`（`TcsStateModifierMaterializer.cpp:108`）⇒ 到期只摘它、**护甲精确复原 30.000→5.000**（S14）；行为链目标 `Attribute.Attack` 挂 `Context.RunSource`（`TcsStepModifyAttribute.cpp:80`，每次起链新发的运行态句柄）⇒ **攻击停在 25.000**（S15）。两条流水各自 `RemoveBySource` 精确匹配、互不误摘（S8 / S15 互为对照）。**否决乙案**（把链改到"会被状态到期一起回退"的属性上）：那会让验收变成**对实现的追认**——`state-layer-e2e-validation` 的既有场景**逐字不改**即已满足，改落点等于改契约去迁就实现。
> - **台账：`ATTR-1` ✅ 闭合（59 → 61）**——它是**"触发条件型"条目第一次真的被触发**（Task 7 要交付真内容资产修正器模板 ⇒ 资产身份必须有 tag 载体 ⇒ 原判"零解析消费者 ⇒ 不开根"的前提消失），就地标结沿用 Task 0 先例。**新增两条**（用户同轮拍板）：**`CHAIN-7`**（链挂的账本条目没有框架侧回收触发点——**同族 `CHAIN-6`**；触发条件 = 出现"链改的属性必须随某生命周期回退"的真实内容需求）与 **`CHAIN-8`**（链原语缺"直写基础值"，即 GAS Instant 等价物；**MUST NOT 并入 `DAMAGE-2`**，含三个待决问题：①绝对赋值 vs 增量 ②是否需要可回收性（与 `CHAIN-7` 交叉）③与 D7-6 的边界，见 `TcsFlowStepsCore.cpp:181`）。
> - **三条回收路径实测全是死的**（`CHAIN-7` 的实测依据）：① **运行结束**——全即时链的运行态在 `ExecuteChain` 返回前**已释放**（`TcsEffectSubsystem_Run.cpp:104-105`）⇒ 无"随运行结束回收"的窗口；② **`CausedBy` 级联**——**已被裁定③明文禁止**；③ **框架自动回收**——`FTcsStepModifyAttribute` **刻意不提供**回收器。⇒ 今日唯一合法用法 = **宿主自存 `RunSource` 再自调 `RemoveBySource`**（该口公开且有真实使用者 `TcsDamageSubsystem.cpp:226`，但**链原语路径零调用**）。另记一条同族事实：**`FTcsStepModifyAttribute` 不等同 GAS Instant**——它写 `ModifierSlots` 而**不写 `BaseValue`**，会被任一 `TAO_Override` **整体抹掉**（`TcsAttributeBandFold.h:161-164`：`bHasOverride` 直接 return，全部 `Add` 被丢弃）、参与 `PercentAdd`/`Mul` 复利、且可按来源摘除。
> - **一处 API 的如实边界**：`ResolveAttrModDef` **运行期调用者为零**——本变更交付的是**身份解析与索引**（外加 S3 的"同一指针"读数），**不交付**"按 tag 取模板供物化"；物化点今天读的是状态实例自己的 `ModifierRows`。同理 `AttrModDef` 根下**只有 `Check` 形态一个词** ⇒ 代表词覆盖极薄。两条均记入证据 §6 与台账 `ATTR-1` 行内，**未单立台账条目**。
> - **四处实施期缺陷当场修（证据 §4 逐条留痕）**：① **`DA_ModDef_E2E` 的 `ParamRef.Key` 是探索期笔误**（写成 `Attribute.Armor`，而快照键全在 `TcsStateParam.Check.*`）⇒ `Overrides.Find(Row.Key)` 落空 ⇒ 静默取兜底 `-999.000` ⇒ 护甲读成 **−994.000**；修 Key 后 `5.000→30.000` 净变 `25.000`。**这条同时沉淀一条资产制作事实**：`FTcsParamValue.Source` 是**构造期已初始化**的 `FInstancedStruct`，`{"_structType":…}` 对象形式被**静默忽略**（`JsonObjectConverter.cpp:910-934` 只在 `!IsValid()` 时认它）而 `set_properties` **仍返回 `true`** ⇒ MUST 用 **UE 文本格式字符串**（走 `HasImportTextItem()` → `InitializeAs`，`JsonObjectConverter.cpp:1054`）。② **事件面基线取晚了**——四个槽原在 `ExecuteChain` **之后**才取，而施加路径**全程同步**（`ApplyState` 内即广播 → 触发行求值 → 行为链再入起链）⇒ `Applied` 增量**恒 0**（S11 首轮 `0 期望 1` 失败）；**线索是 S9 已显示 `Attack 30.000→25.000`**（行为链确实起过）——自相矛盾的读数对。修法 = **基线一律提前到起链之前取**。③ **陈旧 DLL**：S8/S14 消息里残留落点改 `Attack` 之前的旧算术，**该缺陷只在重建二进制后才消失** ⇒ **"现象只在重建后消失"本身就是陈旧 DLL 的判据**（本轮最大单项时间开销）；Live Coding 能给"编译通过"的假象而**不更新交付二进制**，故取证期 MUST NOT 用它。④ **证据文档自身的表格整体偏移一行**（§1.1 / §1.2 的"日志行"列每个检查都指向**它的前一行**）——起因 = 从**作废快照按区段端点做算术偏移**重映射行号，而两份快照的区段基址不同；修法 = **全部按内容正则重定位**（`检查 S\d+` / `检查 [a-g]`），42 处引用逐条复核。**教训：跨快照的行号 MUST 按内容重定位、MUST NOT 按偏移量推算。**
> - **两处判据订正（本 Task 最值钱的副产物）**：① **可复现性判据 MUST NOT 落在区段字节上**——两轮 `State.Run` 区段 **85 vs 84 行 / 13,726 vs 13,620 字节**，逐行归一化求差集得**唯一**差异 = `FTcsEffectStepExecutorRegistry: 步骤执行器登记表已解析（共 10 类）`**只在第一轮**出现（L2674）；真因 = **进程内一次性惰性初始化**（该表在本进程第一次真的执行链步骤时解析并打一行日志）⇒ 与 `FTcsSourceHandle.Id`（**进程内单调计数器**，实测 `3/4` → `19/20`）同族，都**按构造不可复现**。判据改为 **"复现性摘要行逐字相同 + 各检查结论相同"**，摘要只留**不变量**（`Decoupled=1` / `PerOK=1`）。**掩蔽坑**：宽 pattern `步骤执行器登记表已解析` 会**同时命中 Damage 侧**的 `FTcsFlowStepExecutorRegistry: …（共 14 类）`（L3387）⇒ MUST 带**模块前缀**。② **首轮/次轮不等长本身不是缺陷**——凡"首次执行才出现"的初始化日志都属进程生命周期痕迹。
> - **一条工具面边界突破（供后续轮）**：全库 **830** 个 MCP 工具中**无任何 `IsDataValid` 入口**、ProgrammaticToolset 沙箱**禁 `import unreal`**、`Object::IsDataValid` **未暴露给 Python**（`hasattr` 实测 `False`）⇒ 逐资产作者侧校验**唯一**可达通道 = `unreal.EditorValidatorSubsystem.validate_assets_with_settings`（引擎源码 `EditorValidatorSubsystem.cpp:657` **无条件首调 `IsDataValid`**，与装置检查 18 **同一虚函数**）；要点 = 需 `load_assets_for_validation=True`、**一次只验一个资产**否则无法归因。四枚资产各得 `num_checked=1 / num_valid=1 / num_invalid=0 / num_warnings=0`。
> - **两条装置驱动通道事实**：① **Slate 控件 ref 在编辑器重启后全部重新分配**（上一进程可用的底栏输入框引用 `tb5` → 本进程 `tb3`），失效表现为 `FillForm` 返回 `{"returnValue":false}` ⇒ 探测法 = 填 `py print('[TAG] …')` 后查日志**同时**出现 `Cmd:` 与 `LogPython` 输出；② **`Saved/Logs/LegendAutoChess.log` 随编辑器重启轮转**（不跨重启追加）⇒ 证据 MUST 在**同一编辑器会话内**取齐。
> - **一处交付物面扩张（已记入 §交付物）**：装置本体由"`TcsDevSliceRig.cpp` 扩检查块"改为**独立 TU** `TcsDevSliceRig_State.cpp`（803 行）+ 公共面抽出 `TcsDevSliceRig_Internal.h`（181 行）；本体随之 `-139 / +20` 行（**纯搬迁、语义零变更**）。理由 = 本体已 3500 行级，状态装置再摊进去既超 `.cpp` 300 行约定、又会让两套屏显段键位游标挤在一个文件里。**屏显段键基址 = `0x2000`**（`GTcsDevStateScreenBase`，Head/Main/Tail/Summary 四段自 `0x2000` 起算，与既有段游标错开）。
> - **五条计划预期的对账（诚实说明"哪条没落"）**：Step 4 预声明的 5 条边界里 **3 条落、2 条未落**——**未落的是"关系表字段零消费者"与"`Cues`/`EventPayloadFilter`/`InterruptPriority` 仍留位"**（两条，见 Step 4 内文）：它们属 Task 5（叠层/五轴）与 Task 1（Def 字段留位）的面，本竖切**不触达** ⇒ 如实记为"不在本轮证据的覆盖声明范围内"，**未**在 §6 里补一条充数。另有 Step 2 的 `PeekPending` 与 `FTcsAttributeChangedEvent` 未用（理由见 Step 2 内文）。
> - **本 Task 的验收边界（二十条）**见证据 §6。其余与计划 Step 4 预声明项一致者：单机单世界单 PIE 进程（含"Shipping 只验编译"）、`StateLevel*` 只测**数组型**、跨 PIE 残留检查（S1 + §1.7）。
> - **一条已闭合的散落真相（留作纪律，非待办）**：`Config/DefaultGameplayTags.ini` 的 `EffectChain.Check.Behavior` 注释原写"ModifyAttribute **扣护甲** 5"，与 D10 落点（`Attack`）**不符**——**已在提交 `0a4df71` 前修正为"扣 `Attack` 5，落点见 design.md D10"**，tag **路径未变** ⇒ 不影响任何已记录读数（冻结快照内 `DevComment` / `扣护甲` 命中数均为 **0**；证据亦未记录该 ini 的字节数或哈希）。**留此条是为记纪律**：tag 注释是**散落的第二真相**，裁定改落点时必须同步扫一遍 `Config/DefaultGameplayTags.ini`——本轮是靠"提交前复核即将永久入库的每一行"抓到的，不是靠任何自动门禁。

---

## Task 8 — 收束

**目标**：把本轮从"代码绿"收敛为"文档/台账/规格/记忆四处一致"。

**交付物**：本计划全文回写 + 五处文档同步 + 提案归档 + 轮次检查点卡。

- [ ] **Step 1: 设计文档回写**——`[`SPEC-02-states`](../spec/03-module-states.md)`：身份块状态、§2 资产名、§3.1 宿主类名、§3.3 tag 名、§11 验收钩子改为"已验 / 未验"对账表、新增"R5 落地状态"节（收窄轮已落 §12 的全部基础，本步只做落地态回填）。
- [ ] **Step 1b: 相邻规格**——`[`SPEC-01-attributes`](../spec/02-module-attributes.md)` 的一致性待办行勾销；`[`SPEC-03-effects`](../spec/04-module-effects.md)` §2.1 原语实现状态（15 已落 **10**：+`ApplyState` / `ModifyAttribute`）；`[`SPEC-08-damage`](../spec/09-module-damage.md)` **不动**（本轮不涉）。
- [ ] **Step 2: `SPEC-TRACE` 增量**——新增行（状态 Def/实例/快照/等级源/堆叠/物化/`ApplyState`/`ModifyAttribute`/内联触发行/行为 Fragment）+ 改判既有行 + **证据边界注记**（含全部非外推边界）。
- [ ] **Step 3: 台账勾销与新增**——`STAT-2` / `DAMAGE-5` / `WAIT-6` / `TRIG-4` / `TRIG-5`（`AttributeCompare` 一支）/ `DAMAGE-2`（`ModifyAttribute` 一支）标已消费；`DAMAGE-2` 余 3 / `DAMAGE-3` 余 / `DAMAGE-4` / `STAT-3` 的**归属改判为 R5.5**；新增实施期发现；**计数与行数对账**（改了几条就改计数，三处：台账身份块 + `INDEX` §4.5 + `INDEX` §2 行）。
  **Task 7 结转的四条待对账（已在各自落点就地登记，本步只需复核，勿重复改）**：① `INDEX` §4.4 的 **R7 行仍列已消费的 `WAIT-6`**（该条 Task 0 Step 3 已标结）——由本 Task Step 5 一并纠正；② `LEDGER-reflection` 的 **`R-1` 状态句需重写**（其"未消费"的措辞与 Task 3 Step 9 的落地态不符）；③ 拒绝面检查 `I` 的**行内文字写"预期 2 条 Warning"而实测 1 条**——**非锚检查、已登记但本轮不修**（属装置文案口径，非语义）；④ **tag 形态治理漂移**——`StateDef.Probe.*` / `TcsStateParam.Probe.*` / `EffectChain.Probe.*` / `TcsEvent.Probe.Wiring` 把 `Probe` 放在**第 2 段**，而 `unreal-gameplay-tags` 规则要求"`Probe` MUST 出现在第 1 段" ⇒ **建议开一轮治理专项入台账，MUST NOT 在本 Task 内改**（改名会牵动既有装置与资产，属跨轮动作）。
- [ ] **Step 4: 两册日志**——`LOG-IMPLEMENTATION` 补 Task 0~7 的实施与验收记录；`LOG-DECISIONS` 记 §0.2 的六条口径裁决 + 实施期偏离。
- [ ] **Step 5: `INDEX` 同步**——§1 现状一句话（R5 已完成 → 下一步 R6）、§4.3 计划表加 `PLN-R5` 行、§4.4 路线图（**顺手纠错**：现表把 `STAT-1` 记成 R5 消费，台账里它是 R6 的）、§4.5 台账计数、最后更新。
- [ ] **Step 6: 提案归档与门禁**——`openspec archive <id> --yes` 逐个归档（MODIFIED 标题逐字一致）；`openspec validate --all --strict --no-interactive` 全绿 + `openspec/changes/` **零活动提案**；`openspec list` 复核。
- [ ] **Step 7: 轮次检查点卡**——按 `harness-retro` 模板，走用户确认门；本计划标完成（`ACTIVE` → 视 R5.5 是否继续承载而保持 `ACTIVE`，同 `PLN-R4` 先例）。

**验收信号**：门禁全绿、台账计数 = 行数、四处文档一致、检查点卡入库。

---

## 台账消费与新增汇总

| 条目 | 处置 | 落点 |
|---|---|---|
| `STAT-2` | ✅ 消费 | Task 0 Step 2 |
| `DAMAGE-5` | ✅ 消费（+1 字段增量：`LevelProvider`） | Task 0 Step 1 / Task 3 Step 4 |
| `WAIT-6` | ✅ 消费（**归属由 M6 提前至 R5**） | Task 0 Step 3 |
| `TRIG-4` | ✅ 消费 | Task 1 Step 2 + Task 6 Step 4 |
| `TRIG-5`（仅 `AttributeCompare`） | 部分消费 | Task 6 Step 3 |
| `DAMAGE-2`（仅 `ModifyAttribute`） | 部分消费 | Task 6 Step 2 |
| `DAMAGE-2` 余 3 / `DAMAGE-3` 余 / `DAMAGE-4` / `STAT-3` | 改判归属 → **R5.5** | 见 §轮次路线图《R5.5 批次表》 |
| **R4.5-b**（`R-1` 参数源族宿主插槽） | ✅ 消费（**同批落地**：2026-10-04 排定并入 Task 3） | Task 3 Step 9 |
| **新增** `SCRIPT-9` | 属性写入面缺反射（`ApplyModifier` / `RemoveBySource` 无 `UFUNCTION`） | 触发条件型（R5.5-h） |
| **新增** `R-7`（`LEDGER-reflection`） | 属性访问**写侧**宿主插槽（`ITcsAttributeAccess`） | 触发条件型（R5.5-g） |
| `STAT-5` | **✅ 消费（6b 落地时就地标）** | Task 6（6b）：`InRemovalBroadcastHandles` 有限重入守卫 + 六处广播后重查 |
| `TRIG-6` | **✅ 落地（6b 落地时就地标，采用候选修法 ①）** | Task 6（6b）：首个状态载荷读取器 + 两侧泛型 `Subject` 精确匹配 |
| **新增** `STAT-9` | 终止广播内同目标同定义重施（**静态路径风险，未修**） | Task 6（6b）代码评审；触发条件型 |
| `ATTR-1` | **✅ 已闭合（触发条件型条目第一次真的被触发）** | **Task 7**：交付真内容资产修正器模板 ⇒ 资产身份必须有 tag 载体 ⇒ 开 `AttrModDef` 根（根表 10 → 11） |
| **新增** `CHAIN-7` | 链挂的账本条目**没有框架侧回收触发点**（与 `CHAIN-6` 同族；三条回收路径实测全堵） | **Task 7**；触发条件 = 出现"链改的属性必须随某生命周期回退"的真实内容需求 |
| **新增** `CHAIN-8` | 链原语缺**"直写基础值"**（GAS Instant 等价物；**MUST NOT 并入 `DAMAGE-2`**） | **Task 7**；触发条件型 |
| **新增**（实施期） | 本轮实施中发现的边界，逐条登记 | Task 8 Step 3 |

> **Task 7 收束（2026-10-05）就地结一行 + 新增两行**：`ATTR-1`（**✅ 已闭合**——触发条件型条目首次触发；本轮要交付真内容资产修正器模板，而资产身份必须有 tag 载体，原判"零解析消费者 ⇒ 不开根"的前提随之消失）、**`CHAIN-7`**（**新增**——链挂的账本条目没有框架侧回收触发点；三条候选回收路径**实测全堵**：全即时链运行态在 `ExecuteChain` 返回前已释放 / `CausedBy` 级联被裁定③禁止 / `FTcsStepModifyAttribute` 刻意无回收器）、**`CHAIN-8`**（**新增**——链原语缺"直写基础值"，**不并入 `DAMAGE-2`**）。**与"行级状态收束时才标"的关系**：`ATTR-1` 的**收束点就是 Task 7 自身**（触发条件被本 Task 满足，同 Task 0 判据）；两条 `CHAIN-*` 是**实施期新发现**，**已在 Task 7 当晚就地登记进台账**（`CHAIN` 区段末两行，含实测依据与三条死路），故 Task 8 Step 3 那一趟**只需复核、不需补登**。**计数**：59 → 61，台账身份块 / `INDEX` §4.5 / `INDEX` §2 行三处已同步。

> **Task 6（6b）收束（2026-10-05）就地标两行 + 新增一行**：`STAT-5`（**已消费**——守卫落旁表；登记时设想的"按 `Expiring` 阶段判拒"被否）、`TRIG-6`（**已落地**——读取器 + 主体匹配；23g 恢复"先主后次"仍 `+25 / 计数 1`）、`STAT-9`（**新增**——终止广播内同定义重施，未修）。**与"行级状态收束时才标"的关系**：这两条的**收束点就是 Task 6 自身**（同 Task 0 的判据），故不就待到 Task 8 Step 3。

> **Task 0 收束（2026-10-04）就地在台账标记三条**：`STAT-2`（**部分消费**——基类 + `GetIndexForLevel` 落地；PV-10 原案的 `Enumerate` 与"表项总数"均零消费者不预建）、`DAMAGE-5`（消费——两字段 + 头注释口径）、`WAIT-6`（消费——出线发号 + 导出宏）。**与本册"行级状态收束时才标"的关系**：这三条的**收束点就是 Task 0 自身**（已落地即已消费），故不就待到 Task 8 Step 3；Task 8 那一趟只处理剩余行与"实施期新发现"。
>
> **一处判据留痕**：Task 0 的提案面比计划少一个——`unify-source-handle-issuer` 未开（`instance-handle-pool` 规格第 17 行**早已**要求"进程内永不复用"，Step 3 是还债而非改需求 ⇒ 按 `openspec/AGENTS.md` 决策树免提案）。

---

## 非目标（本轮不做）

- **不做**关系表检查器 / 移除级联重评 / 槽位竞争（语义未定稿，R5.5-e）。
- **不做** `Heal` / `Parallel` / `Repeat` / `OnError` 四个原语（R5.5-a/b）。
- **不做** `DAMAGE-4` 消费动作与候选裁决（R5.5-c）。
- **不做** `STAT-3` 流程模板重定向栈（R5.5-d）。
- **不做**技能参数行分派、参数链聚合（R6）。
- **不做** `UCombatWorldRegistrySubsystem`（实体状态机 + 泵接线 + 就绪门禁）——R7/M6。
- **不做**就绪状态机完整版、`PrimaryAssetTypesToScan`、异步加载三策略（R7 `INTEG-3`）。
- **不做**网络操作复制（`WAIT-3`）、Mass 适配（`WAIT-2`）、回合本体论。
- **不做**描述渲染 / tooltip（R8）；不做 Explain 面板（R8 `TOOLS-6`）。
- **不做**`ApplyState` / `RemoveState` 的 `UFUNCTION` 反射面（脚本面整体留 `SCRIPT` 系列）。

---

## 风险与弹性

| 风险 | 判断 | 处置 |
|---|---|---|
| R5 体量仍偏大（8 个 Task） | 中——比 R4 多一个 Task，但切分沿"验收信号" | Task 3 / 4 之间是**天然断点**（有了数值可观测即可先验收一次）；必要时 Task 5 可后置到 R5.5 |
| `FTcsStateDefBase` 形状与 R6 `FSkillDef` 继承冲突 | 中——`FSkillDef` 要"继承白拿"参数行与描述 | Task 1 Step 2 的字段划分 MUST 按"无时值/无堆叠=基类、有时值/有堆叠=FBuffDef"守死；R6 开工时先读本条 |
| 等级源 `InstigatorLevel*` 缺宿主 provider | 高——今天无人实现 | Task 3 Step 4 的接口一次定对 + Task 7 边界清单如实标"未覆盖"（若无宿主实现） |
| 发号器统一影响既有装置断言 | 低——Id 值会变但语义不变 | Task 0 Step 4 用 `Tcs.Test.Slice.Run` 全文对比作回归证据 |
| 物化器遇 `TSoftObjectPtr` 同步加载卡顿 | 低——资产规模个位数 | 记入 Task 4 注记；异步化归 R7 |

---

## 变更记录

- 2026-10-04 建立：R5 开工收窄轮产出——台账折入（§0.1）、六条口径裁决（§0.2）、现状证据（§0.3）、两条错前提更正（§0.4）；Task 0~8 切分；《R5.5 批次表》落纸（防"非正式轮号只住会话里"重演）；《轮次路线图》自 `PLN-R4` 移交至本文档。
- 2026-10-04 Task 0 收束（前置契约）：Step 1 / 2 / 3 落地并勾选；Step 4 的**两半均已通过**（双配置 Succeeded、零 warning；PIE 回归 **19/0 + 7b**、零红字，证据 `EVID-2026-10-04-param-context-and-source-issuer`）。
  - **两处与计划文本的偏离记录在案**：① Step 3 的修法由"`static` 成员"改为"出线 + `TCSCORE_API`"（理由 = 跨 DLL 的 COMDAT 不合并，字面改会把缺陷修成配置相关的假修复——详见该 Step 内文）；② **提案面收缩为一个**（`extend-param-evaluate-context`）：Step 3 属"恢复 `instance-handle-pool` 规格既有行为"的缺陷修复，按 `openspec/AGENTS.md` 决策树**免提案**，故计划的"两个提案"落地为一个，Step 3 的过程留痕在本计划 + 台账 + 两册日志。§0.3 现状证据表同步加"已存在"注记（防再被当现行事实读）。
- 2026-10-04 Task 0 **提交前修正**（用户索要"可枚举基类的实际业务场景"时暴露）：Step 2 里我自造的 `GetIndexCount()` **已删**——理由两条：① **全库无设计出处**（PV-10 各处只写 `Enumerate` + `GetIndexForLevel`）；② 该能力的真实消费者是展示层的 `FTcsParamView_Series`（整表 + 当前档高亮），它要的是**整表数值**（`FTcsViewBuildContext::TryGetSeries`），"表项总数"渲染不出任何一档 ⇒ 无消费者。基类现只留 `GetIndexForLevel`（PV-10 原文有、Task 3 四源会覆写）。
  连带同步：头文件 / openspec 现行规格 / 归档提案与其 delta / 本计划三处 / 台账 `STAT-2` 注记 / 两册日志 / EVID 边界，随后重编双配置。
- 2026-10-04 **Task 1 收束**（模块与 Def 资产族）：Step 1~7 **全勾**；提案 `add-tcs-state-module` **四份 delta** 归档（`+5 added / ~3 modified / →1 renamed`），`openspec validate --all --strict` = **27 passed / 0 failed**、`changes/` 零活动提案；验收 = 双配置编译零 warning / 零 error + PIE **20/0 + 7b** 零红字（**检查 18** 四项判据全过），证据 `EVID-2026-10-04-tcs-state-def-asset`。
  - **四处与计划文本的差异（逐条留痕）**：① **提案 delta 从一份变四份**——除 `plugin-descriptor`（MUST MODIFY）外补 `integration-entity` MODIFIED（Step 5 一落地，"两条按类发现路径"这句事实即失效）、`state-def-asset` ADDED（新 Def 族的形状/身份/校验需要规格落点）、`gameplay-tag-governance` MODIFIED（新增 `StateDef` 根，见 ③）；② **描述载体随本 Task 落**（用户裁定甲案）——`FTcsDescriptionEntry` / `FTcsDescriptionViewSlot` 的类型全库不存在，不补则编译不过；③ **新增 `StateDef` tag 根（9 → 10）**（用户裁定甲案）——Step 7 要真资产就必须有身份词，而根表无任何根承载"状态定义资产身份解析"；`Def.StatusTag` 的根归属**不裁定**（零消费者）挂台账 `STAT-4` 待 R5.5-e；④ **两处类型提前落**——`EDurationPolicy` / `ETcsPeriodRefresh`（`Public/State/TcsStateEnums.h`）与 `FStateStackPolicy` 形状（`Public/State/TcsStateStackPolicy.h`）随本 Task 落，Task 2 / 5 只在其上续写（`SPEC-02-states` §12.5）。
  - **一条引擎/UBT 事实（本轮实测）**：直接使用某模块类型者 MUST 在自己的 `Build.cs` 声明依赖——**传递依赖只给头文件路径、不给导入库**（`TcsDev` 靠 `TcsIntegration` 的 public 依赖能 include `Def/TcsBuffDefAsset.h`，但链接期取不到 `Z_Construct_UClass_UTcsBuffDefAsset`，报 `LNK2019` + `LNK1120`）。
- 2026-10-04 **Task 2 收束**（实例 / per-unit 桶 / 门面 / 生命周期事件）：Step 1~7 **全勾**；提案 `add-state-instance-lifecycle`（`state-instance-lifecycle` ADDED ×7 + `integration-entity` MODIFIED ×1）**待归档**；验收 = 双配置编译零 warning / 零 error + `Tcs.Test.Slice.Run` **26/0 + 7b** 零红字（新增检查 19a–19f）+ `Tcs.Test.Slice.Reject` **7/7**（新增 E/F/G），证据 `EVID-2026-10-04-state-instance-lifecycle`。
  - **三处实施期裁定**：① **身份归资产、登记口显式收身份**（`RegisterStateDef(FGameplayTag, const FTcsBuffDef&)`；**不往数据 struct 加 `DefTag`**——否则同一 tag 在一个资产里手填两遍 = 双真相）；② **实例自持 `Unit`**（句柄无单位段 ⇒ 否则 `GetState(Handle)` 是 O(桶数)，且 Task 4 的修正器物化要用它）；③ **自建槽位、不复用 `TTcsInstancePool`**（那个池是"一池一句柄空间"，状态要"每单位一个句柄空间"；套用只有两条更差的路——理由写进 `TcsStateRegistry.h`）。
  - **一条归档器硬约束（第三次实证，已写进 delta 头部）**：MODIFIED 块里 `### Requirement:` 与 `#### Scenario:` 的**标题都不可改、不可删**（改名或删除被 `validate` 拒为 `omits scenario(s)`）⇒ Task 1 那条"状态定义只进缓存、不被装配到世界"的场景标题**原样保留**（它描述"缓存那一步"、今日仍成立），另**新增**一条场景陈述"被逐世界登记"，两步关系写进需求正文。
  - **一处跨文档口径更新**：`EVID-2026-10-04-tcs-state-def-asset` §1 的"状态定义不做世界装配"读数**今日仍成立但规格已变**——该证据已就地加口径更新注记（两步非互斥），并补记其区段所属日志的**冻结备份整文件哈希**（原证据只记活动日志区段、无文件级锚 ⇒ 日后无法复算）。
  - **一处待对账项（交 Task 8）**：`INDEX` §4.4 的 R7 行仍列**已消费**的 `WAIT-6`（该条已在 Task 0 Step 3 标结）——由 Task 8 Step 5 的"`INDEX` §4.4 同步"那一趟一并纠正；已就地记进台账《Task 2 收束》块。
  - **本 Task 的验收边界（七条）**见证据 §6：时值 / 快照 / 修正器三面零覆盖（Task 3 / Task 4）；`Stacked` 档与五轴未验（Task 5）；`Cancelled` 无内建产生者（R5.5-e）；`StackChanged` / `Periodic` 两枚 tag 零广播；只跑单轮。
- 2026-10-05 **Task 7 收束**（端到端验收与证据）：Step 1~5 **全勾**；提案 `verify-state-layer-e2e`（**1 新能力 + 3 改能力**）**待 §8 归档**，`validate <id> --strict` = valid、`openspec list` = **54/58**；验收 = **先关编辑器 → 双配置 UBT 构建**（Editor `7.96s` / Shipping `28.16s`，零 warning 零 error）**→ 重开 → 重跑**：`Tcs.Test.State.Run` **两轮各 15/0**（零红字、复现性摘要行逐字相同）+ `.Reject` **7/0**（红字恰 6 Warning + 3 Error）+ §5 回归逐字不变（**65/0 + 72/0**、**11/0**）；证据 `EVID-2026-10-05-state-layer-pie`（冻结快照 4,824 行 / 666,087 字节 / `db630575…` + 五区段哈希 + 7 段复算脚本 + §4 九条缺陷留痕 + §6 二十条边界）。
  - **八处与计划文本的偏移（逐条留痕）**：① **交付物 +1 TU**——装置由"扩 `TcsDevSliceRig.cpp` 检查块"改为**独立 TU** `TcsDevSliceRig_State.cpp`，公共面抽出 `TcsDevSliceRig_Internal.h`（本体纯搬迁 `-139 / +20`，语义零变更）；② **内容资产由 2 枚变 4 枚**——修正器模板与行为链**各需独立 tag 身份载体**（计划把它们当 buff 自带的隐含物，实施期证明不成立），且这 4 枚正是 `ATTR-1` 的闭合依据；③ **证据文件名顺延** `2026-10-04` → `2026-10-05`（实际收束日）、`EVID-2026-10-04-state-layer` → `EVID-2026-10-05-state-layer-pie`；④ **Step 2 两处收缩**——`PeekPending` 未用（竖切全程即时结算，用它会读到恒空，属**无对象读数**）、`FTcsAttributeChangedEvent` 未用（属性面已有 `EvaluateCurrent` 前后读数，再加事件计数是同因双证；而定序判据本轮**零读数**）；事件面由"六枚"收缩为**四个槽**（`Applied`/`Removed`/`Expired`/`Periodic`）；⑤ **Step 3 拒绝面由 5 条变 7 条**——"库未就绪"**降级为前置夹具守卫**（不是可断言的"操作被拒"路径）、"脏句柄"细化为**悬空句柄三面**，新增 `RegisterStateDef` 双硬门（Error 级）/ 未登记 `ChainId` / 未登记身份查询落 `nullptr`（**negative control**）；⑥ **Step 4 两条未落**——"关系表字段零消费者"与"`Cues`/`EventPayloadFilter`/`InterruptPriority` 留位"属 Task 5 / Task 1 的面，本竖切不触达 ⇒ 如实标"不在本轮覆盖声明范围内"，**未补条目充数**；另 §6 实有 **20 条**（15 条是实施期才显形的）；⑦ **Step 2 的可复现性判据改口径**——见下条；⑧ **裁定 D10 落点**——见下条。
  - **一处裁定（D10，用户拍板甲案）**：行为链落 **`Attribute.Attack`**、状态修正器保留 **`Attribute.Armor`**。判据 = 前者挂 `Context.RunSource`（常驻）、后者挂 `Instance.CascadeAnchor`（到期摘净）⇒ 两读数互为对照、各自可判。**否决乙案**（把链改到会被状态到期一起回退的属性上）：那会让验收变成**对实现的追认**——既有场景逐字不改即已满足。同批新增台账 `CHAIN-7`（链挂条目无框架侧回收触发点；三条回收路径**实测全堵**）与 `CHAIN-8`（链原语缺"直写基础值"，**不并入 `DAMAGE-2`**）。
  - **两处判据订正（本 Task 最值钱的副产物）**：① **可复现性 MUST NOT 落在区段字节上**——两轮 `State.Run` 区段 85 vs 84 行，逐行归一化求差集得唯一差异 = `FTcsEffectStepExecutorRegistry: …（共 10 类）`**只在第一轮**出现（进程内一次性惰性初始化）⇒ 与 `FTcsSourceHandle.Id`（进程内单调计数器）同族、**按构造不可复现**；判据改为"**摘要行逐字相同 + 各检查结论相同**"，摘要只留不变量。掩蔽坑 = 宽 pattern 会**同时命中 Damage 侧** `FTcsFlowStepExecutorRegistry`（`L3387`）⇒ MUST 带模块前缀。② **"现象只在重建二进制后消失"本身就是陈旧 DLL 的判据**——S8/S14 残留旧算术只在重编后消失；Live Coding 能给"编译通过"的假象而**不更新交付二进制** ⇒ 取证期 MUST NOT 用它。
  - **一处跨快照行号的教训**：证据 §1.1/§1.2 的"日志行"列曾**整体偏移一行**——起因是从**作废快照按区段端点做算术偏移**重映射，而两份快照区段基址不同。修法 = **全部按内容正则重定位**（42 处引用逐条复核）。**跨快照的行号 MUST 按内容重定位、MUST NOT 按偏移量推算。**
  - **一条工具面边界（供后续轮）**：`Object::IsDataValid` **未暴露给 Python**、MCP 830 工具**无此入口** ⇒ 逐资产作者侧校验唯一通道 = `unreal.EditorValidatorSubsystem.validate_assets_with_settings`（引擎 `EditorValidatorSubsystem.cpp:657` 无条件首调 `IsDataValid`，与检查 18 同一虚函数）；需 `load_assets_for_validation=True`、**一次只验一个**才可归因。四枚资产各 `1/1/0/0`。
  - **本 Task 的验收边界（二十条）**见证据 §6：单机单进程单 PIE（**Shipping 只验编译**）；`StateLevel*` 只测数组型；`ResolveAttrModDef` **运行期调用者为零**；`AttrModDef` 根下只有 `Check` 一个词；内容 `Fragments` 按 D2 留空（引用切片侧类型会在切片退役后静默失效）；链挂条目**按设计常驻**；`FTcsStepModifyAttribute` **不等同 GAS Instant**；`PeriodRefresh` 只验 `Keep`；`ExtendDuration` 上界未定；`STAT-9` **刻意未修**；**§3.9 校验是结论、不是"每条规则分支都触发了"的证明**。
- 2026-10-04 **Task 3 收束**（参数快照 / 等级源 / Duration-Period 到期堆 / R4.5-b 宿主插槽）：Step 1~10 **全勾**；两份提案归档（`+7 added / ~3 modified` + `+1 added / ~1 modified`），`validate --all --strict` = **29 passed / 0 failed**、`changes/` 零活动；验收 = 双配置编译零 warning / 零 error + `Tcs.Test.Slice.Run` **即时 32/0 + 延迟段 39/0**（新增 `20a`–`20l` 与一条夹具判定）+ `Tcs.Test.Slice.Reject` **9/0**（新增 H/I），证据 `EVID-2026-10-04-state-param-snapshot-and-level-sources`。
  - **四处实施期裁定**：① **等级读口不进 Core 上下文**（落 `FTcsStateEvaluateContext`；接口住 `TcsState/Public/Host/`——读口随消费者所属的领域层，先例 `FTcsAttributeEvaluateContext` 持 `ITcsAttributeProvider`）；② **`Instigator` 反而补进基础上下文**（分界判据 = "跨域通用的主体身份"进基础、"某域的读口"进该域派生上下文）；③ **快照取设计名** `FTcsParamSnapshot` / `FTcsParamSnapshotEntry`（Task 2 预留的 `FTcsStateParamSnapshot` 作废——快照是参数域概念，不是状态模块专属）；④ **读取适配器是纯 C++ 类**（`ITcsParamTableReader` 不是 `Blueprintable` ⇒ 造 `UObject` 壳属"零消费者预建"，且会把快照指针寿命绑到 GC 上）。
  - **两处交付物面收缩**：① Step 9 的接口与转发器**同住一个头** `TcsParamSourceHost.h`（同族先例 `TcsParamSource_AttributeScaled.h` 把接口 / 派生上下文 / 源三者同处一文件；计划原写的 `TcsParamSource_HostDelegate.h/.cpp` **不建**）；② 计划的 `TcsStateDuration.h/.cpp` **未建**——入堆 / 撤堆 / 重挂集中在 `TcsStateOps_Lifetime.cpp`（"时长驱动"的全部逻辑就是那四个函数，另开一层的抽象成本大于收益）。
  - **两条引擎/UBT 事实（本轮实测）**：① 含 `TUniquePtr` 元素的 `TMap` 类型**不能**加模块导出宏——MSVC 会强制实例化 `TMap` 复制路径并报 `C2280`（`TSparseSetElement` 复制到已删除的函数）；跨模块可达性改由**门面**承担。② `FTimerManager::SetTimer` 的句柄形参是**非 const 左值引用** ⇒ 临时量与 lambda 捕获副本（默认 const）都绑不上（`C2665`）；且 `CreateWeakLambda` 的宿主不能是按值捕获的裸指针。
  - **一处规格新能力的 Purpose 需手工补**（**第四次实证**）：归档器为新能力 `state-param-snapshot` 写的是占位句 `TBD - created by archiving …` ⇒ 归档后 `validate --all` = 28 passed / 1 failed；改主规格 `## Purpose` 后 **29 / 0**。纪律见 `CONVENTION` §6.6。
  - **两处如实边界（交 Task 7 / 内容侧）**：① 验收资产 `DA_Check_BuffDef` 是 `Finite` 而 `DurationTime` 求值为 0（4 条 `状态时值非正` Warning 的来源——本轮检查不读该状态的剩余时长，故不影响任何断言，但让"常规命令零红字"严格不成立；修法 = 内容侧配明确时长，或校验里放行"显式 0 = 立即到期"）；② **R4.5-b 的 C# 侧实测未做**（静态面编译 + glue 产物已核"非空壳"），归 R5 Task 7 端到端或单独补一次探针。
  - **本 Task 的验收边界（九条）**见证据 §5：单机单世界单 PIE；延迟段读数是帧推进型（周期次数随帧率浮动，判据用下限而非等式）；真实资产的等级类源 0 行（机制已验、内容待配置）；`PeriodRefresh` 只验 `Keep`；`StackPolicy` / 关系字段 / `Cancelled` 零覆盖；R4.5-b 只到静态面；`InstigatorLevel*` 只验数组型；`ExtendDuration` 上限未定（本轮口径 = 不钳到总时长）。
