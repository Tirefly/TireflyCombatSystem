## ADDED Requirements

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

`ETcsAssetLoadPolicy` 只表达**各域对自身类型的预载归属声明**，MUST NOT 被实现成加载路径开关（三个策略共用同一条加载路径）。

#### Scenario: 同一资产只加载一次

- **WHEN** 对同一 `FPrimaryAssetId` 连续两次 `LoadAsync`
- **THEN** 底层只发起一次加载，在册项数为 1，且两次返回的句柄都指向同一 `FPrimaryAssetId`

#### Scenario: 句柄计数归零才摘除

- **WHEN** 对同一 `FPrimaryAssetId` 请求两次后先释放一次
- **THEN** 在册项仍在（计数 = 1）；再释放一次后才从在册表移除

### Requirement: 编辑器期规则不触模拟语义

`UTcsAssetButlerSettings : UDeveloperSettings`（`Config = Game, DefaultConfig`，分类名 `Tcs`）MUST 只承载**编辑器/加载期**规则（如扫描路径排除前缀），其字段 **MUST NOT 影响模拟语义**——任何能改变战斗结果的取值 MUST 住对应域的定义资产，而非进程级设置。

#### Scenario: 设置项只影响扫描

- **WHEN** 检查 `UTcsAssetButlerSettings` 的全部 `UPROPERTY`
- **THEN** 每项的消费点都在资产发现/加载路径上，无一项被模拟执行路径读取
