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
| R4-3 | **`FFlowRedirect` 模板重定向栈**（D7-7：状态/装备声明换流程——让渡点 + 重定向栈后挂/高优先 + Source 级联回收；三粒度让渡模式升四粒度） | `09-module-damage.md:28`（D7-7）；plan2 Task 3/4 均列为非目标（"随 M3 状态轮"） | 未落地（2026-09-21 plan2 Task 4 实施时确认：流程机制层与步骤库均无重定向入口） | 待触发（**归属 = R4/M3 状态轮**——消费者是"状态/装备声明换流程"，无状态模块即无场景） |

> 两项的共同锚点 = `ITcsEntityLevelProvider` 定义于 TcsState（D3-11 修订：State/Instigator × Array/Map 四源归 TcsState）——该接口属 R4 轮自身范围，不单列条目，此处登记为确保 R4-1/R4-2 的触发点不丢。

## R1 后续（M0 总线补全）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| M0-1 | **原生侧层级匹配**（父标签订阅：订阅 `Tcs.Event.Attribute` 收全属性域事件）+ 订阅索引结构选型（`TMultiMap` vs 按 Tag 分桶 vs 层级树缓存） | `01-module-m0-core.md:29`（2026-09-18 记录，M0 轮排期；由 2026-08-31 总线文档开放项升格为"三层命名的配套前置"） | 未落地：`FTcsEventBus::DispatchToChannel` 用 `TagIndex.MultiFind(Tag, ...)`，原生派发是**精确匹配**；BP/CS 动态层已支持部分匹配（`MatchesTag`） | 待触发（原生消费者需要全域订阅时；最近机会 = R5 M4a 触发行。本域事件数少，按叶子逐条订阅可接受） |

## R5 轮（M4a 触发行 / M4c 目标选择）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R5-1 | **Context 默认目标初始化 = 事件目标**（D4-4 v2：起链时把事件载荷里的目标填进 `Context.Targets`——"单步链零 SelectTargets 直接消费"的成立条件）；含**载荷 → 目标**的解析通路（载荷类型所属模块实现、机制层调用） | `04-module-effects.md:27`（D4-4 v2 改口）；`2026-09-02-m4-effects-decision-points.md:83`；plan2 Task 1 落地注记（2026-09-18） | R3 只落"调用方预填"——`FTcsEffectContext` 的 `Targets` / `EventPayload` 字段与门面 `ExecuteChain(ChainId, Context)` 已就位，但**载荷 → 目标的解析通路未实现**（R3 无事件触发源、无法实证；TcsEffect 侧没有任何载荷类型可作样本） | 待触发（R5 开工——触发行求值器落地时同批） |
| R5-2 | **TcsEffect 剩余 8 原语整套**：`WaitEvent`（一次性事件订阅挂起 + Payload 写 `LastEvent`）/ `Branch` / `Parallel`（默认不汇合，`bJoin=true` 时 `JoinCount` 计数）/ `Repeat`（带单链单帧熔断）/ `RunSubChain`（默认等待子链完成唤醒父，`bWait=false` 放支线）/ `ModifyAttribute`（机制私有写入原语）/ `SetVar` / `OnError`（软失败接管，默认断链 + 日志 + Explain 线索） | `04-module-effects.md:15`（D4-16 的 15 原语，TcsEffect 9 个）/`:34`（四种唤醒源）/`:41`（解释器行为）/§12 验收钩子（"WaitEvent 挂起唤醒、熔断与打断在竖切后人工检查路径"）；plan2 `:18` 明文"R3 只实现 WaitDelay/SelectTargets/Damage" | 未落地：`Source/TcsEffect/` 只有 `FTcsStepWaitDelay`（2026-09-21 逐处核过 plan1/plan2/台账/README 轮次表/剧本，其余 8 个原语**零命中**——无任何轮次或触发条件认领） | 待触发（**归属 = R5/M4a effects 轮**——触发行求值器与唤醒源同批；其中 `RunSubChain`/`Parallel` 的子链汇合语义与 M5 编排（R6）同批验证；`OnError` 与求值器失败路径同批） |
| R5-3 | **TcsDamage 的 `Heal` / `ModifyFlow` 执行器**：步骤 struct 与 `Damage` 同批定义（`Chain/TcsStepDamage.h` 三 struct），但 R3 只实现 `Damage` 执行器 | plan2 `:67` 原话"**R3 只实现 Damage 执行器，Heal/ModifyFlow 执行器后续轮**"（未指明哪一轮）；`09-module-damage.md:56`（治疗流程 = 同骨架精简版）；`:58`（ModifyFlow = 修改器唯一通道 D7-6 的提交原语） | 未落地：R3 只落 `FTcsStepDamage` 执行器；`ModifyFlow` 是"伤害修改器"通道的关键一环（D7-6），无它则修改器只能靠宿主自定义步骤提交 | 待触发（**归属 = R5/M4a**——与触发行（修改器的订阅方）同批；治疗可按需后置） |

## R6 轮（M5 技能/施法）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R6-1 | **参数链 / 流程属性黑板接入 `FoldTcsAttributeBands`**（D5-5 v3"折叠器单份、三处共用"：M2 属性 / M5 参数链 / TcsDamage 流程属性） | `plan1:550` 增补 8；折叠器已落地于 `Public/Attribute/TcsAttributeBandFold.h` | **已消费（2026-09-20，plan2 Task 3）**——落点 = TcsDamage 流程属性黑板 `Private/Flow/TcsFlowAttributes.cpp` 的 `Read`（读时求值 → 摊平 → 调共享纯函数；自检：`Source/TcsDamage/` 仅此一处折叠调用，无私建第二份）。**剩 M5 参数链一处**，随 R6 轮 | 部分消费（M5 侧待 R6 开工） |

## R7 轮（M6 集成）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R7-1 | **AttributeSet（D2-15）全套**：`UTcsAttributeSet*` 资产（`TArray<FGameplayTag> DefTags`，覆写列首版不做；**2026-09-22 tag 化：原 `TArray<FName> DefIds`**）、实体侧引用点（实体 Def / Actor BP 组件配置）、World 级"当前用哪套"状态、`ApplyAttributeSet`（diff 替换：旧独有移除 / 新独有添加 / 共有保留实例）/ `ClearAttributeSet`、施加路径穿 `IsRuntimeReady()` 门禁（入口在 `RegisterEntity` 之后） | `2026-09-17-attribute-set-and-existence-decision-points.md:19-25`（形态 B1a+B2+B3a）、`:195`（为何同 M6 轮：依赖 DefLibrary 按类型发现/加载 + 实体注册路径） | 未落地；**Task 5 已交付其最贵的前置**（冻结/解冻 = "共有属性保留实例"语义）。**花名册一致性**：Set 必须自洽——含动态边界引用（如 `Health` 的 `Max = Dynamic(MaxHealth)`）时，被引用属性须在同一 Set 内（否则命中 R8-2 的静默钳 0 路径） | 待触发（R7 开工） |
| R7-2 | **AttributeSet 资产命名统一**：`UTcsAttributeSetAsset` vs「Def 族去 Asset 后缀」标准 | 冲突双方：`02-module-attributes.md:54`（写 `UTcsAttributeSetAsset`）vs `plan1:484 ⑥`（Def 命名标准 = `<族>Def`，如 `UTcsAttributeDef`） | 文档口径冲突未解 | 待触发（随 R7-1 动代码前拍板） |
| R7-3 | **`PrimaryAssetTypesToScan` 注册 + 发现机制切换 AssetManager + 加载层三策略/异步**：①把 Def 资产族（链资产 + 后续各 Def 族）注册进 `[/Script/Engine.AssetManagerSettings]`；②DefLibrary 的发现从 `IAssetRegistry::GetAssetsByClass` 换成引擎官方按类型路径（`UAssetManager::GetPrimaryAssetIdList` / `GetPrimaryAssetPath`）；③**加载层三策略（PreloadAll/PreloadSelected/OnDemand）+ 异步默认 / 同步逃生口**（引擎 API 已核实齐备：`LoadPrimaryAssets` 异步返 `FStreamableHandle` `AssetManager.h:326` / `LoadPrimaryAsset` `:340` / `LoadPrimaryAssetsWithType`（PreloadAll）`:354` / `PreloadPrimaryAssets` `:535` / `FStreamableManager::LoadSynchronous`（同步口）`StreamableManager.h:800`）；④DefLibrary 状态机升级为带 `Loading` 态的完整版（`Unloaded→Loading→Ready/Failed`，03 §67 已定稿——R3 只有同步单出口） | `openspec/project.md:16`（"Registering the types in `PrimaryAssetTypesToScan` belongs to the M6 DefLibrary round"）；`03-module-states.md:25` / `08-module-editor-tooling.md:37` / `plan1:476`（三处同口径"属 M6/M8 轮"）；`2026-09-17-attribute-set-and-existence-decision-points.md:195`（AttributeSet 依赖"Def 资产按类型发现/加载（`PrimaryAssetTypes` 注册）"）；**`06-module-integration.md:17`**（"加载（六域三策略：PreloadAll/PreloadSelected/OnDemand）"）与 `:41`（入口服务 `LoadAll/LoadSelected/EnsureLoaded(DefTag)`——**2026-09-22 tag 化**）；**用户 2026-09-21 拍板**（加载层归 R7-3，不在 Task 5 引入异步） | **Task 5 已交付前置**（Def 身份标准落地：显式 `PrimaryAssetType` + `GetPrimaryAssetId()` 覆写；发现走 AssetRegistry 按类扫描；加载用同步 `FAssetData::GetAsset()`）。**未落地**：`PrimaryAssetTypesToScan` 在插件 `Config/`、宿主项目 `Config/`、代码中**全库零命中**（2026-09-21 已核）；`AssetManagerSettings` 零命中。**已核引擎语义**：未注册时 `GetPrimaryAssetId()` 仍正常（纯函数，`DataAsset.cpp:73-122`），但 `GetPrimaryAssetIdList` / `GetPrimaryAssetPath` **静默返回空**（`AssetManager.cpp:2134-2152`，无 log/ensure）——**这是"加载层必须先注册"的硬前提**（`LoadPrimaryAssets*` 族按 `FPrimaryAssetId` 工作）；`PrimaryAssetTypesToScan` 声明见 `AssetManagerSettings.h:79-81`，消费点 `AssetManager.cpp:3867-3899`。**待实测**：插件自带 `Config/DefaultGame.ini` 是否被引擎自动加载（Task 5 无 Config 目录） | 待触发（R7 开工，与 R7-1 同批——AttributeSet 依赖它） |

## R8 轮（M7 表现 + M8 编辑器与工具）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| R8-1 | **PV-8 四联校验矩阵**：`源类型 × 上下文类型 × 可配约定（D5-18 v3）× 视图兼容（D5-17 v3 `IsCompatible(Probe)`）` | `01-module-m0-core.md:22 ⑤`（**M8 显式交付物**）；`2026-09-14-param-fold-and-display-decision-points.md:125` | 未落地 | 待触发（R8 开工） |
| R8-2 | **定义校验矩阵兜住作者侧配置错误**：①`AVD_Wrap` 未给跨度（两侧边界齐备且 `Max > Min` 才回卷，否则返回聚合原值；热路径不拦，留给校验器）；②**动态边界引用的属性在某单位上不存在**——定义是**按属性名全局共享**的一行，其上界 `ABM_Dynamic(X)` 要求该单位也持有 `X`，否则解析读到 0 → 值被静默钳到 0 | `plan1:549` 语义澄清 7（①）；`plan1:547` 偏差 5 + **2026-09-18 Task 6 首轮 PIE 实测（②，真实案例）** | 运行期行为已定稿并落地；**校验侧未做**（②的实测现场：装置单位 B 只添加了 `Health` 而缺 `MaxHealth` → `Health` 100→0，判为夹具缺陷；但同一路径在真实内容里就是静默错值） | 待触发（R8 开工） |
| R8-3 | **Def 双轨同步器**（资产为权威，编辑器侧维护两轨一致性）+ 词表装载；运行期零 DataTable 加载路径。**2026-09-22 tag 化改造后收窄**：原承诺之一"**行名 ↔ 常量映射校验**"**作废**（tag 方案下无"行名"概念——RowName 降为编辑期定位、`DefTag` 才是内容身份；一致性由本同步器维护而非"映射校验"）；`FAttributeRegistry` 的 `Resolve(FName) → 稠密 id` 改述为按 tag 解析 | `plan1:482` 三次复评；`02-module-attributes.md`（同步器与词表装载属 M8）；**2026-09-22 提案 `switch-identifiers-to-gameplay-tags`** | 未落地 | 待触发（R8 开工） |
| R8-4 | **K2 强类型引脚节点**（总线动态监听层） | `01-module-m0-core.md:30`（未来项，M8 轮） | 未落地 | 待触发（R8 开工） |
| R8-5 | **总线动态层两条验证项**：BP InstancedStruct 节点面版本覆盖、CS 读 FInstancedStruct 载荷实测 | `01-module-m0-core.md:30`（不阻塞 R3） | 未验证 | 待验证 |
| R8-6 | **Explain（调试面板 + 钩子）**：策划视角的"这个数怎么算出来的"——数据源 = M0/M2/M4 已埋的 Explain 钩子（D0-6 日志分类 / D2-5 flush / D4-1 静默标记），面板只是消费者 | `08-module-editor-tooling.md:25-31`（§4 Explain 与调试面板）；`04-module-effects.md:43`（`OnError` 的"Explain 线索"）；`09-module-damage.md:73`（非目标明文："不做生产期过程级溯源——'这个数怎么算出来的'走 M8 Explain 开发期回放"） | 未落地（2026-09-21 用户质询"策划怎么看 InputDmg 与 FinalDmg 的差别"时逐处核过：`08 §4` 有定义、无任何计划 Task 认领）。**R3 的临时替代**：入库命令 `Tcs.Damage.DumpRecords`（打印环形缓冲的 Base/Final/差额/Executed）+ `AppendRecord` 的 Log 级记录行 | 待触发（**归属 = R8/M8 轮**） |

## 触发条件型（不绑轮次）

| # | 事项 | 来源锚点 | 现状证据（已核） | 状态 |
|---|---|---|---|---|
| T-1 | `FTcsAttributeChangedEvent` 加 `Reason` 字段（区分 BaseValue / CurrentValue 变更） | `02-module-attributes.md:94` | 未落地（2026-09-18 拍板"先不加——等第一个真实消费者"）；事件语义已定为"当前值变了"（结果事件） | 等第一个真实消费者 |
| T-2 | **Mass 适配**：新建 `TcsMass` 模块（适配器 + 生成/回收 processor），核心零改动 | `06-module-integration.md:33`（移出当前范围；D3-1 注册表 Actor 无关性保留为未来前提） | 未落地 | 等宿主需要小兵路径 |
| T-3 | **复制契约实现** `ICombatReplicationProxy`（`InjectOperation(操作流)` / `ExportSnapshot()`） | `06-module-integration.md:35-36`（仅契约不实现，实现推迟到有联网项目） | 仅契约草案 | 等有联网项目 |
| T-4 | 总线动态层**类型化委托**（增量） | `01-module-m0-core.md:30`（"哪个事件用得痛再单独加"） | 未落地 | 等痛点出现 |
| T-5 | **`IRelationResolver` 阵营判定注入契约**（`IsHostile` / `IsFriendly(来源, 目标)`——设计名，与 `ICombatEntityQuery` 同族的宿主能力契约） | `10-module-targeting.md:31`（§2.3 注入接口）/`:26`（宿主 Filter 内部可调）；plan2 全篇未点名落点（已核） | 未落地——**R3 零消费者**：框架零默认 Filter，竖切的过滤器由测试装置自带判定，不需要阵营契约 | 等第一个需要阵营判定的宿主实现出现（最近机会 = M6 宿主适配轮，或 LAC 首个敌对判定需求） |
| T-6 | **来源发号器统一为进程唯一**：`FTcsSourceHandleRegistry` 可实例化，属性侧装置与 TcsDamage 门面各持一份 → **两个分配器的 Id 空间重叠**，"按来源级联摘除"可能误摘他人来源 | plan2 Task 3 实施注记（2026-09-20）；`TcsCore/Public/Handle/TcsSourceHandle.h`（类可实例化） | 未落地——R3 各分配器用途分离、无实际碰撞场景；**流程侧已用门面内实例**（`UTcsDamageSubsystem::FlowSourceRegistry`） | 等 M6 宿主适配轮（届时多来源并存：状态/装备/流程/技能冷却） |
| T-7 | **AttrCapture（属性捕获）整套**：`ETcsAttrCaptureFrom{Instigator/Target}` + FlowStart 捕获填充 `CapturedAttrs` 快照 + "读默认 Live、命中读快照"的读取语义（09 §2.1"流程内读取一致性"；"本次攻击攻击力 +10%"的双路径之一） | `09-module-damage.md:23`；plan2 Task 4 提案顺延节（2026-09-21 实施时确认） | 未落地：`FTcsDamageFlowContext.CapturedAttrs` 字段已就位，但**无填充者、无读取者**（R3 竖切只有字面量 `DamageBase` 链，无属性修正型修改器） | 等 M5 技能账本轮，或第一个属性修正型修改器出现 |
| T-8 | **流程模板资产化**：`UTcsDamageFlowTemplateDef : UPrimaryDataAsset`（与链资产同形：`{ FGameplayTag TemplateTag; FTcsDamageFlowTemplate Template; }` + 显式 `PrimaryAssetType`（**值 = 类名** `"TcsDamageFlowTemplateDef"`——族内一致，2026-09-21 标准收口）+ 覆写 `GetPrimaryAssetId()` 名取 `TemplateTag.GetTagName()`），DefLibrary 发现并登记进 `UTcsDamageSubsystem`；**先改名**：`FTcsFlowTemplate` → `FTcsDamageFlowTemplate`（现名 `Flow` 段语义太泛，模块内 `Flow*` 类型成堆） | `09-module-damage.md:27`（"**多预设 = 多模板资产**（不同游戏模式/角色配不同模板）。模板选择链：Damage 步骤配置指定 → Def 覆盖 → 全局默认"——**设计明文称"模板资产"**，从未被任何 Task 认领）；用户 2026-09-21 拍板方向（"可以走资产化"）+ 改名要求 | **未落地**：全库 `FlowTemplateDef` / `DamageFlowTemplate` 零命中（2026-09-21 已核）。R3 现状 = 默认模板在 `UTcsDamageSubsystem::Initialize` **C++ 硬编码组装**（`TcsDamageSubsystem.cpp:46-51`，四步 CollectStart→BaseDamage→Execute→Completed）。**模板是"句子"（合法资产化）**——与"步骤是词汇（不可资产化）"分属不同层（R0 §8 锋利边界）。**✅ GC 面已修（2026-09-23）**：登记表 `TMap<FGameplayTag, TUniquePtr<FTcsFlowTemplate>>` 非 UPROPERTY 的地雷，已用 `UTcsDamageSubsystem::AddReferencedObjects` 静态 ARO 覆写 + `AddPropertyReferencesWithStructARO` 逐模板补引用（链侧 `UTcsEffectSubsystem` 同批同款）；规格已并入（`damage-flow` / `effect-chain` 各加"对 GC 可见"要求 + 场景）。提案 `fix-registry-gc-visible-holding` → 归档 `2026-09-22-fix-registry-gc-visible-holding`。**⚠️ 未实证**：GC 场景（无其他强引用的 delegate + 手动 `obj gc`）**未跑**——宿主当前用 `UPROPERTY` 强引用绕过（现成冗余保险），R3 无合适夹具；成立依据是引擎源码机制（`AddPropertyReferencesWithStructARO` 递归进 `FInstancedStruct` 内层，`InstancedStruct.cpp:506`）而非实测。**资产化落地后本问题自然消解**（资产 root 后 ARO 覆写成为冗余但无害） | 等第一个真实内容需求（策划要自定义流程顺序 / 多游戏模式多模板）；改名可随该轮一并做，或提前单独做 |
| T-9 | **跨模块消费面的导出宏存量审计**：公共头里的**非内联符号**若被其他模块（宿主/兄弟模块）引用，声明处 MUST 带模块导出宏（`TCS<模块>_API`）——否则链接失败。**2026-09-21 Task 6 实证两处（已修）**：①原生 Tag（`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**，`NativeGameplayTags.h:31` 无 dllexport）→ 11 个框架事件 Tag 已补导出宏；②`FTcsFlowAttributes` / `FTcsFlowAttributeSubmit`（宿主自研流程步骤的公共调用面）→ 已补 `TCSDAMAGE_API`。**未补的存量面**（纯内联或仅同模块使用，暂安全）：`FTcsDamageFlowContext` / `FTcsConsumePolicy` / `FTcsAttributeStore` / `FTcsAttributeInstance` / `FTcsAttrModOperand` / `FTcsClock` / `FTcsEventBus` 族等 | plan2 Task 6 实施注记②（2026-09-21）；`NativeGameplayTags.h:31`；实测 `LNK2001` / `LNK2019` | **根因**：R3 之前**从无跨模块消费者**（宿主是第一个），故这类"声明缺导出宏"的问题从未暴露。已修的两类是"设计意图明确要供宿主使用"的（事件 Tag 供订阅、黑板供宿主步骤读写） | 随**首个跨模块消费者**出现逐个补（判据 = 该符号是否被跨模块引用）；M6 宿主适配轮可做一次全量审计 |
| T-10 | **跨资产 id 引用的作者侧存在性校验**：六类引用点（`FlowTemplateId` / `ParamRef::Key` / 属性名 / 黑板键 / `TemplateId` / `DefId`）在 tag 化后，**拼写/存在性由 `FGameplayTag::IsValid()` 与 `RequestGameplayTag` 的 ensure 部分覆盖**（构造/解析处即暴露）；但**"已注册却指向不存在实体"**（如合法 tag 但无对应模板）仍无校验——该面归 M8 校验矩阵 | 用户 2026-09-22 指出（"所有配置都在 Def 里…都需要开发者手动填写 FName，这样很容易出事故"）；`08-module-editor-tooling.md:37`（"未登记的 DefId = 保存期报错"——**设计已承诺、R8 未落地**）；台账 R8-1/R8-2 记的面均**未覆盖"id 引用存在性"这一类** | **部分覆盖**（tag 化的直接收益）；剩余面未落地 | 待触发（R8 开工） |

---

## 变更记录

- **2026-09-18 建立**：首版 **15 条**（R4×2 / M0×1 / R6×1 / R7×2 / R8×5 / 触发条件×4）。来源 = R3 plan1 Task 5 收束后的全库调研（plan1 / plan2 / 01 / 02 / 06 / 2026-09-14 / 2026-09-17 决策文档逐条核对）。核对中查出两处文档口径不一致：`TcsParamValueSource.h:15` 的"M2/M5 轮"已过期（M2 已收束）、`02:54` 的 `UTcsAttributeSetAsset` 与 Def 命名标准冲突——分别登记为 R4-2 与 R7-2。
- **2026-09-18 增补（Task 6 首轮 PIE 实测）**：R8-2 加第二类案例（动态边界引用的属性缺失 → 静默钳 0，附实测现场），R7-1 加"Set 花名册一致性"约束。条目总数不变（扩写既有条目，不新开）。
- **2026-09-18 增补（plan2 Task 1 收束）**：新增 **R5-1**（Context 默认目标初始化 = 事件目标——R3 只落"调用方预填"，载荷 → 目标的解析通路无事件触发源可实证，随触发行轮落地），新开 R5 轮区段。条目总数 **16**。**不入册（有 Task 归属，判据第三条）**：链资产类 `UTcsEffectChainDef` 的落点、`UTcsEntityQuery` U 类名与计划 Task 5 实现类名撞名——两项均写入 plan2 Task 5/6 交接注记。
- **2026-09-20 增补（plan2 Task 2 收束）**：新增 **T-5**（`IRelationResolver` 阵营判定注入契约——设计 §2.3 点名、plan2 无 Task 认领、R3 零消费者）。条目总数 **17**。**同批确认**：`FTcsSelSelf` / `FTcsSelEventTarget` 两个默认选择器的"后置"是**用户拍板的 R3 范围收窄**（不是遗漏），已写入 plan2 Task 2 注记，不入台账（有 Task 归属 = 该 Task 自身）。
- **2026-09-20 增补（plan2 Task 3 收束）**：**R6-1 部分勾销**（流程属性黑板侧已消费——落点 `TcsFlowAttributes.cpp::Read`；剩 M5 参数链一处随 R6 轮）；新增 **T-6**（来源发号器应进程唯一——`FTcsSourceHandleRegistry` 可实例化造成 Id 空间重叠面）。条目总数 **18**。
- **2026-09-21 增补（用户质询"TcsEffect 其余 Step 的落地规划在哪"）**：新增 **R5-2**（TcsEffect 剩余 8 原语整套——控制流 6 + `ModifyAttribute`/`SetVar`/`OnError`）与 **R5-3**（TcsDamage 的 `Heal`/`ModifyFlow` 执行器）。**这是一处台账遗漏的补救**：建台账时只核对了 plan1/plan2/01/02/06 与几份决策文档，**未把 `04 §2.1` 的原语清单逐条对照**——而这正是"决策已拍板、代码未落地、不在任何计划 Task 里"的教科书案例（其余 8 个原语在 plan1/plan2/README 轮次表/剧本中零命中）。条目总数 **20**。
- **2026-09-21 增补（plan2 Task 4 收束）**：新增 **R4-3**（`FFlowRedirect` 模板重定向栈，D7-7——归属 M3 状态轮）与 **T-7**（AttrCapture 整套——字段已就位、无填充者与读取者，等 M5 账本或第一个属性修正型修改器）。条目总数 **22**。
- **2026-09-21 增补（用户质询"策划怎么看 InputDmg 与 FinalDmg 的差别"）**：新增 **R8-6**（M8 Explain 调试面板 + 钩子——设计有定义、无 Task 认领；R3 以入库命令 `Tcs.Damage.DumpRecords` 作临时替代）。条目总数 **23**。
- **2026-09-21 增补（Task 5 前置讨论：加载层与流程模板）**：新增 **R7-3**（`PrimaryAssetTypesToScan` 注册与发现机制切换 AssetManager——含**加载层三策略 + 异步默认/同步逃生口**，用户拍板归 R7-3）与 **T-8**（流程模板资产化 + 改名 `FTcsFlowTemplate` → `FTcsDamageFlowTemplate`，用户拍板方向）。条目总数 **25**。**同批确认不入册**：链资产载体形态（已有 Task 5 归属）、加载层归属（本次拍板已写入提案与 R7-3）。
- **2026-09-21 增补（用户报告"链资产到底用来干嘛"的困惑）**：**不新开条目**——该困惑暴露的是"两个解释器两套资产"缺少一份对照说明，已写入 plan2 Task 5 前置讨论注记（链资产 = 效果链句子；流程模板 = 伤害结算句子；两者由 Damage 链步骤串联）。
- **2026-09-21 增补（plan2 Task 6 实施）**：新增 **T-9**（跨模块消费面的导出宏存量审计——两处已修、存量面随首个消费者逐个补）；**T-8 追加约束**（模板登记表非 GC 可见持有 → 与资产化同批解决）；**R7-1 追加范围**（AttributeSet 全套含 `ApplyAttributeSet`/`ClearAttributeSet`，用户 2026-09-21 Task 5 收束时拍板归 R7）。条目总数 **26**。**同批登记（不入册）**：plan1 检查点 2/3/4 的回归网随六套临时装置退役消失（有 Task 归属 = Task 7 端到端验收轮，且结论已随 plan1 归档）。
- **2026-09-22 增补（标识体系 tag 化改造）**：**不新开条目**——六类引用点的作者侧存在性缺口归入既有的 **T-10**（原"部分覆盖"结论在改造后写入该条）；**R7-1 / R7-3 / R8-3 / T-8 的条目描述随改造更新为 tag 口径**（`DefIds` → `DefTags`、`Resolve(FName)` → 按 tag 解析、`TemplateId` → `TemplateTag`）。条目总数仍 **26**。
- **2026-09-23 增补（T-8 GC 面修复）**：**T-8 追加状态**——登记表非 GC 可见持有的地雷已修（`UTcsDamageSubsystem` / `UTcsEffectSubsystem` 各加静态 `AddReferencedObjects` 覆写 + `AddPropertyReferencesWithStructARO`；提案 `fix-registry-gc-visible-holding` → 归档 `2026-09-22-fix-registry-gc-visible-holding`，`damage-flow` / `effect-chain` 各并入一条"对 GC 可见"要求）。**T-8 仍未完全勾销**：资产化面（模板资产 + 改名）待触发；且 GC 场景**未实证**（宿主现用 `UPROPERTY` 强引用绕过，成为冗余保险；R3 无"无其他强引用的 delegate"夹具）——成立依据为引擎源码机制而非实测，条目内已标注。条目总数仍 **26**。
- **2026-09-23 增补（删死字段）**：**不新开条目**——`FTcsAttrModInstance.Tag` / `FTcsAttrModDefTableRow.Tag`（`FName` 型"同来源内分组标签"）经核实**全库零消费者**，用户拍板删除（提案 `remove-dead-modifier-tag-field` → 归档 `2026-09-22-remove-dead-modifier-tag-field`，`attribute-types` 两条需求并入）。**不入册理由**：删除即勾销，无残留待办；"将来若需同来源内分组，以 `FGameplayTag` 形态重新引入"已写进规格正文，不需台账盯。条目总数仍 **26**。
- **2026-09-23 增补（R4 计划三产出 + 路线调整）**：plan3 `2026-09-23-r4-plan3-trigger-row-and-modifier-channel.md` 产出，**含 R4–R8 轮次路线图**。**路线偏离原序**（原 `rebuild-module-map-proposal.md:146`：`R4 M3 → R5 M4a+M4c`）——改为 **R4 触发行先行、R5 M3**，理由：`03 §6` 明文"buff 行为由 M4 触发行订阅 M3 生命周期事件挂接"，M3 先落地则其行为面在交付当天无订阅者、验不了（用户 2026-09-23 认可）。**R6 可能拆两轮**（M5 与剩余 8 原语都是重活），**R7-3 加载层可拆独立轮**（若 R7 超载）。条目总数仍 **26**（本轮消费与新登记待 plan3 Task 5 执行时落）。
- **2026-09-23 增补（plan3 Task 1 收束）**：**不新开条目**——本任务落 D4-1 十字段与 D4-5 前两项条件，其余留位项合并为一条登记（归属 = 各自真实消费者出现的轮次）：**触发行留位字段与剩余条件**——①`EventPayloadFilter`（载荷预筛，等首个带可筛字段的载荷类型）；②`Cues`（CueId 引用列表 → R8 TcsCue）；③`InterruptPriority`（可打断哪些链 → 链打断语义轮）；④`ExecutionGate` 非默认值（→ 网络姿态轮）；⑤D4-5 剩余条件 `AttributeCompare`（需属性读取注入 → 可随 R5）/ `VariableCompare`（需变量存储消费者）/ `GateCheck`（读 M5 `BoolSwitches` → R6）/ Custom 逃逸位。条目总数 **27**。
- **2026-09-23 增补（Task 1 形态精修 + R4 扩容）**：**R5-2 部分消费**——`SetVar`/`Branch`/`RunSubChain`/`WaitEvent` 四个原语**提前到 R4**（用户拍板"最好在 R4 就追加这些 EffectStep 原语"），剩 `Repeat`/`Parallel`/`OnError` + `ModifyAttribute` 留 R5；**`ModifyAttribute` 归属修正**——它需要"属性访问注入位"（`FTcsEffectContext` 今天无属性读写口），与 `ApplyState` 同批归 R5（用户拍板"可以先往后放"）。**触发行留位项更新**：`Cues` 已从定义中**删除**（TcsCue 未敲定，用户拍板——TcsCue 落地时加回），故该条改为"`Cues` 待加回 + `EventPayloadFilter` + `InterruptPriority` + `ExecutionGate` 非默认值 + D4-5 剩余条件"。**新增待办**：①`UTcsEffectTriggerDefAsset` 资产载体 + DefLibrary 发现（Task 2.5）；②条件求值器与步骤执行器的**反射注册入口**（CS 调研 §7.6 G-2 的共享欠账）。条目总数仍 **27**。
- **2026-09-23 增补（plan3 Task 2 收束：登记表与求值器）**：**R5-1 部分消费（更新）**——起链装配的 `Caster` 通路已落地（但形态与原计划不同，见下）；`Targets` 装填仍留待触发（需真实带目标的载荷类型）。**新增登记（27 → 29）**：
  - **`FTcsTriggerPayloadReader` 的属主登记（TcsDamage 侧）**——本批落了注册表与宏，但**没有任何读取器被登记**（`TcsDamage` 的收集事件读取器随 Task 3 同批——那时 TcsDamage 本来就要新开文件）。**当前后果**：真实流程事件上的 `ClassificationTags` 恒为空集 → `HasAllTags` 条件**恒不过**（规格明文交付的语义，非缺陷）。归属 = plan3 Task 3。
  - **触发行 GC 场景未实证**——门面已补 `AddReferencedObjects`（逐行 `AddPropertyReferencesWithStructARO`），但"行内条件携带的、别处无强引用的对象引用不被回收"**未跑实测**（与 T-8 同款：R3 无合适夹具）。成立依据 = 引擎源码机制（`WithAddStructReferencedObjects` 递归）而非实测。归属 = 等一个真实的宿主自定义条件类型 + 可观测的 GC 断言。
- **2026-09-23 纠正（plan3 Task 2 实施期发现的两处计划错误，已回写 plan3 与规格）**：
  - **`plan3` 原文"值语义 `TArray` → 无需 ARO 覆写"判据错误**：是否需要 GC 补引用与"值语义/指针语义"**无关**，只取决于**容器是否 GC 可见**。`FTcsTriggerRegistry` 是门面的非 `UPROPERTY` 成员 → GC 的 `RefLink` 走不到，行内 `FInstancedStruct` 内层可放对象引用 → 静默回收。**这与 T-8 是同一类缺口**（缺口在容器，不在载荷）。已补引用并回写规格场景。
  - **`plan3` 原文"代际校验不适用（无池）"错误**：登记表用空闲链表复用槽位后，陈旧句柄会**静默改指另一行**。已改为自持代际计数（仍不引入 `TTcsInstancePool` 类型）。
  - **`plan3` 原文 `Caster` 解析规则不可实现**：原文写"从载荷内已知类型取（流程收集事件 → `Context->Attacker`）"，但 `TcsEffect` MUST NOT 认识领域载荷类型（依赖铁律）→ 不可能写 `GetPtr<FTcsDamageFlowCollectEvent>()`。改为**载荷读取器注册表**（属主模块自登记）。
