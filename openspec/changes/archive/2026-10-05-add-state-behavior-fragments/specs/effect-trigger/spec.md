## MODIFIED Requirements

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

- `FTcsTriggerPayloadInfo`（USTRUCT，反射）：`Caster: FTcsCombatEntityHandle` / `ClassificationTags: TArray<FGameplayTag>` / `Subject: FInstancedStruct`（`UPROPERTY`，默认空）——单位身份供条件求值，分类集供标签匹配，实例主体仅供事件路由；MUST NOT 在 TcsEffect 中 include 状态句柄类型或硬编码领域载荷类型；
- 读取器签名：`TFunction<FTcsTriggerPayloadInfo(const FInstancedStruct& Payload)>`；
- `FTcsTriggerPayloadReaderRegistry`：`AddPending`（静态自注册）+ `Register(const UScriptStruct*, Reader)`（动态入口，同类型重复登记 MUST 拒绝）+ `Find`（未命中返回 nullptr，**不 ensure**）；
- **寿命语义（2026-09-29 新增，`DEC-04` 裁定 ⑤）**：本注册表同为**进程级单例**且 MUST 保持如此；其**动态**登记项 MUST 受与步骤执行器注册表**同款**的寿命约束——**MUST 记录宿主对象弱引用 + 登记世界弱引用**；静态自注册的纯函数项（含由属主模块登记的领域读取器）MUST NOT 受寿命约束；
  - **失效判据**（任一即失效）：宿主弱引用为空 / 世界弱引用为空 / 条目世界 ≠ 查询方世界；
  - **查询侧校验**：`Find` MUST 接受可选的世界校验入参（调用方持有世界时 MUST 显式传入）；跨世界失效 MUST 视为未命中（返回 nullptr）并移除该条目 + 留含类型名的 **Warning** 日志。**注意与"未注册载荷类型"的处置区分**：后者按既有口径走"默认构造 + **Verbose**"（不是契约违规），而前者是**世界已更换**——两者 MUST NOT 混为一谈；
  - **拒绝门收窄（"同世界活对象重复"）**：既有条目已失效时新登记 MUST **替换**而 MUST NOT 拒绝；仅当既有条目有效且属同世界（或为静态自注册项）时才 MUST 拒绝；
  - **显式移除入口**：MUST 提供按键移除动态条目的入口，MUST NOT 能移除静态项；缺省不调用时正确性 MUST NOT 受影响；
  - **本条特此约束未来新增的动态入口**：当 `LEDGER-reflection` R-2 为载荷读取器新增**宿主脚本插槽**（UObject 基类形态）时，其登记入口 MUST 直接满足上述寿命语义——MUST NOT 先按旧口径落地再返工。
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
