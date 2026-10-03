## MODIFIED Requirements

### Requirement: 流程模板与登记表

`TcsDamage` MUST 以**数据模板**承载瞬时流程（D7-5 流程管线宿主化：阶段构成本身是项目知识，插件不预设）：

- `FTcsFlowTemplate{ FGameplayTag TemplateId; TArray<FInstancedStruct> Steps; }`（USTRUCT；**2026-09-22 改造：`TemplateId` 类型 `FName` → `FGameplayTag`**）——有序步骤数组，元素形状同 `FEffectStep`（步骤无公共基类，类型合法性由执行器注册表在执行期判定）；
- `UTcsDamageSubsystem`（世界级子系统门面）MUST 提供登记表：`RegisterTemplate` / `UnregisterTemplate` / `FindTemplate`，**键 = `TemplateId`**；拒绝面（ensure + false）：`TemplateId` **无效**（`!TemplateId.IsValid()`）、同 id 重复登记（不得静默覆写）；`FindTemplate` 未登记返回 nullptr（**不 ensure**——正常查询路径）；
- `RunTemplate` 遇未登记模板 MUST 拒绝：Error 日志 + 不执行（执行期配置错误，不 ensure）；
- 登记表 MUST 以**地址稳定**方式持有模板（`TMap<FGameplayTag, TUniquePtr<…>>` 或同效手段；**2026-09-22 改造：键类型改 tag**）——解释器在一次执行期间持模板引用；
- 登记表 MUST 同时做到**对 GC 可见**（2026-09-23 补，修台账 WAIT-8）：模板步骤可携带 `UPROPERTY` 对象引用（如 `FTcsFlowDelegate::Delegate` 这类 `TScriptInterface<ITcsDamageFlowDelegate>`），而**裸 C++ 容器不经 GC 的 `RefLink`** —— 子系统 MUST 覆写 `AddReferencedObjects` 并逐模板调 `FReferenceCollector::AddPropertyReferencesWithStructARO`，否则那些引用会被静默回收（步骤取到空引用，表现为"公式不生效"而非崩溃）。

**地址稳定与 GC 可见是两件正交的事**：前者防 `TMap` 扩容搬移导致解释器持有的 C++ 引用悬空；后者防对象引用被 GC 回收。二者 MUST 同时满足。

**模板 id 的来源（2026-09-22；2026-10-01 换根）**：流程模板是**项目知识**（D7-5"阶段构成本身是项目知识"），故 `TemplateId` 属**宿主词汇**——由宿主项目 `Config/DefaultGameplayTags.ini` 声明（**根 = `DamageFlowTemplate`**，形态 `DamageFlowTemplate.<Id>`；根段注册表见 `gameplay-tag-governance` 能力）。**例外**：插件自登记的官方默认模板 `Default` 属**框架词汇**，由插件原生声明（`Tag_DamageFlowTemplate_Default` ↔ `DamageFlowTemplate.Default`）。宿主模板 id 与框架默认模板 id **同根不同词**（`DamageFlowTemplate` 是共享根，宿主可在其下自由加词）。

#### Scenario: 登记后可按 id 查到

- **WHEN** `RegisterTemplate` 一条 `TemplateId = DamageFlowTemplate.FlowTest` 的模板后 `FindTemplate`（同一 tag）
- **THEN** 返回该模板，步骤数组与登记内容一致

#### Scenario: 重复登记被拒

- **WHEN** 对同一 `TemplateId` 再次 `RegisterTemplate`
- **THEN** 返回 false、原模板不被覆写、留 ensure 提示与日志

#### Scenario: 未登记模板被拒绝执行

- **WHEN** `RunTemplate` 一个未登记的 `TemplateId`（有效 tag 但未登记）
- **THEN** 不执行任何步骤 + Error 日志（不崩溃、不 ensure）

#### Scenario: 模板步骤里的对象引用对 GC 可见

- **WHEN** 登记一条含对象引用的模板（步骤带 `TScriptInterface<ITcsDamageFlowDelegate>` 委托字段），且该对象**无其他强引用**，随后触发 GC
- **THEN** GC 后该对象仍存活（未被回收）；模板步骤取到的委托仍有效

### Requirement: 流程上下文（三层值空间）

`FTcsDamageFlowContext` MUST 承载三层值空间（09 §2.1）：账本（技能参数，**不住这里**——由链步骤解算后经请求字段传入）、**公式参数初值**、**流程属性黑板**。

- **公式参数初值**：`TMap<FGameplayTag, FTcsParamValue> FormulaParams`——**只读原料**（**键的根 = `TcsStateParam`**，见下；**MUST NOT 用下标**；宿主公式按名取用；**2026-09-22 改造：键类型 `FName` → `FGameplayTag`**）；
- **请求字段**（上下文请求，不随收集重置清除）：`BaseDamageInput: double`（链步骤解算结果）与 `TargetAttrKey: FGameplayTag`（扣血属性键；**2026-09-22 改造：原 `FTcsAttributeName`**）；
- `TMap<FGameplayTag, double> CapturedAttrs`——属性捕获快照（读默认 Live、命中读快照；**快照填充归标准步骤**，本层只落字段与语义；**2026-09-22 改造：键类型改 tag**）。

**`FormulaParams` 的键归属（2026-10-01 换根定案）**：键 MUST 落在 **`TcsStateParam`** 根下（与参数表键同根同词汇），**MUST NOT 另立根**。**依据 = 三层值空间的层次关系**（`TcsDamageFlowContext.h:24-27`）：第 1 层「**技能参数**（账本，Entry 级持久）」**不进流程**，而是**经解算后作为第 2 层传入**——第 2 层就是 `FormulaParams`，故它的键与 Def 参数行 / 参数表键**同词汇、同消费角色**（参数表读取 + 宿主公式按名取用），**不是第三个键空间**。为"公式参数"单开一根等于把"同一批键在不同传参层出现"误读成两个角色，违反一角色一根（根数量应约等于**消费角色**数量，不随传参层数增长）。根段注册表与归属规则见 `gameplay-tag-governance` 能力。

#### Scenario: 请求字段不随收集重置

- **WHEN** 流程跑过 `CollectStart`（重置黑板）后读取 `BaseDamageInput`
- **THEN** 值仍在（请求字段不受收集重置影响——这是"输入 vs 收集产物"分离的落点）

#### Scenario: 公式参数键与参数表键同根

- **WHEN** 检查 `FormulaParams` 的键与 Def 参数行 / `FTcsParamSource_ParamRef::Key` 的键
- **THEN** 三者同住 `TcsStateParam` 根（形态 `TcsStateParam.<键>`）；`FormulaParams` MUST NOT 使用另一个根或裸 `FName` 形态，插件 MUST NOT 声明任何具体键

### Requirement: 流程属性黑板

`FTcsFlowAttributes`（流程工作值的容器：键 + 每键修正链）MUST 提供：

- **键类型 = `FGameplayTag`**（**2026-09-22 改造：`FName` → `FGameplayTag`**）——契约键（`BaseDamage` / `Executed` / `Absorbed` / `Kill` / `Hit` / `Crit` / `ExecuteCandidates`）属**标准步骤库的契约**，由**插件原生声明**（**根 = `DamageFlowKey`**，形态 `DamageFlowKey.<键>`，常量名逐点换下划线）；宿主自定义键自由，由**宿主 ini** 声明——`DamageFlowKey` 是**共享根**，两侧同根不同词（根段注册表与归属规则见 `gameplay-tag-governance` 能力）；
- **提交**：`Submit(Key, Op, Operand, Consume)`——Operand 为 `FTcsParamValue`（PV 系列载体；黑板键引用保留为流程域自身 Operand 选项）；`SortKey` **只用于消耗裁决、MUST NOT 参与求值顺序**（`SortKey` 是消耗策略的字段，不是本方法的独立形参——本参数列表以源码为准校正）；
- **读取**：`Read(Key)` 按 **M2 同款带式语义**求值——**折叠 MUST 调用 TcsAttribute 的共享纯函数 `FoldTcsAttributeBands`**（D5-5 v3：M2 属性聚合 / M5 参数链 / 本容器**三处共用，MUST NOT 私建第二份**）；
- **收集重置**：`Reset()`——清空全部键的收集（标准步骤 `CollectStart` 的落点）；
- **消耗策略位**：`FTcsDamageModifierConsumePolicy`（**2026-09-30 改名**：原 `FTcsConsumePolicy`——"DamageModifier" 指名"伤害修改器"这一提交者身份，与走运算带聚合的**属性**修正器明确区分）——字段集合限为 `{ MaxUses; Cooldown; SortKey; }` 的**纯数据**，**MUST NOT 含 `TFunction` / 闭包成员**（`OnConsumed` 回调位**已移除**，2026-09-30：消费行为改由**事件语义**表达，见 `damage-primitive` 的「消耗策略为纯数据可反射结构」需求）；**本容器只存不裁**：裁决（SortKey 选一 → 成功才消费）归 `Execute` 步骤（D7-4"收集 ≠ 消费"）；
- **R3 不做值域收口**：黑板是流程工作值、**不是角色属性**——无边界/值域模式概念（`IValueDomainPolicy` 同款逃逸位随值域策略轮，见 09 §2.1 括注）。

#### Scenario: 同键多笔提交求和且与顺序无关

- **WHEN** 对同一键提交两笔 `Add`（或同一组含不同运算带的提交按两种排列）
- **THEN** `Read` 结果一致（带序由 `Op` 决定、顺序无关——共享折叠函数的既有保证）

#### Scenario: 收集重置

- **WHEN** 提交若干修正后调用 `Reset`
- **THEN** 该容器全部键回到空收集状态（`Read` 返回折叠初值 0）

#### Scenario: 消耗策略只存不裁

- **WHEN** 提交时携带 `FTcsDamageModifierConsumePolicy`（如 `MaxUses = 1`）
- **THEN** `Read` 行为不受策略影响，策略可被后续裁决步骤读回（消费语义不在本容器）

#### Scenario: 消耗策略不含闭包

- **WHEN** 检查 `FTcsDamageModifierConsumePolicy` 的成员
- **THEN** 全部为可序列化的纯数据字段（无 `TFunction`）——该结构因此可作 `UPROPERTY`（链步骤 / 数据步骤得以携带消耗策略），且不再阻断 `FTcsDamageFlowContext` 的逐字段反射化

#### Scenario: 宿主自定义键与契约键同根共存

- **WHEN** 宿主在 `DamageFlowKey` 下声明一个自定义键并向黑板提交，同时某标准步骤读写契约键 `DamageFlowKey.BaseDamage`
- **THEN** 两者互不干扰（同一根、不同词）；框架 MUST NOT 因宿主自定义键的存在而拒绝提交或改变契约键语义

### Requirement: 收集事件协议

`TcsDamage` MUST 以总线**立即通道**承载"步骤边界发事件、外部响应提交修正"的协议（09 §2.2/§3）：

- 事件 Tag MUST 按 **`TcsEvent.<域>.<事件名>` 命名公约**由本模块**原生声明**（域段 = `Damage`）——**设计文档旧写法 `Combat.Damage.Collect.<Step>` 不采用**（该写法早于 2026-09-18 的 Tag 公约与"原生声明、不进项目 Tag 表"口径；按 Step 名逐条声明）；**根 = `TcsEvent`**（共享根，见 `gameplay-tag-governance` 能力的根段注册表与归属规则）；
- 门面 MUST 提供广播入口 `PublishCollectEvent(步骤标识, FTcsDamageFlowContext&)`：经总线立即通道**同步**派发（响应方在调用返回前收到）；
- 载荷 MUST 为**上下文指针包装**（`FTcsDamageFlowCollectEvent{ FTcsDamageFlowContext* Context; }`）——进程内瞬态指针，MUST NOT 跨帧持有；响应方在回调内 `Submit` 即可（收集 ≠ 消费）。

#### Scenario: 立即通道同步到达

- **WHEN** 步骤执行器调用 `PublishCollectEvent`
- **THEN** 订阅方的响应在调用返回前已执行完毕（响应内提交的黑板修正对本步骤后续读取可见）

#### Scenario: 载荷指针仅在派发期有效

- **WHEN** 订阅方在回调外保存载荷中的上下文指针
- **THEN** 属误用（契约明文 MUST NOT 跨帧持有）——框架不为其保活

#### Scenario: 框架收集事件由插件原生声明

- **WHEN** 检查本模块收集事件（`TcsEvent.Damage.FlowStarted` / `…PreHit` / `…Hit` / `…Crit` / `…Element` / `…AfterDamage` / `…PreExecute` / `…Completed` / `…Recorded` / `…ModifierConsumed`）的声明处
- **THEN** 全部是插件原生常量；宿主即使完全不配置 tag 表，宿主在 TCS 总线上订阅这些事件依然成立（宿主**自发布**的事件不在此列，它们由宿主 ini 在 `TcsEvent` 根下声明）
