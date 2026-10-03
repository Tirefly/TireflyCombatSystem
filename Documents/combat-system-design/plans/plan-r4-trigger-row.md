# R4 实施计划三：触发行与伤害修改器通道（M4a 触发闭环）

- **文档 ID**：`PLN-R4`
- **类型**：PLN / 计划
- **状态**：ACTIVE
- **权威范围**：R4 当前实施计划；含 R4–R8 轮次路线图（现行排期真相源）
- **最后更新**：2026-10-01

> **换根注记（2026-10-01）**：本文 tag 名已随提案 `reroot-gameplay-tag-vocabulary` 换根——旧前缀 `Tcs.Event.*` / `Tcs.Flow.Key.*` / `Tcs.Flow.Template.*` / `Tcs.Attr.*` / `Tcs.Chain.*` 依次成为 `TcsEvent.*` / `DamageFlowKey.*` / `DamageFlowTemplate.*` / `Attribute.*` / `EffectChain.*`；本文正文一律用新名，旧名仅存于本注记与 `log/`、`ledger/`、`evidence/` 等历史文件。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 打通"**事件 → 触发行 → 效果链**"这条自动性闭环，并让 `09 §2.3` 的**伤害修改器唯一通道（D7-6）**端到端可用——今天 TCS 只能手动调 `ExecuteChainById`，本轮之后事件能自己驱动链，且"目标受火伤 +20%"这类修改器有路可走。

**Architecture:** 触发行住 `TcsEffect`（04 §1：M4a = 触发行/链/解释器），作为**总线共享 Handler**（裁决 2a：事件类型 → Handler CDO，事件 struct 上不自绑 delegate）；行求值四道门 = 事件 Tag 路由 → `ExecutionGate` → `GateTags` → `Conditions`。修改器通道的另一半 `ModifyFlow` 是 **TcsDamage 的链原语**（D4-3 战斗组），它经 `FTcsEffectContext::EventPayload`（**已就位的字段**）取回流程上下文，向黑板提交——**TcsEffect 不需要知道任何 TcsDamage 类型**，依赖方向不破。

**Tech Stack:** UE 5.8 C++、GameplayTags、`FInstancedStruct` 策略/数据载体、UBT；编译验证用 unreal-cpp-compile 技能探测引擎路径。

---

## 0. 上一轮结论的一处更正（先读，避免计划建在错前提上）

我在上一轮口头汇报里说"**6 个收集事件 Tag 声明了但没有任何步骤会发它们**"——**这是错的**。核实结果：

- 标准步骤库**十步全部已实现且已注册执行器**（`TcsFlowStepsCore.cpp` 4 个 + `TcsFlowStepsRest.cpp` 8 个，共 12 个含两个数据步骤）；
- `PreHit / Hit / Crit / Element / AfterDamage / PreExecute` 六步**都会广播各自的收集事件**（`TcsFlowStepsRest.cpp:63/84/109/137/150/164`）；
- 它们"看起来没有发布者"只是因为**官方默认模板只组装了 4 步**（`CollectStart → BaseDamage → Execute → Completed`）——而这是 **D7-5 的有意设计**（"流程阶段构成 = 项目知识"，插件只给标准件与一个最小默认模板，宿主可整表替换）。

**所以"步骤库补全"不是 R4 的任务**——那部分代码早就在了。R4 真正缺的是**订阅侧**（触发行）与**提交侧**（`ModifyFlow` 链原语）：

| 环节 | 现状 |
|---|---|
| 收集事件广播（流程侧） | ✅ 已实现（十步全会发） |
| 消耗裁决（`Execute` 按 `SortKey` 选一） | ✅ 已实现（`TcsFlowStepsCore.cpp:127-140`） |
| **订阅收集事件（触发行）** | ❌ **完全没有**——全库零事件订阅者（插件侧） |
| **提交修正（`ModifyFlow` 链原语）** | ❌ **不存在**——`FTcsFlowModify` 是**流程内**步骤，不是链原语 |

---

## Global Constraints

- **禁 TDD**（用户级最高纪律）：无失败测试步骤；每任务验证 = UBT 编译通过 + 定向人工检查。
- **提交纪律**：任何 `git commit` 仅在用户明确授权后执行；计划不含自动提交步骤，任务边界即"停点待检查"。
- **风格**：创建/修改 C++ 前执行 unreal-cpp-style 技能——Tab 缩进、UTF-8 无 BOM、LF；`.h` 用 region（region 前中文注释、region 内重新声明访问域）、`.cpp` 扁平；单 `.cpp` ≤300 行，超出按 `<Name>_<Feature>.cpp` 拆分；文件头 `// Copyright Tirefly. All Rights Reserved.`。
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名；枚举值 = 枚举名缩写全大写；API 导出宏 `TCS<模块>_API`（跨模块消费面 MUST 带——台账 WAIT-9）；动词纪律（`Resolve` 仅用于句柄/Id→对象）。
- **依赖铁律**：`TcsCore ← TcsAttribute ← TcsEffect ← {TcsDamage, TcsTargeting, TcsState} ← TcsSkill`。**TcsEffect MUST NOT include 任何领域模块**（04 §1）；本轮触发行全部住 TcsEffect，`ModifyFlow` 住 TcsDamage——**两者经 `FTcsEffectContext` 的既存字段通信，零新依赖边**。
- **日志**：归 UE 原生分类制（D0-6 v2）——使用日志 include `<模块>LogChannel.h`；**常规验收命令 MUST 零红字**（故意的失败输出独立成 `.Reject` 命令——本项目 2026-09-18 立、2026-09-21 复犯后泛化的纪律）。
- **编译**：每任务以 UBT **Development Editor** 编译通过为完成门槛；**改动含 `WITH_EDITOR` 面或新 UPROPERTY 时须补跑 Shipping**（Shipping 是唯一能照出"编辑器专用 API 泄漏"的检查——2026-09-22 实证）。
- **规格先行**：每个 Task 的实施以 OpenSpec 提案开道（`openspec validate <id> --strict` 通过 → 实施 → `openspec archive <id> --yes`）。归档时 MODIFIED 需求的标题 MUST 与主规格**逐字一致**；改名需求须用 `## RENAMED Requirements`（2026-09-22 两次归档中止的教训）。
- **过网结构纪律**：MUST 纯反射数据——禁 `TFunction`（UHT 报错）、禁 `TMap`/`TSet`（UHT 报错）、`FInstancedStruct` 内层须可复制。
- **GC 纪律（2026-09-23 新立）**：登记表若持**非 UPROPERTY 容器**（`TMap<…, TUniquePtr<…>>`）且元素含对象引用，MUST 覆写 `AddReferencedObjects` 补引用（`fix-registry-gc-visible-holding` 提案；先例 = `UTcsDamageSubsystem` / `UTcsEffectSubsystem`）。
- **零消费者不预建**：字段可先就位（设计承诺），但**不得为无消费者的字段造机制**；未落地面 MUST 写进计划注记 + `deferred-inputs-ledger.md`。
- 设计规格来源：`Documents/combat-system-design/`（`04-module-effects.md`、`09-module-damage.md`、`2026-09-02-m4-effects-decision-points.md` D4-1/D4-5、`2026-09-02-m5-skill-decision-points.md` D7-6）；冲突时以设计文档 + 用户拍板为准。

---

## 轮次路线图（R4–R8）

本轮起路线**偏离** `../HISTORICAL/dec-2026-09-02-module-map-proposal.md:146`（模块地图提案 v1，已存档）的原序（原序：`R4 M3 → R5 M4a+M4c → R6 M5 → R7 M6 → R8 M7/M8`），改为**触发行先行**。理由（2026-09-23 用户认可）：**触发行是 M3 行为面的验收前置**——`03 §6` 明文"buff 行为由 M4 触发行订阅 M3 生命周期事件挂接"，M3 先落地则其行为面在交付当天没有任何订阅者，验不了。

| 轮次 | 主题 | 主要交付 | 前置 | 台账消费 |
|---|---|---|---|---|
| **R4（本计划）** | **M4a 触发行 + 伤害修改器通道 + 原语补齐（一批）** | ①触发行定义/条件注册表（Task 1 已完成）；②登记表与求值器 + **载荷读取器注册表**（Task 2 **已完成**）；③**独立资产载体 `UTcsEffectTriggerDef` + DefLibrary 发现**（Task 2.5）；④`ModifyFlow` 链原语（Task 3）；⑤**追加 4 个原语**：`SetVar` / `Branch` / `RunSubChain` / `WaitEvent`（Task 3.5）；⑥端到端验收（Task 4）；⑦收束（Task 5） | R3 已收束 | **DAMAGE-3 部分**（ModifyFlow）；**DAMAGE-1 部分**（载荷装填）；**DAMAGE-2 部分**（4 个原语——2026-09-23 用户拍板提前） |
| **R4.5**（**非正式轮号**） | **脚本通道收口**（寄在 R4 与 R5 之间，不占正式轮） | ①注册表跨世界寿命缺陷修复（**已闭环 2026-09-29**）；②R-1 参数源族宿主插槽（未开工）；③R-2 两张注册表宿主脚本插槽 + 载荷读取器登记（**半闭环**） | R4 Task 1/2 已落地 | 不消费台账条目；权威登记 = `LEDGER-reflection` 的 R-1 / R-2 |
| R5 | **M3 状态层（TcsState）** | `UTcsStateDef` 家族 / `FStateInstance` + 中央注册表 / 五轴堆叠 / 关系表 + 级联重评 / Duration-Period + 到期堆 / `ParamSnapshot` / **修正器物化（D3-19）** / **`ApplyState` 链原语（随 M3 同批——否则 Buff 有事件却施加不了状态）** / 生命周期事件全集 / **`Heal` 原语** / `ModifyAttribute`（属性访问注入位与 `ApplyState` 同批） | R4（行为面验收） | **STAT-2**、**DAMAGE-2**、**STAT-3**、**DAMAGE-2 余**（`Repeat`/`Parallel`/`OnError`）、**DAMAGE-3 余**（`Heal`） |
| R6 | **M5 技能层（TcsSkill）** | `UTcsSkillDef` / 账本 `FLearnedSkillEntry` / 六道门禁 / **时段驱动 `FPhaseSpan` + 打断** / 冷却多轨道 + 三事件 / Cost 策略 / **参数链（带式聚合，折叠器复用）** / 链重定向栈 / `FEntrySelector` | R5（`FSkillDef` 继承 `FStateDefBase`） | **STAT-1**（参数链接入折叠器）、**CORE-1**（若全域订阅需求成立） |
| R7 | **M6 集成层** | 两级单位（Mass 小兵 + 全功能军官）/ StateTree 决策接线 / **AttributeSet 全套** / `PrimaryAssetTypesToScan` + 发现机制切换 AssetManager + 加载层三策略 | R6 | **INTEG-1**、**INTEG-2**、**INTEG-3**、**WAIT-2**、**WAIT-5**、**WAIT-6**、**WAIT-8 余**（模板资产化） |
| R8 | **M7 表现 + M8 编辑器与工具** | `TcsCue`（Cue 契约 + PlayCue + 默认适配）/ 校验矩阵（四联 + 定义校验）/ Def 双轨同步器 / Explain 调试面板 / K2 强引脚 | R7 | **TOOLS-1**~**TOOLS-6**、**WAIT-1**、**WAIT-4**、**WAIT-9 全量审计**、**WAIT-10** |

**说明**：
- **R4 扩容（2026-09-23 用户拍板）**：原 5 Task 扩为 7 个 Task（增 Task 2.5 载体 + Task 3.5 原语批次）。理由——触发行落地后"事件 → 条件 → 链"就通了，但链里只有 3 个原语（等待/选目标/扣血）**能触发却做不了什么**；`SetVar`/`Branch`/`RunSubChain`/`WaitEvent` 四个**零新依赖**（或仅需扩唤醒源），与触发行同轮交付最经济。
- **`ModifyAttribute` 与 `ApplyState` 后置到 R5（2026-09-23 用户拍板）**：两者都需要"属性访问注入位"（`FTcsEffectContext` 今天没有属性读/写口），且 `ApplyState` 本身就是 TcsState 的领域步骤——**与 M3 同批落地**最自然（M3 不落地就没有状态可施加）。
- **`Repeat`/`Parallel`/`OnError` 留 R5**：分别需熔断游标、`JoinCount` 汇合、错误路径语义——三者都比上述四个重。
- **R6 可能需拆两轮**（M5 与时段/打断都是重活）——届时按 `M5 账本/施法` 与 `时段与冷却` 分拆，前置关系不变。
- **不绑轮次的触发条件型条目**（WAIT-2/WAIT-3/WAIT-5 等）在其触发条件成立时随当轮消费，不单独排期。
- **INTEG-3 的加载层**是 R7 内最重的一块（三策略 + 异步默认/同步逃生口），若 R7 超载可拆出独立轮。
- **`R4.5` 不占正式轮号**（2026-09-29 立、2026-09-30 落档）：它是 R4 与 R5 之间的一批「脚本通道收口」工作，逐批状态见下方《R4.5 批次表》。**立此条的直接原因 = 它原先只住在会话里**——2026-09-30 实证：会话 compact 摘要把整条 `R4.5` 丢掉了，是靠翻会话记录才捞回来的。

### R4.5「脚本通道收口」批次表（非正式轮号）

> **依据**：`DEC-04` 裁定 ⑤（落地顺序 = 先做 A 类值语义改造，再在其上落 R-1 / R-2 两张新注册表）+ 用户 2026-09-29 裁定「**TCS 插件应该直接兜底，而不是交给宿主项目**」。
>
> **与 R4 剩余 Task 的关系**：互不阻塞。回点顺序按本计划原文（Task 2.5 → 3 → 3.5 → 4 → 5）。
>
> **权威登记处**：本表登记**批次与状态**；条目级真相仍在 `LEDGER-reflection`（`reflection-backlog.md`）的 R-1 / R-2 行——两处冲突以该册为准。
>
> **⚠ 提交边界（2026-09-30 留痕，勿误判）**：本块是**文件内混排**，不是独立提交单元——它与「P-A 排序相位并入 Task 3.5」（`:407-410` Step 块 / `:419-439` 并入注记）、Task 3 的 `UPROPERTY` 改动（`:337-339`）**同时**落在本文件与 `GLOSSARY` / `INDEX` / `reflection-backlog` 的同一批未提交改动里，且那些文件的「最后更新」日期两主题共写（拆不开）。**当前做法 = 随提案 `add-damage-modifyflow-primitive` 同批提交**（用户 2026-09-30 确认）。**将来若需单独提交 R4.5**，须先按 hunk 拆本文件（`git diff HEAD -- <本文件>` 逐块认领），不要以为它已经是一个独立单元。

| 批次 | 内容 | 状态 | 判据 / 证据 |
|---|---|---|---|
| **R4.5-a** | 注册表跨世界寿命缺陷修复：4 张注册表双轨登记值（对象/世界弱引用）+ 失效判据 + 拒绝门收窄为「同世界活对象重复」+ `Unregister` / `GetDynamicKeys` + 门面 `Deinitialize` 按世界撤销 | **✅ 已闭环（2026-09-29）** | 提案 `harden-registry-cross-world-lifetime`（已归档 `2026-09-29-…`）；证据 `EVID-2026-09-29-registry-lifetime`——同一 Editor 进程两连 PIE，5 个缺陷签名（`拒绝重复登记` / `已有执行器` / `保留首个` / `登记失败` / `Handled ensure`）**全 0**；Development + Shipping 双配置 0 error / 0 warning |
| **R4.5-b** | **R-1 参数源族宿主插槽**（`ITcsParamSourceHost` + 转发器 struct，拟名·未实现；已调研并拍板 2026-09-24） | **❌ 未开工** | 判据：C# 侧能定义新数值源并跑通一条链。落点 = `LEDGER-reflection` R-1（"已调研并拍板，待落地"） |
| **R4.5-c** | **R-2 两张注册表的宿主脚本插槽**（`BlueprintType` 升格 + `Register(UScriptStruct*, UObject*)` 反射入口）+ **TcsDamage 载荷读取器登记** | **⚠ 半闭环** | 寿命语义那半已随 R4.5-a 落地（`reflection-backlog.md` R-2 行标"部分闭环"）；**插槽那半未落**——`UE_DEFINE_TRIGGER_PAYLOAD_READER` 至今全库只有定义、零登记（登记需求并入本计划 Task 3） |

> **边界（如实记）**：`EVID-2026-09-29-registry-lifetime` 明文**不覆盖**条件求值器 / 载荷读取器两张注册表的寿命语义行为验证（本次装置未登记它们的动态条目；逻辑同构，属静态实现）——恰是 R4.5-c 要动的那两张。

> **★ 跨轮耦合（2026-09-30 登记）**：R4.5-b / R4.5-c 与**本轮 Task 3.5 的 P-A 评分器插槽**（`ITcsTargetScorerHost`，拟名）是**同一形态的三个实例**（`UINTERFACE(Blueprintable)` + `BlueprintNativeEvent` → USTRUCT 转发器；见 Task 3.5 内的 P-A 说明）。三处分散推进会把同一套验证装置（C# 实现 + 同世界原生 GC + 跨 PIE 寿命）写三遍。**权威登记与并批取舍见 `LEDGER-reflection`《★ 宿主插槽家族的耦合关系》**——本表只留指针，不重复内容。

---

## File Structure（本计划新建/修改）

```
Plugins/Tirefly/TireflyCombatSystem/
  Source/TcsEffect/
    Public/Trigger/TcsTriggerRow.h            （新）触发行的数据形状（10 字段）
    Public/Trigger/TcsTriggerConditions.h     （新）触发条件最小集 + 求值助手
    Public/Trigger/TcsTriggerEvaluator.h      （新）共享 Handler：事件 → 行匹配 → 四道门 → 起链
    Private/Trigger/TcsTriggerConditions.cpp  （新）条件求值实现
    Private/Trigger/TcsTriggerEvaluator.cpp   （新）求值器实现 + 订阅装配
    Public/TcsEffectSubsystem.h               （改）触发行登记表 / 点灯 API / 订阅生命周期
    Private/TcsEffectSubsystem.cpp            （改）同上 + ARO 补引用（登记表持对象引用时）
  Source/TcsDamage/
    Public/Chain/TcsStepModifyFlow.h          （新）ModifyFlow 链原语（提交流程黑板）
    Private/Chain/TcsStepModifyFlow.cpp       （新）执行器 + UE_DEFINE_EFFECT_STEP_EXECUTOR

宿主（不入库，验证用）：
  Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp （改）扩展现有 Tcs.Test.Slice.Run 加"修改器通道"检查
  Content/TcsDev/                              （改）新增一条单步链资产（ModifyFlow）
```

**领域子目录命名依据**：`TcsEffect` 现有 `Chain/`（链与步骤）、`Host/`（宿主契约）；触发行是 M4a 的独立子域，新建 `Trigger/`（`cpp-module-structure` 规格的领域子目录要求）。

---

## Task 1: 触发行数据形状与条件求值器 —— **已完成（2026-09-23，含一次形态精修）**

**Files:**
- Create: `Source/TcsEffect/Public/Trigger/TcsEffectTrigger.h`（`FTcsEffectTriggerDef` + `ETcsExecutionGate`）
- Create: `Source/TcsEffect/Public/Trigger/TcsEffectTriggerInstance.h`（`FTcsEffectTriggerInstance` + `FTcsEffectTriggerHandle`）
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerCondition.h` + `Private/Trigger/TcsTriggerCondition.cpp`（条件注册表 + 自注册宏 + 内置条件 + 求值助手）

**Interfaces:**
- Consumes: TcsCore（`FGameplayTag` / `FTcsSourceHandle` / `FTcsCombatEntityHandle` / `TTcsInstanceHandle` / `FInstancedStruct`）。
- Produces:
```cpp
// —— 执行闸 ——
UENUM() enum class ETcsExecutionGate : uint8 { TEG_Always = 0, TEG_AuthorityOnly = 1 /*未实现*/ };

// —— 触发定义（纯配置；可进资产 / 可内联）——
USTRUCT(BlueprintType)
struct TCSEFFECT_API FTcsEffectTriggerDef
{
	GENERATED_BODY()
	UPROPERTY(...) FGameplayTag EventTag;                     // ① 订阅哪个事件
	UPROPERTY(...) FInstancedStruct EventPayloadFilter;       // ② 载荷预筛（只存不裁）
	UPROPERTY(...) TArray<FInstancedStruct> Conditions;       // ③ 门禁条件（走条件注册表）
	UPROPERTY(...) FGameplayTag EffectChainId;                // ④ 触发后执行的链 id
	UPROPERTY(...) int32 Priority = 0;                        // ⑤ 大者先
	UPROPERTY(...) ETcsExecutionGate ExecutionGate = ...;     // ⑥ 执行闸
	UPROPERTY(...) int32 InterruptPriority = 0;               // ⑦ 只存不裁
	UPROPERTY(...) TArray<FGameplayTag> GateTags;             // ⑧ 行级点灯开关
	UPROPERTY(...) bool bConditionMissIsSilent = true;        // ⑨ 条件未过是否静默
	// 已删：Cues（TcsCue 未敲定，无消费者——TcsCue 落地时加回）
	// 已砍：Scope / HandlerClass（D4-1 v2）
};

// —— 触发实例（运行期 = 定义 + 簿记）——
USTRUCT() struct TCSEFFECT_API FTcsEffectTriggerInstance
{
	GENERATED_BODY()
	UPROPERTY() FTcsEffectTriggerDef Def;      // 定义
	FTcsSourceHandle Source;                   // 级联退订锚点（非 UPROPERTY——句柄不进资产）
	UPROPERTY() FTcsEffectTriggerHandle Self;  // 自身句柄
};

// —— 条件注册表（按类型分派；与步骤执行器同构）——
using FTcsTriggerConditionTest = TFunction<bool(
	const FInstancedStruct& ConditionData, const FTcsTriggerContext& Context, double RandomValue)>;
class TCSEFFECT_API FTcsTriggerConditionRegistry
{
	static FTcsTriggerConditionRegistry& Get();
	void AddPending(FTcsTriggerConditionEntry Entry);                          // 静态自注册
	void Register(const UScriptStruct*, FTcsTriggerConditionTest);             // 动态入口
	const FTcsTriggerConditionTest* Find(const UScriptStruct*);                // 未命中 nullptr
};
#define UE_DECLARE_TRIGGER_CONDITION_EVALUATOR(TestFn) ...
#define UE_DEFINE_TRIGGER_CONDITION_EVALUATOR(ConditionType, TestFn) ...

// —— 触发期上下文（最小集）+ 内置条件 + 求值助手 ——
USTRUCT() struct FTcsTriggerContext { FGameplayTag EventTag; TArray<FGameplayTag> ClassificationTags; FTcsCombatEntityHandle Caster; };
USTRUCT() struct FTcsTriggerCondition_HasAllTags { TArray<FGameplayTag> Tags; };
USTRUCT() struct FTcsTriggerCondition_Chance { double Probability = 0.0; };
bool EvaluateTriggerConditions(const TArray<FInstancedStruct>& Conditions, const FTcsTriggerContext& Context, double RandomValue = 0.0);
```

- [x] **Step 1: OpenSpec 提案**——**两次**：`add-effect-trigger-row`（首次落地）→ `refine-effect-trigger-shape`（形态精修）。均归档，规格库 **23/23** 全绿
- [x] **Step 2: 实施**（见上 Interfaces；三个新头文件 + 一个 `.cpp`）
- [x] **Step 3: 编译验证**——Development Editor **零警告零错误**
- [x] **Step 4: 定向人工检查**——依赖面 `grep "^#include"` 零领域模块；旧类型名零残留

> **实施注记（必读）**：
> - **⚠️ 形态精修（2026-09-23 用户审阅后）——四处修正**：
>   ①**Def/Instance 分层**：原 `FTcsTriggerRow` 把"能进资产的配置"与"绝对不能进资产的 `Source` 句柄"混在一个 struct 里（用户指出）→ 拆为 `FTcsEffectTriggerDef`（纯配置）+ `FTcsEffectTriggerInstance`（定义 + 簿记）；
>   ②`Effects` → **`EffectChainId`**（用户：更直观）；
>   ③**删 `Cues`**（用户：TcsCue 整体未敲定，留字段 = 假控件）；
>   ④**条件改注册表**（下条详述）。
> - **⚠️ 条件形态：注册表而非虚分派基类（我的一次建议被调研结论推翻）**：评审时我曾建议"改成 `USTRUCT` 虚分派基类"（理由：逻辑内聚、与既有策略位同构）。`2026-09-23-scripting-language-ustruct-research.md` §5–§6 的引擎级论证**推翻**该方向：虚分派依赖 vtable，vtable 来自 UHT 为 C++ 类型生成的 `TCppStructOps<T>`（`Class.h:2265`）；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr`（`Class.cpp:3120-3144`）→ 实例内存 `Memzero` 起步、vtable 指针位为 0 → **野调用崩溃**。故虚分派在脚本侧**物理不可达**，注册表分派**可达**。**已按注册表实现**（内置条件也走同一注册表——不分内外两套路径）。
> - **一处 UHT 实证**：`Source: FTcsSourceHandle` **不能作 `UPROPERTY`**（非反射纯 C++ struct，UHT 报 `Unable to find 'class', ... with name 'FTcsSourceHandle'`）。同款先例 = `FTcsAttrModInstance.Source`。**不序列化是正确语义**（句柄运行期发号，本就不该进资产）。
> - **命名与流程侧的分化**：`TcsDamage` 已有 `FTcsConditionHasAllTags` / `FTcsConditionChance`（流程步骤用）。本轮建的是**触发期**版本（上下文 `FTcsTriggerContext`）——TcsEffect 不能 include TcsDamage（依赖铁律），故各持一份，命名用 `FTcsTriggerCondition_*` 前缀区分。
> - **`FTcsTriggerContext` 是最小集**：只含本批条件需要的三个字段。**MUST NOT** 为将来条件预建字段——Buff 生命周期参数（`StateHandle`/`Stacks`/`Level`）要等 M3 落地才有真实数据源。
> - **反射注册入口欠账**：`Register` 是纯 C++ 面（`TFunction` 不可反射）。这是与步骤执行器注册表**共享**的欠账（CS 调研 §7.6 的 G-2），**同批**解决——不在此处单独开一个反射入口（否则两处口径不一）。
> - **无行为实证（如实记）**：本批只落形状与注册表**机制**，零调用方（登记表/求值器属 Task 2）——验证全是静态检查（编译 + grep + validate）。

---

## Task 2: 触发行登记表与求值器（订阅生命周期） —— **已完成（2026-09-23）**

**Files（实际落点）:**
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerEvaluator.h` + `Private/Trigger/TcsTriggerEvaluator.cpp`
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerRegistry.h` + `Private/Trigger/TcsTriggerRegistry.cpp` + `_Query.cpp`（**实施时追加**——登记表拆为纯逻辑类，理由见下）
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerPayloadReader.h` + `Private/Trigger/TcsTriggerPayloadReader.cpp`（**实施时追加**——见下"载荷读取器"）
- Modify: `Source/TcsEffect/Public/TcsEffectSubsystem.h`（登记表 API + 点灯 + 生命周期 + ARO 转发）
- Modify: `Source/TcsEffect/Private/TcsEffectSubsystem.cpp` + `Private/TcsEffectSubsystem_Trigger.cpp`（**实施时拆分**——门面 `.cpp` 实施前已 289 行，塞不下）
- 规格：`openspec/changes/archive/2026-09-23-add-effect-trigger-registry/`（MODIFIED × 1 + ADDED × 3，规格库 23/23）

**实施期发现的三处计划错误（均已纠正并回写规格）:**

1. **`Caster` 解析规则不可实现（计划内部矛盾）**：原注记写"从载荷内已知类型取（流程收集事件 → `Context->Attacker`）"——`TcsEffect` MUST NOT 认识任何领域载荷类型（依赖铁律），不可能写 `GetPtr<FTcsDamageFlowCollectEvent>()`。**且若不管**：`ClassificationTags` 无来源 → 空集 → R4 随规格交付的 `HasAllTags` **恒不过**（出厂即不可用的条件）。**处置** = 新增**触发载荷读取器注册表**（`FTcsTriggerPayloadReaderRegistry`），由载荷类型的属主模块自登记（TcsDamage 的收集事件读取器随 Task 3 登记）。
2. **"值语义 `TArray` → 无需 ARO"判据错误（GC 地雷）**：是否需要 ARO 与值/指针语义**无关**，只取决于**容器是否 GC 可见**。`FTcsTriggerRegistry` 是门面的**非 `UPROPERTY` 成员** → GC 的 `RefLink` 走不到它，而行内 `FInstancedStruct`（`Conditions`/`EventPayloadFilter`）内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用（D4-16 类型不设限）→ **静默回收**。这与 WAIT-8 是同一类缺口（**缺口在容器，不在载荷**）。**处置** = 门面 `AddReferencedObjects` 逐行补引用（与 `ChainDefs` 同款手法）。
3. **"代际校验不适用"错误**：登记表用空闲链表复用槽位后，**陈旧句柄会静默改指另一行**（`UnregisterTriggerRow(旧句柄)` 摘掉无辜的行）。**处置** = 自持代际计数（**仍不引入 `TTcsInstancePool` 类型**——池的挂起锚/占用统计在此确无收益）。

**Interfaces（实际交付）:**
```cpp
// —— 共享 Handler（裁决 2a）——
UCLASS()
class TCSEFFECT_API UTcsTriggerEvaluator : public UTcsEventHandler
{
	GENERATED_BODY()
public:
	virtual void HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload) override;
	void Initialize(UTcsEffectSubsystem* InOwner);
private:
	FTcsTriggerPayloadInfo ReadPayloadInfo(const FInstancedStruct& Payload) const;
	static bool PassesExecutionGate(const FTcsEffectTriggerDef& Def);          // 门②
	bool PassesGateTags(const FTcsEffectTriggerDef& Def) const;                // 门③
	bool PassesConditions(const FTcsEffectTriggerDef& Def, const FTcsTriggerContext& Context);  // 门④（非 const：推进随机流）
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;
};

// —— 登记表（纯逻辑类，照 UTcsEventBusSubsystem 持 FTcsEventBus 的分工）——
class TCSEFFECT_API FTcsTriggerRegistry
{
public:
	void SetEvaluator(UTcsTriggerEvaluator* InEvaluator);   // 弱引用持有
	FTcsEffectTriggerHandle RegisterRow(const FTcsEffectTriggerInstance&, UTcsEventBusSubsystem*);
	bool UnregisterRow(FTcsEffectTriggerHandle, UTcsEventBusSubsystem*);
	int32 UnregisterRowsBySource(const FTcsSourceHandle&, UTcsEventBusSubsystem*);
	void Reset(UTcsEventBusSubsystem*);
	int32 GetRowCount() const;
	void CollectRowsForTag(FGameplayTag, TArray<FTcsEffectTriggerHandle>&) const;
	const FTcsEffectTriggerInstance* FindRow(FTcsEffectTriggerHandle) const;
	void SortRowsByPriority(TArray<FTcsEffectTriggerHandle>&) const;
	void SetGateTagLit(FGameplayTag, bool);  bool IsGateTagLit(FGameplayTag) const;
	void SetRandomSeed(int32);  double NextRandomValue();
	void AddReferencedObjects(FReferenceCollector&, UObject* ReferencingObject);   // GC 补引用
private:
	TArray<FTcsEffectTriggerInstance> Rows;   TArray<uint32> RowGenerations;   TArray<uint32> FreeSlots;
	TMap<FGameplayTag, FTcsEventSubscriptionHandle> TagSubscriptions;   // 同 Tag 共用一个订阅
	TSet<FGameplayTag> LitGateTags;   FRandomStream RandomStream;
	TWeakObjectPtr<UTcsTriggerEvaluator> Evaluator;
};

// —— 载荷读取器（实施时追加）——
USTRUCT() struct FTcsTriggerPayloadInfo { FTcsCombatEntityHandle Caster; TArray<FGameplayTag> ClassificationTags; };
using FTcsTriggerPayloadRead = TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct& Payload)>;
class TCSEFFECT_API FTcsTriggerPayloadReaderRegistry { /* AddPending / Register / Find，与条件注册表同构 */ };
#define UE_DECLARE_TRIGGER_PAYLOAD_READER(ReaderFn) ...
#define UE_DEFINE_TRIGGER_PAYLOAD_READER(PayloadType, ReaderFn) ...

// —— 门面新增公共面（带导出宏）——
FTcsEffectTriggerHandle RegisterTriggerRow(const FTcsEffectTriggerInstance&);
bool UnregisterTriggerRow(FTcsEffectTriggerHandle);
int32 UnregisterTriggerRowsBySource(const FTcsSourceHandle&);
void SetTriggerGateTag(FGameplayTag, bool);  bool IsTriggerGateTagLit(FGameplayTag) const;
int32 GetTriggerRowCount() const;   void SetTriggerRandomSeed(int32);
```

- [x] **Step 1: OpenSpec 提案**（`effect-trigger`：MODIFIED × 1 + ADDED × 3）——`add-effect-trigger-registry`，已归档
- [x] **Step 2: 实施求值器 + 登记表 + 点灯 API**（+ 载荷读取器 + ARO）
- [x] **Step 3: 编译验证**（Development 零警告；**Shipping 也补跑**——新增 `UCLASS`/`UPROPERTY` 面）
- [x] **Step 4: 定向人工检查**——依赖面零领域模块 ✅；订阅计数配对自检 ✅；代际校验自检 ✅

> **实施注记（必读；原注记保留，实施结论附于各项之后）**：
> - **求值顺序 MUST 严格照 04 §3 的"四道门"**：`事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`。顺序有意义：`ExecutionGate`（网络闸）最廉价先判；`Conditions` 最贵最后判。**✅ 已按此实现**（求值器四个私有函数一一对应）。
> - **`Priority` 是"大者先"**。同 `Priority` 时**按登记序**。**✅ 已实现**：`SortRowsByPriority` 显式以 `(Priority 降序, 槽位下标升序)` 作全序（快排不稳定，故不依赖输入序）。
> - **订阅计数配对**：同一 `EventTag` 的多行**共用一个订阅**。**✅ 已实现**：`TagSubscriptions` 是 `TMap<FGameplayTag, 句柄>`（天然一 Tag 一条）；`EnsureSubscription` 首行判重；`DropSubscriptionIfUnused` 首行 `HasRowForTag` 提前返回。**行数经扫描登记表得出**（而非维护行句柄索引表——后者会与真相同步漂移）。
> - **起链装配**：`FTcsEffectContext{Caster = 行上下文解析, EventPayload = 原事件载荷, Targets = 空}` → `ExecuteChain(...)`。**✅ 已实现**。**`Caster` 的解析规则已改**（原注记的规则不可实现，见上文错误 1）——改走载荷读取器。
> - **链未登记时**：`ExecuteChain` 已有拒绝面，本轮**不重复校验**。**✅ 遵守**（求值器注释明文）。
> - **`bConditionMissIsSilent`**：`true` = 静默跳过；`false` = 记 `Verbose`（**MUST NOT 用 Warning/Error**）。**✅ 已实现**。
> - **`UTcsTriggerEvaluator` 的生命周期**：由门面在首次登记时 `NewObject` 创建并 `UPROPERTY` 持有。**✅ 已实现**（懒建在 `RegisterTriggerRow` 内）。门面 `Deinitialize` 时清空登记表并退订全部。**✅ 已实现**。
> - **GC 补引用**：**❌ 原判据错误，已纠正**（见上文错误 2）——值语义**也要**补。
> - **订阅通道 = 立即**（原注记未写，实施期从 Task 3 的时序约束反推得出）：收集协议要求"修正提交落在事件发布返回之前"。**✅ 已实现**（`EED_Immediate`）。
> - **`TcsEffectSubsystem.cpp` 行数**：实施前已 289 行，逼近 300 行上限——故登记表拆为纯逻辑类、门面触发 API 拆到 `TcsEffectSubsystem_Trigger.cpp`。**✅ 已按仓规拆分**（所有 `.cpp` ≤ 296 行）。

> **实施注记（保留原注记的其余部分）**：
> - **求值器内部访问面**：门面以 `friend class UTcsTriggerEvaluator` 开放最小集（收集行/排序/解析/取随机值/取总线），**MUST NOT** 把登记表本身暴露为公共面（外部只该经门面 API 动行）。
> - **持有形态**：登记表用**值语义 `TArray` + 索引句柄**（不引入 `TTcsInstancePool` 类型），**但**必须自持代际计数（见上文错误 3）与 GC 补引用（见上文错误 2）。**代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有行指针**——求值器每次按句柄重解析（与解释器"每步入器前重解析"同款纪律）。

---

## Task 3: `ModifyFlow` 链原语（伤害修改器通道的提交侧）

**Files:**
- Create: `Source/TcsDamage/Public/Chain/TcsStepModifyFlow.h`
- Create: `Source/TcsDamage/Private/Chain/TcsStepModifyFlow.cpp`

**Interfaces:**
- Consumes: Task 2 的触发行（**不直接依赖**——经 `FTcsEffectContext::EventPayload` 通信）；`FTcsFlowAttributes::Submit`（已存在）；`FTcsDamageFlowCollectEvent`（已存在）。
- Produces:
```cpp
/**
 * ModifyFlow 链原语（D4-3 战斗组；09 §2.3"伤害修改器唯一通道"的提交侧）。
 *
 * 用法：状态/装备的触发行订阅流程收集事件 → 命中后起一条**单步链**（本步骤）→
 * 本步骤从 `Context.EventPayload` 取回流程上下文 → 向黑板提交修正。
 * "一条修改器 = 触发行 + 单步链"（09 §2.3 创作糖；M8 将提供模板自动生成）。
 *
 * **流程上下文如何到达链**：经 `FTcsEffectContext::EventPayload`（**已就位字段**）——
 * 收集事件载荷 `FTcsDamageFlowCollectEvent` 原样装进链上下文，本步骤在 TcsDamage 内部
 * 解出 `FTcsDamageFlowContext*`。**TcsEffect 全程不认识任何 TcsDamage 类型**（依赖方向不破）。
 */
USTRUCT()
struct TCSDAMAGE_API FTcsStepModifyFlow
{
	GENERATED_BODY()

	// 目标黑板键（项目词表或契约键 `DamageFlowKey.*`；空 = 落契约键 BaseDamage）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FGameplayTag TargetKey;

	// 运算带（带序唯一真相在 Op；折叠走共享纯函数）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 操作数（PV 载体；Literal 直配，ParamRef 随其来源策略轮）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FTcsParamValue Operand;

	// 消耗策略（D7-4：候选按 SortKey 裁决选一、成功执行才消费；未选中者完全不动）
	// **2026-09-30 用户拍板**：类型改名 FTcsDamageModifierConsumePolicy + 去掉 TFunction（改事件语义）
	//   —— 不先做这一步，本字段无法标 UPROPERTY（UHT 报错），本 Task 编不过。
	//   真相源 = 提案 `add-damage-modifyflow-primitive`（含形状、事件 tag、验收面）。
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FTcsDamageModifierConsumePolicy Consume;
};
```

- [x] **Step 1: OpenSpec 提案**（`damage-step-library` MODIFIED：追加 `ModifyFlow` 链原语需求；`damage-primitive` 视需要）——**落地形态与计划不同**：链原语需求实际落在 **`damage-primitive`（ADDED × 3）**（`FTcsStepModifyFlow` 与 `FTcsStepDamage` 同族、本就住该能力）；`damage-step-library` 只做 MODIFIED × 1（过期理由改写）；另补 **`damage-flow` MODIFIED × 1** 与 **`damage-primitive` MODIFIED × 1**（第四份 delta，见实施注记④）
- [x] **Step 2: 实施 struct + 执行器 + 宏注册**（`Public/Chain/TcsStepModifyFlow.h` + `Private/Chain/TcsStepModifyFlow.cpp`；另补 `Private/Flow/TcsDamageFlowCollectEvent.cpp` = 载荷读取器登记）
- [x] **Step 3: 编译验证**（Development）——**实际双配置**：`LegendAutoChessEditor Win64 Development` + `LegendAutoChess Win64 Shipping` 均 `Succeeded`，**0 error / 0 warning**
- [x] **Step 4: 定向人工检查**——跨模块自注册可查（`UE_DEFINE_EFFECT_STEP_EXECUTOR` 生效）；依赖面零新增边——**两半均 ✅**（2026-10-03 复核；**原措辞"待用户执行"已改按 agent 复核口径**）：①自注册 = 日志「`步骤执行器登记表已解析（共 4 类）`」（`Saved/Logs/LegendAutoChess_2.log:2414`）+ `scripting-language-ustruct` §7.5 执行器计数 **3→4**（那 +1 = `FTcsStepModifyFlow` 的自注册）；②依赖面 = `22133be` 文件清单仅 9 个 `Source/TcsDamage/**`，**未动任何 `Build.cs` / `.uplugin`**

> **2026-09-30 实施注记（Task 3 落地记录；原注记保留于下）**：
> ① **交付面** = 新增 3 文件 + 改 6 文件（清单见 `implementation-log` 2026-09-30 条）；**`TcsEffect` 零改动**（`git diff --stat -- Source/TcsEffect` 为空）、**`Build.cs` 零新增依赖**。
> ② **★ 计划里没预料到的阻塞**：本 Task 的 sketch 写 `UPROPERTY … FTcsConsumePolicy Consume`，而 `FTcsConsumePolicy` 含 `TFunction OnConsumed` ⇒ UHT 阶段即失败。**且光去闭包不够**——非 `USTRUCT` 类型一样进不了 `UPROPERTY`（UHT 报 not supported）⇒ 消耗策略必须**同时升格为反射结构**。用户裁定**方案 A（事件语义）** + 改名 `FTcsDamageModifierConsumePolicy`；消费**动作**仍归台账 `DAMAGE-4`（R5/M4a）。
> ③ **消费事件 tag 钉名（用户拍板）**：`TcsEvent.Damage.ModifierConsumed`——本批**只声明、不发布**（发布归 `DAMAGE-4`）。
> ④ **补了第四份 delta**（`damage-primitive` MODIFIED「流程上下文反射视图」）：该生效需求原文以"提交项深处嵌 `OnConsumed` ⇒ **物理不可反射**"为理由，而该事实随本批消失；不补则 `archive` 合入后生效规格会残留一句**已被证伪**的论断。**同类修正**本提案已给 `damage-step-library` 做过一次。
> ⑤ **顺带扫掉同类过期论断**（活动文档）：`dec-04`（追加修订注记 3）、`RSCH-csharp-authoring` 与 `RSCH-replication`（状态 `FROZEN` ⇒ 按 `docs-convention` §7 只**追加实施注记**、正文不改——其 `G-1b`「必做前置…否则物理不可能」降为"常规工作量 + 排期"）。
> ⑥ **仍待办**：Step 4（人工检查，用户执行）→ 提案归档 + `openspec validate --specs --strict`；**提交需用户明确授权**。

> **Step 4 的可执行面（2026-09-30 实测盘点；用户裁定 4.3/4.4/4.5 归 Task 4）**：
> - **夹具现状**：全仓 `RegisterTriggerRow` / `FTcsEffectTriggerInstance` 在 `TcsEffect/` 之外**使用点为零**（无任何触发行夹具、无触发行内容）；Task 2.5 的 `UTcsEffectTriggerDefAsset` 载体未实现 ⇒ **触发行今天只能 C++ 注册**。插件侧 `Private/Testing/` 当前为空；宿主侧 `Source/TcsDev/` **已入库**（LAC 17 文件，含 `TcsDevSliceRig` 与 `Tcs.Test.Slice.Run` / `.Reject` 命令对）——**装置先例 = 宿主侧入库**（归属仍留 Task 4 定）。
> - **4.3 / 4.4 / 4.5 需新装置**（触发行 + 单步链 + `AfterDamage` 订阅），即本 Task 4 Step 2「装置扩展」的活 ⇒ 已改归 Task 4。
> - **4.1 机制面已证（UHT 差分，不需要编辑器）**：三字段旗标字 `0x0010000000000001`——与既有、已被现存链资产持久化的 `FTcsStepDamage::{FlowTemplateId, DamageBase, TargetAttrKey}` **逐位相同**；`Consume` 是 `FStructProperty` 且 struct getter = `Z_Construct_UScriptStruct_FTcsDamageModifierConsumePolicy`；**两配置 UHT 产物 SHA256 逐字节相同**。
> - **4.2 + 4.5 可由一条命令验**：在 `DA_SliceChain` **临时**加一个 `TcsStepModifyFlow` 步 → PIE → `Tcs.Test.Slice.Run`。判据——`检查 3` 步数 3→4 仍 PASS（反射面活着）；出现**恰好一条** `ModifyFlow[…]: 链上下文无流程收集载荷（EventPayload 为空或类型不符）——本步按完成处理` Warning（**= 4.5**，`LogTcsDamage` 默认等级 `Log` ⇒ 必然可见）；`检查 7a`/`7b` PASS 且扣血仍 45（**7b PASS 即 4.2**——未注册会按"未知步骤类型"断链；同时证明降级路径**零副作用**）。检查 7 的 `FTcsEffectContext` 只填 Caster/Instigator/Targets（`TcsDevSliceRig.cpp:479-483`），`EventPayload` 天然为空 = 降级条件**现成**。**探测后 MUST 删掉该步并保存**——否则常规命令永久吐 Warning，把"零红字"这个验收信号弄哑。
> - **⚠️ 同一次 PIE 重跑会"数字漂"，但那不是 FAIL**（**2026-09-30 修正一处我自己的误判**：初稿按屏显格式推断"重跑会因血量钳位假 FAIL"——**错**）：`检查 7b` 的判据只有 `ActualDamage > 0.0`（`TcsDevSliceRig.cpp:525`），**不是**判等于 `ExpectedDamage`。真正的重跑影响是——`检查 6` 每次扣**目标**血、`检查 7` 每次扣**施法者**血（链上 `Self` 选择器，`:476-477`），两个血池互不干扰；所以重跑时屏显的 `Health before/after` 与"实际扣"会逐次变化，但 `检查 7b` 仍 PASS，**除非施法者血已被打到 0**。要对照的稳定量是同一行里的"链资产配置的 `DamageBase`"（不随重跑变化）。**别把"数字变了"当缺陷**。想干净复现就先 Stop 再 Play。

> **提交清单与三条日志（2026-09-30 备好，**用户裁定暂不提交**；授权后直接照用）**：
> **分区已验证**——TCS 工作区 **27** 个变更 = 源码 9 / 文档 **14** / OpenSpec 4，**零重复、零漏项、零幻影**（脚本对齐 `git status --porcelain` 逐项核过）。**每个文件只出现在一个提交里**（拆分的单位是文件，不是内容主题——教训卡 `MEM-20260930-03`）。
> **① 源码（9）**：`Source/TcsDamage/` 下 3 新增（`Public/Chain/TcsStepModifyFlow.h`、`Private/Chain/TcsStepModifyFlow.cpp`、`Private/Flow/TcsDamageFlowCollectEvent.cpp`）+ 6 修改（`Public/Flow/TcsFlowAttributes.h`、`Private/Flow/TcsFlowAttributes.cpp`、`Public/Flow/Steps/TcsFlowDataSteps.h`、`Public/Flow/TcsDamageFlowContextView.h`、`Public/Flow/TcsDamageFlowCollectEvent.h`、`Private/TcsDamageSubsystem.cpp`）
> **② 文档（14）**：`spec/09-module-damage.md`、`spec/tcs-contract-traceability.md`、`evidence/2026-09-30-modifyflow-primitive-acceptance-pie.md`、`GLOSSARY.md`、`INDEX.md`、`decisions/dec-04-callback-carriers.md`、`ledger/deferred-inputs-ledger.md`、`ledger/reflection-backlog.md`、`log/decisions-log.md`、`log/implementation-log.md`、`plans/plan-r4-trigger-row.md`、`research/csharp-authoring.md`、`research/replication-posture.md`、`research/scripting-language-ustruct.md`
> **③ OpenSpec（4）**：`openspec/specs/{damage-primitive,damage-flow,damage-step-library}/spec.md` + `openspec/changes/archive/2026-09-30-add-damage-modifyflow-primitive/`
>
> **提交 ① 日志草案**：
> ```
> 【ADD】TcsDamage + ModifyFlow 链原语（伤害修改器通道提交侧）+ 【MOD】消耗策略纯数据化
>
> 【ADD】Public/Chain/TcsStepModifyFlow.h + Private/Chain/TcsStepModifyFlow.cpp
> - FTcsStepModifyFlow{TargetKey, Op, Operand, Consume}，四字段全 UPROPERTY（链资产可配）
> - 执行器：从 FTcsEffectContext::EventPayload 解 FTcsDamageFlowCollectEvent → 黑板 Submit → 恒 TSR_Completed
> - 载荷缺失/类型不符 → Warning + 完成（不崩溃、不静默通过、不产生半笔提交）
> - TargetKey 无效 → 执行器内兜底契约键 DamageFlowKey.BaseDamage（不依赖字段默认值）
> - 一行 UE_DEFINE_EFFECT_STEP_EXECUTOR 跨模块自注册进 TcsEffect 注册表
>
> 【ADD】Private/Flow/TcsDamageFlowCollectEvent.cpp
> - 收集事件载荷读取器登记（UE_DEFINE_TRIGGER_PAYLOAD_READER 全库零登记的补齐）
> - Caster ← Attacker、ClassificationTags 直通；空载荷/空 Context → 默认构造
>
> 【MOD】FTcsConsumePolicy → FTcsDamageModifierConsumePolicy，并由纯 C++ struct 升格为 USTRUCT()
> - 去 TFunction OnConsumed（含闭包的结构无法出现在任何 UPROPERTY 上——UHT 报错）
> - 三字段 UPROPERTY(EditAnywhere)：MaxUses / Cooldown / SortKey
> - 光去闭包不够：非 USTRUCT 类型一样进不了 UPROPERTY，故必须同时升格为反射结构
> - 消费行为改事件语义：原生 tag TcsEvent.Damage.ModifierConsumed（只声明不发布，发布归 DAMAGE-4）
>
> 改动面：3 新增 + 6 修改；TcsEffect 零改动（git diff --stat -- Source/TcsEffect 为空）、Build.cs 零新增依赖。
> 验证：LegendAutoChessEditor Win64 Development + LegendAutoChess Win64 Shipping 双配置 0 error / 0 warning；
> 两配置 UHT 产物 SHA256 逐字节相同；运行期 Tcs.Test.Slice.Run 9 PASS / 0 FAIL、全日志零 Error 零 ensure。
> ```
>
> **提交 ② 日志草案**：
> ```
> 【MOD】设计文档回写：ModifyFlow 落地 + 消耗策略事件语义 + 三处同类过期论断
>
> - spec/09-module-damage.md：§2.2 表/§2.3/§2.4/§3 四处 OnConsumed 口径同步；§2.4 视图「不含黑板」理由改为嵌套容器+运行态记录；顺带按源码校正 §2.2 数据步骤字段表（原写 SortKey，实现为 Conditions）；修订记录 v4 增补 3
> - GLOSSARY.md：D7-4 / R-6 两行
> - INDEX.md：SPEC-08-damage / DEC-04 / PLN-R4 三行实现状态
> - ledger/deferred-inputs-ledger.md：DAMAGE-3 部分消费、DAMAGE-4 形状闭合、SCRIPT-3 改判、TOOLS-6 扩展（注册表无枚举面）、TOOLS-2 ③（链资产 IsDataValid 缺「提升为 Valid」段——同族另两处已修）
> - ledger/reflection-backlog.md：R-4 前置由「物理阻碍」改为「常规工作量」、R-6 改为形状已落地、追加变更记录
> - decisions/dec-04-callback-carriers.md：修订注记 3（④ 形状已落地 + 连带效果）
> - research/ 三份 FROZEN 册按 docs-convention §7 只追加实施注记（正文不改）：csharp-authoring（G-1b 降为常规工作量）、replication-posture（闭包依赖面失效）、scripting-language-ustruct（§7.5 执行器计数 3→4）
> - log/decisions-log.md + log/implementation-log.md：裁定落点与验收证据（含 GetPtr 单/双参数语义的引擎行为核实）
> - plans/plan-r4-trigger-row.md：Task 3 实施注记（含 Step 4 可执行面盘点）+ Task 4 承接注记（4.3/4.4 移入 + 夹具缺口）
> ```
>
> **提交 ③ 日志草案**：
> ```
> 【MOD】归档提案 add-damage-modifyflow-primitive → 2026-09-30-add-damage-modifyflow-primitive
>
> - 归档：openspec/changes/archive/2026-09-30-add-damage-modifyflow-primitive/（proposal / tasks / 三份 delta）
> - 生效规格（+3 新增 / ~3 修改，-0）：
>   damage-primitive：ADDED × 3（ModifyFlow 链原语 / 消耗策略为纯数据可反射结构 / 收集事件载荷读取器登记）+ MODIFIED × 1（流程上下文反射视图——过期理由改写）
>   damage-flow：MODIFIED × 1（流程属性黑板——改名、删 OnConsumed 回调位、以源码校正 Submit 签名）
>   damage-step-library：MODIFIED × 1（通用数据步骤——理由改职责划分，禁令不变）
> - 验收：Tcs.Test.Slice.Run 9 PASS / 0 FAIL；人工检查 4.1/4.2/4.5 实证（检查 3 步数 10→11 = 新步骤类型存档后可解析；执行器降级 Warning 恰一条）；4.3/4.4 连同 4.5 命令化移入本 Task（需触发行夹具）
> - openspec validate --specs --strict 24/24；无活动变更残留
> ```

> **实施注记（必读）**：
> - **命名撞车警告**：`TcsDamage` 已有 **`FTcsFlowModify`**（**流程内**步骤，写流程黑板）——那是**流程侧**的；本轮的 `FTcsStepModifyFlow` 是**链侧**原语（由触发行触发）。两者名字相近但分属两层，**头文件注释 MUST 互相指名区分**（否则将来必有人混用）。
> - **⚠️ 载荷通路的确切形状（实施前必须理解，否则会写错）**：`FTcsDamageFlowContext` 是**非反射纯 C++ struct**（`TcsDamageFlowContext.h:22` 明文"纯运行态结构（非反射）"），**装不进 `FInstancedStruct`**。通路靠的是既有的 `FTcsDamageFlowCollectEvent`——它是**反射 USTRUCT，但成员是裸指针**（`FTcsDamageFlowContext* Context`，非 `UPROPERTY`，`TcsDamageFlowCollectEvent.h:48`）。所以：
>   - 广播侧：`PublishCollectEvent` 把 `&Context` 装进载荷（**已实现**）；
>   - 链侧：本步骤 `Context.EventPayload.GetPtr<FTcsDamageFlowCollectEvent>()` → `->Context` 取回指针 → `Submit`。
>   - **`EventPayload` 是值拷贝**（`FInstancedStruct` 语义），拷贝的是**那个裸指针**——故指针有效性完全依赖"流程仍在执行"。
> - **⚠️ 硬约束：修改器链 MUST NOT 挂起**。`FTcsEffectContext` 随 `FTcsChainRun` 存活（池化、跨帧），若链在 `ModifyFlow` 之前有 `WaitDelay`/`WaitEvent` 等挂起步骤，**流程早已结束、指针悬空**。处置：①本步骤取到指针后**先校验非空**（空 = Warning + 跳过）；②**头文件与规格 MUST 明文**"ModifyFlow 只能用于同步单步链"（09 §2.3 的"一条修改器 = 触发行 + 单步链"本就是同步形态）；③**MUST NOT** 为跨帧场景造句柄化机制（零消费者不预建——真需要时是独立设计）。
> - **取不到流程上下文时**：`Context.EventPayload` 非 `FTcsDamageFlowCollectEvent` 或 `Context` 指针为空 → **Warning + 跳过**（不 ensure：手动触发一条含 ModifyFlow 的链是合法用法，只是改不了黑板）。
> - **`Consume` 的 `OnConsumed` 回调**（`TFunction`）**不可反射**（UHT 实证，`TcsFlowAttributes.h:31`）→ 本 struct 的 `Consume` **不能**作为 `UPROPERTY` 全量序列化。处置二选一：(a) `Consume` 不标 `UPROPERTY`（纯 C++ 面，编辑器不可配消耗策略——与 `FTcsFlowModify` 的既有注释同款限制）；(b) 拆出可反射的数值子集。**本计划采用 (a)**，并在注释写明"编辑器配消耗策略需 C++ 步骤"。
> - **提交时机**：`PublishCollectEvent` 是**立即通道同步派发**（`TcsDamageSubsystem.cpp`），触发行回调内 `ExecuteChain` 同步执行完（无挂起步骤时），故 `Submit` 落在 `PublishCollectEvent` 返回前——**在 `Execute` 步骤读黑板之前**。这是本通道成立的关键时序，注释 MUST 写明。
> - **`Heal` 原语不在本轮**：它需要治疗流程模板 + `IHealFlowDelegate`（09 §2.4"同骨架精简版"），是独立一块。**台账 DAMAGE-3 部分消费**（ModifyFlow 已落，Heal 留 R5）。

---

## Task 3.5: 追加 4 个链原语（`SetVar` / `Branch` / `RunSubChain` / `WaitEvent`）

**Files:**
- Create: `Source/TcsEffect/Public/Chain/TcsStepSetVar.h` + `Private/Chain/TcsStepSetVar.cpp`
- Create: `Source/TcsEffect/Public/Chain/TcsStepBranch.h` + `Private/Chain/TcsStepBranch.cpp`
- Create: `Source/TcsEffect/Public/Chain/TcsStepRunSubChain.h` + `Private/Chain/TcsStepRunSubChain.cpp`
- Create: `Source/TcsEffect/Public/Chain/TcsStepWaitEvent.h` + `Private/Chain/TcsStepWaitEvent.cpp`
- Modify: `Source/TcsEffect/Public/Chain/TcsChainRun.h`（`WaitEvent` 需挂起锚：一次性订阅句柄）
- Modify: `Source/TcsEffect/Public/TcsEffectSubsystem.h`（`WaitEvent` 需"事件匹配唤醒源"入口）

**背景（为什么本轮追加）**：触发行落地后"事件 → 条件 → 链"就通了，但链里只有 3 个原语（`WaitDelay`/`SelectTargets`/`Damage`）——**能触发却做不了什么**。本批四个**零新依赖**（或仅需扩唤醒源），与触发行同轮交付最经济（2026-09-23 用户拍板）。

**Interfaces:**
```cpp
// ① SetVar（D4-3 元原语）：写链内变量（`FTcsEffectContext::Variables` 字段已就位）
USTRUCT() struct TCSEFFECT_API FTcsStepSetVar
{
	UPROPERTY(EditAnywhere) FGameplayTag VarKey;      // 变量键（项目词表）
	UPROPERTY(EditAnywhere) FTcsParamValue Value;     // 值（PV 载体——可 Literal，可 ParamRef 等）
};  // 即时步骤（TSR_Completed）

// ② Branch（D4-3 控制流）：按条件选分支——条件走**条件注册表**（复用 Task 1 的求值器）
USTRUCT() struct TCSEFFECT_API FTcsStepBranch
{
	UPROPERTY(EditAnywhere) TArray<FInstancedStruct> Conditions;   // 条件（全过 → 走 Then，否则走 Else）
	UPROPERTY(EditAnywhere) FGameplayTag ThenChainId;              // 条件通过时执行的链（空 = 不执行）
	UPROPERTY(EditAnywhere) FGameplayTag ElseChainId;              // 条件未过时执行的链（空 = 不执行）
	UPROPERTY(EditAnywhere) bool bWait = true;                     // 是否等分支链完成（同 RunSubChain 语义）
};  // 分支链走"子链完成唤醒源"

// ③ RunSubChain（D4-3 控制流；2026-09-23 用户确认命名）
USTRUCT() struct TCSEFFECT_API FTcsStepRunSubChain
{
	UPROPERTY(EditAnywhere) FGameplayTag ChainId;   // 要起的链（引用已登记链）
	UPROPERTY(EditAnywhere) bool bWait = true;      // 默认等待子链完成唤醒父；false = 放支线（父继续）
};  // 子链完成 → 唤醒父（JoinCount 机制的雏形）

// ④ WaitEvent（D4-3 控制流）：一次性订阅，命中即退订，Payload 写入运行态
USTRUCT() struct TCSEFFECT_API FTcsStepWaitEvent
{
	UPROPERTY(EditAnywhere) FGameplayTag EventTag;   // 等哪个事件（空 = 立即完成并 Warning）
	UPROPERTY(EditAnywhere) double TimeoutSeconds = 0.0;  // 超时（0 = 不超时；>0 时走到期堆）
};  // 首入：订阅 + 返回 TSR_Running；命中/超时 → 唤醒重入 → 退订 + 完成
```

- [x] **Step 1: OpenSpec 提案**（`effect-chain` MODIFIED：追加四个原语需求；`effect-interpreter` 若唤醒源契约需改则一并；**+ `targeting-strategy` MODIFIED：并入 P-A 排序相位——见下方"同批并入"注记**）
- [x] **Step 2: 实施**——按"零前置 → 有前置"顺序：`SetVar` → `RunSubChain` → `Branch` → `WaitEvent`；**P-A 排序相位（`targeting-strategy`）** 与本批同步实施
- [x] **Step 3: 编译验证**（Development；`WaitEvent` 改 `TcsChainRun` 结构，须补 **Shipping**——结构改动是 Shipping 能照出的类型；P-A 搭同一次 Shipping 顺风车）——**Development + Shipping 双 `Succeeded`、0 error / 0 warning**（2026-10-03）
- [x] **Step 4: 定向人工检查**——依赖面零领域模块；**熔断自检**（`RunSubChain` 递归起链 + `Repeat` 类自激——`MaxStepsPerFrame` 是否够）；**P-A 追加检查**：排序项在链资产上可配、并列时稳定键决胜可复现（同一输入两次跑结果集一致）——**2026-10-04 PIE 实跑全过**：依赖面零领域模块 ✓；熔断自检 = 17 层起链 / 深度 **17** 熔断 / **16 层父链回卷完成** ✓；P-A 追加 = 去重 1 · NaN 排除（取前 2 而非 3）· 距离并列时稳定键 `Id` 升序决胜（伤害落在先 spawn 的施法者）· 同命令两次**逐字一致** ✓。证据 = `EVID-2026-10-04-chains-primitives`。**一处留白**：`tasks.md` 的 6.6（三条降级路径）未跑——装置无法在不改代码的前提下撤掉时钟/总线（详见该证据文档末节）

> **实施注记（必读）**：
> - **⚠️ `WaitEvent` 是本批最难的一个**：它需要**新增唤醒源**（`04 §2.4` 四种唤醒源之"事件匹配"）。`WaitDelay` 已实证"到期堆唤醒"路径（`PendingExpiry` 锚 + 代际校验重入），`WaitEvent` 需要同款的"订阅句柄锚"——`FTcsChainRun` 要加字段（建议 `FTcsEventSubscriptionHandle PendingSubscription`），且**退订时机**必须覆盖三条路径：①事件命中；②超时（若配）；③运行态被释放（`ReleaseRun` 必须退订——否则订阅泄漏且回调打到已回收的运行态）。
> - **`Branch` 与 `RunSubChain` 共用"子链完成唤醒源"**：先做 `RunSubChain`（单一形态），`Branch` 的 `bWait` 直接复用其机制。**MUST NOT** 为两者各写一套唤醒逻辑。
> - **递归深度**：`RunSubChain` 允许子链再起子链——**已有的 `MaxStepsPerFrame` 熔断只管"单帧步数"，不管"嵌套深度"**。本批 MUST 明确：嵌套深度靠什么护栏（建议复用熔断计数——子链步数计入父链的帧内步数），并在注记里写明实测值。
> - **`SetVar` 的 `Value` 用 `FTcsParamValue`**：与链步骤其它数值字段一致（PV 载体，可 Literal/ParamRef）——**MUST NOT** 另造一个 double 字段（那会让"变量只能用字面量"成为隐式限制）。
> - **`Branch` 的条件复用 Task 1 的条件注册表**：`Conditions` 求值走 `EvaluateTriggerConditions`——但**上下文不同**（那是 `FTcsTriggerContext`，需要 `ClassificationTags`）。链侧没有该上下文，故本批要么①构造一个最小 `FTcsTriggerContext`（从 `FTcsEffectContext` 映射），要么②为链侧条件另立签名。**建议①**（一套条件类型两处可用，零重复），实施时确认映射字段（`EventTag` 取运行态记录的上次事件、`ClassificationTags` 从哪来——**若链侧无来源则本批 `Branch` 只支持不依赖分类标签的条件**，并在注记写明）。

> **同批并入：P-A 排序相位（2026-09-30 用户拍板）**
>
> **来源**：[`RSCH-targeting-absorption`](../research/targeting-abilitykit-absorption.md) 正文 §2.1 / §2.2 / §2.13 / §2.6 / §2.10（AbilityKit 目标查找经验吸收），落地判据见该文档 附录 A10.3.1。
>
> **并入理由（改动点复用，不是"越早越好"）**：本 Task 本来就要 ①写 OpenSpec 提案、②因 `WaitEvent` 改 `TcsChainRun` 结构、③必须补跑 Shipping——P-A 搭这三点顺风车。反之若本 Task 落地后单独做 P-A，规格要改第二次、**既有链资产要按新排序语义复核**（检查点 6 的"零 C++ 加链"资产）、Shipping 再跑一次。
>
> **P-A 最小集合**（MUST NOT 扩大）：①排序契约——有序比较项（Scorer + 方向）+ 严格字典序 + 全等后**稳定键（句柄 Index/Generation）升序决胜**；②浮点边界——任一排序项返回 NaN ⇒ 候选排除，±Inf 保留参与排序；③确定性纪律补进 `10 §3`；④**选择器产出去重**（当前完全无去重——宿主候选来源重叠时下游会对同一目标打两次，这是本轮顺带补掉的无人防守缺口）；⑤复用缓冲（执行器/预览共用 scratch，不引入池化基础设施）。
>
> **MUST NOT 并入**：Top-K 的流式实现、候选来源组合器、空间索引、`ITargetMapper`——均无消费者（理由见设计文档 §2.3 / §2.7 / §2.8 / §2.9）。
>
> **交付面（2026-09-30 用户拍板后的最终形态）**：
> 1. **唯一内置评分器 = 距离**（只依赖 `ITcsEntityQuery::GetLocation`，**零宿主语义**）；
> 2. **排序契约**——有序比较项（Scorer + 方向）+ 严格字典序 + 全等后**稳定键决胜**；
> 3. **评分器宿主插槽**（`ITcsTargetScorerHost` + 转发器，拟名·未实现）——**本批必须补的第三个插槽家族**（选择器/过滤器两个已在 SCRIPT-8 落地）；否则"血量最低"这类条件无处安放，宿主只能去写 C++ 策略类型。规格 SHOULD 注明"排序评分优先 C++ 策略、脚本插槽用于低频/非关键排序"（评分调用频次是"逐候选 × 逐排序项"，高于过滤器，跨语言反射在热路径上不便宜）；**★ 本项与 R4.5-b / R4.5-c 同形、可并批**——取舍见 `LEDGER-reflection`《★ 宿主插槽家族的耦合关系》；
> 4. **选择器产出去重**；5. 复用缓冲 + 浮点边界 + 确定性纪律。
>
> **内置边界规则（用户 2026-09-30 原则，写进规格）**：*"TCS 属于可复用插件，理论上不应该直接干涉宿主项目生命值属性的配置方法。"* ⇒ 泛化为——**凡是需要宿主词汇的内置策略，插件一律不提供**（判据 = 该策略引用了什么词汇：只引用框架自身契约 ⇒ 可内置；引用宿主属性名 / Tag 词表 / 阵营关系 ⇒ 归宿主）。这与 `10 §2.2`"框架零默认 Filter"（怎么算存活、怎么算敌人是宿主语义）同源。**据此本文初稿那条"评分器契约向 `FTcsParamValue` 参数源体系对齐"的建议作废**——它等价于让 TCS 认识 `AttributeScaled` 这类宿主侧属性源，正是本规则禁止的事。
>
> **前置条件**：**已满足**（用户 2026-09-30 确认"距离"是唯一需要插件内置的条件）。策划的技能选择条件清单降级为非阻塞项——用途改为"验证宿主侧属性量/状态量/关系量条件的覆盖面是否够，尤其关系量是否需要扩 `ITcsEntityQuery` 面"。
>
> **验收追加**：排序项在链资产上可配（零 C++）；并列时稳定键决胜可复现（同输入两次跑结果集一致）；NaN 评分的候选被排除而非排末尾。

> **实施注记（2026-10-03 落地；真相源 = 提案 `add-chain-primitives-and-target-sorting`）**
>
> **落地范围**：四个原语 + P-A 排序相位全部落地，**Development 编译 0 error / 0 warning**（Shipping 与人工检查见 Step 3/4 未勾项）。**偏离原计划的 7 处**逐条住提案 `proposal.md` 的「与计划的偏离」表；设计决策 D-1 ~ D-10 住同提案 `design.md`——本注记只记**计划原文被推翻或补空**的三点：
>
> - **① 计划建议的"订阅句柄锚"（`FTcsEventSubscriptionHandle PendingSubscription`）未采用**——改为**共享订阅 + 门面等待表**：同一 `EventTag` 的 N 条等待**共用一条总线订阅**（计数配对），运行态只持"在等哪个 tag"。三条依据：总线 `Subscribe` 只收 `UTcsEventHandler*`（传不了 lambda）且**派发不传订阅句柄**（Handler 无法自辨身份）；`FTcsTriggerRegistry` 已有明文纪律"同一 Tag 多行共用一条订阅…MUST NOT 每行各订一次"；每等待一条订阅会让同一回调被打 N 次、收益为零。**代价**：多一张表（`FTcsChainEventWaitRegistry`）+ 一个 `UPROPERTY` 锚定的共享 Handler。
> - **② 嵌套深度的护栏 = 两条，且深度上限是固定常量（= `16`）**——计划原文只问"靠什么护栏（建议复用熔断计数）"。实施结论：**共享预算**（子链步数计入父链本次进入）拦自激，**另加固定常量深度上限**拦调用栈——因为预算值由链定义给出、**作者可配到 4096**，不能作为栈深度的唯一护栏。常量住 `Source/TcsEffect/Private/TcsEffectSubsystem_Run.cpp:TcsMaxChainNestingDepth`，**MUST NOT** 做成链定义字段（它是调用栈护栏，不是行为语义）。**✅ 实测已回填（2026-10-04，PIE 实跑）**：自激链起链 **17 层**后于**深度 17** 熔断（判定式 `ChainEntryDepth > 16`），累计步数 **17**（每层 1 步 —— 共享预算 64 **未触发** ⇒ "深度护栏先撞"的设计预期成立）；随后 **16 层父链被逐一唤醒并走完**（17 起 − 1 熔断 = 16 完成）⇒ "异常结束同样唤醒父、不留死链"实证。两次独立实跑结果一致。证据见 `EVID-2026-10-04-chains-primitives`；台账 `CHAIN-3` 已消费。
> - **③ `Branch` 在链侧只支持不依赖 `EventTag` / `ClassificationTags` 的条件**——计划的"实施时确认映射字段"落了实：`FTcsEffectContext` **无事件 tag 字段**、也无分类标签来源 ⇒ 链侧 `FTcsTriggerContext` 只填 `Caster`，另两字段恒空 ⇒ 依赖它们的条件在链侧**恒不通过**（如 `HasAllTags`）。**这是明示接受的限制**（写进了 `TcsStepBranch.h` 头注释、`SPEC-06-targeting` 与提案规格），自然补法 = 起链时把触发事件 tag 记进运行态；欠账登记台账 `CHAIN-1`。
>
> **本批顺带交付的两件事（不在原计划 Files 列表里）**：
> - **`TcsEffectSubsystem.cpp` 按 `TcsEffectSubsystem_<Feature>.cpp` 先例四拆**（原文件 679 行 → `TcsEffectSubsystem.cpp` 213 / `_Run.cpp` 279 / `_StepProtocol.cpp` 92 / `_RunAccess.cpp` 123，另 `_ChainWait.cpp` 128 为本批新增；单文件全部 ≤ 300 行）。**注意**：拆出的三个新 `.cpp` 首次编译会被 UBT 的源文件目录扫描缓存漏掉（表现为链接期一堆 `LNK2019`，而 unity 清单里根本没有它们）——删 `Intermediate/Build/SourceFileCache.bin` 后重建即恢复。
> - **窗口/配平纪律成文**：`TcsChainRun.h` 补"嵌套起链共用预算 + 深度上限"两条纪律与三个挂起锚的配平规则（锚由步骤装/配平、门面唤醒时不清锚、"运行态释放即解锚"是唯一无条件解锚点）。
>
> **台账**：新增 `CHAIN-1` ~ `CHAIN-5`（五条实施期发现的边界）；`DAMAGE-2` 部分消费（四个原语已落地，余 `Repeat`/`Parallel`/`OnError` + `ModifyAttribute`）。

---

## Task 2.5: 触发行独立资产载体（`UTcsEffectTriggerDefAsset` + DefLibrary 发现装配） —— **已完成（2026-10-04）**

> **本 Task 标题原写 `UTcsEffectTriggerDef`（无 `Asset` 后缀），与本文 Files/Interfaces 块自相矛盾**——实施期由 UHT 判了：`Asset` 形态胜出（理由与实测报错见下方实施注记①）。

**Files（实际落点）:**
- Create: `Source/TcsIntegration/Public/Trigger/TcsEffectTriggerDefAsset.h` + `Private/Trigger/TcsEffectTriggerDefAsset.cpp`
- Modify: `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` + `Private/TcsDefinitionSubsystem.cpp`（加一条发现路径 + 装配 + ARO + 清理）
- Create: `Source/TcsIntegration/Private/TcsDefinitionSubsystem_Trigger.cpp`（**实施时追加**——主 `.cpp` 已 176 行，按仓规 `<Name>_<Feature>.cpp` 分片；两文件 246 / 92 行，均 ≤ 300）

**为什么住 TcsIntegration**：链资产 `UTcsEffectChainDef` 就在那里——**资产载体类需要同时看到"定义类型"（TcsEffect）与"DefLibrary 的发现机制"（TcsIntegration）**，而 TcsIntegration 是唯一依赖 TcsEffect 的上层。放 TcsEffect 会形成反向依赖。

**Interfaces:**
```cpp
// 触发定义资产（Def 资产族统一约定：显式 PrimaryAssetType + 覆写 GetPrimaryAssetId + IsDataValid）
UCLASS(BlueprintType)
class TCSINTEGRATION_API UTcsEffectTriggerDefAsset : public UPrimaryDataAsset
{
	static const FPrimaryAssetType PrimaryAssetType;   // 值 = 类名 "TcsEffectTriggerDefAsset"
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag TriggerTag;   // 内容身份（非资产名）
	UPROPERTY(EditAnywhere) FTcsEffectTriggerDef Def;                     // 定义（纯配置）
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;           // [PrimaryAssetType, TriggerTag.GetTagName()]
#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext&) const override;  // TriggerTag 有效 + 定义非空
#endif
};

// DefLibrary 新增（与 DiscoverChainDefs 同款）
void DiscoverTriggerDefs();                                  // AssetRegistry 按类扫描
const FTcsEffectTriggerDef* ResolveTriggerDef(FGameplayTag TriggerTag) const;
```

- [x] **Step 1: OpenSpec 提案**（`integration-entity` MODIFIED：加触发定义资产的发现与解析；`effect-trigger` 若需则补"资产载体"需求）——**✅ 落地为提案 `add-effect-trigger-def-asset`**（`integration-entity` MODIFIED + **新能力 `effect-trigger-asset`** + `gameplay-tag-governance` MODIFIED × 3；`effect-trigger` 未动——规格早已把"定义加载期登记"写成事实，本批只是把它实现）
- [x] **Step 2: 实施**（资产类 + DefLibrary 发现/缓存/装配）——**✅ 另加"装配为触发行"这一步**（见实施注记②）
- [x] **Step 3: 编译验证**（Development + **Shipping**——含 `IsDataValid` 的 `WITH_EDITOR` 面，Shipping 必跑）——**✅ 双配置 `Result: Succeeded`、0 error / 0 warning**（UHT 侧还带 `-WarningsAsErrors`；Dev 首编译被 UHT 拒一次，见实施注记①）
- [x] **Step 4: 定向人工检查**——依赖面：`TcsEffect` 零反向依赖（资产类住 TcsIntegration）；DefLibrary 的失败清单能报出空 `TriggerTag` —— **✅ 四轮 PIE 全过**：依赖面 grep 零命中；失败清单实证 `触发定义 0 条，失败 1 条` + `触发行 0/0 条`；正路实证规则命中 → 起链 → 挂起 → 唤醒 → 完成；另加"引用链预检"与"可复现 + 跨 PIE 零残留"两轮。证据 = `EVID-2026-10-04-trigger-def-asset`

> **实施注记（2026-10-04 落地；真相源 = 提案 `add-effect-trigger-def-asset`）**
>
> **三处与计划原文的偏离**（逐条住该提案 `design.md` 的 D-11 ~ D-16）：
>
> - **① 类名 `UTcsEffectTriggerDef` 被 UHT 否决 ⇒ 恢复计划的 `…Asset` 形态**（**计划原文反而是对的**，但计划自身标题与 Files/Interfaces 块自相矛盾）。实测报错（UHT 阶段，非编译期）：`Error: Class 'UTcsEffectTriggerDef' shares engine name 'TcsEffectTriggerDef' with struct 'FTcsEffectTriggerDef'`——**UHT 按"去前缀后的引擎名"判重**，`U` 类与 `F` 结构体算同一个名字（与"头文件名必须唯一"是两条独立门槛）。处置：资产类取 `Asset` 后缀，并在 `openspec/project.md` 的「Def 资产命名标准」补限定语（"`<Family>Def` 已被同类型占用时加 `Asset` 后缀"）；**MUST NOT** 反过来改名 Task 1 已交付的数据 struct。
> - **② 计划只写"发现 + 解析"两条面 ⇒ 实施另加"装配为触发行"**（本 Task 的真正业务闭环）：`SeedWorld` 在链装配**之后**逐条 `RegisterTriggerRow`（`Source` = 定义库自持的来源句柄，`Initialize` 时发放一次）。**理由**：`effect-trigger` 规格早已把"定义加载期登记（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"写成事实，而照计划原文落地则资产**零消费者**（策划填了不生效 = 假控件）。另加**引用链预检**：`Def.EffectChainId` 在该世界不可解析 ⇒ **Warning + 仍登记**（否决"跳过登记"与"不校验"，理由见 D-14）。
> - **③ "值语义 struct 无需 ARO"判据错误（本计划下方原注记第 1 条）⇒ 两份缓存都补**：`ChainDefs` / `TriggerDefs` 都是本类的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 走不到，而两者的 `FInstancedStruct` 内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用 ⇒ 不补即静默回收。判据是"**容器是否 GC 可见**"，与值/指针语义无关（**同 Task 2 错误 2 的同一处判据**，第二次犯、第二次纠正）。**链缓存那一半是同批驱动修**。
>
> **顺带做对的一件**（D-16）：新类 `IsDataValid` 在无错时把 `NotValidated` 提升为 `Valid`（基类默认返回 `NotValidated`）；链资产缺这一段属台账 `TOOLS-2` ③，**不在本批修**（避免扩大回归面）。
>
> **同批跨仓落地**：宿主侧新根段 `EffectTriggerDef` + 声明位置条文 + 检查词 `EffectTriggerDef.Check.Seed`，落成 LAC 提案 `add-effect-trigger-def-tag-root`（`host-gameplay-tag-registry` MODIFIED × 2；含"两类验证词"区分：`Probe` 管装置词 / `<功能根>.Check.<名>` 管内容检查词）。根清单由 13 根 → **14 根**，`Validate-GameplayTags.ps1` 跑 **零违规**（采集 37 条 = ini 16 / native 21）。
>
> **本批顺带的一条治理订正**：既有 5 个 `EffectChain.Check.*` 词已在跑，但规格只登记了"2 段"形态 ⇒ 本批把 **`Check` 子段约定**写进规格（并补一条新判据"**根名互不为真前缀**"——`EffectTrigger` 会是 `EffectTriggerGate` 的真前缀，而 BP/CS 动态层支持部分匹配）。
>
> **人工检查**：四轮 PIE 全过（正路+幂等 / 失败面 / 引用链预检 / 可复现+跨 PIE 零残留），证据 = `EVID-2026-10-04-trigger-def-asset`。**留白一条**：`IsDataValid == Valid` 未取到运行期证据（MCP 无校验入口）⇒ 建议 Task 4 扩装置时用 C++ 直接断言（`Tcs.Test.Slice.Reject` 的检查 A 已有同款手法）。

> **实施注记（必读；原文保留，逐条附实施结论）**：
> - **GC 补引用**：DefLibrary 的缓存若用 `TMap<FGameplayTag, TUniquePtr<...>>` 持**定义内容**（值语义 struct）则无需 ARO；但**资产对象本身**必须 `UPROPERTY` 锚定（照 `ChainDefAssets` 的既有做法）——否则重载后资产被回收、`ResolveTriggerDef` 返回的指针指向已释放内容。 —— **❌ 前半句判据错（见上方 ③）**：值语义**也要**补 ARO，两份缓存都补；**后半句（资产 `UPROPERTY` 锚定）已照做并保留**。
> - **内联位（SkillDef/BuffDef）本批不做**：`SkillDef`/`BuffDef` 今天都不存在（M3/M5 未落地）。**本批只做独立资产**，内联位在 R5/R6 给那两个 Def 加 `TArray<FTcsEffectTriggerDef>` 字段时自然成立（定义类型已是纯配置，可直接内联——**这正是 Task 1 分层的收益**）。
> - **"系统级规则"是本资产的业务场景**（用户 2026-09-23 确认）：全局常驻、与任何 Def 无关的触发规则（如"任何单位死亡时触发某链"）。Buff/Skill 的行为走**内联**（施加时注册、Source = 状态实例句柄）。

---

## Task 4: 端到端验收（修改器通道实证） —— **已完成（2026-10-04）**

**Files:**
- Modify: `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（扩展既有 `Tcs.Test.Slice.Run`，**不新增命令**——遵循"暂不添加常驻验收命令"的用户口径，新功能验收并入既有命令）
- Modify: `Content/TcsDev/`（新增一条单步链资产：`ModifyFlow` 半伤）
- Modify: `Config/DefaultGameplayTags.ini`（新增链 id tag）

**验收场景（"破甲"修改器——09 §2.3 的最小实例）**：

1. **准备（三件，均住宿主侧）**：
   - **链资产** `DA_ArmorBreakChain`（`ChainId = EffectChain.ArmorBreak`，**单步** `FTcsStepModifyFlow{TargetKey=DamageFlowKey.BaseDamage, Op=TAO_Mul, Operand=Literal(0.5)}`）；项目 tag 表加 `EffectChain.ArmorBreak`。
   - **新流程模板** `DamageFlowTemplate.Slice_Modifier`（`TcsDevBootstrap` 内注册）——步骤 = `CollectStart → BaseDamage → **PreExecute** → Execute → Completed`。**必须新建**：宿主既有的 `SliceDefault`（4 步）与 `SliceFlow`（2 步自研）**都不含 `PreExecute`**（已核 `TcsDevBootstrap.cpp:226-255`），而 `PreExecute` 是收集修改器候选的那一步（`TcsFlowStepsRest.cpp:155-166`）。
   - **触发行**（装置注册）：`EventTag = TcsEvent.Damage.PreExecute`、`Effects = EffectChain.ArmorBreak`、`Priority = 0`、`bConditionMissIsSilent = true`、`Source = <装置来源句柄>`。
2. **跑流程**：用新模板起伤害流程，`BaseDamageInput` 设一个**非整数值**（如 30——减半后 15，与"原值 30"可区分；**避免用 1.0 这类减半后仍是可猜值的数**）。
3. **断言（三个半边）**：
   - ① **提交生效**：挂行 → 最终伤害 = 基准值 × 0.5；
   - ② **摘行还原**：`UnregisterTriggerRow` → 值还原为基准值；
   - ③ **按来源级联摘除**：`UnregisterTriggerRowsBySource(装置来源)` → 值还原为基准值。

- [x] **Step 1: 建链资产 + 项目 tag + 新流程模板**（资产经 UE MCP 建；**先与用户确认再操作编辑器**——MCP 操作边界纪律；流程模板在 `TcsDevBootstrap` 内注册）——**✅ 建 3 个资产 + 5 个词 + 1 个六步模板**（见实施注记①②）
- [x] **Step 2: 装置扩展**（新增检查：挂行前基准值 / 挂行后减半 / 摘行后还原 / 按来源级联摘除）——**✅ 落地为检查 8–17**（多出：资产路径装配前置、门③对照组、重挂初态、载荷读取器、反向对照、作者侧校验门）
- [x] **Step 3: 编译验证**（Development + **Shipping**——装置含 `WITH_EDITOR` 面时必跑）——**✅ 双配置 `Result: Succeeded`、0 error / 0 warning**（Shipping 侧另确认 `#if WITH_EDITOR` 包裹未留 C4505）
- [x] **Step 4: PIE 实测**（用户执行；**常规命令 MUST 零红字**——预期失败面若需要，独立成 `.Reject`）——**✅ 两轮 `.Run` 各 通过 19 / 失败 0 且关键行逐字一致 + 一轮 `.Reject` 4/0**；证据 = `EVID-2026-10-04-modifier-channel`
- [x] **Step 5: 验收记录回写**（plan3 注记 + README 决策日志）——**✅ 本注记 + 证据文档 + 台账**（`TRIG-1` 消费 / `WAIT-6` 升级 / `DAMAGE-1`·`DAMAGE-3`·`DAMAGE-4` 更新）；**README 决策日志随 Task 5 一并落**（避免同一轮写两遍）

> **实施注记（2026-10-04 落地；真相源 = `EVID-2026-10-04-modifier-channel`）**
>
> **四处与计划原文的偏离**：
>
> - **① 主场景改走内容资产路径**（计划原文写"触发行（装置注册）… `Source` = <装置来源句柄>"）。**原因**：Task 4 的 2026-09-30 承接注记以"`UTcsEffectTriggerDefAsset` 载体未实现 ⇒ 触发行今天只能 C++ 注册"为前提，而 Task 2.5 已把该载体落地 ⇒ 破甲行改由 `DA_Check_ArmorBreakTrigger`（`UTcsEffectTriggerDefAsset`）经**定义库装配**成形，**零 C++ 注册**即生效（实测：`发现触发定义资产 2 个` → `触发行 2/2 条`）。**收益**：验收走的是**策划真实作者路径**（比"只能 C++ 注册"保真度高），且 Task 2.5 的资产载体第一次拥有**行为面消费者**。C++ 注册只保留给需要句柄/来源锚点的生命周期半边。
> - **② 模板加第 6 步 `AfterDamage`**（计划原文的模板是五步）。**原因**：`4.4`（载荷读取器）需要一个 `AfterDamage` 订阅点，而 `PreExecute`/`AfterDamage` 两步**都不在**既有 `SliceDefault`（4 步）/ `SliceFlow`（2 步）里，且 `SliceDefault` 是检查点 6 的证据链（MUST NOT 动）⇒ 新建 `DamageFlowTemplate.Check.ArmorBreakModifier`（六步，两个数值步骤**刻意不带 delegate**：基础值取上下文输入、执行量取裁决候选——避免公式逃生口给判据引入无关变量）。
> - **③ 资产行配 `GateTags`、默认暗**（计划原文未提）。**原因**：该资产是常驻内容，行若默认点灯，**此后任何含 `PreExecute` 步骤的模板都会被它静默减半**（未来 R5/R6 的模板首当其冲）⇒ 设计为"默认暗 + 装置在检查块内点灯、块末灭灯"，实测"未点灯 ⇒ 扣原值 30 / 点灯 ⇒ 扣 15"。**顺带收益**：给台账 `TRIG-3`（定义库装配行没有摘除入口）一个**可用的控制面口径**——启停走点灯，不走摘除。
> - **④ `4.5` 的判据改用"句柄活性"而非日志文本**：`ExecuteChain` 对已登记链恒返回**有效**句柄（全即时链在返回前走完并释放），对未登记链返回**无效**句柄 ⇒ `句柄有效 && !IsRunActive` 精确表述"链找到 + 进了 + 没挂起（= 走完）"，"降级后卡在 Running"与"链没找到"都会让它变红。该检查住 `Tcs.Test.Slice.Reject`（它必产一条 Warning，常规命令必须零红字）。
>
> **一处台账升级（不是计划错误，是把既有条目的触发条件提前）**：装置实现期撞上 `FTcsSourceHandleRegistry` 的**每实例计数器**语义——插件内两个独立注册表（定义库装配行 / 伤害门面每流程发号）**头一个 Id 都是 1**，而 `UnregisterTriggerRowsBySource` 已是公开面 ⇒ "宿主自建注册表发号的行"会与"定义库装配的行"**同号**，级联摘除**误摘后者**。**该缺陷早前已入册 = `WAIT-6`**（2026-09-20，原记"无实际碰撞场景，等 M6"）⇒ 本批**不重复登记**，只把触发条件提前到"来源级联一进入实用面"并补上真实碰撞场景。装置侧用**直写高位 Id** 规避（代码注释已写明理由），**不是机制保证**。
>
> **边界（写进证据 §5，MUST NOT 外推）**：只覆盖单机/单世界/单 PIE 进程；`ModifyFlow` 的成功提交**只经 `Op=Mul` 落在 `DamageFlowKey.BaseDamage` 键**验过；`Execute` 的**候选裁决**通道（`ExecuteCandidates` + `SortKey`）仍无真实候选走过（归 `DAMAGE-4`）；GC 面未覆盖（`FTcsTriggerRegistry::AddReferencedObjects` 的存活语义仍无行为证据，与 `WAIT-8` 同款留白）；`IsDataValid == Valid` 只扫触发定义资产（链资产缺提升段，`TOOLS-2` ③）。

> **2026-09-30 承接注记（从 Task 3 移入；用户裁定"连同测试结果说明一起并入，使上下文完整"）**：
> **① 本 Task 多背三项检查**（原属 Task 3 的 `tasks.md` §4，本次移出）：
> - **4.3 端到端"破甲"**：触发行命中 → 单步链 `[ModifyFlow]` → 黑板多出一笔提交 → 后续 `Damage` 读到修正值（`BaseDamage` 经 `Op=Add` 后扣血值与预期一致）；
> - **4.4 载荷读取器实证**：订阅 `TcsEvent.Damage.AfterDamage` 的触发行，`Caster` 解析为本次流程的 `Attacker`（改造前读到空句柄）；
> - **4.5 的命令化**：Task 3 已实测降级路径成立（留 Warning + 链继续），但其"独立成 `.Reject` 命令"的收口随装置一并归本 Task。
> **② 三项的共同前置 = 触发行夹具**，而**仓里一个也没有**：全仓 `RegisterTriggerRow` / `FTcsEffectTriggerInstance` 在 `TcsEffect/` 之外**使用点为零**；Task 2.5 的 `UTcsEffectTriggerDefAsset` 载体未实现 ⇒ **触发行今天只能 C++ 注册**，纯内容侧配不出来 ⇒ Step 1/Step 2 必须含装置代码。**装置住哪仍未裁定**（用户留本 Task 定）；先例 = 宿主侧 `Source/TcsDev/`（LAC 已入库 17 文件，含 `TcsDevSliceRig` 与 `Tcs.Test.Slice.Run` / `.Reject` 命令对），插件侧 `Private/Testing/` 按惯例不入库。
> **③ Task 3 的验收结果（本 Task 的起点事实）**：`Tcs.Test.Slice.Run` 单次运行 **9 PASS / 0 FAIL**、全日志零 Error 零 ensure；`检查 3` 步数 **10 → 11** 证明新步骤类型存档后可解析；执行器降级 Warning 恰一条。**两条 MUST NOT 当作已证**（详见 `add-damage-modifyflow-primitive/tasks.md` §6）："扣血仍 45"不构成"降级不干扰流程值"的证据（该步追加在链尾）；**成功提交路径从未被执行过**——这正是 4.3 存在的理由。
> **④ 顺带**：`UTcsEffectChainDef::IsDataValid` 缺"提升为 `Valid`"段（台账 `TOOLS-2` ③）会在本 Task 的资产校验面暴露——**装置里若断言 `IsDataValid == Valid` 必然失败**（返回 `NotValidated`），别把它当成本 Task 的缺陷。

> **实施注记（必读）**：
> - **判据要写它真正要拦的东西**（`MEM-20260922-02`）：本验收拦的是"**修改器通道端到端可用**"，不是"某个数值恰好是 X"。故装置 MUST 同时断言**对照组**（不挂行 = 原值）与**实验组**（挂行 = 减半）——只有实验组会假绿（若基准值本身配错，减半后可能恰好等于期望值）。
> - **三个半边都要验**：①**提交生效**（挂行 → 值变）；②**摘行还原**（摘行 → 值还原——证 `UnregisterTriggerRow` 真的断了订阅）；③**按来源级联摘除**（`UnregisterTriggerRowsBySource` → 值还原）。只验①会让"订阅泄漏"藏起来。
> - **`Tcs.Test.Slice.Run` 的既有 10 项检查 MUST 保持全绿**——本任务只**追加**检查，不改既有断言。若追加后既有检查变红，那是回归不是新功能问题（先查因再改）。
> - **链资产重建而非复用**：`ModifyFlow` 是新 struct，链资产的 `Steps` 里 `FInstancedStruct` 需要它——**新资产**比改既有资产安全（既有 `DA_SliceChain` / `DA_FormulaChain` 是检查点 1/6 的证据链，动它们会让那两条验收失去可比性）。
> - **模板新建而非改既有**：`SliceDefault`（4 步）是检查点 6 的证据链（"改 `DamageBase` 零 C++ 生效"那条），**MUST NOT** 给它加 `PreExecute` 步——那会改变它的步骤数并使该检查点的既有结论失去可比性。新建 `Slice_Modifier` 模板专供本验收。

---

## Task 5: 收束（文档回写 + 台账 + 归档）

- [ ] **Step 1: 设计文档回写**——`04-module-effects.md` 的 §2.2 触发行段（补"R4 已落地哪些字段/哪些留位"）、§12 验收钩子；`09-module-damage.md` §2.3（补 ModifyFlow 落地状态）
- [ ] **Step 2: 台账更新**——`deferred-inputs-ledger.md`：
  - **DAMAGE-3 部分消费**（`ModifyFlow` 已落 / `Heal` 留 R5）
  - **DAMAGE-1 部分消费**（载荷装填通路已落 / `EventTarget` 选择器留待真实消费者）
  - **新增条目**：D4-5 剩余条件（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom）+ `EventPayloadFilter` / `Cues` / `InterruptPriority` / `ExecutionGate` 非默认值 —— 合并为一条"触发行留位字段与剩余条件"（归属 = 各自真实消费者出现的轮次）
- [ ] **Step 3: README 决策日志**追加条目（本轮范围 / 路线偏离原序的理由 / 验收结果）
- [ ] **Step 4: 全部提案归档** + `openspec validate --specs --strict` 全绿
- [ ] **Step 5: 轮次检查点卡**（按 harness-retro 模板，走用户确认门）

---

## 台账消费与新增汇总

**本轮消费**：
| 条目 | 消费程度 |
|---|---|
| **DAMAGE-3**（TcsDamage 的 Heal / ModifyFlow 执行器） | **部分**——`ModifyFlow` 落地；`Heal` 留 R5 |
| **DAMAGE-1**（Context 默认目标初始化 = 事件目标） | **部分**——载荷装填通路（`EventPayload`）与 `Caster` 解析落地；`EventTarget` 选择器留待真实带目标的载荷类型 |
| **WAIT-9**（跨模块导出宏） | **按需**——本轮新增的跨模块消费面（触发行 API 供宿主调用）MUST 带导出宏 |

**本轮新增（Task 5 登记）**：
| 事项 | 归属 |
|---|---|
| D4-5 剩余触发条件（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom） | `AttributeCompare` → 需属性读取注入（可随 R5）；`GateCheck` → 随 R6（读 M5 `BoolSwitches`）；`VariableCompare` → 等变量存储消费者 |
| 触发行留位字段（`EventPayloadFilter` / `Cues` / `InterruptPriority` / `ExecutionGate` 非默认值） | `Cues` → R8（TcsCue）；`InterruptPriority` → 链打断语义轮；`ExecutionGate` 非默认值 → 网络姿态轮；`EventPayloadFilter` → 首个带可筛字段的载荷类型出现时 |

---

## Self-Review（写完后自查记录）

**1. 规格覆盖**：D4-1 的 10 字段 → Task 1 全部落地（其中 4 个标注"只存不裁"并说明理由）；D4-5 条件最小集 → Task 1 落 2 项 + 余项入台账；D7-6 修改器唯一通道 → Task 2（订阅）+ Task 3（提交）+ Task 4（实证）三段闭合；04 §3 四道门顺序 → Task 2 注记明文。

**2. 占位符扫描**：无 "TBD"/"TODO"/"稍后补"；所有 struct 字段、函数签名、验收场景均给出实际内容；未落地面**显式列出并给出归属**（不是留白）。

**3. 类型一致性**：`FTcsTriggerRow.Effects` 在 Task 2 的起链装配里被读为 `ChainId`（`ExecuteChain(Row.Effects, Context)`）——类型 `FGameplayTag` 一致；`FTcsTriggerContext` 在 Task 1 定义、Task 2 消费，字段名一致；`FTcsStepModifyFlow.TargetKey` 与 `FTcsFlowAttributes::Submit` 的键参数类型一致（`FGameplayTag`）。

**4. 已识别的实施风险**（写入注记而非隐藏）：
- 命名撞车（`FTcsFlowModify` vs `FTcsStepModifyFlow`）——注记要求互相指名；
- ~~`FTcsConsumePolicy` 含 `TFunction` 不可反射——采纳"不标 UPROPERTY"的收窄~~ **已于 2026-09-30 解除**：用户拍板改事件语义（去 `TFunction` + 改名 `FTcsDamageModifierConsumePolicy`），`Consume` 字段**改为正常标 `UPROPERTY`**——原"不标"收窄随之作废（真相源 = 提案 `add-damage-modifyflow-primitive`）；
- **载荷裸指针 + 链挂起 = 悬空**（`FTcsDamageFlowCollectEvent::Context` 是裸指针，链运行态跨帧）——已在 Task 3 明文"MUST NOT 挂起"；
- 装置追加检查可能扰动既有 10 项——注记要求先查因；
- 宿主既有模板都不含 `PreExecute`——Task 4 改为**新建** `Slice_Modifier` 模板（不污染检查点 6 的证据链 `SliceDefault`）。

**5. 对上一轮结论的更正已写入 §0**（"6 个收集事件无发布者"是错的——十步全实现且会广播，只是默认模板不组装）。这条更正的意义：**若照错前提做计划，会重复实现已存在的代码**。

**6. 提交边界的留痕（2026-09-30 加）**：本文件的未提交改动里**混着两条工作线**——①本计划自身（Task 3 sketch 的 `UPROPERTY` 改动、旧收窄作废留痕、Task 3.5 的四原语改写）与 ②**P-A 排序相位 + R4.5 落档**（后者属另一条线的设计成果）。核查结论：**无法按文件切出干净的 R4.5 提交**（R4.5 的权威载体 = 本文《R4.5 批次表》，而 `GLOSSARY` / `INDEX` / `reflection-backlog` 三处是混合文件且「最后更新」日期共写）。已在批次表头部写入**提交边界**说明（见该块第 4 条引用行），防止后续会话把该块误当独立单元。

---

## 执行方式

按项目纪律（用户级"禁 TDD" + 本仓 `AGENTS.md`），执行时：
1. 每 Task 以 **OpenSpec 提案开道**（`validate --strict` 通过 → 实施 → 归档）；
2. 每 Task 结束 = **UBT 编译通过** + 定向人工检查，即"停点待检查"（**不含自动提交**——提交需用户明确授权）；
3. Task 4 的 PIE 实测由**用户执行**（装置命令经 Output Log 提交）；
4. 遇设计文档与计划冲突时，裁决顺序 = **用户现场决定 > 设计文档 > 本计划 > 会话提示词**。

