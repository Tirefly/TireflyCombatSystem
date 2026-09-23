# attribute-types Specification

## Purpose
TBD - created by archiving change add-tcsattribute-types-and-store. Update Purpose after archive.
## Requirements
### Requirement: 运算带封闭枚举与带权

`TcsAttribute` MUST 提供封闭五带运算枚举 `ETcsAttributeOp`：`TAO_Add = 0`（默认）、`TAO_Override = 1`、`TAO_PercentAdd = 2`、`TAO_Mul = 3`、`TAO_FlatAdd = 4`；**无 Custom 逃逸位**（D2-7：计算一律在上游求值传入终值，聚合无代码插点）；新运算 = 末尾追加枚举值（D2-10 首例；追加需论证与既有带的交换性）。MUST 提供带权助手 `GetTcsAttributeBandWeight(ETcsAttributeOp)`（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30，header-only）——**带序唯一真相在 Op**；账本修正器的 `SortKey` 仅为展示/审计位，折叠 MUST NOT 依赖它。

**覆盖带内的强弱排座次同样不由本枚举承担**：`OverridePriority`（修正器侧）是第一裁决键、属性定义侧的 `OverrideTieBreak`（同优先级策略）是第二级——两者都 MUST NOT 被 `SortKey` 替代（`SortKey` 在折叠里始终是零语义字段），详见"覆盖带的优先级与同优先级策略"需求。

#### Scenario: 默认值与五带齐备

- **WHEN** 默认构造一个运算字段并读取枚举值
- **THEN** 取值为 `TAO_Add`（值 0）；五带对应带权分别返回 0 / 10 / 15 / 20 / 30

#### Scenario: 带权助手与 SortKey 不构成双真相

- **WHEN** 一条修正器被手工置 `SortKey = 0` 而 `Op = TAO_FlatAdd`
- **THEN** 折叠仍按 Op 落入 FlatAdd 带（带权按其 Op 判定，不由 SortKey 决定）

#### Scenario: 覆盖强弱与带序是两个不同的问题

- **WHEN** 两条 `TAO_Override` 修正器带不同 `OverridePriority`，同时另一条 `TAO_Add` 修正器带任意 `SortKey`
- **THEN** 前者按优先级选出赢家，后者的 `SortKey` 不影响任何结果（带序仍只由 `Op` 决定）

### Requirement: 操作数双形状

`TcsAttribute` MUST 提供操作数的定义侧与运行侧**两个形状**（D2-13，载体已由 PV 系列取代 D2-12）：

- 定义侧 `FTcsAttrModOperandDef`（USTRUCT）：`Kind: ETcsOperandKind`（`OPK_Literal = 0` 默认 / `OPK_AttributeScaled = 1`，**仅此两种**——D2-11 拒绝清单不破）、`Literal: FTcsParamValue`（可配等级表等源，模板默认值可等级化）、`Attribute: FGameplayTag`、`Coefficient: double = 1.0`；
- 运行侧 `FTcsAttrModOperand`（纯 C++ struct，反射面外）：`Kind` / `Literal: double`（**恒为已解析规范值**——账本零膨胀，物化器单点转换）、`Attribute: FGameplayTag` / `Coefficient`。

#### Scenario: 定义侧承载参数源

- **WHEN** 模板把 `Literal` 配为等级表源（`FTcsParamValue`）
- **THEN** 定义侧可容纳该源并参与求值；运行侧 `Literal` 始终是单个已解析 double

#### Scenario: 运算种类封闭

- **WHEN** 试图以第三种 Kind 表达"按时间衰减/随机/跨域引用"的取值
- **THEN** 无此枚举值可用（D2-11 拒绝清单的编译期体现）

### Requirement: 边界三态与值域模式

`TcsAttribute` MUST 提供：

- `ETcsAttributeBoundMode{ ABM_None = 0, ABM_Static = 1, ABM_Dynamic = 2 }` 与 `FTcsAttributeBound`（USTRUCT：`Mode` / `StaticValue: double` / `DynamicAttribute: FGameplayTag`）——Min 与 Max **各自独立三态**（D2-4）；`ABM_Dynamic` 的边界属性按管线求值（"HP ≤ MaxHP"形态），**自引用禁止**；
- `FTcsAttributeBounds`（USTRUCT：`Min` / `Max`，反射可见——属性定义数据行需要承载它）；
- `ETcsAttributeValueDomain{ AVD_Clamp = 0, AVD_Custom = 1, AVD_Wrap = 2 }`（D2-6）：`AVD_Custom` 是**逃逸位且固定值 1**（全插件 Custom=1 约定），语义为"IValueDomainPolicy 只接管值域函数，时序/级联/事务仍由引擎守护"。**值域策略接口不在本任务**（R3 未建，收口行为归 Task 5；使用 `AVD_Custom` 而策略接口缺席时的行为 MUST 在 Task 5 的收口点显式可见）。

#### Scenario: 边界两侧独立

- **WHEN** 属性定义 `Min = ABM_Static(0)`、`Max = ABM_Dynamic(Tcs.Attr.MaxHealth)`
- **THEN** 两个边界各自按自己的模式处理（静态值 / 动态属性求值），互不牵连

#### Scenario: 值域逃逸位取值固定

- **WHEN** 读取 `AVD_Custom` 的枚举值
- **THEN** 值为 1（全插件 Custom=1 约定，与 Algo 无关）

### Requirement: 账本修正器与属性实例

`TcsAttribute` MUST 提供账本侧两个**纯 C++ struct**（不进反射面——D2-13 账本形状只为聚合热路径服务）：

- `FTcsAttrModInstance{ Target: FGameplayTag, Op: ETcsAttributeOp, Operand: FTcsAttrModOperand, Source: FTcsSourceHandle, OverridePriority: int32, SortKey: int32 }`——`Source` 是级联撤销锚点（D2-2：来源注销 → 按 Source 全量移除）；`OverridePriority` **仅 `TAO_Override` 读**（其余带忽略），`SortKey` 始终不参与折叠（**2026-09-23 删除：原 `Tag: FName` 字段**——零消费者，见「修正器模板资产与约定列白名单」的同款说明）；
- `FTcsAttributeInstance{ Attr: FGameplayTag, BaseValue: double, CachedCurrent: double, bDirty: bool, Bounds: FTcsAttributeBounds, ValueDomain: ETcsAttributeValueDomain, OverrideTieBreak: ETcsAttrOverrideTieBreak, ModifierSlots: TArray<FTcsAttrModInstance> }`——`CachedCurrent` 是**派生缓存非权威**（聚合管线唯一生产者、惰性重算，D2-8）；`OverrideTieBreak` 由定义侧展开（热路径不回查定义）。

**无分组标签字段（2026-09-23 用户拍板）**：账本修正器**不携带**任何"同来源内分组标签"字段——原 `Tag: FName` 从旧 TCS 搬来且**全库零消费者**（折叠/物化/级联撤销均不读），已删除。若将来出现"同来源内分组"的真实需求，该能力 MUST 以 `FGameplayTag` 形态重新引入（与全系统标识体系一致），**MUST NOT** 复活 `FName` 版本。

#### Scenario: 修正器携带级联锚点

- **WHEN** 同一来源挂两条修正器到不同属性
- **THEN** 两条修正器的 `Source` 相等，可按该句柄一次性全量摘除（D2-2）

#### Scenario: 覆盖优先级随实例进账本

- **WHEN** 一条 `TAO_Override` 修正器以 `OverridePriority = 9` 挂到某属性
- **THEN** 账本条目保留该值，折叠按它参与强弱裁决（非覆盖带上该字段不参与任何计算）

#### Scenario: 账本修正器无分组标签字段

- **WHEN** 检查 `FTcsAttrModInstance` 的字段集
- **THEN** 不含任何分组标签字段（`Target` / `Op` / `Operand` / `Source` / `OverridePriority` / `SortKey` 六项之外无字段）

#### Scenario: 实例具备脏标记位

- **WHEN** 属性实例被新建
- **THEN** 实例带 `bDirty` 标记位且**新建即脏**——初值由聚合管线在"批外读取或提交"时结算（值域收口与既有修正器在该次结算生效）并清零脏标记（见 `attribute-store` 与 `attribute-pipeline` 能力）

### Requirement: 修正器模板资产与约定列白名单

`TcsAttribute` MUST 以**同款双轨组织**提供修正器模板（D3-19 纯模板=默认值，引用处零字段覆写）：两个类型同住 `Attribute/TcsAttrModDef.h`——

- **`FTcsAttrModDefTableRow : FTableRowBase`**（**定义字段的唯一声明处** + **编辑期载体**）：`TemplateTag: FGameplayTag`（身份，2026-09-22 改造——原 `FName TemplateId`）+ `Target: FGameplayTag` / `Op` / `Operand: FTcsAttrModOperandDef` / `ValueConvention: ETcsValueConventionFlag`（D5-18 约定列，物化边界经 `FTcsValueConvention::ConvertToCanonical` 转规范值）/ `OverridePriority`（`TAO_Override` 用，2026-09-18 增补）/ `SortKey`。**行身份 = RowName（= DA 资产名）**，`TemplateTag` 是内容身份；
- **`UTcsAttrModDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身 `TemplateTag` 与**组合持有的一行** `Def`——MUST NOT 复制模板字段集；MUST 同款**显式声明 `PrimaryAssetType` + 覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, TemplateTag.GetTagName()]`**。

**无分组标签字段（2026-09-23 用户拍板）**：模板行**不携带**"同来源内分组标签"字段——原 `Tag: FName` 与账本侧同名同物、同样零消费者，已删除。**删除的额外理由**：该字段在模板行上是 `UPROPERTY(EditAnywhere)`，配置者在编辑器里看得见它、填了却无任何效果——**死字段比缺字段更难查**。若将来出现真实需求，MUST 以 `FGameplayTag` 形态重新引入。

**表格编辑局限（记录在案）**：`Operand.Literal` 是 `FTcsParamValue`（`TInstancedStruct` 载荷），CSV/Excel 往返不保留该列（引擎 CSV 导入无法表达多态实例结构）——本表行只支持**编辑器内表格编辑**，标量列仍可表格批量编辑。

基类取 `UPrimaryDataAsset` 是 **Def 资产族的统一约定**（2026-09-17 拍板）——Def 引用语义本是"id + 注册表/DefLibrary 解析"，主资产身份让"id ↔ 资产"解析、按类型发现/加载与打包分块归属由引擎提供（未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类）。

MUST 在 `IsDataValid`（`WITH_EDITOR`）实现 **D5-18 v3 约定列白名单**：约定列非 `VCF_None` 时，其数值来源 MUST 允许约定（判据由源自身声明——见 `param-value` 能力的约定能力位）；`ParamRef` 与 `AttributeScaled` 禁配（前者二次转换、后者约定作用对象有歧义）——违者报错且**错误挂该配置元素**（可操作建议，非仅日志）。同时 MUST 校验：`TemplateTag` 无效、数值来源为空、`OPK_AttributeScaled` 而属性 tag 无效、`Target` 无效。
**覆盖优先级的适用范围 MUST 给警告（非错误）**：`Op != TAO_Override` 而行内 `OverridePriority != 0` 时，该值不参与折叠——配置本身无害，但**不得静默**（2026-09-18 增补）。

#### Scenario: 模板双轨同款组织

- **WHEN** 检查修正器模板的资产与表行
- **THEN** 资产持 `TemplateTag` + 一行 `Def`，字段集只在表行声明一次（资产不复制字段）

#### Scenario: 模板行无分组标签字段

- **WHEN** 在编辑器里检视 `FTcsAttrModDefTableRow` 的字段
- **THEN** 不含任何"同来源内分组标签"字段（不给配置者一个填了无效果的控件）

#### Scenario: 字面量源可配约定

- **WHEN** 模板配 `Literal` 源 + `VCF_Percent`
- **THEN** 物化边界按 `ConvertToCanonical` 转规范值（策划写 85 → 账本 0.85）

### Requirement: 属性值参数源

`TcsAttribute` MUST 提供 `FTcsParamSource_AttributeScaled : FTcsParamValueSource`（PV-3）：`Attribute: FGameplayTag`（2026-09-22 改造——原 `FTcsAttributeName`）/ `Coefficient: double = 1.0` / `Fallback: double`；求值 = `Coefficient × Current(Attribute)`（**Snapshot 语义——R3 唯一路径**：快照构建时求值一次冻结）。读取经**扩展求值上下文** `FTcsAttributeEvaluateContext : FTcsParamEvaluateContext`（PV-1 扩展机制：结构体继承 + 源内 checked cast，类型标识走 Core 上下文的 `GetScriptStruct()` 虚函数）——持 `Provider: TScriptInterface<ITcsAttributeProvider>`。

**"上下文单位"的落点（实施定案 2026-09-16）**：单位由 `ITcsAttributeProvider` 的实现者绑定（该契约签名不含单位参数，军官组件/Mass 桶适配器各绑自己的单位——02 §2.3），故 R3 上下文以读口本身代表"对谁求值"；PV-1 规划的 `Subject`（`FCombatEntityHandle`）与 `EffectiveLevel` 字段按 PV-1 既定时序随 TcsState 等级源同批进 Core 上下文——本任务**不预建**（该句柄是纯 C++ 值类型，不能作为反射 USTRUCT 的 UPROPERTY，预建即违反本能力的"上下文反射可见"约束）。

checked cast 失败或 Provider 为空 MUST 落 `Fallback`（不崩溃、不 ensure——上下文不匹配由 M8 校验矩阵兜底）。本源 MUST 覆写 `AllowsValueConvention()` 为 `false`（D5-18 v3）。R3 不实现 Live 模式与"读即登记"（已承诺基建，随 Live 化实现）。

#### Scenario: 属性值换算取用

- **WHEN** 扩展上下文的 Provider 给出 `Attribute = Tcs.Attr.AttackPower → 60`，源配 `Coefficient = 2.0`
- **THEN** 求值为 120

#### Scenario: 上下文不匹配落兜底

- **WHEN** 求值上下文是基础 `FTcsParamEvaluateContext`（无法 cast 为属性扩展上下文）或 Provider 为空
- **THEN** 返回 `Fallback`，不广播、不崩溃

#### Scenario: 本源禁配约定列

- **WHEN** 查询本源是否允许行级值约定列
- **THEN** 返回 false（系数若需百分比语义直接写小数）

### Requirement: 覆盖带的优先级与同优先级策略

`TcsAttribute` MUST 为 `TAO_Override` 带提供**显式的强弱排座次**（2026-09-18 用户拍板）——理由是数值大小本身不含方向："取最大值"对护甲类属性（越大越强）成立，对承伤倍率/冷却类属性（越小越强）则取到最温和的一条，而框架无从知道语义方向。裁决阶梯 MUST 为：

1. **`OverridePriority`（修正器侧 `FTcsAttrModInstance`）**——大者胜，**唯一的第一裁决键**；数值大小不参与第一级。
2. **`OverrideTieBreak`（属性定义侧 `FTcsAttributeDefData` → 实例展开）**——优先级打平时按策略比较数值，封闭四值 `ETcsAttrOverrideTieBreak`：`OTB_Max = 0`（默认，取最大值）/ `OTB_Min = 1`（取最小值）/ `OTB_MaxAbs = 2`（取绝对值最大、符号保留）/ `OTB_MinAbs = 3`（取绝对值最小、符号保留）。
3. **有符号值**——策略下仍不可区分时（如 `OTB_MaxAbs` 下的 +5 与 -5）取有符号值大者，补齐**全序**。

MUST NOT 提供自定义策略（无 Custom 逃逸位）：这是热路径上的比较函数，必须全域且确定（赢家与遍历顺序无关）；开放自定义会把"谁说了算"重新变成不可静态推演的东西（与 `AVD_Custom` 区别：后者是值域语义、另有按 Clamp 的确定性回落）。
**框架 MUST NOT 定义任何其它"谁盖谁"的规则**（无来源分层约定、无按值域方向的隐含调整）——"到底以哪个为准"只能有一个答案，二次规则对使用者是纯负担。
`OverridePriority` MUST 只对 `TAO_Override` 生效（其余带忽略，编辑器侧给警告见 `attribute-moddef` 一节）；强弱裁决 MUST NOT 依赖遍历顺序——`Entries` 的任意排列 MUST 得到同一条赢家。
默认值 MUST 保持历史行为：全部优先级为 0 且策略为默认 `OTB_Max` 时，结果与"引入本机制之前取组内最大值"逐位一致。

#### Scenario: 优先级压过数值大小

- **WHEN** 两条 `TAO_Override`（值 -10 / 优先级 10）与（-20 / 优先级 20）
- **THEN** 结果为 -20——优先级大者胜，即使它在"取最大值"下本来会输

#### Scenario: 同优先级按属性语义方向裁决

- **WHEN** 同一属性的定义为 `OverrideTieBreak = OTB_Min`，两条同优先级 `TAO_Override`（-10 与 -20）
- **THEN** 结果为 -20（越低越强的属性语义由定义侧声明，修正器侧不必各自操心方向）

#### Scenario: 绝对值策略与符号保留

- **WHEN** `OverrideTieBreak = OTB_MaxAbs`，两条同优先级 `TAO_Override`（+3 与 -5）
- **THEN** 结果为 -5（幅度大者胜且**返回原值**——不是它的绝对值）

#### Scenario: 策略下打平也能确定

- **WHEN** `OverrideTieBreak = OTB_MaxAbs`，两条同优先级 `TAO_Override`（-5 与 +5）
- **THEN** 结果为 +5（第三级有符号值比较补齐全序——结果不随条目顺序改变）

#### Scenario: 默认值不改变既有行为

- **WHEN** 两条 `TAO_Override` 均未设优先级（0）且属性定义为默认策略
- **THEN** 结果为两条中数值较大者（与引入优先级之前的组内取值口径一致）

#### Scenario: 撤销赢家后自动递补

- **WHEN** 当前生效的 `TAO_Override`（最高优先级）被按其来源撤销（`RemoveBySource`）
- **THEN** 下一次折叠在剩余条目里重新选优（读侧选优——无需任何"递补/复活"状态）

### Requirement: 属性身份 = GameplayTag

`TcsAttribute` MUST 以 **`FGameplayTag`** 承载属性身份（2026-09-22 用户拍板，**重开 D2-1**）：属性名、参数键、DefTag 统一为 tag；**MUST NOT 再提供 `FTcsAttributeName` 包装结构**（该类型整体移除）。

**删除包装的正当性**（逐项核实）：`FTcsAttributeName` 的全部能力由 `FGameplayTag` 以等价或更强形式提供——
- **`explicit` 构造保护 → 更强**：`FGameplayTag(const FName&)` 是 **`protected`**（`GameplayTagContainer.h:219`，注释 `Intentionally private so only the tag manager can use`）——外部无法从 FName 直接构造 tag，只能走 `RequestGameplayTag(FName, bool ErrorIfNotFound = true)`（`:57`，**默认 ensure**）或原生 tag 常量。故"裸 FName 传不进属性 API"这条纪律以**引擎机制**提供，且**多一层存在性校验**（包装的 explicit 只拦隐式转换，拦不住"包装一个拼错的 FName"）。
- **`IsNone()` → `IsValid()`**：`GameplayTagContainer.h:138`，语义等价（`TagName != NAME_None`）。
- **`operator==` → 逐字相同**：`GameplayTagContainer.h:69`（`TagName == Other.TagName`）。
- **`GetTypeHash` → 逐字相同**：`GameplayTagContainer.h:168`（`GetTypeHash(Tag.TagName)`）——`FGameplayTag` 内部即 `FName`，**比较与哈希成本与现实现一致**。

**tag 来源（框架原生 + 项目 ini，用户 2026-09-22 拍板）**：属性名属**项目词汇**（插件 MUST NOT 持有战斗域词汇），故由**项目侧声明**——项目 `Config/DefaultGameplayTags.ini` 的 `+GameplayTagList=(Tag="Tcs.Attr.Health",…)`，代码侧经**缓存解析**取用（`RequestGameplayTag` 带 `TScopeLock(GameplayTagMapCritical)` + 重定向查询，`GameplayTagsManager.cpp:2372`，**MUST NOT 进热路径**）。

**属性名命名约定**：`Tcs.Attr.<Name>`（与事件 tag 的 `Tcs.Event.*` 并列，共用 `Tcs` 命名空间但域段隔离）。

#### Scenario: 属性 API 只接受 tag

- **WHEN** 调用点把裸 `FName` 或字符串字面量直接传给期望 `FGameplayTag` 的属性 API
- **THEN** 编译失败（`FGameplayTag(const FName&)` 是 protected，无法隐式构造）

#### Scenario: 拼错的属性 tag 在解析处即被拦截

- **WHEN** 项目 ini 未声明 `Tcs.Attr.Nonexistent` 而代码侧解析它
- **THEN** `RequestGameplayTag` 的默认 `ErrorIfNotFound = true` 触发 ensure（**不静默产空 tag**）——失败点前移到解析处，而非运行期查表 miss

#### Scenario: 作为 TMap 键

- **WHEN** 两个 `FGameplayTag` 承载同一 tag，分别作 `TMap<FGameplayTag, …>` 的键读写
- **THEN** 命中同一槽位（相等 + 哈希一致——由引擎实现保证）

### Requirement: 属性定义（双轨制 + tag 身份）

`TcsAttribute` MUST 以**双轨制**承载属性定义（2026-09-17 用户拍板）：**表 = 编辑期载体、资产 = 运行期载体**——DataTable 只服务策划批量编辑与编辑器即时响应，**不作为运行期加载源**；运行期按主资产身份解析资产。

**身份改为 tag（2026-09-22 改造）**：属性身份从 `FName DefId` 改为 **`FGameplayTag DefTag`**；定义内容抽为独立的**数据 struct**，两个载体各自持有 `DefTag + DefData`（用户 2026-09-22 方案："DA 里 `DefTag + DefStruct`；DT 表里，DA 资产名为 RowName，TableRow 为 `DefTag + DefStruct`"）。

三个类型，同住 `Attribute/TcsAttributeDef.h`：

- **`FTcsAttributeDefData`**（USTRUCT）——**定义字段的唯一声明处**：`BaseValue` + `Bounds` + `ValueDomain`（值域模式挂定义，D2-6）+ `OverrideTieBreak`（覆盖带同优先级策略，2026-09-18 增补）。抽出的理由是 **DataTable 行必须携带 `DefTag`**（tag 是内容身份），而字段集 MUST 仍只声明一次（资产组合持有、不复制）。
- **`FTcsAttributeDefTableRow : FTableRowBase`**（`FTableRowBase` 是 DataTable 行结构的 UHT 前提）：`DefTag: FGameplayTag` + `Def: FTcsAttributeDefData`。**行身份 = RowName（= DA 资产名）**——RowName 是引擎硬约束的 `FName`，无法承载 tag；`DefTag` 是**内容身份**、RowName 是**编辑期定位**，两者**分工而非双真相**（MUST NOT 要求二者同名；一致性由 M8 同步器维护）。
- **`UTcsAttributeDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身身份 `DefTag: FGameplayTag` 与**组合持有的定义数据** `Def: FTcsAttributeDefData`——MUST NOT 复制定义字段集（字段形状单份）。MUST **显式声明主资产类型** `static const FPrimaryAssetType PrimaryAssetType`（族语义固定，不靠类名派生）并**覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefTag.GetTagName()]`**（名取 tag 的 FName 形态而非资产名——资产文件可自由改名/挪目录而不失联；`FPrimaryAssetId` 的 name 位是 `FName`，tag 须经 `GetTagName()` 转换，这是引擎类型约束下的必要一步）。

MUST 在资产 `IsDataValid`（`WITH_EDITOR`）报错：`DefTag` 无效（`!DefTag.IsValid()`——主资产身份随之失去意义）。资产名与 `DefTag` 不一致**不再校验**（身份的名不取资产名，二者无需同名）。

两轨一致性 MUST 由编辑器侧同步器维护（资产为权威），运行期**零 DataTable 加载路径**；同步器与词表装载属 M8/M6 工具面，不在本能力范围。

#### Scenario: 运行期资产持有定义数据且身份按 tag 解析

- **WHEN** 读取 `UTcsAttributeDef` 的 `DefTag`、其组合数据 `Def` 与 `GetPrimaryAssetId()`
- **THEN** 定义字段来自 `Def`，且主资产身份等于 `[PrimaryAssetType, DefTag.GetTagName()]`（与资产文件叫什么无关）

#### Scenario: 编辑期表行以 RowName 为定位、以 DefTag 为身份

- **WHEN** 以 `FTcsAttributeDefTableRow` 作 `UDataTable::RowStruct`，以资产名写入行名、行内填 `DefTag = Tcs.Attr.Health`
- **THEN** 该行可被读出且字段往返保真（含 `OverrideTieBreak` 与 `DefTag`）；RowName 与 `DefTag` 不要求同名

#### Scenario: 空身份被拦截

- **WHEN** 资产 `DefTag` 无效时执行 `IsDataValid`
- **THEN** 报错（Invalid）

#### Scenario: 实例自持定义数据

- **WHEN** 单位由定义行添加属性后再读取实例
- **THEN** 实例的边界、值域模式与覆盖带同优先级策略来自定义行，但实例不持有定义行或资产的引用（改定义不影响已建实例）

