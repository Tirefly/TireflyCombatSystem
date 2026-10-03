## MODIFIED Requirements

### Requirement: 边界三态与值域模式

`TcsAttribute` MUST 提供：

- `ETcsAttributeBoundMode{ ABM_None = 0, ABM_Static = 1, ABM_Dynamic = 2 }` 与 `FTcsAttributeBound`（USTRUCT：`Mode` / `StaticValue: double` / `DynamicAttribute: FGameplayTag`）——Min 与 Max **各自独立三态**（D2-4）；`ABM_Dynamic` 的边界属性按管线求值（"HP ≤ MaxHP"形态），**自引用禁止**；
- `FTcsAttributeBounds`（`USTRUCT()`：`Min` / `Max`，类型反射可见——属性定义数据行需要承载它）；
- `ETcsAttributeValueDomain{ AVD_Clamp = 0, AVD_Custom = 1, AVD_Wrap = 2 }`（D2-6）：`AVD_Custom` 是**逃逸位且固定值 1**（全插件 Custom=1 约定），语义为"IValueDomainPolicy 只接管值域函数，时序/级联/事务仍由引擎守护"。**值域策略接口不在本任务**（R3 未建，收口行为归 Task 5；使用 `AVD_Custom` 而策略接口缺席时的行为 MUST 在 Task 5 的收口点显式可见）。

#### Scenario: 边界两侧独立

- **WHEN** 属性定义 `Min = ABM_Static(0)`、`Max = ABM_Dynamic(Attribute.MaxHealth)`
- **THEN** 两个边界各自按自己的模式处理（静态值 / 动态属性求值），互不牵连

#### Scenario: 值域逃逸位取值固定

- **WHEN** 读取 `AVD_Custom` 的枚举值
- **THEN** 值为 1（全插件 Custom=1 约定，与 Algo 无关）

### Requirement: 属性值参数源

`TcsAttribute` MUST 提供 `FTcsParamSource_AttributeScaled : FTcsParamValueSource`（PV-3）：`Attribute: FGameplayTag`（2026-09-22 改造——原 `FTcsAttributeName`）/ `Coefficient: double = 1.0` / `Fallback: double`；求值 = `Coefficient × Current(Attribute)`（**Snapshot 语义——R3 唯一路径**：快照构建时求值一次冻结）。读取经**扩展求值上下文** `FTcsAttributeEvaluateContext : FTcsParamEvaluateContext`（PV-1 扩展机制：结构体继承 + 源内 checked cast，类型标识走 Core 上下文的 `GetScriptStruct()` 虚函数）——持 `Provider: TScriptInterface<ITcsAttributeProvider>`。

**"上下文单位"的落点（实施定案 2026-09-16）**：单位由 `ITcsAttributeProvider` 的实现者绑定（该契约签名不含单位参数，军官组件/Mass 桶适配器各绑自己的单位——02 §2.3），故 R3 上下文以读口本身代表"对谁求值"；PV-1 规划的 `Subject`（`FCombatEntityHandle`）与 `EffectiveLevel` 字段按 PV-1 既定时序随 TcsState 等级源同批进 Core 上下文——本任务**不预建**（该句柄是纯 C++ 值类型，不能作为 `USTRUCT()` 类型的 `UPROPERTY` 字段，预建即违反本能力的"上下文类型反射可见"约束）。

checked cast 失败或 Provider 为空 MUST 落 `Fallback`（不崩溃、不 ensure——上下文不匹配由 M8 校验矩阵兜底）。本源 MUST 覆写 `AllowsValueConvention()` 为 `false`（D5-18 v3）。R3 不实现 Live 模式与"读即登记"（已承诺基建，随 Live 化实现）。

#### Scenario: 属性值换算取用

- **WHEN** 扩展上下文的 Provider 给出 `Attribute = Attribute.AttackPower → 60`，源配 `Coefficient = 2.0`
- **THEN** 求值为 120

#### Scenario: 上下文不匹配落兜底

- **WHEN** 求值上下文是基础 `FTcsParamEvaluateContext`（无法 cast 为属性扩展上下文）或 Provider 为空
- **THEN** 返回 `Fallback`，不广播、不崩溃

#### Scenario: 本源禁配约定列

- **WHEN** 查询本源是否允许行级值约定列
- **THEN** 返回 false（系数若需百分比语义直接写小数）

### Requirement: 属性身份 = GameplayTag

`TcsAttribute` MUST 以 **`FGameplayTag`** 承载属性身份（2026-09-22 用户拍板，**重开 D2-1**）：属性名、参数键、DefTag 统一为 tag；**MUST NOT 再提供 `FTcsAttributeName` 包装结构**（该类型整体移除）。

**删除包装的正当性**（逐项核实）：`FTcsAttributeName` 的全部能力由 `FGameplayTag` 以等价或更强形式提供——
- **`explicit` 构造保护 → 更强**：`FGameplayTag(const FName&)` 是 **`protected`**（`GameplayTagContainer.h:219`，注释 `Intentionally private so only the tag manager can use`）——外部无法从 FName 直接构造 tag，只能走 `RequestGameplayTag(FName, bool ErrorIfNotFound = true)`（`:57`，**默认 ensure**）或原生 tag 常量。故"裸 FName 传不进属性 API"这条纪律以**引擎机制**提供，且**多一层存在性校验**（包装的 explicit 只拦隐式转换，拦不住"包装一个拼错的 FName"）。
- **`IsNone()` → `IsValid()`**：`GameplayTagContainer.h:138`，语义等价（`TagName != NAME_None`）。
- **`operator==` → 逐字相同**：`GameplayTagContainer.h:69`（`TagName == Other.TagName`）。
- **`GetTypeHash` → 逐字相同**：`GameplayTagContainer.h:168`（`GetTypeHash(Tag.TagName)`）——`FGameplayTag` 内部即 `FName`，**比较与哈希成本与现实现一致**。

**tag 来源（框架原生 + 宿主 ini，用户 2026-09-22 拍板；2026-10-01 换根）**：属性名属**宿主词汇**（插件 MUST NOT 持有战斗域词汇），故由**宿主侧声明**——宿主 `Config/DefaultGameplayTags.ini` 的 `+GameplayTagList=(Tag="Attribute.Health",…)`，代码侧经**缓存解析**取用（`RequestGameplayTag` 带 `TScopeLock(GameplayTagMapCritical)` + 重定向查询，`GameplayTagsManager.cpp:2372`，**MUST NOT 进热路径**）。

**属性名命名约定（2026-10-01 换根）**：`Attribute.<Name>`。该根由**宿主仓的根段注册表**登记（全项目只有一个属性系统，故不加系统限定）；它与本插件的事件根 `TcsEvent` 并列——两者各自唯一指向一个消费场景（属性存储/聚合求值 vs 事件总线订阅/广播）。本插件 MUST NOT 在根段注册表里重复登记 `Attribute` 根，也 MUST NOT 声明任何具体的属性名。

#### Scenario: 属性 API 只接受 tag

- **WHEN** 调用点把裸 `FName` 或字符串字面量直接传给期望 `FGameplayTag` 的属性 API
- **THEN** 编译失败（`FGameplayTag(const FName&)` 是 protected，无法隐式构造）

#### Scenario: 拼错的属性 tag 在解析处即被拦截

- **WHEN** 宿主 ini 未声明 `Attribute.Nonexistent` 而代码侧解析它
- **THEN** `RequestGameplayTag` 的默认 `ErrorIfNotFound = true` 触发 ensure（**不静默产空 tag**）——失败点前移到解析处，而非运行期查表 miss

#### Scenario: 作为 TMap 键

- **WHEN** 两个 `FGameplayTag` 承载同一 tag，分别作 `TMap<FGameplayTag, …>` 的键读写
- **THEN** 命中同一槽位（相等 + 哈希一致——由引擎实现保证）

#### Scenario: 属性名根由宿主侧登记

- **WHEN** 检查属性名的声明方与根归属
- **THEN** 声明方是宿主 ini、根是宿主侧登记的 `Attribute`；本插件的根段注册表**不含**该根（也不含任何属性名）

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
- **THEN** 定义字段来自 `Def`，且主资产身份等于 `[PrimaryAssetType, DefTag.GetTagName()]`（与资产文件叫什么无关；`GetTagName()` 返回**完整 tag 文本**，故身份名里同时含根段）

#### Scenario: 编辑期表行以 RowName 为定位、以 DefTag 为身份

- **WHEN** 以 `FTcsAttributeDefTableRow` 作 `UDataTable::RowStruct`，以资产名写入行名、行内填 `DefTag = Attribute.Health`
- **THEN** 该行可被读出且字段往返保真（含 `OverrideTieBreak` 与 `DefTag`）；RowName 与 `DefTag` 不要求同名

#### Scenario: 空身份被拦截

- **WHEN** 资产 `DefTag` 无效时执行 `IsDataValid`
- **THEN** 报错（Invalid）

#### Scenario: 实例自持定义数据

- **WHEN** 单位由定义行添加属性后再读取实例
- **THEN** 实例的边界、值域模式与覆盖带同优先级策略来自定义行，但实例不持有定义行或资产的引用（改定义不影响已建实例）
