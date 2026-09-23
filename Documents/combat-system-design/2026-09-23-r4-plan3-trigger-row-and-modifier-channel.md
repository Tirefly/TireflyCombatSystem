# R4 实施计划三：触发行与伤害修改器通道（M4a 触发闭环）

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
- **命名**：类型 = 前缀字母 + `Tcs` + 语义名；枚举值 = 枚举名缩写全大写；API 导出宏 `TCS<模块>_API`（跨模块消费面 MUST 带——台账 T-9）；动词纪律（`Resolve` 仅用于句柄/Id→对象）。
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

本轮起路线**偏离** `2026-09-02-rebuild-module-map-proposal.md:146` 的原序（原序：`R4 M3 → R5 M4a+M4c → R6 M5 → R7 M6 → R8 M7/M8`），改为**触发行先行**。理由（2026-09-23 用户认可）：**触发行是 M3 行为面的验收前置**——`03 §6` 明文"buff 行为由 M4 触发行订阅 M3 生命周期事件挂接"，M3 先落地则其行为面在交付当天没有任何订阅者，验不了。

| 轮次 | 主题 | 主要交付 | 前置 | 台账消费 |
|---|---|---|---|---|
| **R4（本计划）** | **M4a 触发行 + 伤害修改器通道 + 原语补齐（一批）** | ①触发行定义/条件注册表（Task 1 已完成）；②登记表与求值器 + **载荷读取器注册表**（Task 2 **已完成**）；③**独立资产载体 `UTcsEffectTriggerDef` + DefLibrary 发现**（Task 2.5）；④`ModifyFlow` 链原语（Task 3）；⑤**追加 4 个原语**：`SetVar` / `Branch` / `RunSubChain` / `WaitEvent`（Task 3.5）；⑥端到端验收（Task 4）；⑦收束（Task 5） | R3 已收束 | **R5-3 部分**（ModifyFlow）；**R5-1 部分**（载荷装填）；**R5-2 部分**（4 个原语——2026-09-23 用户拍板提前） |
| R5 | **M3 状态层（TcsState）** | `UTcsStateDef` 家族 / `FStateInstance` + 中央注册表 / 五轴堆叠 / 关系表 + 级联重评 / Duration-Period + 到期堆 / `ParamSnapshot` / **修正器物化（D3-19）** / **`ApplyState` 链原语（随 M3 同批——否则 Buff 有事件却施加不了状态）** / 生命周期事件全集 / **`Heal` 原语** / `ModifyAttribute`（属性访问注入位与 `ApplyState` 同批） | R4（行为面验收） | **R4-1**、**R4-2**、**R4-3**、**R5-2 余**（`Repeat`/`Parallel`/`OnError`）、**R5-3 余**（`Heal`） |
| R6 | **M5 技能层（TcsSkill）** | `UTcsSkillDef` / 账本 `FLearnedSkillEntry` / 六道门禁 / **时段驱动 `FPhaseSpan` + 打断** / 冷却多轨道 + 三事件 / Cost 策略 / **参数链（带式聚合，折叠器复用）** / 链重定向栈 / `FEntrySelector` | R5（`FSkillDef` 继承 `FStateDefBase`） | **R6-1**（参数链接入折叠器）、**M0-1**（若全域订阅需求成立） |
| R7 | **M6 集成层** | 两级单位（Mass 小兵 + 全功能军官）/ StateTree 决策接线 / **AttributeSet 全套** / `PrimaryAssetTypesToScan` + 发现机制切换 AssetManager + 加载层三策略 | R6 | **R7-1**、**R7-2**、**R7-3**、**T-2**、**T-5**、**T-6**、**T-8 余**（模板资产化） |
| R8 | **M7 表现 + M8 编辑器与工具** | `TcsCue`（Cue 契约 + PlayCue + 默认适配）/ 校验矩阵（四联 + 定义校验）/ Def 双轨同步器 / Explain 调试面板 / K2 强引脚 | R7 | **R8-1**~**R8-6**、**T-1**、**T-4**、**T-9 全量审计**、**T-10** |

**说明**：
- **R4 扩容（2026-09-23 用户拍板）**：原 5 Task 扩为 7 个 Task（增 Task 2.5 载体 + Task 3.5 原语批次）。理由——触发行落地后"事件 → 条件 → 链"就通了，但链里只有 3 个原语（等待/选目标/扣血）**能触发却做不了什么**；`SetVar`/`Branch`/`RunSubChain`/`WaitEvent` 四个**零新依赖**（或仅需扩唤醒源），与触发行同轮交付最经济。
- **`ModifyAttribute` 与 `ApplyState` 后置到 R5（2026-09-23 用户拍板）**：两者都需要"属性访问注入位"（`FTcsEffectContext` 今天没有属性读/写口），且 `ApplyState` 本身就是 TcsState 的领域步骤——**与 M3 同批落地**最自然（M3 不落地就没有状态可施加）。
- **`Repeat`/`Parallel`/`OnError` 留 R5**：分别需熔断游标、`JoinCount` 汇合、错误路径语义——三者都比上述四个重。
- **R6 可能需拆两轮**（M5 与时段/打断都是重活）——届时按 `M5 账本/施法` 与 `时段与冷却` 分拆，前置关系不变。
- **不绑轮次的触发条件型条目**（T-2/T-3/T-5 等）在其触发条件成立时随当轮消费，不单独排期。
- **R7-3 的加载层**是 R7 内最重的一块（三策略 + 异步默认/同步逃生口），若 R7 超载可拆出独立轮。

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
2. **"值语义 `TArray` → 无需 ARO"判据错误（GC 地雷）**：是否需要 ARO 与值/指针语义**无关**，只取决于**容器是否 GC 可见**。`FTcsTriggerRegistry` 是门面的**非 `UPROPERTY` 成员** → GC 的 `RefLink` 走不到它，而行内 `FInstancedStruct`（`Conditions`/`EventPayloadFilter`）内层可放宿主自定义 struct 的 `UPROPERTY` 对象引用（D4-16 类型不设限）→ **静默回收**。这与 T-8 是同一类缺口（**缺口在容器，不在载荷**）。**处置** = 门面 `AddReferencedObjects` 逐行补引用（与 `ChainDefs` 同款手法）。
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

	// 目标黑板键（项目词表或契约键 `Tcs.Flow.Key.*`；空 = 落契约键 BaseDamage）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FGameplayTag TargetKey;

	// 运算带（带序唯一真相在 Op；折叠走共享纯函数）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") ETcsAttributeOp Op = ETcsAttributeOp::TAO_Add;

	// 操作数（PV 载体；Literal 直配，ParamRef 随其来源策略轮）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FTcsParamValue Operand;

	// 消耗策略（D7-4：候选按 SortKey 裁决选一、成功执行才消费；未选中者完全不动）
	UPROPERTY(EditAnywhere, Category = "Tcs|Damage|Chain") FTcsConsumePolicy Consume;
};
```

- [ ] **Step 1: OpenSpec 提案**（`damage-step-library` MODIFIED：追加 `ModifyFlow` 链原语需求；`damage-primitive` 视需要）
- [ ] **Step 2: 实施 struct + 执行器 + 宏注册**
- [ ] **Step 3: 编译验证**（Development）
- [ ] **Step 4: 定向人工检查**——跨模块自注册可查（`UE_DEFINE_EFFECT_STEP_EXECUTOR` 生效）；依赖面零新增边

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
> - **`Heal` 原语不在本轮**：它需要治疗流程模板 + `IHealFlowDelegate`（09 §2.4"同骨架精简版"），是独立一块。**台账 R5-3 部分消费**（ModifyFlow 已落，Heal 留 R5）。

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

- [ ] **Step 1: OpenSpec 提案**（`effect-chain` MODIFIED：追加四个原语需求；`effect-interpreter` 若唤醒源契约需改则一并）
- [ ] **Step 2: 实施**——按"零前置 → 有前置"顺序：`SetVar` → `RunSubChain` → `Branch` → `WaitEvent`
- [ ] **Step 3: 编译验证**（Development；`WaitEvent` 改 `TcsChainRun` 结构，须补 **Shipping**——结构改动是 Shipping 能照出的类型）
- [ ] **Step 4: 定向人工检查**——依赖面零领域模块；**熔断自检**（`RunSubChain` 递归起链 + `Repeat` 类自激——`MaxStepsPerFrame` 是否够）

> **实施注记（必读）**：
> - **⚠️ `WaitEvent` 是本批最难的一个**：它需要**新增唤醒源**（`04 §2.4` 四种唤醒源之"事件匹配"）。`WaitDelay` 已实证"到期堆唤醒"路径（`PendingExpiry` 锚 + 代际校验重入），`WaitEvent` 需要同款的"订阅句柄锚"——`FTcsChainRun` 要加字段（建议 `FTcsEventSubscriptionHandle PendingSubscription`），且**退订时机**必须覆盖三条路径：①事件命中；②超时（若配）；③运行态被释放（`ReleaseRun` 必须退订——否则订阅泄漏且回调打到已回收的运行态）。
> - **`Branch` 与 `RunSubChain` 共用"子链完成唤醒源"**：先做 `RunSubChain`（单一形态），`Branch` 的 `bWait` 直接复用其机制。**MUST NOT** 为两者各写一套唤醒逻辑。
> - **递归深度**：`RunSubChain` 允许子链再起子链——**已有的 `MaxStepsPerFrame` 熔断只管"单帧步数"，不管"嵌套深度"**。本批 MUST 明确：嵌套深度靠什么护栏（建议复用熔断计数——子链步数计入父链的帧内步数），并在注记里写明实测值。
> - **`SetVar` 的 `Value` 用 `FTcsParamValue`**：与链步骤其它数值字段一致（PV 载体，可 Literal/ParamRef）——**MUST NOT** 另造一个 double 字段（那会让"变量只能用字面量"成为隐式限制）。
> - **`Branch` 的条件复用 Task 1 的条件注册表**：`Conditions` 求值走 `EvaluateTriggerConditions`——但**上下文不同**（那是 `FTcsTriggerContext`，需要 `ClassificationTags`）。链侧没有该上下文，故本批要么①构造一个最小 `FTcsTriggerContext`（从 `FTcsEffectContext` 映射），要么②为链侧条件另立签名。**建议①**（一套条件类型两处可用，零重复），实施时确认映射字段（`EventTag` 取运行态记录的上次事件、`ClassificationTags` 从哪来——**若链侧无来源则本批 `Branch` 只支持不依赖分类标签的条件**，并在注记写明）。

---

## Task 2.5: 触发行独立资产载体（`UTcsEffectTriggerDef` + DefLibrary 发现）

**Files:**
- Create: `Source/TcsIntegration/Public/Trigger/TcsEffectTriggerDefAsset.h` + `Private/Trigger/TcsEffectTriggerDefAsset.cpp`
- Modify: `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` + `Private/TcsDefinitionSubsystem.cpp`（加一条发现路径）

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

- [ ] **Step 1: OpenSpec 提案**（`integration-entity` MODIFIED：加触发定义资产的发现与解析；`effect-trigger` 若需则补"资产载体"需求）
- [ ] **Step 2: 实施**（资产类 + DefLibrary 发现/缓存/装配）
- [ ] **Step 3: 编译验证**（Development + **Shipping**——含 `IsDataValid` 的 `WITH_EDITOR` 面，Shipping 必跑）
- [ ] **Step 4: 定向人工检查**——依赖面：`TcsEffect` 零反向依赖（资产类住 TcsIntegration）；DefLibrary 的失败清单能报出空 `TriggerTag`

> **实施注记（必读）**：
> - **GC 补引用**：DefLibrary 的缓存若用 `TMap<FGameplayTag, TUniquePtr<...>>` 持**定义内容**（值语义 struct）则无需 ARO；但**资产对象本身**必须 `UPROPERTY` 锚定（照 `ChainDefAssets` 的既有做法）——否则重载后资产被回收、`ResolveTriggerDef` 返回的指针指向已释放内容。
> - **内联位（SkillDef/BuffDef）本批不做**：`SkillDef`/`BuffDef` 今天都不存在（M3/M5 未落地）。**本批只做独立资产**，内联位在 R5/R6 给那两个 Def 加 `TArray<FTcsEffectTriggerDef>` 字段时自然成立（定义类型已是纯配置，可直接内联——**这正是 Task 1 分层的收益**）。
> - **"系统级规则"是本资产的业务场景**（用户 2026-09-23 确认）：全局常驻、与任何 Def 无关的触发规则（如"任何单位死亡时触发某链"）。Buff/Skill 的行为走**内联**（施加时注册、Source = 状态实例句柄）。

---

## Task 4: 端到端验收（修改器通道实证）

**Files:**
- Modify: `Source/TcsDev/Private/Dev/TcsDevSliceRig.cpp`（扩展既有 `Tcs.Test.Slice.Run`，**不新增命令**——遵循"暂不添加常驻验收命令"的用户口径，新功能验收并入既有命令）
- Modify: `Content/TcsDev/`（新增一条单步链资产：`ModifyFlow` 半伤）
- Modify: `Config/DefaultGameplayTags.ini`（新增链 id tag）

**验收场景（"破甲"修改器——09 §2.3 的最小实例）**：

1. **准备（三件，均住宿主侧）**：
   - **链资产** `DA_ArmorBreakChain`（`ChainId = Tcs.Chain.ArmorBreak`，**单步** `FTcsStepModifyFlow{TargetKey=Tcs.Flow.Key.BaseDamage, Op=TAO_Mul, Operand=Literal(0.5)}`）；项目 tag 表加 `Tcs.Chain.ArmorBreak`。
   - **新流程模板** `Tcs.Flow.Template.Slice_Modifier`（`TcsDevBootstrap` 内注册）——步骤 = `CollectStart → BaseDamage → **PreExecute** → Execute → Completed`。**必须新建**：宿主既有的 `SliceDefault`（4 步）与 `SliceFlow`（2 步自研）**都不含 `PreExecute`**（已核 `TcsDevBootstrap.cpp:226-255`），而 `PreExecute` 是收集修改器候选的那一步（`TcsFlowStepsRest.cpp:155-166`）。
   - **触发行**（装置注册）：`EventTag = Tcs.Event.Damage.PreExecute`、`Effects = Tcs.Chain.ArmorBreak`、`Priority = 0`、`bConditionMissIsSilent = true`、`Source = <装置来源句柄>`。
2. **跑流程**：用新模板起伤害流程，`BaseDamageInput` 设一个**非整数值**（如 30——减半后 15，与"原值 30"可区分；**避免用 1.0 这类减半后仍是可猜值的数**）。
3. **断言（三个半边）**：
   - ① **提交生效**：挂行 → 最终伤害 = 基准值 × 0.5；
   - ② **摘行还原**：`UnregisterTriggerRow` → 值还原为基准值；
   - ③ **按来源级联摘除**：`UnregisterTriggerRowsBySource(装置来源)` → 值还原为基准值。

- [ ] **Step 1: 建链资产 + 项目 tag + 新流程模板**（资产经 UE MCP 建；**先与用户确认再操作编辑器**——MCP 操作边界纪律；流程模板在 `TcsDevBootstrap` 内注册）
- [ ] **Step 2: 装置扩展**（新增检查：挂行前基准值 / 挂行后减半 / 摘行后还原 / 按来源级联摘除）
- [ ] **Step 3: 编译验证**（Development + **Shipping**——装置含 `WITH_EDITOR` 面时必跑）
- [ ] **Step 4: PIE 实测**（用户执行；**常规命令 MUST 零红字**——预期失败面若需要，独立成 `.Reject`）
- [ ] **Step 5: 验收记录回写**（plan3 注记 + README 决策日志）

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
  - **R5-3 部分消费**（`ModifyFlow` 已落 / `Heal` 留 R5）
  - **R5-1 部分消费**（载荷装填通路已落 / `EventTarget` 选择器留待真实消费者）
  - **新增条目**：D4-5 剩余条件（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom）+ `EventPayloadFilter` / `Cues` / `InterruptPriority` / `ExecutionGate` 非默认值 —— 合并为一条"触发行留位字段与剩余条件"（归属 = 各自真实消费者出现的轮次）
- [ ] **Step 3: README 决策日志**追加条目（本轮范围 / 路线偏离原序的理由 / 验收结果）
- [ ] **Step 4: 全部提案归档** + `openspec validate --specs --strict` 全绿
- [ ] **Step 5: 轮次检查点卡**（按 harness-retro 模板，走用户确认门）

---

## 台账消费与新增汇总

**本轮消费**：
| 条目 | 消费程度 |
|---|---|
| **R5-3**（TcsDamage 的 Heal / ModifyFlow 执行器） | **部分**——`ModifyFlow` 落地；`Heal` 留 R5 |
| **R5-1**（Context 默认目标初始化 = 事件目标） | **部分**——载荷装填通路（`EventPayload`）与 `Caster` 解析落地；`EventTarget` 选择器留待真实带目标的载荷类型 |
| **T-9**（跨模块导出宏） | **按需**——本轮新增的跨模块消费面（触发行 API 供宿主调用）MUST 带导出宏 |

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
- `FTcsConsumePolicy` 含 `TFunction` 不可反射——采纳"不标 UPROPERTY"的收窄；
- **载荷裸指针 + 链挂起 = 悬空**（`FTcsDamageFlowCollectEvent::Context` 是裸指针，链运行态跨帧）——已在 Task 3 明文"MUST NOT 挂起"；
- 装置追加检查可能扰动既有 10 项——注记要求先查因；
- 宿主既有模板都不含 `PreExecute`——Task 4 改为**新建** `Slice_Modifier` 模板（不污染检查点 6 的证据链 `SliceDefault`）。

**5. 对上一轮结论的更正已写入 §0**（"6 个收集事件无发布者"是错的——十步全实现且会广播，只是默认模板不组装）。这条更正的意义：**若照错前提做计划，会重复实现已存在的代码**。

---

## 执行方式

按项目纪律（用户级"禁 TDD" + 本仓 `AGENTS.md`），执行时：
1. 每 Task 以 **OpenSpec 提案开道**（`validate --strict` 通过 → 实施 → 归档）；
2. 每 Task 结束 = **UBT 编译通过** + 定向人工检查，即"停点待检查"（**不含自动提交**——提交需用户明确授权）；
3. Task 4 的 PIE 实测由**用户执行**（装置命令经 Output Log 提交）；
4. 遇设计文档与计划冲突时，裁决顺序 = **用户现场决定 > 设计文档 > 本计划 > 会话提示词**。

