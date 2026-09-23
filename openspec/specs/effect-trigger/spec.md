# effect-trigger Specification

## Purpose
TBD - created by archiving change add-effect-trigger-row. Update Purpose after archive.
## Requirements
### Requirement: 触发行数据形状

`TcsEffect` MUST 以**数据定义**承载"事件 → 效果链"的映射（D4-1 终版字段；行为由共享 Handler 承载，事件 struct 上不自绑 delegate——裁决 2a）：

- `FTcsEffectTriggerDef`（USTRUCT，**定义侧 = 纯配置**）字段：`EventTag: FGameplayTag`（订阅哪个事件——Tag 路由）/ `EventPayloadFilter: FInstancedStruct`（载荷预筛，先于条件求值）/ `Conditions: TArray<FInstancedStruct>`（门禁条件）/ `EffectChainId: FGameplayTag`（触发后执行的链 id，引用已登记链）/ `Priority: int32`（同 Tag 多行触发顺序，**大者先**）/ `ExecutionGate: ETcsExecutionGate`（执行闸——网络姿态挂点）/ `InterruptPriority: int32`（可打断哪些正在跑的链——R4 只存不裁）/ `GateTags: TArray<FGameplayTag>`（行级开关 Tag，运行时点灯控制整行）/ `bConditionMissIsSilent: bool`（条件未过是否静默）；
- **定义侧 MUST 为纯配置**（零运行期字段）——**MUST NOT 携带 `FTcsSourceHandle` 等运行期发号的句柄**（它们跨会话/跨机不同、MUST NOT 进内容资产）。定义侧因此**可作资产内容、可内联进 SkillDef/BuffDef**；
- 全部字段**独立可空**、零策略类（D4-1："各自独立可空，空 = 默认行为"）；`Scope` / `HandlerClass` **MUST NOT** 存在（D4-1 v2 砍除：目标归属由 `SelectTargets` 步骤或上下文默认目标表达；求值器本身就是全部行的共享 Handler）；
- `Cues: TArray<FGameplayTag>`（D4-1 原第 ⑨ 字段）**MUST NOT 存在**（2026-09-23 用户拍板删除）：TcsCue 模块整体未敲定（`Source/TcsCue` 不存在），留一个填了没有任何消费者的字段 = 给策划一个假控件（与同批删除的修正器族死字段 `Tag` 同款理由）。**TcsCue 落地时加回**；
- `ETcsExecutionGate`（UENUM）MUST 至少含 `TEG_Always = 0`（恒通过）；**R4 只实现该值**，其余取值（如仅权威侧）随网络姿态轮——枚举值 MUST NOT 以"末位追加"以外的顺序变化（Custom 逃逸位规约：值 0 = 默认）。

#### Scenario: 定义侧字段集完整且独立可空

- **WHEN** 构造一个仅填 `EventTag` 与 `EffectChainId` 的 `FTcsEffectTriggerDef`
- **THEN** 其余字段取默认值（空 tag / 空数组 / `Priority = 0` / `ExecutionGate = TEG_Always` / `bConditionMissIsSilent = true`），定义合法

#### Scenario: 定义侧不含运行期字段

- **WHEN** 检查 `FTcsEffectTriggerDef` 的字段集
- **THEN** 不含任何 `FTcsSourceHandle` 或其它运行期发号句柄（定义侧可安全进资产）

#### Scenario: 不含已砍除与已删除字段

- **WHEN** 检查 `FTcsEffectTriggerDef` 的字段集
- **THEN** 不含 `Scope` / `HandlerClass`（D4-1 v2 砍除）与 `Cues`（2026-09-23 删除——无消费者）

### Requirement: 触发条件最小集

`TcsEffect` MUST 以**纯数据谓词 + 求值器注册表**承载触发条件（D4-5 条件最小集）：

- `FTcsTriggerCondition_HasAllTags`：`Tags: TArray<FGameplayTag>`——触发上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）；
- `FTcsTriggerCondition_Chance`：`Probability: double`（[0,1]）——**随机值由调用方注入**（D0-1 确定性纪律：求值器内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）；
- `FTcsTriggerContext`（USTRUCT，**触发期最小上下文**）：`EventTag` / `ClassificationTags` / `Caster`——只含本批条件真正需要的字段；
- **分派走注册表** `FTcsTriggerConditionRegistry`：`Register(const UScriptStruct*, FTcsTriggerConditionTest)` 动态入口 + `UE_DECLARE/DEFINE_TRIGGER_CONDITION_EVALUATOR` 自注册宏对；键 = 条件 struct 的反射类型；`Find` 未命中返回 nullptr（**不 ensure**——由求值助手处置）；同类型重复登记 MUST 拒绝（ensure + 保留首个）；
- **内置条件 MUST 走同一注册表**（经宏自登记）——**MUST NOT 存在"内置 if-else + 宿主注册表"两套路径**（两套路径必然导致行为分歧）；
- `EvaluateTriggerConditions(Conditions, Context, RandomValue = 0.0) -> bool`：全部条件通过 → `true`；任一不过 → `false`（**短路**）；**未注册的条件类型 → 视为不过 + Warning 日志**（MUST NOT 静默通过——静默会让"条件类型未注册/写错"表现成"条件通过"）；
- 条件类型之间**无公共基类、零虚函数**（D4-16 纯数据）——这是**脚本友好的必要条件**：虚分派在脚本侧物理不可达（见下）。

**为什么是注册表而非 USTRUCT 虚分派基类（2026-09-23，依据 `2026-09-23-scripting-language-ustruct-research.md` §5–§6）**：虚分派依赖 vtable，而 vtable 来自 UHT 为 **C++ 类型**生成的 `TCppStructOps<T>`（`Class.h:2265`）；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr`（`Class.cpp:3120-3144`）→ 实例内存由 `FMemory::Memzero` 起步、**vtable 指针位为 0** → `GetStructPtr<T>` 重解释后调用 = **野调用**。故虚分派不可被脚本层扩展，而注册表分派可达（§6）。**附带收益**：TCS 内部对"按类型分派"本有两套机制（步骤 = 注册表、参数源/选择器/过滤器 = 虚分派），条件归注册表侧不再加深该分裂。

**与流程侧条件的分工（MUST NOT 混用）**：`TcsDamage` 的 `FTcsConditionHasAllTags` / `FTcsConditionChance` 服务于**流程步骤**（上下文 `FTcsDamageFlowContext`）；本能力的 `FTcsTriggerCondition_*` 服务于**触发行**（上下文 `FTcsTriggerContext`）。`TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律 `Core←Attribute←Effect←{Damage,…}`），故两侧各自持有条件类型。

**反射面欠账（明示，非遗漏）**：`Register` 目前是纯 C++ 面（`FTcsTriggerConditionTest` 是 `TFunction`，不可反射）。设计意图（D4-17 双入口之反射面）要求它可被脚本层触达——该欠账与步骤执行器注册表**同批**解决，**MUST NOT** 在此处单独开一个反射入口（否则两处口径不一）。

#### Scenario: 全部条件通过才触发

- **WHEN** 一行的条件为 `[HasAllTags([A, B]), Chance(1.0)]`，上下文分类 Tag 集含 A 与 B，注入随机值 0.5
- **THEN** 求值返回 `true`

#### Scenario: 任一条件不过即不触发

- **WHEN** 条件为 `[HasAllTags([A, B])]` 而上下文只含 A
- **THEN** 求值返回 `false`（**不求值后续条件**——短路）

#### Scenario: 未注册条件类型不静默通过

- **WHEN** 条件数组含一个未注册求值器的 struct 类型
- **THEN** 求值返回 `false` + 留 Warning 日志（含类型名）

#### Scenario: 宿主新增条件类型零插件改动

- **WHEN** 宿主定义一个纯数据条件 struct，并用自注册宏登记其求值器
- **THEN** 该条件类型可被求值助手分派（无需修改插件源码）

#### Scenario: 重复登记被拒

- **WHEN** 对同一条件类型再次 `Register`
- **THEN** 拒绝并保留首个登记，留 ensure 提示

#### Scenario: 概率条件依赖注入的随机值

- **WHEN** `Chance(0.5)` 分别以注入随机值 0.4 与 0.6 求值
- **THEN** 前者通过、后者不过——**同一输入恒得同一结果**（求值器内部不取随机数）

### Requirement: 触发行实例与级联退订

`TcsEffect` MUST 以**运行期形态**承载已登记的触发行——定义与运行期簿记**分层**（2026-09-23 用户拍板）：

- `FTcsEffectTriggerInstance`（USTRUCT）字段：`Def: FTcsEffectTriggerDef`（定义）+ `Source: FTcsSourceHandle`（级联退订锚点）+ `Self: FTcsEffectTriggerHandle`（自身句柄）；
- `FTcsEffectTriggerHandle`（USTRUCT）以 `TTcsInstanceHandle<FTcsEffectTriggerTag>` 作下标句柄（类型区分标签防与链运行态句柄互换）；
- **`Source` 的语义取决于"行怎么被登记"**：定义加载期登记（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄；**施加状态时登记（Buff 行为）→ `Source` = 该状态实例句柄**。后者是 `09 §2.3`"订阅随 Source 生命周期自动退订——'状态在，修改器就在'天然成立"的机制落点；
- 级联退订：按 `Source` 一次性摘除该来源登记的全部行（与 M2 `RemoveBySource` 同款语义）；
- **持有形态**：登记表以 `TArray<FTcsEffectTriggerInstance>`（值语义）持有——触发行无高频增删、无挂起语义，**MUST NOT** 引入实例池（池的代际校验在此是零收益的复杂度）。**代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有实例指针**——求值器每次从登记表按下标重解析（与链解释器"每步入器前重解析"同款纪律）。

#### Scenario: 同一来源的行可被一次性摘除

- **WHEN** 同一来源登记多行触发行
- **THEN** 各实例的 `Source` 相等，可按该句柄一次性全量摘除

#### Scenario: 状态施加期登记的行走状态生命周期

- **WHEN** 以某状态实例句柄为 `Source` 登记触发行（Buff 行为形态），随后该状态被移除
- **THEN** 按该句柄级联退订后，该来源登记的行不再触发（"状态在，行为就在"）

#### Scenario: 实例携带自身句柄

- **WHEN** 检查一个已登记实例
- **THEN** `Self` 为该实例在登记表内的有效下标句柄（供退订/点灯/求值器回填上下文使用）

