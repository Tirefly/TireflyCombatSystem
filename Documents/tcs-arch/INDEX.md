# TCS 模块档案 · 入口索引

- **文档 ID**：`ARCH-INDEX`
- **类型**：治理（入口索引 + 审核方法与进度总表）
- **状态**：生效中（2026-10-17 建立）
- **权威范围**：本文件回答「**当前实际落地的是什么、还欠什么**」。它不承载决策理由（旧 `DEC-*` 是理由的真相源）、也不承载可执行需求（`openspec/specs/` 是契约的真相源）。三者的分工见 §2。
- **最后更新**：2026-10-17（建立；`ARCH-01-core` / `ARCH-02-notation` / `ARCH-03-asset-butler` / `ARCH-04-attribute` / `ARCH-05-state` 已审）

---

## 1. 这套文档是什么，和旧文档什么关系

### 1.1 两个文档族的定位

| 文档族 | 位置 | 状态 | 读者该拿它干什么 |
|---|---|---|---|
| **旧设计文档族** | `Documents/combat-system-design/`（96 篇 / 27 512 行） | **冻结为历史真相源**（2026-10-17 起不再维护） | 查**历史理由**：某个决策当时为什么这么定、否决了什么。**MUST NOT** 当作现状描述读——它含大量已过期内容（见 §1.3） |
| **本档案族（新）** | `Documents/tcs-arch/` | **当前权威**（描述"代码今天实际长什么样"） | 查**现状**：落地了什么、契约是什么、欠了什么账 |

### 1.2 为什么必须冻结旧文档族

2026-10-17 用户裁定：旧文档族已积累大量**过期与互相冲突**的内容，债务越积越多，必须停下来重新梳理。判据是实测的：

- 旧 `SPEC-00-core` 里全部核心类型名仍是 `Combat*` 一族（`FCombatEventBus` / `UCombatClockSubsystem` / `TCombatInstanceHandle` / `TInstancePool` / `FExpiryHeap` / `FCombatClock` / `ITimeSource` / `FCombatEntityHandle`），而代码**早已全量改为 `Tcs*` 一族**（实测旧名在 `Source/` 内命中 **0** 个文件）。照旧规格写代码**编译不过**。
- 同一件事在三处有不同说法（旧 SPEC / `openspec/specs/` / 代码注释），且**没有校验器能发现**。旧 `INDEX` §5 自己立的纪律「计数声称 MUST 全库同批回改」正是这类漂移的补丁，但补丁本身也会漏。

### 1.3 旧文档族的已知缺陷类型（审核时逐模块登记）

| 缺陷类型 | 含义 | 本档案的处置 |
|---|---|---|
| **改名滞后** | 符号已改名，旧文档仍写旧名 | 不做全局修补；只在本档案写**当前名**，需要历史映射时回旧文档查 |
| **声称未落地** | 旧文档写"提供 X"，代码里没有 X | 记入该模块 §债务 或 §未实现 |
| **实现超文档** | 代码有 X，旧文档与 `openspec` 都没写 X | 记入该模块 §落地，并标注"文档缺失" |
| **已推翻仍留存** | 旧文档写了后来被裁决推翻的内容 | 记入该模块 §未实现（附推翻它的决策编号） |

---

## 2. 三层真相源（MUST NOT 混用）

| 层 | 载体 | 回答什么 | 冲突时谁赢 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/<能力>/spec.md`（45 个能力） | "系统 MUST 做什么"（有 `#### Scenario:`、可被 `validate` 校验） | **这一层赢**（它是契约） |
| **实际实现** | `Source/<模块>/` | "代码今天做了什么" | 与需求冲突 = **本档案要登记的技术债** |
| **历史理由** | `Documents/combat-system-design/decisions/` + `log/` | "当时为什么这么定、否决了什么" | 只作追溯，不作现状 |

> **本档案的判据**：三层的**两两差异**都要显式登记。不存在"哪个对"的模糊地带——需求说 MUST 而代码没做 = 债务；代码做了而需求没写 = 文档缺失；旧文档说做了而需求没提 = 旧文档缺陷。

---

## 3. 模块总表与审核进度

11 个代码模块各自成篇。顺序 = **编译期依赖自底向上**（先审地基，上游稳定后下游的契约才好判）。

| # | 模块 | 行数 | 直接依赖 | 档案 | 审核状态 |
|---|---|---|---|---|---|
| 00 | — | — | — | [`ARCH-00-overview`](00-overview.md) | 待写 |
| 01 | `TcsCore` | 2 599 | 无 | [`ARCH-01-core`](01-core.md) | ✅ **已审 2026-10-17** |
| 02 | `TcsNotation` | 92 | 无 | [`ARCH-02-notation`](02-notation.md) | ✅ **已审 2026-10-17** |
| 03 | `TcsAssetButler` | 1 058 | 无 | [`ARCH-03-asset-butler`](03-asset-butler.md) | ✅ **已审 2026-10-17** |
| 04 | `TcsAttribute` | 3 012 | Core, Notation | [`ARCH-04-attribute`](04-attribute.md) | ✅ **已审 2026-10-17** |
| 05 | `TcsState` | 2 753 | Core, Notation, Attribute | [`ARCH-05-state`](05-state.md) | ✅ **已审 2026-10-17** |
| 06 | `TcsEffect` | 7 115 | Core, Attribute, AssetButler | [`ARCH-06-effect`](06-effect.md) | 待审 |
| 07 | `TcsBuff` | 5 839 | Core, Notation, Attribute, Effect, State, AssetButler | [`ARCH-07-buff`](07-buff.md) | 待审 |
| 08 | `TcsSkill` | 11 052 | Core, Notation, Attribute, Effect, State | [`ARCH-08-skill`](08-skill.md) | 待审 |
| 09 | `TcsTargeting` | 1 144 | Core, Effect | [`ARCH-09-targeting`](09-targeting.md) | 待审 |
| 10 | `TcsDamage` | 3 225 | Core, Attribute, Effect | [`ARCH-10-damage`](10-damage.md) | 待审 |
| 11 | `TcsIntegration` | 1 536 | 全部 | [`ARCH-11-integration`](11-integration.md) | 待审 |

**合计**：337 文件 / 39 425 行。

### 3.1 依赖图（实测自各模块 `*.Build.cs`）

```
层 0（无依赖）   TcsCore          TcsNotation      TcsAssetButler
                   │                  │                  │
层 1              └──────► TcsAttribute ◄──────────────────┘
                              │    │
层 2                          │    ├──► TcsState
                              │    └──► TcsEffect ◄── AssetButler
                              │            │
层 3                          │      ┌─────┼──────────┬──────────┐
                              │      ▼     ▼          ▼          ▼
                              │  TcsDamage TcsTargeting TcsBuff  TcsSkill
                              │      │     │          │          │
层 4                          └──────┴─────┴──────────┴──────────┘
                                        ▼
                                 TcsIntegration（全部）
```

**两条值得记住的边**：
- `TcsBuff` 与 `TcsSkill` **互不依赖**——它们共用的全部是 `TcsState` 的共享层（`FTcsStateDefBase` / `FTcsStateInstance` / 关系表族）。2026-10-17 的 `DEC-2026-10-17-tag-relationship-ownership` 正是靠这条实测边成立的。
- `TcsState` **不依赖** `TcsEffect`（`PLN-REFACTOR-1` 的"去掉 TcsEffect → TcsState 边"裁定已兑现）；但 `TcsBuff` **依赖** `TcsEffect`——所以"状态层不碰效果层"这句话只对 `TcsState` 成立，对 `TcsBuff` **不成立**。

### 3.2 旧文档族与代码模块的对应缺口（实测）

| 现象 | 具体 | 归属 |
|---|---|---|
| **旧文档族无模块规格** | `TcsAssetButler`（1 058 行）→ 旧 `spec/` 里**零命中**，只有一份 `DEC-2026-10-13-asset-butler`。⚠️ 但 `openspec` 侧**有** `primary-asset-loading`（108 行 / 6 需求）——该能力建于 2026-10-15，晚于旧 `spec/` 的写作时点 | 本档案 `ARCH-03` 首次成篇 |
| **有代码无独立规格** | `TcsBuff`（5 839 行）→ 内容混在旧 `SPEC-02-states` 里 | 本档案 `ARCH-07` 拆出独立成篇 |
| **有规格无代码** | 旧 `SPEC-09-editor`（PENDING）、旧 `SPEC-10-presentation`（PENDING） | 归 `ARCH-00-overview` §未实现的模块 |
| **规格状态与代码不符** | 旧 `SPEC-07-notation` 标 `PENDING`，但 `TcsNotation` 有 92 行代码 | `ARCH-02` 判定实际落地度 |
| **旧 SPEC 状态与代码不符（新）** | 旧 `SPEC-01-attributes` 标 `ACTIVE` 且质量高，但含两处已过时内容：§2.2a 的 `ModifierSlots`（已改名 `AttrModInstances`）、§5 的网络姿态 NET-1/2（`openspec` 零对应需求） | `ARCH-04` §7 已逐项登记 |
| **权威层主语错了（★ 最严重的一类）** | 10 个 `state-*` 能力里 **7 份整篇在讲 `TcsBuff`**，`openspec` **没有任何能力回答「`TcsState` 今天剩什么」**；而**旧 `SPEC-02-states` 反而更准**（有 §12.13 的 `PLN-REFACTOR-1` 收束回写）⇒ 本模块的权威顺序是**反的** | `ARCH-05` §1.2 / §5 D-1 / §7 |

> **这正是"11 篇模块规格 ≠ 11 个代码模块"的实证**：旧文档族按**设计轮次**切（M0–M9），代码按**编译单元**组织，两者在 `TcsBuff` / `TcsAssetButler` 上分叉。本档案按**代码模块**切，因为审的是"代码落地了什么"。

---

## 4. 每篇的固定结构（六节，MUST 齐备）

| 节 | 回答什么 | 硬要求 |
|---|---|---|
| **§1 模块定位与边界** | 一句话职责 + 依赖/被依赖 | 一句话必须能当"这个模块是什么"的答案 |
| **§2 已落地** | 最终落地的决策、思路、设计、实现 | 每条 MUST 有**代码锚点**（`路径:符号`）与**需求锚点**（`openspec/specs/<能力>`） |
| **§3 对外契约** | 其它模块能依赖什么、MUST NOT 依赖什么 | 区分"稳定契约"与"内部实现细节" |
| **§4 竖切影响** | 在端到端流程里对其它模块的影响细节 | 必须写出**调用方向**与**时序位置**，不只列名字 |
| **§5 技术债** | 明明应该落地但实现有偏差的 | 每条 MUST 有：**期望 vs 实际**、**偏差证据**、**影响面**、**是否已登记** |
| **§6 未实现** | 尚未实现的内容 + 重新核对的设计细节与决策 | 每条 MUST 写：**设计是否还成立**、**当初的依据今天是否仍有效**、**前置条件** |

**§5 与 §6 的判据区别**（这是最容易混的一对）：
- **技术债** = 需求说了 / 设计定了，但**代码做了却做偏了**（或做了一半）。**代码存在但不达标**。
- **未实现** = 代码**根本不存在**。设计可能有，也可能需要重新核对。

### 4.1 置信度标注（MUST 逐条标）

| 标记 | 含义 |
|---|---|
| **【实测】** | 读过代码或跑过命令直接确认 |
| **【推断】** | 由多处证据推理得出，未直接验证 |
| **【待核】** | 未确认，读者 MUST NOT 当结论用 |

---

## 5. 审核方法（每个模块都走这五步）

1. **读可执行需求**：`openspec/specs/` 里属于该模块的能力（`grep -rl "<模块名>" openspec/specs/*/spec.md`）。
2. **读代码**：提取全部公开符号（`UCLASS`/`USTRUCT`/`UENUM`/`class`/`struct` + 公开函数签名），建立实际 API 面。
3. **读旧模块规格**：找出它声称的符号，与第 2 步逐项对照 → 产出「改名滞后 / 声称未落地 / 实现超文档」三类差异。
4. **追竖切**：查该模块的符号在**其它模块**里的使用点，判断对外契约的真实面（`grep -rn "<符号>" Source/ --include=*.h`，按模块归类命中数）。
5. **交叉验证**：对每条结论标注置信度；凡"未实现"项必须反查是否只是**在别的模块里实现**（本仓有多次"以为没做、其实搬到别处"的先例）。

### 5.1 三条已知的审核陷阱（本仓实测）

1. **改名族会导致"文档全错但代码全对"**：`Combat* → Tcs*` 这类全局改名，旧文档不会自动跟。**判据**：凡旧文档里的类型名，先在代码里 `grep` 一遍；命中 0 就说明是旧名。
2. **同一符号可能有两个真相**：`ForEachForGC` 在 `openspec` 里住在 `entity-runtime-mechanism`，而池的实现住 `instance-handle-pool` —— 一条能力可能被拆到两个 spec 里写。**判据**：搜符号不能只搜一个能力目录。
3. **"零消费者"不等于"未实现"**：本仓大量设施因"零消费者不预建"而**有意**未建（如 `FTcsParamEnumerableSource::Enumerate`）。这类要记 **§未实现** 并写明"这是有意留白，不是遗漏"。

---

## 6. 全局待办（跨模块，逐模块审核时追加）

> 这里只放**跨模块**的事项；单模块的债与缺口记在各自档案的 §5 / §6。

| # | 事项 | 现状 | 影响 |
|---|---|---|---|
| G-1 | 旧文档族冻结横幅 | 未加 | 读者可能仍按旧 `SPEC` 写代码（旧 `SPEC-00-core` 的类型名已全错） |
| G-2 | `openspec` 与旧 `SPEC` 的口径差 | 已知 `openspec` 用的是新名、旧 `SPEC` 用旧名 | 审核时一律以 `openspec` 为准 |
| G-3 | `SPEC-TRACE`（契约追踪矩阵，85 KB / LIVING）的时效 | 未核 | 它声称"区分静态实现 / glue / PIE 已验证"，但最后一次实质更新早于 `R5.5-e1` |
| **G-4** | **`openspec/specs/state-def-asset/spec.md:71` 的载体归属是假事实** | **已定位，未修** | 它写 `FTcsDescriptionEntry` / `FTcsDescriptionViewSlot` **住 `TcsBuff`**，实际住 `TcsState`；且按依赖图前者**不可实现**（`TcsSkill` 不依赖 `TcsBuff`，却要白拿 `Descriptions`）。详见 `ARCH-02-notation` §5 D-1。**`openspec` 是最高权威层，这条 MUST 修** |
| **G-5** | **旧文档族的 `PENDING` 状态词不够用** | 已定位 | `ARCH-02` 出现"一模块两状态"（值约定已落地 + 描述视图体系未落地）。旧五态词表无法表达"部分落地"，导致 `PENDING` 被读成"什么都没做"。详见 `ARCH-02` §5 D-3 |
| **G-6** | **`openspec/specs/primary-asset-loading/spec.md` 漏收录 `FTcsPrimaryAssetQuery`** | **已定位，未修** | 规格 6 需求 / 12 场景**零提及**该口，而它是该模块**唯一有真实消费者**的设施（4 处调用）、且修复了两处真实回归（`Slice.Run` 2/4→65/0、`State.Run` 6/9→15/0）。根因 = 执行序：Task 8（开规格+归档）在 Task 6（建查询口）之前，Task 5（回写）也夹在中间 ⇒ 无一轮覆盖。详见 `ARCH-03` §5 D-1。**与 G-4 同类：最高权威层漏写，MUST 修** |
| **G-7** | **`openspec/specs/integration-entity/spec.md:51` 的"五条按类发现路径"已假** | **已定位，未修** | 五条里 3 条（Chain / Trigger / State）已改走 `FTcsPrimaryAssetQuery`；且"不依赖 `PrimaryAssetTypesToScan` 注册"与 3 处自注册冲突。该行正是 Task 8《实施记录》登记给 Task 5 的待回写，而 Task 5 已跑完未消费 ⇒ 无人盯。详见 `ARCH-03` §5 D-2 |
| **G-8** | **"三策略已落"的措辞需订正** | 已定位 | 台账 `INTEG-3` 记「加载三策略 ✅ 已落」：作 **API** 语义成立，作**能力**语义不成立（`FTcsAssetLoader` 外部引用 0、`ETcsAssetLoadPolicy` 读 0 处、三处注册全 OnDemand）。建议统一改为「枚举已落、语义未落」。详见 `ARCH-03` §5 D-4 / §6 U-5 |
| **G-9** | **`openspec/specs/attribute-types/spec.md:164` 引用了不存在的能力 `attribute-moddef`** | **已定位，未修** | 该行写「编辑器侧给警告见 `attribute-moddef` 一节」，而 `openspec/specs/` 下**没有**这个能力（实测只有 `attribute-pipeline` / `-store` / `-transaction` / `-types` 四个）。该悬空引用自归档变更 `2026-09-18-add-tcsattribute-pipeline-and-transaction` 起就存在，下一轮 delta **逐字复制** ⇒ 两轮归档都没修。`openspec validate --strict` **不校验跨能力引用**（只校验 Requirement/Scenario 结构）⇒ 这类错误**永远不会被工具发现**。详见 `ARCH-04` §5 D-3。**性质与 G-4/G-6/G-7 不同：不是「说错了」，而是「指向不存在的东西」** |
| **G-10** | **「规格更严、代码更松」是一整类缺陷，`ARCH-04` 一次审出 4 条** | 已定位 | `attribute-transaction` 的「失败零写入」/ `attribute-pipeline` 的「图每轮重建」与「MUST NOT 直伸存储」/ `attribute-transaction` 的「候选值」——**规格全是 MUST，代码全没做**。这与 G-6/G-7 的「规格漏写」方向**正好相反**。⇒ **后续模块审核 MUST 双向查**：既要查「代码做了规格没写」，也要查「规格写了代码没做」。`ARCH-03` 只查了前者 |
| **G-11** | **台账与档案是两套账，`ARCH-04` 新增的 11 条债一条都没进台账** | 已定位 | `ARCH-04` §5 的 D-1..D-11 全部「是否已登记 = 否」（D-6 除外，它对应 `STAT-11`）。台账 `deferred-inputs-ledger.md` 是「跨轮遗留输入的唯一登记册」，而本档案是「现状审核」。**两者没有同步机制** ⇒ 审出来的债不会自动变成待办。**这是流程级缺口，不是某一模块的问题** |
| **G-12** | **「代码搬走了、指向它的名字没搬」是本仓特有的第三类缺陷** | 已定位 | `ARCH-05` 一次审出 **4 条**（D-4 `RegisterStateDef` 符号已不存在 / D-5 上下文门面写错模块 / D-6 `BaseStruct` 指向不存在的类型 / D-8 依赖空转），**全部由 `PLN-REFACTOR-1` Task 4 拆出 `TcsBuff` 造成**。这与 G-10 记的两个方向（规格更严 / 规格漏写）**都不同**：它是**模块拆分后，符号名 / 模块名 / 类型名 / 字段位置四类引用全部滞留**。⇒ **后续凡经历过拆分的模块，审核 MUST 逐项核对「规格里的符号名是否还指向它」**。⚠ D-6 尤其危险：`BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment"` 指向**全仓不存在的类型**，而 `openspec validate` **不校验字符串元数据** ⇒ 工具永远查不出 |
| **G-13** | **两条既定判据在 `SourceRef` 上正面冲突** | 已定位 | 「零消费者不预建」（框架纪律）vs 「源引用位是 Debug 与 Live 化的既定基建，删掉会与设计冲突」（`SPEC-02-states` §3.6 / PV-3）。`FTcsParamSnapshotEntry::SourceRef` **写入 2 处、读取 0 处**（只有 GC 补引用）。⇒ 本仓需要一条**「契约默认」判据**来裁决这类：设计明文承诺的字段即使零消费者也保留，而**便利 API** 零消费者即删。这与 `ARCH-04` 的 D-8（`GetTcsAttributeBandWeight` 零调用即属违反）**不是同一档** |
| **G-14** | **「四条注释 + 一个 Scenario 承诺、实现漏一行」——契约兑现率没有校验手段** | 已定位 | `ARCH-05` 的 **D-10**：`ForEach` 的「访问者返回 false 即提前终止」在 **4 处公开头注释**与**规格 1 个 Scenario**里都写了，而实现是 `Pool.ForEach([&](Item){ Visitor(Item); })`——**bool 返回值被丢弃**，且池的 `ForEach` 签名本就是 `void`。⇒ **注释里的契约无人校验**，而 `openspec validate` 只查结构不查语义。这类缺陷的共性 = **功能仍正确、只有性能与读数偏离，因而不进任何断言** |

---

## 7. 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。落地用户裁定：旧文档族冻结为历史真相源、新档案按**代码模块**切 11 篇、目录名 `Documents/tcs-arch/`。首篇 `ARCH-01-core` 完成。 |
| 2026-10-17 | `ARCH-02-notation` 完成；新增 **G-4**（`state-def-asset` 载体归属假事实）/ **G-5**（旧状态词表不够用）。 |
| 2026-10-17 | `ARCH-03-asset-butler` 完成；修正 §3.2 第 1 行（原文"有代码无模块规格"会误导——`openspec` 侧**有** `primary-asset-loading`，缺的是旧 `spec/`）；新增 **G-6**（`primary-asset-loading` 漏收录 `FTcsPrimaryAssetQuery`）/ **G-7**（`integration-entity:51` 已假）/ **G-8**（"三策略已落"措辞需订正）。 |
| 2026-10-17 | `ARCH-04-attribute` 完成（534 行）。首次审出**「规格更严、代码更松」**这一类缺陷（4 条：D-1 失败零写入 / D-4 图不重建 / D-7 直伸存储 / D-9 候选值不完整）——与 `ARCH-03` 的方向**正好相反**。新增 **G-9**（`attribute-types:164` 引用不存在的能力 `attribute-moddef`）/ **G-10**（该类缺陷需双向查，`ARCH-03` 只查了一半）/ **G-11**（台账与档案是两套账，无同步机制）。另修正 §3.2 无需改动（`TcsAttribute` 旧 `SPEC-01` 存在且质量高） |
| 2026-10-17 | `ARCH-05-state` 完成（**645 行**，本档案迄今最长）。审出 **D-1..D-12 / U-1..U-7**。三条结构性发现：① **权威层主语错了**——10 份 `state-*` 规格里 7 份整篇讲 `TcsBuff`，`openspec` 里没有一个能力回答「`TcsState` 剩什么」，而**旧 `SPEC-02-states` 反而更准**（§12.13 有收束回写）⇒ 本模块的权威顺序是反的；② 新增缺陷方向 **G-12**（代码搬走、引用滞留，4 条）；③ 唯一**纯代码缺陷**是 **D-10**（`ForEach` 提前终止契约未实现——4 处注释 + 1 个 Scenario 都承诺了）。另新增 **G-13**（两条判据在 `SourceRef` 上冲突，需要「契约默认」裁决）/ **G-14**（注释里的契约无人校验）。**同时修正 §3.2**：新增「权威层主语错了」一行 |