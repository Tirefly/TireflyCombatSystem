## MODIFIED Requirements

### Requirement: 状态 Def 数据形状

`TcsState` MUST 提供状态定义的**数据形状**（`USTRUCT`，纯反射数据、可作 DataTable 行；字段集只声明一次，资产与表行各自组合持有）：

- `ETcsParamMode{ EPM_Snapshot = 0, EPM_Live = 1 }`——`0` 即默认值；`Snapshot` 在施加时求值一次冻结，`Live` 供技能侧使用；
- `FTcsNumericParamRow{ FGameplayTag Key, FTcsParamValue Base, ETcsParamMode Mode = EPM_Snapshot, ETcsValueConventionFlag ValueConvention = VCF_None }`——
  键 MUST 落 `TcsStateParam` 根（具体键由宿主声明，插件 MUST NOT 声明任何键）；
- `FTcsStateDefBase`（抽象：**中性默认实现 + `meta = (Hidden)`**，MUST NOT 用 `=0` 或 `PURE_VIRTUAL`）：
  `FGameplayTag StatusTag` / `int32 LevelBase = 1` / `int32 MaxLevel = 0` / `TArray<FTcsNumericParamRow> Params` / `TArray<FTcsDescriptionEntry> Descriptions`（载体见「描述配置载体」需求）/ `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows`；
- `FTcsBuffDef : FTcsStateDefBase`：时值 `EDurationPolicy DurationPolicy = EDP_Finite` / `FTcsParamValue DurationTime` / `double Period = 0.0` / `ETcsPeriodRefresh PeriodRefresh = EPR_Keep`；
  堆叠 `FStateStackPolicy StackPolicy`；行为 `TArray<FTcsEffectTriggerDef> Triggers`（内联触发行位）+ `TArray<FInstancedStruct> Fragments`（行为 Fragment 载荷，选择器经手写 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")` 收窄——形状与语义见 `state-behavior-fragments` 能力）；
  关系 `TArray<FGameplayTag> Blocks` / `TArray<FGameplayTag> Requires` / `int32 Priority = 0` / `TArray<FGameplayTag> Cancels`。

**字段划分 MUST 守死"无时值 / 无堆叠 = 基类、有时值 / 有堆叠 = 派生"**——R6 的 `FSkillDef` 要按继承白拿参数行与描述，而它无时值、无堆叠。

**基类 MUST NOT 预建的字段**：基类的"生命周期事件词汇"字段（消费者是**行为 Fragment 的兴趣 Tag**，而兴趣 Tag 直接住 `FTcsBuffDef::Fragments` 的片段里 ⇒ 基类那一层仍零消费者）；`Descriptions` 只落字段与作者期校验，**渲染归 R8**。

#### Scenario: 时值与堆叠只在派生结构体

- **WHEN** 检查 `FTcsStateDefBase` 的成员
- **THEN** 不含 `DurationPolicy` / `DurationTime` / `Period` / `PeriodRefresh` / `StackPolicy` / `Fragments`（它们均住 `FTcsBuffDef`）——R6 的 `FSkillDef` 因此能只继承"参数行 + 描述 + 修正器行"

#### Scenario: 内联触发行可配置

- **WHEN** 在 `FTcsBuffDef.Triggers` 填一条 `FTcsEffectTriggerDef`
- **THEN** 该字段可容纳并往返保真（本能力只落**配置位**；登记与退订时机归 `effect-trigger` 能力）

#### Scenario: 行为片段可配置

- **WHEN** 在 `FTcsBuffDef.Fragments` 填一条行为 Fragment
- **THEN** 该字段可容纳并往返保真（本能力只落**配置位**；订阅与退订时机归 `state-behavior-fragments` 能力）

#### Scenario: 关系字段只落形状

- **WHEN** 检查 `Blocks` / `Requires` / `Priority` / `Cancels` 的消费者
- **THEN** 本轮无检查器（字段语义未定稿，归 R5.5-e）——字段只落形状，零消费者如实登记
