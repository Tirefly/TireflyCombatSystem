# 资产加载层裁决：新建 `TcsAssetButler`、分层让位与四处搬迁

> **术语订正（2026-10-14）**：本文原用「单位 / `Unit`」，该词已退役，现统一为「**实体 / `Entity`**」（见 `DEC-2026-10-14-entity-vocabulary`）。**正文为当时原话，保留不改。**

- **文档 ID**：`DEC-2026-10-13-asset-butler`
- **类型**：DEC / 决策记录（架构定位拍板）
- **状态**：FROZEN（2026-10-13 拍板；**实现状态 = 零落地**——本裁决只定分层与归属，实施属新建计划）
- **权威范围**：Def 类资产的**加载/注册层**的模块归属与形状（新模块 `TcsAssetButler`）、它与**定义缓存层**（`UTcsDefinitionSubsystem`）的分层边界、**四处现有类型的搬迁归属**、以及 StateTree 的模块归属口径。**不在本文**——运行态基底化与 `TcsBuff` 拆出（住 [`DEC-2026-10-13-state-module-positioning`](dec-2026-10-13-state-module-positioning.md)）、`R5.5-e` 关系表族语义、各 Def 族的字段形状
- **最后更新**：2026-10-13

> **性质**：本裁决源自用户 2026-10-13 的一条质疑——「按现有计划，未来的 `TcsBuff`、`TcsSkill`、`TcsAttribute` 甚至 `TcsCue` 可能都要引用 `TcsIntegration` 才能注册自己的 DefAsset，但这样就会让 `TcsEffectChainDef`、`TcsEffectTriggerDefAsset`、`TcsCombatEntityComponent`、`TcsPieEntityQuery` 也被其他模块引用，**这不对吧**」。该质疑**经实测成立**（§2 ①），并连带推翻了一条更早的合并提议（§2 ②）。
>
> **证据标注**：【源码取证】= 本机全仓实测（`Plugins/Tirefly/TireflyCombatSystem` + LAC 宿主 + `Config/`）；【引擎源码】= 本机 UE 5.8.3（`E:\UnrealEngine\UE_5.8`，`++UE5+Release-5.8`，CL **58210709**，`Build.version` 实测）逐条读源；【用户拍板】= 2026-10-13 本会话逐项裁定。
>
> **★ 一处取证更正（MUST 读）**：本会话早期我曾判定"本机**未定位到 UE 引擎源码树**"（该结论已写进 `DEC-2026-10-13-relation-semantics` 的取证边界节）。**该判定是错的**——引擎源码在 **`E:\UnrealEngine\UE_5.8`**（本机另有 `UE_5.6_ex` / `UE_5.7_ex`），我早期只探了 `C:\Program Files\Epic Games` 与注册表。⇒ **本文所有引擎事实均改为逐条读源**，并已回头订正那份裁决的边界声明。

---

## 0. 裁定总表（2026-10-13，用户逐项拍板）

| # | 议题 | 裁定 | 判据一句话 | 影响面 |
|---|---|---|---|---|
| **①** | **新建 `TcsAssetButler` 模块** | 只承载 **PrimaryAsset 机制**：类型注册表 / 运行期加载三策略 / 异步默认 + 同步逃生口 / 加载防重 / 编辑器期规则 | 用户原话：「只负责 PrimaryAsset 的运行期加载三方案…以及编辑器期间 PrimaryAsset 的相关机制，**不再负责其他任何内容**」 | 新模块 |
| **②** | **它的位置** | **住低层**（`TcsCore` 旁），**不依赖任何 Tcs 业务模块** | 各域（含层级最低的 `TcsAttribute`）要向它注册 ⇒ 它 MUST 住得比所有注册者都低，否则成环（§2 ①） | 依赖链 |
| **③** | **注册键 MUST 类型擦除** | 注册表只收 `FPrimaryAssetType` + `UClass*` + 扫描路径 + 策略，**MUST NOT** 认识任何业务 Def 类型 | **本仓已有四处同型先例**：`FTcsEffectStepExecutorRegistry::Register(const UScriptStruct*, ...)` 等，其宿主 `TcsEffect` 完全不认识 `TcsDamage` 的步骤类型却服务它 | 新模块 API |
| **④** | **分层：加载层 vs 缓存层** | **加载注册层 = `TcsAssetButler`（低、泛型）**；**定义缓存层 = `UTcsDefinitionSubsystem`（高、认识业务类型）**，二者 MUST 分开 | §2 ③ 的"中央矛盾"据此消解 | 两处 |
| **⑤** | **四处搬迁** | `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` → **`TcsEffect`**；`UTcsCombatEntityComponent` / `UTcsPieEntityQuery` → **实体接线层**（与加载无关） | 各自消费面实测（§3）：前两者是链/触发机制的家，后两者是 Actor 接线且**零资产引用** | 4 类型 |
| **⑥** | **`TcsDef` 方案作废** | 撤销"新建 `TcsDef`（承载共用 Def 族）"与"`TcsDef` 并入 `TcsIntegration`"两个中间提议 | 前者无法被下层依赖（成环）；后者把加载层推到最顶层（§2 ②） | — |
| **⑦** | **StateTree 的归属口径** | StateTree 若要引入，**将来单独成 `TcsStateTree` 模块**，**MUST NOT** 让别的模块承载它 | 用户原话：「就算要加，也应该之后单独开一个 `TcsStateTree` 模块，而不是污染其他模块」 | §4 |
| **⑧** | **编辑器期跨资产解析** | **不建**——该需求已被 `FGameplayTag` 消除 | 用户原话：「这些都是以前 `DefId` 为 `FName` 时的设想，现在 `DefId` 都是 GameplayTag，已经不再需要单独做这种编辑器机制了」（§5 实测支持） | — |

---

## 1. 勘察（现状是什么）

### 1.1 依赖层级实测（【源码取证】）

```
TcsCore ─┬─ TcsNotation
         └─ TcsAttribute ─→ TcsEffect ─→ TcsState ─→ TcsSkill ─→ TcsIntegration
            （各模块 Build.cs 逐条读取）
```

| 模块 | 依赖 |
|---|---|
| `TcsCore` / `TcsNotation` | 仅引擎 |
| `TcsAttribute` | `TcsCore`, `TcsNotation` |
| `TcsEffect` | `TcsCore`, `TcsAttribute` |
| `TcsState` | `TcsCore`, `TcsNotation`, `TcsAttribute`, `TcsEffect` |
| `TcsSkill` | 上述 + `TcsState` |
| `TcsIntegration` | **全部业务模块** + `AssetRegistry` |

### 1.2 Def 资产类散落四处（【源码取证】）

| 模块 | Def 资产类 |
|---|---|
| `TcsAttribute` | `UTcsAttributeDef` / `UTcsAttrModDef` |
| `TcsIntegration` | `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` |
| `TcsState` | `UTcsStateDef`（基类）+ `UTcsBuffDefAsset` |
| `TcsSkill` | `UTcsSkillDef` |

### 1.3 引擎事实（【引擎源码】）

| # | 事实 | 锚点 |
|---|---|---|
| E1 | `AssetManagerClassName` 是 **`globalconfig`**（`ConfigRestartRequired`）—— 只有项目级 ini 能设，**插件设不了** | `Runtime/Engine/Classes/Engine/Engine.h:UEngine::AssetManagerClassName` |
| E2 | 该类名无效时引擎 **`Fatal`**（不是静默回退） | `Runtime/Engine/Private/UnrealEngine.cpp`（`AssetManagerClassName` 读取处） |
| E3 | `StartInitialLoading()` 是 **`ENGINE_API virtual`**（public，可覆写）；其内先 `ScanPrimaryAssetTypesFromConfig()` 再广播 `OnAssetManagerCreatedDelegate` | `Runtime/Engine/Private/AssetManager.cpp:UAssetManager::StartInitialLoading`；声明 `Runtime/Engine/Classes/Engine/AssetManager.h:UAssetManager::StartInitialLoading` |
| E4 | `ScanPathsForPrimaryAssets(...)` 是 **`ENGINE_API virtual`**，可编程调用（**绕开 ini**） | `AssetManager.h:UAssetManager::ScanPathsForPrimaryAssets` |
| E5 | 配置扫描函数读 `UAssetManagerSettings`（`UCLASS(config = Game, defaultconfig)`）的 `PrimaryAssetTypesToScan`；**同一 `PrimaryAssetType` 重复出现时报 `Error`，且明言 "Only a single entry per type is supported"** | `Runtime/Engine/Classes/Engine/AssetManagerSettings.h`；`AssetManager.cpp:UAssetManager::ScanPrimaryAssetTypesFromConfig` |
| E6 | 提供 `CallOrRegister_OnCompletedInitialScan` / `CallOrRegister_OnAssetManagerCreated` 两个钩子 | `AssetManager.h` |
| E7 | 配置项含 **`PrimaryAssetIdRedirects` / `PrimaryAssetTypeRedirects` / `AssetPathRedirects`** 三件重定向（搬迁资产类型的正规手段） | `AssetManagerSettings.h` |
| E8 | `UAssetManagerSettings` 是 `defaultconfig` ⇒ **插件自带 ini 可提供类型清单**（但 §0 裁定采用代码注册，见 E4） | 同 E5 |

### 1.4 本仓现状：加载机制全未落地（【源码取证】）

| 项 | 事实 |
|---|---|
| `.uplugin` 模块 | **9 个，全部 `Runtime` + `LoadingPhase: Default`**（无加载阶段差异、无可选模块） |
| `PrimaryAssetTypesToScan` | **全库零命中**（含插件与宿主 `Config/`） |
| `UAssetManager` 子类 / `StartInitialLoading` / `AssetManagerClassName` | **全库零命中** |
| `CoreRedirects` | **未配置**（宿主 `DefaultEngine.ini` / `DefaultGame.ini` 均零命中） |
| 现有发现机制 | 定义库 **5 条 `Discover*` 全走 `IAssetRegistry::GetAssetsByClass`**（非 AssetManager 官方路径） |
| 插件侧 `Config/` 目录 | **不存在** |

---

## 2. 两条被推翻的中间提议（含实测判据）

### ① 为什么"各域自注册"不能在 `TcsIntegration` 里做 —— **会成环**

**实测**：`TcsIntegration` 依赖 `TcsAttribute`（`TcsIntegration.Build.cs`）⇒ 若 `TcsAttribute` 反过来依赖 `TcsIntegration` 以注册 `UTcsAttributeDef`，**即构成循环依赖，编译不过**。

**同批的第二重问题（用户指出）**：`TcsIntegration` 的 public 面还含 `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` / `UTcsCombatEntityComponent` / `UTcsPieEntityQuery`，一旦被下层依赖，这四样**全部**进入下层的可达面 —— 与"它们和加载机制无关"直接矛盾。

### ② 为什么"新建 `TcsDef` 承载共用 Def 族"也作废

该提议（见 `DEC-2026-10-13-state-module-positioning` §6 的讨论）本意是给**两域共用的 Def 族**（`FTcsStateDefBase` / `UTcsStateDef` / `FTcsNumericParamRow` / `TcsStateParamKeys`，实测 **473 行**）找一个家。**但它不是加载层**：它承载的是**定义数据形状**，且被 `TcsState` 与 `TcsSkill` **继承**（`FTcsSkillDefData : FTcsStateDefBase`）。⇒ 它既不需要、也不应该承担注册职责；**该提议与本文的加载层是两件事**，其归属仍按 §6 的结论（留 `TcsState`）。

### ③ 核心矛盾及其消解

| 要求 | 冲突点 |
|---|---|
| "各域自注册" ⇒ 注册表 MUST 住低 | 而现有 `UTcsDefinitionSubsystem` **缓存 4 种业务数据 struct**（`FTcsEffectChain` / `FTcsEffectTriggerDef` / `FTcsBuffDef` / `FTcsSkillDefData`）⇒ **它不可能住得比这四者更低** |

**消解（裁定③④）**：把**注册/加载**与**缓存/解析**分成两层 —— 前者**类型擦除**故可住低，后者认识业务类型故留高。**本仓已有四处成功先例**（§0 ③）。

---

## 3. 四处搬迁的实测依据

| 类型 | 本质 | 插件内消费面 | 宿主 | 资产引用 | 去向 |
|---|---|---|---|---|---|
| `UTcsEffectChainDef` | 效果链的 Def 资产类 | `TcsIntegration`(11) + **`TcsState`(1)** | 9 处 | **12** | **`TcsEffect`**（链机制的家，已有 `Chain/` 目录） |
| `UTcsEffectTriggerDefAsset` | 触发行的 Def 资产类 | `TcsIntegration`(9) + **`TcsState`(2)** | 5 处 | **2** | **`TcsEffect`**（已有 `Trigger/` 目录） |
| `UTcsCombatEntityComponent` | 实体组件（Actor 身份锚 + 查询门面） | `TcsIntegration`(18) | 5 处 | **0** | **实体接线层** |
| `UTcsPieEntityQuery` | PIE 的实体查询实现（实现 `ITcsEntityQuery`，接口住 `TcsEffect`） | `TcsIntegration`(15) | 12 处 | **0** | **实体接线层** |

**一处症状性证据**：`TcsState` 已在**注释里**提到 `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset`，但 `TcsState` **不依赖** `TcsIntegration`（实测 0 处）⇒ **真正的消费者拿不到它**。这正是"Def 资产类住错模块"的症状。

**搬迁的资产保全手段**：前两者共 **14 个已落资产**引用 `/Script/TcsIntegration.<类名>`，而宿主**未配任何 `CoreRedirects`** ⇒ 搬迁时必须同批处理。**正规手段 = 引擎自带的 `PrimaryAssetTypeRedirects` / `AssetPathRedirects`（E7）**，比 `CoreRedirects` 更对症（**待实测**，见 §6）。

---

## 4. StateTree 的归属口径（连带两处文档订正）

**裁定⑦**：StateTree 将来若引入，**单独成 `TcsStateTree` 模块**，MUST NOT 由别的模块承载。

⇒ **两处现有记载因此变陈旧**（本次登记，订正归后续回写）：

| 位置 | 现文 | 问题 |
|---|---|---|
| `SPEC-06-integration` §1 依赖行 | 「依赖：`TcsSkill`, `TcsCue` + **`GameplayStateTree`**（**全插件唯一依赖 StateTree 的模块**）」 | 按裁定⑦，StateTree 该住它自己的模块 ⇒ 该句需重写 |
| `DEC-00-constitution` §9 模块表 `TcsIntegration` 行 | 「**全插件唯一依赖 GameplayStateTree 的模块**」 | 同上 |

**★ 同批一处 as-built 漂移（MUST 记）**：上述两处声称的 `GameplayStateTree` 依赖**今天并不存在** —— 实测 `.uplugin` **零 StateTree 插件依赖声明**、`Source/` 全仓对 `StateTree` 的命中**全是注释**（"StateTree 同构"一类），**零 include、零 Build.cs 依赖**。⇒ 那是**R7 的设计意图**，不是现状；与裁定⑦ 叠加后该句应**重写**而非小改。

**同批一处用词澄清（防同词异义）**：**"实体状态机"与 "StateTree" 是两件无关的东西** ——
- **实体状态机** = **手写的枚举机** `Unregistered→Registered→Loading→Ready→TornDown`，住 `UCombatWorldRegistrySubsystem : UTickableWorldSubsystem`（`SPEC-06-integration` §2.1）；
- **StateTree** = 引擎插件，作为**可选的单位级决策宿主**。

二者在 `SPEC-06-integration` §2.1 原文是**分两行列的**；任何把它们并列成"实体状态机 / StateTree"的写法**都是错的**（本会话我自己犯过一次，见 §7 修正记录）。

---

## 5. 编辑器期跨资产解析：需求已被 tag 化消除

**用户口径**：该机制是 `DefId` 为 `FName` 时代的设想（"先声明 `Attr_MaxMana`，再声明 `Attr_Mana`，则 `Attr_Mana` 里可配动态最大值"需要跨资产前置解析）；**现在 `DefId` 都是 `GameplayTag`，不再需要**。

**实测支持**（【源码取证】）：
- `UTcsAttributeSubsystem::RegisterAttributeDef` / `FindAttributeDef` 均按 **`FGameplayTag`** 键（`TcsAttributeSubsystem.h`）；
- `FGameplayTag` 是**与声明顺序无关的稳定标识** ⇒ "目标是否已声明"不再是配置前提。

**★ 一处如实更正**：我**未能在 `TcsDefinitionSubsystem` 内找到**任何该机制的代码痕迹（`TcsIntegration` 里的 `WITH_EDITOR` 命中**全部**属 `UTcsEffectChainDef` / `UTcsEffectTriggerDefAsset` 的资产自身 `IsDataValid`，不是跨资产解析）。⇒ 准确结论是：**该机制在当前代码里不存在**（设计期设想的未落地项），**不构成需要清理的死代码**。裁定的效果 = **明确不建**，而非"删除"。

---

## 6. 待实测项（如实登记，MUST NOT 当已验证）

| # | 待测 | 为什么重要 |
|---|---|---|
| T1 | **覆写 `StartInitialLoading()` 后补调 `ScanPathsForPrimaryAssets(...)` 的时序后果** | E3 显示 `Super::` 内已跑 `ScanPrimaryAssetTypesFromConfig()` 并广播 `OnAssetManagerCreatedDelegate`；源码另有一句注释「Allow `StartInitialLoading()` bulk scan to continue past `FinishInitialLoading()`」（`AssetManager.cpp`）⇒ **看起来被支持，但我未读透 `GInitialBulkScan` 语义**。若错过批次，会延误 `OnCompletedInitialScan` |
| T2 | **`PrimaryAssetTypeRedirects` / `AssetPathRedirects` 能否保全已落资产**（14 个引用 `/Script/TcsIntegration.*` 的资产） | 搬迁的成败判据；本仓 `CoreRedirects` 零配置，两条路都未实测 |
| T3 | **`TcsAssetButler` 的模块 `LoadingPhase` 取值** | `AssetManagerClassName` 指向的类若住该模块，则该模块 MUST 在 AssetManager 实例化**之前**加载；9 个既有模块全 `Default`，而引擎创建 AssetManager 的时机需实测（E2 的 `Fatal` 说明"类加载不到"是硬失败） |
| T4 | **纯蓝图宿主可行性** | 裁定① 声称宿主只需一行 ini（`AssetManagerClassName=/Script/TcsAssetButler.TcsAssetManager`）+ **零 C++**；该声称依据 E1/E5/E8，**未经实跑** |

---

## 7. 本会话已就地撤回的错论据（汇总，诚实登记）

| # | 错论据 | 实况 |
|---|---|---|
| ① | "本机未定位到 UE 引擎源码树"（曾写进 `DEC-2026-10-13-relation-semantics`） | **错**——源码在 `E:\UnrealEngine\UE_5.8`（5.8.3 / CL 58210709）；早期只探了 `C:\Program Files\Epic Games` 与注册表。**该处已回头订正** |
| ② | "`TcsDef` 并入 `TcsIntegration`" | **错**——会把加载层推到最顶层，与"各域向下注册"直接冲突（§2 ①） |
| ③ | "各域自注册**无法实现**/失去意义" | **错**——注册键类型擦除即可（裁定③）；**本仓已有四处同型先例**。我先前把"加载注册层"与"定义缓存层"混作一层 |
| ④ | "实体状态机 / StateTree 接线"（并列写法） | **错**——两者无关（§4）；`SPEC-06` 原文分两行列 |

---

## 变更记录

- 2026-10-13 建立：用户质疑「各域自注册会让无关类型被迫暴露」⇒ 实测确认**会成环**（§2 ①），并连带推翻两条中间提议（§2 ②、§7 ②③）；逐项拍板八条裁定（§0）。**本裁决把"加载/注册"与"缓存/解析"确立为两层**，并给出类型擦除键的既有先例（§0 ③）。**同批更正本会话早期的引擎源码误判**（§7 ①）。
