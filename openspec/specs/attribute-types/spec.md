# attribute-types Specification

## Purpose
TBD - created by archiving change add-tcsattribute-types-and-store. Update Purpose after archive.
## Requirements
### Requirement: 属性名包装

`TcsAttribute` MUST 提供 `FTcsAttributeName`（USTRUCT，反射可见——Def 资产与属性定义数据行需要承载它）：内含 `FName Name`（默认 `NAME_None`）、默认构造、**`explicit` 单参构造 `FTcsAttributeName(FName)`**（裸 FName/TEXT 传不进属性 API，D2-1）、`IsNone()`、相等比较与 `GetTypeHash`（供 `TMap` 键控）。**MUST NOT** 在本任务落"稠密 int32 id 缓存"字段——该字段的维护者（属性词表注册表）属 M8，零消费者不预建。

#### Scenario: 显式构造拦截裸 FName

- **WHEN** 调用点把裸 `FName` 或字符串字面量直接传给期望 `FTcsAttributeName` 的属性 API
- **THEN** 编译失败（D2-1 的 token 化保证）

#### Scenario: 作为 TMap 键

- **WHEN** 两个 `FTcsAttributeName` 包装同一 `FName`，分别作 `TMap<FTcsAttributeName, …>` 的键读写
- **THEN** 命中同一槽位（相等 + 哈希一致）

### Requirement: 运算带封闭枚举与带权

`TcsAttribute` MUST 提供封闭五带运算枚举 `ETcsAttributeOp`：`TAO_Add = 0`（默认）、`TAO_Override = 1`、`TAO_PercentAdd = 2`、`TAO_Mul = 3`、`TAO_FlatAdd = 4`；**无 Custom 逃逸位**（D2-7：计算一律在上游求值传入终值，聚合无代码插点）；新运算 = 末尾追加枚举值（D2-10 首例；追加需论证与既有带的交换性）。MUST 提供带权助手 `GetTcsAttributeBandWeight(ETcsAttributeOp)`（Override 0 / Add 10 / PercentAdd 15 / Mul 20 / FlatAdd 30，header-only）——**带序唯一真相在 Op**；账本修正器的 `SortKey` 仅为展示/审计位，折叠 MUST NOT 依赖它。

#### Scenario: 默认值与五带齐备

- **WHEN** 默认构造一个运算字段并读取枚举值
- **THEN** 取值为 `TAO_Add`（值 0）；五带对应带权分别返回 0 / 10 / 15 / 20 / 30

#### Scenario: 带权助手与 SortKey 不构成双真相

- **WHEN** 一条修正器被手工置 `SortKey = 0` 而 `Op = TAO_FlatAdd`
- **THEN** 折叠仍按 Op 落入 FlatAdd 带（带权按其 Op 判定，不由 SortKey 决定）

### Requirement: 操作数双形状

`TcsAttribute` MUST 提供操作数的定义侧与运行侧**两个形状**（D2-13，载体已由 PV 系列取代 D2-12）：

- 定义侧 `FTcsAttrModOperandDef`（USTRUCT）：`Kind: ETcsOperandKind`（`OPK_Literal = 0` 默认 / `OPK_AttributeScaled = 1`，**仅此两种**——D2-11 拒绝清单不破）、`Literal: FTcsParamValue`（可配等级表等源，模板默认值可等级化）、`Attribute: FTcsAttributeName`、`Coefficient: double = 1.0`；
- 运行侧 `FTcsAttrModOperand`（纯 C++ struct，反射面外）：`Kind` / `Literal: double`（**恒为已解析规范值**——账本零膨胀，物化器单点转换）、`Attribute` / `Coefficient`。

#### Scenario: 定义侧承载参数源

- **WHEN** 模板把 `Literal` 配为等级表源（`FTcsParamValue`）
- **THEN** 定义侧可容纳该源并参与求值；运行侧 `Literal` 始终是单个已解析 double

#### Scenario: 运算种类封闭

- **WHEN** 试图以第三种 Kind 表达"按时间衰减/随机/跨域引用"的取值
- **THEN** 无此枚举值可用（D2-11 拒绝清单的编译期体现）

### Requirement: 边界三态与值域模式

`TcsAttribute` MUST 提供：

- `ETcsAttributeBoundMode{ ABM_None = 0, ABM_Static = 1, ABM_Dynamic = 2 }` 与 `FTcsAttributeBound`（USTRUCT：`Mode` / `StaticValue: double` / `DynamicAttribute: FTcsAttributeName`）——Min 与 Max **各自独立三态**（D2-4）；`ABM_Dynamic` 的边界属性按管线求值（"HP ≤ MaxHP"形态），**自引用禁止**；
- `FTcsAttributeBounds`（USTRUCT：`Min` / `Max`，反射可见——属性定义数据行需要承载它）；
- `ETcsAttributeValueDomain{ AVD_Clamp = 0, AVD_Custom = 1, AVD_Wrap = 2 }`（D2-6）：`AVD_Custom` 是**逃逸位且固定值 1**（全插件 Custom=1 约定），语义为"IValueDomainPolicy 只接管值域函数，时序/级联/事务仍由引擎守护"。**值域策略接口不在本任务**（R3 未建，收口行为归 Task 5；使用 `AVD_Custom` 而策略接口缺席时的行为 MUST 在 Task 5 的收口点显式可见）。

#### Scenario: 边界两侧独立

- **WHEN** 属性定义 `Min = ABM_Static(0)`、`Max = ABM_Dynamic(MaxHealth)`
- **THEN** 两个边界各自按自己的模式处理（静态值 / 动态属性求值），互不牵连

#### Scenario: 值域逃逸位取值固定

- **WHEN** 读取 `AVD_Custom` 的枚举值
- **THEN** 值为 1（全插件 Custom=1 约定，与 Algo 无关）

### Requirement: 账本修正器与属性实例

`TcsAttribute` MUST 提供账本侧两个**纯 C++ struct**（不进反射面——D2-13 账本形状只为聚合热路径服务）：

- `FTcsAttrModInstance{ Target: FTcsAttributeName, Op: ETcsAttributeOp, Operand: FTcsAttrModOperand, Source: FTcsSourceHandle, SortKey: int32, Tag: FName }`——`Source` 是级联撤销锚点（D2-2：来源注销 → 按 Source 全量移除），`Tag` 为可选同来源内分组；
- `FTcsAttributeInstance{ Attr: FTcsAttributeName, BaseValue: double, CachedCurrent: double, bDirty: bool, Bounds: FTcsAttributeBounds, ValueDomain: ETcsAttributeValueDomain, ModifierSlots: TArray<FTcsAttrModInstance> }`——`CachedCurrent` 是**派生缓存非权威**（聚合管线唯一生产者、惰性重算，D2-8）。

#### Scenario: 修正器携带级联锚点

- **WHEN** 同一来源挂两条修正器到不同属性
- **THEN** 两条修正器的 `Source` 相等，可按该句柄一次性全量摘除（D2-2）

#### Scenario: 实例具备脏标记位

- **WHEN** 属性实例以 `BaseValue` 初始化并置 `bDirty = false`
- **THEN** 该实例在无修正器时 `CachedCurrent` 等于 `BaseValue`（Task 4 无管线写入路径；重算语义归 Task 5）

### Requirement: 属性定义（双轨制）

`TcsAttribute` MUST 以**双轨制**承载属性定义（2026-09-17 用户拍板）：**表 = 编辑期载体、资产 = 运行期载体**——DataTable 只服务策划批量编辑与编辑器即时响应，**不作为运行期加载源**；运行期按主资产身份解析资产。两个类型，同住 `Attribute/TcsAttributeDef.h`：

- **`FTcsAttributeDefTableRow : FTableRowBase`**（`FTableRowBase` 是 DataTable 行结构的 UHT 前提）——**定义字段的唯一声明处**：`BaseValue` + `Bounds` + `ValueDomain`（值域模式挂定义，D2-6）。**身份 = RowName（= 属性名，D2-1 的"行名 ↔ 项目侧常量"映射）——行内 MUST NOT 再存 id 字段**（2026-09-17 用户口径：DataTable 的键本就是行名，行内再放一份只会制造"行名与字段谁为准"的双真相）。
- **`UTcsAttributeDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身身份 `DefId` 与**组合持有的一行** `Def`（`FTcsAttributeDefTableRow`）——MUST NOT 复制定义字段集（字段形状单份）。MUST **显式声明主资产类型** `static const FPrimaryAssetType PrimaryAssetType`（族语义固定，不靠类名派生）并**覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefId]`**（名取 `DefId` 而非资产名——资产文件可自由改名/挪目录而不失联；引擎 `UPrimaryDataAsset` 文档亦指向此覆写路径）。

MUST 在资产 `IsDataValid`（`WITH_EDITOR`）报错：`DefId` 为空（主资产身份随之失去意义）。资产名与 `DefId` 不一致**不再校验**（身份的名不取资产名，二者无需同名）。
两轨一致性 MUST 由编辑器侧同步器维护（资产为权威，按"行名 = DefId"配对），运行期**零 DataTable 加载路径**；同步器与词表装载属 M8/M6 工具面，不在本能力范围。

#### Scenario: 运行期资产持有定义行且身份按 DefId 解析

- **WHEN** 读取 `UTcsAttributeDef` 的 `DefId`、其组合行 `Def` 与 `GetPrimaryAssetId()`
- **THEN** 定义字段来自该行，且主资产身份等于 `[PrimaryAssetType, DefId]`（与资产文件叫什么无关）

#### Scenario: 编辑期表行以行名为身份

- **WHEN** 以 `FTcsAttributeDefTableRow` 作 `UDataTable::RowStruct` 并以属性名为行名写入一行
- **THEN** 该行可被读出且字段往返保真；行结构内无 id 字段

#### Scenario: 空身份被拦截

- **WHEN** 资产 `DefId` 为空时执行 `IsDataValid`
- **THEN** 报错（Invalid）

#### Scenario: 实例自持定义数据

- **WHEN** 单位由定义行添加属性后再读取实例
- **THEN** 实例的边界与值域模式来自定义行，但实例不持有定义行或资产的引用（改定义不影响已建实例）

### Requirement: 修正器模板资产与约定列白名单

`TcsAttribute` MUST 以**同款双轨组织**提供修正器模板（D3-19 纯模板=默认值，引用处零字段覆写）：两个类型同住 `Attribute/TcsAttrModDef.h`——

- **`FTcsAttrModDefTableRow : FTableRowBase`**（**定义字段的唯一声明处** + **编辑期载体**）：`Target` / `Op` / `Operand: FTcsAttrModOperandDef` / `ValueConvention: ETcsValueConventionFlag`（D5-18 约定列，物化边界经 `FTcsValueConvention::ConvertToCanonical` 转规范值）/ `SortKey` / `Tag`——**身份 = RowName（= 模板 Id）**，行内 MUST NOT 再存 id 字段（同属性定义行口径）；
- **`UTcsAttrModDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身 `TemplateId` 与**组合持有的一行** `Def`——MUST NOT 复制模板字段集；MUST 同款**显式声明 `PrimaryAssetType` + 覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, TemplateId]`**。

**表格编辑局限（记录在案）**：`Operand.Literal` 是 `FTcsParamValue`（`TInstancedStruct` 载荷），CSV/Excel 往返不保留该列（引擎 CSV 导入无法表达多态实例结构）——本表行只支持**编辑器内表格编辑**，标量列仍可表格批量编辑。

基类取 `UPrimaryDataAsset` 是 **Def 资产族的统一约定**（2026-09-17 拍板）——Def 引用语义本就是"FName Id + 注册表/DefLibrary 解析"，主资产身份让"FName ↔ 资产"解析、按类型发现/加载与打包分块归属由引擎提供（未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类）。

MUST 在 `IsDataValid`（`WITH_EDITOR`）实现 **D5-18 v3 约定列白名单**：约定列非 `VCF_None` 时，其数值来源 MUST 允许约定（判据由源自身声明——见 `param-value` 能力的约定能力位）；`ParamRef` 与 `AttributeScaled` 禁配（前者二次转换、后者约定作用对象有歧义）——违者报错且**错误挂该配置元素**（可操作建议，非仅日志）。同时 MUST 校验：`TemplateId` 为空、资产 `TemplateId` 与行内不一致（各为错误）、数值来源为空、`OPK_AttributeScaled` 而属性名为空、`Target` 为空。

#### Scenario: 模板双轨同款组织

- **WHEN** 检查修正器模板的资产与表行
- **THEN** 资产持 `TemplateId` + 一行 `Def`，字段集只在表行声明一次（资产不复制字段）

#### Scenario: 字面量源可配约定

- **WHEN** 模板配 `Literal` 源 + `VCF_Percent`
- **THEN** `IsDataValid` 通过（本源书写的数值即结果，约定生效于其书写值）

#### Scenario: 引用源禁配约定

- **WHEN** 模板配 `ParamRef` 源 + `VCF_Percent`
- **THEN** `IsDataValid` 报错（读到的已是规范值，再转 = 二次转换）

#### Scenario: 属性换算源禁配约定

- **WHEN** 模板配 `OPK_AttributeScaled`（或 `AttributeScaled` 参数源）+ `VCF_Percent`
- **THEN** `IsDataValid` 报错（约定作用在系数还是乘积上无定义）

#### Scenario: 空来源与空名字报错

- **WHEN** 模板的数值来源为空，或 `OPK_AttributeScaled` 而属性名为 `None`，或 `Target` 为 `None`
- **THEN** `IsDataValid` 各自报错（编辑器保存期即见，策划即配即报）

### Requirement: 属性值参数源

`TcsAttribute` MUST 提供 `FTcsParamSource_AttributeScaled : FTcsParamValueSource`（PV-3）：`Attribute: FTcsAttributeName` / `Coefficient: double = 1.0` / `Fallback: double`；求值 = `Coefficient × Current(Attribute)`（**Snapshot 语义——R3 唯一路径**：快照构建时求值一次冻结）。读取经**扩展求值上下文** `FTcsAttributeEvaluateContext : FTcsParamEvaluateContext`（PV-1 扩展机制：结构体继承 + 源内 checked cast，类型标识走 Core 上下文的 `GetScriptStruct()` 虚函数）——持 `Provider: TScriptInterface<ITcsAttributeProvider>`。

**"上下文单位"的落点（实施定案 2026-09-16）**：单位由 `ITcsAttributeProvider` 的实现者绑定（该契约签名不含单位参数，军官组件/Mass 桶适配器各绑自己的单位——02 §2.3），故 R3 上下文以读口本身代表"对谁求值"；PV-1 规划的 `Subject`（`FCombatEntityHandle`）与 `EffectiveLevel` 字段按 PV-1 既定时序随 TcsState 等级源同批进 Core 上下文——本任务**不预建**（该句柄是纯 C++ 值类型，不能作为反射 USTRUCT 的 UPROPERTY，预建即违反本能力的"上下文反射可见"约束）。

checked cast 失败或 Provider 为空 MUST 落 `Fallback`（不崩溃、不 ensure——上下文不匹配由 M8 校验矩阵兜底）。本源 MUST 覆写 `AllowsValueConvention()` 为 `false`（D5-18 v3）。R3 不实现 Live 模式与"读即登记"（已承诺基建，随 Live 化实现）。

#### Scenario: 属性值换算取用

- **WHEN** 扩展上下文的 Provider 给出 `Attribute = AttackPower → 60`，源配 `Coefficient = 2.0`
- **THEN** 求值为 120

#### Scenario: 上下文不匹配落兜底

- **WHEN** 求值上下文是基础 `FTcsParamEvaluateContext`（无法 cast 为属性扩展上下文）或 Provider 为空
- **THEN** 返回 `Fallback`，不广播、不崩溃

#### Scenario: 本源禁配约定列

- **WHEN** 查询本源是否允许行级值约定列
- **THEN** 返回 false（系数若需百分比语义直接写小数）

