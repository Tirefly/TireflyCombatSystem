# EVID-2026-10-16-merge-status-tag — `e0` 类别容器统一（`StatusTag` 并入 `CategoryTags`）取证

- **文档 ID**：`EVID-2026-10-16-merge-status-tag`
- **类型**：EVID / 验收证据
- **权威范围**：变更 `merge-status-tag-into-category-tags`（即 `R5.5-e0`）的**代码 / 资产 / 编译 / PIE 四处读数**，含本轮抓出的**一处真实遗漏**与**两条 MCP 错路**。**不在本文**：字段合并的**设计判据**（见提案 `design.md`）、关系表检查器本体（归 `R5.5-e1`）。
- **最后更新**：2026-10-16

> **轮次日 / 机器日错位留痕（照 `MEM-20261004-12` 先例）**：本变更的**提案与设计文档按轮次日写 `2026-10-15`**（建立当日），而**证据档与归档目录按机器日取 `2026-10-16`**（PIE 取证时已跨零点）。**两者错位属实、不改名**。

---

## 1. 本轮改的是什么（一句话 + 判据）

`FTcsStateDefBase` 原持**单个** tag 字段 `StatusTag`；本变更**删除它**并代之以 `FGameplayTagContainer CategoryTags`。

**判据三条**（详见提案 `design.md` §D1）：① 容器 ⊇ 单值（元数冗余）；② 外部先例同向（GAS 的 `UGameplayEffect::InheritableGameplayEffectTags` 就是容器，`GameplayEffect.h:2346`）；③ `DEC-2026-10-13-relation-row-shape` §4.5 的否定判据（"`StatusTag` 单义 ⇒ 技能也有它读不通"）**已随 `PLN-REFACTOR-1` 瓦解**（技能运行态与技能 Def 现在都与状态侧共用同一基/基底）。

**同批改名**：`FTcsSkillEntrySelector::CategoryTags`（**查询**）→ `ByCategoryTags`，消除与定义侧**容器**的同名不同型陷阱。

---

## 2. 代码面（改动清单）

### 2.1 插件仓（TCS，6 文件 / +58 −17）

| 文件 | 改动 |
|---|---|
| `Source/TcsState/Public/Def/TcsStateDefBase.h` | 字段替换 + 44 行判据注释 + 字段沿革段 |
| `Source/TcsIntegration/Private/TcsDefinitionSubsystem_State.cpp` | 发现期校验**四条 → 三条**（删 `Def.StatusTag` 非空；留删除理由） |
| `Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` | 头注释"四条是身份级"同步 |
| `Source/TcsBuff/Private/State/TcsBuffOps_Snapshot.cpp` | 日志读数改 `CategoryTags`（"状态"→"类别"） |
| `Source/TcsSkill/Public/Skill/TcsSkillEntrySelector.h` | `CategoryTags` → `ByCategoryTags` + 11 行判据 |
| `Source/TcsSkill/Private/Skill/TcsParamChain.cpp` | 调用点注释同步（阻塞三条减为两条） |

### 2.2 宿主仓（LAC，9 文件 / **29 处**）

- **20 处赋值**：`X.StatusTag = Fn(...)` → `X.CategoryTags = FGameplayTagContainer(Fn(...))`
- **6 处读侧**：检查 18 判据由 **`== 相等` 改为 `HasTag 包含`**（实质改进——容器语义下"等于该词"会退化成"容器里只能有它一个"）+ 读数文案 + 函数参数名
- **3 处注释**：`TcsDevTags.h` / D9 探针

### 2.3 ★ 本轮抓出的一处真实遗漏（首轮编译暴露，已修）

**症状**：

```
TcsDevSkillParamChainProbe.cpp(829,14): error C2039: 'CategoryTags': is not a member of 'FTcsSkillEntrySelector'
    Selector.CategoryTags = FGameplayTagQuery::MakeQuery_MatchAnyTags(...)
```

**根因（如实登记）**：我的批量替换正则只匹配 `<Var>.StatusTag = (SkillCastProbeTag|SkillLayerTag)(...)` 两种形态；而该处是**选择器侧赋值**——左值是选择器字段、右值是 `FGameplayTagQuery` 工厂，两个特征都不在正则里。随后的"残留扫描"用的是 `grep StatusTag`（该文件此时已无 `StatusTag` 字样）⇒ **扫不到**，成盲区。

**判据（本轮确立）**：**改名字族的扫描 MUST 按「被改的名字」逐名核，且两侧（定义侧 / 消费者侧）各自扫**——只扫单侧必留盲区。承 `PLN-REFACTOR-1` Task 5 立过的同款判据（"改名族的登记 MUST 按被改的名字逐名核，MUST NOT 按本 Task 动了哪些文件核"）。

**修正**：改为 `Selector.ByCategoryTags`，并补 5 行判据注释。

---

## 3. 资产迁移（6 个，类型变更，非改名）

### 3.1 为什么不能走 `PropertyRedirects`

`StatusTag`（`FGameplayTag`）→ `CategoryTags`（`FGameplayTagContainer`）是**类型变更**。引擎的 `PropertyRedirects`（`BaseEngine.ini:437` 有同型改名先例）**只做同型改名、不做类型转换** ⇒ 不适用。只能"读旧值 → 写新字段 → 保存"。

### 3.2 ★ 一条反直觉的实测事实（MUST 读）

**旧值不会被自动继承，且在新 schema 下已不可读**。

首次探查 `DA_Check_BuffDef` 的实际值：

```json
categoryTags: {"gameplayTags":[]}      // 空容器
```

而 `list_properties` 返回的 schema 中**只有 `categoryTags`、没有 `statusTag`** ⇒ 按"读旧值→搬新值"的通用写法会**搬到一个空值**。

**正确做法 = 按迁移前对 `.uasset` 的二进制实测值逐个硬编码**（下表）：
```
grep -a -o -E 'StateDef\.[A-Za-z\.]+|SkillDef\.[A-Za-z\.]+' <asset>.uasset
```

| 资产 | 迁移前实测词 | 迁移后回读 | 磁盘 |
|---|---|---|---|
| `DA_Check_BuffDef` | `StateDef.Check.Burn` | ✅ | `StatusTag=0 CategoryTags=1` |
| `DA_State_E2E` | `StateDef.Check.StateLayer` | ✅ | 同上 |
| `DA_SkillDef_E2E` | `SkillDef.Check.SkillLayer` | ✅ | 同上 |
| `DA_SkillDef_E2E_Second` | `SkillDef.Check.SecondSkill` | ✅ | 同上 |
| `DA_SkillDef_E2E_Replaceable` | `SkillDef.Check.SkillLayerReplaceable` | ✅ | 同上 |
| `DA_SkillDef_E2E_SinglePhase` | `SkillDef.Check.SkillLayerSinglePhase` | ✅ | 同上 |

**执行通道**：编辑器内 MCP（`ObjectTools.set_properties` 写 → `ObjectTools.get_properties` 逐个回读 → `AssetTools.save_assets` 落盘）。写容器形态 = `{"gameplayTags":[{"tagName":"X"}]}`，返回 `returnValue: true`。

**磁盘证据**：6 个 `.uasset` 的 `StatusTag` 命中数 **0**、`CategoryTags` **1**；`git status` 六项全 `M`。

---

## 4. 编译（双配置）

| 目标 | 配置 | 结果 | 日志 |
|---|---|---|---|
| `LegendAutoChessEditor` | Win64 **Development** | ✅ `Result: Succeeded` | `Saved_UBT/ubt-e0-editor-dev.log` |
| `LegendAutoChess` | Win64 **Shipping** | ✅ `Result: Succeeded` | `Saved_UBT/ubt-e0-game-shipping.log` |

**`warning C` = 0 / 0、`error C` = 0 / 0**（两日志分别机械统计）。

**首轮 `Failed` 的成因见 §2.3**（一处真实遗漏，非环境问题）。

---

## 5. PIE 验收（硬门禁，四轮全绿）

**路径**：编辑器内 MCP 驱动（`EditorAppToolset.StartPIE` → `SlateInspectorToolset` 写底栏 `tb3` → `LogsToolset.GetLogEntries` 收读数 → `StopPIE`）。脚本 = `Saved/TcsAcceptance/E0/e0_full.ps1`。

**读数**（日志**累积**，下表按前缀差切分为各轮增量）：

| 轮次 | 命令 | PASS / FAIL | 基线 | 判定 |
|---|---|---|---|---|
| R1 | `Tcs.Test.State.Run` | **15 / 0** | 15/0 | ✅ 一致 |
| R2 | `Tcs.Test.State.Reject` | **7 / 0** | 7/0 | ✅ 一致 |
| R3 | `Tcs.Test.Slice.Run` | **73 / 0** | 65/0 | ✅ **不退化**（检查号 0–23 全覆盖、无缺号） |
| R4 | `Tcs.Test.State.Run`（重跑） | **15 / 0** | 15/0 | ✅ 一致 |

**红字**：`ensure` / `LogTcs*: Error` / `LogTcs*: Warning` 在 R1 / R3 / R4 **全部 0 / 0 / 0**。

### 5.1 ★ 本变更的核心验收点：检查 18 新判据生效

```
[PASS] 检查 18：状态定义资产发现与作者侧门
       （按 DefTag 解析=成功；CategoryTags 含定义词=是；
        IsDataValid == Valid 2/2 个；失败清单状态定义条目 0 条）
```

**判据由 `StatusTag 一致` 改为 `CategoryTags 含定义词`**，且 **2/2 个资产**通过——证迁移后的容器值真的被解析侧读到。

> **⚠ 基线变更留痕（MUST 读）**：该读数文案已变，**旧基线摘要行不再逐字可比**。这属**预期内的基线变更**（判据本身改了），**MUST NOT** 计为回归。**新基线 = 上述文案**。

### 5.2 复现性（本仓纪律：**摘要行逐字相同**，非原始读数）

```
[复现性摘要]|S4 Armor0=5.000 Attack0=30.000 Slots0=0|S5 States=0/1 Decoupled=1 Lv=1|
S6 Rows=2->3|S7 N=3 Mod=25.000 Ovr=10.000 Lvl=30.000|S8 Armor=30.000 d=25.000 Slots=1|
S9 Attack=25.000 d=-5.000|S11 Live=1 PerOK=1 App=1|S12 Exp=1 Rem=0 Rows=2 Armor=5.000 ...
```

R1 与 R4 **逐字相同** ✅

### 5.3 一处差异，已核实为**非回归**

逐条比对两轮 `State.Run`，**唯一差异**在 S5：

| 轮次 | 读数 |
|---|---|
| R1 | `Source.Id=3 CascadeAnchor.Id=4` |
| R4 | `Source.Id=10 CascadeAnchor.Id=11` |

**判据**：这是**进程级发号器的计数**（`TcsSourceHandle.h:11` "进程内唯一 Id"），历史证据已明文记载同一现象——

> `EVID-2026-10-05-state-layer-pie` § 读数表：`S5 … Source.Id=3 CascadeAnchor.Id=4 互异=是；Level=1 期望 1=LevelBase`（**id 数值随进程前进，判据是互异而非定值**）

且两轮的**互异性判据（`Decoupled=1`）与复现性摘要行均相同** ⇒ **MUST NOT 计为回归**。

---

## 6. 边界（如实，MUST NOT 读成"全绿"）

1. **无头路线未跑**——本变更的验收全在 PIE（内容资产前置状态所限，同 `EVID-2026-10-13-cost-policy-pie` §1 的 ②）。**判据**：本变更改的是 Def 字段与装置判据，其消费者是内容资产；无头下内容发现为 0 条 ⇒ 跑了也证不了本变更。
2. **资产迁移是机械搬运，非等值证明**——6 个词是**逐个人工核对**后硬编码的（§3.2）；若将来新增第 7 个资产，**同一迁移手法 MUST 重跑一遍**（不可假定自动继承）。
3. **`bInterruptible*` / `ECFR_Interrupted` 一族改名未做**——按裁定归 `e1`（见 `design.md` §D6）。本轮资产迁移**刻意不改名**（避免同一批资产两次重存）。
4. **事件 tag `TcsEvent.Cast.Interrupted` 未改**（裁定 F：宿主项目该 tag 之后大概率退役）。
5. **不开新根**——`CategoryTags` 本轮**仍零解析消费者**（检查器归 `e1`、选择器真实现归 `e2`）⇒ 按「零消费者不预建根」，`gameplay-tag-governance` 根表 12 根与 `## Purpose` 数字**一字未动**；定根顺延至 `e1`。
6. **`CategoryTags` 的 tag 根归属仍未裁**（台账 `STAT-4` / `STAT-10` 合并后的那一条）。

---

## 7. 两条被排除的 MCP 错路（供后人省时）

| # | 错路 | 症状 | 正解 |
|---|---|---|---|
| **①** | **每次新建 MCP 会话调 Slate 工具** | `Windows` 返 `[]`、`Snapshot` 返空串 | **Slate 观察者状态是会话级的** ⇒ MUST **同一会话内连续调用**（先 `Observe(w1)` 再 `Snapshot`，立刻拿到完整窗口树） |
| **②** | **直接 `FillForm` 写底栏** | `returnValue: false` | `tb3` 初始未布局（`pos=3988,2000` 越出窗口高 1392）⇒ MUST **先 `Click(tb3)`** 使其获焦/布局，再 `FillForm` |

**另有两条承历史 `task7_pie.ps1` 的教训**（本轮沿用）：**绝不按 Escape**（PIE 视口持焦点时会停掉 PIE）；**每轮命令独占一个 PIE 会话**。

---

## 8. 证据锚点

| 面 | 路径 |
|---|---|
| 提案 | `openspec/changes/merge-status-tag-into-category-tags/`（proposal / design / tasks + 5 delta） |
| 编译日志 | `Saved_UBT/ubt-e0-editor-dev.log` / `ubt-e0-game-shipping.log` |
| PIE 脚本 | `Saved/TcsAcceptance/E0/e0_full.ps1` |
| PIE 日志 | `Saved/TcsAcceptance/E0/r1-state-run.log` / `r2-state-reject.log` / `r3-slice-run.log` / `r4-state-run-repeat.log` |
| 迁移脚本 | `Saved/McpProbe/migrate_status_to_category.py`（记录实测 API 事实） |
