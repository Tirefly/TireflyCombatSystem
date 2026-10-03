## MODIFIED Requirements

### Requirement: 触发条件最小集

`TcsEffect` MUST 以**纯数据谓词 + 求值器注册表**承载触发条件（D4-5 条件最小集）：

- `FTcsTriggerCondition_HasAllTags`：`Tags: TArray<FGameplayTag>`——触发上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）；
- `FTcsTriggerCondition_Chance`：`Probability: double`（[0,1]）——**随机值由调用方注入**（D0-1 确定性纪律：求值器内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）；
- `FTcsTriggerContext`（USTRUCT，**触发期最小上下文**）：`EventTag` / `ClassificationTags` / `Caster`——只含本批条件真正需要的字段；
- **分类集词汇的根（2026-10-01 换根）**：`FTcsTriggerCondition_HasAllTags::Tags` 与 `FTcsTriggerContext::ClassificationTags` 里的词 MUST 落在 **`DamageCategory`** 根下——消费角色 = **供条件匹配的伤害分类集**；声明方 = 宿主 `Config/DefaultGameplayTags.ini`（词由宿主的 `ITcsDamageFlowDelegate::Execute_ResolveElement` 实现决定）；形态 `DamageCategory.<词>`（2 段）。**该根由触发侧与流程侧两个匹配面共用**（流程侧 = `damage-step-library` 的 `FTcsConditionHasAllTags`），MUST NOT 为两侧各立一根——两处只是**同一套词的两个读取点**，词的归属只有一份（判据边界见 `gameplay-tag-governance` 的「一角色一根」与根段注册表备注）。插件 MUST NOT 声明任何具体分类词；
- **分派走注册表** `FTcsTriggerConditionRegistry`：`Register(const UScriptStruct*, FTcsTriggerConditionTest)` 动态入口 + `UE_DECLARE/DEFINE_TRIGGER_CONDITION_EVALUATOR` 自注册宏对；键 = 条件 struct 的 `const UScriptStruct*`（**按 struct 类型分派**）；`Find` 未命中返回 nullptr（**不 ensure**——由求值助手处置）；同类型重复登记 MUST 拒绝（ensure + 保留首个）；
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤）**：本注册表同为**进程级单例**且 MUST 保持如此；其**动态**登记项 MUST 受与步骤执行器注册表**同款**的寿命约束——MUST 记录宿主对象弱引用 + 登记世界弱引用；**静态自注册的纯函数项 MUST NOT 受寿命约束**（内置条件全部属此类，永不过期）；
  - **失效判据**（任一即失效）：宿主弱引用为空 / 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询侧校验**：`Find` MUST 接受可选的世界校验入参（调用方持有世界时 MUST 显式传入）；跨世界失效 MUST 视为未命中（返回 nullptr）并移除该条目 + 留含类型名的 **Warning** 日志，MUST NOT 静默按"未登记"处理（静默会把"世界已更换"表现成"条件类型未注册"）；
  - **拒绝门收窄（"同世界活对象重复"）**：既有条目已失效时新登记 MUST **替换**而 MUST NOT 拒绝；仅当既有条目有效且属同世界（或为静态自注册项）时才 MUST 拒绝；
  - **显式移除入口**：MUST 提供按键移除动态条目的入口，MUST NOT 能移除静态项；缺省不调用时正确性 MUST NOT 受影响。
  - **本条特此约束未来新增的动态入口**：当 `LEDGER-reflection` R-2 为条件求值器新增**宿主脚本插槽**（UObject 基类形态）时，其登记入口 MUST 直接满足上述寿命语义——MUST NOT 先按旧口径落地再返工；
- **内置条件 MUST 走同一注册表**（经宏自登记）——**MUST NOT 存在"内置 if-else + 宿主注册表"两套路径**（两套路径必然导致行为分歧）；
- `EvaluateTriggerConditions(Conditions, Context, RandomValue = 0.0) -> bool`：全部条件通过 → `true`；任一不过 → `false`（**短路**）；**未注册的条件类型 → 视为不过 + Warning 日志**（MUST NOT 静默通过——静默会让"条件类型未注册/写错"表现成"条件通过"）；
- 条件类型之间**无公共基类、零虚函数**（D4-16 纯数据）——这是**脚本友好的必要条件**：虚分派在脚本侧物理不可达（见下）。

**为什么是注册表而非 USTRUCT 虚分派基类（2026-09-23，依据 `2026-09-23-scripting-language-ustruct-research.md` §5–§6）**：虚分派依赖 vtable，而 vtable 来自 UHT 为 **C++ 类型**生成的 `TCppStructOps<T>`（`Class.h:2265`）；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr`（`Class.cpp:3120-3144`）→ 实例内存由 `FMemory::Memzero` 起步、**vtable 指针位为 0** → `GetStructPtr<T>` 重解释后调用 = **野调用**。故虚分派不可被脚本层扩展，而注册表分派可达（§6）。**附带收益**：TCS 内部对"按类型分派"本有两套机制（步骤 = 注册表、参数源/选择器/过滤器 = 虚分派），条件归注册表侧不再加深该分裂。

**与流程侧条件的分工（MUST NOT 混用）**：`TcsDamage` 的 `FTcsConditionHasAllTags` / `FTcsConditionChance` 服务于**流程步骤**（上下文 `FTcsDamageFlowContext`）；本能力的 `FTcsTriggerCondition_*` 服务于**触发行**（上下文 `FTcsTriggerContext`）。`TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律 `Core←Attribute←Effect←{Damage,…}`），故两侧各自持有条件类型。**两个条件类型各自持有、但匹配的词同根**（`DamageCategory`）——"类型不能共用"是依赖方向约束，"词必须共用"是消费角色约束，两者不矛盾。

**脚本可达面欠账（明示，非遗漏；2026-09-29 口径更新）**：`Register` 目前是纯 C++ 面（`FTcsTriggerConditionTest` 是 `TFunction`——**无 `USTRUCT` 宏、无法进 UHT 签名面**，故方法无法标 `UFUNCTION()`）。设计意图（D4-17 双入口之脚本可达面）要求它可被脚本层触达。**原口径为"与步骤执行器注册表同批解决"**——因步骤执行器注册表侧已于 SCRIPT-8（2026-09-27）先行落地脚本插槽，而条件求值器侧未随之落地，该口径已过期，现改为：**与载荷读取器同批，作为 `LEDGER-reflection` R-2 的独立提案落地**（2026-09-29 用户拍板，`DEC-04` 裁定 ①⑤）；本提案只先补齐两者的**寿命语义**，MUST NOT 借此新增脚本插槽（避免把两类改动混成一批、放大回归面）。

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

- **WHEN** 对同一条件类型在**同一世界**内、既有登记**仍有效**时再次 `Register`
- **THEN** 拒绝并保留首个登记，留 ensure 提示

#### Scenario: 概率条件依赖注入的随机值

- **WHEN** `Chance(0.5)` 分别以注入随机值 0.4 与 0.6 求值
- **THEN** 前者通过、后者不过——**同一输入恒得同一结果**（求值器内部不取随机数）

#### Scenario: 跨世界失效的条件登记被判定未命中

- **WHEN** 世界 A 登记的动态条件求值器随 A 结束而失效，世界 B 的求值器查询该类型
- **THEN** 返回 nullptr（视为未命中）并移除该条目，MUST NOT 解引用已失效对象

#### Scenario: 分类词与流程侧匹配面同根

- **WHEN** 检查触发行条件 `FTcsTriggerCondition_HasAllTags{Tags=[…]}` 里的词与流程步骤条件 `FTcsConditionHasAllTags` 里的词
- **THEN** 二者同住 `DamageCategory` 根（同一套宿主分类词的两个读取点）；插件 MUST NOT 为触发侧/流程侧各立一根，也不声明任何具体分类词

### Requirement: 触发行登记表与订阅生命周期

`TcsEffect` MUST 以门面 API 承载触发行的登记与摘除，并按 `EventTag` 装配总线订阅：

- `RegisterTriggerRow(const FTcsEffectTriggerInstance&) -> FTcsEffectTriggerHandle`：登记表持有（值语义）+ **按 `Def.EventTag` 装配总线订阅**。`EventTag` 或 `EffectChainId` 无效时 ensure + 返回无效句柄（配置错误，不静默接受）；
- `UnregisterTriggerRow(FTcsEffectTriggerHandle) -> bool`：摘除单行（先校验代际——失配即拒 + 返回 false，不 ensure）；
- `UnregisterTriggerRowsBySource(const FTcsSourceHandle&) -> int32`：按来源全量摘除（级联退订锚点，与 M2 `RemoveBySource` 同款语义）；
- **订阅计数配对（核心纪律）**：同一 `EventTag` 的多行**共用一条订阅**——首次出现该 Tag 时订阅一次，该 Tag 的行数归零时才退订。**MUST NOT** 每行各订一次（那会让总线订阅表随行数膨胀且退订易漏）；
- **订阅通道 = 立即**（`ETcsEventDispatch::EED_Immediate`）：收集协议要求"修正提交落在事件发布返回之前"（`TcsDamage` 的收集事件走立即通道），订帧末会让修正晚一拍；
- **点灯 API**：`SetTriggerGateTag(FGameplayTag, bool)` / `IsTriggerGateTagLit(FGameplayTag) const`——行级开关（`GateTags` 全部点亮才通过该道门）；**这两条 API 的 `FGameplayTag` 形参即 `EffectTriggerGate` 根的唯一消费者**（角色 = 触发行的行级开关；声明方 = 宿主 `Config/DefaultGameplayTags.ini`；形态 `EffectTriggerGate.<词>`，2 段；根段注册表见 `gameplay-tag-governance`）——`FTcsEffectTriggerDef::GateTags` 里的词与点灯时传入的词 MUST 同住该根；插件 MUST NOT 声明任何具体开关词；
- **观测 API**：`GetTriggerRowCount() const`（装置断言用）；
- **求值器生命周期**：由门面在首次登记时创建并 **`UPROPERTY` 持有**——总线订阅表持**弱引用**（`FTcsEventSubscription::Handler` 是 `TWeakObjectPtr`），不 root 会被 GC 掉、订阅静默失效；
- `Deinitialize` MUST 清空登记表 + **全量退订**（不留跨世界残留订阅）；
- **脚本层可达（2026-09-24 补）**：上列方法中**形参均可作 `UFUNCTION` 形参**（即形参类型为 `USTRUCT()` 或原生可承载类型）的部分（`RegisterTriggerRow` / `UnregisterTriggerRow` / `SetTriggerGateTag` / `IsTriggerGateTagLit` / `GetTriggerRowCount` / `SetTriggerRandomSeed`）MUST 标记 `UFUNCTION()`（无 specifier，口径见 `effect-interpreter` 的「门面脚本可达面」需求——该需求旧标题为「门面反射面」，SCRIPT-8 归档后以新名为准）——这是"宿主用 C# 写技能/Buff 逻辑"的登记入口。**形参含无 `USTRUCT` 宏的裸 struct 的 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)` 不在其列**（需先给 `FTcsSourceHandle` 加 `USTRUCT()`）。措辞口径见 `Documents/combat-system-design/ledger/reflection-terminology.md`。

#### Scenario: 同一事件 Tag 的多行共用一个订阅

- **WHEN** 连续登记 3 行，`EventTag` 均为 T
- **THEN** 总线订阅表中 Tag = T 的订阅恰有 1 条（而非 3 条）

#### Scenario: 末行摘除才退订

- **WHEN** 上述 3 行先摘 1 行、再摘 1 行
- **THEN** Tag = T 的订阅仍在（还有 1 行）；摘除最后 1 行后该订阅被退订

#### Scenario: 无效配置被拒

- **WHEN** 以空 `EventTag`（或空 `EffectChainId`）调 `RegisterTriggerRow`
- **THEN** 返回无效句柄 + 留 ensure 提示，登记表行数不变

#### Scenario: 求值器被 GC 持有

- **WHEN** 登记过至少一行后检查门面的持有面
- **THEN** 求值器对象被 `UPROPERTY` 持有（不因无强引用而被回收——否则订阅静默失效）

#### Scenario: 反初始化全量退订

- **WHEN** 门面 `Deinitialize`（世界销毁 / PIE 结束）
- **THEN** 登记表清空且全部订阅被退订

#### Scenario: 脚本层可登记触发行

- **WHEN** 宿主脚本层（C#）调 `RegisterTriggerRow` 并传入一个 `FTcsEffectTriggerInstance`
- **THEN** 调用成立（方法反射可见、形参类型 `FTcsEffectTriggerInstance` 反射可见），登记表行数 +1

#### Scenario: 开关词住 `EffectTriggerGate` 根

- **WHEN** 检查 `FTcsEffectTriggerDef::GateTags` 里的词与 `SetTriggerGateTag` 的实参形态
- **THEN** 二者同住 `EffectTriggerGate` 根（2 段）；该根由这两条点灯 API 独占消费，MUST NOT 与 `DamageCategory`（分类匹配）或 `TcsEvent`（事件路由）共根
