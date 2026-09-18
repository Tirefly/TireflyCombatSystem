## MODIFIED Requirements

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

### Requirement: 账本修正器与属性实例

`TcsAttribute` MUST 提供账本侧两个**纯 C++ struct**（不进反射面——D2-13 账本形状只为聚合热路径服务）：

- `FTcsAttrModInstance{ Target: FTcsAttributeName, Op: ETcsAttributeOp, Operand: FTcsAttrModOperand, Source: FTcsSourceHandle, OverridePriority: int32, SortKey: int32, Tag: FName }`——`Source` 是级联撤销锚点（D2-2：来源注销 → 按 Source 全量移除），`Tag` 为可选同来源内分组；`OverridePriority` **仅 `TAO_Override` 读**（其余带忽略），`SortKey` 始终不参与折叠；
- `FTcsAttributeInstance{ Attr: FTcsAttributeName, BaseValue: double, CachedCurrent: double, bDirty: bool, Bounds: FTcsAttributeBounds, ValueDomain: ETcsAttributeValueDomain, OverrideTieBreak: ETcsAttrOverrideTieBreak, ModifierSlots: TArray<FTcsAttrModInstance> }`——`CachedCurrent` 是**派生缓存非权威**（聚合管线唯一生产者、惰性重算，D2-8）；`OverrideTieBreak` 由定义侧展开（热路径不回查定义）。

#### Scenario: 修正器携带级联锚点

- **WHEN** 同一来源挂两条修正器到不同属性
- **THEN** 两条修正器的 `Source` 相等，可按该句柄一次性全量摘除（D2-2）

#### Scenario: 覆盖优先级随实例进账本

- **WHEN** 一条 `TAO_Override` 修正器以 `OverridePriority = 9` 挂到某属性
- **THEN** 账本条目保留该值，折叠按它参与强弱裁决（非覆盖带上该字段不参与任何计算）

#### Scenario: 实例具备脏标记位

- **WHEN** 属性实例被新建
- **THEN** 实例带 `bDirty` 标记位且**新建即脏**——初值由聚合管线在"批外读取或提交"时结算（值域收口与既有修正器在该次结算生效）并清零脏标记（见 `attribute-store` 与 `attribute-pipeline` 能力）

### Requirement: 属性定义（双轨制）

`TcsAttribute` MUST 以**双轨制**承载属性定义（2026-09-17 用户拍板）：**表 = 编辑期载体、资产 = 运行期载体**——DataTable 只服务策划批量编辑与编辑器即时响应，**不作为运行期加载源**；运行期按主资产身份解析资产。两个类型，同住 `Attribute/TcsAttributeDef.h`：

- **`FTcsAttributeDefTableRow : FTableRowBase`**（`FTableRowBase` 是 DataTable 行结构的 UHT 前提）——**定义字段的唯一声明处**：`BaseValue` + `Bounds` + `ValueDomain`（值域模式挂定义，D2-6）+ `OverrideTieBreak`（覆盖带同优先级策略，2026-09-18 增补）——`OverrideTieBreak` 与本行的其它字段同款：只在本结构声明一次、随实例展开、不在修正器侧重复声明。**身份 = RowName（= 属性名，D2-1 的"行名 ↔ 项目侧常量"映射）——行内 MUST NOT 再存 id 字段**（2026-09-17 用户口径：DataTable 的键本就是行名，行内再放一份只会制造"行名与字段谁为准"的双真相）。
- **`UTcsAttributeDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身身份 `DefId` 与**组合持有的一行** `Def`（`FTcsAttributeDefTableRow`）——MUST NOT 复制定义字段集（字段形状单份）。MUST **显式声明主资产类型** `static const FPrimaryAssetType PrimaryAssetType`（族语义固定，不靠类名派生）并**覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefId]`**（名取 `DefId` 而非资产名——资产文件可自由改名/挪目录而不失联；引擎 `UPrimaryDataAsset` 文档亦指向此覆写路径）。

MUST 在资产 `IsDataValid`（`WITH_EDITOR`）报错：`DefId` 为空（主资产身份随之失去意义）。资产名与 `DefId` 不一致**不再校验**（身份的名不取资产名，二者无需同名）。
两轨一致性 MUST 由编辑器侧同步器维护（资产为权威，按"行名 = DefId"配对），运行期**零 DataTable 加载路径**；同步器与词表装载属 M8/M6 工具面，不在本能力范围。

#### Scenario: 运行期资产持有定义行且身份按 DefId 解析

- **WHEN** 读取 `UTcsAttributeDef` 的 `DefId`、其组合行 `Def` 与 `GetPrimaryAssetId()`
- **THEN** 定义字段来自该行，且主资产身份等于 `[PrimaryAssetType, DefId]`（与资产文件叫什么无关）

#### Scenario: 编辑期表行以行名为身份

- **WHEN** 以 `FTcsAttributeDefTableRow` 作 `UDataTable::RowStruct` 并以属性名为行名写入一行
- **THEN** 该行可被读出且字段往返保真（含 `OverrideTieBreak`）；行结构内无 id 字段

#### Scenario: 空身份被拦截

- **WHEN** 资产 `DefId` 为空时执行 `IsDataValid`
- **THEN** 报错（Invalid）

#### Scenario: 实例自持定义数据

- **WHEN** 单位由定义行添加属性后再读取实例
- **THEN** 实例的边界、值域模式与覆盖带同优先级策略来自定义行，但实例不持有定义行或资产的引用（改定义不影响已建实例）

### Requirement: 修正器模板资产与约定列白名单

`TcsAttribute` MUST 以**同款双轨组织**提供修正器模板（D3-19 纯模板=默认值，引用处零字段覆写）：两个类型同住 `Attribute/TcsAttrModDef.h`——

- **`FTcsAttrModDefTableRow : FTableRowBase`**（**定义字段的唯一声明处** + **编辑期载体**）：`Target` / `Op` / `Operand: FTcsAttrModOperandDef` / `ValueConvention: ETcsValueConventionFlag`（D5-18 约定列，物化边界经 `FTcsValueConvention::ConvertToCanonical` 转规范值）/ `OverridePriority`（`TAO_Override` 用，2026-09-18 增补）/ `SortKey` / `Tag`——**身份 = RowName（= 模板 Id）**，行内 MUST NOT 再存 id 字段（同属性定义行口径）；
- **`UTcsAttrModDef : UPrimaryDataAsset`**（Def 资产族统一基类）：持自身 `TemplateId` 与**组合持有的一行** `Def`——MUST NOT 复制模板字段集；MUST 同款**显式声明 `PrimaryAssetType` + 覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, TemplateId]`**。

**表格编辑局限（记录在案）**：`Operand.Literal` 是 `FTcsParamValue`（`TInstancedStruct` 载荷），CSV/Excel 往返不保留该列（引擎 CSV 导入无法表达多态实例结构）——本表行只支持**编辑器内表格编辑**，标量列仍可表格批量编辑。

基类取 `UPrimaryDataAsset` 是 **Def 资产族的统一约定**（2026-09-17 拍板）——Def 引用语义本就是"FName Id + 注册表/DefLibrary 解析"，主资产身份让"FName ↔ 资产"解析、按类型发现/加载与打包分块归属由引擎提供（未来 `UTcsStateDef` 家族与 `UTcsSkillModDef` 同此基类）。

MUST 在 `IsDataValid`（`WITH_EDITOR`）实现 **D5-18 v3 约定列白名单**：约定列非 `VCF_None` 时，其数值来源 MUST 允许约定（判据由源自身声明——见 `param-value` 能力的约定能力位）；`ParamRef` 与 `AttributeScaled` 禁配（前者二次转换、后者约定作用对象有歧义）——违者报错且**错误挂该配置元素**（可操作建议，非仅日志）。同时 MUST 校验：`TemplateId` 为空、资产 `TemplateId` 与行内不一致（各为错误）、数值来源为空、`OPK_AttributeScaled` 而属性名为空、`Target` 为空。
**覆盖优先级的适用范围 MUST 给警告（非错误）**：`Op != TAO_Override` 而行内 `OverridePriority != 0` 时，该值不参与折叠——配置本身无害，但**不得静默**（2026-09-18 增补）。

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

#### Scenario: 覆盖优先级填错带只警告不报错

- **WHEN** 模板 `Op = TAO_Add` 而行内 `OverridePriority = 5`
- **THEN** `IsDataValid` 结果为 Valid（无错误）**且**产出一条警告（该值不会参与折叠）
- **AND** `Op = TAO_Override` 时同配置不再产出该警告

## ADDED Requirements

### Requirement: 覆盖带的优先级与同优先级策略

`TcsAttribute` MUST 为 `TAO_Override` 带提供**显式的强弱排座次**（2026-09-18 用户拍板）——理由是数值大小本身不含方向："取最大值"对护甲类属性（越大越强）成立，对承伤倍率/冷却类属性（越小越强）则取到最温和的一条，而框架无从知道语义方向。裁决阶梯 MUST 为：

1. **`OverridePriority`（修正器侧 `FTcsAttrModInstance`）**——大者胜，**唯一的第一裁决键**；数值大小不参与第一级。
2. **`OverrideTieBreak`（属性定义侧 `FTcsAttributeDefTableRow` → 实例展开）**——优先级打平时按策略比较数值，封闭四值 `ETcsAttrOverrideTieBreak`：`OTB_Max = 0`（默认，取最大值）/ `OTB_Min = 1`（取最小值）/ `OTB_MaxAbs = 2`（取绝对值最大、符号保留）/ `OTB_MinAbs = 3`（取绝对值最小、符号保留）。
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
