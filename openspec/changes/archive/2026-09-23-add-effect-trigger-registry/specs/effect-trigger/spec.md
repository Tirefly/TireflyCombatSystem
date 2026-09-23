## MODIFIED Requirements

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

## ADDED Requirements

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
- `Deinitialize` MUST 清空登记表 + **全量退订**（不留跨世界残留订阅）。

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
