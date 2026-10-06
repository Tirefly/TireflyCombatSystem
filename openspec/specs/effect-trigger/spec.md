# effect-trigger Specification

## Purpose
定义触发行：数据形状、条件最小集、实例与来源级联退订、登记表与订阅生命周期、四道门的求值器、触发载荷读取器，以及触发行句柄的反射性。

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

### Requirement: 触发行实例与级联退订

`TcsEffect` MUST 以**运行期形态**承载已登记的触发行——定义与运行期簿记**分层**（2026-09-23 用户拍板）：

- `FTcsEffectTriggerInstance`（USTRUCT）字段：`Def: FTcsEffectTriggerDef`（定义）+ `Source: FTcsSourceHandle`（级联退订锚点）+ `Self: FTcsEffectTriggerHandle`（自身句柄）+ `Subject: FInstancedStruct`（运行期实例主体绑定，`UPROPERTY`，默认空）；`Subject` MUST NOT 加进定义侧配置（它与 `Source` 一样是运行期身份，不进内容资产）；
- `FTcsEffectTriggerHandle`（USTRUCT）以 `TTcsInstanceHandle<FTcsEffectTriggerTag>` 作下标句柄（类型区分标签防与链运行态句柄互换）；
- **`Source` 的语义取决于"行怎么被登记"**：定义加载期登记（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄；**施加状态时登记（Buff 行为）→ `Source` = 该状态实例的级联锚点 `FTcsStateInstance::CascadeAnchor`**（每实例恒发新号、刷新不换）。MUST NOT 用施加方来源 `FTcsStateInstance::Source` 作该行的撤销锚点——同一施加方可施加多个不同定义，按它撤销会互相误摘。该语义是 `09 §2.3`"订阅随 Source 生命周期自动退订——'状态在，修改器就在'天然成立"的机制落点；
- **`Subject` 与 `Source` 分工不同**：`Source` 负责级联退订，`Subject` 只负责实例局部事件的路由。状态内联行 MUST 在登记时以 `FInstancedStruct::Make<FTcsStateHandle>(Instance.Handle)` 绑定主体；状态句柄的 `Index + Generation` 是完整实例身份（代际发号全局唯一，跨单位同槽位不撞身份），MUST NOT 另加单位字段构造第二套匹配键；
- **主体匹配的适用边界**：仅当行与载荷读取结果都提供有效 `Subject` 时筛选。主体 MUST 有完全相同的 `UScriptStruct` 类型，且 `UScriptStruct::CompareScriptStruct(A, B, 0)` 比较值相等，才允许投递；类型不同或值不同 MUST 静默跳过。未绑定全局行（空 `Subject`）仍接收该 Tag 下全部事件；载荷未提供 `Subject` 的非实例外部事件（Damage、没有读取器的自定义事件等）仍按原 Tag 语义投递给绑定行。此匹配 MUST NOT 被解读为 `EventPayloadFilter` 的实现——该内容预筛字段仍留位；
- 级联退订：按 `Source` 一次性摘除该来源登记的全部行（与 M2 `RemoveBySource` 同款语义）；主体绑定 MUST NOT 改变该 API 的配对语义；
- **持有形态**：登记表以 `TArray<FTcsEffectTriggerInstance>`（值语义）持有——触发行无高频增删、无挂起语义，**MUST NOT** 引入实例池类型（`TTcsInstancePool` 的挂起锚/占用统计在此是零收益的复杂度）。**代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有实例指针**——求值器每次从登记表按下标重解析（与链解释器"每步入器前重解析"同款纪律）。
- **槽位复用 MUST 配代际校验（2026-09-23 收紧）**：摘除后槽位可被后续登记复用，故登记表 MUST 自持**空闲槽位表 + 每槽代际计数**，并恒把代际写进 `Self.Generation`。**MUST NOT** 让代际段恒为 0——`TTcsInstanceHandle` 携带 `Generation` 段而登记表恒填 0，等于让类型对自身语义说谎，且**陈旧句柄会静默改指另一行**（`UnregisterTriggerRow(旧句柄)` 摘掉无辜的行）。校验由登记表自身完成（代际失配即拒），**不**复用 `TTcsInstancePool` 类型。
- **GC 引用收集（MUST 覆写）**：登记表是门面的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它**；行的定义侧含 `FInstancedStruct`（`Conditions` / `EventPayloadFilter`），运行侧的 `Subject` 也可能承载宿主反射结构体的对象引用。故门面 `AddReferencedObjects` MUST 逐条已分配行调 `FReferenceCollector::AddPropertyReferencesWithStructARO(FTcsEffectTriggerInstance::StaticStruct(), &Row, Owner)`，遍历**整条反射实例**，同时覆盖 `Def` 与 `Subject`。**判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——plan3 注记曾以"值语义 → 无需 ARO"为由判断不需要，该判据是错的（实施时纠正）。

#### Scenario: 同一来源的行可被一次性摘除

- **WHEN** 同一来源登记多行触发行
- **THEN** 各实例的 `Source` 相等，可按该句柄一次性全量摘除

#### Scenario: 状态施加期登记的行走状态生命周期

- **WHEN** 以某状态实例的 `CascadeAnchor` 为 `Source` 登记触发行（Buff 行为形态），随后该状态被移除
- **THEN** 按该锚点级联退订后，该来源登记的行不再触发（"状态在，行为就在"）；不误摘同一施加方的其它状态实例

#### Scenario: 实例携带自身句柄

- **WHEN** 检查一个已登记实例
- **THEN** `Self` 为该实例在登记表内的有效下标句柄（供退订/点灯/求值器回填上下文使用）

#### Scenario: 陈旧句柄被拒且不误伤复用槽位的新行

- **WHEN** 登记一行取得句柄 H、摘除 H、再登记一行复用同一槽位，然后以陈旧句柄 H 调 `UnregisterTriggerRow`
- **THEN** 该调用被拒（代际失配），**新行不受影响**

#### Scenario: 行内条件携带的对象引用被 GC 可见

- **WHEN** 一行条件里内联了一个带 `UPROPERTY` 对象引用的宿主自定义 struct，且该对象在别处无强引用
- **THEN** 该对象不被 GC 回收（门面 `AddReferencedObjects` 已逐行补引用）

#### Scenario: 运行期主体携带的对象引用被 GC 可见

- **WHEN** 一行的 `Subject` 内层含宿主结构体的 `UPROPERTY` 对象引用，且对象在别处无强引用
- **THEN** 该对象被整条实例的 ARO 遍历覆盖，不被 GC 回收

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

### Requirement: 触发求值器与四道门

`TcsEffect` MUST 以**共享 Handler**（裁决 2a）承载触发求值，门序固定：

- `UTcsTriggerEvaluator : UTcsEventHandler`（`UCLASS`）——事件类型 → Handler 对象（事件 struct 上不自绑 delegate）；由门面创建、`UPROPERTY` 持有、以 `UTcsEventHandler*` 身份订阅总线；
- **四道门（MUST 按此序，顺序有意义）**：`事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`。**实例主体匹配属于第一道事件路由**：Tag 命中后、`ExecutionGate` 之前，按「触发行实例与级联退订」的精确类型和值比较规则筛选；MUST NOT 另立第五道门，MUST NOT 执行留位 `EventPayloadFilter`。`ExecutionGate`（网络闸）最廉价先判；`Conditions`（可含宿主自定义求值）最贵最后判。任一道不过即**不触发本行**（不短路其它行——同行之间彼此独立）；
- **行遍历顺序**：按 `Def.Priority` **降序**（大者先，与覆盖带 `OverridePriority` 同向）；**同 `Priority` 按登记序**（`TArray` 稳定序遍历 + 稳定排序——遍历顺序 MUST NOT 依赖容器哈希序）；
- **`ExecutionGate` 的 R4 语义**：`TEG_Always` 恒通过；`TEG_AuthorityOnly` 在**单机**形态下同样通过（本地即权威）——判别逻辑随网络姿态轮，枚举值本身不得改变含义；
- **起链装配**：`FTcsEffectContext{Caster = 载荷读取结果, EventPayload = 原事件载荷, Targets = 空, CausedBy = 本行 Source}` → `ExecuteChain(Def.EffectChainId, Context)`。`RunSource` 由 `ExecuteChain` 发新号（本行 `Source` 不作链运行身份）。`Targets` 本轮留空（完整"事件载荷 → 目标"通路需真实带目标的载荷类型）；
- **条件未过的日志级别**：`Def.bConditionMissIsSilent == true` → 静默跳过（默认）；`false` → 记一条 **`Verbose`**（M8 Explain 面板的数据源）。**MUST NOT** 用 Warning/Error——条件未过是**正常业务路径**，不是故障；主体不匹配同样是正常路由跳过；
- **每行的求值 MUST NOT 跨帧持有实例指针**：求值期从登记表按句柄重解析（`TArray` 扩容会搬移元素地址）。**宿主条件回调的重入同样受约束**：回调前 MUST 复制本行配置、主体及来源到值语义快照，MUST NOT 跨回调保留或使用登记表中的实例指针；条件通过后 MUST 再校验原行句柄的活性与代际，回调中已摘除或已被复用的行 MUST NOT 起链。回调造成登记表扩容但本行仍有效时，MUST 继续按该快照处理。

#### Scenario: 四道门按序拦截

- **WHEN** 一行的 `ExecutionGate` 不过（未来网络姿态下）且其 `Conditions` 本会通过
- **THEN** 该行不触发，且其条件求值器**未被调用**（门序在前者先拦截）

#### Scenario: Priority 大者先、同级按登记序

- **WHEN** 同一 Tag 下登记三行，`Priority` 分别为 10 / 0 / 10（两个 10 的登记序为 A 后 B）
- **THEN** 起链顺序为 A(10) → B(10) → C(0)

#### Scenario: 点灯关闭整行

- **WHEN** 一行的 `GateTags = [G]` 而 G 未点亮
- **THEN** 该行不触发；`SetTriggerGateTag(G, true)` 后同一事件可使其触发

#### Scenario: 起链上下文装填原载荷

- **WHEN** 事件 E 带载荷 P 命中一行
- **THEN** 起链的 `FTcsEffectContext.EventPayload` 即 P（按值拷贝），且 `Caster` 来自载荷读取器结果，`CausedBy` 等于该行快照的 `Source`

#### Scenario: 条件未过不产生红字

- **WHEN** 一行条件未过且 `bConditionMissIsSilent == false`
- **THEN** 只留一条 `Verbose` 日志，**MUST NOT** 出现 Warning/Error（常规验收命令零红字）

#### Scenario: 状态实例局部事件不会引爆其它实例绑定的行

- **WHEN** 两个不同状态实例各自登记同 Tag 内联行，发布其中一个实例的生命周期事件
- **THEN** 只命中 `Subject` 为该 `FTcsStateHandle` 的绑定行，其它实例的行在路由阶段静默跳过；同单位与跨单位均按完整句柄身份区分

#### Scenario: 主体比较必须同时满足精确类型和值相同

- **WHEN** 一行与载荷读取结果都带 `Subject`，但结构类型不同，或类型相同而句柄的 Index / Generation 不同
- **THEN** 本行被路由跳过，条件不求值；主体类型完全相同且反射值相同时才进入 `ExecutionGate`

#### Scenario: 未绑定全局行继续接收所有同 Tag 事件

- **WHEN** 一条全局行的 `Subject` 为空，发布不同实例的同 Tag 生命周期事件
- **THEN** 该行对这些事件均继续走既有四道门，不因载荷带主体而被屏蔽

#### Scenario: 无实例主体的外部事件保留绑定行的既有语义

- **WHEN** 已绑定实例主体的行收到同 Tag 的 Damage 载荷，或未注册读取器的自定义载荷，读取结果 `Subject` 为空
- **THEN** 该行继续走既有四道门，MUST NOT 仅因事件没有实例主体而跳过

#### Scenario: 条件回调移除或复用本行后不再起链

- **WHEN** 宿主条件回调摘除当前行并返回通过，且可能登记新行复用当前槽位
- **THEN** 旧句柄重新校验失败，该行不再起链；不得读取悬空指针或把新行当成旧行

#### Scenario: 条件回调扩容登记表时本行仍可安全求值

- **WHEN** 宿主条件回调登记其它行导致登记表扩容，当前行仍然在册并返回通过
- **THEN** 起链使用本行的值语义快照，链身份与来源完整，不依赖扩容前的元素地址

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

### Requirement: 触发行句柄的反射性

`FTcsEffectTriggerHandle` MUST 是**反射可见类型**（`USTRUCT()`）且**值可跨语言往返**——脚本层调 `RegisterTriggerRow` 接住行句柄、再把它传回 `UnregisterTriggerRow` 摘除该行（2026-09-24 随门面反射面同批落地）。

- **字段 MUST 展平**（2026-09-24 实测修正，与 `FTcsChainRunHandle` 同根因同修法）：句柄 MUST 直接持有 `Index`/`Generation` 两个 `UPROPERTY int32` 字段，**MUST NOT** 内嵌 `TTcsInstanceHandle<T>`——后者是模板类型、无法作 `UPROPERTY`，会让绑定产物生成**空壳**（`ToNative`/`FromNative` 函数体为空）⇒ 脚本层接住句柄时读不到值、传回时写全零 ⇒ **代际失配、往返失效**（该缺陷在门面反射面打通前不可见——C# 此前根本调不到这些方法）；
- `Index` MUST 为 `int32`（UHT 不支持 `uint32` 作属性类型）；**无效值 `-1` 与 `TTcsInstanceHandle::InvalidIndex(0xFFFFFFFF)` 位模式相同**——MUST 经唯一转换点（`GetInner`/`SetInner`）与登记表内句柄互转，保证往返无损；
- **MUST NOT** 加 `BlueprintType`（口径同 `FTcsChainRunHandle`：运行期身份词、非配置数据；R0 §9 蓝图不承诺）。

#### Scenario: 行句柄值可跨语言往返

- **WHEN** 脚本层调 `RegisterTriggerRow` 接住返回的行句柄，随后原样传回 `UnregisterTriggerRow`
- **THEN** 摘除成功（句柄值完整往返：`Index`/`Generation` 均保持，代际校验通过）

#### Scenario: 行句柄字段可被脚本层读出

- **WHEN** 脚本层读取接住的行句柄的 `Index` / `Generation`
- **THEN** 读到的是登记表的真实值——绑定产物 MUST 为这两个字段生成真实的读写代码（非空壳）

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
