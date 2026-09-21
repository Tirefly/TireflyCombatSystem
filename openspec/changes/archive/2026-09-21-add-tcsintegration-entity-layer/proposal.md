# Change: 落地集成层——战斗实体组件、实体查询实现、定义库最小版与链资产类（integration-entity / effect-chain-asset）

## Why
plan2 Task 5 动工前的规格先行提案。前四站交付的是**机制与领域层**（链解释器 / 目标策略 / 流程机制 / 标准步骤库与链原语），但**没有"宿主怎么用"的那一层**：谁把实体注册进属性系统、谁提供实体查询、谁在什么时候把内容资产登记进各门面。本任务补上集成层最小版，并**收口三处计划交接注记**（链资产类落点 / 实体查询实现类名撞名 / 句柄↔Actor 映射唯一归宿主）。

## What Changes
- 新增能力规格 **`integration-entity`**（4 条需求）：
  1. **`UTcsCombatEntityComponent`（三职责封顶）**：①**身份锚**——注册期把所属 Actor 注册为战斗实体并记录自己的句柄；注销期反向清理；②**查询门面**——`GetCurrent(FTcsAttributeName)` / 施加与撤销（供宿主/调试用）；③**手动触发 API**——`ExecuteChainById(FName)`（R3 无触发行，手动触发是竖切入口）；**MUST NOT 越出三职责**（组件不是状态机、不持策略、不做表现）；
  2. **`ITcsEntityQuery` 的 PIE 实现**（类名 `UTcsPieEntityQuery`——**必须改名**：`UTcsEntityQuery` 已被接口的 U 类占用）：`EnumerateEntities` 遍历带该组件的 Actor 并吐**句柄**（稳定序）/ `GetLocation` / `IsAlive`；**它是宿主侧唯一的"句柄 ↔ Actor"映射点**（组件注册时登记句柄）；
  3. **`UTcsDefinitionSubsystem`（GameInstance 级最小版）**：`OnDefinitionsReady` **单出口**（M6 双层引导的最小版）+ 定义就绪状态机（`IsRuntimeReady` / 失败清单位）+ **定义缓存与"装配到世界"**；**MUST 为 `UGameInstanceSubsystem`**（Const 定义内容跨 PIE 共享、图鉴/UI 等无世界场景要能查——生命周期分层判据）；
  4. **组件 ↔ 实体的注册/注销纪律**：注册必须在定义就绪之后（门禁）、注销走句柄（旧句柄自动失效）。
- 新增能力规格 **`effect-chain-asset`**（2 条需求）：
  1. **`UTcsEffectChainDef : UPrimaryDataAsset`**：字段 `{ FName ChainId; FTcsEffectChain Chain; }`；`static const FPrimaryAssetType PrimaryAssetType` **显式声明（值 = 类名 `"TcsEffectChainDef"`——族内一致，2026-09-21 用户收口）** + 覆写 `GetPrimaryAssetId()` 使**名取 `ChainId`**（2026-09-17 Def 身份标准：资产可改名/移动不破坏解析）；`Chain.ChainId` 与 `ChainId` **MUST 一致**（不一致拒绝登记 + Error）；
  2. **登记路径**：DefLibrary 发现并加载链资产后调 `RegisterChain`（**暂定自动登记**——用户 2026-09-21 定"开发时再议"）。
- **不做的**（非目标，保持 R3 边界）：不做触发行求值器（M4a/R5）；不做 AttributeSet（D2-15，R7）；不做流程模板资产类（同链资产形态，随真实需求轮）；不做复制；不做输入系统对接/UI/图鉴页面（DefLibrary 只提供查询 API）；不做 Mass 适配（`TcsMass` 后置）；**不做 `PrimaryAssetTypesToScan` 注册**（见下方钉名表——属 M6 轮，本任务用 AssetRegistry 按类扫描替代）。

## 落点与口径
1. **三处交接注记的收口**（Task 1/3 埋的）：①链资产类 = **资产轨**（2026-09-21 用户拍板；否决 DataTable 行轨与双轨）；②实体查询实现类名 = **`UTcsPieEntityQuery`**；③**句柄 ↔ Actor 映射唯一归宿主**（本任务的查询实现即该映射点——机制层与内容资产都不碰）。
2. **文件落点**：`Public/Entity/TcsCombatEntityComponent.h` + `Private/Entity/`（组件）、`Public/Entity/TcsEntityQuery.h` + `Private/Entity/`（**注意与 TcsEffect 的 `Host/TcsEntityQuery.h` 同名但不同模块**——本模块的是**实现**、那个是**契约**；文件名同名在跨模块 include 时需注意，已在头注释写明）、`Public/TcsDefinitionSubsystem.h` + `Private/`、`Public/Chain/TcsEffectChainDef.h` + `Private/Chain/`。
3. **组件注册纪律**：组件在 `BeginPlay` 注册（`RegisterUnit`）、`EndPlay` 注销（`UnregisterUnit`）；**注册前检查 DefLibrary 就绪**（未就绪 → Warning + 跳过，不 ensure——时序是宿主责任）。

4. **本任务对 Def 身份标准的一处收窄**（2026-09-21）：`GetPrimaryAssetId()` 覆写与 `static const FPrimaryAssetType` 显式声明照 2026-09-17 标准保留（**纯函数、不依赖注册**，`DataAsset.cpp:73-122` 已核实）；但 R3 **不注册** `PrimaryAssetTypesToScan`，故本任务内**不使用** `GetPrimaryAssetId()` 做发现，只用它作为身份声明。注册与 AssetManager 切换**登记台账 R7-3**（M6 轮）。
5. **发现层与加载层分离**（2026-09-21 拍板）——**本任务只做发现，不做加载策略**：
   - **发现层（R3 本任务）**：`IAssetRegistry::GetAssetsByClass` 扫出资产路径（`FAssetData`）。理由：不提前替 M6 拍板注册时机；扫描失败可见。
   - **发现层是运行期机制（非编辑器专属）**：`AssetRegistry` 是 **Runtime 模块**，两套数据源——编辑器里由 `AssetDataGatherer` 扫盘（`AssetRegistry.cpp:750`），**cooked 构建由 `FCookedAssetRegistryPreloader` 在启动时加载预生成注册表数据**（`ARLoader.cpp:161`；触发条件 `RequiresCookedData() && (IsRunningGame() || IsRunningDedicatedServer())`，`ARLoader.cpp:43`；数据由打包期 `FAssetRegistryGenerator` 写入）。**验收判据 = 剧本检查点 6"零 C++ 加链"**（`2026-09-02-r3-vertical-slice-script.md:35`）——策划只建资产、不改代码，链即可被 `ExecuteChainById` 调到，这物理上依赖发现层在运行期自动找到它。
   - **实现 MUST 处理异步扫描坑**：编辑器里初始扫描是**异步**的（`SearchAllAssetsInitialAsync`，`AssetRegistry.cpp:750`）——扫描未完成时 `GetAssetsByClass` **静默返回不完整结果**（不报错）。故扫描前 MUST 确认注册表就绪（`IsLoadingAssets()` `IAssetRegistry.h:1025` / `WaitForCompletion()` `:830`）。
   - **加载层（R3 本任务）**：同步 `FAssetData::GetAsset()` 取回。R3 是单地图 / 两单位 / 4 属性，**无异步消费者**；且 DefLibrary 就绪是**同步单出口**（`OnDefinitionsReady` 幂等一次）——引入异步须同时把状态机改成 `Unloaded→Loading→Ready` 异步迁移，属无消费者的过度工程（R0 §0.4 第二系统效应）。
   - **加载层归 R7-3**（台账）：`PrimaryAssetTypesToScan` 注册后，**三策略（PreloadAll/PreloadSelected/OnDemand）+ 异步默认 / 同步逃生口**一次到位——引擎 API 已核实齐备：`LoadPrimaryAssets`（异步，返回 `FStreamableHandle`，`AssetManager.h:326`）/ `LoadPrimaryAsset`（`AssetManager.h:340`）/ `LoadPrimaryAssetsWithType`（PreloadAll，`AssetManager.h:354`）/ `PreloadPrimaryAssets`（`AssetManager.h:535`）/ `FStreamableManager::LoadSynchronous`（同步逃生口，`StreamableManager.h:800`）。**硬前提**：`LoadPrimaryAssets*` 族按 `FPrimaryAssetId` 工作，必须先注册类型——未注册时 `GetPrimaryAssetIdList` 静默返回空（`AssetManager.cpp:2134-2152`）。
   - **口径一句话**：**不是"所有 DefAsset 都走 AssetRegistry"，而是"发现走 AssetRegistry（R3）、加载走 AssetManager（R7-3）"**。
6. **四处拍板**（2026-09-21 前置讨论，用户）：DefLibrary 类名 / 发现机制 / 登记时机 / 链资产载体——见下方钉名表。

## Impact
- Affected specs：`integration-entity`（新建）、`effect-chain-asset`（新建）。
- Affected code（`Source/TcsIntegration/`）：新增组件 / 实体查询实现 / DefLibrary / 链资产类；`Build.cs` **须加 `AssetRegistry` 依赖**（扫描用；`Engine` 虽含 AssetRegistry 但那是过渡期临时依赖，见 `Engine.Build.cs:98` 注释）；`TcsEffect` 侧**零改动**（链资产类经 `RegisterChain` 登记，注册表 API 已就位）。
- 决策依据：`06-module-integration.md`（三职责封顶 / 双层引导 / DefLibrary 管辖面 / "把定义库装配到世界"）；`MEM-20260902-05`（生命周期分层：Const 定义 GameInstance 级、可变状态 World 级）；2026-09-17 Def 身份与命名标准；2026-09-21 链资产载体 + 本提案四处拍板；D3-1（Actor 无关性）。
- 验证：UBT 编译零警告 + PIE 实测（组件注册 → 句柄有效 → 触发链 → 扣血；实体查询遍历吐句柄；链资产加载登记）+ 依赖面自检。

## 提案内钉名（计划/设计未钉或需收窄）

| 项 | 钉法 | 依据 |
|---|---|---|
| 链资产类 | `UTcsEffectChainDef : UPrimaryDataAsset`（`Public/Chain/TcsEffectChainDef.h`） | 2026-09-21 用户拍板资产轨；遵 Def 命名标准（去 `Asset` 后缀）；**否决**行轨（嵌套 `FInstancedStruct` 单元格编辑差）与双轨（同步器负担） |
| 实体查询实现类名 | `UTcsPieEntityQuery` | `UTcsEntityQuery` 已被接口的 U 类占用（Task 1 埋的交接注记） |
| 组件注册时机 | `BeginPlay` 注册 / `EndPlay` 注销；**DefLibrary 未就绪 → Warning + 跳过** | 06 文档"施加点 = RegisterEntity 之后、DefLibrary Ready 门禁之后"；时序是宿主责任，不用 ensure 刷屏 |
| 手动触发 API | `ExecuteChainById(FName ChainId)` → 组 `FTcsEffectContext`（`Caster` = 自身句柄、`Targets` = 自身）→ `ExecuteChain` | R3 竖切入口（无触发行）；目标集默认自身（Task 2 的 `FTcsSelSelf` 未落地，故由组件填） |
| DefLibrary 级别 | `UGameInstanceSubsystem` | `MEM-20260902-05`（图鉴反例：无世界也要能查定义）；06 §40 明文 |
| DefLibrary 类名 | **`UTcsDefinitionSubsystem`**（2026-09-21 拍板） | 族内风格一致（全 Tcs 前缀）；06 文档 `UCombatDefLibrarySubsystem` 已回写修正（v4 增补条） |
| 发现机制 | **AssetRegistry 按类扫描**（2026-09-21 拍板）：`IAssetRegistry::GetAssetsByClass(UTcsEffectChainDef::StaticClass()->GetClassPathName(), ...)` | 不提前替 M6 拍板 `PrimaryAssetTypesToScan`；扫描失败显式可见（未注册类型走 AssetManager 会**静默返回空列表**——`AssetManager.cpp:2134-2152`，排障成本高）。M6 换官方路径时只动 DefLibrary 一处。`Build.cs` 须加 `AssetRegistry` |
| 登记时机 | **缓存 + 世界初始化装配**（2026-09-21 拍板）：订阅 `FWorldDelegates::OnPostWorldInitialization`，幂等 | 跨级方向问题：DefLibrary 是 GameInstance 级（跨世界存活）、`UTcsEffectSubsystem` 是 **World 级**（每世界重建）——"ready 时登记一次"会在关卡切换后丢失全部登记（R3 单地图 PIE 测不出）。即 06 "把定义库装配到世界"。**时机已核**：`InitializeSubsystems()`（`World.cpp:2447`）早于 `OnPostWorldInitialization.Broadcast`（`2601`） |
| 链资产登记时机 | **暂定** DefLibrary 自动登记（发现 → 加载 → `RegisterChain`） | 用户 2026-09-21："先暂定 DefLibrary 自动登记，等真正开始开发时再讨论" |
| 加载层归属 | **R7-3**（本任务只做同步 `GetAsset`） | 用户 2026-09-21 拍板；三策略 + 异步默认/同步可选随 `PrimaryAssetTypesToScan` 注册一次到位；引擎 API 已核实齐备（见上方"发现层与加载层分离"条） |
| 流程模板资产化 | **确认方向（合法：模板 = 句子），不在本任务** | 用户 2026-09-21：改名 `TcsDamageFlowTemplate` + 可走资产化；R3 默认模板继续硬编码（官方内置、零内容依赖），资产化等第一个真实内容需求（策划要自定义流程顺序）——**登记台账** |

## 检查点
落点验收 = 编译零警告 + PIE（组件三职责可用 / 实体查询吐句柄 / 链资产登记成功 / 触发链扣血）+ 三处交接注记确认收口。触发行、AttributeSet、流程模板资产类不在本提案。

**步骤 struct 两条纪律（2026-09-21 引擎源码核实，链资产走资产轨的前提）**：①步骤 struct **MUST NOT** 加 `Atomic` / `Immutable` specifier——会退化成 `SerializeBin` 裸二进制，字段增删不再安全（`Class.cpp:3338-3421`）；②步骤 struct **类名改名 = 已存资产失联**（该步骤变空 + `LogCore` Warning，`InstancedStruct.cpp:237-248`），补救 = `[CoreRedirects] +StructRedirects` 且须在 cook 前生效（cooked 构建下 import 重定向被 `RequiresCookedData()` 短路，`LinkerLoad.cpp:2168`）。**字段增删是安全的**（tagged property stream：加字段取默认值、删字段被 `Tag.Size` 跳过）。
