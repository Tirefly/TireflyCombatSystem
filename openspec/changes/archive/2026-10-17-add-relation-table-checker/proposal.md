# Change: 关系表检查器（`Blocks` / `Requires` / `Cancels` 第一次有真实消费者）

## Why

关系表族四字段（`Blocks` / `Requires` / `Priority` / `Cancels`）自 R5/R6 起**形状已落、零消费者**——两个 Def 上都配了字段，但**没有任何代码读它们**（全仓 grep 消费点为零）。后果是三条：

1. **施加态与施法态的共存冲突没有确定结果**——同一实体的两个互斥状态/技能可以同时存在，且无人报错；
2. **`EBRC_Cancelled` 无内建产生者**——该枚举自落地起只有注释自述"由调用方显式传入"，即框架给了原因却从不产生它；
3. **`CategoryTags` 仍零解析消费者**——它自 2026-10-15 由 `StatusTag` 合并而来，是关系表匹配的**主力词面**，但至今没有读取点。

本变更让这三个空缺**同批关闭**，且把**关系行**这一层第一次落成可复用类型（`e2` 的类目降维层直接复用，不重写）。

## What Changes

- **新增关系检查器**（`FTcsStateRelation`，住 `TcsState`）：纯 C++ 静态函数族，**按值取参**（主体词集 + 客体词集 + 四个容器视图），判定只走 `FGameplayTag::MatchesTag` 层级匹配——**不引入 `FGameplayTagQuery`**；
- **新增关系行类型 `FTcsRelationRow`**（纯值 struct，住 `TcsState`）：`{AppliesToTag, RequiredTags, ForbiddenTags, TagsToBlock, TagsToCancel}` 五字段。**e1 只接 Def 字段这一个来源**，e2 接中心 `UDataAsset` 时复用同一类型（判据见 design.md D1）；
- **BREAKING**：两个 Def 上的四字段**改名**（零消费者、零迁移成本）——
  `Blocks` → **`ForbiddenTags`**（**受动/前置**，约束自己）、`Requires` → **`RequiredTags`**、`Cancels` → **`TagsToCancel`**；**新增 `TagsToBlock`**（**施动/对外**，约束目标）。
  ⚠️ **`Blocks` MUST 映射到 `ForbiddenTags`、MUST NOT 映射到 `TagsToBlock`**——两者词形相近而**方向相反**，错映射 = 静默语义反转；
- **apply 挂点**：`ForbiddenTags` 命中 ⇒ 拒绝（`EBAR_Rejected` + 具名 `Log`，**零事件**）；`RequiredTags` 未满足 ⇒ 同样拒绝。判定**排在共存决策之前**；
- **`TagsToCancel` 挂点**：施加成功路径按它逐条移除既有状态，原因传 `EBRC_Cancelled`——**该枚举首次有内建产生者**；
- **`TagsToBlock` 挂点**（预防语义）：覆盖**两处时机**——① 目标尝试开始时；② **声明方开始时就已存在的目标须被即时阻断**；
- **反向查找**：`ForbiddenTags` 的检查时机取「**apply 时反向查找**」（"是否存在存活实例的 `TagsToBlock` 覆盖我"），而非要求作者双向声明（用户 2026-10-16 裁定）；
- **技能侧调用点**：`TryActivate` 门禁序列插入关系门（同检查器，主体 = `FTcsSkillInstance`）；
- **BREAKING**：`Interrupted` → `Canceled` 一族改名（`ECFR_Interrupted` → `ECFR_Canceled`、`IsInterruptibleNow()` → `CanBeCanceled()`、`bInterruptible` → `bCancellable`、`bInterruptibleDefault` → `bCancellableDefault`、`ECT_OnCastInterrupted` → `ECT_OnCastCanceled`）。**事件 tag `TcsEvent.Cast.Interrupted` 不改**（该 tag 大概率退役）；
- **载荷补字段**：`FTcsCastEventPayload` 新增 `Reason`（`ECastFinishReason`）——订阅者此前只能靠"哪枚事件"反推，无法区分 `Interrupted` 与 `Replaced`；
- **作者期校验两条**：① `A.TagsToCancel` 与 `B.TagsToCancel` 互相包含 ⇒ 双向强夺、无确定赢家 ⇒ Error；② `A.ForbiddenTags ∋ A`（自指）⇒ 永不可满足 ⇒ Error；
- **不开新 tag 根**：`CategoryTags` 的词归宿主 ini 声明（插件 MUST NOT 声明具体分类词）⇒ `gameplay-tag-governance` 根表 12 与 5 处数字副本**不变**，只闭合台账 `STAT-4`。

## Impact

- Affected specs: `state-def-asset` / `skill-def-asset` / `state-instance-lifecycle` / `skill-cast-runtime` / `skill-cast-phases` / `gameplay-tag-governance`
- Affected code:
  - 新增 `Source/TcsState/Public/State/TcsStateRelation.h` + `Private/State/TcsStateRelation.cpp`
  - 新增 `Source/TcsState/Private/State/TcsStateOps_Relation.cpp`
  - 新增 `Source/TcsSkill/Private/Skill/TcsSkillOps_Relation.cpp`
  - 改 `Source/TcsBuff/Public/Def/TcsBuffDef.h`、`Source/TcsSkill/Public/Def/TcsSkillDefData.h`（四字段改名 + 新增）
  - 改 `Source/TcsBuff/Private/TcsBuffSubsystem.cpp`（ApplyState / RemoveState 挂点）
  - 改 `Source/TcsSkill/Private/...`（门禁 + 改名族 17 处 `.cpp` / 6 处 `.h`）
  - 改 `Source/TcsSkill/Public/Skill/TcsCastEvents.h`（载荷 `Reason`）
  - 改 `Config/DefaultGameplayTags.ini`（2 行过期 `DevComment`）
- **资产影响面（实测）**：`bInterruptible` 命中 **2 个**资产（`DA_SkillDef_E2E_Replaceable` / `DA_SkillDef_E2E_SinglePhase`）须重存；`Blocks` / `Requires` / `Cancels` **0 个**资产命中（零迁移成本）

---

## ★★ 归档时的追注（2026-10-17）：本变更的**契约部分将被推翻**

> **用户 2026-10-17 的设计裁定**（MUST 读，防后人按本提案的契约形状继续建）：
>
> 1. **`Buff` 与 `Skill` 在关系表眼里都只是"状态"**（两者同继承 `FTcsStateInstance`）
>    ⇒ **TagRelationship 的管理归 `TcsState` 模块**，`TcsBuff` 与 `TcsSkill` 都受其影响；
> 2. **统一读口不暴露域**——判定对象 = 该实体上**在跑的全部运行态**；
> 3. **`DefTag` 与 `CategoryTags` 都进匹配面**——**本项取代** `e0` 的 D3
>    （`state-def-asset` 规格原文「两个查询通道各自独立、MUST NOT 混为一个并集匹配」）；
> 4. **"彼此打断"是硬性要求**（用户原话：「现在既然跑不通，那就确实是要补的缺口」）。

**本提案中将被推翻的部分**：

| 件 | 处置 |
|---|---|
| `ITcsStateLiveQuery`（单向、只读状态域、单槽位） | ❌ **将被统一读口取代** |
| `FTcsLiveStateView`（视图形状） | ⚠️ 形状可复用，但来源改为"合并两域" |
| 技能侧 `CheckRelationGate` 的客体词面来源 | ⚠️ 由"只读状态"改为"读全部运行态" |

**继续有效的部分**（MUST NOT 一并推翻）：

- `FTcsRelationRow` 与 `FTcsStateRelation` 检查器（**判定逻辑一字不改**）；
- 两个 Def 的四个关系字段（已上移基类）；
- 改名族（`Interrupted` → `Canceled`）、载荷 `Reason`、两条作者期 Error；
- `PropertyRedirects` 配置（同型改名的正道）。

**证据**：`Documents/combat-system-design/evidence/2026-10-17-relation-checker.md` §8。

