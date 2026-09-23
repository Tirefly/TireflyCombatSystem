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
| **R4（本计划）** | **M4a 触发行 + 伤害修改器通道** | 触发行数据形状/条件/求值器/登记表与订阅生命周期 + `ModifyFlow` 链原语 | R3 已收束 | **R5-3 部分**（ModifyFlow）；**R5-1 部分**（载荷装填） |
| R5 | **M3 状态层（TcsState）** | `UTcsStateDef` 家族 / `FStateInstance` + 中央注册表 / 五轴堆叠 / 关系表 + 级联重评 / Duration-Period + 到期堆 / `ParamSnapshot` / **修正器物化（D3-19）** / `ApplyState` 链原语 / 生命周期事件全集 | R4（行为面验收） | **R4-1**（`FTcsParamEnumerableSource`）、**R4-2**（上下文补 `Subject`/`EffectiveLevel`）、**R4-3**（`FFlowRedirect` 模板重定向栈）、**R5-3 余**（`Heal`） |
| R6 | **M5 技能层（TcsSkill）+ 控制流原语** | `UTcsSkillDef` / 账本 `FLearnedSkillEntry` / 六道门禁 / 时段驱动 + 打断 / 冷却多轨道 + 三事件 / Cost 策略 / **参数链（带式聚合，折叠器复用）** / 链重定向栈 / `FEntrySelector` / **`WaitEvent`/`Branch`/`Parallel`/`Repeat`/`RunSubChain` + `ModifyAttribute`/`SetVar`/`OnError`** | R5（`FSkillDef` 继承 `FStateDefBase`） | **R6-1**（参数链接入折叠器）、**R5-2**（剩余 8 原语）、**M0-1**（若全域订阅需求成立） |
| R7 | **M6 集成层** | 两级单位（Mass 小兵 + 全功能军官）/ StateTree 决策接线 / **AttributeSet 全套** / `PrimaryAssetTypesToScan` + 发现机制切换 AssetManager + 加载层三策略 | R6 | **R7-1**、**R7-2**、**R7-3**、**T-2**、**T-5**、**T-6**、**T-8 余**（模板资产化） |
| R8 | **M7 表现 + M8 编辑器与工具** | `TcsCue`（Cue 契约 + PlayCue + 默认适配）/ 校验矩阵（四联 + 定义校验）/ Def 双轨同步器 / Explain 调试面板 / K2 强类型引脚 | R7 | **R8-1**~**R8-6**、**T-1**、**T-4**、**T-9 全量审计**、**T-10** |

**说明**：
- **R6 可能需拆两轮**（M5 与剩余原语都是重活）——届时按 `M5 账本/施法` 与 `控制流原语` 分拆，前置关系不变。
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

## Task 1: 触发行数据形状与条件求值器

**Files:**
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerRow.h`
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerConditions.h`
- Create: `Source/TcsEffect/Private/Trigger/TcsTriggerConditions.cpp`

**Interfaces:**
- Consumes: TcsCore（`FGameplayTag` / `FTcsSourceHandle` / `FInstancedStruct`）。
- Produces:
```cpp
// —— 触发条件（D4-5 最小集的首批两项；求值上下文是**触发期**而非流程期）——
USTRUCT() struct TCSEFFECT_API FTcsTriggerCondition_HasAllTags
{
	GENERATED_BODY()
	// 事件上下文分类 Tag 集须含全部（空数组 = 无条件通过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") TArray<FGameplayTag> Tags;
};

USTRUCT() struct TCSEFFECT_API FTcsTriggerCondition_Chance
{
	GENERATED_BODY()
	// 通过概率 [0,1]；**随机值由调用方注入**（确定性纪律 D0-1——求值内部 MUST NOT 取随机数）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double Probability = 0.0;
};

// 触发期上下文（条件求值面；**最小集**——只含条件需要的字段，不搬整个链上下文）
USTRUCT()
struct TCSEFFECT_API FTcsTriggerContext
{
	GENERATED_BODY()
	UPROPERTY() FGameplayTag EventTag;                    // 命中的事件 Tag
	UPROPERTY() TArray<FGameplayTag> ClassificationTags;  // 事件携带的分类标签（如伤害流程的 分类Tag集）
	UPROPERTY() FTcsCombatEntityHandle Caster;            // 施法者（触发行解析出的主体）
};

// 条件求值助手（**同名字段 + 各求值点首行调用**的纪律，与流程步骤侧同款——
// 步骤/条件类型之间无公共基类 D4-16，没有基类虚函数可挂）
bool EvaluateTriggerConditions(
	const TArray<FInstancedStruct>& Conditions,
	const FTcsTriggerContext& Context,
	double RandomValue = 0.0);   // 未知条件类型 → 视为不过 + Warning（不静默通过）
```

```cpp
// —— 触发行（D4-1 终版 10 字段）——
USTRUCT()
struct TCSEFFECT_API FTcsTriggerRow
{
	GENERATED_BODY()

	// ① 订阅哪个事件（Tag 路由，裁决 2a）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") FGameplayTag EventTag;

	// ② 载荷预筛（廉价字段匹配，先于条件求值；R4 留位——载荷类型目前只有流程收集事件）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") FInstancedStruct EventPayloadFilter;

	// ③ 门禁条件（不求值即不触发；未过按 ⑩ 决定是否静默）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") TArray<FInstancedStruct> Conditions;

	// ④ 触发后执行的链（引用已登记的 ChainId；内联链后置）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") FGameplayTag Effects;

	// ⑤ 同 Tag 多行触发顺序（**大者先**——与覆盖带"大者胜"同向）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") int32 Priority = 0;

	// ⑥ 执行闸（网络姿态挂点；R4 只有默认值 0 = 恒通过——R4 无联网）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") ETcsExecutionGate ExecutionGate = ETcsExecutionGate::TEG_Always;

	// ⑦ 可打断哪些正在跑的链（**R4 只存不裁**——链打断语义尚无实现）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") int32 InterruptPriority = 0;

	// ⑧ 行级开关 Tag（运行时点灯控制整行——未点亮的行直接跳过）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") TArray<FGameplayTag> GateTags;

	// ⑨ CueId 引用列表（帧末通道；**R4 只存不裁**——TcsCue 属 R8）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") TArray<FGameplayTag> Cues;

	// ⑩ 条件未过时是否静默（false = 记录一条日志线索——M8 Explain 的数据源）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") bool bConditionMissIsSilent = true;

	// —— 登记侧字段（不属 D4-1 的 10 字段，是登记表所需的簿记）——
	// 来源（级联退订锚点；`UnregisterTriggerRowsBySource` 按它全量摘除——与 M2 RemoveBySource 同款）
	UPROPERTY(EditAnywhere, Category = "Tcs|Effect|Trigger") FTcsSourceHandle Source;
};

// 执行闸（D4-1 ⑥；R4 无联网 → 只有恒通过值，其余值留给网络姿态轮）
UENUM()
enum class ETcsExecutionGate : uint8
{
	TEG_Always = 0		UMETA(DisplayName = "恒通过（R4 唯一实现）"),
	TEG_AuthorityOnly = 1	UMETA(DisplayName = "仅权威侧（未实现，留位）"),
};
```

- [ ] **Step 1: OpenSpec 提案**（新能力 `effect-trigger`，两条需求：触发行数据形状 / 触发条件最小集）
- [ ] **Step 2: 实施三个文件**（头文件含完整 Doxygen 注释；条件求值助手住 `.cpp`）
- [ ] **Step 3: 编译验证**（Development；本轮无 `WITH_EDITOR` 面，暂不需 Shipping）
- [ ] **Step 4: 定向人工检查**——`openspec validate effect-trigger --strict` 通过；依赖面 `grep "^#include"` 零领域模块

> **实施注记（必读）**：
> - **条件类型与流程侧重名风险**：`TcsDamage` 已有 `FTcsConditionHasAllTags` / `FTcsConditionChance`（流程步骤用）。本轮在 `TcsEffect` 建的是**触发期**版本（求值上下文不同：`FTcsTriggerContext` vs `FTcsDamageFlowContext`）。**这不是重复实现而是必要分化**——TcsEffect 不能 include TcsDamage（依赖铁律），且两者上下文形状本就不同。命名上用 `FTcsTriggerCondition_*` 前缀区分。
> - **`FTcsTriggerContext` 是"最小集"**：只含条件求值真正需要的字段。**MUST NOT** 为将来可能的条件预建字段（零消费者不预建）——`AttributeCompare`/`VariableCompare`/`GateCheck` 落地时再各自扩上下文（那时它们有真实消费者）。
> - **D4-5 剩余条件**（`AttributeCompare` / `VariableCompare` / `GateCheck` / Custom）**不在本轮**：`AttributeCompare` 需要属性读取注入、`GateCheck` 读 M5 的 `BoolSwitches`、`VariableCompare` 需要变量存储——三者今天都零消费者。**已记台账**（见文末）。
> - **`EventPayloadFilter` 与 `Cues` 只存不裁**：前者载荷类型单一（流程收集事件无字段可筛）、后者 TcsCue 属 R8。**MUST NOT** 造空转的匹配器。
> - **`Source` 字段说明**：D4-1 的 10 字段不含它，但设计明文"来源注销自动退订"需要锚点——它是**登记簿记**不是触发语义，故与 10 字段分列并注明。

---

## Task 2: 触发行登记表与求值器（订阅生命周期）

**Files:**
- Create: `Source/TcsEffect/Public/Trigger/TcsTriggerEvaluator.h`
- Create: `Source/TcsEffect/Private/Trigger/TcsTriggerEvaluator.cpp`
- Modify: `Source/TcsEffect/Public/TcsEffectSubsystem.h`（登记表 + 点灯 API + 生命周期）
- Modify: `Source/TcsEffect/Private/TcsEffectSubsystem.cpp`

**Interfaces:**
- Consumes: Task 1 的 `FTcsTriggerRow` / `FTcsTriggerContext` / `EvaluateTriggerConditions`；TcsCore 的 `UTcsEventHandler` / `UTcsEventBusSubsystem`。
- Produces:
```cpp
// —— 共享 Handler（裁决 2a：事件类型 → Handler CDO；事件 struct 上不自绑 delegate）——
UCLASS()
class TCSEFFECT_API UTcsTriggerEvaluator : public UTcsEventHandler
{
	GENERATED_BODY()
public:
	// 事件入口：Tag 路由 → 取匹配行（Priority 降序）→ 四道门 → 起链
	virtual void HandleEvent_Implementation(FGameplayTag EventTag, const FInstancedStruct& Payload) override;

	// 装配：求值器由门面创建并持弱引用回指（起链要经门面）
	void Initialize(UTcsEffectSubsystem* InOwner);

private:
	TWeakObjectPtr<UTcsEffectSubsystem> Owner;
};
```

```cpp
// —— 触发行句柄（复用 TcsCore 的句柄范式；见 TcsChainRun.h 的同款形态）——
// 说明：本计划**不引入池**——触发行是"登记期写入、运行期只读"的静态内容，
// 无高频增删，故登记表用 `TArray<FTcsTriggerRow>`（值语义）+ 索引句柄即可（零 GC 面）。
struct FTcsTriggerRowTag {};   // 类型区分标签（防与链运行态句柄互换）

struct FTcsTriggerRowHandle
{
	TTcsInstanceHandle<FTcsTriggerRowTag> Inner;

	bool IsValid() const { return Inner.IsValid(); }
};
```

```cpp
// —— UTcsEffectSubsystem 新增公共面（登记表 / 点灯 / 生命周期）——
	/**
	 * 登记触发行：登记表持有 + 按 `Row.EventTag` 装配总线订阅（首次出现该 Tag 时订阅一次，
	 * 末行摘除时退订——订阅计数配对，不给总线留死订阅）。
	 *
	 * @param Row 触发行（按值拷入）。
	 * @return 返回行句柄；`EventTag` 无效或 `Effects` 无效时 ensure + 返回无效句柄。
	 */
	FTcsTriggerRowHandle RegisterTriggerRow(const FTcsTriggerRow& Row);

	/** 摘除单行（订阅计数归零时退订）。 */
	bool UnregisterTriggerRow(FTcsTriggerRowHandle Handle);

	/** 按来源全量摘除（级联退订锚点——与 M2 `RemoveBySource` 同款语义）。 */
	int32 UnregisterTriggerRowsBySource(const FTcsSourceHandle& Source);

	/** 点灯/灭灯（行级开关；`GateTags` 全部点亮才通过）。 */
	void SetTriggerGateTag(FGameplayTag GateTag, bool bLit);
	bool IsTriggerGateTagLit(FGameplayTag GateTag) const;

	/** 登记行数（观测/装置断言用）。 */
	int32 GetTriggerRowCount() const;
```

> **持有形态说明（实施前必读）**：登记表用 **`TArray<FTcsTriggerRow>`（值语义）+ 索引句柄**，**不用 `TTcsInstancePool`**——触发行无高频增删、无挂起语义，池的代际校验在这里是零收益的复杂度。**代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有行指针**（回调内即取即用；求值器每次从登记表按索引重解析——与解释器"每步入器前重解析"同款纪律）。

- [ ] **Step 1: OpenSpec 提案**（`effect-trigger` 追加：登记表与订阅生命周期 / 求值器四道门）
- [ ] **Step 2: 实施求值器 + 登记表 + 点灯 API**
- [ ] **Step 3: 编译验证**（Development）
- [ ] **Step 4: 定向人工检查**——依赖面零领域模块；**订阅计数配对自检**（登记 N 行 → 退订 N 行 → 总线订阅表为空）

> **实施注记（必读）**：
> - **求值顺序 MUST 严格照 04 §3 的"四道门"**：`事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`。顺序有意义：`ExecutionGate`（网络闸）最廉价先判；`Conditions` 最贵最后判。
> - **`Priority` 是"大者先"**（与覆盖带 `OverridePriority` 同向，避免两套"谁更强"的相反约定）。同 `Priority` 时**按登记序**（确定性——遍历顺序不得依赖容器哈希序；`TSet` 哈希序不确定的坑见 2026-09-22 plan2 Task 7 Step 1）。
> - **订阅计数配对**：同一 `EventTag` 的多行**共用一个订阅**（订阅一次，回调内遍历该 Tag 的全部行）。**MUST NOT** 每行各订一次——那会让总线订阅表随行数膨胀且退订易漏。计数归零时 `Unsubscribe`。
> - **起链装配**：`FTcsEffectContext{Caster = 行上下文解析, EventPayload = 原事件载荷, Targets = 空}` → `ExecuteChain(Row.Effects, Context)`。**`Targets` 本轮留空**——"事件载荷 → 目标"通路（台账 R5-1）的完整形态需要真实带目标的载荷类型，R4 的消费者（`ModifyFlow`）不需要目标。`Caster` 的解析规则：从载荷内已知类型取（流程收集事件 → `Context->Attacker`），取不到则默认构造（记 Verbose 日志，不 ensure——载荷类型未知不是契约违规）。
> - **链未登记时**：`ExecuteChain` 已有拒绝面（Error + 不起链），本轮**不重复校验**——但那会让常规验收出红字，故**装置侧 MUST 保证被测链已登记**（见 Task 3 验收）。
> - **`bConditionMissIsSilent`**：`true` = 静默跳过（默认）；`false` = 记一条 `LogTcsEffect` 的 `Verbose` 行（**MUST NOT 用 Warning/Error**——条件未过是正常业务路径，不是故障；M8 Explain 面板将消费这些线索）。
> - **`UTcsTriggerEvaluator` 的生命周期**：由门面在首次登记时 `NewObject` 创建并 `UPROPERTY` 持有（总线订阅表持**弱引用**——不 root 会被 GC 掉，订阅静默失效；宿主 `UTcsDevScreenObserver` 的既有注释即为实证）。门面 `Deinitialize` 时清空登记表并退订全部。
> - **GC 补引用**：若登记表用 `TArray<FTcsTriggerRow>`（值语义）则**无需** ARO 覆写；若改用 `TUniquePtr` 持行（为地址稳定）则**必须**补（本计划采用值语义 + 句柄，故不需要——但实施时若改变持有形态，此条立即生效）。

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

