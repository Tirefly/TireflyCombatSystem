# 规格修复批次 R1 · 执行记录

- **文档 ID**：`ARCH-FIX-R1`
- **类型**：修复批次记录（零代码 · 只动 `openspec/specs/`）
- **状态**：已执行 2026-10-17
- **权威范围**：本文件回答「**本批次改了哪些规格、为什么改、改成了什么**」。它不重复债务描述——那些在 `ARCH-01`~`ARCH-11` 的 §5 / §6 与 `INDEX` §6。
- **边界声明**：本批次 **MUST NOT 触碰 `Source/`**（由并行的代码会话负责）。凡某条债的正确修法需要改代码、或需要一次设计裁决，**本批不收**，只在此登记「为什么不收」。

---

## 1. 批次定义

用户裁定：**当前会话执行「低风险、零代码的规格修复」**，代码侧交由分支会话。

三条判据（本批的实际筛选标准）：

| 判据 | 含义 | 被它排除的 |
|---|---|---|
| **① 事实性** | 改动只涉及「代码里叫什么 / 在哪个模块 / 什么签名」——**正确答案由代码唯一确定**，不需要任何设计判断 | 需要裁决的（`ARCH-11` U-2 实体注销编排点、`ARCH-09` D-4 上下文默认目标归属） |
| **② 零代码** | 只改 `openspec/specs/` 与档案自身 | `Build.cs` 注释（`ARCH-11` D-6/D-7）、`IsDataValid` 缺 `Valid` 提升的实现半（`ARCH-06` D-10） |
| **③ 可验证** | 改完能跑 `openspec validate --strict` 且结果可判 | 需要 PIE 才能验证的行为面（全部） |

---

## 2. 本批改动：8 类共 18 条

### 2.1 假符号（规格里的名字在代码里不存在或已改名）

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 1 | `state-instance-lifecycle:197/203/217` | `UTcsStateSubsystem::RegisterStateDef` / `UnregisterStateDef` / `GetRegisteredStateDef` | `UTcsBuffSubsystem::RegisterBuffDef` / `UnregisterBuffDef` / `GetRegisteredBuffDef` | `ARCH-05` D-4 / `ARCH-07` D-5 / `X-2`（三篇各自记过） |
| 2 | `integration-entity`（登记口两处 + Scenario 一处） | `UTcsStateSubsystem::RegisterStateDef` / `GetRegisteredStateDef` | 同上 | 同一错误在第二个能力里重复 |
| 3 | `state-stacking-policies`（4 处正文） | `FTcsBuffStackPolicy` | `FStateStackPolicy` | `ARCH-07` D-3；代码 `TcsBuffStackPolicy.h:83` |
| 4 | `state-def-asset:18` | `FTcsBuffStackPolicy StackPolicy` | `FStateStackPolicy StackPolicy` | 同上 |
| 5 | `state-stacking-policies`（8 处） | `EAR_Applied` / `EAR_Refreshed` / … | `EBAR_*` | `ARCH-07` D-3；代码 `TcsBuffEnums.h:70-76`。**同一份文件第 5 行用新名、第 84 行起用旧名** |
| 6 | `skill-cast-runtime` / `skill-cooldown` / `skill-param-chain` / `skill-cast-phases` / `skill-layer-e2e-validation`（**31 处**） | `RunSource` / `PhaseExpiryEntry` | `CascadeAnchor` / `ExpiryEntry` | `ARCH-08` D-5；代码 `TcsStateInstance.h:88`（**字段上提进共享基类**） |

### 2.2 悬空引用（指向不存在的东西）

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 7 | `attribute-types:164` | 「编辑器侧给警告见 `attribute-moddef` 一节」 | 指向本能力自己的对应需求 | `G-9`；`openspec/specs/` 下无该能力（**躲过 2 轮归档**） |
| 8 | `state-def-asset:18` | `BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment"` | `"/Script/TcsBuff.TcsBuffBehaviorFragment"` | `ARCH-05` D-6；**字符串元数据，validate 不校验** |

### 2.3 载体 / 模块归属写错

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 9 | `state-def-asset:71` | 「`TcsBuff` MUST 提供…（住 `TcsBuff`）」 | 「`TcsState` MUST 提供…（住 `TcsState`，`TCSSTATE_API`）」 | `ARCH-02` D-1；**按依赖图原说法物理不可实现**（`TcsSkill` 不依赖 `TcsBuff`） |
| 10 | `state-behavior-fragments:23` | `UTcsStateSubsystem* Subsystem` | `UTcsBuffSubsystem* Subsystem` | `ARCH-05` D-5 / `ARCH-07` D-10；代码 `TcsBuffBehaviorFragment.h:29` |
| 11 | `state-def-asset`（关系五字段） | 平铺写在 `FTcsBuffDef` 下 | 更正为「继承自基类 `FTcsStateDefBase`」并加归属说明 | `ARCH-05` D-7 |

### 2.4 签名缺形参

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 12 | `skill-registry:65` | `GetLevel(FTcsSkillEntryHandle)` | `GetLevel(FTcsCombatEntityHandle, FTcsSkillEntryHandle, const TArray<double>& LevelModifiers)` | `ARCH-08` D-9；代码 `TcsSkillSubsystem.h:466-469`（**少 2 个形参**） |
| 13 | `skill-registry:66` | `GetNumericParam(FTcsSkillEntryHandle, …)` / `IsSwitchSet(FTcsSkillEntryHandle, …)` | 两者都补 `FTcsCombatEntityHandle` 前置 | 同上；代码 `:~490` / `:~505`（**各少 1 个形参**） |

### 2.5 物理上做不到的形状

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 14 | `damage-primitive:137` | `uint64 FlowId` / `uint64 Sequence` | `int64` / `int64` | `ARCH-10` D-10；**UHT 不支持 `uint64` 作属性类型**，按规格写编译不过 |

### 2.6 规格漏收录「唯一有真实消费者的设施」

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 15 | `primary-asset-loading`（**新增 1 条需求 + 2 个 Scenario**） | 6 需求 / 12 场景**零提及** `FTcsPrimaryAssetQuery` | 新增需求「Def 类按类查询口」，含两个可区分性 Scenario | `G-6` / `ARCH-03` D-1 |
| 16 | `effect-chain-asset`（**新增 1 条需求 + 1 个 Scenario**） | 无任何校验需求（而代码有真实规则） | 新增需求「链资产的作者期校验」 | `ARCH-06` D-10 |
| 17 | `integration-entity`（发现路径那条） | 「**五条**按类发现路径」全走 `GetAssetsByClass` | 3 走 `FTcsPrimaryAssetQuery` / 2 走旧口，并记两口门禁差异 | `G-7` / `ARCH-11` D-7 |

### 2.7 被删空的契约（回归补回）

| # | 位置 | 改前 | 改后 | 依据 |
|---|---|---|---|---|
| 18 | `integration-entity`（**1 段契约 + 4 个 Scenario**） | 无「就绪单出口」、无跨世界装配验证 | 恢复「就绪单出口」契约段 + `就绪后组件可注册` / `链资产自动登记` / `新世界初始化后链仍可查（跨世界装配）` / `跨世界装配的实现时序锚点` | `ARCH-11` D-1 / D-3；**`539b14e` 误删**（同提交也删了 `damage-step-library` 十步） |

---

## 3. 「规格要求 vs 代码现状」的分工（本批最重要的一条纪律）

第 18 条带来一个必须写死的规则，否则下一轮会误判：

> **规格写「MUST 提供 X」而当时代码没有 X，是合法的**——规格是可执行需求（最高权威层），代码是实际实现；两者不一致正是本档案要登记的技术债。**MUST NOT 因为「代码没有」而删掉规格要求**，那正是 `G-30` / `G-32` 教的东西：**规格缺失不制造缺陷，只让缺陷变得不可发现**。

本批有 **3 处**属于这个形态，全部**在规格里明确标注了「代码现状（如实登记，MUST NOT 读成本条已实现）」**：

| # | 规格要求 | 代码现状 | 归属 |
|---|---|---|---|
| 18 | 定义库 MUST 经单出口通知「定义已就绪」 | `Initialize` 只置 `bRuntimeReady = true`，**不广播**；`OnDefinitionsReady` **从未存在** | `ARCH-11` D-1（★★★） |
| 16 | 链资产 `IsDataValid` MUST 把 `NotValidated` 提升为 `Valid` | 该实现**缺这一段**（同族其余 6 个类都有） | `ARCH-06` D-10 + 台账 `TOOLS-2` |
| 15 | `GetAssetsForDefClass` 的「未登记类型」与「零资产」MUST 可区分 | 有实现（`TcsPrimaryAssetQuery.cpp:51-55` 两段不同文案），但**本批未逐行验证两者是否真的落不同分支** | `ARCH-03` D-4 邻域，【待核】 |

---

## 4. 本批**明确不收**的（附理由）

| 条 | 为什么不收 |
|---|---|
| `ARCH-11` D-6（3 条空转依赖） | 需改 `TcsIntegration.Build.cs` ⇒ **代码** |
| `ARCH-11` D-7（`Build.cs:35` 注释「五条改走」→「三条」） | 同上（注释在 `Build.cs` 里） |
| `ARCH-11` D-4（`RemoveAtSwap` 破稳定序） | 需改 `.cpp` ⇒ 代码 |
| `ARCH-11` D-1 的**实现半**（补 `OnDefinitionsReady`） | 需写代码 + 决定广播形态（原生事件 tag vs 委托）⇒ 一次设计判断 |
| `ARCH-11` U-2（实体注销编排点） | **需要一次裁决**（编排点放组件 `EndPlay` 还是别处） |
| `ARCH-09` D-4（上下文默认目标归属） | 需要一次裁决（三处硬编码该由谁统一） |
| `ARCH-10` D-4 / D-5（`CandidateKey` 零读取 / 多目标语义） | 前者是代码缺失；后者需要**重新设计**（规格从未写过该语义） |
| `ARCH-07` D-1（Replace 支 use-after-free） | **★★★ 崩溃级代码缺陷**，必须走代码会话 + PIE |
| `ARCH-10` D-2（十步契约被删空） | 不是补文本就行——**需先裁定十步是否仍成立**，且恢复会改变 `FTcsFlowExecute` 的语义 ⇒ 应与代码侧同批 |
| `ARCH-06` U-1（`Parallel` / `Repeat` / `OnError` 从未存在） | 需先定形状 ⇒ 设计工作 |
| 台账与档案的同步（`G-11`） | 是流程缺口而非某条规格；需先定同步机制 |

---

## 5. 验证

| 项 | 结果 |
|---|---|
| `openspec validate --all --strict --no-interactive` | **45 passed / 0 failed**（改前也是 45/0 ⇒ 本批**未引入结构退化**） |
| 规格符号全量交叉验证（Node 脚本：244 个 `Tcs*` 族反引号符号逐个在 `Source/` 337 文件里查） | **13 个零命中，全部为正确的历史 / 否定记录**——`FTcsAttributeName`（被删旧名）/ `TcsEditor`（未建模块）/ `TcsFlowRestKey_*`（规格要求删掉的死常量）/ `FTcsCastAttrCapture` 等（被删类型）/ `FTcsViewBuildContext` 等（R8 未建项）/ `FTcsStateRegistry` 等（订正注记里引用的旧名） |
| 改动面 | **15 个 `spec.md` / +154 / −48 行**（另加本档案 1 文件） |
| PIE | **未跑**（本批零代码，无行为面可验） |

> **⚠ `validate` 的 45/0 不能当作「本批改对了」的证据**：它只校验 Requirement / Scenario 的**结构**（每条需求至少一个 Scenario、Scenario 用四个 `#`），**不校验**符号是否存在、跨能力引用是否悬空、字符串元数据指向哪里、字段类型是否被 UHT 接受。本批修的 18 条里，**只有第 15/16 条（新增需求）会被它看见**，其余 16 条它**一条都发现不了**——那 16 条全部是靠人眼对代码找出来的。这正是 `ARCH-00` §8.2 那条结论的实证。

---

## 6. 剩余待办（本批之后）

| 项 | 数量 | 归属 |
|---|---|---|
| `INDEX` §6 的全局待办 | **35 条（G-1..G-35）** | 本批消化了 **G-4 / G-6 / G-7 / G-9 / G-18** 的**规格半**；判据本身仍留在 §6（它们是「下次审核要主动找的形态」，不是一次性待办） |
| 本批已修的债条目 | **18 条**（跨 5 篇档案） | `ARCH-02` D-1 / `ARCH-05` D-4·D-5·D-6·D-7 / `ARCH-06` D-9·D-10（规格半）/ `ARCH-07` D-3·D-5·D-10 / `ARCH-08` D-5·D-9 / `ARCH-10` D-10 / `ARCH-11` D-1·D-3（规格半）·D-7（规格半） |
| 需要**代码会话**的 | 约 **9 条** ★★★ 级 + 多条 ★★ | 见 §4 |
| 需要**一次裁决**的 | **3 条**（`ARCH-11` U-2 / `ARCH-09` D-4 / `ARCH-10` D-5） | 裁决后才能动 |
