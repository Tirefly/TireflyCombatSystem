## MODIFIED Requirements

### Requirement: 触发条件最小集

`TcsEffect` MUST 以**纯数据谓词 + 求值器注册表**承载触发条件（D4-5 条件最小集）：

- `FTcsTriggerCondition_HasAllTags`：`Tags: TArray<FGameplayTag>`——触发上下文分类 Tag 集须含**全部**给定 Tag（空数组 = 无条件通过）；
- `FTcsTriggerCondition_Chance`：`Probability: double`（[0,1]）——**随机值由调用方注入**（D0-1 确定性纪律：求值器内部 MUST NOT 取随机数，否则同输入不同输出、回放失效）；
- `FTcsTriggerCondition_AttributeCompare`（2026-10-05 新增，第三条内置条件）：`{ Attribute, Comparison, Threshold }`——语义、失败面与场景见下条「属性比较条件（AttributeCompare）」需求；
- `FTcsTriggerContext`（`USTRUCT(BlueprintType)`，**触发期最小上下文**）：`EventTag` / `ClassificationTags` / `Caster` + **`World`（2026-10-05 新增）**——只含本批条件真正需要的字段。`World` 是**非 `UPROPERTY` 的裸指针**（触发期瞬时读数，不序列化、不进资产）：需要取门面/读账本的条件（`AttributeCompare`）靠它解析世界；由构造上下文的求值器（`UTcsTriggerEvaluator`）填入，未填时相关条件按"不可求值"处理。**2026-10-06 升格 `BlueprintType`**（`R-2` 后段）：本类型作宿主脚本插槽 `Test` 的形参，`BlueprintNativeEvent` 触发 UHT 蓝图参数校验 ⇒ 非 `BlueprintType` 无法作该签名形参；
- **分类集词汇的根（2026-10-01 换根）**：`FTcsTriggerCondition_HasAllTags::Tags` 与 `FTcsTriggerContext::ClassificationTags` 里的词 MUST 落在 **`DamageCategory`** 根下——消费角色 = **供条件匹配的伤害分类集**；声明方 = 宿主 `Config/DefaultGameplayTags.ini`（词由宿主的 `ITcsDamageFlowDelegate::Execute_ResolveElement` 实现决定）；形态 `DamageCategory.<词>`（2 段）。**该根由触发侧与流程侧两个匹配面共用**（流程侧 = `damage-step-library` 的 `FTcsConditionHasAllTags`），MUST NOT 为两侧各立一根——两处只是**同一套词的两个读取点**，词的归属只有一份（判据边界见 `gameplay-tag-governance` 的「一角色一根」与根段注册表备注）。插件 MUST NOT 声明任何具体分类词；
- **分派走注册表** `FTcsTriggerConditionRegistry`：`Register(const UScriptStruct*, FTcsTriggerConditionTest)` 动态入口 + `UE_DECLARE/DEFINE_TRIGGER_CONDITION_EVALUATOR` 自注册宏对；键 = 条件 struct 的 `const UScriptStruct*`（**按 struct 类型分派**）；`Find` 未命中返回 nullptr（**不 ensure**——由求值助手处置）；同类型重复登记 MUST 拒绝（ensure + 保留首个）；
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤）**：本注册表同为**进程级单例**且 MUST 保持如此；其**动态**登记项 MUST 受与步骤执行器注册表**同款**的寿命约束——MUST 记录宿主对象弱引用 + 登记世界弱引用；**静态自注册的纯函数项 MUST NOT 受寿命约束**（内置条件全部属此类，永不过期）；
  - **失效判据**（任一即失效）：宿主弱引用为空 / 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询侧校验**：`Find` MUST 接受可选的世界校验入参（调用方持有世界时 MUST 显式传入）；跨世界失效 MUST 视为未命中（返回 nullptr）并移除该条目 + 留含类型名的 **Warning** 日志，MUST NOT 静默按"未登记"处理（静默会把"世界已更换"表现成"条件类型未注册"）；
  - **拒绝门收窄（"同世界活对象重复"）**：既有条目已失效时新登记 MUST **替换**而 MUST NOT 拒绝；仅当既有条目有效且属同世界（或为静态自注册项）时才 MUST 拒绝；
  - **显式移除入口**：MUST 提供按键移除动态条目的入口，MUST NOT 能移除静态项；缺省不调用时正确性 MUST NOT 受影响。
  - **本条特此约束未来新增的动态入口（2026-10-06 兑现）**：`LEDGER-reflection` R-2 的**宿主脚本插槽已落地**——其登记入口直接满足上述寿命语义，未按旧口径落地再返工（见下条「条件求值器宿主脚本插槽」）；
- **宿主脚本插槽（2026-10-06 新增，`R-2` 后段落地）**：MUST 提供 `ITcsTriggerConditionEvaluator`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`），签名 `bool Test(const FInstancedStruct& ConditionData, const FTcsTriggerContext& Context, double RandomValue)`；门面 `RegisterConditionEvaluator(const UScriptStruct*, TScriptInterface<ITcsTriggerConditionEvaluator>)` MUST 把宿主对象包成 `FTcsTriggerConditionTest` 转发进**本注册表**（键与查表逻辑零改动）；
  - **宿主实现载体 = `UINTERFACE`，MUST NOT 用 `UCLASS` 基类、MUST NOT 造 `USTRUCT` 转发器**：前者让宿主**既有类**直接实现插槽（一个类可同时实现多个插槽，不必为每个插槽专造对象）；后者在此**无对象可接**——转发器的用途是把"脚本结构体无 C++ 类型（`CppStructOps == nullptr` ⇒ vtable 位为 0）"接进 `USTRUCT` 虚分派体系，而本注册值的类型是 `TFunction`、**没有策略基类可继承**；自造策略基类 = 改注册值类型 = SCRIPT-8 刻意绕开的 SCRIPT-2 路线；
  - **GC 与寿命（两半都要）**：宿主对象 MUST 由门面以 `UPROPERTY` 数组**强持有**（裸 C++ 注册表持不住对象引用，不持有则被静默回收，表现为"条件不生效"而非崩溃）；**且**其对象弱引用与登记世界弱引用 MUST 交给本注册表（`TScriptInterface` 本身不构成 GC 强引用，不满足前一条即被回收；只强持有不记弱引用则跨世界留 stale）。拒绝面：键为空 / 宿主对象为空 / 同世界活对象重复登记（保留首个）；
- **内置条件 MUST 走同一注册表**（经宏自登记）——**MUST NOT 存在"内置 if-else + 宿主注册表"两套路径**（两套路径必然导致行为分歧）；**内置路径的注册值类型与查表逻辑 MUST NOT 因插槽落地而变动**（双轨并存：内置走 C++ 快路径零反射开销，插槽只服务宿主扩展，多一次 `UFunction::Invoke` 是明示接受的取舍）；
- `EvaluateTriggerConditions(Conditions, Context, RandomValue = 0.0) -> bool`：全部条件通过 → `true`；任一不过 → `false`（**短路**）；**未注册的条件类型 → 视为不过 + Warning 日志**（MUST NOT 静默通过——静默会让"条件类型未注册/写错"表现成"条件通过"）；
- 条件类型之间**无公共基类、零虚函数**（D4-16 纯数据）——这是**脚本友好的必要条件**：虚分派在脚本侧物理不可达（见下）。

**为什么是注册表而非 USTRUCT 虚分派基类（2026-09-23，依据 `2026-09-23-scripting-language-ustruct-research.md` §5–§6）**：虚分派依赖 vtable，而 vtable 来自 UHT 为 **C++ 类型**生成的 `TCppStructOps<T>`（`Class.h:2265`）；C# 定义的结构体没有 C++ 类型 → `CppStructOps == nullptr`（`Class.cpp:3120-3144`）→ 实例内存由 `FMemory::Memzero` 起步、**vtable 指针位为 0** → `GetStructPtr<T>` 重解释后调用 = **野调用**。故虚分派不可被脚本层扩展，而注册表分派可达（§6）。**附带收益**：TCS 内部对"按类型分派"本有两套机制（步骤 = 注册表、参数源/选择器/过滤器 = 虚分派），条件归注册表侧不再加深该分裂。

**与流程侧条件的分工（MUST NOT 混用）**：`TcsDamage` 的 `FTcsConditionHasAllTags` / `FTcsConditionChance` 服务于**流程步骤**（上下文 `FTcsDamageFlowContext`）；本能力的 `FTcsTriggerCondition_*` 服务于**触发行**（上下文 `FTcsTriggerContext`）。`TcsEffect` MUST NOT 依赖 `TcsDamage`（依赖铁律 `Core←Attribute←Effect←{Damage,…}`），故两侧各自持有条件类型。**两个条件类型各自持有、但匹配的词同根**（`DamageCategory`）——"类型不能共用"是依赖方向约束，"词必须共用"是消费角色约束，两者不矛盾。**判据边界**：求值上下文里的 `World` 是触发侧专属字段（流程侧上下文另有自己的世界通路），两侧 MUST NOT 互相替换上下文。

**脚本可达面（2026-10-06 已落地，原欠账关闭）**：`Register` 的纯 C++ 面（`FTcsTriggerConditionTest` 是 `TFunction`——无 `USTRUCT` 宏、无法进 UHT 签名面）**保持原样不变**；脚本可达性由**独立插槽**提供（`ITcsTriggerConditionEvaluator` + 门面 `RegisterConditionEvaluator`，形参全反射 ⇒ 可标 `UFUNCTION()`）。**设计意图（D4-17 双入口之脚本可达面）由此满足**，且**未采用**"换 `TFunction` 签名"路线（SCRIPT-2；SCRIPT-8 已确立"以 UObject 反射载体替代 `TFunction` 作注册值、改动面更小"的判据——本条按该判据落地，载体取 `TScriptInterface`）。

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

#### Scenario: 宿主脚本求值器可被登记并由求值器分派

- **WHEN** 宿主用任意 UE 脚本语言（C# / AS / Lua / TS / 蓝图）实现 `ITcsTriggerConditionEvaluator` 并调用门面 `RegisterConditionEvaluator` 登记其条件 struct
- **THEN** 求值助手在处理该 struct 的条件时分派到宿主实现（内置条件的 C++ 快路径不受影响）；**宿主实现载体是接口**（宿主既有类可直接实现，无需为插槽新建对象）

#### Scenario: 宿主脚本求值器被 GC 强持有且受世界寿命约束

- **WHEN** 宿主登记脚本求值器后，在同一世界内经垃圾回收继续求值；随后世界结束、同类查询在世界 B 发生
- **THEN** 回收后求值仍分派到该宿主对象（**MUST NOT** 被静默回收——门面 `UPROPERTY` 强持有）；世界 B 的查询视为未命中并移除该条目（对象/世界弱引用判失效），且 MUST NOT 解引用已失效对象

#### Scenario: 插槽登记不改动内置路径与注册值类型

- **WHEN** 检查插槽落地后内置条件（`HasAllTags` / `Chance` / `AttributeCompare`）的登记方式与 `FTcsTriggerConditionTest` 注册值类型
- **THEN** 三者仍经静态自注册宏登记、注册值类型仍为 `TFunction`（骨架零改动），MUST NOT 因插槽而改为 `USTRUCT` 策略载体

### Requirement: 触发载荷读取器

`TcsEffect` MUST 以**载荷读取器注册表**承载"从事件载荷里读出主体信息"的能力——`TcsEffect` 全程**不认识任何领域载荷类型**（依赖铁律 `Core←Attribute←Effect←{Damage,…}`，04 §1）：

- `FTcsTriggerPayloadInfo`（`USTRUCT(BlueprintType)`，反射）：`Caster: FTcsCombatEntityHandle` / `ClassificationTags: TArray<FGameplayTag>` / `Subject: FInstancedStruct`（`UPROPERTY`，默认空）——单位身份供条件求值，分类集供标签匹配，实例主体仅供事件路由；MUST NOT 在 TcsEffect 中 include 状态句柄类型或硬编码领域载荷类型。**2026-10-06 升格 `BlueprintType`**（`R-2` 后段）：本类型作宿主脚本插槽 `Read` 的返回类型与 `Test` 的形参族，`BlueprintNativeEvent` 触发 UHT 蓝图参数校验 ⇒ 非 `BlueprintType` 无法进该签名面；
- 读取器签名：`TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct& Payload)>`；
- `FTcsTriggerPayloadReaderRegistry`：`AddPending`（静态自注册）+ `Register(const UScriptStruct*, Reader)`（动态入口，同类型重复登记 MUST 拒绝）+ `Find`（未命中返回 nullptr，**不 ensure**）；
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤）**：本注册表同为**进程级单例**且 MUST 保持如此；其**动态**登记项 MUST 受与步骤执行器注册表**同款**的寿命约束——**MUST 记录宿主对象弱引用 + 登记世界弱引用**；静态自注册的纯函数项（含由属主模块登记的领域读取器）MUST NOT 受寿命约束；
  - **失效判据**（任一即失效）：宿主弱引用为空 / 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询侧校验**：`Find` MUST 接受可选的世界校验入参（调用方持有世界时 MUST 显式传入）；跨世界失效 MUST 视为未命中（返回 nullptr）并移除该条目 + 留含类型名的 **Warning** 日志。**注意与"未注册载荷类型"的处置区分**：后者按既有口径走"默认构造 + **Verbose**"（不是契约违规），而前者是**世界已更换**——两者 MUST NOT 混为一谈；
  - **拒绝门收窄（"同世界活对象重复"）**：既有条目已失效时新登记 MUST **替换**而 MUST NOT 拒绝；仅当既有条目有效且属同世界（或为静态自注册项）时才 MUST 拒绝；
  - **显式移除入口**：MUST 提供按键移除动态条目的入口，MUST NOT 能移除静态项；缺省不调用时正确性 MUST NOT 受影响；
  - **本条特此约束未来新增的动态入口（2026-10-06 兑现）**：`LEDGER-reflection` R-2 的**宿主脚本插槽已落地**——其登记入口直接满足上述寿命语义，未按旧口径落地再返工（见下条「载荷读取器宿主脚本插槽」）；
- **宿主脚本插槽（2026-10-06 新增，`R-2` 后段落地）**：MUST 提供 `ITcsTriggerPayloadReader`（`UINTERFACE(MinimalAPI, Blueprintable)` + `UFUNCTION(BlueprintNativeEvent)`），签名 `FTcsTriggerPayloadInfo Read(const FInstancedStruct& Payload)`；门面 `RegisterPayloadReader(const UScriptStruct*, TScriptInterface<ITcsTriggerPayloadReader>)` MUST 把宿主对象包成读取器转发进**本注册表**（键与查表逻辑零改动）；
  - **宿主实现载体 = `UINTERFACE`**（同条件求值器插槽的四条理由：宿主既有类可挂、本注册值无策略基类可继承故转发器无对象可接、自造策略基类 = 改注册值类型 = SCRIPT-2 路线、语言可达性由宿主面是否为 `UObject` 反射类型决定）；
  - **GC 与寿命（两半都要）**：同条件求值器插槽——门面 `UPROPERTY` 强持有 + 对象/世界弱引用交注册表；`TScriptInterface` 本身不构成 GC 强引用；
  - **"每事件一次"语义 MUST 保住**：`ReadPayloadInfo` 已在求值器里优化为**各行共用、只读一次载荷**；本插槽转发**只替换"读取器"这一层**，MUST NOT 把读取点挪进每行循环；
- 自注册宏对 `UE_DECLARE/DEFINE_TRIGGER_PAYLOAD_READER`——**由载荷类型的属主模块登记**（已有的 TcsDamage 收集事件读取器住 TcsDamage，本轮新增的状态生命周期读取器住 TcsState；MUST NOT 宣称本轮是首个全局读取器）；
- **状态生命周期读取器**：TcsState MUST 为 `FTcsStateEventPayload` 静态自登记读取器（`Private/State/TcsStateEvents.cpp`，首个 include 为 `State/TcsStateEvents.h`，只依赖触发读取器注册表、不取效果门面）；`Caster` MUST 取有效 `Payload.Instigator`，否则取 `Payload.Unit`；`Subject` MUST 为 `FInstancedStruct::Make<FTcsStateHandle>(Payload.Handle)`。为满足无发起者时的单位回退，状态载荷 MUST 增加 `Unit: FTcsCombatEntityHandle`（`UPROPERTY(BlueprintReadOnly)`），广播构造点 MUST 从 `Instance.Unit` 复制；读取器直接读载荷快照，MUST NOT 回查正在移除或已被复用的状态实例。分类标签集仍为空（本轮不引入状态分类词）；
- **未注册载荷类型**：默认构造 `FTcsTriggerPayloadInfo`（空 Caster / 空标签集 / 空 Subject）+ **Verbose** 日志——**MUST NOT ensure**（"载荷类型未知"不是契约违规：手动发布一条自定义事件是合法用法）；
- **`ClassificationTags` 无来源时为空集**：这是**显式交付的语义**，不是遗漏——空集上匹配非空 Tag 数组的 `HasAllTags` 恒不过（条件的既有语义），故内容侧若要用该条件，MUST 有读取器提供标签集；既有 TcsDamage 读取器未提供 `Subject` 时仍保持其分类与 Caster 的现有语义。

#### Scenario: 属主模块自登记读取器

- **WHEN** 某领域模块为自己定义的事件载荷 struct 用自注册宏登记读取器
- **THEN** 求值器在收到该载荷时能取到其主体信息（无需修改 TcsEffect 源码）

#### Scenario: 未注册载荷类型不 ensure

- **WHEN** 求值器收到一个没有读取器的载荷类型
- **THEN** `Caster` 为空、标签集为空、`Subject` 为空，留一条 `Verbose` 日志，**MUST NOT** 出现 ensure / Warning / Error

#### Scenario: 重复登记被拒

- **WHEN** 对同一载荷类型在**同一世界**内、既有登记**仍有效**时再次 `Register`
- **THEN** 拒绝并保留首个登记，留 ensure 提示

#### Scenario: 跨世界失效的读取器登记被判定未命中

- **WHEN** 世界 A 登记的动态载荷读取器随 A 结束而失效，世界 B 的求值器读取同一载荷类型
- **THEN** 返回 nullptr（视为未命中）并移除该条目 + 留 Warning 日志，MUST NOT 解引用已失效对象；且 MUST NOT 与"未注册载荷类型"的 Verbose 路径混淆

#### Scenario: 状态载荷读取有效发起者与实例主体

- **WHEN** 状态载荷的 `Instigator` 有效，并携带 `Unit` 与 `Handle`
- **THEN** 读取结果的 `Caster` 等于 `Instigator`，`Subject` 内层类型为 `FTcsStateHandle`、值等于载荷的 `Handle`，无需 TcsEffect 依赖 TcsState

#### Scenario: 无发起者的状态载荷回退到所属单位

- **WHEN** 状态载荷的 `Instigator` 无效而 `Unit` 有效
- **THEN** 读取结果的 `Caster` 等于载荷的 `Unit`，`Subject` 仍是该实例句柄；不回查状态门面、不依赖实例是否已释放

#### Scenario: 宿主脚本读取器可被登记并参与求值

- **WHEN** 宿主用任意 UE 脚本语言实现 `ITcsTriggerPayloadReader` 并调用门面 `RegisterPayloadReader` 登记其载荷 struct
- **THEN** 求值器在收到该载荷类型时取到宿主实现给出的主体信息（内置/领域读取器的 C++ 快路径不受影响）

#### Scenario: 宿主脚本读取器不破坏每事件一次的读取语义

- **WHEN** 同一事件一次广播绑定多行触发行、且这些行的载荷类型由宿主脚本读取器提供
- **THEN** 宿主读取器在**该次事件**中只被调用一次（各行共用同一份读取结果），MUST NOT 退化为每行各读一次

#### Scenario: 宿主脚本读取器被 GC 强持有且受世界寿命约束

- **WHEN** 宿主登记脚本读取器后，在同一世界内经垃圾回收继续读取；随后世界结束、同类查询在世界 B 发生
- **THEN** 回收后读取仍分派到该宿主对象（门面 `UPROPERTY` 强持有）；世界 B 的读取视为未命中并移除该条目 + 留 Warning 日志，且 MUST NOT 与"未注册载荷类型"的 Verbose 路径混淆
