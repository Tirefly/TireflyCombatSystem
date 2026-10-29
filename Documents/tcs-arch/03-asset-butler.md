# ARCH-03-asset-butler · TcsAssetButler（资产加载层）

- **文档 ID**：`ARCH-03-asset-butler`
- **类型**：模块档案（现状 + 债务 + 未实现）
- **状态**：生效中
- **权威范围**：`TcsAssetButler` **今天实际落地了什么**。本模块是**加载层**：它只认引擎词汇（`FPrimaryAssetType` / `UClass*` / 扫描路径 / 策略），**MUST NOT 认识任何业务 Def 类型**——这条红线不是洁癖，而是"各域能向下注册"的唯一前提（见 §3.2）。
- **最后更新**：2026-10-17（首次审核）

---

## §1 模块定位与边界

**一句话**：把各域自己的 Def 资产族**向下注册**进引擎 PrimaryAsset 机制、在启动扫描期把它们扫进 `UAssetManager`，并提供**按 Def 类发现**的统一入口。

| 项 | 内容 |
|---|---|
| 代码规模 | **1 058 行**（10 个 `.h` + 7 个 `.cpp`）；`TcsAssetButler.Build.cs` 另 29 行 ⇒ 目录总 **1 087 行** |
| 编译期依赖 | **零 Tcs 业务依赖**（`Core` / `CoreUObject` / `Engine` / `DeveloperSettings` / `GameplayTags` / `AssetRegistry`）；与 `TcsCore` / `TcsNotation` **同住层 0** |
| 被谁依赖 | `TcsEffect`（Build.cs:22）/ `TcsBuff`（:30）/ `TcsIntegration`（:37）/ 宿主 `TcsDev`（:44）——**4 个** |
| 对应可执行需求 | `openspec/specs/primary-asset-loading/`（**6 需求 / 12 场景 / 108 行**，由归档变更 `2026-10-15-add-primary-asset-butler` 建立） |
| 旧文档族 | **没有模块规格**。只有 `DEC-2026-10-13-asset-butler`（182 行裁决）+ `PLN-REFACTOR-1` Task 8 + `EVID-2026-10-15-refactor-1-task6`（323 行）+ 台账 `INTEG-3` |
| 落地日期 | 2026-10-15（`PLN-REFACTOR-1` Task 8，含 T1/T2/T3 三项实测） |

### 1.1 六个文件组的职责划分【实测】

| 组 | 文件 | 行 | 职责 |
|---|---|---|---|
| 模块壳 | `TcsAssetButlerModule.h/.cpp` | 26 | **空体、零注册**（注释明写登记发生在各域静态初始化期） |
| 日志 | `Public/TcsAssetButlerLogChannel.h` + `Private/.cpp` | 15 | 独立通道 `LogTcsAssetButler`（D0-6 v2：只 include 本头，不 include `Module.h`） |
| 注册项 | `Asset/TcsPrimaryAssetEntry.h` | 82 | **类型擦除**的注册项结构 + 自注册宏 `UE_DEFINE_PRIMARY_ASSET_TYPE` |
| 注册表 | `Asset/TcsPrimaryAssetRegistry.h/.cpp` | 246 | 进程级单例；延迟解析 + 唯一性对齐引擎 |
| 启动扫描 | `Asset/TcsAssetManager.h/.cpp` | 134 | `UTcsAssetManager : UAssetManager`，覆写 `StartInitialLoading()` |
| 统一查询 | `Asset/TcsPrimaryAssetQuery.h/.cpp` | 165 | `FTcsPrimaryAssetQuery::GetAssetsForDefClass`（**★ 本模块唯一有真实消费者的入口**） |
| 加载三件 | `Asset/TcsAssetLoader.h/.cpp` + `TcsAssetLoadHandle.h` + `TcsAssetLoadPolicy.h` | 293 | 异步 / 同步逃生口 / 防重 / 三策略枚举（**零消费者**，见 §5 D-4） |
| 编辑器设置 | `Asset/TcsAssetButlerSettings.h/.cpp` | 66 | `UTcsAssetButlerSettings`，**唯一字段** `ExcludedScanPathPrefixes` |

### 1.2 本模块最反常的一点：**规格与真实价值错位**

| 事实 | 数值【实测】 |
|---|---|
| 规格收录的需求 | 6 条（模块边界 / 注册项 / 注册表唯一性 / 启动扫描 / **加载三件与防重** / 编辑器规则） |
| 规格里**零消费者**的部分 | **加载三件与防重**（`FTcsAssetLoader` + `FTcsAssetLoadHandle` + 三策略 = **293 行**，全仓外部引用 **0**） |
| 规格里**零提及**的部分 | **`FTcsPrimaryAssetQuery`**（165 行，全仓**唯一有真实调用点**的东西——4 处，且它修复了两处真实回归） |

**⇒ 规格写的那 293 行今天空转；今天在干活的那 165 行规格里没有。** 这是本模块最值得记住的一句话，展开见 §5 D-1 / D-4。

---

## §2 已落地

### 2.1 类型擦除的注册项【实测】

```
Source/TcsAssetButler/Public/Asset/TcsPrimaryAssetEntry.h:26
struct TCSASSETBUTLER_API FTcsPrimaryAssetEntry
```

字段集 = **纯引擎词汇**，零业务 Def 类型：`GetBaseClass`（函数指针）/ `ScanPaths`（`TArray<FString>`）/ `Policy` / `bHasBlueprintClasses` / `bIsEditorOnly` / `Type`（`FPrimaryAssetType`）/ `BaseClass`（`UClass*`）/ `bResolved`。

**关键设计——存 getter 而非直接存类指针**（`:28-29`）：登记发生在各域 `.cpp` 的**静态初始化期**，该时点不假定 UObject 设施就绪。手法与引擎 `FNativeGameplayTag` 的 `GetIfAllocated()` 同源。**解析后** `Type` / `BaseClass` 被填充，字段形态与需求一致。

### 2.2 注册表：延迟解析 + 唯一性对齐引擎【实测】

```
Source/TcsAssetButler/Private/Asset/TcsPrimaryAssetRegistry.cpp
:17   Get()            —— 函数局部静态，单游戏线程访问
:101  ResolveEntry     —— 调 getter；Type 由 BaseClass->GetFName() 派生
:118  AddResolved      —— 唯一性判据（同类型重复 ⇒ Error + ensureMsgf，保留首个）
:135  ResolvePending   —— 幂等，只跑一次
```

| 契约 | 实现 | 与需求的关系 |
|---|---|---|
| 类型名由**反射名派生** | `:112` `FPrimaryAssetType(Entry.BaseClass->GetFName())` | ✅ 零字符串面；类改名不需要改任何注册处或 ini |
| 唯一性**不自建第二套** | `:121` `Entries.Contains(Entry.Type)` | ✅ 报错文案逐字含引擎原话 `Only a single entry per type is supported.` |
| 待解析期 / 已解析期**两套入口、同一套判据** | `Register` 分叉（`:29` 入队 / `:35` 即时解析） | ✅ 两条路径都落到 `AddResolved` |
| 解析失败**不静默保留半成品** | `:105-109` 留 Error 并 return（`bResolved` 仍 false） | ✅ 失败项不进 `ForEach` 遍历面 |

### 2.3 自注册宏：3 处落地 / 7 个 Def 类【实测】

```
TcsEffect/Private/Chain/TcsEffectChainDef.cpp:15            UTcsEffectChainDef         { TEXT("/Game") }  OnDemand
TcsEffect/Private/Trigger/TcsEffectTriggerDefAsset.cpp:15   UTcsEffectTriggerDefAsset  { TEXT("/Game") }  OnDemand
TcsBuff/Private/Def/TcsBuffDefAsset.cpp:24                  UTcsBuffDefAsset           { TEXT("/Game") }  OnDemand
```

全仓 `UPrimaryDataAsset` 的 Def 类共 **7 个**：5 个直接子类（`UTcsAttributeDef` / `UTcsAttrModDef` / `UTcsStateDef` / `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset`）+ 2 个经 `UTcsStateDef` 派生（`UTcsBuffDefAsset` / `UTcsSkillDef`）。**已注册 3 / 7**——与台账 `INTEG-3` 记的"3/7"逐字一致。

三条补登记录（都不在 Task 8 里）：`TcsEffect` 两条来自 Task 8；`TcsBuff` 那条是 **Task 6** 修回归时补的。

### 2.4 启动扫描：绕开 ini + 时序已实证【实测】

```
Source/TcsAssetButler/Private/Asset/TcsAssetManager.cpp:11  UTcsAssetManager::StartInitialLoading()
```

| 步骤 | 锚点 |
|---|---|
| 调 Super（读 `UAssetManagerSettings.PrimaryAssetTypesToScan` + 开 `GInitialBulkScan` 窗口 + 广播 `OnAssetManagerCreatedDelegate`） | `:15` |
| `ForEach` 注册表 → 按 `ExcludedScanPathPrefixes` **前缀过滤**扫描路径 | `:27-51` |
| `ScanPathsForPrimaryAssets(Type, EffectivePaths, BaseClass, bHasBlueprintClasses, bIsEditorOnly)` | `:54-59` |
| `CallOrRegister_OnCompletedInitialScan` 挂时序自证钩子 | `:71` |

**时序结论（T1，实测而非推断）**：补扫**既不错过 bulk scan、也不延误 OnCompletedInitialScan**。证据链 = 引擎读源 + 实跑日志：

- `GInitialBulkScan` 的收束点是 `FCoreDelegates::GetOnPostEngineInit()`，**远晚于** `FinishInitialLoading()` ⇒ 落在同一批次；
- 实跑日志"进入补扫段"时 `HasInitialScanCompleted=0`，而"OnCompletedInitialScan 已广播"在**约 11 秒后**才到 ⇒ **后到而非当场同步**，与读源一致。

**类型清单 MUST NOT 由 ini 提供**（裁定⑤）：全库 `PrimaryAssetTypesToScan` **零命中**【实测】。宿主侧只写了一行 `Config/DefaultEngine.ini:95` 的 `AssetManagerClassName=/Script/TcsAssetButler.TcsAssetManager`。

**T3 结论**：`LoadingPhase` 取 **Default** 已足够且已实证（`LoadClass` 在 `UEngine::InitializeObjectReferences()` 内，而 `LoadStartupModules()` 的 Default 阶段在其之前）。**实测 11 个模块全 Default，本模块未特殊化**【实测：`.uplugin:69-73`】。

### 2.5 统一查询口：本模块唯一在干活的入口【实测】

```
Source/TcsAssetButler/Public/Asset/TcsPrimaryAssetQuery.h:50      class FTcsPrimaryAssetQuery
Source/TcsAssetButler/Private/Asset/TcsPrimaryAssetQuery.cpp:15   GetAssetsForDefClass(UClass*, TArray<FAssetData>&, FString*)
```

**它解决的是一个只有实测才能发现的坑：重定向的「第四层」。**

| 层 | 手段 | 救什么 | 救不了什么 |
|---|---|---|---|
| 1 | 类型名（反射派生） | 类改名 | — |
| 2 | 资产路径重定向 | 资产搬家 | — |
| 3 | `CoreRedirects` 的 `ClassRedirects` | **加载期**把旧类路径映射到新类 | **不改写 AssetRegistry 索引里记录的类路径** |
| **4** | **按类路径的索引查询** | — | **不被任何重定向覆盖**，只能改查询手段 |

机理：资产文件里存的仍是 `/Script/TcsIntegration.TcsEffectChainDef`（逐字节实测），而 `GetAssetsByClass` 传的是**当前**类路径 `/Script/TcsEffect.…` ⇒ 索引里无匹配 ⇒ **静默返 0**。

**实测症状（两条路线都复现）**：`Tcs.Test.Slice.Run` 由基线 3/3 退为 **2/4**、PIE `Tcs.Test.State.Run` 由 15/0 退为 **6/9**；检查 1a 链资产由 **12 个**退为 **0 个**。且**零报错、FailureList 为空**（这一点比回归本身更重要，见 §4.3）。

**修法**：改走 `UAssetManager::GetPrimaryAssetIdList(FPrimaryAssetType)`，**与类路径解耦** ⇒ 与 `FTcsPrimaryAssetRegistry::ResolveEntry` 同口径（反射名派生，零字符串面）。

**修复后读数【实测】**：`Slice.Run` **65/0**、定义库链 **12** / 触发 **2** / 状态 **2**、`State.Run` **15/0**（三轮复现性摘要逐字相同）；**其余 9 条命令逐字未变**（零附带损伤）。

**新增可观测性（相对旧口的正面差异）**：把**未登记**与**已登记但空**分开报告——前者进 `OutDiagnostic` + Warning，后者只留 Log 且算正常状态（`:47-77`）。这正好消解了本仓 2026-10-06 就记下的顾虑「未注册时按类型查询会静默返回空，排障成本高」。

### 2.6 编辑器期设置：一个字段，且有真实消费面【实测】

`UTcsAssetButlerSettings : UDeveloperSettings`（`Config = Game, DefaultConfig`，分类名 `Tcs`），**唯一字段** `ExcludedScanPathPrefixes`。

**红线写在类型注释里**（`:15-21`）：字段 **MUST NOT 影响模拟语义**——设置是进程级单例、无版本、不随内容回滚，接进模拟语义会让"同一份内容在不同机器上算出不同结果"。

**"零消费者不预建"的正面样本**：类注释明写「本类当前只登记『路径排除』这一条**确有消费面**的项，策略默认值等其余项待有真实消费方再加」——消费点 = `TcsAssetManager.cpp:35`。这是本仓少见的**没有预建一批设置项**的地方。

### 2.7 加载三件：API 齐备，**零消费者**【实测】

| 符号 | 行 | 外部引用 |
|---|---|---|
| `FTcsAssetLoader`（`LoadAsync` / `LoadSync` / `NumTrackedLoads` / `ReleaseRequest` / `AcquireHandle`） | 262 | **0** |
| `FTcsAssetLoadHandle`（`AssetId` / `Release()` / `IsValid()`） | 31 | **0** |
| `ETcsAssetLoadPolicy`（`PreloadAll` / `PreloadSelected` / `OnDemand`） | 31 | 写 **3** 处 / **读 0** 处 |

实现本身**是对的**且注释诚实：防重的唯一落点是 `AcquireHandle`（`:105-109` 同 id 复用 + 计数 +1）；`ReleaseRequest` 计数归零才摘除。类注释还明确拒绝"自己再判是否已在内存"（引擎 `FStreamableHandle` 内部已含快路径，自己再判会造出第二套真相）。

**但这三件今天的实际作用是零**（见 §5 D-4）。真正在跑的加载路径是另外两条：`GetPrimaryAssetIdList` + `GetPrimaryAssetData`（本模块 2.5），以及 `TSoftObjectPtr` 的 `LoadSynchronous()`（`TcsBuff/Private/State/TcsBuffModifierMaterializer.cpp:53`）。**全仓 `LoadSynchronous` 仅此 1 处，且完全绕开 `FTcsAssetLoader`。**

### 2.8 三处 UHT/工程教训（写进代码注释了，值钱）【实测】

| # | 教训 | 位置 |
|---|---|---|
| 1 | 枚举值**上方**的注释会被 UHT 自动登记为该值的 `ToolTip`；再显式写 `ToolTip` 即撞一致性检查（Metadata key 'ToolTip' first seen with value…）⇒ 说明文字 MUST 只出现一处 | `TcsAssetLoadPolicy.h:18-21` |
| 2 | 新增 `.cpp` 后若报 `LNK2019`，**真因可能是 Build.cs 缺依赖**（`TcsIntegration.Build.cs` 未列 `TcsAssetButler`），报错只说要找的符号、不指向 Build.cs | `EVID-…task6` §5 |
| 3 | 用 `strings` + grep 判"符号是否导出"会得 0，**且对已知必导出符号也得 0**（量具自身失效）；`dumpbin /EXPORTS` 才自证 | 同上 §6 |

---

## §3 对外契约

### 3.1 稳定契约（其它模块可依赖）

| 符号 | 契约 | 稳定度 |
|---|---|---|
| `UE_DEFINE_PRIMARY_ASSET_TYPE(DefClass, {paths}, Policy)` | 在自己域 `.cpp` 里一行自注册；**类型名自动从 StaticClass() 派生**，改名无需同步 | **稳定**（本模块的主用途） |
| `FTcsPrimaryAssetQuery::GetAssetsForDefClass(UClass*, TArray<FAssetData>&, FString*)` | 按 Def 类发现主资产。返回 false 时 `OutDiagnostic` 非空 = **未登记**（退化），空 = **已登记但零资产**（正常） | **稳定**（但规格未收录，见 D-1） |
| `FTcsPrimaryAssetRegistry` 的 `Get/Find/ForEach/Num/Unregister` | 注册表的读面；`Register` 由宏调用，运行期补登亦可 | 稳定 |
| `UTcsAssetButlerSettings::ExcludedScanPathPrefixes` | 宿主可改的扫描排除前缀（**前缀匹配**，非精确路径） | 稳定 |
| `UTcsAssetManager` 类名 | 宿主 ini 一字（`AssetManagerClassName`）；类名错是 **Fatal** 不是静默回退 | 稳定 |
| `FTcsAssetLoader` / `FTcsAssetLoadHandle` / `ETcsAssetLoadPolicy` | 名义公开，今天零消费者 | **未验证**（无消费面 ⇒ 契约未受检验） |
| `LogTcsAssetButler` | 独立日志通道 | 稳定 |

### 3.2 MUST NOT 依赖（这条红线是本模块存在的全部理由）

**`Public/` 头 MUST NOT include 任何业务 Def 头。** 理由不是风格，而是**架构必然**：

```
各域（含层级最低的 TcsAttribute）── 注册 ──►  TcsAssetButler
若 TcsAssetButler ── 认识 ──► 任一业务 Def 类型
   ⇒ 它必须住到那些类型之上
   ⇒ 各域无法再向下注册（成环，UBT 编译不过）
```

**该环已在本仓实测过**（`DEC-2026-10-13-asset-butler` §2 ①）。同型先例 = `FTcsEffectStepExecutorRegistry::Register(const UScriptStruct*, …)` 一族（宿主 `TcsEffect` 完全不认识 `TcsDamage` 的步骤类型却服务它）。

**验收结果【实测】**：Build.cs 依赖恰好 = 需求列的**六个引擎模块**，`Public/` 十个头只 include 引擎头与本模块内头。

### 3.3 分层：加载注册层（低）vs 定义缓存层（高）

| 层 | 载体 | 认识业务类型？ | 职责 |
|---|---|---|---|
| **加载注册层（低）** | `TcsAssetButler` | **不认识** | 类型注册 + 扫描 + 按类取 `FAssetData` |
| **定义缓存层（高）** | `UTcsDefinitionSubsystem`（`TcsIntegration`） | 认识（缓存 4 种业务 struct） | 解析 / 校验 / 缓存 / 装配到世界 |

**判据**：`UTcsDefinitionSubsystem` 缓存 `FTcsEffectChain` / `FTcsEffectTriggerDef` / `FTcsBuffDef` / `FTcsSkillDefData` ⇒ **它不可能住得比这四者更低**。故"注册住低、缓存住高"是唯一可行切法。

---

## §4 竖切影响

### 4.1 调用方向（谁在什么时候碰它）【实测】

| 时刻 | 调用方 | 调用 | 方向 |
|---|---|---|---|
| 各域 `.cpp` **静态初始化期** | 3 个域 | `UE_DEFINE_PRIMARY_ASSET_TYPE` → `FTcsPrimaryAssetRegistrar` 构造 → `Register` | 域 **向下**注册 |
| `UEngine::InitializeObjectReferences()` 内 | 引擎 | `StartInitialLoading()` → `ForEach` → `ScanPathsForPrimaryAssets` | 引擎 → 加载层 → 引擎 |
| `FinishInitialLoading()`（同一次 `GEngine->Init()` 更晚处） | 引擎 | 广播 `OnCompletedInitialScan` | 引擎 → 自证钩子 |
| 定义库装配时 | `UTcsDefinitionSubsystem` 三处 | `GetAssetsForDefClass`（Chain / Trigger / Buff） | 上层 **调用**加载层 |
| 装置装配时 | 宿主 `TcsDev` 一处 | `GetAssetsForDefClass`（Chain） | 宿主 调用 |

**全仓 `GetAssetsForDefClass` 调用点 = 4 处**【实测】：
`TcsDefinitionSubsystem.cpp:94`（Chain）/ `_State.cpp:21`（Buff）/ `_Trigger.cpp:20`（Trigger）/ `TcsDevSliceRig.cpp:216`（Chain）。
**四处的类都在已注册的 3 个之内** ⇒ 无一命中"未登记"退化路径【实测】。

### 4.2 时序位置（硬约束）

```
LoadStartupModules()（Default 阶段，TcsAssetButler 在此加载）
  └─► GEngine->Init()
        └─► InitializeObjectReferences()
              ├─► LoadClass<UObject>(AssetManagerClassName)   ← 类名错 = Fatal
              └─► AssetManager->StartInitialLoading()          ← 本模块覆写点（补扫）
        └─► …（更晚）… FinishInitialLoading()
              └─► PostInitialAssetScan() ⇒ bHasCompletedInitialScan = true + 广播
        └─► …（更晚）… OnPostEngineInit ⇒ GInitialBulkScan 收束
```

**两条硬约束**：

1. **`TcsAssetButler` 的 LoadingPhase MUST 不晚于 Default**——`AssetManagerClassName` 类加载不到是 **Fatal**（`UnrealEngine.cpp:3826`），不是"没有 AssetManager"的可用态。今天取 Default，已实证。
2. **补扫 MUST 落在初始扫描同一批次内**——若落在收束之后，`OnCompletedInitialScan` 会**当场同步**执行（而实测是后到 11 秒）⇒ 该钩子本身就是判据。

### 4.3 一条竖切级教训：「失败清单为空」不构成「发现层正常」的证据

回归二的症状极隐蔽：`GetAssetsByClass` 返回 false 被既有代码读成"尚无内容"（**正常状态**），于是 `FailureList` **为空**、装置"定义库就绪且失败清单为空"**照常 PASS**，而链资产实际是 0 个。

**⇒ 判据**：发现层 MUST 有**正数读数**（"扫到 N 个"），**不能只有布尔/空清单**。这就是为什么装置里"检查 1a"（正数读数）比"检查 0"（空清单）更值钱。

**这条对下游模块的约束**：任何"按需发现资产"的模块（`TcsAttribute` / `TcsSkill` 走旧口的那两条）今天都处在同一风险面下——它们的类没搬过家，所以旧口还能用；但**"能用"的证据必须是正数读数**，不是"没报错"。

---

## §5 技术债（期望 vs 实际，逐条带证据）

### D-1 ★★★ 规格零收录本模块唯一在干活的入口（最高severity，与 G-4 同类）

| 项 | 内容 |
|---|---|
| **期望** | `openspec` 是**最高权威层**（见 `ARCH-INDEX` §2）。本模块的核心对外设施 `FTcsPrimaryAssetQuery` 必须在 `primary-asset-loading` 里有需求与场景 |
| **实际** | 规格 **6 需求 / 12 场景 / 108 行**，对 `FTcsPrimaryAssetQuery` / `GetAssetsForDefClass` / `GetAssetsByClass` / 重定向四层 **零提及**【实测：对该文件 grep 上述模式，命中 **0**】 |
| **偏差证据** | 变更 `add-primary-asset-butler` 归档于 **2026-10-15**；而 `FTcsPrimaryAssetQuery` 是 **Task 6** 建立的——执行序为 1→2→3→4→8→5→6→7，**Task 8（开提案+归档）在 Task 6 之前**。Task 8 的《实施记录》只登记了 1 条待回写（`integration-entity`，归 Task 5），而 Task 5 也已在 Task 6 **之前**跑完 ⇒ **没有任何一轮消费这个新符号**【推断，依据执行序 + 两份实施记录】 |
| **影响面** | ① 只读规格的后续实现者**不会知道这个口存在**，极可能重新引入 `GetAssetsByClass` 并重演两处回归；② 「重定向第四层」这条只有踩过才知道的机理，今天只活在 `TcsPrimaryAssetQuery.h` 头注释与 `DefaultEngine.ini` 批注里，**不在契约层** |
| **是否已登记** | **否**。台账 `INTEG-3` 记的是"发现面 3/5 条改走新统一口"（准确），但那是**进度记录**，不是契约 |

### D-2 ★★ `integration-entity` 规格的发现机制描述已假，且它正是 Task 8 登记过的待回写

| 项 | 内容 |
|---|---|
| **期望** | Task 8《实施记录》明确登记：`integration-entity` 能力的主规格仍描述「定义发现走 `IAssetRegistry::GetAssetsByClass`、不依赖 `PrimaryAssetTypesToScan` 注册」… ⇒ **Task 5 MUST 补写该能力的"主资产注册"面** |
| **实际** | `openspec/specs/integration-entity/spec.md:51` **至今原文未动**：「`IAssetRegistry::GetAssetsByClass`（不依赖 `PrimaryAssetTypesToScan` 注册——属 M6 轮）；**五条按类发现路径**」【实测】 |
| **偏差证据** | 五条里已有 **3 条**改走 `FTcsPrimaryAssetQuery`（Chain / Trigger / State）⇒「五条按类发现路径」对其中 3 条**已是事实错误**；且"不依赖注册"这半句与 2.3 的 3 处自注册**直接冲突** |
| **影响面** | 该规格是 `TcsIntegration` 的主契约。读者会以为 5 条仍同构，从而在新增 Def 族时选错手段 |
| **是否已登记** | **部分**——登记在**计划实施记录**里（不是规格、不是台账），且所指的 Task 5 已跑完未消费 ⇒ 实际处于**无人盯**状态 |

### D-3 ★ 四处代码注释 + 两处 Build.cs 批注仍称走旧口

| 位置 | 现文 | 实况 |
|---|---|---|
| `TcsIntegration/Public/TcsDefinitionSubsystem.h:37` | 「…（`DiscoverSkillDefs`，2026-10-06 R6 Task 1）——**均走 `IAssetRegistry::GetAssetsByClass`**」 | 5 条里 3 条已改，「均」**已假** |
| `TcsEffect/Public/Chain/TcsEffectChainDef.h:45` | 「R3 的发现走 `IAssetRegistry::GetAssetsByClass`」 | **已假**（该类正是改走新口的三条之一） |
| `TcsEffect/Public/Trigger/TcsEffectTriggerDefAsset.h:54` | 「当前发现走 `IAssetRegistry::GetAssetsByClass`」 | **已假** |
| `TcsState/Public/Def/TcsStateDef.h:43` | 「当前发现走 `IAssetRegistry::GetAssetsByClass`」 | **已假**（状态发现现查 `UTcsBuffDefAsset`，走新口） |
| `TcsIntegration.Build.cs:35` | 「五条 `Discover*` 由 `GetAssetsByClass` 改走加载层统一口」 | **过度声称**：实为 **3/5** |
| `TcsBuff.Build.cs:28` | 「旧的按类路径发现（`GetAssetsByClass`）静默返 0」 | 表述正确（只讲机理，未声称全改） |

**性质**：**实现超文档的镜像**——代码改了，注释/批注没跟上。**影响面**：注释是开发者的一等信息源，此处会误导"新增 Def 族该用哪个口"。**已登记**：否。

### D-4 ★★★ 加载三件的规格要求已兑现，但**消费面从未存在** —— **✅ 2026-10-26 闭合（R-21）**

> **闭合证据**：`refactor-asset-load-policy-as-contract` 落地。三策略各自兑现为可观察的加载行为差异（`LoadDefinitions` 的三档分流），配置面由 `UTcsAssetButlerSettings::LoadPolicyByType` + `ResolveLoadPolicy` 提供，消费点 = 定义库按类型分支加载。`ETcsAssetLoadPolicy` 读取点从 **0** 变为 `TcsDefinitionSubsystem.cpp` 的双分支 + `ResolveLoadPolicy`。**D-7 的 RAII 隐患随之失效**：定义库改为直接持 `TSharedPtr<FStreamableHandle>`（`PreloadHandles`），不再经过 `FTcsAssetLoadHandle` 的 `RefCount` 手工计数。
>
> **顺序问题本身仍然成立**（"同一份计划里同一条判据用出两个相反结论"）——那是过程教训，不随代码闭合而消失。

| 项 | 内容 |
|---|---|
| **期望** | `primary-asset-loading` 需求「运行期加载三件与防重」写明：三策略的**消费点是各域的预载决策**（"哪些类型该在启动期全部预热"），并给了 2 个场景（同资产只加载一次 / 计数归零才摘除） |
| **实际** | `FTcsAssetLoader` / `FTcsAssetLoadHandle` 外部引用 **0**；`ETcsAssetLoadPolicy` **读 0 处**（3 处写入全是注册宏的字面参数）⇒ **"各域预载决策点"根本不存在**；三处注册**全部 OnDemand**，而 `OnDemand` 的语义（"首次按 `FPrimaryAssetId` 解析时加载"）**没有任何代码兑现**——真实加载走 `GetPrimaryAssetData` 或 `TSoftObjectPtr::LoadSynchronous` |
| **偏差证据** | 全仓 grep：`FTcsAssetLoader` 0、`NumTrackedLoads` 0、`LoadAsync` 0、`TcsAssetLoader.h` 0（模块外）；`LoadSynchronous` 仅 `TcsBuffModifierMaterializer.cpp:53` 一处且不经过本模块 |
| **影响面** | **293 行有维护成本、零收益**：任何对 `FTcsAssetLoader` 的"契约依赖"都是空中楼阁（该契约**从未被任何调用点检验过**）。同时它给规格一个**虚假的完成度**——读者会认为"运行期加载已具备"，实际"按需加载"这件事在仓里是 `TSoftObjectPtr` 直取 |
| **是否已登记** | **部分**。台账 `INTEG-3` 写「③ 加载三策略 ✅ 已落」——**"已落"作 API 语义成立，作能力语义不成立**。这个措辞差异正是本条要修的 |
| **设计是否还成立** | **成立**（异步默认 + 同步逃生口 + 防重是三件正确的事）。**但顺序错了**：Task 8 以"零消费者不预建"为判据**推迟**了 `Discover*` 迁移（Step 8 / T1 注记），却**同期预建**了消费面更空的加载三件（Step 6）。同一份计划里**同一条判据用出了两个相反结论** |

### D-5 ★ 决策文档的状态行仍写「实现状态 = 零落地」

`DEC-2026-10-13-asset-butler` 的状态行（`:7`）：「**FROZEN**（2026-10-13 拍板；**实现状态 = 零落地**——本裁决只定分层与归属，实施属新建计划）」。

**实际**：2026-10-15 Task 8 已全量落地（1 058 行 + 双配置 0 error）。该句在当时**是准确的**（决策写于 10-13，实施在 10-15），但今天会让读者以为"这个模块还没建"。**已登记**：否（属旧文档族冻结面，但它是**唯一**描述本模块的决策文档，误读代价高）。

**处置建议**：旧文档族已冻结为历史真相源，故**不改原文**，由本档案承担现状描述——但需在 `ARCH-INDEX` §1.3 的"声称未落地"类下点明这是一处**时间性滞后**而非事实错误。

### D-6 ☆ `TcsPrimaryAssetQuery.cpp:99` 的 return 丢失缩进

```
99| return !OutAssets.IsEmpty();      ← 顶格（其余全部为制表符缩进）
```

**性质**：纯样式瑕疵，零行为影响。**已登记**：否。列入此处仅为让下一次触碰该文件的人顺手修。

### D-7 ★★ `FTcsAssetLoadHandle` 无 RAII：忘 `Release()` 即永久泄漏计数

`FTcsAssetLoadHandle` 是**纯值类型**（`AssetId` + 两个成员函数），**无析构、无拷贝语义约束**。而 `FTcsAssetLoader` 的在册表靠 `RefCount` 决定何时摘除。

| 误用 | 后果 |
|---|---|
| 拿到句柄后直接丢弃（不调 `Release`） | `RefCount` 永久 ≥1 ⇒ 在册项**永不摘除**，底层 `FStreamableHandle` **永不释放** |
| 同一句柄 `Release` 两次 | **安全**——`Release` 内首行把 `AssetId` 清空（`:19`），第二次是 no-op |

**为什么是债而非缺陷**：`TSharedPtr` 本身是引用计数的，这里又手工维护一套 `RefCount`，两者**语义重复且只对齐了一半**（`TSharedPtr` 无需人工释放，`RefCount` 需要）。类注释解释了动机（`FStreamableHandle` 自身没有公开的引用计数查询），但**没有采用 `TSharedPtr` 自定义删除器或 RAII 包装**这条更省心的路。

**影响面**：今天为零（无消费者）。**一旦有人用**，忘记 `Release` 是**静默**的（不报错、不 ensure），只表现为"资产永不卸载"。**已登记**：否。

---

## §6 未实现（设计是否还成立 + 前置条件）

### U-1 ★ 4 个 Def 类的自注册未落（3/7 → 7/7） —— **✅ 2026-10-26 闭合（R-21）**

> **闭合结果**：`UTcsAttributeDef`（2026-10-17 已补）、`UTcsAttrModDef`（原有）、`UTcsSkillDef`（本次补）均已在册 ⇒ **7 个 Def 类里 6 个注册**，唯一不注册的是 `UTcsStateDef`——用户 2026-10-26 裁定第 6 条明确「形状层基类，**不注册**」并给它加了 `abstract`。⇒ 本条目标从「7/7」订正为「**6 个实体类型 + 1 个基类明确不注册**」。

| Def 类 | 域 | 现状 |
|---|---|---|
| `UTcsAttributeDef` / `UTcsAttrModDef` | `TcsAttribute` | 未注册；Build.cs 也无 `TcsAssetButler` 依赖 |
| `UTcsStateDef` | `TcsState` | 未注册（作为基类，自身是否需独立登记属**待裁**） |
| `UTcsSkillDef` | `TcsSkill` | 未注册；Build.cs 无依赖 |

- **设计是否还成立**：**成立**。但今天它的**性质变了**——从"可选统一"变成"**迁移前的硬前置**"：`FTcsPrimaryAssetQuery` 对未登记类型直接返回失败，故**任何**想改走新口的域 MUST **先**补注册，否则从"能用"直接退化成"Warning + 零资产"。
- **当初依据今天是否仍有效**：「零消费者不预建」仍有效（今天没有"按 `FPrimaryAssetId` 拿这四域定义"的调用点）。`DiscoverAttrModDefs` / `DiscoverSkillDefs` 走旧口且**类未搬过家**，故实测仍正常（命中 1 / 5）。
- **前置条件**：补注册与迁移那两条 `Discover*` MUST **同批**做，顺序 = 注册 → 迁移。**MUST NOT** 只做迁移。
- **已登记**：✅ 台账 `INTEG-3` 逐项在册。

### U-2 ★ 发现面 2/5 未迁（`DiscoverAttrModDefs` / `DiscoverSkillDefs`） —— **✅ 2026-10-26 闭合（R-21）**

> **闭合结果**：六族发现统一走 `FTcsPrimaryAssetQuery::GetAssetsForDefClass`，六条 `Discover*` 收口为一条 `DiscoverDefs`（只取 `FAssetData`、零加载）。反查 `GetAssetsByClass` 在 `TcsIntegration` 下**零命中**（仅注释提及）。

`TcsDefinitionSubsystem_AttrModDef.cpp:31` 与 `_Skill.cpp:32` 仍走 `AssetRegistry->GetAssetsByClass`【实测】。

- **设计是否还成立**：**成立**，但**风险已变化**：这两条今天正常**只因为它们的类没搬过家**。一旦 `TcsAttribute` 或 `TcsSkill` 的 Def 类因任何原因换模块（拆模块、合并模块），这两条会**静默返 0**——且症状与回归二**逐字相同**（失败清单为空、装置 PASS）。
- **当初依据今天是否仍有效**：「类未搬 ⇒ 旧口正常 ⇒ 迁属架构统一、非缺陷修复」——**依据有效，但结论是脆的**。
- **前置条件**：依赖 U-1 的注册先落。
- **已登记**：✅ 台账 `INTEG-3`。

### U-3 就绪状态机（Unloaded → Loading → Ready/Failed）未落

台账 `INTEG-3` ④ 项：DefLibrary 状态机升级为带 Loading 态的完整版…**仍未落**（归属 R7 不变）。

- **设计是否还成立**：**存疑**。该设计定于 R3 期（同步单出口时代），而本模块已提供**异步加载路径**（`LoadAsync`）——异步一旦启用，Loading 态才**真正必要**。今天异步零消费者 ⇒ Loading 态**无对象**。
- **前置条件**：先有异步加载的真实消费点，再谈 Loading 态。**顺序 MUST NOT 颠倒**（否则又是一个"预建"）。
- **注意**：该状态机**不在** `primary-asset-loading` 规格内，只在台账。若将来落，须先补规格。

### U-4 `AssetManagerClassName` 错类名的硬失败面（E2 的 Fatal）未构造

- **设计是否还成立**：成立。但它是**故意不构造**——"故意触发失败输出"按本仓纪律 MUST 独立 opt-in 命令。
- **性质**：**不是遗漏**，是纪律性留白。
- **已登记**：✅ `EVID-…task6` 与 Task 8 实施记录均如实登记"未构造"。

### U-5 `PreloadAll` / `PreloadSelected` 两个策略值无任何语义 —— **✅ 2026-10-26 闭合（R-21）**

> **闭合结果**：三档全部有语义。`PreloadAll` = 定义库按类型异步全量预载（今天 5 个装配型类型的默认值）；`PreloadSelected` = 只装 `SelectedAssetsByType` 清单；`OnDemand` = 零预载 + 目录可查 + 解析口按需加载。**判据**（规格已写死）：「改变一个类型的策略值，该类型在启动期的加载行为 MUST 随之改变」。
>
> **新增一条硬禁令（本次裁定的直接产物）**：**装配型类型 MUST NOT 用 `OnDemand`**，且该禁令 MUST 硬拒（计入失败清单 + 不就绪），不是警告后自动退化。理由：装配型类型在世界初始化时会被逐世界登记，`OnDemand` 的零加载会让装配为空，而**启动期零 Error**——症状是「buff 全加不上」，日志却只刷「定义未在本世界登记」，排障方向被引向错误的地方。

3 处注册全为 `OnDemand`；另两个值**从未被任何代码读过**（见 D-4）。

- **设计是否还成立**：**成立但无对象**。三策略是 DEC 裁定① 的原文要求（"运行期加载三策略"），故**不该删枚举**；但**"已落"这个说法 MUST 精确化为"枚举已落、语义未落"**。
- **前置条件**：出现真实的预载需求（如"属性定义数量少且全程必需 ⇒ PreloadAll"）。
- **已登记**：⚠️ 台账记的是"✅ 已落"——**措辞需订正**（见 D-4 末行）。

### U-6 编辑器期跨资产解析：**已裁定不建**（不是未实现）

`DEC-2026-10-13-asset-butler` 裁定⑧：**不建**——该需求已被 `FGameplayTag` 消除（"都是以前 `DefId` 为 `FName` 时的设想"）。

**必须区分**：这**不是**本档案 §6 意义上的"未实现"。它是**已撤销的设计**，且实测支持撤销（`RegisterAttributeDef` / `FindAttributeDef` 均按 `FGameplayTag` 键，与声明顺序无关）。列在此处只为**防止被当成待办重新捡起**。

---

## §7 文档资产对照

| 文档 | 体量 | 与本模块的关系 | 时效 |
|---|---|---|---|
| `openspec/specs/primary-asset-loading/spec.md` | 108 行 / 6 需求 / 12 场景 | **契约层（最高权威）** | ⚠️ **漏 `FTcsPrimaryAssetQuery`**（D-1） |
| `openspec/specs/integration-entity/spec.md` | — | `TcsIntegration` 主契约，含发现机制描述 | ❌ `:51` 已假（D-2） |
| `decisions/dec-2026-10-13-asset-butler.md` | 182 行 | **唯一**描述本模块设计的决策文档（8 条裁定 + 8 条引擎事实 E1–E8 + 4 项待实测 T1–T4） | ⚠️ 状态行「零落地」已过时（D-5）；T1/T2/T3 已由 Task 8 实测闭合，T4 未验 |
| `plans/plan-refactor-1-runtime-base.md` Task 8 段 | 1892–1989 行 | 实施形状 + **T1/T2/T3 实测注记**（引擎读源级） | ✅ 当前（含 T2 方向性出入的订正） |
| 同上，Task 8 实施记录 | 1469–1564 行 | 落地清单 + 3 条红线验收 + 4 条未落地项 | ✅ 当前 |
| `evidence/2026-10-15-refactor-1-task6.md` | 323 行 | **两处真实回归的发现与修复**（重定向第四层的原始取证） | ✅ 当前（§7 边界第 1 条：PIE 面未复验） |
| `ledger/deferred-inputs-ledger.md` 的 `INTEG-3` | — | 进度真相源（3/7、3/5、三策略、状态机四项） | ✅ **最准确的一份**（除"三策略已落"措辞，见 D-4） |
| `Config/DefaultEngine.ini` 的 `:90-130` | 41 行批注 | **重定向四层 + T2 实测结论**的唯一落地说明 | ✅ 当前 |

**⇒ 本模块的文档形态是"证据极厚、契约有洞"**：实施级取证（引擎读源 + 双路线复现 + 阴性对照方法论）是本仓最扎实的一批；但**最高权威层（openspec）漏了最该收录的那个入口**。

---

## §8 审核记录

| 项 | 结论 |
|---|---|
| 审核方法 | 读 `primary-asset-loading` 全部 6 需求 → 提取 `Source/TcsAssetButler/` 全部公开符号（17 文件 / 1 058 行）→ 与旧 DEC 与 plan 逐项对照 → 对每个符号全仓 grep 消费者 → 逐条标置信度 |
| 实测命令 | find + wc 行数统计 / 对 6 个公开符号全仓 grep / `grep '### Requirement:'` 计数 / `cat -A` 核缩进 / grep `UPrimaryDataAsset` 枚举 Def 类 |
| 自纠一处 | 初审时曾据旧记忆认为"本模块**无**模块规格"——实测 `primary-asset-loading` **存在**（108 行 / 6 需求）。该纠正已回写 `ARCH-INDEX` §3.2 |
| 未能确认 | `UTcsStateDef`（基类）是否**需要**独立登记（U-1 表内标"待裁"）——需在迁移 `DiscoverStateDefs` 时定 |
| 未覆盖 | 本模块**无装置命令**（无 `Tcs.Test.*` 对应面），故"回归已闭合"的证据全部来自 `TcsIntegration` 的装置读数，非本模块自身 |

---

## §9 结论

1. **代码质量高**：零业务依赖这条红线守得干净（`Public/` 十头零业务 include），类型擦除手法对齐本仓既有先例，唯一性判据**主动对齐引擎而非自建**，设置类**只登记确有消费面的一个字段**。

2. **债务集中在"规格与消费面的错位"**，不在实现：
   - 规格写了 **293 行零消费者**的东西（D-4）；
   - 规格**漏了 165 行唯一在干活**的东西（D-1）；
   - 两者互为镜像，根因是**执行序**：Task 8（开规格）在 Task 6（建查询口）之前，而 Task 5（回写规格）也夹在中间 ⇒ 无一轮覆盖。

3. **本模块最该被记住的一句**：**「重定向有四层，第四层只能改查询手段」**——这条只有踩过才知道的机理，今天住在代码头注释里，**不在契约层**。

4. **给下一模块（`TcsAttribute`）的三个关注点**：
   - 它是 U-1 **最大的缺口**（2 个 Def 类未注册），且它的 Build.cs 至今不依赖 `TcsAssetButler`——判"这是有意还是漏了"；
   - 它的 `DiscoverAttrModDefs` 走旧口（U-2），风险面与 §4.3 的"失败清单为空不等于正常"直接相关；
   - 它是**层级最低的业务模块**（依赖 Core + Notation）。DEC 里"含层级最低的 `TcsAttribute` 要向加载层向下注册"这句话是加载层住层 0 的**理由本身**——核实这个理由今天是否已由代码兑现。
