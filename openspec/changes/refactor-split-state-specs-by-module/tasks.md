# Tasks — 按模块重切 `state-*` 规格族

> **执行方式已变更（2026-10-24）**：原计划走 `openspec archive`，实测遇**工具解析限制**（见 proposal「执行记录」），
> 按用户裁定改为**手工改名 + 保留 change 记录**。第一刀已**手工落地**；第二三刀未开工。
> **本 change 不执行 `openspec archive`**——由用户手工删除。

## 0. 前置（已完成）

- [x] 0.1 实测 `openspec` 1.13.2 **无 `rename` 命令**；能力改名的工具路径 = `REMOVED` 全需求 + `ADDED` 新能力 + `.openspec.yaml` 的 `retire_capabilities: true`
- [x] 0.2 实测 `schema:` 必须写 `spec-driven`（写 `change` 会报 `unknown schema`）
- [x] 0.3 实测 `archive/` 里 30 个旧 delta 目录**不影响** `validate --all`
- [x] 0.4 实测归档后新能力 `## Purpose` 变成 `TBD - created by archiving change ...`，**必须手工补**（本仓 5 次实证）
- [x] 0.5 按**需求级主语**逐条复核 10 份 `state-*`：①5 份全 Buff / ②1 份宿主装置 / ③2 份混合 / ④2 份真共享层
- [x] 0.6 定位三类「不可随改名的引用」：`EVID-<date>-state-*` 证据文档路径、`Documents/combat-system-design/` 冻结层文件名、`openspec/changes/archive/` 历史 delta 目录名
- [x] 0.7 **实测归档器限制**：需求正文内含**表格**的能力无法被 `retire_capabilities` 退休——逐份单独测，**4 份通过 / 2 份失败**（`state-instance-lifecycle` 六枚事件 tag 表、`state-stacking-policies` 四档回执表）

## 1. 第一刀：改名 6 份能力 —— ✅ **已手工落地**

### 1.1 delta（提案记录，供审阅；实际由手工执行）
- [x] 1.1.1 `specs/buff-instance-lifecycle/spec.md`（ADDED 7 条）
- [x] 1.1.2 `specs/buff-stacking-policies/spec.md`（ADDED 6 条）
- [x] 1.1.3 `specs/buff-modifier-materialization/spec.md`（ADDED 7 条）
- [x] 1.1.4 `specs/buff-behavior-fragments/spec.md`（ADDED 4 条）
- [x] 1.1.5 `specs/buff-step-library/spec.md`（ADDED 1 条）
- [x] 1.1.6 `specs/buff-layer-e2e-validation/spec.md`（ADDED 4 条）
- [x] 1.1.7 六份 REMOVED（合计 29 条需求标题逐条列出）
- [x] 1.1.8 三处外部 MODIFIED：`entity-runtime-mechanism` / `integration-entity` / `state-def-asset`
- [x] 1.1.9 `.openspec.yaml`（`schema: spec-driven` + `retire_capabilities: true`）

### 1.2 实际执行与验证
- [x] 1.2.1 备份 `openspec/specs/` → `_tcs_specs_backup`（45 个能力全量）
- [x] 1.2.2 六个目录改名 `state-*` → `buff-*`
- [x] 1.2.3 六个标题行改名（`# state-X Specification` → `# buff-X Specification`）
- [x] 1.2.4 正文能力名互引替换（6 份内部 + 3 份外部，共 9 个文件）
- [x] 1.2.5 `EVID-<date>-state-*` 证据文档引用**全部保留**（3 处：`buff-behavior-fragments` / `entity-runtime-mechanism` / `skill-cast-runtime`）
- [x] 1.2.6 `openspec validate refactor-split-state-specs-by-module --strict --no-interactive` → valid
- [x] 1.2.7 `openspec validate --all --strict --no-interactive` → **46 passed / 0 failed**
- [x] 1.2.8 **逐字节往返比对**：把新文件的能力名换回旧名，与备份**完全一致** ⇒ 证明只改了能力名、正文一字未动（6/6 通过）
- [x] 1.2.9 行数与行尾基线比对：348/225/161/111/37/117 **全对**，LF、均以新行结尾
- [x] 1.2.10 活跃规格内旧名残留反查 → **零**（`EVID-` 排除后）
- [x] 1.2.11 能力总数核对 → **45**（改名不增不减）
- [x] 1.2.12 `Documents/combat-system-design/` 零改动确认

## 2. 第二刀：拆 2 份混合篇 —— ✅ **已手工落地（2026-10-24）**

### 2.1 拆分结果

| 原能力 | 拆成 | 需求分布 |
|---|---|---|
| `state-def-asset`（6 需求 / 197 行） | `state-def-asset`（3 需求 / 89 行）+ `buff-def-asset`（5 需求 / 154 行） | 共享：数据形状基底 / 描述配置载体 / Def 资产基类<br>Buff：Buff Def 数据形状 / Buff 资产身份 / 表行 / 作者期校验 / 值约定白名单 |
| `state-param-snapshot`（6 需求 / 206 行） | `state-param-snapshot`（2 需求 / 58 行）+ `buff-param-snapshot`（5 需求 / 172 行） | 共享：快照类型 / 读取适配器<br>Buff：构建规则 / 等级源 / 时值周期 / 时长同步 / 撤销顺序 |

### 2.2 关键发现：**两份能力的旧名本来就是对的**

拆完 Buff 部分后，`state-def-asset` 只剩 `FTcsStateDefBase` / `FTcsDescriptionEntry` / `UTcsStateDef`，`state-param-snapshot` 只剩 `FTcsParamSnapshot` / `UTcsStateParamTableReader`——**全是 `TcsState` 的东西**。它们不是"名字谎报主语"，而是"名字对、内容混了一半"。⇒ 拆分即自动修正，**两份不需要改名**。

这与第一刀的 6 份形成对照：那 6 份是**名字错、内容也对**（全 Buff），所以只能改名；这 2 份是**名字对、内容混**，所以只能拆。

### 2.3 执行与验证

- [x] 2.3.1 备份 `openspec/specs/` → `_tcs_specs_backup2`（45 个能力全量）
- [x] 2.3.2 切分两条跨域需求：`状态 Def 数据形状`（从 `FTcsBuffDef` 行断开）、`参数快照与其构建规则`（从 `FTcsBuffInstance` 行断开）
- [x] 2.3.3 四份新 `## Purpose` 全部手写（**无一个 `TBD`**）
- [x] 2.3.4 **场景完备性核对**：原始 44 个场景（19 + 25）**全部保留**，仅新增 1 个（`资产只组合持有数据`）
- [x] 2.3.5 **场景标题不改名**：`族基类不含 buff 专属字段` 一度被改成 `…任何域字段`，已还原（openspec MODIFIED 语义下 Scenario 标题不可改名）
- [x] 2.3.6 共享层两条需求各补场景（`state-def-asset` 补 3 / `state-param-snapshot` 补 2）——否则 `validate` 报「Requirement must have at least one scenario」
- [x] 2.3.7 共享层需求主语修正：`TcsBuff` MUST 提供 → `TcsState`（共享层）MUST 提供
- [x] 2.3.8 **6 处引用指向订正**：`buff-instance-lifecycle`（3 处：构建规则 → `buff-param-snapshot`）/ `buff-stacking-policies`（1 处）/ `buff-layer-e2e-validation`（1 处：六能力清单）/ `integration-entity`（1 处：资产校验 → `buff-def-asset`）
- [x] 2.3.9 保留不动的引用（判定为**正确**）：`state-relation-check:10`（四数组住 `FTcsStateDefBase`，归共享层）/ `skill-def-asset`（关系四字段形状归 `state-def-asset`）
- [x] 2.3.10 `openspec validate --all --strict --no-interactive` → **48 passed / 0 failed**
- [x] 2.3.11 四份文件完整性：标题 / Purpose 无 TBD / 需求>0 / 场景>0 / LF / 末尾换行 / 零 U+FFFD **全通过**
- [x] 2.3.12 能力数：45 → **47**（+2）
- [ ] 2.3.13 编译验证（本变更零代码，见第 4 节）

## 3. 第三刀：补共享层契约 —— ✅ **已落地（2026-10-24），但形态与计划不同**

### 3.0 ★ 计划变更：**不创建 `tcs-shared-layer` 能力**（用户 2026-10-24 裁定）

原计划新建第 46 个能力。**核实后两条缺口都是「内容已经在、位置错了」，不是「内容缺失」**：

| 缺口 | 原以为 | 实测 |
|---|---|---|
| 门面三方法 + 存在判据 | 规格零覆盖 | **内容全在** `buff-instance-lifecycle:140` 的「门面拆分」段里，只是写在 Buff 篇 |
| 基底 `FTcsStateInstance` 6 字段 | 无契约 | **契约全在** `TcsStateInstance.h` 头注释（含六个字段的逐条判据 + 三处邻词陷阱），规格里只有一行字段名 |

⇒ 第三刀是**搬家**（把契约挪到主语正确的能力里），不是补写。第二刀之后共享层已有 `state-def-asset`（3 需求）与 `state-param-snapshot`（2 需求）两份能力在承载，再建一个会与 `entity-runtime-mechanism` 的 Purpose（自称"定义共享层共用的运行态机制"）重叠。

### 3.1 实际落地方式：两条新需求并入 `entity-runtime-mechanism`

该能力的 Purpose 本就自称「定义 `TcsState`（共享层）…共用的运行态机制」，门面与基底**都属机制面**，故并入而非新建：

- [x] 3.1.1 **需求「共享层门面与其存在判据」**（新增 3 场景）：三方法清单 + 判据原文 + **两次失效史**（判据本身曾写错 / 「唯一共用设施」的排他断言在 `f626582` 后失效）
- [x] 3.1.2 **需求「共享层运行态基底 `FTcsStateInstance`」**（新增 3 场景）：6 字段 + 逐字段判据表 + 不进基底的域内字段清单 + **三处邻词陷阱**（`CascadeAnchor` ≠ `Source` / `ExpiryEntry` ≠ `PhaseIndex`）
- [x] 3.1.3 订正该能力 Purpose 里「**本能力不对其字段作规定**」这句——基底搬入后不再成立，已就地改述并留痕
- [x] 3.1.4 `buff-instance-lifecycle` 两处原文加**归属指针**（保留为对照说明，契约以 `entity-runtime-mechanism` 为准）

### 3.2 代码事实核实（新写需求必须逐条对得上代码）

| 核实项 | 结果 |
|---|---|
| `UTcsStateSubsystem` 公开方法 | **恰为 3 个**：`GetParamTableReader` / `GetLiveQuery` / `SetLiveQuery` ✅ |
| 存在判据在头注释 | `TcsStateSubsystem.h` 含「本类的存在判据」✅ |
| `FTcsStateInstance` 字段 | **恰为 6 个**：`DefTag` / `Entity` / `CascadeAnchor` / `ParamSnapshot` / `Level` / `ExpiryEntry` ✅ |
| `FTcsBuffInstance` 派生 | 8 字段全为域内，**零重复**基底字段 ✅ |
| `FTcsSkillInstance` 派生 | `EntryHandle` / `PhaseIndex` / `ChainRunHandle` / `ActiveChainId` / `MainChainStart` / `MainChainStartPhaseTag`，**零重复** ✅ |

### 3.3 验证

- [x] 3.3.1 `openspec validate --all --strict --no-interactive` → **48 passed / 0 failed**
- [x] 3.3.2 `entity-runtime-mechanism` 需求数 6 → **8**，行数 195 → **261**，场景 22 → **28**
- [x] 3.3.3 能力总数 **47**（不新建，故不变）
- [x] 3.3.4 **顺带修复 3 份既有 CRLF 文件**（见下）

### 3.4 顺带发现并修复：3 份规格文件是 CRLF

全量扫描 47 份发现 3 份行尾为 CRLF，违反项目约定（`project.md`「文件为 UTF-8 无 BOM、LF 行尾」）与 `.gitattributes` 的 `*.md text eol=lf`：

| 文件 | CRLF 行数 |
|---|---|
| `gameplay-tag-governance` | 255 |
| `skill-def-asset` | 190 |
| `state-relation-check` | 117 |

**这三份本轮从未被写过**（`git status` 起初无它们），属**既有问题**。之所以 `git status` 也看不到，是因为 `.gitattributes` 的 `eol=lf` 会在 `git add` 时归一化 ⇒ **工作区 CRLF 对 git 不可见**。已统一转 LF，47 份现全部 LF。

## 4. 收束 —— ⬜ **待第二三刀完成后**

- [ ] 4.1 `Documents/tcs-arch/05-state.md` §1.2 分布表就地改述为新的按模块归属
- [ ] 4.2 `Documents/tcs-arch/05-state.md` §5 D-1 标记闭合（附提交 hash）
- [ ] 4.3 `Documents/tcs-arch/INDEX.md` §8 修复表加 R-17 行
- [ ] 4.4 编译验证（本变更零代码，应无变化）

---

## 备注：本 change 的处置

**不执行 `openspec archive`**——工具限制使其无法退休含表格的能力。归档器不会记录本变更。
第一刀的落地事实**已写进 `openspec/specs/` 本身**（目录名、标题、正文互引均为最终态），
本 change 目录仅作**提案与执行记录**，由用户手工删除。
