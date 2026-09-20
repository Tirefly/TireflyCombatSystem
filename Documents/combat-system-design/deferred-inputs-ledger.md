# TCS 跨轮遗留输入台账

- 建立：2026-09-18（用户拍板新建独立台账文档；同轮否掉"建 plan3 装遗留项"的方案，理由见下"与计划的分工"）
- 性质：**登记册 / 索引**，不是计划，也不是待办许愿池
- 用途：登记「**决策已拍板、代码未落地/未验证**」且**不在任何现有计划 Task 里**的输入——这些内容此前散在决策文档段落、README 收束段与记忆卡中，缺单一入口

## 与计划的分工（为什么不是 plan3）

| | 计划（plan1 / plan2） | 本台账 |
|---|---|---|
| 前置 | 该轮设计已**按实施视角收窄**、决策已拍板 | 无——任何时点可登记 |
| 内容 | Task / Step / 文件清单 / 接口草稿 / 验收信号 | 事项 + 归属 + 来源锚点 + 状态，**不含步骤** |
| 生命周期 | 交付即冻结（历史留痕） | 活文档：轮次开工通读 → 折进该轮计划 → 收束勾销 |
| 数量 | 一轮一份 | 全项目一份 |

**plan3 未来一定会存在**——它应该是 R4 轮（M3 状态/Buff）的实施计划，前置动作是"把 `03-module-states.md` 按实施视角收窄 + 补齐该轮输入增补"。遗留项不构成一份计划的内容。

## 入册判据（三条全中才入册）

1. 决策已拍板——MUST 带来源锚点（文档 + 行号 / 决策编号）
2. 代码未落地、未验证、或细节未定稿——MUST 带现状证据（已核）
3. **不在任何现有计划的 Task 里**，也不是"某轮整模块范围"的粗粒度内容（后者归模块地图轮次表）

**无归属的条目禁止入册**：归属 = 轮次（R4–R8）或触发条件（"等第一个 X 消费者"）。

## 纪律

- **轮次开工时通读一遍**：把该轮条目折进该轮计划的输入增补段；收束时标「已消费（日期 + 落点）」，**条目不删除**（留痕）。
- **"已定不做 + 否决理由"不抄进来**：它们已住决策文档的"被否方案与理由"表（如 D2-16 那张五项表）。本台账只装"要做的"，不装"不做的"。
- **有 Task 归属的不入册**：例如"删除两个 `Private/Testing/` 临时夹子"已写在 plan1 Task 6，重复登记只会制造两处真相。
- 与记忆卡的分工：本台账随仓库版本走（项目内事实）；记忆卡是跨项目方法论（`~/.agents/memory`）。

---

## R4 轮（M3 状态/Buff = TcsState）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R4-1 | `FTcsParamEnumerableSource`（PV-10 源可枚举能力：`Enumerate` / `GetIndexForLevel`，索引解析唯一真相在源）；落地随 **TcsState 等级源**同批 | `01-module-m0-core.md:22 ④`；`2026-09-14-param-fold-and-display-decision-points.md:98` | 代码与规格中均无（已 grep `Source/` 与 `openspec/specs/param-value/`）；当初明记"不进 plan1 Task 0——零消费者不预建" | 待触发（R4 开工） |
| R4-2 | `FTcsParamEvaluateContext` 补 `Subject`（`FCombatEntityHandle`）+ `EffectiveLevel`（int32）；落地随 TcsState 等级源同批 | `01-module-m0-core.md:22 ②`；`plan1:543` 偏差 1（句柄升格反射 USTRUCT 后 Subject 解禁） | 上下文中无这两个字段（`Source/TcsCore/Public/Parameter/TcsParamValueSource.h:15-18`）；**且该头注释仍写"随 M2/M5 轮补齐"——M2 已收束，注释过期，口径待统一为"随 TcsState 等级源"** | 待触发（R4 开工） |

> 两项的共同锚点 = `ITcsEntityLevelProvider` 定义于 TcsState（D3-11 修订：State/Instigator × Array/Map 四源归 TcsState）——该接口属 R4 轮自身范围，不单列条目，此处登记为确保 R4-1/R4-2 的触发点不丢。

## R1 后续（M0 总线补全）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| M0-1 | **原生侧层级匹配**（父标签订阅：订阅 `Tcs.Event.Attribute` 收全属性域事件）+ 订阅索引结构选型（`TMultiMap` vs 按 Tag 分桶 vs 层级树缓存） | `01-module-m0-core.md:29`（2026-09-18 记录，M0 轮排期；由 2026-08-31 总线文档开放项升格为"三层命名的配套前置"） | 未落地：`FTcsEventBus::DispatchToChannel` 用 `TagIndex.MultiFind(Tag, ...)`，原生派发是**精确匹配**；BP/CS 动态层已支持部分匹配（`MatchesTag`） | 待触发（原生消费者需要全域订阅时；最近机会 = R5 M4a 触发行。本域事件数少，按叶子逐条订阅可接受） |

## R5 轮（M4a 触发行 / M4c 目标选择）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R5-1 | **Context 默认目标初始化 = 事件目标**（D4-4 v2：起链时把事件载荷里的目标填进 `Context.Targets`——"单步链零 SelectTargets 直接消费"的成立条件）；含**载荷 → 目标**的解析通路（载荷类型所属模块实现、机制层调用） | `04-module-effects.md:27`（D4-4 v2 改口）；`2026-09-02-m4-effects-decision-points.md:83`；plan2 Task 1 落地注记（2026-09-18） | R3 只落"调用方预填"——`FTcsEffectContext` 的 `Targets` / `EventPayload` 字段与门面 `ExecuteChain(ChainId, Context)` 已就位，但**载荷 → 目标的解析通路未实现**（R3 无事件触发源、无法实证；TcsEffect 侧没有任何载荷类型可作样本） | 待触发（R5 开工——触发行求值器落地时同批） |

## R6 轮（M5 技能/施法）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R6-1 | **参数链 / 流程属性黑板接入 `FoldTcsAttributeBands`**（D5-5 v3"折叠器单份、三处共用"：M2 属性 / M5 参数链 / TcsDamage 流程属性） | `plan1:550` 增补 8；折叠器已落地于 `Public/Attribute/TcsAttributeBandFold.h` | 折叠器与五带口径已随 Task 5 交付；**消费者尚未接入**（plan2 Task 3 `FTcsFlowAttributes` 的"封闭五带运算（同 M2）"是自然首用点，但未写成显式步骤） | 待触发（就近 = plan2 Task 3；远期 = R6 M5 参数账本） |

## R7 轮（M6 集成）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R7-1 | **AttributeSet（D2-15）全套**：`UTcsAttributeSet*` 资产（`TArray<FName> DefIds`，覆写列首版不做）、实体侧引用点（实体 Def / Actor BP 组件配置）、World 级"当前用哪套"状态、`ApplyAttributeSet`（diff 替换：旧独有移除 / 新独有添加 / 共有保留实例）/ `ClearAttributeSet`、施加路径穿 `IsRuntimeReady()` 门禁（入口在 `RegisterEntity` 之后） | `2026-09-17-attribute-set-and-existence-decision-points.md:19-25`（形态 B1a+B2+B3a）、`:195`（为何同 M6 轮：依赖 DefLibrary 按类型发现/加载 + 实体注册路径） | 未落地；**Task 5 已交付其最贵的前置**（冻结/解冻 = "共有属性保留实例"语义）。**花名册一致性**：Set 必须自洽——含动态边界引用（如 `Health` 的 `Max = Dynamic(MaxHealth)`）时，被引用属性须在同一 Set 内（否则命中 R8-2 的静默钳 0 路径） | 待触发（R7 开工） |
| R7-2 | **AttributeSet 资产命名统一**：`UTcsAttributeSetAsset` vs「Def 族去 Asset 后缀」标准 | 冲突双方：`02-module-attributes.md:54`（写 `UTcsAttributeSetAsset`）vs `plan1:484 ⑥`（Def 命名标准 = `<族>Def`，如 `UTcsAttributeDef`） | 文档口径冲突未解 | 待触发（随 R7-1 动代码前拍板） |

## R8 轮（M7 表现 + M8 编辑器与工具）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R8-1 | **PV-8 四联校验矩阵**：`源类型 × 上下文类型 × 可配约定（D5-18 v3）× 视图兼容（D5-17 v3 `IsCompatible(Probe)`）` | `01-module-m0-core.md:22 ⑤`（**M8 显式交付物**）；`2026-09-14-param-fold-and-display-decision-points.md:125` | 未落地 | 待触发（R8 开工） |
| R8-2 | **定义校验矩阵兜住作者侧配置错误**：①`AVD_Wrap` 未给跨度（两侧边界齐备且 `Max > Min` 才回卷，否则返回聚合原值；热路径不拦，留给校验器）；②**动态边界引用的属性在某单位上不存在**——定义是**按属性名全局共享**的一行，其上界 `ABM_Dynamic(X)` 要求该单位也持有 `X`，否则解析读到 0 → 值被静默钳到 0 | `plan1:549` 语义澄清 7（①）；`plan1:547` 偏差 5 + **2026-09-18 Task 6 首轮 PIE 实测（②，真实案例）** | 运行期行为已定稿并落地；**校验侧未做**（②的实测现场：装置单位 B 只添加了 `Health` 而缺 `MaxHealth` → `Health` 100→0，判为夹具缺陷；但同一路径在真实内容里就是静默错值） | 待触发（R8 开工） |
| R8-3 | **Def 双轨同步器**（资产为权威，编辑器侧维护两轨一致性）+ 词表装载；运行期零 DataTable 加载路径 | `plan1:482` 三次复评；`02-module-attributes.md`（同步器与词表装载属 M8） | 未落地 | 待触发（R8 开工） |
| R8-4 | **K2 强类型引脚节点**（总线动态监听层） | `01-module-m0-core.md:30`（未来项，M8 轮） | 未落地 | 待触发（R8 开工） |
| R8-5 | **总线动态层两条验证项**：BP InstancedStruct 节点面版本覆盖、CS 读 FInstancedStruct 载荷实测 | `01-module-m0-core.md:30`（不阻塞 R3） | 未验证 | 待验证 |

## 触发条件型（不绑轮次）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| T-1 | `FTcsAttributeChangedEvent` 加 `Reason` 字段（区分 BaseValue / CurrentValue 变更） | `02-module-attributes.md:94` | 未落地（2026-09-18 拍板"先不加——等第一个真实消费者"）；事件语义已定为"当前值变了"（结果事件） | 等第一个真实消费者 |
| T-2 | **Mass 适配**：新建 `TcsMass` 模块（适配器 + 生成/回收 processor），核心零改动 | `06-module-integration.md:33`（移出当前范围；D3-1 注册表 Actor 无关性保留为未来前提） | 未落地 | 等宿主需要小兵路径 |
| T-3 | **复制契约实现** `ICombatReplicationProxy`（`InjectOperation(操作流)` / `ExportSnapshot()`） | `06-module-integration.md:35-36`（仅契约不实现，实现推迟到有联网项目） | 仅契约草案 | 等有联网项目 |
| T-4 | 总线动态层**类型化委托**（增量） | `01-module-m0-core.md:30`（"哪个事件用得痛再单独加"） | 未落地 | 等痛点出现 |

---

## 变更记录

- **2026-09-18 建立**：首版 **15 条**（R4×2 / M0×1 / R6×1 / R7×2 / R8×5 / 触发条件×4）。来源 = R3 plan1 Task 5 收束后的全库调研（plan1 / plan2 / 01 / 02 / 06 / 2026-09-14 / 2026-09-17 决策文档逐条核对）。核对中查出两处文档口径不一致：`TcsParamValueSource.h:15` 的"M2/M5 轮"已过期（M2 已收束）、`02:54` 的 `UTcsAttributeSetAsset` 与 Def 命名标准冲突——分别登记为 R4-2 与 R7-2。
- **2026-09-18 增补（Task 6 首轮 PIE 实测）**：R8-2 加第二类案例（动态边界引用的属性缺失 → 静默钳 0，附实测现场），R7-1 加"Set 花名册一致性"约束。条目总数不变（扩写既有条目，不新开）。
- **2026-09-18 增补（plan2 Task 1 收束）**：新增 **R5-1**（Context 默认目标初始化 = 事件目标——R3 只落"调用方预填"，载荷 → 目标的解析通路无事件触发源可实证，随触发行轮落地），新开 R5 轮区段。条目总数 **16**。**不入册（有 Task 归属，判据第三条）**：链资产类 `UTcsEffectChainDef` 的落点、`UTcsEntityQuery` U 类名与计划 Task 5 实现类名撞名——两项均写入 plan2 Task 5/6 交接注记。
