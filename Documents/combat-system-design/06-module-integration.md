# 06-module-integration.md — TcsIntegration 集成层设计（v2 定稿重写）

- 日期：2026-09-02
- 状态：**v2 定稿**——全部增补（D6-1~5 v3、用户范围声明）已折入正文；修订记录见文末
- 职责一句话：**引擎与战斗核心的接线层——把 Actor 世界的生死翻译成注册表条目、把定义库装配到世界、托管可选的 StateTree 决策宿主、定义复制契约。适配器零逻辑。**

## 1. 模块边界

- 消费者：宿主项目（挂组件/注入接口）、M7/M8。
- 依赖：TcsSkill, TcsCue + GameplayStateTree（**全插件唯一依赖 StateTree 的模块**）——**最小编译集**（R0 §9；层级序允许触达全部下层，此处只列实际 include）。
- 被依赖：TcsEditor（全部模块均被其消费）。

## 2. 类型词汇（对外）

### 2.1 双层引导（D6-3 v3 终定，用户图鉴反例修正）
- **`UTcsDefinitionSubsystem : UGameInstanceSubsystem`**——M1 DefLibrary（概念名，沿用设计词汇）的宿主（**类名收口 2026-09-21 两轮**：设计侧原写 `UCombatDefLibrarySubsystem` → 用户先拍 `UTcsDefLibrary` → 再定 `UTcsDefinitionSubsystem`。判据：①与族内风格一致（`Tcs` + 域 + `Subsystem`，如 `UTcsAttributeSubsystem` / `UTcsEffectSubsystem` / `UTcsDamageSubsystem`）；②**避免"Registry"一词**——该词已被**中央注册表**（`UCombatWorldRegistrySubsystem`，可变运行态 per-unit 桶）占用，且设计文档另有"定义注册表"（本类）与"链定义登记表"（领域子系统）两处混用，故新类名不再加重该词负担；`DefLibrary` 保留为**概念名**，与"概念名 `UTcsAttributeSubsystem` ↔ 文档'属性词表'"同款惯例）：
  - 加载（六域三策略：PreloadAll/PreloadSelected/OnDemand，配置继承 TCS 形态）→ 校验（重复/非法引用加载期报错）→ 按名解析；
  - **D3-2 就绪状态机住此**：`Unloaded→Loading→Ready/Failed(失败清单)`，Ready 广播至多一次且以全量成功为前提，**单出口派发**；
  - 服务图鉴/UI：无世界可查（用户 LAC 图鉴反例的直接落点——定义是 Const 内容，生命周期长于世界）。
  - **装配到世界（2026-09-21 增补，跨级方向纪律）**：DefLibrary 是 GameInstance 级（跨世界存活），而定义**消费方**（如 `UTcsEffectSubsystem`）是 World 级（每世界重建）——故定义 MUST 由 DefLibrary **缓存**，并在**每个世界初始化时装配进该世界**（订阅 `FWorldDelegates::OnPostWorldInitialization`，幂等）。"ready 时对当时的 World 登记一次"会在关卡切换后丢失全部登记。R3 阶段发现机制用 `IAssetRegistry::GetAssetsByClass` 按类扫描（`PrimaryAssetTypesToScan` 注册属 M6 轮，见台账 R7-3）。
- **`UCombatWorldRegistrySubsystem : UTickableWorldSubsystem`**——M3 中央注册表 + M0 泵接线的宿主：
  - 单位桶、实体状态机（`Unregistered→Registered→Loading→Ready→TornDown`）、到期堆接线；
  - **门禁 = 查询 `DefLibrary.IsRuntimeReady()`**（GameInstance 比 World 长寿，指针方向安全；失败清单透传给实体状态机 Failed 态）。
- 论证修正声明：Const 定义跨 PIE 世界共享正确且合需（同构建同内容，加载一次）；需世界级隔离的只有单位桶与运行时状态。

### 2.2 实体组件（D6-1/D6-2 终定，用户命名与职责确认）
- **`UCombatEntityComponent : UActorComponent`**——三职责封顶：
  1. **注册表身份锚**：`Initialize → RegisterEntity`（分配 `FCombatEntityHandle`、创建桶）、`EndPlay → Unregister`——Actor 生死 → 注册表条目生死的唯一翻译器；挂组件 = 策划声明"本 Actor 是战斗单位"；
  2. **查询门面**：实现 `ICombatAttributeProvider`（缓存桶指针转发）；**并暴露该单位的 AttributeSet 查询与切换入口**（D2-15，2026-09-17：Set 引用点住实体侧配置——组件持引用；"当前生效的 Set"是 World 级可变状态）；
  3. **可选 StateTree 决策宿主**：子对象挂引擎原生 `UStateTreeComponent` + `UCombatStateTreeSchema : UStateTreeComponentSchema`（Context 收敛注入——通道复用 TCS 已验证形态，10:85-87）；不挂零成本。
- **属性集合的施加时序（D2-14/D2-15，2026-09-17 裁决）**：单位属性由 **AttributeSet 在注册期初始化**——施加点在 `RegisterEntity` 之后，且**必须在 DefLibrary `IsRuntimeReady()` 门禁通过之后**（Set 资产是 GameInstance 级 Const 内容，未就绪时不得施加）；后续"换情景"由宿主调 `ApplyAttributeSet`（diff 替换，共有保留实例）或 `ClearAttributeSet`（整组清空）。**引擎不认识"游戏模式"轴**——情景与 Set 的对应关系归宿主（换实体身上的引用或换实体）。
- 适配器零逻辑；**全部战斗组件不自 TickComponent**（D6-5：M0 泵唯一驱动）。
- 宿主注入接口：`ICombatEntityQuery{Enumerate/GetLocation/IsAlive}`（**D4-15 统一命名**——接口定义在 TcsEffect，实现名 ITcsEntityQuery 见 plan2；旧名 ICombatEntitySpatial 已并于此）、`IRelationResolver{IsHostile}`（阵营——插件无阵营本体论）。
- ~~Mass 适配~~ **移出当前范围（用户范围声明）**：注册表 Actor 无关性（D3-1）保留为未来前提，届时新增 `TcsMass` 模块（适配器 + 生成/回收 processor），核心零改动。

### 2.3 复制契约（D6-4：仅契约不实现）
- `ICombatReplicationProxy`（草案）：`InjectOperation(操作流)` / `ExportSnapshot()`——代理 Actor 读写中央 store 的单一入口；实现推迟到有联网项目（届时按 ReplicationGraph/Iris 细化形态）。

## 3. 入口服务

- DefLibrary：`LoadAll/LoadSelected/EnsureLoaded(DefTag)`、`IsRuntimeReady()/GetFailureList()`、`ResolveDef(FGameplayTag)`；**并管辖 `UTcsAttributeSetAsset` 一族**（D2-15：Set 是 Const 内容、GameInstance 级，**无世界也要可查**——与图鉴/UI 同款消费场景）。**身份 2026-09-22 tag 化**：Def 引用键由 `FName` 改 `FGameplayTag`（`ResolveDef(FName)` → `ResolveDef(FGameplayTag)`）。
- WorldRegistry：`RegisterEntity/UnregisterEntity`、`GetEntityState(handle)`、`ForEachEntity(谓词)`（RadiusArea 的遍历源）、`ResolveProvider(handle)`。
- Entity 组件：`GetHandle()`、`GetAttributes()`（含"当前 Set"查询）、可选 `GetBrain()`。

## 4. 关键机制

- **引导硬规则**（高风险三连收官）：①到达 Ready 的所有迁移路径**单出口**经过 OnReady 派发点（TCS ReadyNow 绕行实证的反面）；②就绪门禁 = `IsRuntimeReady()` 全量判定 + 失败清单透传（TCS"指针非空"实证的反面）。
- **实体状态机门禁**：`TryActivate` 第一道门 = 实体状态 Ready；Loading 中排队或拒绝由门禁参数定（默认拒绝，返回具名原因）。
- **StateTree 决策语义**：只做决策/编排（裁决定位）；tick 三值映射——ManualOnly 默认（TCS 瞬发语义继承）、WhileActive = 泵订阅等价、RunOnce 保留；**不造第二套调度器**（泵 + 决策组件自持）。
- **空间查询**：`RadiusArea`（后置形态——10 文档 v2）= 遍历注册表已注册实体 + `ICombatEntityQuery` 位置过滤；千人规模时的优化（空间哈希）留给实现期，接口不变。

## 5. 网络姿态落点（NET-1/2）

- `ICombatReplicationProxy` 契约（§2.3）；DefLibrary 的定义是 Const 共享数据——跨世界/跨端共享正确且合需；世界级隔离只针对单位桶与运行时状态。

## 6. 非目标

**不做 Mass 适配**（未来 TcsMass）；不做输入系统对接；不做 UI/图鉴页面（DefLibrary 只提供查询 API）；不实现复制；不做关卡管理；不做机器人/AI。

## 7. 依据

- 拍板：D6-1~D6-5 v3（2026-09-02 两轮问答框；用户贡献：组合式确认 + Entity 三职责封顶 + CombatEntity 命名、**Mass 移出当前范围**、**图鉴反例推翻 World 级加载 → 双层架构**、复制只定契约、泵唯一驱动）。
- 证据：TCS 10/11 核验（继承式集成、ReadyNow 绕行、"指针非空"门禁、组件级 Schema 通道已验证、零 Mass/零复制）；MEM-20260902-05 生命周期分层。

## 8. 修订记录

- v1（2026-09-02）：初版决策折入（World 级加载方案）。
- v2 增补（2026-09-02）：CombatEntity 命名与三职责、Mass 移出、双层引导（图鉴反例）、D6-3 解释增补。
- v3 增补（2026-09-17，D2-14/D2-15 裁决折入）：组件第 2 职责补"AttributeSet 查询与切换入口"；新增"属性集合的施加时序"条（注册期 + 门禁之后 + 引擎不认识游戏模式）；DefLibrary 管辖面补 `UTcsAttributeSetAsset` 一族；入口服务补"当前 Set"查询。
- v4 增补（2026-09-21，Task 5 前置讨论落档）：§2.1 DefLibrary 类名收口（`UCombatDefLibrarySubsystem` → **`UTcsDefinitionSubsystem`**，两轮拍板；`DefLibrary` 降为概念名）；新增"装配到世界"跨级方向纪律条（GameInstance 级缓存 + 每世界初始化装配，订阅 `OnPostWorldInitialization`）；R3 发现机制定为 `IAssetRegistry::GetAssetsByClass`（`PrimaryAssetTypesToScan` 注册入台账 R7-3，属 M6 轮）。
- v2 定稿重写（2026-09-02）：全部增补折入正文（本文）。

## 9. 验收钩子

引导硬规则人工检查：构造"定义加载失败"与"快慢两种注册路径"，验证 OnReady 单出口与 Failed 清单；CombatEntity 注册/注销的句柄代际级联。（~~RadiusArea 走查~~ **后置**——RadiusArea/FTargetingShape 已后置（10 文档 v2），届时回归本钩子。）
