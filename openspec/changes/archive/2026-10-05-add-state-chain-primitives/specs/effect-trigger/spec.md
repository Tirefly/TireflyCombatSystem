## MODIFIED Requirements

### Requirement: 触发条件最小集

`TcsEffect` MUST 以**纯数据谓词 + 求值器注册表**承载触发条件（D4-5 条件最小集）：

- `FTcsTriggerCondition_HasAllTags`：`Tags: TArray<FGameplayTag>`——触发上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）；
- `FTcsTriggerCondition_Chance`：`Probability: double`（[0,1]）——**随机值由调用方注入**（D0-1 确定性纪律：求值器内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）；
- `FTcsTriggerCondition_AttributeCompare`（2026-10-05 新增，第三条内置条件）：`{ Attribute, Comparison, Threshold }`——语义、失败面与场景见下条「属性比较条件（AttributeCompare）」需求；
- `FTcsTriggerContext`（USTRUCT，**触发期最小上下文**）：`EventTag` / `ClassificationTags` / `Caster` + **`World`（2026-10-05 新增）**——只含本批条件真正需要的字段。`World` 是**非 `UPROPERTY` 的裸指针**（触发期瞬时读数，不序列化、不进资产）：需要取门面/读账本的条件（`AttributeCompare`）靠它解析世界；由构造上下文的求值器（`UTcsTriggerEvaluator`）填入，未填时相关条件按"不可求值"处理；
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

**与流程侧条件的分工（MUST NOT 混用）**：`TcsDamage` 的 `FTcsConditionHasAllTags` / `FTcsConditionChance` 服务于**流程步骤**（上下文 `FTcsDamageFlowContext`）；本能力的 `FTcsTriggerCondition_*` 服务于**触发行**（上下文 `FTcsTriggerContext`）。`TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律 `Core←Attribute←Effect←{Damage,…}`），故两侧各自持有条件类型。**两个条件类型各自持有、但匹配的词同根**（`DamageCategory`）——"类型不能共用"是依赖方向约束，"词必须共用"是消费角色约束，两者不矛盾。**判据边界**：求值上下文里的 `World` 是触发侧专属字段（流程侧上下文另有自己的世界通路），两侧 MUST NOT 互相替换上下文。

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

## ADDED Requirements

### Requirement: 属性比较条件（AttributeCompare）

`TcsEffect` MUST 提供第三条内置触发条件 `FTcsTriggerCondition_AttributeCompare`（`D4-5` 剩余条件之一，2026-10-05 落地）：

- 条件数据：`Attribute: FGameplayTag` / `Comparison: ETcsAttributeComparison`（词住 `TcsAttribute`，见 `attribute-types` 能力的「属性值比较枚举」）/ `Threshold: double`；
- 求值 MUST 读**触发上下文 `Caster`** 的该属性**当前值**——经 `UTcsTriggerEvaluator` 填入的 `Context.World` 解析属性门面（口径与白名单见 `attribute-pipeline` 能力的「属性门面的解析点」），MUST NOT 读基值 / 未提交候选值；
- 求值 MUST NOT 取随机数、MUST NOT 复用触发行的随机流（注入的 `RandomValue` 对本条件无意义）；
- **失败面分两档（都不 ensure）**：
  - `Attribute` 无效 / `Caster` 无效 / `Context.World` 为空 / 该单位无属性账本 ⇒ 返回"不通过"，**零红字**（与"条件未过不是故障"的既有口径一致）；
  - **`Attribute` 有效但该单位账本上没有这个属性键 ⇒ 返回"不通过" + 一条含属性键的 `Warning`**——`EvaluateCurrent` 对"键不存在"与"值恰为 0"给同一个读数（0.0），不区分就会让"键写错"表现成"按 0 参与比较"（可能与阈值比较后**静默通过**）。该红字属**拒绝面**（常规验收命令 MUST NOT 覆盖这条路径）；
- 登记 MUST 走既有 `FTcsTriggerConditionRegistry` 与自注册宏（`MUST NOT` 存在"内置 if-else + 宿主注册表"两套路径）。

#### Scenario: 属性当前值满足比较时通过

- **WHEN** `Caster` 的属性 A 当前值为 10，条件为 `AttributeCompare(A, GreaterOrEqual, 10)`
- **THEN** 条件通过（该行其余条件都过时起链）

#### Scenario: 属性当前值不满足比较时不通过

- **WHEN** 同上但条件为 `AttributeCompare(A, Less, 10)`
- **THEN** 条件不通过（不起链），且**不产生任何红字**

#### Scenario: 属性键在账本上不存在时不静默按 0 比较

- **WHEN** 条件引用一个该单位账本上不存在的属性键（且方向与阈值使得"0 通过"成立，如 `Less(10)`）
- **THEN** 条件判为**不通过**并留一条含该属性键的 `Warning`——MUST NOT 静默按 0.0 比较（那会让"键写错"表现成"条件通过"）

#### Scenario: 无世界读数时不 ensure

- **WHEN** 上下文未填 `World`（或该世界无属性子系统）时求值本条件
- **THEN** 返回"不通过"，不 ensure、不崩溃
