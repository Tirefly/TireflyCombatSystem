# R5 实施计划：M3 状态层（`TcsState` 全生命周期竖切）

- **文档 ID**：`PLN-R5`
- **类型**：PLN / 计划
- **状态**：ACTIVE
- **权威范围**：R5 当前实施计划；含 **R5–R8 轮次路线图（现行排期真相源）** 与《R5.5 批次表（非正式轮号）》。R4 及以前的实施记录不住这里（见 `PLN-R4` / `PLN-R3-1` / `PLN-R3-2`）
- **最后更新**：2026-10-04

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

- [ ] **Step 1: 句柄与枚举**——句柄 `FTcsStateHandle{ Index, Generation }`（`IsValid()`：`Index >= 0 && Generation > 0`）。
  枚举：`EDurationPolicy{ EDP_Finite = 0, EDP_Infinite = 1 }` / `ETcsPeriodRefresh{ EPR_Keep = 0, EPR_Reset = 1, EPR_Immediate = 2 }` / `EStatePhase{ ESP_Inactive = 0, ESP_Active = 1, ESP_Expiring = 2 }`。
  `EStateRemoveCause{ ESRC_Expired = 0, ESRC_Removed = 1, ESRC_Cancelled = 2 }` / `EApplyResult{ EAR_Applied = 0, EAR_Refreshed = 1, EAR_Stacked = 2, EAR_Rejected = 3 }`（拒绝原因的具名细分随 R5.5-e 扩，**本轮只留 `Rejected` 一档 + 日志**）。
- [ ] **Step 2: `FStateInstance`（池化，纯数据）**——身份与来源：`DefTag` / `Handle` / `Source: FTcsSourceHandle`（**发号经 Task 0 的统一发号器**）/ `Instigator`。
  数值与时间：`Stacks: int32 = 1` / `Level: int32 = 0` / `Phase: EStatePhase` / `ParamSnapshot: FTcsParamSnapshot`（Task 3 落类型，本步先留字段）/ `DurationRemaining` / `PeriodRemaining` / `ExpiryEntry: FTcsTimeEntryHandle`（取消锚）。
  **MUST NOT** 持策略 / 载荷 / 订阅句柄 / `UObject` 引用（D3-7 v2 纪律）。
- [ ] **Step 3: 注册表与桶**——`FTcsStateRegistry`：`TMap<FTcsCombatEntityHandle, FStateBucket>`（`TUniquePtr` 间接层，**避免 `TSet`/`TMap` 扩容搬移导致元素地址失效**）；桶 = `TArray<FStateInstance>` + `SlotGenerations` + `FreeSlots`（照 `FTcsTriggerRegistry` 的槽位/代际手法）。脏句柄（代际失配）一律拒绝并记 `Warning`（时序竞态口径，**不 ensure**）。
- [ ] **Step 4: 门面 `UTcsStateSubsystem`**——世界过滤 `DoesSupportWorldType`（仅 Game/PIE/GamePreview，同族先例）+ `Deinitialize()` 全量清理（池重置 + 桶清空 + 到期条目撤销）。
  施加与查询：`ApplyState(FTcsCombatEntityHandle Target, FGameplayTag DefTag, FTcsSourceHandle Source, const TMap<FGameplayTag,double>& Overrides) -> EApplyResult` / `GetState(FTcsStateHandle) -> const FStateInstance*` / `ForEachState(FTcsCombatEntityHandle, TFunctionRef<bool(const FStateInstance&)>)`。
  移除与生命周期操作：`RemoveState(FTcsStateHandle, EStateRemoveCause)` / `ExpireState(FTcsStateHandle)` / `ExtendDuration` / `SetRemaining`（先落签名，堆同步在 Task 3 接线）/ `UnregisterUnit(FTcsCombatEntityHandle)`。
  **反射面**：`ApplyState` / `RemoveState` / `GetState` 一类**本轮不加 `UFUNCTION`**（句柄与 `TMap` 形参今天不可反射；脚本面留待 `SCRIPT` 系列，登记台账）。
- [ ] **Step 5: 生命周期事件**——`TcsStateEvents.h` 声明原生 tag（**带 `TCSSTATE_API` 导出宏**，裸 `extern` 会 LNK2001）：`Tag_TcsEvent_State_Applied` / `_Refreshed` / `_StackChanged` / `_Expired` / `_Removed` / `_Periodic`，tag 文本 = `TcsEvent.State.Applied` 等（换根后词根）。
  载荷 = `FTcsStateEventPayload{ FTcsStateHandle Handle; FGameplayTag DefTag; FTcsSourceHandle Source; FTcsCombatEntityHandle Instigator; int32 Stacks; int32 Level; EStateRemoveCause Cause; }`（`Cause` 仅 `Removed`/`Expired` 有意义）。
  **注意**：`SPEC-02-states` §3.3 里的 `Combat.State.Periodic` 是**换根前的旧名**，本轮一并改为 `TcsEvent.State.Periodic`。
- [ ] **Step 6: 广播点与 Phase 迁移**——`Apply` / `Refresh` / `StackChange` / `Expire` / `Remove` 五处广播（提交尾 flush 语义照 M2 惯例：`ApplyState` 的 `OnStateApplied` 走**立即通道**）；Phase 迁移矩阵非法迁移 = `ensure`（**配置错误**语义，与门面脏句柄的竞态口径分开）。
- [ ] **Step 7: 编译 + 冒烟**——双配置编译；装置侧：直接调 `ApplyState` 建出一个实例、总线订阅者收到 `TcsEvent.State.Applied`、`RemoveState` 后再收 `Removed`。

**验收信号**：实例可建可查可删；六个事件 tag 至少三个（Applied / StackChanged / Removed）有真实广播与订阅者回执。

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

- [ ] **Step 1: 快照类型**——`FTcsParamSnapshotEntry{ FGameplayTag Key; double Value; FInstancedStruct SourceRef; }`（**源引用位**供 Debug 与将来 Live 化；**禁用 `Resolved*` 命名**——动词纪律）；`FTcsParamSnapshot{ TArray<FTcsParamSnapshotEntry> Entries; }` + `TryGetNumericParam(FGameplayTag, double&) const`。
- [ ] **Step 2: 快照构建**——`Apply` 时对 `Def.Params` 逐行求值：`Overrides`（施加方覆盖）**优先**，否则 `Def` 默认值；求值上下文 = `FTcsParamEvaluateContext{ ParamTable = 施加方参数表, Subject = Target, EffectiveLevel = Level }`；**`ValueConvention` 转换在此写入点发生**（`FTcsValueConvention::ConvertToCanonical`——**全库首次点亮**），快照内**永远规范值**。
- [ ] **Step 3: 快照读取适配器**——`FTcsStateParamTableReader : public ITcsParamTableReader`（把 `FTcsParamSnapshot` 当参数表暴露给 `FTcsParamSource_ParamRef` 与修正器物化）——**这是 Task 4 物化器的输入口**，本步先落类型与 `TryGetNumericParam`。
- [ ] **Step 4: 等级源与宿主契约**——`ITcsEntityLevelProvider`：`UINTERFACE(Blueprintable)` + `UFUNCTION(BlueprintNativeEvent) int32 GetEntityLevel(FTcsCombatEntityHandle Entity) const`（**不带 `const` 在 `BlueprintNativeEvent` 上是硬规则**——按 UHT 要求去掉 `const` 并补 `_Implementation` 声明），宿主实现；门面加 `SetEntityLevelProvider(TScriptInterface<...>)` / `GetEntityLevelProvider()`（`UPROPERTY` 持有；未注入 = `nullptr`，是**配置状态**不是错误）。
  四个源：`FTcsParamSource_StateLevelArray{ TArray<double> Values; }`（读 `Context.EffectiveLevel` → 下标，越界落最后一档）/ `_StateLevelMap{ TMap<int32,double> Values; }` / `_InstigatorLevelArray` / `_InstigatorLevelMap`（读 `Context.Subject` → provider → level）。四型均继承 `FTcsParamEnumerableSource`（Task 0 Step 2），`GetIndexForLevel` 即"索引解析唯一真相在源"。
  上下文补第三个字段：`UPROPERTY(BlueprintReadWrite) TScriptInterface<ITcsEntityLevelProvider> LevelProvider;`（同 `ParamTable` 先例——`DAMAGE-5` 只记了两个字段，实现期按需补第三个，**记为对台账条目的增量**）。
- [ ] **Step 5: Duration 与 Period 落堆**——`Finite`：`PushExpiry(Clock->GetClock().Elapsed + DurationTime, OwnerId, 回调)`；`Infinite`：不入堆、永不过期。`Period > 0`：**重复到期条目**——每次周期回调广播 `TcsEvent.State.Periodic`（载荷带当前 `Stacks` + `Level`）后**重新入堆**；默认**等首个周期**（"施加即生效"由 apply 响应链表达）。
- [ ] **Step 6: `PeriodRefresh` 与生命周期操作**——刷新（Refresh/StackChange）时按 `EPR_Keep`（不动）/ `EPR_Reset`（重建周期条目）/ `EPR_Immediate`（立即执行一次并重置）；`ExtendDuration(handle, Δ)` / `SetRemaining(handle, T)` = 撤销旧条目 + 按新余量重入堆（**句柄配对清理**），`Infinite` 上调这些口 = `Warning` + 无操作。
- [ ] **Step 7: 到期路径**——到期回调（`UWorld` 弱引用 + 句柄代际校验）→ `ExpireState` → `EStateRemoveCause::Expired` → Task 2 的广播与回收链。
- [ ] **Step 8: 编译 + 冒烟**——Finite + Period buff：施加 → 若干个 `Periodic` → 到期 `Expired` + 实例回收（**装置逐 tick 轮询观察**，勿用"下一 tick"假定）。

- [ ] **Step 9: R4.5-b 参数源族宿主插槽（同批落地——同族同验证面）**——`ITcsParamSourceHost`（`UINTERFACE(MinimalAPI, Blueprintable)` + 两个 `UFUNCTION(BlueprintNativeEvent)`：`double Evaluate(const FTcsParamEvaluateContext& Context)` / `bool AllowsValueConvention()`）+ `FTcsParamSource_HostDelegate : FTcsParamValueSource`（持 `UPROPERTY TScriptInterface<ITcsParamSourceHost> Host`；两个虚函数**纯转发** + 空 Host 守卫 = 返回 0 / true）。
  两处都住 `TcsCore/Public/Parameter/`（`ITcsParamTableReader` 已在此；转发器 MUST 与基类同模块）；**载体与调用点零改动**（`FTcsParamValue.Source` 是裸 `FInstancedStruct`，照装）。
  **两个虚函数都要转发**（R-1 调研的硬判据）——漏 `AllowsValueConvention` 会让宿主新源在本轮 Task 3 的 `ValueConvention` 白名单上拿到错误的默认能力位。
  验证（R-1 调研已定的流程）：UBT 编译 + **glue 产物核验**（"能导出 ≠ 能往返"——确认 `Evaluate` 生成真实方法体，非空壳）+ **C# 实现实测**（配到 `FTcsParamValue.Source` 后求值走脚本；**复用 SCRIPT-8 的探针形态，不得重写**）。规格 delta = `param-value` 能力 ADDED 一条"宿主参数源插槽"需求 + 场景。
- [ ] **Step 10: 编译 + 冒烟收口**——本节全部落地后重跑双配置编译 + Task 3 的冒烟（快照/等级源/周期），确认 Step 9 未动既有求值路径。

**验收信号**：快照数值随 `Overrides` / Def 默认 / 等级源三者组合正确变化；周期事件计数与到期时刻可复现（同一 Seed 下逐字一致）；**C# 自定义源能在链里求值**（R4.5-b 判据）。

**非目标**：不做参数链（R6 `STAT-1`）；不做快照 Live 化（`Live` 模式本轮只在 `ETcsParamMode` 上留值）。

---

## Task 4 — 修正器物化（D3-19）

**目标**：让 buff **真的改数值**——`ModifierRows` 在 apply 时物化成 M2 账本条目（`Source` = 状态句柄），移除时按来源级联摘除。这是本轮第一个"肉眼可见"的硬信号。

**交付物**：

- `Source/TcsState/Public/State/TcsStateModifierMaterializer.h` + `Private/State/TcsStateModifierMaterializer.cpp`（新）
- `Source/TcsAttribute/Public/Attribute/TcsAttrModInstance.h`（改：补一个**显式构造/填充入口**——今天"全仓从未构造过该结构"）
- `Source/TcsState/Private/State/TcsStateOps_Modifier.cpp`（新：apply/remove 两个挂点）

**提案**：`add-modifier-materialization`

**依赖**：Task 3（快照 = 物化输入）

- [ ] **Step 1: 物化入口形状**——`FTcsStateModifierMaterializer::Materialize(const FTcsBuffDef& Def, const FTcsParamSnapshot& Snapshot, FTcsSourceHandle Source, FTcsCombatEntityHandle Target, TArray<FTcsAttrModInstance>& Out)`。
  逐条流水：`ModifierRows` → 解析 `UTcsAttrModDef::Def`（`FTcsAttrModDefTableRow`）→ `FTcsAttrModOperandDef` 转运行侧 `FTcsAttrModOperand`（`Literal` 若为 `FTcsParamSource_ParamRef`，经 `FTcsStateParamTableReader` 从**快照**取值）→ `ValueConvention` 规范转换 → 填 `Source`（状态来源句柄）/ `OverridePriority` / `SortKey`。
  **技术选型先例**：解析 `TSoftObjectPtr<UTcsAttrModDef>` 走 `LoadSynchronous()`（与定义库同步单出口一致；异步加载归 `INTEG-3`/R7）。
- [ ] **Step 2: apply 挂点**——`ApplyState` 流程尾部（`OnStateApplied` **之前**）：物化 + 逐条 `UTcsAttributeSubsystem::ApplyModifier(Target, Instance)`；`BeginBatch`/`Commit` 包裹（M2 事务语义）。
- [ ] **Step 3: remove 挂点**——`Remove/Expire` 流程头部：`UTcsAttributeSubsystem::RemoveBySource(Target, 状态来源句柄)`（**按来源级联摘除**，与触发行退订同一来源句柄）。
- [ ] **Step 4: 属性访问解析点（Q-2 缓解①）**——新增 `Private/State/TcsAttributeAccess.h`：`FTcsAttributeAccess::Resolve(const UWorld*)` 一处集中取 `UTcsAttributeSubsystem`；**本轮允许触碰的属性 API 只有 `ApplyModifier` / `RemoveBySource` / `EvaluateCurrent` 三个**，头注释写明"将来补 `ITcsAttributeAccess` 注入契约时**只换这一处**"。
- [ ] **Step 5: 编译 + 冒烟**——施加一个"减火抗"buff：M2 账本出现条目（`EvaluateCurrent` 观测到变化）→ 驱散后条目消失且**余量归位**（`RemoveBySource` 返回恰 1 条）。

**验收信号**：**属性数值前后可观测**（两条观测面：`EvaluateCurrent` 读数 + `FTcsAttributeChangedEvent` 广播）。

**非目标**：技能参数行分派（R5.5-f）；关系表检查器（R5.5-e）；参数链式聚合（R6）。

---

## Task 5 — 五轴堆叠与刷新政策（含 Custom 决策 Fragment）

**目标**：同一 buff 的重复施加有确定行为——层数、分组、溢出、数值叠加、刷新对时长的影响。

**交付物**：

- `Source/TcsState/Public/State/TcsStateStackPolicy.h` + `Private/State/TcsStateStackPolicy.cpp`（新：五轴枚举 + `FStateStackPolicy` + 决策 Fragment 基类）
- `Source/TcsState/Private/State/TcsStateOps_Stack.cpp`（新：共存决策与刷新）
- `Source/TcsState/Public/State/TcsStateStackFragment.h`（新：`FTcsStateStackDecisionFragment` 反射基类）

**提案**：`add-state-stacking-policies`

**依赖**：Task 4

- [ ] **Step 1: 五轴定义**——`EGroupByPolicy{ EGB_None = 0, EGB_PerSource = 1, EGB_PerInstigator = 2, EGB_PerTag = 3, EGB_Custom = 1<<4 }`（**值 0 = 默认/None、Custom 走逃逸位**：0 与 Custom 位各自的语义 MUST 在枚举注释里写清）；`MaxStacks`（`≤0` = 无限）；`EOverflowPolicy{ RejectNew, ReplaceOldest, ReplaceNewest }`；`EValueStackPolicy{ KeepMax, AddValues, PerStackValue }`；`EStackDurationPolicy{ None, RefreshRemainingToTotal }`。
- [ ] **Step 2: 组键与共存决策**——组键 = `GroupBy` 四态的确定性组合（`PerSource` → 来源句柄；`PerInstigator` → 发起者句柄；`PerTag` → 自定义分组 Tag）；命中组内按 `MaxStacks` 与 `Overflow` 决策：`RejectNew`（拒绝并返回 `EAR_Rejected`）/ `ReplaceOldest` / `ReplaceNewest`（移除旧实例 + 新施加 + 广播 `StackChanged`）；未满仓则 `Stacks++` + 广播。
- [ ] **Step 3: 刷新语义**——同组同来源的重新施加 = `Refresh`：按 `ValueStack` 更新数值（`KeepMax` 取大 / `AddValues` 累加 / `PerStackValue` 按层取值）→ **快照重建**（新 payload 覆盖，D3-12）→ 按 `StackDurationPolicy` 决定剩余时长（`RefreshRemainingToTotal` = 回到满额）→ `EPR_*` 作用于周期条目 → 广播 `TcsEvent.State.Refreshed`。
- [ ] **Step 4: Custom 决策 Fragment（唯一的策略落点）**——`FTcsStateStackDecisionFragment`（`USTRUCT(meta = (Hidden))`，USTRUCT 反射基类 + 虚分派 + 中性默认实现）：`virtual bool ShouldAccept(...)` / `virtual int32 ResolveStacks(...)` 一类**按决策种类**的抽象；`FStateStackPolicy` 以 `TInstancedStruct<FTcsStateStackDecisionFragment>` 持有一个 Custom 位；**策略实例归 Def 资产持有**（单字段暴露，类/载荷配对 bug 类消失）。
  **MUST NOT** 预建 `IValueDomainPolicy` / `ICostPolicy`（零消费者，Q-6）。
- [ ] **Step 5: 编译 + 冒烟**——四种组合各跑一遍：`None + ∞`（`NoMerge` 等价）/ `MaxStacks=1 + ReplaceOldest`（`UseNewest` 等价）/ `MaxStacks=1 + RejectNew`（`UseOldest` 等价）/ `PerInstigator + AddValues`（`StackByInstigator` 等价）——**组合覆盖 TCS 四个旧 Merger 的行为**（规格 §3.2 的等价表即验收清单）；另跑一个 Custom 策略样本。

**验收信号**：五轴组合下 `Stacks` / 数值 / 剩余时长的行为与规格 §3.2 等价表逐条一致；Custom 策略被真实调用（日志可证）。

**非目标**：不内建 Overflow 升级替换（规格明文：走"`StackChanged` 满仓触发行 → `ApplyState` 强力 → `RemoveState` 原"三步组合）；不做状态级重定向（R5.5-d 邻域）。

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

- [ ] **Step 1: `FTcsStepApplyState`（TcsState 领域步骤）**——字段：`FTcsCombatEntityHandle Target`（无效则取 `Context.Targets[0]`，仍无效 ⇒ 软失败）/ `FGameplayTag DefTag` / `TMap<FGameplayTag, double> Overrides`。
  执行体照 `FTcsStepSetVar` 形状（即时步骤、无挂起）：经 `Run.Owner->GetWorld()` 取 `UTcsStateSubsystem` → `ApplyState(...)`。
  **来源句柄 = 本次链运行态**（`Run.Self` 或链级来源位——实施时按"来源应表达谁施加的"定，并在头注释写明）；失败 ⇒ `Warning` + `TSR_Completed`（**不断链**），仅 `DefTag` 不可解析时 ⇒ `Error` + 断链。
- [ ] **Step 2: `FTcsStepModifyAttribute`（TcsEffect 机制原语）**——字段：`FTcsCombatEntityHandle Target` / `FGameplayTag Attribute` / `ETcsAttributeOp Op`（复用 M2 枚举，**不新造词表**）/ `FTcsParamValue Operand`。
  执行体经 `Run.Owner->GetWorld()` + **属性访问解析点** 取 `UTcsAttributeSubsystem` → 构造一次性 `FTcsAttrModInstance`（`Source` = 链运行态来源）→ `ApplyModifier`。
  **边界**：`TcsEffect` 本轮首次 include `TcsAttribute`——头注释 MUST 写明"允许依赖**下层**领域模块（Attribute），MUST NOT 依赖上层（Damage/Targeting/State/Skill）"。
- [ ] **Step 3: `AttributeCompare` 触发条件**——求值器注册进 `FTcsTriggerConditionRegistry`：条件数据 `{ FGameplayTag Attribute; ETcsAttributeComparison Comparison; double Threshold; }`（`Greater/Less/GreaterOrEqual/LessOrEqual`）；求值经属性访问解析点读 `EvaluateCurrent`（**与 Step 2 共用同一解析点**）。头注释写明"依赖属性读取注入位（本轮 = 直接依赖 `UTcsAttributeSubsystem`）"。
- [ ] **Step 4: 内联触发行（`TRIG-4`）**——`Apply` 时逐条 `Def.Triggers` 构造 `FTcsEffectTriggerInstance{ Def, Source = 状态实例来源句柄 }` → `UTcsEffectSubsystem::RegisterTriggerRow(...)`（**来源句柄取自 Task 0 的统一发号器**，与修正器物化**共用同一个来源句柄**——同一个状态 = 同一个来源，摘除一次全清）；`Remove/Expire` 时 `UnregisterTriggerRowsBySource(同一句柄)`。规格背书：`openspec/specs/effect-trigger/spec.md`「施加状态时登记 ⇒ `Source` = 状态实例句柄」。
- [ ] **Step 5: 行为 Fragment 订阅挂接**——`FTcsStateBehaviorFragment`（`USTRUCT(meta=(Hidden))`，反射基类 + 中性默认实现）：`TArray<FGameplayTag> Interests` + `virtual void OnStateEvent(const FGameplayTag& EventTag, const FInstancedStruct& Payload, const FStateBehaviorContext& Ctx)`；`Apply` 时按 `Interests` 向总线订阅（`Source` = 状态句柄，复用 D4-1 的订阅配对 + 来源级联退订机制），`Remove/Expire` 自动退订。**`FragmentSet` 归 Def 持有**（实例零策略）。
- [ ] **Step 6: 编译 + 冒烟**——一条链：`ApplyState(buff)` → buff 的 `Triggers` 订阅 `TcsEvent.State.Applied` → 起一条行为链 → 行为链里 `ModifyAttribute` 改属性 → `RemoveState` 后触发行级联退订、修正器级联摘除。
- [ ] **Step 7: 规格同步**——`SPEC-03-effects` 的"纯机制层"边界句改述为"**不依赖上层领域模块**"（Q-2 缓解②）；`SPEC-02-states` §1 的 `FStepApplyState` 落地状态更新。

**验收信号**：链里施加状态成功；buff 自带行为链被自己的生命周期事件驱动；来源级联一次摘干净（触发行 + 修正器同源）。

**非目标**：`Heal` / `Parallel` / `Repeat` / `OnError`（R5.5-a/b）；链侧中断/取消（M5/R6）；`ApplyState` 的操作复制（R7）。

---

## Task 7 — 端到端验收与证据

**目标**：一条竖切把 R5 全部面串起来，产出可复核证据与**如实边界清单**。

**交付物**：

- 宿主装置（**LAC 仓** `Source/TcsDev/`）：`TcsDevSliceRig.cpp` 扩检查块 + 新命令 `Tcs.Test.State.Run`（零红字）/ `Tcs.Test.State.Reject`（拒绝面，自带屏显声明）
- `Documents/combat-system-design/evidence/2026-10-04-state-layer-pie.md`（新，`EVID-2026-10-04-state-layer`）
- 内容资产：一个 buff 资产（Finite + Period + 修正器行 + 内联触发行 + 行为链）、一条施加链

**提案**：`verify-state-layer-e2e`（宿主侧装置为主，插件侧零行为改动）

**依赖**：Task 6

- [ ] **Step 1: 竖切脚本**——`ApplyState(灼烧类 buff)` → 快照冻结（`Overrides` 覆盖一项、等级源驱动一项）→ 修正器物化（减火抗 `X → Y`）→ `TcsEvent.State.Applied` → buff 自带触发行起行为链 → 行为链 `ModifyAttribute` 写属性 → 周期 `Periodic` 若干次 → 到期 `Expired` → 属性归位 + 触发行级联退订 + 实例回收。
- [ ] **Step 2: 观测面（双证据）**——① `EvaluateCurrent` / `PeekPending` 读数（属性面）；② `FTcsAttributeChangedEvent` 与六个状态事件的广播计数（事件面）；③ 每轮读数**逐字一致**（可复现性）。
- [ ] **Step 3: 拒绝面独立命令**——`Tcs.Test.State.Reject`：不可解析 `DefTag` / 无效目标 / 脏句柄 / 库未就绪 / `ExtendDuration` 作用于 `Infinite`——**全部红字集中在此命令**，常规命令零红字。
- [ ] **Step 4: 边界清单（如实记）**——证据文档 MUST 含：单机单世界单 PIE 进程；等级源只测 `StateLevel*` 两型（`InstigatorLevel*` 需宿主实现 provider，若无宿主实现则标"未覆盖"）；关系表字段**零消费者**（检查器归 R5.5-e）；`Cues` / `EventPayloadFilter` / `InterruptPriority` 仍留位；跨 PIE 残留检查（照 R4 的"世界反初始化全量退订"口径）。
- [ ] **Step 5: 复核**——装置输出留档（含 SHA-256，照 R4 惯例）+ 关键日志行号引用。

**验收信号**：`Tcs.Test.State.Run` **全绿且零红字**；`.Reject` 命令按预期报拒绝；证据文档的边界节逐条有实测支撑（无"推断"冒充实测）。

**非目标**：不做竞技级压测；不做 Mass 路径；不做网络复制。

---

## Task 8 — 收束

**目标**：把本轮从"代码绿"收敛为"文档/台账/规格/记忆四处一致"。

**交付物**：本计划全文回写 + 五处文档同步 + 提案归档 + 轮次检查点卡。

- [ ] **Step 1: 设计文档回写**——`[`SPEC-02-states`](../spec/03-module-states.md)`：身份块状态、§2 资产名、§3.1 宿主类名、§3.3 tag 名、§11 验收钩子改为"已验 / 未验"对账表、新增"R5 落地状态"节（收窄轮已落 §12 的全部基础，本步只做落地态回填）。
- [ ] **Step 1b: 相邻规格**——`[`SPEC-01-attributes`](../spec/02-module-attributes.md)` 的一致性待办行勾销；`[`SPEC-03-effects`](../spec/04-module-effects.md)` §2.1 原语实现状态（15 已落 **10**：+`ApplyState` / `ModifyAttribute`）；`[`SPEC-08-damage`](../spec/09-module-damage.md)` **不动**（本轮不涉）。
- [ ] **Step 2: `SPEC-TRACE` 增量**——新增行（状态 Def/实例/快照/等级源/堆叠/物化/`ApplyState`/`ModifyAttribute`/内联触发行/行为 Fragment）+ 改判既有行 + **证据边界注记**（含全部非外推边界）。
- [ ] **Step 3: 台账勾销与新增**——`STAT-2` / `DAMAGE-5` / `WAIT-6` / `TRIG-4` / `TRIG-5`（`AttributeCompare` 一支）/ `DAMAGE-2`（`ModifyAttribute` 一支）标已消费；`DAMAGE-2` 余 3 / `DAMAGE-3` 余 / `DAMAGE-4` / `STAT-3` 的**归属改判为 R5.5**；新增实施期发现；**计数与行数对账**（改了几条就改计数，三处：台账身份块 + `INDEX` §4.5 + `INDEX` §2 行）。
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
| **新增**（实施期） | 本轮实施中发现的边界，逐条登记 | Task 8 Step 3 |

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
