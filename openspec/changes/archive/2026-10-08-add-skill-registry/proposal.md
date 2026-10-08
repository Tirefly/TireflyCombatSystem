# Change: 落地已学技能账本（`TcsSkill` 门面 + 定义登记口 + 账本参数读取面 + 实体可操作门禁）

> 对应计划 `PLN-R6` Task 2（`Documents/combat-system-design/plans/plan-r6-skill-layer.md`）。
> **提案名与能力名的订正**：计划原稿的提案行写 `` `effect-trigger` 或 `host-entity-query` MODIFIED ``——
> 前者与实体查询无关、后者**全仓零命中**（承载该方法的实名是 **`entity-query-contract`**），且"或"是一个未关掉的二选一 hedge。
> 账本行为面在既有 35 个能力里**无承载**（唯一的技能侧能力 `skill-def-asset` 只管资产形状）⇒ 按 R5 先例新建 **ADDED `skill-registry`**。

## Why

Task 1 已交付 `TcsSkill` 模块与技能 Def 资产族（`UTcsSkillDef` / `FTcsSkillDefData`），但**技能还在"零运行期消费者"状态**：
定义只进 `UTcsDefinitionSubsystem` 的缓存，没有任何东西能回答"某个单位会哪些技能"。

Task 2 要交付那个答案，并且它同时是 Task 3（施法与六道门禁）与 Task 4（参数链）的前置。
起草前扫描抓到一处**与已归档规格的直接冲突**（见下 What Changes 第 1 条），必须同批改规格而不是绕过它。

## What Changes

1. **技能定义逐世界登记（改判既有规格，硬前置）**——账本的 `GetDef()` / `LevelBase` / `Params` / `BoolSwitches` 需要定义内容，
   但定义缓存住 `TcsIntegration`，而 `TcsSkill` **MUST NOT** 反向依赖它（成环）；既有规格 `integration-entity`
   还明文写着「技能定义**不做世界装配**……MUST NOT 在 `OnPostWorldInitialization` 里建任何每世界结构」。
   **该条自己的理由就写着消费者是"Task 2 起的账本与门禁"** ⇒ 前提已消失，按 `ATTR-1` 的同款触发机制改判为
   **「逐世界登记进技能门面」**（`UTcsSkillSubsystem::RegisterSkillDef`），照状态侧 `RegisterStateDef` 的既有先例逐字同构。
   - **MODIFIED `integration-entity`**：「定义库（GameInstance 级最小版）」需求整块改写该条 + 世界装配日志补技能定义计数 + 两条新 scenario。
2. **实体可操作性门禁（契约追加）**——**MODIFIED `entity-query-contract`**：给 `ITcsEntityQuery` 加第四支能力
   `IsEntityReady(FTcsCombatEntityHandle) -> bool`（**纯追加**，破坏面为零：全仓实现仅 1 个、`IsAlive` 调用点为 0）。
   设计 `06 §4` 的门禁第一道原词就是"实体状态 **Ready**"，其持有者属 R7 ⇒ R6 必须有替身，该方法与设计 1:1 对应。
   `IsAlive` **原封不动**（正交轴；`boundary-audit` 的不许删名单）。
3. **ADDED `skill-registry`**：per-unit 已学技能账本（条目 + 代际句柄 + 授予/撤销/按来源级联）、
   账本参数读取面（`GetLevel` / `GetNumericParam` / `IsSwitchSet` + `EffectiveLevel` 语义）、
   技能定义登记口（门面侧）、实体可操作门禁（技能侧消费面与降级语义）。
4. **计划侧四处订正（同批，见 `PLN-R6` Task 2 与本轮变更记录）**：交付物列表的"（+ Registry）"挂错名字、
   池与运行态误列本轮、提案行两个能力名、`FTcsLearnedSkillEntry` 反射性未声明。

## 明确不做（非目标）

- **不做激活**（`TryActivate` / 六道门禁序列 / `FTcsCastRun` 池与运行态）——归 Task 3；
  本轮只交 `FTcsCastRunHandle` **纯句柄值类型**（作为 `RunHandles` 的元素，**零写入者**），
  池 MUST NOT 在本轮建（其元素类型是 Task 3 的产物，而池实例化要求元素类型完整 ⇒ 提前建即硬编译错误）。
- **不做参数链修正器物化**（归 Task 4）；**不做冷却轨道状态**（归 `R6.5-a`）。
- **不做账本的脚本 / 蓝图读写面**（`FTcsLearnedSkillEntry` 本轮**非反射**；该需求已入台账 `SCRIPT-10`，触发条件型）。
- **不做关系字段检查器**（`Blocks` / `Requires` / `Priority` / `Cancels`，归 `R5.5-e`）。

## Impact

- **Affected specs**：
  - `skill-registry`（**ADDED** × 4 需求 / 12 scenario）
  - `entity-query-contract`（**MODIFIED** × 1 需求：三支 → 四支 + 1 新 scenario）
  - `integration-entity`（**MODIFIED** × 1 需求：「定义库（GameInstance 级最小版）」——改判 1 条 + 补 1 处日志口径 + 2 新 scenario）
- **Affected code**（插件仓 TCS）：
  - 新：`Source/TcsSkill/Public/Skill/TcsSkillRegistry.h`、`Skill/TcsSkillOps.h`、`Skill/TcsSkillEntryHandle.h`、`Skill/TcsCastRunHandle.h`、`Source/TcsSkill/Public/TcsSkillSubsystem.h` + `Private/…`（分片：`_Subsystem` / `_Definition` / `_Registry` / `_ParamRead` / `_EntityReady`；引擎函数 `Skill/TcsSkillOps.cpp` / `_ParamRead.cpp`）
  - 改：`Source/TcsEffect/Public/Host/TcsEntityQuery.h`（加方法）、`Source/TcsIntegration/Public/Entity/TcsPieEntityQuery.h` / `Private/…cpp`（实现）、`Source/TcsIntegration/Private/TcsDefinitionSubsystem.cpp`（`SeedWorld` 增技能定义装配）
- **分工遵循设计 §3.1（门面薄壳 + 引擎函数）**：`FTcsSkillRegistry` 只存数据与索引，授予 / 撤销 / 级联 / 解析 / 读取的流程逻辑全在 **`FTcsSkillOps`**（静态函数族）；门面同名方法为一行转发，经 `friend class FTcsSkillOps` 开放最小私有集——同 `UTcsStateSubsystem` / `FTcsStateOps` 的处置。
- **命名（实施期改判，用户 2026-10-08 裁定）**：本能力原拟名 `skill-ledger`，**改判 `skill-registry`**——容器形状与 `FTcsStateRegistry` 完全同形（per-unit 桶 + 槽位代际），按"同一形状同一名"统一；且英文 `ledger` 在本仓已专指**台账**（`LEDGER-*` 三份文档）与 **M2 属性账本**（`IsLedgerReady`）。**概念义不变**：设计语料里的「已学技能账本」（`SPEC-04-skill` §3.1 / `D5-2`）仍成立，只有类型与文件取 `Registry`。
- **不改**：`Source/TcsState/**`（零改动）；`TcsSkill.Build.cs` 依赖集（仍不含 `TcsDamage` / `TcsTargeting`，也不加 `TcsIntegration`——那会成环）；`FTcsSkillDefData`（Task 1 已取证收官，本轮不回改）。
- **跨仓**：宿主装置改动在 LAC 仓（`Source/TcsDev/`），随 Task 2 验收交付。
