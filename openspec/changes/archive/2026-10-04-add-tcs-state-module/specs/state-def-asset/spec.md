## Purpose

定义状态层 Def 家族的数据形状、资产身份、编辑期表行与作者期校验：`FTcsStateDefBase` / `FTcsBuffDef` 两个数据 struct、`UTcsStateDef` / `UTcsBuffDefAsset` 两个资产类与编辑期表行 `FTcsBuffDefTableRow`，让 buff 定义成为可发现、可校验、可解析的内容资产。

## ADDED Requirements

### Requirement: 状态 Def 数据形状

`TcsState` MUST 提供状态定义的**数据形状**（`USTRUCT`，纯反射数据、可作 DataTable 行；字段集只声明一次，资产与表行各自组合持有）：

- `ETcsParamMode{ EPM_Snapshot = 0, EPM_Live = 1 }`——`0` 即默认值；`Snapshot` 在施加时求值一次冻结，`Live` 供技能侧使用；
- `FTcsNumericParamRow{ FGameplayTag Key, FTcsParamValue Base, ETcsParamMode Mode = EPM_Snapshot, ETcsValueConventionFlag ValueConvention = VCF_None }`——
  键 MUST 落 `TcsStateParam` 根（具体键由宿主声明，插件 MUST NOT 声明任何键）；
- `FTcsStateDefBase`（抽象：**中性默认实现 + `meta = (Hidden)`**，MUST NOT 用 `=0` 或 `PURE_VIRTUAL`）：
  `FGameplayTag StatusTag` / `int32 LevelBase = 1` / `int32 MaxLevel = 0` / `TArray<FTcsNumericParamRow> Params` / `TArray<FTcsDescriptionEntry> Descriptions`（载体见「描述配置载体」需求）/ `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows`；
- `FTcsBuffDef : FTcsStateDefBase`：时值 `EDurationPolicy DurationPolicy = EDP_Finite` / `FTcsParamValue DurationTime` / `double Period = 0.0` / `ETcsPeriodRefresh PeriodRefresh = EPR_Keep`；
  堆叠 `FStateStackPolicy StackPolicy`；行为 `TArray<FTcsEffectTriggerDef> Triggers`（内联触发行位）；
  关系 `TArray<FGameplayTag> Blocks` / `TArray<FGameplayTag> Requires` / `int32 Priority = 0` / `TArray<FGameplayTag> Cancels`。

**字段划分 MUST 守死"无时值 / 无堆叠 = 基类、有时值 / 有堆叠 = 派生"**——R6 的 `FSkillDef` 要按继承白拿参数行与描述，而它无时值、无堆叠。

**本轮 MUST NOT 预建的字段**：基类的"生命周期事件词汇"字段（行为 Fragment 的兴趣 Tag 已是声明面 ⇒ 零消费者）；`Descriptions` 只落字段与作者期校验，**渲染归 R8**。

#### Scenario: 时值与堆叠只在派生结构体

- **WHEN** 检查 `FTcsStateDefBase` 的成员
- **THEN** 不含 `DurationPolicy` / `DurationTime` / `Period` / `PeriodRefresh` / `StackPolicy`（它们均住 `FTcsBuffDef`）——R6 的 `FSkillDef` 因此能只继承"参数行 + 描述 + 修正器行"

#### Scenario: 内联触发行可配置

- **WHEN** 在 `FTcsBuffDef.Triggers` 填一条 `FTcsEffectTriggerDef`
- **THEN** 该字段可容纳并往返保真（本能力只落**配置位**；登记与退订时机归 `effect-trigger` 能力）

#### Scenario: 关系字段只落形状

- **WHEN** 检查 `Blocks` / `Requires` / `Priority` / `Cancels` 的消费者
- **THEN** 本轮无检查器（字段语义未定稿，归 R5.5-e）——字段只落形状，零消费者如实登记

### Requirement: 描述配置载体（本轮最小集）

`TcsState` MUST 提供描述配置的两个**纯数据**载体（**住 TcsState**：`DEC-02-fold-display` 的 v3 命名批表明写"描述视图槽位 / 配置条目 → TcsState（Def 形状所在）"；
TcsNotation 只拥有**词汇约定**与视图机制——"描述配置空间（字段归各 Def；词汇约定归本模块）"）：

- `FTcsDescriptionEntry{ FName DescriptionId, FName TextKey, TArray<FTcsDescriptionViewSlot> Views }`——`DescriptionId` = 多描述入口的展示侧命名标签（`"Tip"` / `"Codex"` / `"LevelUpPreview"`…）；
  `TextKey` = StringTable 键（`FText::FromStringTable` 的引擎约束是 `FName`）；三个 `FName` 是 2026-09-22 tag 化改造的**明确例外**（展示侧标签与 StringTable 键，不参与任何内容引用与解析）；
- `FTcsDescriptionViewSlot{ FName SlotName, FInstancedStruct View }`——`SlotName` 对应 StringTable 文案里的占位符名（文案零语法：`"对目标造成 {Rate} 的火焰伤害"`）。

**本轮 MUST NOT 建视图策略族**：`FTcsParamView` 基类（`BuildText` / `IsCompatible`）、四个内置视图（`Value` / `Series` / `Range` / `Attribute`）、`FTcsViewBuildContext` / `FTcsViewProbe` 与组装器全部归 R8（`SPEC-07-notation` §2.2），
故本轮 `View` 是**普通 `FInstancedStruct`、不带 `meta = (BaseStruct=…)`**；`FTcsParamView` 落地时 MUST 回补该约束（类型限制届时由 `meta` 收紧），MUST NOT 用"Kind 枚举 + 载荷"的旧形态过渡。

**消费者登记**：本轮 `Descriptions` **零真实消费者**（渲染归 R8、技能面板归 R6）——只落字段与作者期校验，如实登记。

#### Scenario: 字段能承载多入口与多视图槽

- **WHEN** 在 `FTcsStateDefBase.Descriptions` 填一条 `DescriptionId = "Tip"`、`TextKey` 指向 StringTable、`Views` 内含两个不同 `SlotName` 的槽
- **THEN** 该配置可容纳、可往返保真（字段与槽位均为纯反射数据，不作 DataTable 之外的额外约束）

#### Scenario: 描述配置错误在作者期报出

- **WHEN** 资产分别配成 `DescriptionId` 为空 / `TextKey` 为空 / 同一条 Entry 内 `SlotName` 为空或重复
- **THEN** `IsDataValid` 各自报错并挂到对应配置元素（这三条规则**只在本需求声明一处**，校验需求只留指针）

#### Scenario: 视图族未落地时不假装可配

- **WHEN** 检查本轮 `FTcsDescriptionViewSlot.View` 的类型约束
- **THEN** 是普通 `FInstancedStruct`（无 `meta = (BaseStruct=…)`）——视图策略基类尚不存在，MUST NOT 以任何中心名单/枚举替代它

### Requirement: 状态 Def 资产身份

`TcsState` MUST 提供状态 Def 资产族，基类统一 `UPrimaryDataAsset`（Def 资产族的统一约定）：

- `UTcsStateDef : UPrimaryDataAsset`——族基类，持内容身份 `DefTag: FGameplayTag`，并承载 `IsDataValid` 校验挂点；MUST 显式声明 `static const FPrimaryAssetType PrimaryAssetType`
  （取值 = 类名，不依赖引擎的类名回退），并覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefTag.GetTagName()]`；
- `UTcsBuffDefAsset : UTcsStateDef`——持 `BuffDef: FTcsBuffDef` 与自身的主资产类型（取值 `"TcsBuffDefAsset"`）。

**资产类名 MUST 带 `Asset` 后缀**：UHT 按"去 `U/A/F/E/I/T` 前缀后的引擎名"判重，资产类与同族数据 struct 同名（`UTcsBuffDef` vs `FTcsBuffDef`）在 **UHT 阶段**即失败；
补救方向 MUST 是给资产类改名，MUST NOT 反过来改已定名的数据类型（先例 `UTcsEffectTriggerDefAsset`）。

资产 MUST NOT 复制数据字段集——身份（`DefTag`）与数据（`BuffDef`）分工，字段形状只在数据 struct 声明一次。

#### Scenario: 主资产身份按 tag 解析

- **WHEN** 读取某 `UTcsBuffDefAsset` 的 `DefTag` 与 `GetPrimaryAssetId()`
- **THEN** 主资产身份 = `[PrimaryAssetType, DefTag.GetTagName()]`，与资产文件叫什么名字、住哪个目录无关（资产可改名/挪目录而不失联）

#### Scenario: 族基类不含 buff 专属字段

- **WHEN** 检查 `UTcsStateDef`
- **THEN** 它只持 `DefTag` 与校验挂点，不含 `BuffDef`（buff 数据住 `UTcsBuffDefAsset`；R6 的 `UTcsSkillDef` 同此基类）

### Requirement: 状态 Def 表行（编辑期载体）

`TcsState` MUST 提供 `FTcsBuffDefTableRow : FTableRowBase`（`DefTag: FGameplayTag` + `BuffDef: FTcsBuffDef`），与资产构成**双轨制**：**表 = 编辑期载体、资产 = 运行期载体**——
运行期 MUST 零 DataTable 加载路径，一律按 `DefTag` 走定义库解析资产。

行身份 = `RowName`（引擎硬约束的 `FName`）是**编辑期定位**，行内 `DefTag` 是**内容身份**：二者是分工而非双真相，MUST NOT 被要求同名。
两轨一致性由编辑器侧同步器维护（R8 `TOOLS-3`，不在本能力内）。

#### Scenario: 表行字段往返保真

- **WHEN** 以 `FTcsBuffDefTableRow` 作 `UDataTable::RowStruct` 写入一行（RowName 与 `DefTag` 不同名）
- **THEN** 该行可被读出且字段往返保真（含 `Params` 参数行与 `Triggers` 内联触发定义）

#### Scenario: 运行期不读 DataTable

- **WHEN** 检查状态定义的运行期加载路径
- **THEN** 只有"定义库按 `DefTag` 解析资产"这一条；不存在任何 `UDataTable` 加载或行查找（双轨制的运行期半边）

### Requirement: 状态 Def 作者期校验

状态 Def 资产的 `IsDataValid`（`WITH_EDITOR`）MUST 报出以下**错误**（每条错误 MUST 挂在出问题的配置元素上，给出可操作信息）：
`DefTag` 无效 / `Params` 的 `Key` 重复 / `ModifierRows` 元素为空引用 / `Triggers` 内 `Def.EventTag` 无效 / `Triggers` 内 `Def.EffectChainId` 无效 / `DurationPolicy == EDP_Finite` 而 `DurationTime` 的数值来源为空。

MUST 报出以下**警告**（配置无害但不得静默）：`Period < 0`；`PeriodRefresh != EPR_Keep` 而 `Period == 0`。

`Descriptions` 的三条错误（`DescriptionId` / `TextKey` 为空、同一 Entry 内 `SlotName` 为空或重复）**只在「描述配置载体」需求声明一处**，本需求不重复其判据（同一份规则、单一真相）。

无错无警时 MUST 把基类返回的 `NotValidated` **提升为 `Valid`**——否则编辑器显示"未验证"、装置侧 `IsDataValid == Valid` 的断言失效。

#### Scenario: 六类错误各自报出

- **WHEN** 资产分别配成 `DefTag` 无效、`Params` 键重复、`ModifierRows` 含空引用、`Triggers` 内 `EventTag` 无效、`Triggers` 内 `EffectChainId` 无效、`Finite` 而 `DurationTime` 未配
- **THEN** `IsDataValid` 逐个报错（资产保存期即见，策划即配即报），且每条错误挂对应配置元素

#### Scenario: 两条警告不阻断保存

- **WHEN** 资产配 `Period = -1`，或 `PeriodRefresh = EPR_Reset` 而 `Period = 0`
- **THEN** `IsDataValid` 产出一条警告（非错误），资产仍视为合法

#### Scenario: 校验通过时提升为 Valid

- **WHEN** 资产无错无警
- **THEN** 返回 `Valid`（不是基类返回的 `NotValidated`）——装置侧据此断言"资产可用"
