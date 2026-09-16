# M6（集成）决策点提案 v1（待拍板）

- 日期：2026-09-02
- 状态：**决策点提案**——M6 横切层：两级单位适配、StateTree 集成、引导时序、tick 接线、复制契约
- 证据核验：2026-09-02 对照 TCS 10/11/ZZ。关键事实：TCS StateTree 集成为**继承式**（`UTcsStateComponent : UStateTreeComponent : UBrainComponent`，10:12）；**零 Mass 记载**（纯 Actor 组件模型）；**零复制代码**；引导缺陷两处实证（ReadyNow 跳 OnReady；ready 门控只查 DefMgr 指针非空不调 IsRuntimeReady，11:106-120）；orphan-scan = **Editor DataTableSync 的 DefAsset 资产清理缺陷**（归 M8，不在本轮）。

## D6-1 StateTree 集成形态
- **A 继承 UStateTreeComponent（TCS 现状）**：引擎决策组件生态免费；但绑死 UBrainComponent 语义——Mass 小兵（无 Actor）无法继承，与"两级单位"直接冲突；且"StateTree 只做决策"的定位下继承全功能组件过重。
- **B 组合式（推荐）**：军官单位组件 `UCombatUnitComponent` 继承 **UActorComponent**；StateTree 以**可选成员**接入（引擎原生 `UStateTreeComponent` 子对象 + 我们的 `UCombatStateTreeSchema : UStateTreeComponentSchema`——组件级 Schema 收敛 Context 注入的通道复用 TCS 已验证形态，10:85-87）。不挂 StateTree 的项目零决策树成本；挂了才有决策。
- 与裁决一致性：StateTree 只做决策/编排（单位级"放什么技能"），行为执行仍是原语链（D3-5 边界不变）。
- 净新增声明：组合式无 TCS 先例，有意分歧，理由 = 两级单位硬约束。

## D6-2 两级单位注册协议
- **统一身份**：`FCombatUnitHandle`（M0 句柄机制）由中央注册表（M3）分配；军官组件与小兵 Mass 桶只是适配器（D3-1 定案）。
- **军官注册**：`UCombatUnitComponent::Initialize`（引擎推荐的 CDO 安全初始化）→ `RegisterUnit(适配器)`；BeginPlay 只做世界相关收尾。注销对称（EndPlay/OnUnregister）。
- **小兵注册**：Mass 生成/回收 processor 回调 → 同一 RegisterUnit/Unregister；适配器 = Mass fragment 内的桶引用 + `ICombatAttributeProvider` 实现。
- 适配器零逻辑：缓存桶指针 + 转发查询（D3-1 定案的落实位置）。

## D6-3 引导时序状态机（高风险三连收官）
- TCS 两处实证缺陷的硬规则化：
  1. **ReadyNow 分支跳过 OnReady**（bootstrap cpp:68-72）→ 新规则：**状态机所有到达 Ready 的迁移路径必须经过同一个 OnReady 派发点（单出口）**，禁止任何分支绕行；
  2. **ready 门控只查 DefMgr 指针非空**（cpp:112-117）→ 新规则：**ready 门禁 = M1 注册表 `IsRuntimeReady()` 全量成功判定**（D3-2 状态机，Failed 带清单）。
- **预加载触发者（本轮拍板）**：
  - A GameInstance 级（TCS 式 DefManagerSubsystem）：跨世界持久，但 PIE 每世界词表可能不同；
  - **B WorldSubsystem Initialize 收集 + DeveloperSettings 补充（推荐）**：世界级适配 PIE；三策略（PreloadAll/PreloadSelected/OnDemand）配置继承 TCS 六域策略形态（02 报告），收集 = 扫描 Def 注册表 + DevSettings 显式列表。

## D6-4 复制代理契约（仅契约，不实现）
- **推荐：本轮只定契约草案** `ICombatReplicationProxy`（代理 Actor 读写中央 store 的单一入口：操作流注入/状态快照导出），实现推迟到有联网项目时（YAGNI；NET-2 千人战场的形态届时按 ReplicationGraph/Iris 细化）。
- 备选：现在就实现每单位代理 Actor——两个项目都不联网，纯付成本。

## D6-5 tick 接线
- **全部战斗组件不自 TickComponent**（TCS 各组件自 Tick 的反面）——M0 时钟泵唯一驱动（D0-5）。
- StateTree 决策树（军官可选）tick 语义映射：TCS 三值（RunOnce=Restart+Tick(0)+超时强停；ManualOnly=仅 Restart；WhileActive=入调度器，10:248）→ 新系统：ManualOnly 继承为默认（TCS 默认，瞬发语义）；WhileActive = 泵订阅等价；RunOnce 保留。调度器职责收归 M0 泵 + 决策组件自持（不再造第二套调度器）。

## 归属更正：orphan-scan
DataTableSync DefAsset 清理缺陷（子目录递归扫描不回写、严格相等判定误删）→ **M8 修复清单**（范围白名单 + 回写触发 + 重复 DefId 判定加缓冲），M6 不涉及。

## v2 修正（用户反馈 2026-09-02）

1. **命名**：军官组件定名 `UCombatEntityComponent`；句柄同步更名 `FCombatEntityHandle`（与组件一致；文档行文"单位（Entity）"不变）。
2. **范围声明（重要）**：战斗系统插件**当前阶段不直接支持 Mass**——Mass 注册协议移出当前范围，进入"未来扩展"清单；中央注册表的 Actor 无关性（D3-1）保留为未来前提，届时只需补适配器 + 生成/回收 processor，核心零改动。MD-1 地图中 M6"两级单位"表述同步调整为"当前仅 Actor（Entity 组件）路径，Mass 为未来扩展项"。
3. **D6-1 组件职责清单**（回答"除了 StateTree 还有什么职责"）：①注册表身份锚——Initialize/EndPlay 翻译 Actor 生死为注册表条目生死（核心职责）；②查询门面——ICombatAttributeProvider 等适配实现；③可选 StateTree 决策宿主。适配器零逻辑原则的落点；挂组件 = 策划声明"本 Actor 是战斗单位"。
4. **D6-3 解释增补**：状态机 = 每单位 Unregistered→Registered→Loading→Ready→TornDown，TryActivate 第一道门禁；单出口 OnReady 防通知丢失（TCS ReadyNow 分支实证）；门禁查 IsRuntimeReady 全量状态而非指针存在性；WorldSubsystem 理由 = 生命周期匹配（PIE 隔离）+ 时序匹配（Initialize 时关卡已定）+ 同域原则（定义缓存与单位桶同注册表）；GameInstance 级只读定义缓存可作未来性能优化，门禁仍走世界级。

## 拍板方式
D6-1~D6-5 五项，逐项或"按推荐"。

## D6-3 v3 终定（用户模型采纳，2026-09-02）

- **双层拆分**：DefLibrary 定义库 = **GameInstance 级**（加载/缓存/校验/按名解析；六域三策略继承；D3-2 就绪状态机住此；服务图鉴/UI——无世界可查；M1 词表宿主）；战斗注册表驱动器 = **WorldSubsystem 级**（单位桶、实体状态机 Unregistered→…→Ready、tick 泵接线）。
- **门禁方向**：世界级实体状态机查询 `DefLibrary.IsRuntimeReady()`（GameInstance 比 World 长寿，指针方向安全）。
- **论证修正声明**：原"同域原则"（定义缓存与单位桶同注册表）论证错误——混淆了 Const 定义与可变运行态；Const 定义跨 PIE 世界共享正确且合需（同构建同内容，加载一次），需世界级隔离的只有单位桶与运行时状态。
- **与 TCS 关系**：形状殊途同归（GameInstance DefManager + 世界级 bootstrap），但修复其两个实证缺陷（单出口 OnReady、全量门禁）——是 TCS 的正确版本而非回退。
- 其余终定：D6-1 组合式+职责清单+CombatEntity 命名 ✓；D6-2 Mass 移出当前范围 ✓；D6-4 只定契约 ✓；D6-5 泵唯一驱动 ✓。

## 拍板状态（终）

| 项 | 状态 |
|---|---|
| D6-1 / D6-2 / D6-3(v3) / D6-4 / D6-5 | ✅ 全部拍板（2026-09-02） |
