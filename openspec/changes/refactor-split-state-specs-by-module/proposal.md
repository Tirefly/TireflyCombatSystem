# Change: 按模块重切 `state-*` 规格族（改名 6 份 + 拆 2 份 + 新建共享层能力）

## Why

`ARCH-05` 的 D-1（★★★）登记：`openspec` 里**没有一个能力回答「`TcsState` 这个模块今天还剩什么」**。根因是 `PLN-REFACTOR-1` Task 4 把 `TcsBuff` 拆出去时**只改了代码与注释，没有重切规格**——规格的文件名沿用 `state-*`，而主语已经搬进 `TcsBuff`。

按需求级主语实测（不是按目录名），10 份 `state-*` 能力的分布是：

| 类 | 能力 | 需求主语 | 要动什么 |
|---|---|---|---|
| ① 纯 Buff 篇 | `state-instance-lifecycle` / `state-stacking-policies` / `state-modifier-materialization` / `state-behavior-fragments` / `state-step-library` | **25/25 全 `TcsBuff`** | **只改名**，正文一字不改 |
| ② 宿主装置篇 | `state-layer-e2e-validation` | 4/4 宿主装置 | **只改名** |
| ③ 混合篇 | `state-def-asset` / `state-param-snapshot` | 各 4 Buff + 2 State | **必须拆**，改名救不了 |
| ④ 真共享层篇 | `state-relation-check`(5/5) / `entity-runtime-mechanism`(6/6) | 全 `TcsState` | 不动（`entity-runtime-mechanism` 补门面需求） |

**两个必须分开看的问题**：

1. **名字谎报主语**（①②）——改名才能修，补内容修不了。这正是本变更存在的主要理由。
2. **权威层缺覆盖**（③④ + 门面）——`UTcsStateSubsystem` 的 3 个门面方法**零需求规定**，而它的存在判据（「这个设施是否被两个域在代码级使用」）**在规格里零命中**，是全仓最健康的一处设计却没有契约。且基底 `FTcsStateInstance` 的 6 字段**住在 Buff 篇**里，而 `entity-runtime-mechanism` 明文声明「本能力不对其字段作规定」⇒ 共享层类型的字段契约住在域篇里，**权威层错位**。

## What Changes

### 第一刀：改名 6 份能力（零正文风险）

| 现名 | 新名 | 正文改动 |
|---|---|---|
| `state-instance-lifecycle` | `buff-instance-lifecycle` | 仅能力名互引 |
| `state-stacking-policies` | `buff-stacking-policies` | 仅能力名互引 |
| `state-modifier-materialization` | `buff-modifier-materialization` | 仅能力名互引 |
| `state-behavior-fragments` | `buff-behavior-fragments` | 仅能力名互引（`EVID-` 证据文档名**保留**） |
| `state-step-library` | `buff-step-library` | 仅能力名互引 |
| `state-layer-e2e-validation` | `buff-layer-e2e-validation` | 仅能力名互引 |

**改名机制**（openspec 1.13.2 **没有** `rename` 命令，本机实测）：`REMOVED` 旧能力**全部 29 条需求** + `ADDED` 新能力同 29 条 + change 目录的 `.openspec.yaml` 写 `schema: spec-driven` + `retire_capabilities: true`。归档时工具会退休旧规格并建新规格。

**⚠ 归档后必须手工补 `## Purpose`**：归档器只会写 `TBD - created by archiving change ...`。本仓历史上有 **5 次实证**（见 `archive/2026-10-05-add-state-behavior-fragments/tasks.md:41` 等）。

### 第二刀：拆 2 份混合篇

| 现能力 | 拆成 | 依据 |
|---|---|---|
| `state-def-asset`（6 需求） | `buff-def-asset`（4 条 Buff）+ 共享层 Def 形状（2 条：`UTcsStateDef` 基类 / 描述配置载体） | Purpose 自述「`FTcsStateDefBase` / `FTcsBuffDef` 两个数据 struct」 |
| `state-param-snapshot`（6 需求） | `buff-param-snapshot`（4 条 Buff）+ 共享层快照（2 条：`FTcsParamSnapshot` 类型 / `UTcsStateParamTableReader`） | Purpose 自述「数值与时间两条腿」 |

### 第三刀：新建 `tcs-shared-layer` 能力

主语明确是 `TcsState`，承接共享层本体契约：

| 需求 | 内容 |
|---|---|
| 共享层门面与存在判据 | `UTcsStateSubsystem` 只持**两件**共用设施（`GetParamTableReader` / `GetLiveQuery`+`SetLiveQuery`），判据 = 「这个设施是否被两个域在代码级使用」 |
| 共享层 Def 形状族 | `FTcsStateDefBase` 五组字段 + `UTcsStateDef` 基类（从第二刀的 `state-def-asset` 迁入） |
| 共享层参数快照 | `FTcsParamSnapshot` + `UTcsStateParamTableReader`（从第二刀的 `state-param-snapshot` 迁入） |
| 共享层运行态基底 | `FTcsStateInstance` 6 字段（**从 `buff-instance-lifecycle` 迁出**——它住共享层，却由 Buff 篇规定） |

## Impact

- **Affected specs**：改名 6（`state-*` → `buff-*`）、拆分 2（`state-def-asset` / `state-param-snapshot`）、新建 1（`tcs-shared-layer`）、外部 MODIFIED 3（`entity-runtime-mechanism` / `integration-entity` / `state-def-asset`）
- **Affected code**：**零**。本变更只动 `openspec/specs/`，不碰 `Source/`
- **不动的东西**：
  - `Documents/combat-system-design/`（冻结层）——含 8 个 `state-*` 证据文档与决策文档，其名是**历史现场**，MUST NOT 改
  - `openspec/changes/archive/` 里 30 个旧 delta 目录——实测**不影响** `validate --all`，且它们是历史记录
  - 正文里 `EVID-2026-10-05-state-behavior-fragments` 这类**证据文档路径引用**——指向冻结层真实文件，MUST NOT 随能力改名
- **风险**：改名后旧名在活跃规格中消失，外部若有人按旧名查会 miss。缓解 = 本 proposal 的对照表 + `ARCH-05` §1.2 的就地改述

## 与 `ARCH-05` D-1 的关系

本变更**完整闭合 D-1**（原建议处置：「新建一个 `tcs-shared-layer` 能力（或把 7 份里的共用部分抽出来），主语明确是 `TcsState`。**这不是本档案能做的决定**，需要用户裁」）。用户 2026-10-17 裁定：**三刀都做**，且 `archive/` 一个字不动，走 change 流程（change id = `refactor-split-state-specs-by-module`）。


---

## 执行记录（2026-10-24，第一刀已落地）

### 为什么手工改名而不走 `openspec archive`

本机实测（`openspec` 1.13.2）：归档器的 retirement 检查在「全需求 REMOVED」之后还会检查**文件里是否还有它无法归并的内容**。两份能力的**需求正文内含表格**，被它判为"无法安全归并"：

| 能力 | 触发的需求 | 表格 |
|---|---|---|
| `state-instance-lifecycle` | 「状态生命周期事件」 | 六枚事件 tag 表（常量 / 词 / 广播时机） |
| `state-stacking-policies` | 「共存决策与四档回执」 | 四档回执表（组内 / 来源 / 层数 / 回执 / 事件） |

逐份单独实测：**4 份通过，2 份失败**。失败报错原文：

> `'state-instance-lifecycle' declares retire_capabilities, but the spec holds content the merge cannot safely account for and deleting the file would take with it: "| 常量 | 词 | 广播时机 |", "|---|---|---|", ... and 8 more line(s).`

**这是工具解析限制，不是 delta 写错**——需求标题逐条比对过 `规格 7 = REMOVED 7 = ADDED 7`，一字不差。

**用户裁定（2026-10-24）**：选「手工改名 + 保留 change 记录」，**不执行 `openspec archive`**。理由：表格是规格主体，为迁就工具解析器改写它 = 让地图迁就测绘工具。

### 实际执行方式

1. 目录改名 `state-*` → `buff-*`（6 个）
2. 标题行 `# state-X Specification` → `# buff-X Specification`
3. 正文内能力名互引替换（含 3 个外部文件：`entity-runtime-mechanism` / `integration-entity` / `state-def-asset`）
4. **`EVID-<date>-state-*` 证据文档引用一律保留**（指向 `Documents/combat-system-design/evidence/` 下的真实文件，属冻结层，MUST NOT 随能力改名）
5. `openspec/changes/archive/` 里 30 个旧 delta 目录**一个字不动**（历史现场，实测不影响 `validate --all`）

### 验证

| 项 | 结果 |
|---|---|
| `openspec validate --all --strict --no-interactive` | **46 passed / 0 failed**（45 规格 + 1 change） |
| 能力总数 | **45**（改名不增不减） |
| 6 份**逐字节往返比对** | **完全一致**——证明只改了能力名，正文一字未动 |
| 文件行数 | 6 份全部与基线相同（348/225/161/111/37/117） |
| 行尾 / 末尾换行 | LF / 均以新行结尾，与基线一致 |
| 活跃规格内旧名残留 | **零**（`EVID-` 前缀排除后） |
| `Documents/combat-system-design/` | **零改动**（冻结层） |
| 编译 | 本变更零代码，不影响编译 |
