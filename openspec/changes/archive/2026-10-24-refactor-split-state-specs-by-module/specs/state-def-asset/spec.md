## MODIFIED Requirements

### Requirement: 状态 Def 数据形状

> **枚举名订正（2026-10-17，`ARCH-07` D-3 / `G-18`）**：本需求里 `FTcsBuffDef` 的字段行原用 `EDurationPolicy` / `EDP_Finite` / `ETcsPeriodRefresh` / `EPR_Keep`——**同一行四个旧名，且四个在全仓均不存在**。实际是 `EBuffDurationPolicy`（`EBDP_*`）/ `ETcsBuffPeriodRefresh`（`EBPR_*`）——`TcsBuffEnums.h:14` / `:25`。
> **根因**：`PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 时，**三个枚举加了 `Buff` 前缀**（`EBuffApplyResult` / `EBuffDurationPolicy` / `ETcsBuffPeriodRefresh`）**而堆叠策略 struct 没加**（`FStateStackPolicy`），`EOverflowPolicy` / `EStackDurationPolicy` 也保持原名 ⇒ **一次改名没有单一规则**（`ARCH-07` §5 D-4）。
> ⇒ 判断某类型该不该带 `Buff` 前缀，**MUST 看 `TcsBuffEnums.h` 与 `TcsBuffStackPolicy.h`**，MUST NOT 按「看起来该统一」去推断。

`TcsBuff` MUST 提供状态定义的**数据形状**（`USTRUCT`，纯反射数据、可作 DataTable 行；字段集只声明一次，资产与表行各自组合持有）：

- `ETcsParamMode{ EPM_Snapshot = 0, EPM_Live = 1 }`——`0` 即默认值；`Snapshot` 在施加时求值一次冻结，`Live` 供技能侧使用；
- `FTcsNumericParamRow{ FGameplayTag Key, FTcsParamValue Base, ETcsParamMode Mode = EPM_Snapshot, ETcsValueConventionFlag ValueConvention = VCF_None }`——
  键 MUST 落 `TcsStateParam` 根（具体键由宿主声明，插件 MUST NOT 声明任何键）；
- `FTcsStateDefBase`（抽象：**中性默认实现 + `meta = (Hidden)`**，MUST NOT 用 `=0` 或 `PURE_VIRTUAL`）：
  `FGameplayTagContainer CategoryTags` / `int32 LevelBase = 1` / `int32 MaxLevel = 0` / `TArray<FTcsNumericParamRow> Params` / `TArray<FTcsDescriptionEntry> Descriptions`（载体见「描述配置载体」需求）/ `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows`；
- `FTcsBuffDef : FTcsStateDefBase`：时值 `EBuffDurationPolicy DurationPolicy = EBDP_Finite` / `FTcsParamValue DurationTime` / `double Period = 0.0` / `ETcsBuffPeriodRefresh PeriodRefresh = EBPR_Keep`；
  堆叠 `FStateStackPolicy StackPolicy`；行为 `TArray<FTcsEffectTriggerDef> Triggers`（内联触发行位）+ `TArray<FInstancedStruct> Fragments`（行为 Fragment 载荷，选择器经手写 `meta = (BaseStruct = "/Script/TcsBuff.TcsBuffBehaviorFragment")` 收窄——形状与语义见 `buff-behavior-fragments` 能力）；
  关系 `TArray<FGameplayTag> RequiredTags` / `TArray<FGameplayTag> ForbiddenTags` / `TArray<FGameplayTag> TagsToBlock` / `TArray<FGameplayTag> TagsToCancel` / `int32 Priority = 0`——**四数组为 2026-10-16 `R5.5-e1` 落地**（检查器见 `state-relation-check` 能力）。**⚠ 这五个字段实际住在基类 `FTcsStateDefBase`**（`TcsStateDefBase.h`，与 `CategoryTags` / `LevelBase` / `MaxLevel` / `Params` / `Descriptions` / `ModifierRows` 同处），不是 `FTcsBuffDef`——见下方订正。
  **改名映射（MUST 逐条照做，防语义反转）**：`Blocks` → **`ForbiddenTags`**（受动·前置：B 在时**我**不可施加）、`Requires` → **`RequiredTags`**（受动·前置：必须已存在）、`Cancels` → **`TagsToCancel`**（施动·终止）；**新增 `TagsToBlock`**（施动·预防：我在跑期间目标不能开始）。
  **⚠ `Blocks` MUST 映射到 `ForbiddenTags`、MUST NOT 映射到 `TagsToBlock`**——两格词形极近而**方向相反**（前者约束**自己**、后者约束**目标**），且该字段族当时零消费者 ⇒ 错映射**编译器不报错**，是静默的语义反转。**受动**（我自己的门禁）与**施动**（我对外的影响）两个方向 MUST 分开。
  **`Priority` 不属关系字段族**（它归槽位竞争表 `R5.5-e4`）：本变更 MUST NOT 触碰它，其物理去留亦然。

  > **字段归属订正（2026-10-17，`ARCH-05` D-7）**：关系四字段 + `Priority` **已上移基类 `FTcsStateDefBase`**，而本条把它们平铺写在 `FTcsBuffDef` 之下 ⇒ 读者会以为只有 Buff 域有关系语义。实际形状同上一条判据：`FTcsSkillDefData : FTcsStateDefBase` **也要白拿关系四字段**（技能侧的关系门禁 `TcsSkillOps_Relation.cpp` 就在读它们）。⇒ 本行应理解为「`FTcsBuffDef` **继承**到这些字段」，MUST NOT 读成「这些字段声明在 `FTcsBuffDef` 里」。

  > **`BaseStruct` 订正（2026-10-17，`ARCH-05` D-6）**：本行原写 `/Script/TcsState.TcsStateBehaviorFragment`——该类型**在全仓不存在**（`grep` 工作树与 `git log --all -S` 双零）。实际类型是 **`FTcsBuffBehaviorFragment`**（`USTRUCT`，`F` 前缀），住 `TcsBuff`（`State/TcsBuffBehaviorFragment.h:55`，`TCSBUFF_API`），故路径为 `/Script/TcsBuff.TcsBuffBehaviorFragment`（`TcsBuffDef.h:90` 实测）。根因同 `ARCH-05` 的 `G-12`（代码搬走、引用滞留）：Task 4 拆出 `TcsBuff` 时该 Fragment 基类随 Buff 专属语义一起搬走，而本条停在旧模块名上。⚠ **本条尤其危险**：`BaseStruct` 是**字符串元数据**，而 `openspec validate` **不校验字符串内容**（只校验 Requirement / Scenario 结构）⇒ 指向不存在类型的 metadata **永远不会被工具发现**，编辑器里表现为 picker 列不出任何类型。
  **数组去留不属本变更**：`DEC-2026-10-13-relation-row-shape` §4.4 已裁「Def 上三数组的去留归 `R5.5-e4`」⇒ 本变更**只改名 + 新增一格**，MUST NOT 把它们收拢成行结构体（那等于提前替 `e4` 决定去留）。

**`CategoryTags` 的语义与约束（2026-10-15 由 `StatusTag` 合并而来，MUST 读）**：它是**定义级类别容器**，声明"**我是什么**"（语义分类），**不是**"我要匹配谁"（那是消费者侧的查询）。

- **MUST NOT 必填**：容器为空是合法配置（有消费者按它筛选时，空容器自然不命中任何查询——这是明确结果，不是缺省故障）。**原 `StatusTag` 的"非空"硬约束随本合并取消**；
- **每词一义**：单个 `FGameplayTag` 能表达的一切（具体词 = 只作用于它自己），容器用**单元素**表达；容器额外支持多元素（如一个 buff 同具"火属性 + 异常状态"）与**层级匹配**（填父词覆盖一族）。⇒ 它是 `StatusTag` 的**超集**，不是另一个概念；
- **`DefTag` 与它分工**：`DefTag` = **唯一键**（定义库的解析键与去重键，重复即报错），答"这是哪份资产"；`CategoryTags` = **语义词**（不参与去重，多个资产可共享同值），答"这在语义上属于什么"。**两个查询通道各自独立、MUST NOT 混为一个并集匹配**——`DefTag` 表达不了"一类"，把它并进类别匹配会把"一族"退化成"一个资产"。

**字段划分 MUST 守死"无时值 / 无堆叠 = 基类、有时值 / 有堆叠 = 派生"**——R6 的 `FSkillDef` 要按继承白拿参数行与描述，而它无时值、无堆叠。

**基类 MUST NOT 预建的字段**：基类的"生命周期事件词汇"字段（消费者是**行为 Fragment 的兴趣 Tag**，而兴趣 Tag 直接住 `FTcsBuffDef::Fragments` 的片段里 ⇒ 基类那一层仍零消费者）；`Descriptions` 只落字段与作者期校验，**渲染归 R8**。

#### Scenario: 时值与堆叠只在派生结构体

- **WHEN** 检查 `FTcsStateDefBase` 的成员
- **THEN** 不含 `DurationPolicy` / `DurationTime` / `Period` / `PeriodRefresh` / `StackPolicy` / `Fragments`（它们均住 `FTcsBuffDef`）——R6 的 `FSkillDef` 因此能只继承"参数行 + 描述 + 修正器行"

#### Scenario: 类别容器为空是合法配置

- **WHEN** 某状态定义资产的 `CategoryTags` 为空容器
- **THEN** 该资产仍视为合法（`IsDataValid` 不报错、定义库发现期不拦）——容器是**筛选依据**，不是身份；空容器在有消费者筛选时自然不命中任何查询

#### Scenario: 类别容器支持多元素与层级匹配

- **WHEN** 一个 buff 的 `CategoryTags` 配 `[Category.Burn.Fire, Category.Debuff]` 两个词
- **THEN** 该字段可容纳并往返保真；以父词（如 `Category.Burn`）作层级匹配时**命中该资产**（父词命中子词，与 `DefTag` 的既有匹配语义一致）

#### Scenario: 内联触发行可配置

- **WHEN** 在 `FTcsBuffDef.Triggers` 填一条 `FTcsEffectTriggerDef`
- **THEN** 该字段可容纳并往返保真（本能力只落**配置位**；登记与退订时机归 `effect-trigger` 能力）

#### Scenario: 行为片段可配置

- **WHEN** 在 `FTcsBuffDef.Fragments` 填一条行为 Fragment
- **THEN** 该字段可容纳并往返保真（本能力只落**配置位**；订阅与退订时机归 `buff-behavior-fragments` 能力）

#### Scenario: 关系字段只落形状

> 场景标题里的「只落形状」是**建立时的轮次标签**（本仓裁定：`#### Scenario:` 标题在 MODIFIED
> 语义下**不可改名**，先例 = `openspec/changes/archive/2026-10-04-add-tcs-state-module/specs/plugin-descriptor/spec.md`）
> ——本场景的**判据已随 `R5.5-e1` 现况化**：关系字段**已有消费者**（`state-relation-check` 能力的检查器）。

- **WHEN** 检查关系四数组的消费者
- **THEN** `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel` 四者**均有检查器读取点**（apply 门禁 / 取消挂点 / 预防挂点）；**唯一仍零消费者的是 `Priority`**（不属关系字段族，归 `R5.5-e4` 槽位竞争表）——该事实 MUST 如实保留，MUST NOT 因「检查器已落地」而误读成「`Priority` 也已有消费者」
