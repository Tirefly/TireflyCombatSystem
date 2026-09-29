# TCS 战斗系统设计工作区 · 入口索引

- **文档 ID**：`INDEX`
- **类型**：治理（入口索引）
- **状态**：生效中（2026-09-29 建立，取代原 `README.md` 的"文档清单"职能）
- **权威范围**：本文回答"**该读哪一篇**"。编号含义见 `GLOSSARY.md`；写法纪律见 `docs-convention.md`；逐条拍板流水见 `LOG-DECISIONS`。
- **最后更新**：2026-09-29

---

## 1. 现状一句话

TCS 战斗系统设计已完成 **M0–M9 全部决策拍板**，实施进行到 **R4 轮**（触发行与伤害修改器通道）：

- **R1/R2/R3 已完成**（核心 + 属性 + 六模块竖切，7 项人工检查点全通过）；
- **R4 进行中**（触发行数据形状与登记表已完成 2/7 个 Task）；
- **R5–R8 未开工**（状态层 → 技能层 → 集成层 → 表现与编辑器工具）。

> **最容易误判的一条**：原 `README.md` 里 plan3 的状态写着"待执行 / 5 Task"，**那是过期信息**（README 已于 2026-09-29 拆分为导航页，内容迁入 `log/`）——plan3 的 Task 1/2 已完成，且 R4 已扩容为 7 个 Task。以本文 §3 的轮次表为准。



## 2. 我该读哪一篇？（按目的选）

| 你的目的 | 读这个 | 说明 |
|---|---|---|
| **了解系统全貌** | `SPEC-00-core`～`SPEC-10-presentation`（11 篇模块规格） | 一篇一个模块，统一模板：模块边界 / 类型词汇 / 入口服务 / 网络姿态 / 非目标 / 依据 / 验收钩子。**注意编号 ≠ 文件序**：`SPEC-09` 主题词是 `editor`（文件 `spec/08-…`） |
| **只想知道某个模块对外是什么** | 该模块 `SPEC` 的 §1 模块边界 + §2 类型词汇 | — |
| **想知道"当初为什么这么定"** | `GLOSSARY.md` 查到决策编号 → 按 §4 的落点进对应 `LOG-xx` / `DEC-xx` | 决策理由只住决策文档，不住规格 |
| **要知道哪些事还没做** | `LEDGER-deferred`（跨轮遗留台账，37 条） | 唯一待办登记册；按轮次分组 |
| **要知道某能力现在到底通没通** | `SPEC-TRACE`（契约追踪矩阵）+ `EVID-*`（证据） | 矩阵区分"静态实现 / glue / PIE 已验证 / 未验证" |
| **要接着干活（实施）** | `PLN-R4`（当前的实施计划，含 R4–R8 路线图） | 已完成轮次的计划：`PLN-R3-1`、`PLN-R3-2` |
| **写涉及"反射"字样的文本** | `LEDGER-terminology`（**MUST 先通读**） | 该词在本仓承载 5 种含义，有唯一处方措辞 |
| **要查某个缩写/编号** | `GLOSSARY.md` | 15 个编号族 + 逐条注册表 |
| **要新增或改写文档** | `CONVENTION` | 类型/状态词/身份块/引用格式 |
| **想了解本轮重构做了什么** | 勘察报告：`docs/2026-09-29-tcs-docs-recon.md`（仓库根 `docs/` 目录） | 45 篇文档的结构与关系体检 |

### 2.1 目录结构（找文件用）

```
combat-system-design/
├── INDEX.md            ← 本文：入口（该读哪一篇 + 现状 + 三层真相源）
├── GLOSSARY.md         ← 术语与编号权威表 + 文档 ID → 路径映射
├── docs-convention.md  ← 文档规范（类型/状态词/引用格式/写作纪律）
├── README.md           ← 导航页 + 2026-09-02 历史前言（不再是入口）
├── spec/               ← 模块规格 SPEC-00-core ~ SPEC-10-presentation + SPEC-TRACE（12 篇）
│                          ⚠ 编号与文件序不同轨：SPEC-06-targeting→`10-…`、SPEC-07-notation→`11-…`、
│                            SPEC-08-damage→`09-…`、SPEC-09-editor→`08-…`、SPEC-10-presentation→`07-…`
├── decisions/          ← 决策记录 DEC-*（10 篇：立场书 + 4 篇早期裁决 + PV/折叠/AttributeSet/回调载体）
├── log/                ← 决策日志 LOG-*（8 篇：按模块轮次分片 + 拍板流水 + 实施记录）
├── plans/              ← 实施计划 PLN-*（5 篇：R3 计划一二 + 竖切剧本 + 内容指南 + R4 计划）
├── research/           ← 调研 RSCH-*（6 篇：AbilityKit 三路 + 复制 + 脚本 + C#）
├── evidence/           ← 证据 EVID-*（3 篇 PIE 取证，带 SHA-256）
├── ledger/             ← 台账与规约 LEDGER-*（3 篇活文档）
└── HISTORICAL/         ← 历史存档（模块地图提案 v1，已被 DEC-00 §9 取代）
```

---

## 3. 三层真相源（谁说了算）

**这是本工作区此前从未写明的关键约定**：

| 层 | 载体 | 管什么 | 冲突时 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/<能力>/spec.md`（24 条） | MUST / Scenario——代码必须满足的行为 | **规格优先**（它进 `openspec validate --strict`） |
| **设计意图** | 本工作区 `SPEC-*` / `DEC-*` / `LOG-*` | 为什么这么设计、边界在哪、否决了什么 | 规格未覆盖处以此为准 |
| **项目约定** | `openspec/project.md` | 命名标准、依赖铁律、测试策略、过网结构纪律 | 全局约定优先 |

- **实现状态**既不在规格也不在规格文档里，只在 `SPEC-TRACE` + `LEDGER-deferred` + 计划注记三处登记。
- **规格目录正在引用本工作区文档的节号**（如 `02 §2.3`、`04 §2.1`）——所以**改节号等于改规格**，见 `CONVENTION` §5。

---

## 4. 文档清单

> 状态词义见 `CONVENTION` §4。**完整路径映射表在 `GLOSSARY.md` §5.1**（本表只列阅读顺序）。

### 4.1 模块规格（`SPEC`，11 + 1 篇）

| ID | 模块 | 编译层模块名 | 状态 | R 范围 |
|---|---|---|---|---|
| `SPEC-00-core` | M0 内核 | `TcsCore` | 生效中 | R3 已交付 |
| `SPEC-01-attributes` | M2 属性/数值 | `TcsAttribute` | 生效中 | R3 已交付 |
| `SPEC-02-states` | M3 状态/Buff | `TcsState` | 待落地 | **R5** |
| `SPEC-03-effects` | M4 效果执行 | `TcsEffect` | 生效中 | R3 已交付（触发行为 R4） |
| `SPEC-04-skill` | M5 技能/施法 | `TcsSkill` | 待落地 | R6 |
| `SPEC-05-integration` | M6 集成 | `TcsIntegration` | 生效中 | R3 已交付（扩展 R7） |
| `SPEC-06-targeting` | 目标选择 | `TcsTargeting` | 生效中 | R3 已交付 |
| `SPEC-07-notation` | 策划记法 | `TcsNotation` | 待落地 | 骨架壳 R3；内容 R8 |
| `SPEC-08-damage` | 伤害瞬时流程 | `TcsDamage` | 生效中 | R3 已交付（修改器 R4） |
| `SPEC-09-editor` | M8 编辑器与工具 | `TcsEditor` | 待落地 | R8 |
| `SPEC-10-presentation` | M7 表现 | `TcsCue` | 待落地 | R8 |
| `SPEC-TRACE` | 契约追踪矩阵 | （跨模块） | 活文档 | — |

### 4.2 决策记录（`DEC`）与决策日志（`LOG`）

| ID | 内容 | 状态 |
|---|---|---|
| `DEC-00-constitution` | R0 立场书：**§8 分离宪法**、**§9 模块物化规定（模块表真相源）** | 生效中 |
| `LOG-DECISIONS` | 逐条拍板流水（历史裁决 / 模块物化 / 拍板记录）——[`log/decisions-log.md`](log/decisions-log.md) | 活文档 |
| `LOG-IMPLEMENTATION` | 逐 Task 实施与验收记录 + 检查点状态——[`log/implementation-log.md`](log/implementation-log.md) | 活文档 |
| `DEC-2026-08-31-state-relation` | 裁决 1：状态关系表 + 状态槽归属 | 冻结 |
| `DEC-2026-08-31-event-bus` | 裁决 2a：事件分发 = 总线 + 订阅表 + 共享 Handler | 冻结 |
| `DEC-2026-08-31-tick-pump` | 裁决 2b：Tick 泵与组件执行边界 | 冻结 |
| `DEC-2026-08-31-visualization` | 裁决 3：可视化编辑范式 | 冻结 |
| `DEC-2026-09-01-abilitykit` | AbilityKit 对照综合 + 原语集/触发行提案 v1 | 冻结（**提案值已被取代**：17→15 原语、12→10 字段） |
| `LOG-00-core` | M0-min + M2 决策点（`D0-*`、`D2-*`、`MD-*`、`NET-*`） | 冻结（已拍板） |
| `LOG-01-states` | M3 决策点（`D3-*`） | 冻结（已拍板） |
| `LOG-02-effects` | M4 决策点（`D4-1`~`D4-5`、`D4-14`~`D4-17`、**`D7-*`**） | 冻结（已拍板） |
| `LOG-03-skill` | M5 决策点（`D5-*`、`PV-10`、**`D4-5`**） | 冻结（已拍板） |
| `LOG-04-integration` | M6 决策点（`D6-*`） | 冻结（已拍板） |
| `DEC-01-pv` | PV 系列：参数值来源策略体系（`PV-0`~`PV-10`） | 冻结（已拍板） |
| `DEC-02-fold-display` | 参数折叠与展示（`D5-5` v3 / `D5-17` v2→v3 / `D5-18` v3 / `PV-1`、`PV-10` 增补） | 冻结（已拍板） |
| `DEC-03-attribute-set` | 属性存在性与 AttributeSet（`D2-14` / `D2-15`） | 冻结（已拍板） |
| `DEC-04-callback-carriers` | **回调载体**：全仓 `TFunction` 用途分类与替换边界（A 类注册值可换 / B 类 `TFunctionRef` 不可换 / C 类内部回调不动、其中 `OnConsumed` 重做 / D 类静态自注册禁止换）+ 各角色生命周期策略（CDO vs 每执行实例）——[`decisions/dec-04-callback-carriers.md`](decisions/dec-04-callback-carriers.md) | **决策已拍板** 2026-09-29；**实现部分落地**（裁定 ⑤ 第一批 = 注册表寿命语义，已归档并实测；裁定 ③ 记账生效；余项待后续提案） |
| `LOG-99-misc` | Mass 连续内存备忘 | 冻结（**未拍板**，未来优化抽屉） |
| `DEC-2026-09-02-module-map` | 模块拆分地图提案 v1 | **历史存档**（被 `DEC-00-constitution` §9 取代） |

### 4.3 实施计划（`PLN`）

| ID | 轮次 | 内容 | 状态 |
|---|---|---|---|
| `PLN-R4` | **R4（当前）** | 触发行与伤害修改器通道；**含 R4–R8 轮次路线图（现行顺序的真相源）** | 进行中（Task 1/2 完成，余 5 项未执行） |
| `PLN-R3-vertical-slice` | R3 | 竖切验收剧本（7 项人工检查单） | 冻结（7/7 已验） |
| `PLN-R3-1` | R3 | 计划一：Core + Attribute（Task 0–6） | 冻结（全部完成） |
| `PLN-R3-2` | R3 | 计划二：Effect + Targeting + Damage + Integration（Task 0–7） | 冻结（全部完成并验收） |
| `PLN-R3-content-guide` | R3 | 竖切内容资产建立指南（人工步骤 + 实测结果） | 冻结（已执行完毕） |

### 4.4 轮次路线图（R4–R8）

| 轮次 | 主题 | 前置 | 台账消费 |
|---|---|---|---|
| **R4**（进行中） | M4a 触发行 + 伤害修改器通道 + 原语补齐 | R3 已收束 | `DAMAGE-1`/`DAMAGE-2`/`DAMAGE-3`（部分） |
| **R5** | M3 状态层（`TcsState`） | R4（行为面验收） | `STAT-1`/`STAT-2`/`STAT-3`、`DAMAGE-2` 余、`DAMAGE-4` |
| **R6** | M5 技能层（`TcsSkill`） | R5（`FSkillDef` 继承 `FStateDefBase`） | `STAT-1`、`CORE-1`（若成立） |
| **R7** | M6 集成层（两级单位 / StateTree / AttributeSet / 加载层） | R6 | `INTEG-1`~`INTEG-3`、`WAIT-2`/`WAIT-5`/`WAIT-6` |
| **R8** | M7 表现 + M8 编辑器与工具 | R7 | `PRES-1`、`TOOLS-1`~`TOOLS-6`、`WAIT-1`/`WAIT-4`/`WAIT-9`/`WAIT-10` |

> 弹性：R6 可能拆两轮；`INTEG-3` 的加载层可拆独立轮；`WAIT-*` 条目在触发条件成立时随当轮消费。

### 4.5 台账与追踪（`LEDGER`，活文档）

| ID | 内容 | 规模 |
|---|---|---|
| `LEDGER-deferred` | 跨轮遗留输入台账（**唯一待办登记册**）：按轮次分组的 37 条 + 变更记录 | 37 条 |
| `LEDGER-reflection` | 反射未解决项（`R-1`~`R-6`）+ 逐项专项调研登记册 | 6 项 |
| `LEDGER-terminology` | 「反射」术语规约（一词五义的唯一处方措辞）——**写含"反射"的文本前 MUST 通读** | — |

### 4.6 调研与证据（`RSCH` / `EVID`）

| ID | 内容 | 用途 |
|---|---|---|
| `RSCH-scripting-ustruct` | C# 脚本方案选型（UnrealSharp vs UnrealCSharp），`G-1`~`G-4` 定义处 | 脚本化决策依据 |
| `RSCH-csharp-authoring` | C# 编写 TCS 逻辑的可行性（glue 产物实证） | `SCRIPT-*` 条目来源 |
| `RSCH-replication` | UE 5.8 源码级：产物快照与 Cue 上下文能否过网 | 网络姿态依据 |
| `RSCH-abilitykit-pipeline` | AbilityKit A 路：技能管线与原语 | 原语集提案来源 |
| `RSCH-abilitykit-trigger` | AbilityKit B 路：触发器与属性修饰器 | 触发行提案来源 |
| `RSCH-abilitykit-editor` | AbilityKit C 路：编辑器工具与确定性/回放 | 裁决 3 校准来源 |
| `EVID-2026-09-28-scripting-e2e` | 宿主脚本插槽 E2E + 原生 GC 证据 | `SCRIPT-8` 验收 |
| `EVID-2026-09-28-selector-ref` | selector `ref` 容器回写两轮 PIE 证据 | 选择器签名验收 |
| `EVID-2026-09-29-registry-lifetime` | **两连 PIE：注册表跨世界寿命修复**（同进程二次登记不再被拒；含生效机制诊断） | 提案 `harden-registry-cross-world-lifetime` 核心判据 |

### 4.7 治理（`GOV`）

| ID | 用途 |
|---|---|
| `INDEX` | 本文——入口 |
| `GLOSSARY` | 术语与编号权威表 + 文档 ID 映射 |
| `CONVENTION` | 文档规范（类型 / 状态 / 身份块 / 引用格式 / 写作纪律） |

---

## 5. 更新纪律（谁该在什么时候改什么）

| 发生了什么 | 必须更新 |
|---|---|
| 新增/删除文档 | 本文 §4 + `GLOSSARY.md` §5.1（ID → 路径） |
| 新增决策编号 | `GLOSSARY.md` 对应族表 + 该决策的 `LOG`/`DEC` 文档 |
| 拍板一项 | `LOG-DECISIONS`（日期 + 决定 + 依据 + 落档点），并回写对应 `SPEC` |
| 一轮开工 | 通读 `LEDGER-deferred` 该轮区段 → 折进该轮计划 |
| 一轮收束 | `LEDGER-deferred` 勾销已消费条目（**不删条目**）+ 计划标完成 + `SPEC-TRACE` 更新验证状态 |
| 移动/改名文档 | `GLOSSARY.md` §5.1 + 全库引用（引 ID 者无需改） |
| 改了 `SPEC` 的节号 | 检查 `openspec/specs/*` 里对本工作区节号的引用（现有 10 处） |