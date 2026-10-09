# Change: 技能参数账本（双形状修正器 + 参数账本 + 带式折叠收束）

> 对应计划 `PLN-R6` Task 4（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`，Step 1~8）。
> 依赖：Task 2（账本）/ Task 3（快照 / 施法运行态）——两者均已归档。
>
> **★ 本稿是重写版（2026-10-09 第二版）**：第一版把修正器定成「定义侧 = 账本侧」的**单一形状**，
> 撞上既有代码纪律「账本 MUST NOT 持 `UObject` 引用」；经用户两条硬需求逼回读 M2 实现后，
> 改为**照 M2 的双形状**。差异与判据见 `research/r6-task4-param-ledger-mirror.md` 与
> `research/r6-task4-divergence-decisions.md`。

## Why

技能参数**今天改不了数**：Task 1~3 交付了定义形状、账本与施法运行态，但"某个参数键上挂一条修正、
按五带折叠出新值"这条链**完全没有载体**——`FTcsNumericParamModifier` /
`FTcsNumericParamModInstance` / `FTcsParamChain` / `FTcsEntrySelector` 在全仓 `Source/` **零命中**
（实测 269 个文件），`FTcsNumericParamModifier` 唯一一次出现还只是 `TcsSkillRegistry.h:27` 的一句**注释**。

本变更交付那条链，并**收束台账 `STAT-1`**：五带折叠器 `FoldTcsAttributeBands` 已有**单份实现**
（`TcsAttributeBandFold.h:115`），M2 属性聚合（`TcsAttributePipeline.cpp:251`）与 TcsDamage 流程属性
（`TcsFlowAttributes.cpp:45`）两处已接入，**只剩 M5 参数链这一处**。本变更即那一处。

### 用户两条硬需求（本稿全部设计取舍的裁决标准）

1. **ParamModifier 对 SkillParam 的修改，计算过程 MUST 与 AttributeModifier 有同样的聚合流程**；
2. **ParamModifier 的 Apply 流程与 Removal 流程 MUST 准确无误**。

### 一并闭合：Task 3 留下的一处**真实缺口**（MUST 记录）

起草本提案时实测发现：**Task 3 构建了参数快照，但没有任何"读数值"的消费者**——
`GetNumericParam` 逐行重新求值 `Def.Params`（绕开快照 ⇒ **数值会漂移**、且绕开"值约定只转一次"的单一收口），
激活上下文 `ParamTable` 恒为空（`TcsCastOps.cpp:29`），`Mode` 列**完全未被读取**（`BuildSkillSnapshot`
无 `Mode` 判断）。⇒ 归档规格 §Scenario「施法中途改数值来源不影响本次读数」**并未真正成立**。
本变更以 MODIFIED 增补 `skill-cast-runtime` 把它闭合（含 `EPM_Live` 分流与读取接线）。

## What Changes

### 1. ADDED `skill-param-chain`（新能力，7 条需求）

- **双形状修正器（照 M2 的 D2-13 裁定）**：
  - **定义侧** `USTRUCT(BlueprintType) FTcsNumericParamModifier{ParamKey, Op, Operand: FTcsParamValue, Source, SortKey, OverridePriority, CompeteGroup, ValueConvention}`——`Operand` 是多态参数源，**可**持对象引用；
  - **账本侧** 纯 C++ `FTcsNumericParamModInstance{ParamKey, Op, ResolvedValue: double, Source, SortKey, OverridePriority, CompeteGroup}`——**无 `USTRUCT`、无对象引用**；
  - **判据**：账本条目住 `TArray<FTcsLearnedSkillEntry>`（非 `UPROPERTY`、元素非反射）⇒ GC 看不见 ⇒ 若账本侧持 `FTcsParamValue`，那些对象会被**静默回收**（源变空引用，**不崩溃**）。M2 的账本侧注释即此义：「账本只为聚合热路径服务，不进反射面」。
  - **`SortKey` 与 `OverridePriority` 并存、各归其位**：折叠器读**后者**，前者只是同带内审计位。**MUST NOT 令 `OverridePriority = SortKey`**——那会让 M5 的 Override 胜负判据与 M2 **不同**（M2 是「优先级大者胜 → 打平才按策略比数值」三级比较，**不是**「取最大值」）。
- **参数账本（每条目一套槽位）**：`FTcsLearnedSkillEntry` 增 `TArray<FTcsNumericParamModInstance> NumericParamModInstances`（照 `FTcsAttributeInstance::AttrModInstances`）；`ApplyParamModifiers(Unit, FTcsEntrySelector, TArrayView<FTcsNumericParamModifier>)` + `RemoveParamModifiersBySource(Unit, Source)`（返回摘除条数；0 条命中 = **正常路径不 ensure**；Apply 与 Removal **成对**）。
- **参数链求值**：槽位 → `CompeteGroup` 分桶取组内最大者 → 摊平 `FTcsAttributeBandEntry{Op, Value, OverridePriority}` → **调 `FoldTcsAttributeBands(初值, Entries)`**（`STAT-1` 收束，`Source/TcsSkill/` 内**仅此一处**折叠调用）；初值 = 该键参数行求值结果，**无参数行则 0**。
- **物化边界**：求值 `Operand` + `ValueConvention` **只转一次**（能力位为假的源不转——判据由源自身声明）；账本**不二次转换**。
- **定义侧 `ParamChainRows` 的激活期物化**：`Source` = `FTcsCastRun.RunSource`（随施法终结级联摘除）；**只落内联形态**（模板引用归 `R6.5-f`）。
- **`FTcsSkillEntrySelector`**（**命名经实施期订正**，见下）：四档各有字段载体；**两个未实现档
  （`ByCategoryTags` / `Custom`）具名报出"未实现"且零施加**。
  **★ 实施期订正（2026-10-09 用户裁定）**：
  - **改名**：`FTcsEntrySelector` → **`FTcsSkillEntrySelector`**、`ETcsEntrySelectorMode` →
    **`ETcsSkillEntrySelectorMode`**（前缀 `ESS_`）、文件 → `Skill/TcsSkillEntrySelector.h`。
    判据 = 它**只服务技能账本条目**（状态侧没有"已学条目"这一层），名字带 `Skill` 免得将来第二个域
    出现时逼出改名（那时改动面是整个公开面）。
  - **`ByTag` → `ByDefTag`**（字段 `TagFilter` → `DefTagFilter`）：原档名含混——"Tag"在 TCS 里同时指
    属性词/状态词/事件词/参数键等七八种东西 ⇒ 与字段名 `DefTag` 对齐。
  - **删除 `ById` 档**（**判据充分的删除，MUST NOT 读成"漏做"**）：原档按 `FTcsSkillEntryHandle`
    （`Index` + `Generation`）精确命中，而该句柄是**运行时产物**——编辑器里没有、也无法预先填出合法值
    ⇒ 对"以配置为入口的外部施加"而言**这一档等于不存在**（只有代码能传）；且"按 Id 筛"的历史由来
    （Def 身份原为 `FName DefId`）**已于 2026-09-22 全量迁移到 `GameplayTag`（`DefTag`）** ⇒
    其正当形态就是按 tag 筛，已被 `ByDefTag` 覆盖。
  - **新增 `ByCategoryTags` 空壳**（承接删除位，**只留壳不落机制**）：按定义上的**类别标识容器**
    （`FGameplayTagContainer`：如一个 Buff 同具"火属性伤害 + 异常状态"，一个技能同具"左手/右手/双手
    释放（三选一）+ 投掷类 + 引导类"）筛选，规则**倾向复用引擎 `FGameplayTagQuery`**。
    **只留壳的三条判据**：① **载体不存在**（`StateDef` 侧类别标识容器字段尚未新增，属独立变更）；
    ② **规则与档名未拍板**（用户明示可能改名 `ByCategoryTag` / `ByCategoryTags`，**"还要讨论"**）；
    ③ **可能另立模块**（GameplayTag 筛选在 TCS 里将是重要角色、**甚至可能单独开 `TcsGameplayTag` 模块**）
    ⇒ 现在写进 `TcsSkill` 内部将来极可能要搬家。**台账登记 = `STAT-10`**。
- **`Level` 特殊键**：改 `EffectiveLevel`（`clamp(0, LevelBase + Σ)`，经**同一条**五带折叠），
  **不走**普通参数折叠；快照时点 = 激活时。**★ 实施期订正（2026-10-09 用户裁定 + 后续追问订正）**：该键的词
  MUST 由**插件模块原生声明**（带模块导出宏）——它是**框架自己解释的词**（公式由等级域求值）；
  下放宿主 ini 会让宿主漏配变成**静默失效**（数值照进账本、等级永不动、零报错）。
  **声明模块 = `TcsState`**（`Public/Param/TcsStateParamKeys.h`），**不是 `TcsSkill`** ——判据：
  ① `LevelBase`/`MaxLevel` 声明在 `FTcsStateDefBase`（技能 Def 继承它）、D3-11 的家在状态侧规格；
  ② `TcsSkill` 已依赖 `TcsState`，声明在上游则两侧消费零成本，**反过来会让状态侧消费时反向依赖 TcsSkill = 成环**。
  `TcsStateParam` 根的**一般键**仍归宿主 ini。

### 2. MODIFIED `skill-cast-runtime`（**闭合 Task 3 的真实缺口**）

「激活期参数快照绑定」增补：**`Mode` 列 MUST 参与分流**（`EPM_Snapshot` 冻结 / `EPM_Live` 标记跳过）；
**快照的消费面 = 求值期的参数表载体**（物化 `ParamChainRows` 时绑定，供 `ParamRef` 取值——
先例 = 状态侧 `FTcsStateModifierMaterializer`；**MUST NOT** 读成"公共读口的冻结来源"）；
并**就地登记该缺口**（初版只构建不消费 + `Mode` 未接）以免后人以为已实现。

> **★ 实施期对该条的订正（2026-10-09 用户裁定 A′，MUST 读）**：本稿初版把缺口写成
> "**`GetNumericParam` MUST 改走快照**"——**该措辞本身不成立**：快照住**每次 run**
> （`FTcsCastRun.ParamSnapshot`），而读口签名 `(Unit, EntryHandle, Key)` **没有 run 段**；
> 且 `CI_InstancePerExecution` 下同一 Entry **可有两个 run 并存**（Task 3 已实测"读数 = 2"）
> ⇒ "读哪一次施法的冻结值"**无唯一答案**。**真实缺口 = 快照零消费者 + `Mode` 未接**，
> 而非"读口未接快照"。⇒ 按 A′ 改为"快照作求值参数表"，`GetNumericParam` 保持**实时**语义
> （参数行初值 + 账本槽位折叠）；**不新增 run 作用域读口**（今天无消费者 = 零消费者预建）。
> **可证伪判据（已实测）**：`P16` 用 `ParamRef(KeyA, Fallback=-999)` + 参数行 `KeyA=42`
> ⇒ 读数 **42**（不是 -999）即证快照真被消费。

### 3. MODIFIED `skill-registry`（条目补 `NumericParamModInstances`）

Task 2 裁定条目字段**恰好六项**、且"MUST NOT 声明参数修正链字段（元素类型尚不存在）"⇒ 回补为**七项**，
并写明**元素是账本侧纯 C++ 类型** ⇒ 该需求原有的 GC 纪律（"账本 MUST NOT 持 `UObject` 引用"）
**依然成立、门面 ARO 无需改动**。

### 4. MODIFIED `skill-def-asset`（`ParamChainRows` 由"未声明"改为"已声明"）

Task 1 的裁定原文即约定"Task 4 以 MODIFIED 增补本需求"，本变更即 Task 4。

## 明确不做（非目标）

- **不做冷却与 Cost**（`R6.5-a` / `R6.5-d`）。
- **不做 `FBoolSwitchModifier` 及其账本条目（归 `R6.5-e`）**——
  **★ 用户 2026-10-09 追问「BoolSwitchParam 对应的 ParamModifier 也要有 ModifierInstance 吧？」**：
  **是，设计早已预留** `FBoolSwitchModifier{SwitchKey, Value, Source}`（`spec/05-module-skill.md:56`，
  D5-5 v2 命名统一，原 `FLogicGateModifier`），并**刻意极简**（布尔无 Σ/Π 代数、无可比大小、无值约定
  ⇒ 无 `Op`/`SortKey`/`CompeteGroup`/`ValueConvention`）。批次表 `:202` 归 `R6.5-e`，
  `TcsBoolSwitchRow.h` 文件头亦自证"布尔修正器归 R6.5"。
  **不并入本 Task 的实质理由**：布尔修正器**没有聚合计算**（无折叠器可共用），其价值全在
  "与 `GateCheck` 条件求值器配对"，而后者住 `TcsEffect` 条件系统、属 `R6.5-e` 同批
  ⇒ **先落一个零消费者的布尔修正器 = 零消费者预建**。
  **★ 但记一条形状预留（MUST）**：`R6.5-e` 的布尔槽位应是**并列的第二个数组**
  （**`TArray<FTcsBoolSwitchModInstance> BoolSwitchModInstances`**）而非挤进 `NumericParamModInstances` 同一数组——
  M2 的单一 `AttrModInstances` 成立是因为五带**同属一个代数**；布尔与数值**不同代数**，
  混装会让折叠必须按元素类型分派（M2 无此概念）。**该预留只写注释与规格，MUST NOT 现在就建空数组。**
  **★ 命名订正（2026-10-09 用户追问"BoolSwitch 的相关变量命名都是什么样的"后核实）**：
  本稿初版写 `FTcsBoolSwitchParamInstance`，**那是错的**——命名规则 = **`[表词] + Mod + 面后缀`**，
  两张表各有表词：数值侧 `NumericParam`（`FTcsNumericParamRow` / `FTcsNumericParamModifier` /
  `FTcsNumericParamModInstance`），布尔侧 **`BoolSwitch`**（`FTcsBoolSwitchRow` /
  `FTcsBoolSwitchModifier` / **`FTcsBoolSwitchModInstance`**）。初版那个名字把布尔表词与数值表词
  `Param` 混装，两头都不沾（`FTcsBoolSwitchRow` 本身也不带 `Param`——设计管它叫"布尔**开关**表"）。
- **不做 `UTcsSkillModDef` 的技能侧参数行分派**（`R6.5-f`）——实测该类型在全仓 `Source/` **零命中**，
  "引用目标"没有载体 ⇒ `ParamChainRows` 本 Task **只落内联形态**。
- **不做描述视图组装**（`FTcsParamView` 族归 R8）。
- **不做单位级"冻结暂存区"式扫描**（M2 `RemoveBySource` 要扫冻结区，是因为属性可被 `RemoveAttribute` 冻结；
  技能账本**今天没有**对应的冻结机制 ⇒ 本 Task 的摘除只扫在册条目。**若将来出现"技能条目冻结"，该扫描面 MUST 同批补上**——登记为边界。）

## Impact

- **Affected specs**：
  - `skill-param-chain`（**ADDED**，7 需求）
  - `skill-cast-runtime`（**MODIFIED** ×1：「激活期参数快照绑定」——补 `Mode` 分流 + 快照读取接线 + 缺口登记）
  - `skill-registry`（**MODIFIED** ×1：「已学技能账本」——字段六项 → 七项，补 `NumericParamModInstances`）
  - `skill-def-asset`（**MODIFIED** ×1：「技能 Def 数据形状」——`ParamChainRows` 由未声明改为已声明）
- **Affected code**（插件仓 TCS）：
  - 新：`Source/TcsSkill/Public/Skill/TcsNumericParamModifier.h`（双形状：定义侧 `FTcsNumericParamModifier` + 账本侧 `FTcsNumericParamModInstance`）、
    `Skill/TcsParamChain.h` + `Private/Skill/TcsParamChain.cpp`（账本 + Apply/Removal）、
    `Skill/TcsParamChainOps.h`（求值流水 + 物化边界）、`Skill/TcsEntrySelector.h`
  - 改：`Source/TcsSkill/Public/Def/TcsSkillDefData.h`（回补 `ParamChainRows`）、
    `Public/Skill/TcsSkillRegistry.h`（回补 `NumericParamModInstances`）、`Public/TcsSkillSubsystem.h` + 分片（门面转发）、
    `Private/Skill/TcsSkillOps_ParamRead.cpp`（读取面改走快照——**本项闭合 Task 3 缺口**）、
    `Private/Skill/TcsCastOps_Snapshot.cpp`（加 `Mode` 分流）
- **跨仓**（宿主 LAC）：扩既有 `TcsDevSkillCastProbe*` 加参数链读数（**复用既有装置**，不新建第三个——
  判据 = Task 0 的用户裁定"探针 MUST 复用"）；`Config/DefaultGameplayTags.ini` 补检查键。
- **台账**：`STAT-1` 标**已消费（全三处）**（M2 / TcsDamage / **M5 本变更**）。
- **设计文本同批划改（MUST NOT 漏）**：`spec/05-module-skill.md:56` 那句同时含三处失步
  （只列 `SortKey` 无 `OverridePriority`、"`SortKey` 退化为带权"、"Override 组取**最大值**"）
  ⇒ MUST 保留原文 + 就地注明订正理由（按 D5-5 v3 的实际口径与 M2 实现），
  否则会留下"设计 7 字段 / 实现 8 字段"的第二真相。
