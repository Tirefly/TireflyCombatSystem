# state-def-asset Specification

## Purpose

定义共享层的 **Def 形状族**：`FTcsStateDefBase` 五组字段（类别 / 关系 / 等级 / 参数 / 修正器）、描述配置载体 `FTcsDescriptionEntry`，以及 Def 资产族基类 `UTcsStateDef`。
**它为什么住共享层**：`FTcsSkillDefData` 与 `FTcsBuffDef` 都继承 `FTcsStateDefBase`，两者都要白拿参数行 / 描述 / 关系四字段 / 修正器行——若这些形状住 `TcsBuff`，则 `TcsSkill` 必须依赖 `TcsBuff`，而 `TcsSkill.Build.cs` **明确不含它**。
**范围澄清**：Buff 专属的时值 / 堆叠 / 行为片段 / 表行 / 作者期校验归 `buff-def-asset` 能力。

## Requirements

### Requirement: 状态 Def 数据形状

> **枚举名订正（2026-10-17，`ARCH-07` D-3 / `G-18`）**：本需求里 `FTcsBuffDef` 的字段行原用 `EDurationPolicy` / `EDP_Finite` / `ETcsPeriodRefresh` / `EPR_Keep`——**同一行四个旧名，且四个在全仓均不存在**。实际是 `EBuffDurationPolicy`（`EBDP_*`）/ `ETcsBuffPeriodRefresh`（`EBPR_*`）——`TcsBuffEnums.h:14` / `:25`。
> **根因**：`PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 时，**三个枚举加了 `Buff` 前缀**（`EBuffApplyResult` / `EBuffDurationPolicy` / `ETcsBuffPeriodRefresh`）**而堆叠策略 struct 没加**（`FStateStackPolicy`），`EOverflowPolicy` / `EStackDurationPolicy` 也保持原名 ⇒ **一次改名没有单一规则**（`ARCH-07` §5 D-4）。
> ⇒ 判断某类型该不该带 `Buff` 前缀，**MUST 看 `TcsBuffEnums.h` 与 `TcsBuffStackPolicy.h`**，MUST NOT 按「看起来该统一」去推断。

`TcsState`（共享层）MUST 提供 Def 族的**基底数据形状**（`USTRUCT`，纯反射数据、可作 DataTable 行；字段集只声明一次，资产与表行各自组合持有）：

- `ETcsParamMode{ EPM_Snapshot = 0, EPM_Live = 1 }`——`0` 即默认值；`Snapshot` 在施加时求值一次冻结，`Live` 供技能侧使用；
- `FTcsNumericParamRow{ FGameplayTag Key, FTcsParamValue Base, ETcsParamMode Mode = EPM_Snapshot, ETcsValueConventionFlag ValueConvention = VCF_None }`——
  键 MUST 落 `TcsStateParam` 根（具体键由宿主声明，插件 MUST NOT 声明任何键）；
- `FTcsStateDefBase`（抽象：**中性默认实现 + `meta = (Hidden)`**，MUST NOT 用 `=0` 或 `PURE_VIRTUAL`）：
  `FGameplayTagContainer CategoryTags` / `int32 LevelBase = 1` / `int32 MaxLevel = 0` / `TArray<FTcsNumericParamRow> Params` / `TArray<FTcsDescriptionEntry> Descriptions`（载体见「描述配置载体」需求）/ `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows`；

#### Scenario: 时值与堆叠只在派生结构体

- **WHEN** 检查 `FTcsStateDefBase` 的成员
- **THEN** 不含 `DurationPolicy` / `DurationTime` / `Period` / `PeriodRefresh` / `StackPolicy` / `Fragments`（它们均住 `FTcsBuffDef`）——R6 的 `FSkillDef` 因此能只继承"参数行 + 描述 + 修正器行"

#### Scenario: 类别容器为空是合法配置

- **WHEN** 某状态定义资产的 `CategoryTags` 为空容器
- **THEN** 该资产仍视为合法（`IsDataValid` 不报错、定义库发现期不拦）——容器是**筛选依据**，不是身份；空容器在有消费者筛选时自然不命中任何查询

#### Scenario: 类别容器支持多元素与层级匹配

- **WHEN** 一个 buff 的 `CategoryTags` 配 `[Category.Burn.Fire, Category.Debuff]` 两个词
- **THEN** 该字段可容纳并往返保真；以父词（如 `Category.Burn`）作层级匹配时**命中该资产**（父词命中子词，与 `DefTag` 的既有匹配语义一致）

### Requirement: 描述配置载体（本轮最小集）

`TcsState` MUST 提供描述配置的两个**纯数据**载体（**住 `TcsState`**：`Source/TcsState/Public/Def/TcsDescriptionEntry.h`，导出宏 `TCSSTATE_API`；`DEC-02-fold-display` 的 v3 命名批表明写"描述视图槽位 / 配置条目 → TcsState（Def 形状所在）"——**该句原文就是对的**，`PLN-REFACTOR-1` Task 4 只把 **Buff 专属字段**搬去 `TcsBuff`，**共用 Def 基类留在 `TcsState`**；
TcsNotation 只拥有**词汇约定**与视图机制——"描述配置空间（字段归各 Def；词汇约定归本模块）"）：

> **载体订正（2026-10-17，`ARCH-02` D-1 / `ARCH-00` §3.1 同类）**：本条原写「`TcsBuff` MUST 提供…（**住 `TcsBuff`**）…自 Task 4 起 Def 形状住 `TcsBuff`」——**这个外推是错的**。`Descriptions` 字段挂在 `FTcsStateDefBase`（`TcsStateDefBase.h:177`，`TCSSTATE_API`），而 `FTcsSkillDefData : FTcsStateDefBase` 也要白拿描述；若载体真住 `TcsBuff`，则 `TcsSkill` 必须依赖 `TcsBuff`——但 `TcsSkill.Build.cs` **明确只依赖 Core / Notation / Attribute / Effect / State**，不含 `TcsBuff`。⇒ 原说法**在物理上不可实现**。根因是把「Buff 专属 Def」与「共用 Def 基类」混为一谈，于是把「Def 形状住 `TcsBuff`」错误外推到基类载体上。

- `FTcsDescriptionEntry{ FName DescriptionId, FName TextKey, TArray<FTcsDescriptionViewSlot> Views }`——`DescriptionId` = 多描述入口的展示侧命名标签（`"Tip"` / `"Codex"` / `"LevelUpPreview"`…）；
  `TextKey` = StringTable 键（`FText::FromStringTable` 的引擎约束是 `FName`）；三个 `FName` 是 2026-09-22 tag 化改造的**明确例外**（展示侧标签与 StringTable 键，不参与任何内容引用与解析）；
- `FTcsDescriptionViewSlot{ FName SlotName, FInstancedStruct View }`——`SlotName` 对应 StringTable 文案里的占位符名（文案零语法：`"对目标造成 {Rate} 的火焰伤害"`）。

**本轮 MUST NOT 建视图策略族**：`FTcsParamView` 基类（`BuildText` / `IsCompatible`）、四个内置视图（`Value` / `Series` / `Range` / `Attribute`）、`FTcsViewBuildContext` / `FTcsViewProbe` 与组装器全部归 R8（`SPEC-07-notation` §2.2），
故本轮 `View` 是**普通 `FInstancedStruct`、不带 `meta = (BaseStruct=…)`**；`FTcsParamView` 落地时 MUST 回补该约束（类型限制届时由 `meta` 收紧），MUST NOT 用"Kind 枚举 + 载荷"的旧形态过渡。

**消费者登记**：本轮 `Descriptions` **零真实消费者**（渲染归 R8、技能面板归 **R8**——面板依赖 `FTcsParamView` 视图策略族，而该族被同一份规格明文钉在 R8；2026-10-06 R6 Task 1 改判，原写"技能面板归 R6"）——只落字段与作者期校验，如实登记。

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

`TcsState` MUST 提供 Def 资产族**基类** `UTcsStateDef : UPrimaryDataAsset`（Def 资产族的统一约定；`UTcsBuffDefAsset` 与 `UTcsSkillDef` 都继承它）：
- 持内容身份 `DefTag: FGameplayTag`，并承载 `IsDataValid` 校验挂点；
- MUST 显式声明 `static const FPrimaryAssetType PrimaryAssetType`（取值 = 类名，不依赖引擎的类名回退），并覆写 `GetPrimaryAssetId()` 返回 `[PrimaryAssetType, DefTag.GetTagName()]`；
- **MUST NOT 持任何域数据**——它只持身份与校验挂点；派生资产（`UTcsBuffDefAsset` / `UTcsSkillDef`）各自持自己的数据 struct。

**派生资产的命名判据（不分域，族级约定）**：当 `<Family>Def` 已被同族**数据类型**占用时，资产类 MUST 加 `Asset` 后缀消歧——UHT 按"去 `U/A/F/E/I/T` 前缀后的引擎名"判重，资产类与同族数据 struct 同名（`UTcsBuffDef` vs `FTcsBuffDef`）在 **UHT 阶段**即失败；补救方向 MUST 是给资产类改名，MUST NOT 反过来改已定名的数据类型（先例 `UTcsEffectTriggerDefAsset`）。

#### Scenario: 主资产身份按 tag 解析

- **WHEN** 读取某派生资产（如 `UTcsBuffDefAsset`）的 `DefTag` 与 `GetPrimaryAssetId()`
- **THEN** 主资产身份 = `[PrimaryAssetType, DefTag.GetTagName()]`，与资产文件叫什么名字、住哪个目录无关（资产可改名/挪目录而不失联）

#### Scenario: 族基类不含 buff 专属字段

- **WHEN** 检查 `UTcsStateDef`
- **THEN** 它只持 `DefTag` 与校验挂点，不含 `BuffDef` 或任何域数据（`UTcsBuffDefAsset` / `UTcsSkillDef` 各自持自己的数据 struct）
