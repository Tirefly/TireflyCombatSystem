# Change: 把 `ETcsAssetLoadPolicy` 从「声明」做实为「契约」——DeveloperSettings 配置 + 定义库按策略异步预载 + 解析口可异步

## Why

`ARCH-03` `D-4` 与本次复核（2026-10-26）共同确认：**三策略今天只有枚举、没有语义，而且实际行为与声明相反**。

| 事实 | 证据 |
|---|---|
| `ETcsAssetLoadPolicy` **读取点 0** | 全仓 `grep "\.Policy\b"` ⇒ 只有注释命中；5 处写入全是注册宏的字面参数 |
| `FTcsAssetLoader` / `FTcsAssetLoadHandle` **模块外引用 0** | 零消费者 |
| 5 个已注册类型**全部声明 `OnDemand`** | `TcsAttributeDef` / `TcsAttrModDef` / `TcsBuffDefAsset` / `TcsEffectChainDef` / `TcsEffectTriggerDefAsset` |
| **但实际行为 = 全量同步加载** | `UTcsDefinitionSubsystem::Initialize` 里 6 条 `Discover*` 全部走 `AssetData.GetAsset()`（同步硬加载） |

⇒ `OnDemand` 的 ToolTip 承诺「**不预载**，首次按 `FPrimaryAssetId` 解析时加载」，实际是**启动期全量同步加载**。**声明与行为正好相反。**

而 `TcsAttributeDef.cpp:14` 的注释还写着「策略用 `OnDemand`：属性定义今天是"宿主/定义库需要时才拿"，不是"启动期全量预热"」——**注释、声明、代码三者互相矛盾**。

## 用户裁定（2026-10-26，逐条，全部已拍板）

| # | 裁定 |
|---|---|
| 1 | `ETcsAssetLoadPolicy` **是契约**，不是纯记录；必须有消费者 |
| 2 | 设 `DeveloperSettings`，**给每种 DefAsset 类型都做一条策略配置** |
| 3 | 定义库按配置的策略执行**异步预加载**——「TCS 插件默认加载，当然是异步，怎么可以用同步加载」 |
| 4 | `LoadPolicyByType` 的键 = **`FPrimaryAssetType`** |
| 5 | `SelectedAssetsByType` 的值 = **`TArray<FPrimaryAssetId>`** |
| 6 | `UTcsStateDef` 加 **`abstract`**，且**不加注册**（它是形状层基类，不是主资产类型） |
| 7 | `UTcsSkillDef` **加注册** |
| 8 | `SeedWorld` **未就绪时拒绝装配、登记待装配世界集合，等就绪回调再装** |
| 9 | `FStreamableHandle` 由 `UTcsDefinitionSubsystem` **持有**（否则 handle 释放 ⇒ 资产被卸载） |
| 10 | **异步加载失败 ⇒ 无法就绪**；并**新增 `OnDefinitionsFailed` 单出口**（与 `OnDefinitionsReady` 对称） |
| 11 | `PreloadAll` **兼查重责任**，重复资产 MUST 用 **Warning** 明确爆出 |
| 12 | `OnDemand` 索引 = **`FPrimaryAssetId` 目录 + 资产总数**（**不做 `DefTag → Id` 映射**——`FPrimaryAssetId` 本就含 `DefTag`） |
| 13 | `OnDemand` 与「`PreloadSelected` 未选中的资产」**默认异步加载**：解析口加「加载模式 + 监听代理」参数——`Async` 返回 `nullptr` + 填代理，`Sync` 直接返回资产；**既有解析函数全部改签名**（实现需求优先） |
| 14 | 代理用引擎原生 **`FStreamableDelegate`**；`ETcsResolveMode` **默认 `Async`** |
| 15 | 「待装配世界」用**弱引用集合** |

## What Changes

### 一、策略进 `DeveloperSettings`（按 Def 类型逐条）

`UTcsAssetButlerSettings` 扩两个字段：

| 字段 | 形态 | 用途 |
|---|---|---|
| `LoadPolicyByType` | `TMap<FPrimaryAssetType, ETcsAssetLoadPolicy>` | 宿主按类型**覆盖**策略；未列出者回落该域 `.cpp` 宏的声明值 |
| `SelectedAssetsByType` | `TMap<FPrimaryAssetType, TArray<FPrimaryAssetId>>` | **`PreloadSelected` 的载体**（补 `ARCH-03` `U-5` 的缺口） |

**为什么是「覆盖」不是「取代」**：`TcsAssetLoader.h:25` 明写「策略是**各域对自身类型的预载归属声明**」——域最懂自己的默认值。设置负责**宿主级覆盖**，宏负责**域级默认**。这是本仓既有的第三态「契约默认」。

### 二、定义库：**发现与加载彻底分离**

今天 `Discover*` 里直接 `AssetData.GetAsset()` ⇒ **「发现」这个动作隐含了全量同步加载**，任何策略都无法生效（策略管加载，而加载被写死在发现里）。**只要发现里带加载，策略就永远是死的。**

```
Initialize():
  for 每个已注册 Def 类型:
    ① 发现：只取 FAssetData（不加载）
    ② 生效策略 = Settings.LoadPolicyByType[Type] ?? Entry.Policy
    ③ 按策略加载（全部异步）:
       PreloadAll:      async LoadPrimaryAssetsWithType(Type)   ← 兼查重：重复名 Warning
       PreloadSelected: async LoadPrimaryAssets(Settings.SelectedAssetsByType[Type])
       OnDemand:        零加载；只记 FPrimaryAssetId 目录 + 资产总数
    ④ 全部完成 → bRuntimeReady = true → OnDefinitionsReady.Broadcast() → 装配「待装配世界」集合
       任一步失败 → bRuntimeReady 保持 false → OnDefinitionsFailed.Broadcast()（永不就绪）
```

### 三、`OnDemand`：只记 `FPrimaryAssetId` 目录，**不做 `DefTag` 映射**

**依据（本轮实测，比我原先以为的更强）**：全部 7 个 Def 类的 `GetPrimaryAssetId()` 都是**纯函数**——

```cpp
return FPrimaryAssetId(PrimaryAssetType, DefTag.GetTagName());   // 逐个核对，无一处读资产内容
```

⇒ **`DefTag` 可以反算 `FPrimaryAssetId`，那张映射表是可被纯函数替代的冗余。**

`OnDemand` 的索引 = **该类型的全部 `FPrimaryAssetId` + 资产总数**（总数用于诊断「发现 12 个 / 预载 3 个」与未来的 Debug 工具）。

### 四、解析口改签名：支持异步（第 13/14 条）

```cpp
enum class ETcsResolveMode : uint8
{
    Async,   // 默认：返回 nullptr + 填代理，加载完成时触发
    Sync,    // 逃生口：直接返回资产指针
};

// 形态示意（6 个 Resolve* 统一改造）
const FTcsBuffDef* ResolveStateDef(
    FGameplayTag DefTag,
    ETcsResolveMode Mode = ETcsResolveMode::Async,
    FStreamableDelegate& InOutOnLoaded = /* 默认空代理 */);
```

- **返回值仍是裸指针**（保住既有形态）：`Async` 模式恒返回 `nullptr`，靠代理回调；`Sync` 模式直接返回；
- **`Async` 下的「已在内存」快路径**：若资产已在内存，MUST **直接返回指针并同步触发代理**（不制造无谓的一帧延迟）——`FStreamableHandle` 本身就有这个快路径；
- **改造代价极小（实测）**：`Resolve*` 的消费者**只有宿主装置 `TcsDev`（12 处断言）**，**插件内部业务代码零消费者**（`grep` 六个域模块 ⇒ 只有注释命中）。

### 五、`abstract` + 注册的三处调整（第 6/7 条）

| 类 | 动作 | 理由 |
|---|---|---|
| `UTcsStateDef` | 加 `abstract`，**不加注册** | 形状层基类，不是主资产类型。加了 `abstract` 后直接实例化会编译期被拒 |
| `UTcsSkillDef` | **加注册** | `GetPrimaryAssetId()` 早已覆写，只差注册；它是独立的 `FPrimaryAssetType` |
| `UTcsBuffDefAsset` | 已注册（不动） | 与 `UTcsSkillDef` 同层，各有独立类型 |

### 六、就绪语义反转（第 10 条）——**推翻今天的一条明文**

| | 今天（`TcsDefinitionSubsystem.h:205-207`） | 本变更后 |
|---|---|---|
| 一条资产损坏 | 就绪 ✅，该条进失败清单，其余可用 | **不就绪** ❌，整个定义库停摆 |
| 宿主 | 自己看 `GetFailureList()` 决定 | 框架替它决定（并广播 `OnDefinitionsFailed`） |
| 失败面 | 局部退化 | **全局阻塞** |

**⚠ 排障成本转移（MUST 记）**：一个拼错的资产路径会让**整个 TCS 停摆**，症状是「组件全部不注册」——看起来是 TCS 没工作，实际是内容里有一个坏文件。

**缓解措施（本变更同批做）**：
- 失败时 MUST 打**一次性的、Error 级的、含完整失败清单的**日志（不是逐条刷屏）；
- `OnDefinitionsFailed` 作为**单出口**，与 `OnDefinitionsReady` 对称，让宿主能主动响应而不是干等。

### 七、`SeedWorld` 的待装配世界集合（第 8/15 条）

今天 `SeedWorld` 由 `OnPostWorldInitialization` 直接调。异步预载把「世界早于就绪」从「几乎不可能」变成**常态** ⇒ 新增：

```
HandlePostWorldInitialization(World):
    if (!IsRuntimeReady())
        登记到 PendingWorlds（**TArray<TWeakObjectPtr<UWorld>>**）
    else
        直接装配

就绪回调里:
    遍历 PendingWorlds，逐个装配，然后清空（失效条目跳过）
```

**弱引用的理由**：`UTcsDefinitionSubsystem` 是 GameInstance 级，世界可能在等待期间被销毁。

## Impact

- Affected specs: `primary-asset-loading`（策略升为契约 + 两个新需求）、`integration-entity`（定义库加载面 + 就绪语义反转 + 失败单出口）
- Affected code:
  - `TcsAssetButlerSettings.h/.cpp`（+2 字段 + 生效策略解析）
  - `TcsDefinitionSubsystem.h/.cpp` + 6 个 `_Xxx.cpp`（发现/加载分离、策略分流、异步、onDefinitionsFailed、PendingWorlds、hold handles、Resolve* 改签名）
  - `TcsState/Public/Def/TcsStateDef.h`（+ `abstract`）
  - `TcsSkill/Private/Def/TcsSkillDef.cpp`（+ 注册）+ `TcsSkill.Build.cs`（+ `TcsAssetButler` 依赖）
  - 宿主装置 `TcsDev` 12 处 `Resolve*` 调用（显式传 `Sync`）
- **BREAKING**：
  - `IsRuntimeReady` 语义从「失败不影响就绪」反转为「有失败即不就绪」
  - 6 个 `Resolve*` 签名加参数，且**默认 `Async`** ⇒ 所有既有调用点（装置 12 处）必须显式传 `Sync` 才能保持原行为
- 不涉及：`ARCH-11` U-1（丙，组件订阅就绪）是**批次 A**，本变更之后再做

## 与批次 A 的顺序（用户裁定：**先 B 后 A**）

| | 批次 B（本变更） | 批次 A（`fix-entity-component-late-registration`） |
|---|---|---|
| 内容 | 加载策略契约化 | 组件订阅就绪、未就绪不再永久失效 |
| 为什么 B 先 | A 修的洞**今天走不到**（`Discover*` 全同步 ⇒ `Initialize` 返回必已就绪）；B 落地后「未就绪」才成为**常态** |
| **A 必须追加的分支** | — | **`OnDefinitionsFailed` 下组件 MUST 放弃并留 Error**——否则「失败即永不就绪」会让组件永久静默挂起 |

## 实测结论（2026-10-26，引擎源码逐层核对，**四条推翻了我原来的假设**）

### R-1 同类型同名资产：引擎**覆盖**，且**已自带三件套警告**

`UAssetManager::TryUpdateCachedAssetData`（`AssetManager.cpp:1630-1698`）：

```cpp
if (OldData && OldData->GetAssetPtr().ToSoftObjectPath() != NewAssetPath)
{
    UE_LOGF(LogAssetManager, Warning, "Found duplicate PrimaryAssetID %ls, path %ls conflicts with ...");
#if WITH_EDITOR
    if (GIsEditor) { /* 通知：Duplicate Asset ID ... rename one to avoid a conflict */ }
#endif
    if (!bAllowDuplicates && !TypeData.Info.bIsEditorOnly)
        ensureMsgf(!OldData, TEXT("Found Duplicate PrimaryAssetID %s! ..."));
}
TypeData.FindOrAddAsset(...);   // ← FindOrAdd = **覆盖**
```

⇒ **我原来写的「`PreloadAll` 兼查重、打 Warning」是重复劳动**（引擎已打 Warning + 编辑器通知 + ensure，且我们分不清是谁打的）。

**引擎不说、而我们必须说的是**：`FindOrAdd` 是覆盖，而扫描顺序**无保证**（引擎源码自注 `// Temporary performance fix while waiting for…`）⇒ **同一个 `DefTag` 在不同次启动可能解析到不同资产**。

⇒ **改为「检测到重复 MUST 拒绝登记」**（用户 2026-10-26 裁定采纳）：这计入失败清单 ⇒ 按「失败即不就绪」阻止定义库启动。理由 = **「随机生效一个」比「全部失败」更糟**——后者能查出来，前者表现为「我改的资产没生效」的间歇性谜题。

### R-2 未注册类型：**静默返空 + 同步触发 OnComplete**

逐层核对（`AssetManager.cpp` / `StreamableManager.cpp`）：

| 层 | 未注册时 |
|---|---|
| `GetPrimaryAssetIdList`（`:2134`） | 找不到类型 ⇒ 返回 false，**零日志** |
| `LoadPrimaryAssetsWithType`（`:2542`） | 空数组转调 `LoadPrimaryAssets` |
| `ChangeBundleStateForPrimaryAssets`（`:2195`） | 空循环，零日志 |
| `CreateCombinedHandle`（`StreamableManager.cpp:2543`） | `ChildHandles.IsEmpty()` ⇒ **同步执行 `OnComplete`**，返回 `nullptr` |

⇒ **「代理立刻被调用」+「句柄 nullptr」+「零 Error 零 Warning」**。只挂 `OnComplete` 的调用方会读成「加载完成、共 0 个资产」⇒ **计入成功** ⇒ 一路走到就绪。

⇒ 唯一可靠判据 = **调用前自查** `GetPrimaryAssetTypeInfo`（`:2156`，未注册返回 false）。规格已写成硬要求。

### R-3 单资产与批量加载的失败面**不同**

`LoadPrimaryAsset` 单资产查不到 `NameData` 时调 `WarnAboutInvalidPrimaryAsset`（`Warning`，受 `bShouldWarnAboutInvalidAssets` 开关控制）；**批量类型级没有这层**。⇒ 两条路径的诊断性 MUST 各自补齐，MUST NOT 假定「批量会像单资产一样报」。

### R-4 扫描顺序无保证

引擎注释自认是临时方案 ⇒ **MUST NOT 依赖任何顺序规则消解重复**（「后注册者胜」「路径序在前者胜」都是错的）。

---

## 开放问题（剩余，实施时处理）

| # | 问题 | 影响 |
|---|---|---|
| Q-1 | `Sync` 模式下「资产不在内存」时用 `LoadSynchronous`——与「插件默认不同步加载」的关系怎么表述？ | 规格措辞：应为「**插件默认路径必须异步；同步只作解析口逃生口**」 |
| Q-2 | `ResolveChain` 的键是 `ChainId`、`ResolveAttrModDef` 的键是 `TemplateTag`（与其他用 `DefTag` 不同）——改签名时是否统一命名 | 影响 6 个函数的签名一致性，实施时定 |
