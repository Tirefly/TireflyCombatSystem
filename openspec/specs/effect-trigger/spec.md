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
- **持有形态**：登记表以 `TArray<FTcsEffectTriggerInstance>`（值语义）持有——触发行无高频增删、无挂起语义，**MUST NOT** 引入实例池类型（`TTcsInstancePool` 的挂起锚/占用统计在此是零收益的复杂度）。**代价**：`TArray` 扩容会搬移元素地址，故**MUST NOT 跨帧持有实例指针**——求值器每次从登记表按下标重解析（与链解释器"每步入器前重解析"同款纪律）。
- **槽位复用 MUST 配代际校验（2026-09-23 收紧）**：摘除后槽位可被后续登记复用，故登记表 MUST 自持**空闲槽位表 + 每槽代际计数**，并恒把代际写进 `Self.Generation`。**MUST NOT** 让代际段恒为 0——`TTcsInstanceHandle` 携带 `Generation` 段而登记表恒填 0，等于让类型对自身语义说谎，且**陈旧句柄会静默改指另一行**（`UnregisterTriggerRow(旧句柄)` 摘掉无辜的行）。校验由登记表自身完成（代际失配即拒），**不**复用 `TTcsInstancePool` 类型。
- **GC 引用收集（MUST 覆写）**：登记表是门面的**非 `UPROPERTY` 成员**，GC 的 `RefLink` 遍历**走不到它**；而行的定义侧含 `FInstancedStruct`（`Conditions` / `EventPayloadFilter`），其**内层内存可放宿主自定义 struct 的 `UPROPERTY` 对象引用**（D4-16 类型不设限）→ 不补引用即**静默回收**。故门面 `AddReferencedObjects` MUST 逐行调 `FReferenceCollector::AddPropertyReferencesWithStructARO`（与 `ChainDefs` 同款手法）。**判据是"容器是否 GC 可见"，与"值语义还是指针语义"无关**——plan3 注记曾以"值语义 → 无需 ARO"为由判断不需要，该判据是错的（实施时纠正）。

#### Scenario: 同一来源的行可被一次性摘除

- **WHEN** 同一来源登记多行触发行
- **THEN** 各实例的 `Source` 相等，可按该句柄一次性全量摘除

#### Scenario: 状态施加期登记的行走状态生命周期

- **WHEN** 以某状态实例句柄为 `Source` 登记触发行（Buff 行为形态），随后该状态被移除
- **THEN** 按该句柄级联退订后，该来源登记的行不再触发（"状态在，行为就在"）

#### Scenario: 实例携带自身句柄

- **WHEN** 检查一个已登记实例
- **THEN** `Self` 为该实例在登记表内的有效下标句柄（供退订/点灯/求值器回填上下文使用）

#### Scenario: 陈旧句柄被拒且不误伤复用槽位的新行

- **WHEN** 登记一行取得句柄 H、摘除 H、再登记一行复用同一槽位，然后以陈旧句柄 H 调 `UnregisterTriggerRow`
- **THEN** 该调用被拒（代际失配），**新行不受影响**

#### Scenario: 行内条件携带的对象引用被 GC 可见

- **WHEN** 一行条件里内联了一个带 `UPROPERTY` 对象引用的宿主自定义 struct，且该对象在别处无强引用
- **THEN** 该对象不被 GC 回收（门面 `AddReferencedObjects` 已逐行补引用）

### Requirement: 触发行登记表与订阅生命周期

`TcsEffect` MUST 以门面 API 承载触发行的登记与摘除，并按 `EventTag` 装配总线订阅：

- `RegisterTriggerRow(const FTcsEffectTriggerInstance&) -> FTcsEffectTriggerHandle`：登记表持有（值语义）+ **按 `Def.EventTag` 装配总线订阅**。`EventTag` 或 `EffectChainId` 无效时 ensure + 返回无效句柄（配置错误，不静默接受）；
- `UnregisterTriggerRow(FTcsEffectTriggerHandle) -> bool`：摘除单行（先校验代际——失配即拒 + 返回 false，不 ensure）；
- `UnregisterTriggerRowsBySource(const FTcsSourceHandle&) -> int32`：按来源全量摘除（级联退订锚点，与 M2 `RemoveBySource` 同款语义）；
- **订阅计数配对（核心纪律）**：同一 `EventTag` 的多行**共用一条订阅**——首次出现该 Tag 时订阅一次，该 Tag 的行数归零时才退订。**MUST NOT** 每行各订一次（那会让总线订阅表随行数膨胀且退订易漏）；
- **订阅通道 = 立即**（`ETcsEventDispatch::EED_Immediate`）：收集协议要求"修正提交落在事件发布返回之前"（`TcsDamage` 的收集事件走立即通道），订帧末会让修正晚一拍；
- **点灯 API**：`SetTriggerGateTag(FGameplayTag, bool)` / `IsTriggerGateTagLit(FGameplayTag) const`——行级开关（`GateTags` 全部点亮才通过该道门）；
- **观测 API**：`GetTriggerRowCount() const`（装置断言用）；
- **求值器生命周期**：由门面在首次登记时创建并 **`UPROPERTY` 持有**——总线订阅表持**弱引用**（`FTcsEventSubscription::Handler` 是 `TWeakObjectPtr`），不 root 会被 GC 掉、订阅静默失效；
- `Deinitialize` MUST 清空登记表 + **全量退订**（不留跨世界残留订阅）；
- **脚本层可达（2026-09-24 补）**：上列方法中形参全为反射类型的部分（`RegisterTriggerRow` / `UnregisterTriggerRow` / `SetTriggerGateTag` / `IsTriggerGateTagLit` / `GetTriggerRowCount` / `SetTriggerRandomSeed`）MUST 标记 `UFUNCTION()`（无 specifier，口径见 `effect-interpreter` 的「门面反射面」需求）——这是"宿主用 C# 写技能/Buff 逻辑"的登记入口。**形参含非反射裸 struct 的 `UnregisterTriggerRowsBySource(const FTcsSourceHandle&)` 不在其列**（需先反射化 `FTcsSourceHandle`）。

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

### Requirement: 触发求值器与四道门

`TcsEffect` MUST 以**共享 Handler**（裁决 2a）承载触发求值，门序固定：

- `UTcsTriggerEvaluator : UTcsEventHandler`（`UCLASS`）——事件类型 → Handler 对象（事件 struct 上不自绑 delegate）；由门面创建、`UPROPERTY` 持有、以 `UTcsEventHandler*` 身份订阅总线；
- **四道门（MUST 按此序，顺序有意义）**：`事件 Tag 路由 → ExecutionGate → GateTags → Conditions → 起链`。`ExecutionGate`（网络闸）最廉价先判；`Conditions`（可含宿主自定义求值）最贵最后判。任一道不过即**不触发本行**（不短路其它行——同行之间彼此独立）；
- **行遍历顺序**：按 `Def.Priority` **降序**（大者先，与覆盖带 `OverridePriority` 同向）；**同 `Priority` 按登记序**（`TArray` 稳定序遍历 + 稳定排序——遍历顺序 MUST NOT 依赖容器哈希序）；
- **`ExecutionGate` 的 R4 语义**：`TEG_Always` 恒通过；`TEG_AuthorityOnly` 在**单机**形态下同样通过（本地即权威）——判别逻辑随网络姿态轮，枚举值本身不得改变含义；
- **起链装配**：`FTcsEffectContext{Caster = 载荷读取结果, EventPayload = 原事件载荷, Targets = 空}` → `ExecuteChain(Def.EffectChainId, Context)`。`Targets` 本轮留空（完整"事件载荷 → 目标"通路需真实带目标的载荷类型）；
- **条件未过的日志级别**：`Def.bConditionMissIsSilent == true` → 静默跳过（默认）；`false` → 记一条 **`Verbose`**（M8 Explain 面板的数据源）。**MUST NOT** 用 Warning/Error——条件未过是**正常业务路径**，不是故障；
- **每行的求值 MUST NOT 跨帧持有实例指针**：求值期从登记表按句柄重解析（`TArray` 扩容会搬移元素地址）。

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
- **THEN** 起链的 `FTcsEffectContext.EventPayload` 即 P（按值拷贝），且 `Caster` 来自载荷读取器结果

#### Scenario: 条件未过不产生红字

- **WHEN** 一行条件未过且 `bConditionMissIsSilent == false`
- **THEN** 只留一条 `Verbose` 日志，**MUST NOT** 出现 Warning/Error（常规验收命令零红字）

### Requirement: 触发载荷读取器

`TcsEffect` MUST 以**载荷读取器注册表**承载"从事件载荷里读出主体信息"的能力——`TcsEffect` 全程**不认识任何领域载荷类型**（依赖铁律 `Core←Attribute←Effect←{Damage,…}`，04 §1）：

- `FTcsTriggerPayloadInfo`（USTRUCT，反射）：`Caster: FTcsCombatEntityHandle` / `ClassificationTags: TArray<FGameplayTag>`——**载荷能提供的全部主体信息**；
- 读取器签名：`TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct& Payload)>`；
- `FTcsTriggerPayloadReaderRegistry`：`AddPending`（静态自注册）+ `Register(const UScriptStruct*, Reader)`（动态入口，同类型重复登记 MUST 拒绝）+ `Find`（未命中返回 nullptr，**不 ensure**）；
- 自注册宏对 `UE_DECLARE/DEFINE_TRIGGER_PAYLOAD_READER`——**由载荷类型的属主模块登记**（如 TcsDamage 的收集事件读取器住 TcsDamage）；
- **未注册载荷类型**：默认构造 `FTcsTriggerPayloadInfo`（空 Caster / 空标签集）+ **Verbose** 日志——**MUST NOT ensure**（"载荷类型未知"不是契约违规：手动发布一条自定义事件是合法用法）；
- **`ClassificationTags` 无来源时为空集**：这是**显式交付的语义**，不是遗漏——空集上匹配非空 Tag 数组的 `HasAllTags` 恒不过（条件的既有语义），故内容侧若要用该条件，MUST 有读取器提供标签集。

#### Scenario: 属主模块自登记读取器

- **WHEN** 某领域模块为自己定义的事件载荷 struct 用自注册宏登记读取器
- **THEN** 求值器在收到该载荷时能取到其主体信息（无需修改 TcsEffect 源码）

#### Scenario: 未注册载荷类型不 ensure

- **WHEN** 求值器收到一个没有读取器的载荷类型
- **THEN** `Caster` 为空、标签集为空，留一条 `Verbose` 日志，**MUST NOT** 出现 ensure / Warning / Error

#### Scenario: 重复登记被拒

- **WHEN** 对同一载荷类型再次 `Register`
- **THEN** 拒绝并保留首个登记，留 ensure 提示

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

