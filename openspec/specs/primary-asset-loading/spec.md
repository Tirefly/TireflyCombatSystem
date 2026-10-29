# primary-asset-loading Specification

## Purpose
定义**主资产的类型注册与运行期加载**：以**类型擦除**的注册项（`FPrimaryAssetType` + `UClass*` + 扫描路径 + 加载策略）让各域把自己的 Def 资产族**向下注册**进插件最低层模块 `TcsAssetButler`，并以 `UTcsAssetManager` 在引擎启动扫描期把这些注册项补扫进 `UAssetManager`，从而获得「按 `FPrimaryAssetId` 解析主资产」的能力。该层只认引擎词汇，**MUST NOT 认识任何业务 Def 类型**（否则各域无法再向下注册）；定义内容本身的**发现、校验、缓存与世界装配**归上层定义库（`UTcsDefinitionSubsystem`）——本能力只管**加载与注册**，不管**定义是什么**。

## Requirements

### Requirement: 资产加载层模块边界

插件 MUST 提供独立模块 `TcsAssetButler` 承载 **PrimaryAsset 机制**（类型注册表 / 运行期加载 / 加载防重 / 编辑器期规则），且该模块 **MUST NOT 依赖任何 Tcs 业务模块**——它的 `Build.cs` 只允许 `Core` / `CoreUObject` / `Engine` / `DeveloperSettings` / `GameplayTags` / `AssetRegistry`，其 `Public/` 头 **MUST NOT** include 任何业务 Def 头。

理由：各域（含层级最低的 `TcsAttribute`）要能向加载层**向下注册**自己的 Def 资产类型；加载层一旦认识任一业务 Def 类型，它就必须住到那些类型之上，注册边即反向成环。**该环已在本仓实测过同型先例**（同 `FTcsEffectStepExecutorRegistry` 的类型擦除手法）。

MUST NOT 用该模块承载"定义缓存 / 解析"职责——那是上层定义库（`UTcsDefinitionSubsystem`）的职责（**加载注册层低、定义缓存层高**）。

#### Scenario: 加载层零业务依赖

- **WHEN** 检查 `Source/TcsAssetButler/TcsAssetButler.Build.cs` 的 `PublicDependencyModuleNames` 与 `Public/` 下全部头的 `#include`
- **THEN** 依赖项仅为上述六个引擎模块，且零处 include 任何 `Tcs*` 业务头（含 Def 资产类头）

#### Scenario: 各域向下注册不成环

- **WHEN** 检查各域（`TcsEffect` 等）的 `Build.cs`
- **THEN** 其依赖含 `TcsAssetButler`；而 `TcsAssetButler` 的依赖**不含**任何 Tcs 业务模块——依赖方向严格朝下，UBT 可编译通过

### Requirement: 类型擦除的主资产注册项

注册项 `FTcsPrimaryAssetEntry` MUST 只收引擎级词汇——`FPrimaryAssetType Type` / `UClass* BaseClass` / `TArray<FString> ScanPaths` / `bool bHasBlueprintClasses` / `ETcsAssetLoadPolicy Policy` / `bool bIsEditorOnly`——**MUST NOT** 出现任何业务 Def 类型或其字段。

基类 MUST 以**延迟解析**方式登记（存 getter、首次查询时才调用）：登记发生在各域 `.cpp` 的静态初始化期，该时点不假定 UObject 设施就绪（同引擎 `FNativeGameplayTag` 的 `GetIfAllocated()` 手法）。

主资产类型名 MUST 由基类的**反射名派生**（零字符串面）——MUST NOT 由注册处手写字符串，也 MUST NOT 由 ini 提供。

#### Scenario: 注册项不含业务类型

- **WHEN** 检查 `FTcsPrimaryAssetEntry` 的字段集
- **THEN** 字段类型集合 = `FPrimaryAssetType` / 函数指针 / `TArray<FString>` / 枚举 / `bool` / `UClass*`，零业务 Def 类型

#### Scenario: 类型名由反射派生

- **WHEN** 一个域用 `UE_DEFINE_PRIMARY_ASSET_TYPE(UTcsXxxDef, ...)` 自注册
- **THEN** 该注册项的 `Type` 等于 `FPrimaryAssetType(UTcsXxxDef::StaticClass()->GetFName())`——类改名后无需改任何注册处或 ini

#### Scenario: 静态初始化期不触达 UObject

- **WHEN** 注册器（`FTcsPrimaryAssetRegistrar`）在模块静态初始化期构造
- **THEN** 它只把注册项挂入待解析表，**MUST NOT** 调用基类 getter

### Requirement: 注册表唯一性对齐引擎语义

`FTcsPrimaryAssetRegistry` MUST 拒绝同一 `FPrimaryAssetType` 的第二条注册项并报 `Error`——**判据与文案 MUST 对齐引擎**（`UAssetManager::ScanPrimaryAssetTypesFromConfig` 的原话是 `Only a single entry per type is supported`），**MUST NOT 自建第二套唯一性判据**。

#### Scenario: 同类型重复注册被拒

- **WHEN** 两个域以同一个 `FPrimaryAssetType` 各注册一条
- **THEN** 第二条被拒（保留首个）+ `Error` 日志含 `Only a single entry per type` 字样

#### Scenario: 首次查询才触发解析

- **WHEN** 首次调用 `ForEach` / `Find` / `Num`
- **THEN** 全部待解析项在此刻被一次性解析（幂等；后续调用不重复解析）

### Requirement: 启动扫描绕开 ini

`UTcsAssetManager : UAssetManager` MUST 覆写 `StartInitialLoading()`，在 `Super::` 之后遍历注册表对每个已解析项调用 `ScanPathsForPrimaryAssets(Entry.Type, EffectivePaths, Entry.BaseClass, Entry.bHasBlueprintClasses, Entry.bIsEditorOnly)`。

类型清单 **MUST NOT 由 ini 提供**（`PrimaryAssetTypesToScan`）——它由各域代码自注册给出。扫描路径 MUST 经 `UTcsAssetButlerSettings` 的排除前缀过滤后才使用。

补扫 MUST 落在引擎初始扫描的同一批次内：**既不错过 bulk scan，也不延误 `OnCompletedInitialScan`**。

#### Scenario: 补扫落在同一初始扫描批次内

- **WHEN** 引擎启动并执行到 `UEngine::InitializeObjectReferences()`
- **THEN** 日志出现「`StartInitialLoading` 进入补扫段」时 `HasInitialScanCompleted() == false`；随后「`OnCompletedInitialScan` 已广播」在其**之后**到达（非当场同步执行）

#### Scenario: 排除前缀过滤生效

- **WHEN** `UTcsAssetButlerSettings::ExcludedScanPathPrefixes` 含某前缀，而某注册项的扫描路径命中它
- **THEN** 该路径被跳过（日志留痕）；若过滤后路径集为空，则该类型不扫且留 `Warning`

### Requirement: 运行期加载三件与防重

插件 MUST 提供 `ETcsAssetLoadPolicy{PreloadAll, PreloadSelected, OnDemand}`、`FTcsAssetLoader` 与 `FTcsAssetLoadHandle`：

- **异步默认**：`LoadAsync` 走 `UAssetManager::LoadPrimaryAssets` 并持 `FStreamableHandle`；
- **同步逃生口**：`LoadSync` 走 `FStreamableManager::LoadSynchronous`；
- **防重**：同一 `FPrimaryAssetId` **MUST 只加载一次**——重复请求复用同一条底层句柄并把句柄计数 +1；释放时 -1，**归零才移除在册项**。

**`ETcsAssetLoadPolicy` 是契约，不是记录**（2026-10-26 重判，取代此前「只表达归属声明、MUST NOT 被实现成加载路径开关」的口径）：三个策略值 MUST 各自对应**可观察的加载行为差异**，且 MUST 有真实消费者。判据 = **「改变一个类型的策略值，该类型在启动期的加载行为 MUST 随之改变」**；若改变策略不改变任何行为，则该策略是假的。

**三档的语义（MUST 逐档兑现）**：

| 档 | 承诺 | 兑现判据 |
|---|---|---|
| `PreloadAll` | 启动期把该类型下的**全部**主资产加载进内存 | 该类型全部资产在就绪时已在内存 |
| `PreloadSelected` | 只加载**显式列出的**那些主资产 | 清单内的在内存、清单外的**不在** |
| `OnDemand` | **不预载**，首次按 `FPrimaryAssetId` 解析时加载 | 就绪时该类型资产**零个在内存**，但**目录可查** |

**`OnDemand` 的存储形态 MUST NOT 是硬引用**（`TObjectPtr`）——硬引用意味着已加载，与「不预载」矛盾。索引 MUST 为 **`FPrimaryAssetId` 目录 + 该类型的资产总数**；**MUST NOT 建立 `DefTag → FPrimaryAssetId` 映射**——判据：全部 7 个 Def 类的 `GetPrimaryAssetId()` 都是**纯函数**（`return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());`，无一处读资产内容）⇒ `DefTag` 可反算 `FPrimaryAssetId`，该映射是可被纯函数替代的**冗余**。**判据**：就绪时该类型的资产对象数 MUST 为 0。

**装配型类型 MUST NOT 用 `OnDemand`，且该禁令 MUST 硬拒而不是警告**：

- **装配型** = 定义库在 `SeedWorld` 里**逐世界登记**其定义的类型（今天 = 链 / 触发 / 状态 / 技能 / 属性 五类；修正器模板 `UTcsAttrModRef` 的消费面是资产直引用，**不在此列**，故它是唯一合法可用 `OnDemand` 的 Def 类型）；
- **为什么禁令是硬的**：`OnDemand` 承诺零加载 ⇒ 缓存为空 ⇒ 装配 0 条 ⇒ 该门面每次查询都返 `nullptr`，而**启动期零 Error**——症状是「buff 全加不上 / 技能链全不执行」，日志只刷「定义未在本世界登记」，**排障方向被引向错误的地方**；
- **为什么不是警告后自动退化**：配置值被静默改写，正是本需求要消灭的「假策略」（声明与行为相反）；
- **触发后果**：该配置 MUST 计入失败清单并阻止定义库就绪（与「失败即永不就绪」同一口径）。**这条配置在任何情况下都不是宿主的本意**——配了 `OnDemand` 等于要求「装配 0 条」。

**查重：检测到重复 MUST 拒绝登记，MUST NOT 只留 Warning**（2026-10-26 实测修正）：

- 检测到**同一类型下 `PrimaryAssetName` 重复**（`DefTag` 相同而路径不同）时，该身份 MUST 被**拒绝登记**并计入失败清单，该失败 MUST 按「失败即不就绪」口径阻止定义库就绪；
- **为什么不能只留 Warning**：引擎**已经**在 `TryUpdateCachedAssetData`（`AssetManager.cpp:1656-1690`）打过 Warning + 编辑器通知 + `ensureMsgf` 三件套 ⇒ 我们再做一次是重复劳动，且**分不清是谁打的**；
- **引擎没说、而我们必须说的是**：引擎的 `FindOrAddAsset` 对同名是**覆盖**，而扫描顺序**无保证**（引擎源码自注 `// Temporary performance fix while waiting for…`）⇒ **同一个 `DefTag` 在不同次启动可能解析到不同资产**。**「随机生效一个」比「全部失败」更糟**——后者能查出来，前者表现为「我改的资产没生效」的间歇性谜题。

**扫描顺序无保证（MUST NOT 假设）**：MUST NOT 依赖「后注册的赢」「路径字典序在前者胜」这类顺序假设来消解重复——引擎不保证。唯一可接受的做法是**拒绝登记**。

**为什么 `OnDemand` 的索引不需要额外元数据**（MUST 读，防重复发明）：引擎在资产扫描时自动把 `GetPrimaryAssetId()` 写进 AssetRegistry 标签（`Obj.cpp:2517-2524`：`PrimaryAssetTypeTag` / `PrimaryAssetNameTag`），而 TCS 每个 Def 类都覆写了 `GetPrimaryAssetId()` 使 `PrimaryAssetName == DefTag.GetTagName()` ⇒ **`FAssetData` 不加载资产即可得出 `DefTag`**（`FAssetData::GetPrimaryAssetId()` → `RequestGameplayTag(PrimaryAssetName)`）。**MUST NOT** 为此刻意覆写 `GetAssetRegistryTags`——引擎已提供。

> **落地状态（2026-10-17，`ARCH-03` D-4 / `G-8`，如实登记 MUST NOT 读成"能力已具备"）**：以上三件是**API 面**，不是**能力面**。实测——`FTcsAssetLoader` / `FTcsAssetLoadHandle` 在 `TcsAssetButler` 之外的引用 **0**；`ETcsAssetLoadPolicy` 的**读取点 0**（3 处出现全是注册宏的字面参数，无任何一处按策略分支）；三个已注册类型**全部 `OnDemand`**，而 `OnDemand` 的语义（"首次按 `FPrimaryAssetId` 解析时加载"）**没有任何代码兑现**——真实加载走的是 `GetPrimaryAssetData` 与 `TSoftObjectPtr::LoadSynchronous`。⇒ 台账 `INTEG-3` 写的「加载三策略 ✅ 已落」**作 API 语义成立、作能力语义不成立**，本条据此订正为「**枚举与句柄类型已落、策略语义与消费面未落**」。
> **顺序错误（比缺失更值得记）**：同一份计划里，「零消费者不预建」这条判据被用出了**两个相反结论**——Task 8 以它为由**推迟**了 `Discover*` 迁移，却**同期预建**了消费面更空的这三件（Step 6 先于迁移落地）。⇒ 判据：**同一计划内对同一条纪律的两个相反结论，MUST 在计划里显式写明谁优先**，否则先落的那件会被当成"已完成"而不再回头。

#### Scenario: 同一资产只加载一次

- **WHEN** 对同一 `FPrimaryAssetId` 连续两次 `LoadAsync`
- **THEN** 底层只发起一次加载，在册项数为 1，且两次返回的句柄都指向同一 `FPrimaryAssetId`

#### Scenario: 句柄计数归零才摘除

- **WHEN** 对同一 `FPrimaryAssetId` 请求两次后先释放一次
- **THEN** 在册项仍在（计数 = 1）；再释放一次后才从在册表移除

#### Scenario: 改变策略值必然改变加载行为

- **WHEN** 把某 Def 类型的策略从 `OnDemand` 改为 `PreloadAll`（其余配置不变）并重启
- **THEN** 该类型在主资产就绪时的**已加载资产数** MUST 从 0 变为该类型的全部资产数；若两次读数相同，则该策略未兑现（本需求被违反）

#### Scenario: OnDemand 类的资产在就绪时零加载

- **WHEN** 某类型配置为 `OnDemand`（且它不是装配型），定义库报告就绪
- **THEN** 该类型的资产对象数 MUST 为 0，但其 `FPrimaryAssetId` 目录与资产总数 MUST 可查（目录可用、资产未载）

#### Scenario: 装配型类型被配成 OnDemand 时硬拒

- **WHEN** 某装配型 Def 类型（链 / 触发 / 状态 / 技能 / 属性）的生效策略被配成 `OnDemand`
- **THEN** MUST 计入失败清单并留 Error，定义库 MUST NOT 就绪；MUST NOT 以「警告 + 自动改回 PreloadAll」的方式静默修正配置值

#### Scenario: 同名重复资产被拒绝登记

- **WHEN** 某类型下有两个资产的 `PrimaryAssetName`（即 `DefTag`）相同而路径不同
- **THEN** 该身份 MUST 被**拒绝登记**并计入失败清单，MUST NOT 静默选用其中一个；且按「失败即不就绪」口径，定义库 MUST NOT 就绪

#### Scenario: 不依赖扫描顺序消解重复

- **WHEN** 存在同名重复资产
- **THEN** 实现 MUST NOT 通过「后注册者胜」「路径序在前者胜」等顺序规则选定其一——引擎扫描顺序无保证，任何顺序假设都会造成跨启动的不确定解析

### Requirement: 按 Def 类型的加载策略配置

插件 MUST 在 `UTcsAssetButlerSettings`（`UDeveloperSettings`，项目设置 `Tcs` 分类）提供**按 Def 类型逐条**的加载策略配置：

- **`LoadPolicyByType`**（`TMap<FPrimaryAssetType, ETcsAssetLoadPolicy>`）：宿主按类型**覆盖**策略；**未列出的类型 MUST 回落到该类型在 `UE_DEFINE_PRIMARY_ASSET_TYPE` 宏里的声明值**；
- **`SelectedAssetsByType`**（`TMap<FPrimaryAssetType, FTcsAssetIdList>`，其中 `FTcsAssetIdList` 内持 `TArray<FPrimaryAssetId>`）：`PreloadSelected` 档的**选定清单载体**（用 `FPrimaryAssetId` 而非路径——与加载口一致，且 `FPrimaryAssetId` 本就是资产的规范身份）。

**为什么清单要包一层 `USTRUCT`**：UHT 硬拒容器作 `TMap` 的值（`Error: The type 'TArray<FPrimaryAssetId>' can not be used as a value in a TMap`）⇒ `FTcsAssetIdList` 是引擎环境下表达「类型 → 一组身份」的标准形态，**语义与直接嵌套完全一致**。MUST NOT 因此把清单拆成扁平数组——扁平表会让同一类型重复填键，填错时不报错、只静默少加载几个资产。

**为什么是「覆盖」而不是「取代」**：策略的**归属**是各域对自己类型的声明（域最懂自己的默认值）；设置负责**宿主级覆盖**。这是本仓既有的第三态「契约默认」——**默认值住域、覆盖点住设置**，两条腿缺一即不成契约。

**MUST NOT 再设一个「全局默认策略」字段**：那会让宏里的声明值变成**看得见、不起作用的假真相**（`grep` 到的是谎报）。全局默认通过「把各域宏声明改对」表达。

**为什么不把「逐类型配置」做成 ini 的唯一来源**：`DEC-2026-10-13-asset-butler` 裁定②已否决「用 ini 提供类型清单」（类型名走 `StaticClass()` 反射派生，规避改名漏改 ini）——**本需求 MUST NOT 被读成推翻该裁定**：它只让「策略值」可覆盖，「类型清单」仍只来自代码自注册。

#### Scenario: 未配置的类型回落到宏声明值

- **WHEN** 某类型在 `LoadPolicyByType` 里没有条目，而它的 `UE_DEFINE_PRIMARY_ASSET_TYPE` 声明为 `PreloadAll`
- **THEN** 生效策略 MUST 是 `PreloadAll`

#### Scenario: 配置覆盖宏声明

- **WHEN** 某类型宏里声明 `OnDemand`，而 `LoadPolicyByType` 把它配为 `PreloadAll`
- **THEN** 生效策略 MUST 是 `PreloadAll`（配置优先），且其加载行为随之为「全量预载」

#### Scenario: PreloadSelected 缺清单时报错而非静默空载

- **WHEN** 某类型配置为 `PreloadSelected`，但 `SelectedAssetsByType` 里没有它的条目（或清单为空）
- **THEN** MUST 留**具名 Warning** 并退化为「零资产预载」，MUST NOT 静默当作「已预载完成」，也 MUST NOT 退化为 `PreloadAll`

### Requirement: 按策略异步预载并驱动就绪与失败

定义库 MUST **把「发现」与「加载」彻底分离**，并按**生效策略**执行预载：

- **发现 MUST NOT 带加载副作用**：发现阶段只取 `FAssetData`，**MUST NOT** 调 `AssetData.GetAsset()`——**只要发现里带加载，策略就永远是死的**（策略管加载，而加载被写死在发现里）；
- **预载 MUST 异步**（`UAssetManager::LoadPrimaryAssets` / `LoadPrimaryAssetsWithType`）：**MUST NOT 用同步加载作为插件默认预载手段**——插件无法预知宿主资产规模，同步会阻塞游戏线程；
- **`FStreamableHandle` MUST 由定义库持有**至反初始化——否则句柄释放即触发卸载，已「预载」的资产会变成未加载；
- **就绪语义 MUST 改为「按策略该加载的都加载完了」**，因此置位与 `OnDefinitionsReady` 广播 MUST 发生在**异步预载完成之后**，MUST NOT 在 `Initialize` 返回时假定已就绪；
- **异步加载失败 ⇒ MUST NOT 就绪**：任一步失败 MUST 保持 `IsRuntimeReady() == false` **且永不置位**，并 MUST 经 **`OnDefinitionsFailed` 单出口**广播一次（与 `OnDefinitionsReady` 对称）。失败时 MUST 打**一次性、Error 级、含完整失败清单**的日志，MUST NOT 逐条刷屏；
- **未注册类型 MUST 在调用加载前自查，MUST NOT 依赖 `LoadPrimaryAssetsWithType` 的返回值报错**（2026-10-26 实测，见下）；该情形 MUST 计入失败清单并阻止就绪。

**⚠ 未注册类型的真实行为（引擎源码逐层核对，MUST 读，这是最险的静默失效形态）**：

| 层 | 未注册时的行为 |
|---|---|
| `GetPrimaryAssetIdList`（`AssetManager.cpp:2134`） | 找不到类型 ⇒ **返回 false，零日志** |
| `LoadPrimaryAssetsWithType`（`:2542`） | 拿到空数组 ⇒ 转调 `LoadPrimaryAssets` |
| `ChangeBundleStateForPrimaryAssets`（`:2195`） | 空循环 ⇒ 零日志 |
| `CreateCombinedHandle`（`StreamableManager.cpp:2543`） | `ChildHandles.IsEmpty()` ⇒ **同步执行 `OnComplete`** 并**返回 `nullptr`** |

⇒ **完整表现 = 「代理立刻被调用」+「句柄为 `nullptr`」+「零 Error 零 Warning」。** 调用方若只挂 `OnComplete` 就会把它读成「该类型加载完成、共 0 个资产」⇒ **计入成功** ⇒ 一路走到就绪。

**唯一可靠的检测手段 = 调用前自查**：`GetPrimaryAssetTypeInfo(Type, OutInfo)`（`:2156`）在未注册时返回 **false**。MUST 用它（或本仓注册表 `FTcsPrimaryAssetRegistry::Find`）作为判据，MUST NOT 靠「加载返回空 ⇒ 判定未注册」。

**单资产加载与批量加载的失败面不同**（MUST 分开写）：`LoadPrimaryAsset` 走同一函数，但单资产查不到 `NameData` 时会调 `WarnAboutInvalidPrimaryAsset`（`Warning`，且受 `bShouldWarnAboutInvalidAssets` 开关控制）⇒ **单资产有 Warning、批量类型级没有**。两条路径的诊断性 MUST 各自补齐，MUST NOT 假定「批量会像单资产一样报」。

#### Scenario: 预载完成前不就绪

- **WHEN** 定义库 `Initialize` 已返回但某类型的异步预载尚未完成
- **THEN** `IsRuntimeReady()` MUST 为 false，且 `OnDefinitionsReady` MUST NOT 已广播

#### Scenario: 预载完成后恰好广播一次

- **WHEN** 全部类型的预载（按各自策略）完成
- **THEN** `IsRuntimeReady()` 变为 true，`OnDefinitionsReady` 广播**恰好一次**（幂等），且广播时订阅者查 `IsRuntimeReady()` MUST 已为 true

#### Scenario: 加载失败则永不就绪并广播失败

- **WHEN** 某类型的异步加载失败（资产损坏 / 路径错 / 类型未注册）
- **THEN** `IsRuntimeReady()` MUST 恒为 false（**永不置位**），`OnDefinitionsFailed` MUST 广播一次，且 MUST 有 Error 级日志含完整失败清单

#### Scenario: 未注册类型在加载前被自查拦下

- **WHEN** 某 Def 类未 `UE_DEFINE_PRIMARY_ASSET_TYPE` 却被要求预载
- **THEN** MUST 在调用加载**之前**用 `GetPrimaryAssetTypeInfo`（或本仓注册表 `Find`）判定为未注册，MUST 计入失败清单并阻止就绪；MUST NOT 靠「`LoadPrimaryAssetsWithType` 返回空」反推未注册——那会把「未注册」误读成「成功加载 0 个」

#### Scenario: 未注册时不把同步完成回调读成成功

- **WHEN** 误对未注册类型调用了 `LoadPrimaryAssetsWithType`（`OnComplete` 会被同步触发、handle 为 `nullptr`）
- **THEN** 实现 MUST 以自查结果为准，MUST NOT 因「代理被调用」而判定该类型加载成功

### Requirement: 定义解析口支持异步

定义库的按身份解析口（`ResolveChain` / `ResolveTriggerDef` / `ResolveStateDef` / `ResolveAttrModDef` / `ResolveSkillDef`）MUST 支持**异步解析**：

- 签名 MUST 含两个新参数：**加载模式**（`ETcsResolveMode{Async, Sync}`，**默认 `Async`**）与**监听代理**（`FStreamableDelegate`，**按值传**——非 const 引用不能带默认值绑临时量）；
- **返回值仍是裸指针**：`Async` 模式**恒返回 `nullptr`** 并填充代理（加载完成时触发）；`Sync` 模式**直接返回资产指针**；
- **`Async` 的「已在内存」快路径**：若资产已在内存，MUST **直接返回指针并同步触发代理**——MUST NOT 制造无谓的一帧延迟（`FStreamableHandle` 自带该快路径）；
- **5 个解析口 MUST NOT 是 `const`**：`Async` 解析要在加载完成时把内容写进缓存，写缓存就不能 const——这是「按需加载」的直接后果，不是顺手去 const；
- **默认 `Async` 的理由**：插件默认路径必须异步；`Sync` 是**逃生口**，只给「必须在当帧拿到」的调用点，且调用点 MUST 各自说明代价；
- 消费者改造：既有调用点 MUST 显式传 `Sync` 才能保持原行为——插件内部业务代码**零消费者**，只有宿主装置需改。

**为什么只有 5 个而不是 6 个**：属性定义**没有** GI 侧公开解析口（`ARCH-04` U-1 的边界）——它的运行期读口在世界门面 `UTcsAttributeSubsystem::FindAttributeDef`，本类不给 GI 侧读口（加了就是零消费者预建）。

#### Scenario: Async 模式返回空并靠代理交付

- **WHEN** 以默认 `Async` 模式解析一个**不在内存**的 `DefTag`
- **THEN** MUST 返回 `nullptr`，且代理 MUST 在加载完成后被触发

#### Scenario: Async 模式下已加载资产走快路径

- **WHEN** 以 `Async` 模式解析一个**已在内存**的 `DefTag`
- **THEN** MUST 直接返回有效指针，且代理 MUST 被**同步**触发（不延迟一帧）

#### Scenario: Sync 模式直接返回

- **WHEN** 以 `Sync` 模式解析一个不在内存的 `DefTag`
- **THEN** MUST 同步加载并返回有效指针（逃生口语义）

### Requirement: 编辑器期规则不触模拟语义

`UTcsAssetButlerSettings : UDeveloperSettings`（`Config = Game, DefaultConfig`，分类名 `Tcs`）MUST 只承载**编辑器/加载期**规则（如扫描路径排除前缀），其字段 **MUST NOT 影响模拟语义**——任何能改变战斗结果的取值 MUST 住对应域的定义资产，而非进程级设置。

#### Scenario: 设置项只影响扫描

- **WHEN** 检查 `UTcsAssetButlerSettings` 的全部 `UPROPERTY`
- **THEN** 每项的消费点都在资产发现/加载路径上，无一项被模拟执行路径读取

### Requirement: Def 类按类查询口（`FTcsPrimaryAssetQuery`）

插件 MUST 提供 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`，供**上层定义库**按"Def 类"发现资产——它与 `GetAssetsByClass` 的差别是**先经 `UAssetManager` 解析主资产扫描结果、再按类过滤**，而不是直接按当前类路径查 `IAssetRegistry` 索引。

**为什么必须有它**：类**换过模块**后（本仓实测：`UTcsEffectTriggerDefAsset` 从 `TcsIntegration` 搬入 `TcsEffect`、`UTcsStateDef` 家族拆出 `TcsBuff`），`IAssetRegistry::GetAssetsByClass` 按**当前类路径**查索引会**静默返 0**——不报错、不警告，表现为"内容资产全部消失"。该口修复了两处真实回归（`Slice.Run` 2/4→65/0、`State.Run` 6/9→15/0）。

- 异步就绪门禁 MUST 用 `UAssetManager::HasInitialScanCompleted()`（**MUST NOT** 用 `IAssetRegistry::IsLoadingAssets`）——与主资产扫描的收束点在同一个批次上；
- 类路径 MUST 由 `Class->GetClassPathName()` 反射派生（零字符串面），与「类型名由反射派生」同一条纪律；
- 未登记的主资产类型 MUST 可区分于"已登记但零资产"：前者是**退化**（该 Def 类少了 `UE_DEFINE_PRIMARY_ASSET_TYPE`），MUST 留 `Warning` 并进失败清单；后者是**正常的空内容**，MUST NOT 报错。

#### Scenario: 类换模块后仍能发现
- **WHEN** 某 Def 类从模块 A 搬到模块 B，而 `IAssetRegistry` 索引里该资产仍记着旧类路径
- **THEN** `GetAssetsForDefClass` 仍发现到该资产（不会静默返 0）；同一场景下 `GetAssetsByClass` 会返 0

#### Scenario: 未登记类型与零资产可区分
- **WHEN** 某 Def 类没有 `UE_DEFINE_PRIMARY_ASSET_TYPE` 自注册，而另一个已注册的类在当前扫描路径下确实零资产
- **THEN** 前者留 `Warning` 并进失败清单（退化），后者静默通过（正常空内容）——两者 MUST NOT 落同一条日志

> **收录订正（2026-10-17，`ARCH-03` D-1 / `G-6`）**：本能力 6 条需求 / 12 个场景**原本零提及** `FTcsPrimaryAssetQuery`，而它是本模块**唯一有真实消费者的设施**（4 处调用点：定义库的三条 `Discover*`），且修复了两处真实回归。根因 = **执行序**：Task 8（开规格 + 归档）排在 Task 6（建查询口）之前，Task 5（回写）夹在中间 ⇒ 无一轮覆盖。⇒ 与 `G-4`（`state-def-asset` 载体归属假事实）同类：**最高权威层漏写/写错**。
