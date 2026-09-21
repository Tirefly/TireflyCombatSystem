# integration-entity Specification

## Purpose
TBD - created by archiving change add-tcsintegration-entity-layer. Update Purpose after archive.
## Requirements
### Requirement: 战斗实体组件（三职责封顶）

`TcsIntegration` MUST 提供 `UTcsCombatEntityComponent : UActorComponent`（`ClassGroup=(Combat)`、`meta=(BlueprintSpawnableComponent)`），**恰好三职责**：

1. **身份锚**：`BeginPlay` 时把所属 Actor 注册为战斗实体（`UTcsAttributeSubsystem::RegisterUnit`）并**记录自己的句柄**；`EndPlay` 时反向注销。注册**MUST** 检查定义就绪门禁（DefLibrary 未就绪 → Warning + 跳过注册，**不 ensure**——时序是宿主责任）；
2. **查询门面**：`GetCurrent(FTcsAttributeName) -> double`（转发属性门面求值）+ 施加/撤销调试用修正器入口（供宿主与装置；R3 不做 AttributeSet）；
3. **手动触发 API**：`ExecuteChainById(FName ChainId)` —— 组 `FTcsEffectContext`（`Caster` = 自身句柄、`Targets` = 自身句柄、`Instigator` 留空）→ `UTcsEffectSubsystem::ExecuteChain`；返回运行态句柄（全即时链在返回前已走完，句柄活性由 `IsRunActive` 判定）。

**MUST NOT 越出三职责**：组件不是状态机、不持策略对象、不做表现（Cue/指示器归 M7/宿主）；**MUST NOT 认识具体链/步骤类型**（链 id 由调用方给）。

#### Scenario: 注册后句柄有效

- **WHEN** 组件 `BeginPlay`（且定义库就绪）
- **THEN** 所属 Actor 已注册为战斗实体，`GetEntityHandle()` 返回有效句柄；`EndPlay` 后该句柄失效

#### Scenario: 定义库未就绪时不注册

- **WHEN** 组件 `BeginPlay` 而 DefLibrary 尚未就绪
- **THEN** 留 Warning 并跳过注册（不 ensure、不崩溃）——待宿主修时序

#### Scenario: 手动触发链

- **WHEN** 调用 `ExecuteChainById("Chain_X")`（该链已登记）
- **THEN** 链按 `Caster`/`Targets` = 自身句柄执行；未登记链 → Error 日志 + 无效句柄（不崩溃）

### Requirement: 实体查询实现（PIE）

`TcsIntegration` MUST 提供 `ITcsEntityQuery` 的实现 **`UTcsPieEntityQuery`**（**类名不得用 `UTcsEntityQuery`**——该 U 类名已被接口占用）：

- `EnumerateEntities(TFunctionRef<void(FTcsCombatEntityHandle)>)`：遍历世界中带 `UTcsCombatEntityComponent` 的 Actor 并吐**句柄**（**稳定序**——按组件注册序或 Actor 名排序，同输入同输出）；
- `GetLocation(句柄, FVector&)` / `IsAlive(句柄)`：经**本实现持有的"句柄 ↔ Actor"映射**解析（组件注册时登记、注销时移除）；
- **本实现是宿主侧唯一的"句柄 ↔ Actor"映射点**：机制层（TcsEffect/TcsTargeting/TcsDamage）MUST NOT 依赖该映射，内容资产 MUST NOT 存句柄（授权约束）。
- 注入方式：宿主在 `BeginPlay`/DefLibrary 就绪后调 `UTcsEffectSubsystem::SetEntityQuery`（R3 由测试/宿主接线）。

#### Scenario: 遍历吐句柄且稳定序

- **WHEN** 世界中三个带组件的 Actor 调用 `EnumerateEntities`
- **THEN** 访问者收到三个**句柄**（非 Actor），两次遍历顺序一致

#### Scenario: 句柄解析为定位与存活

- **WHEN** 以合法句柄调 `GetLocation` / `IsAlive`
- **THEN** 分别返回该 Actor 的坐标与"组件仍注册"的存活判定；未注册/已注销句柄返回 false

### Requirement: 定义库（GameInstance 级最小版）

`TcsIntegration` MUST 提供 `UTcsDefinitionSubsystem : UGameInstanceSubsystem`（**MUST 为 GameInstance 级**——Const 定义内容跨 PIE 共享，图鉴/UI 等**无世界**场景要能查）：

- **单出口** `OnDefinitionsReady()`（M6 双层引导的最小版）：定义发现/加载/校验完成时调用一次（幂等——重复调用不重复登记）；
- 就绪状态：`IsRuntimeReady() -> bool` + 失败清单（`GetFailureList()`）——组件注册门禁查它；
- **链资产发现**：经 **`IAssetRegistry::GetAssetsByClass(UTcsEffectChainDef::StaticClass()->GetClassPathName(), ...)`** 按类扫描（**MUST NOT** 依赖 `PrimaryAssetTypesToScan` 注册——该注册属 M6 轮，且未注册时 AssetManager 按类型查询会**静默返回空列表**，排障成本高）；
- **发现是运行期机制**：`AssetRegistry` 为 Runtime 模块——编辑器期由 `AssetDataGatherer` 扫盘、**cooked 构建由引擎启动时加载预生成注册表数据**（`FCookedAssetRegistryPreloader`），故打包后本发现路径同样成立（这是"零 C++ 加链"验收项的运行期前提）；
- **异步扫描就绪**：编辑器期初始扫描为**异步**，未完成时 `GetAssetsByClass` **静默返回不完整结果**（不报错）——扫描前 **MUST** 确认注册表就绪（`IsLoadingAssets()` / `WaitForCompletion()`），**MUST NOT** 在未就绪时把空结果当作"无链资产"；
- **定义缓存与装配到世界**（跨级方向纪律）：DefLibrary **MUST 缓存**发现的链定义（Const 数据），并**在每个世界初始化时装配进该世界的 `UTcsEffectSubsystem`**——**MUST NOT** 只在 ready 时对当时的 World 登记一次（DefLibrary 跨世界存活而 EffectSubsystem 每世界重建，只登记一次会在关卡切换后丢失全部链）。装配 **MUST 幂等**（重复装配同一世界不重复登记）；**非游戏型世界跳过**（编辑器世界等取不到 EffectSubsystem 实例即静默跳过——该子系统自身按 `DoesSupportWorldType` 过滤）；
- **链资产登记**：装配时逐个调 `RegisterChain`（**暂定自动登记**——用户 2026-09-21："先暂定 DefLibrary 自动登记，等真正开始开发时再讨论"）；登记失败（id 冲突等）MUST 计入失败清单 + Error 日志，MUST NOT 静默跳过；
- **管辖边界**：DefLibrary **只做资产发现与注册**，不做执行（执行归 `UTcsEffectSubsystem`）；MUST NOT 持可变运行态（缓存的是 Const 定义，非运行态）。

#### Scenario: 就绪后组件可注册

- **WHEN** `OnDefinitionsReady` 被调用
- **THEN** `IsRuntimeReady()` 返回 true，此后组件注册通过门禁

#### Scenario: 链资产自动登记

- **WHEN** DefLibrary 加载到一个 `ChainId = "Chain_Fireball"` 的链资产
- **THEN** `UTcsEffectSubsystem::FindChain("Chain_Fireball")` 可查到该链

#### Scenario: 新世界初始化后链仍可查（跨世界装配）

- **WHEN** DefLibrary 已就绪（链已缓存），此后一个新世界完成初始化
- **THEN** 该世界的 `UTcsEffectSubsystem::FindChain(...)` 能查到全部已缓存链（无需重新扫描/重新 ready）

#### Scenario: 登记失败进清单

- **WHEN** 链资产的 `ChainId` 为空、或与已登记链冲突
- **THEN** 该资产登记被拒 + Error 日志 + 计入失败清单（不静默跳过）

