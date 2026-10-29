# 调研 · AbilityKit 的实体管理（Entity）

> **审核状态**：⚠️ **agent 产出，未审**。本文件是调研，不是 V2 结论。
> 对标文件：[02-核心基线/战斗实体.md](../02-核心基线/战斗实体.md)
>
> **调研对象**（用户 2026-10-26 指定两处）
>
> | 来源 | 位置 | 体量 | 性质 |
> |---|---|---|---|
> | ① 模块设计文档 | `AbilityKit/Unity/Packages/com.abilitykit.combat.entitymanager/Document/实体管理模块开发设计文档.md` | 1 039 行 | 作者自述 v1.0 / 2026-03-19，**教学向**（大量 ASCII 图、图书馆类比） |
> | ② 调研技能 | `.agents/skills/.../ability-kit-research/`（27 份 reference + INDEX） | 09 号报告 §3.4/§4.4/§5/§8.2 命中 | **事实基线 HEAD `3952d9b28`（2026-09-27）**，含抽检更正记录 |
>
> **补充回源**（本轮实读，不是转述）：
> - 源码：`com.abilitykit.combat.entitymanager/Runtime/EntityManager/*.cs` **10 个文件全文**
> - 作者 canonical 文档：`Docs/design/08-GameplayModules/09-EntityAndSkillIndexing.md`（v3.2，基线 2026-08-16，321 行）
> - 同名族 ECS 层：`Docs/design/02-LogicalWorldDesign/02-EntityDesign.md`（v3.0）
> - 本仓库的旧设计依据：`combat-system-design/spec/06-module-integration.md`、`tcs-arch/11-integration.md` U-1

---

## 0. 结论速览

**一句话**：AbilityKit 的 entitymanager **不值得照搬实现，但有三样东西值得吸纳**——而且它自己写下的三条限制，恰好是我们 V1 已付过学费的同型病，可以当**免学费的反面教材**。

| 类 | 条目 | 处置建议 |
|---|---|---|
| ✅ **可吸纳** | ① 实体 **bucket 层**与实体**本体层**分层，bucket 不持有实体内容 | 直接回答「实体世界第一版建不建」的一半 |
| 🟡 **结构可吸纳 / 查询零消费** | ② **显式索引**：键由调用方给，框架不做内建维度 | 结构对，**但它的索引在生产路径上没有被查询消费**（§2.4） |
| 🟡 **结构可吸纳 / API 零消费者** | ③ **对称反查表**（实体 → 它有哪些键），但 `TryGetKeys()` 这个公开口**全仓零调用** | 吸收「对称维护」这个结构，**不吸收**「对外暴露反查 API」 |
| ❌ **职责在、成品不在** | ③b **查询加速职责**（文档三大承诺之首） | 设计文档写得完整，**落地只到示例层**：空间查询零实现、自动同步靠手工 `SetKey`（§2.4） |
| 🟡 **反向加强** | ④ 有代际 ⇄ 无代际，是**同一判据的两种正确解** | 给「什么时候必须有代际」补一条判据 |
| 🟡 **踩坑警示** | ⑤ `Invalid` 自身 `IsValid==true`；⑥ `Has<T>()` 世界级语义 | 两条都是「同名不同义」，V2 契约里要防 |
| ❌ **不能吸纳** | ⑦ `NotifyUpdated` + `EntityUpdate` 信封 + 三个 update DTO | **零内建消费者** ⇒ 违反铁律 1/2 |
| ❌ **不能吸纳** | ⑧ `AddIndex` 回放 | 内建索引 `OnEntityAdded` 是**空实现** ⇒ 回放对它们完全无效 |
| ❌ **不能吸纳** | ⑨ 活视图 + 无线程模型 | 作者自己写在限制里；我们的 `TFunctionRef` 遍历**已优于它** |

**最重要的一条负结论**：AbilityKit **没有「实体就绪态」这个概念**。它的 bucket 层只有 ID 集，ECS 层只有 `Create/Destroy/IsAlive`。§5 用它来反推 `IsEntityReady` 的可能语义。

> **本轮自我更正一处**：我第一版把 `TryGetKeys`（多键反查口）判成「✅ 可吸纳」。回查调用点后**不成立**——它全仓零调用；真正被用的是**单键**的 `TryGetKey`，且那是 `SetKey` 的内部自用件。已按 §2.3 改写为「吸结构、不吸 API」。**记在这里是因为这正是我该被抓的那类错误：从「接口存在」直接推到「该吸纳」。**

---

## 1. 它到底是什么（先划清，不然会误吸）

### 1.1 三个「实体」概念在这个仓库里是分开的

| 名字 | 包 | 承载什么 | 有没有实体内容 |
|---|---|---|---|
| `EntityRegistry<TId>` / `BattleEntityManager<TId>` | `combat.entitymanager` | **ID 集 + 派生索引** | ❌ 只有一个可哈希 Id |
| `EntityWorld` / `IEntity` / `IEntityId` | `world.ecs`（三套 ECS 之一） | **实体本体**：槽位、版本、组件、父子、事件 | ✅ |
| `MobaEntityManager` | `demo.moba.runtime` | **项目的键目录** | ❌ 只调 `CreateKeyedIndex` + `SetKey` |

**这个分层本身就是答案的一部分**：一个成熟的通用战斗框架，把「世界里有**哪些**实体」（bucket）与「实体**是什么**」（本体）放在两个互不依赖的包里，且本体层**三套实现并列可选**（自研 ECS / Entitas / Svelto）。

### 1.2 规模与依赖（用来判它的成熟度）

| 项 | 事实 |
|---|---|
| 包体量 | **10 个 .cs**，`package.json` / `asmdef` **只依赖 `com.abilitykit.core`** |
| 生产消费方 | `demo.moba.runtime`（`MobaEntityManager.cs:52-56` 建 `ByTeam/ByMainType/ByUnitSubType/ByOwnerPlayer` 四个 keyed 索引，`:148-151` 聚合 `SetKey`） |
| 测试 | **只有 3 个用例**，且只验三个 update DTO 的构造与字段（`KeyedIndexUpdateTests.cs`）⇒ 索引行为**零测试** |
| 作者自评（canonical §8） | 「EntityManager 为 E0 实现、E2 MOBA 消费，现有 E3 入口只固定 update 信封结构，**不能称为索引行为 E3**」 |
| 变更状态 | 基线 `885afcee6` → HEAD `3952d9b28` 本包**零改动**（09 号报告「变更补丁」A.3） |

⇒ **它是一个小、稳、自洽、但有明显未完成面的包**。下面按这个成色来吸收。

---

## 2. 可以吸纳的三样东西

### 2.1 ✅ 实体 bucket 层与实体本体层分层

**AbilityKit 的做法**：`EntityRegistry<TId>` 的全部状态只有一个 `HashSet<TId> _entities` 和一个 `List<IEntityIndex<TId>> _indices`。它**不持有**位置、血量、存活、父子——那些全在 `EntityWorld` 里。

**为什么这合法（按我们的判据）**：bucket 层不假设「实体是什么」，只依赖 (a) 一个可哈希的 Id、(b) 每个索引自己的 selector。⇒ 满足 [铁律 1](../../01-定位与判据.md)「接口只依赖它自己需要的东西」。

**对 V2 的直接作用**——它把「实体世界第一版建不建」这个待调研项**拆成了两问**：

| 子问题 | AbilityKit 的答案 | 我们的情况 |
|---|---|---|
| 建不建「世界里有**哪些**实体」的 bucket 层？ | ✅ 建，且只有 10 个文件 | V1 **已经有半个**（`FTcsCombatEntityHandleRegistry`），只是**发号错位**在属性模块 |
| 建不建「实体**本体** + 生命周期 + 就绪态」？ | ❌ bucket 层不建；本体层交给可选 ECS | V1 **建过又整层没落地**（`UCombatWorldRegistrySubsystem`） |

⇒ **可吸纳的判据**：「实体 bucket」和「实体本体」是两件事，前者是核心基线，后者可以是宿主可选实现——**不必捆成一次大工程**。

### 2.2 🟡 显式索引：**结构**可吸纳，但必须配真实查询消费者

**作者立场（canonical §1 原文）**：

> 「框架只拥有注册表、bucket 和增量索引维护契约；项目拥有权威实体/技能主数据、索引键目录、聚合写入口和异常后的重建策略。**MOBA 的 Team/MainType 等索引是项目选择，不应成为通用 EntityManager 的内建维度。**」

**这句话与我们的铁律 1 是同一个判据的两次独立得出**——一个在 C# 侧的第三方作者，一个是我们 V1 复盘，结论一致。这种「外部独立验证」值得记下来。

#### 但我上一版这里也判高了

我第一版写「✅ 可吸纳……不改契约就能补能力的路」。**读完它的真实数据流后要降级**（全表见 §2.4）——它的 keyed 索引在生产路径上**没有被查询消费**：

| 索引 | 建立者 | 查询消费方 |
|---|---|---|
| `ByTeam/ByMainType/ByUnitSubType/ByOwnerPlayer` | `MobaEntityManager:52-56` | 包装成 `GetTeam/GetMainType/...`（`:396-402`）——**全仓零调用** |
| 同一批索引的目标搜索用途 | — | MOBA 的候选源 `AllActorsCandidateProvider` / `FilteredActorsCandidateProvider` **都是遍历 `MobaActorRegistry.Entries` + 逐实体 predicate**，不用索引 |

⇒ **「显式索引」这个结构本身是对的，但 AbilityKit 没有给出「索引被查询消费」的成品**。吸结构时必须连同「谁查它」一起定，否则就是又一个只有写入、没有读取的空转件。

⇒ 与「`EnumerateEntities` 扩展搁置」的关系因此要重写：**搁置的裁定已被用户 2026-10-26 推翻（见 §2.7），索引不再是「替代加方法」的退路，而是「扩展方法之后谁供数」的问题**。

### 2.3 🟡 对称反查表：**结构**可吸纳，**公开 API** 不可吸纳

**结构**（`MultiKeyEntityIndex.cs`）：

```
Dictionary<TKey, HashSet<TId>> _map;          // 键 → 实体（正向查询）
Dictionary<TId, HashSet<TKey>> _keysByEntity; // 实体 → 键们（反查，对称维护）
```

**这条直指 V1 的一处真实成本**。`tcs-arch/11-integration.md` U-2 的订正记录了：

> 「每个泄漏桶**永久抬高** `FTcsBuffOps::Find` 的成本——它按句柄**扫描全部桶**（`TcsBuffOps_Query.cpp:22`），而这是状态移除 / 技能施放 / 周期回调的公共路径。」

⇒ **V1 是「实体问『我身上有什么』要靠扫全部桶」（O(桶数)）**；AbilityKit 的反查表把同一问变成 O(1)。

#### 但我第一版这里判错了，必须更正

我第一版把这个方法列成「✅ 可吸纳」。**回查调用点后不成立**——`TryGetKeys` 在**全仓零调用**：

| API | 全仓调用点 | 说明 |
|---|---|---|
| `KeyedEntityIndex.TryGetKey(id)` | **8 处**，全部在 `MobaEntityManager.cs`（`:140-143`、`:328-331`、`:190`） | 有用——**但它是 `SetKey` 的内部自用件**：换键前要先知道旧键才能从旧 bucket 摘掉 |
| `MultiKeyEntityIndex.TryGetKeys(id)` | **0 处** | 零生产者、零消费者 |

⇒ **真正被用的是「反查能力」，不是「反查 API」**——单键索引的反查是换键机制的自用件；多键索引**从不换键**（换键 = 先 `RemoveKey` 再 `AddKey`），所以反查用不上。作者自己写下的多键用途（canonical §3.1）就是「用于**反向清理**」，同一条自用件逻辑。

文中两处示例消费方（`MobaEntityManager` 与 `CombatSampleFixtures`）都只建**单键**索引（Team/MainType/UnitSubType/OwnerPlayer、Faction/Kind）；多键索引只出现在包内示例与 sample fixtures，且都**只 Add/Remove、不反查**。

#### 修正后的结论

| 吸什么 | 不吸什么 |
|---|---|
| 「**反查表作为内部自用件**」这个结构——换键与清理都必须靠它，且它是 O(1) 而不是扫描 | `TryGetKeys` 这个**公开 API**——零消费者，正合[铁律 1/2](../../01-定位与判据.md) 的判死条件 |

⇒ 对 V2 的正确措辞是：**「实体 → 它有哪些 X」的反查，应当在需要它的机制内部对称维护；而不是往实体契约上加一个公开的反查口。**

⚠️ **实现坑仍值得记下**：`AddKey` 里 `keys.Add(key)` 成功后，若反向 bucket 建立失败，正向与反向会分裂；`RemoveKey` 不检查 `id` 是否存在。`SetKey` 同样不检查 ID 是否在 `Registry` 里——canonical §3.4 明写「可以建立悬空索引记录」，且后续 `Registry.Remove` 会因主集合无此 ID 直接返回，清理永远不触发。

### 2.4 查询加速职责的真实数据流（本节回答「为什么我上一版漏了它」）

用户指出的职责原文在**设计文档自己**的三大承诺里（§1.3）：

| 承诺 | 原文 |
|---|---|
| 1️⃣ 查询高效 | 「用索引表替代遍历，查询是 **O(1)** 字典查找」 |
| 2️⃣ 自动同步 | 「实体属性变化时，所有相关索引**自动更新**」 |
| 3️⃣ 统一入口 | 「所有实体查询都通过同一个管理器」 |

§7.1 另列两条：**解耦查询与存储**（「可以随时添加新的查询维度，不需要改实体」）、**批量操作优化**（`NotifyUpdatedBatch`）。§7.2 的适用场景写的是「帧同步游戏：**每帧都需要高效查询**」。

⇒ **这是我上一版的漏项**：我只查了「索引这个数据结构存不存在」，没查「索引有没有被查询消费」。补查之后，真实数据流是这样的——

#### 写入侧（4 处，全部在 MOBA 生产代码）

| 环节 | 位置 | 说明 |
|---|---|---|
| 建索引 | `MobaEntityManager.cs:52-56` | 四个 keyed：`ByTeam` / `ByMainType` / `ByUnitSubType` / `ByOwnerPlayer` |
| 写索引（注册） | `MobaEntityManager.cs:151-154` | **手工四次 `SetKey`**，不是「自动同步」 |
| 写索引（更新） | `MobaEntityManager.cs:328-331` + `:190` | 同样手工；换键前用 `TryGetKey` 记旧值 |
| 失败回滚 | `MobaEntityManager.cs:161-168` | `catch` 里手工撤销 `Index.Remove` ——**索引写入不在任何事务里** |

#### 读取侧（生产 **0 处**）

| 读取路径 | 位置 | 是否算「查询加速」 |
|---|---|---|
| `GetTeam/GetMainType/GetUnitSubType/GetOwner` 包装 | `MobaEntityManager.cs:396-402` | ❌ **全仓零调用方**（`GetTeam(` 的 17 个命中里，其余全是 Behavior 层同名的**单实体**取队伍，与索引无关） |
| 目标搜索的候选源 | `SearchTargetService.cs:345-366`（`AllActorsCandidateProvider`）、`MobaTargetQueryFactories.cs:740-765`（`FilteredActorsCandidateProvider`） | ❌ **两个都遍历 `MobaActorRegistry.Entries` + 逐实体 `Predicate<ActorEntity>`**，一个索引都没查 |
| 全量遍历 | `MobaEntityManagerCleanupSystem.cs:34` → `GetRegisteredActorIds` | ❌ 那是索引的**退化用法**（遍历 `Registry.Entities`），恰恰是索引要消灭的 O(n) |

#### 唯一真实生效的查询消费，在**示例**里

```csharp
// Unity/Packages/com.abilitykit.samples/Runtime/Samples/Combat/CombatFirstWaveSamples.cs:112-119
var candidateIds = world.ByFaction.Get(SampleFaction.Monsters);   // ← 索引剪枝，O(1)
var query = SearchPipelineBuilder.Create()
    .From(new EntityCollectionCandidateProvider(candidateIds))     // ← 只在这堆里挑
    .Filter(new CircleShapeRule(new SearchVec2(0f, 0f), 7f))
    .ScoreBy(new DistanceToEntityScorer(new EntityId(1001)))
    .Select(new TopKByScoreSelector())
    .Take(2)
    .Build();
```

⇒ **「索引 → 候选集 → 目标搜索」这条链路，AbilityKit 只在示例里跑通，生产代码没有用。**

#### 所以「查询加速」这个职责本身要不要学？

| 职责要素 | AbilityKit 交付状态 | 可吸纳性 |
|---|---|---|
| **索引这个数据结构**（keyed / multi-key bucket） | ✅ 有，10 个文件，干净 | 🟡 吸结构 |
| **索引被查询消费** | ⚠️ **只在示例里**（`CombatFirstWaveSamples.cs:112`）；生产零消费 | ❌ 无成品可吸 |
| **索引自动同步** | ❌ 靠调用方手工 `SetKey`；「自动」那条通道（`NotifyUpdated`）内建零消费者 | ❌ 反向教材（见 §4.1） |
| **索引生命周期**（谁建、谁重建、谁销毁） | ❌ 作者自承「后加的内建索引仍是空索引，调用方必须遍历领域数据完成 backfill」「当前 API 不能提供原子挂载」 | ❌ 缺口 |
| **空间/范围查询** | ❌ 完全没有。它的`ProximityEntityIndex` 只是设计文档 §6.3 里的**教学示例**，全仓无实现 | ❌ 没给 |

⇒ **一句话**：AbilityKit 交付的是「索引的**容器形状**」，不是「查询加速的**能力**」。用户记忆里的那条职责，在它的设计文档里写得很完整（三大承诺 + §7.2 适用场景），但**落地只到示例层**。

### 2.5 V1 侧实测：`EnumerateEntities` 的问题比「O(n)」更具体

我上一版只写了「O(n) 全遍历」。读实现后发现它其实**一件方法扛了三件事**（`TcsPieEntityQuery.cpp:39-57`）：

```cpp
for (int32 Index = RegistrationOrder.Num() - 1; Index >= 0; --Index)
{
    const FTcsCombatEntityHandle Handle = RegistrationOrder[Index];
    const TWeakObjectPtr<AActor>* Found = EntityToActor.Find(Handle);
    if (!Found || !Found->IsValid())   // ← ② 每次遍历都做一遍弱指针校验 + 就地清理
    {
        EntityToActor.Remove(Handle);
        RegistrationOrder.RemoveAtSwap(Index, EAllowShrinking::No);
        continue;
    }
    Visitor(Handle);                   // ← ③ 逐实体回调
}
```

| # | 它在做 | 性质 | 拆开后的正确归属 |
|---|---|---|---|
| ① | **稳定序**（`RegistrationOrder` 驱动，因为 `TMap` 迭代序不稳） | 正确性 | 遍历序契约 |
| ② | **失效项就地清理**（Actor 已 GC 但没走注销路径） | 正确性 | 这不该在遍历里做 |
| ③ | **O(n) 全扫** | 性能 | 真正要解决的那个 |

⇒ **① 和 ② 是「一遍遍历顺手干两件事」的耦合**，不是性能问题；只有 ③ 是。V2 若只做「加一个范围枚举」而不解开 ①②，等于在同一段代码上再叠一层。

V1 当前的消费者只有一个：

| 消费 | 位置 | 用法 |
|---|---|---|
| 多选目标 | `TcsDevMultiSelector.cpp:24` | `EntityQuery->EnumerateEntities([&](FTcsCombatEntityHandle H){ ... })` |
| 发散探针 | `TcsDevSkillGateQuery.cpp:7` | 空实现（`TcsDevSkillRegistryProbe_Gate.cpp:81` 注释：「`GetLocation` 恒假、`EnumerateEntities` 空实现」） |

### 2.6 `EnumerateEntities` 的三种扩展形态 —— **我已判错，全部收缩**

> **本节保留原判断与更正，因为错在推理方式，不只是结论。**

我当时的判断：

| 形态 | 我当时说 |
|---|---|
| **(a) 增量枚举** | 「依赖帧，需先裁」 |
| **(b) 范围枚举** | 「**最该由框架给形状**——用户点名的群体导航正是它」 |
| **(c) 键值枚举** | 「最不该进契约——队伍/阵营是宿主词汇」 |

**用户 2026-10-26 四条裁定把 (b) 整条推翻**：

> 「**群体导航不是 TCS 应该负责的，这完全是另一个系统要操心的问题**。」
> 「形状库我觉得无论如何，都应住在 **TcsTarget 域** 中，毕竟形状查询太过于归属 Target 功能了。」

#### 错误的具体形态

我做了这样一条推理：**LAC 群体导航头疼 → LAC 需要范围查询 → 范围查询属于实体枚举 → 实体契约要加范围枚举。**

**这条链在第二步就断了**，而且断的方式很典型：**我用宿主的业务痛点，去论证框架的机制需求**。群体导航是 Movement/Steering 的事，它需要空间查询，但**不需要 TCS 提供**——LAC 自己用引擎的物理/网格就能做。

#### 更正后的结论（含一条正面收获）

| 形态 | 最终处置 | 归属 |
|---|---|---|
| (a) 增量 | **不做**（后续确有必要再补） | — |
| (b) 范围 | **形状库归 TcsTargeting 域**；且**不需要改 `ITcsEntityQuery`** | TcsTargeting |
| (c) 键值 | **不建** | — |

**正面收获**：把范围枚举从实体契约里拿掉之后，`ITcsEntityQuery` 的四个方法**第一次被证明真的够用**——`EnumerateEntities` + `GetLocation` 就能支撑形状判定（因为形状不枚举世界、只判定调用方给的点）。**这是依赖倒置缝发挥作用的实证**，比我原来「往契约里加方法」的方案强。

**另一条附带的自我更正**：§2.6 原文我猜「LAC 若是格子制，格位就是现成的键值索引」。用户 R-2 明确：**LAC 战斗期基本不考虑格子，可看作连续空间**；且「就算是格子，也可以靠计算转换成连续空间」。⇒ 我的猜测既无依据、方向也错，**不该由我提出**。

### 2.7 用户 2026-10-26 裁定：搁置被推翻

> 「其实 LAC 的 Runtime 单位数据量级也没那么小，**你不要以为自走棋就只能 Runtime 10+ Entity**（我现在还在为**群体导航**头疼呢）；所以我觉得 V2 可以把 `EnumerateEntities` 提前布局上。」

**这推翻了两条东西**：

| 被推翻的 | 原内容 | 新状态 |
|---|---|---|
| [战斗实体.md](../02-核心基线/战斗实体.md) §1.3 的「先搁置」裁定 | 「V2 第一版不做扩展，契约里不预建空方法」 | ❌ **作废**，改为「V2 提前布局」 |
| [01-定位与判据.md](../01-定位与判据.md) 里对 LAC 的画像 | 「回合制、确定性、可回放、**场上约 20 实体**」 | ⚠️ **单位量级被高估了小**——用户明确说不是 10+ |

### 2.8 用户 2026-10-26 第二批裁定：三支全部收缩

§2.6 我把「范围枚举」判成框架该做的事，理由是「LAC 群体导航头疼」。用户直接指出**这个理由不成立**：

| # | 裁定原文 | 影响 |
|---|---|---|
| R-1 | 「群体导航**不是 TCS 应该负责的**，这完全是另一个系统要操心的问题」 | 需求来源作废 |
| R-2 | 「LAC 在战斗期间基本不考虑格子，**可看作连续空间**；就算是格子，也可以靠计算转换成连续空间」 | 我的「格子=键值索引」猜测作废 |
| R-3 | 「形状库无论如何都应住在 **TcsTarget 域**中」 | 归属已定，不再三选一 |
| R-4 | 「TcsPieEntityQuery 只是测试用的……**V1 的代码已经不需要再动了**」 | 我上一轮问「要不要连 V1 一起改」是违背纪律 |

⇒ 最终：**(a) 不做 / (b) 归 TcsTargeting 且不改进实体契约 / (c) 不建**。详见 [战斗实体.md §1.3](../02-核心基线/战斗实体.md)。

⇒ **我的错误**：我把 LAC 当「小规模基准消费者」，于是推出了「O(n) 无所谓」。用户的实际约束是我没掌握的信息。**这类错误我应该在写下「约 20 实体」时就标注为「未经用户确认」。**

> **⚠️ 后续再更正（同日，见 §2.8）**：「提前布局」的具体形态已被全部收缩——**增量不做、范围归 TcsTargeting、键值不建**。本节保留是因为「搁置 → 提前布局 → 收缩出本域」这条演变本身是记录价值：**三次判断，前两次都是我推的，只有第三次是用户定的。**

---

## 3. 两条「反向加强」：我们已有的结论被外部印证

### 3.1 🟡 有代际 ⇄ 无代际是同一判据的两种正确解

`战斗实体.md` §1.1 里有这张表（池句柄有代际、实体句柄无代际）。AbilityKit 的 ECS 侧给出了**判据本身**：

| | AbilityKit `IEntityId` | 我们 `FTcsCombatEntityHandle` |
|---|---|---|
| 形态 | `Index + Version` | 只有 `Id` |
| 为什么 | 槽位进 **free stack 复用** ⇒ 旧句柄会落到新实体身上 | `Id` **单调递增永不复用**（`TcsCombatEntityHandle.h:17`） |
| 悬空表现 | 版本不匹配 ⇒ 判无效 | 查不到 |

⇒ **判据补一条**：「槽位会复用 ⇒ 必须有代际；ID 永不复用 ⇒ 代际无意义」。我们不是「忘了加代际」，是**没有需要加代际的前提**。这条现在有外部同族实现作证了。

### 3.2 🟡 两个「同名不同义」的坑

AbilityKit 自己踩了两个，都是**同型**——名字暗示的是实体级，行为是世界级：

| 坑 | 源码事实 | 后果（作者自述） |
|---|---|---|
| `IEntityId.Invalid` 自身「有效」 | `Invalid => default`（即 `(0,0)`），而 `IsValid => Index >= 0` | 「不能只用 `IsValid` 判断空句柄」 |
| `IEntity.Has<T>()` 是世界级检查 | 转发到无实体参数的 `world.HasComponent<T>()` | 「调用方可能把世界级存在误当成当前实体拥有」 |

**这与我们 `IsAlive` / `IsEntityReady` 的争辩是同一个病根**：一个方法名读起来像 A，实现的是 B。我们在 2026-10-26 已经裁定必须分开；AbilityKit 提供的是**同一错误在两个不同位置复发**的实例 ⇒ 值得写进契约纪律：

> **契约方法的注释 MUST 写出「问的是谁的状态」**，而不只是写「返回什么」。

---

## 4. 不能吸纳的四项（各自为什么）

### 4.1 ❌ `NotifyUpdated` + `EntityUpdate` 信封 + 三个 update DTO

**它是什么**：`EntityUpdate` = `int Type + object Payload` 信封；配套 `SetKeyUpdate` / `AddKeyUpdate` / `RemoveKeyUpdate` 三个 readonly struct；`EntityRegistry.NotifyUpdated` 按 `IUpdateTypeAwareIndex.Accepts(type)` 过滤后转给自定义索引。

**为什么不能吸纳——三重证据，全是它自己给的**：

| 证据 | 来源 |
|---|---|
| 内建 `KeyedEntityIndex.OnEntityUpdated` / `MultiKeyEntityIndex.OnEntityUpdated` 都是**空实现**（`KeyedEntityIndex.cs:71-73`、`MultiKeyEntityIndex.cs:111-113`），且**都不实现** `IUpdateTypeAwareIndex` | 源码实读 |
| 三个 update DTO **没有被任何内建索引消费** | 作者 canonical §3.3 原文 |
| 整个包唯一的 E3 证据，就是**这三个 DTO 的构造与字段**（3 个用例） | `KeyedIndexUpdateTests.cs` |

⇒ 这条通道的净效果是：**一条「属性变了」的通知路径，最终没有任何内置消费者**。

**这正是 V1「拆太细 ⇒ 加了很多注册、通知」的同型病**——[铁律 1/铁律 2](../../01-定位与判据.md) 因此直接判死：0 消费者不建。

具体的 V2 替代：键变化本来就是**调用方的一次显式动作**（`SetKey`），不需要绕一圈「发通知 → 索引自己决定改不改」。绕过一圈的唯一收益是「支持自定义索引自动派生键」——而那正好是 `skilllibrary` 的 selector 形态，**用 selector 就够了，不需要信封**。

### 4.2 ❌ `AddIndex` 的「后挂回放」

**它看起来很美**：`AddIndex` 先把索引加入列表，再遍历现有全部 ID 调 `OnEntityAdded` ⇒ 「新索引自动包含所有已有实体」。

**实际是空的**：内建索引的 `OnEntityAdded` 是**空实现**（`KeyedEntityIndex.cs:62-64`、`MultiKeyEntityIndex.cs:102-104`）。⇒ **回放对两个内建索引完全无效**，只有自定义索引受益。

作者自己在 canonical §3.4 把结论写死了：

> 「新增索引时，Registry 会向它回放当前全部 ID；但内建索引的 `OnEntityAdded` 为空，所以**后加的内建索引仍是空索引**。调用方必须遍历领域数据完成 backfill。……**当前 API 不能提供原子挂载。**」

⇒ 这是「**接口存在、语义为空**」的教科书样本。与 V1 的 `AsyncAction` / 帧末通道同型：机制建起来了，行为是真空。

### 4.3 ❌ 活视图 + 零线程模型

`Get` / `TryGet` 返回内部 `HashSet` 的只读接口视图，不是快照；`Entities` 直接暴露主集合的延迟枚举。作者 §5.3 自己写：「遍历期间发生重入或并发修改可能使枚举失效。需要跨回调、跨帧或跨线程保存结果时**必须复制快照**」。

**我们这点已经做得比它好**：`ITcsEntityQuery::EnumerateEntities` 用 `TFunctionRef` 逐回调，**根本不把容器交出去**。⇒ 不需要吸纳，反而是**我们该守住的形式**。

### 4.4 ❌ `object Payload` 装箱信封

`GetPayload<T>()` 用 `Payload is T v ? v : throw new InvalidCastException(...)`。在 C# 侧是装箱 + 运行期类型失败。

对 UE 侧的对应物应是 `TInstancedStruct` / `FVariant`——**但我们已经有 `FTcsEventBus` 的 Tag 路由**，再加一个平行信封就是**第二真相**。⇒ 不建。

---

## 5. 最有价值的一条负结论：AbilityKit 没有「就绪态」

`战斗实体.md` 把「`IsEntityReady` 的判定标准」列为待调研项，四个疑点里第一个是：

> 「『资产加载完成』是谁的状态——是 `UTcsDefinitionSubsystem::IsRuntimeReady()`（定义库），还是各域自己的就绪判据？」

**AbilityKit 的回答是：这个概念不存在于实体层。**

| 层 | 有没有「这个实体能不能操作」的判据 |
|---|---|
| `EntityRegistry` / `EntityManager` | ❌ 只有 `Contains(id)`——「在不在索引里」，等价于我们 `IsAlive` 的 PIE 实现 |
| `EntityWorld` | ❌ 只有 `IsAlive(id)`——「槽位活着」，版本匹配 |
| World DI（`IWorldModule.Configure` + `WorldModulePlanner` 拓扑排序） | ✅ 有，但它是**世界装配**级的，不是 per-entity |

⇒ **一个成熟的通用战斗框架，把「能不能操作」这件事放在了世界装配层，而不是实体层。**

这对我们的待调研项是一个真实的信息，但不是结论——因为 V1 的设计原词就是 per-entity 的「实体状态 `Ready`」，且用户 2026-10-26 的回忆也是 per-entity 的（「Attribute、State 模块都做好了资产加载、当前 World 有效」）。**把它记为候选而非裁定**：

| 候选 | 形态 | 与用户回忆的贴合度 |
|---|---|---|
| A. 世界级判据，per-entity 方法只是转发 | `IsEntityReady(e)` ⇒ 查「本世界 TCS 装配完成 **且** 该实体已注册进 bucket」 | 中——解释了「资产加载」与「当前 World 有效」，但解释不了「这个实体」（除非「已注册」就是那部分） |
| B. 真正的 per-entity 状态机 | `Unregistered→Registered→Loading→Ready→TornDown`（V1 旧设计原词） | 高——与 `SPEC-06 §2.1` 逐字一致 |
| C. 组合式 | 世界就绪 ∧ 实体已注册 ∧ 该实体的属性容器已建 | 高，但需要一个「容器已建」的判据 |

**AbilityKit 的贡献是把 A 这一支变成有先例的路**，而不是回答它。

---

## 6. 建议的 V2 产物（**候选，未获批准，MUST NOT 据本文件改代码或改基线**）

| # | 候选 | 证据来源 | 缺什么才能定 |
|---|---|---|---|
| P-1 | 实体 bucket 层（Id 集 + 可选索引）与实体本体层**分层**，bucket 层进 V2 第一版，本体层交给宿主 | AbilityKit 的三层拆分（§1.1）+ V1 发号错位已成事实 | 用户裁「实体世界第一版建到什么程度」 |
| P-2 | 框架**只提供索引的形状**，不提供 Team/Type 等维度；且**必须同时给出查询侧消费者**，否则不建 | AbilityKit canonical §1 立场句 + **它的索引在生产路径上零查询消费**（§2.4） | 「谁查它」——LAC 的三个具体查询场景 |
| P-3 | 「实体 → 它有哪些 X」的反查，MUST 由**需要它的机制内部**对称维护，**不往实体契约上加公开反查口** | AbilityKit `_keysByEntity` 是自用件、`TryGetKeys` 零调用（§2.3 更正）+ V1 `FTcsBuffOps::Find` 的实测成本 | 反查归属哪个域（Buff？还是 bucket 层？） |
| P-4 | 契约注释 MUST 写出「问的是谁的状态」，防同名不同义 | AbilityKit 自己踩的两个坑（§3.2） | 无（可直接并入 [铁律 4](../../01-定位与判据.md)） |
| P-5 | `IsEntityReady` 语义记为候选 A/B/C（§5），**不裁** | AbilityKit 的负结论 | 用户裁定 |
| **P-6** | ~~`EnumerateEntities` 扩展进 V2 第一版~~ ⇒ **全部收缩**：增量不做、范围归 TcsTargeting、键值不建 | 用户 R-1~R-4（§2.8） | 无（已裁） |
| **P-7** | 形状库（2D 圆/盒/扇 + 3D 球/盒/锥）住 **TcsTargeting 域**；形状是**判定**不是**枚举**，只判调用方给的点 | 用户 R-3 + `targeting-abilitykit-absorption.md` 的 D-1~D-6（用户 2026-09-30 已拍） | 无（可直接复用既有研究） |
| **P-8** | 遍历必须**解开三件事的耦合**：稳定序 / 失效清理 / 全扫（§2.5）；**只写进 V2 文档，V1 代码不动** | V1 `TcsPieEntityQuery.cpp:39-57` 实测 + 用户 R-4 | 无（文档层纪律） |

---

## 7. 一句话回答用户的原始问题

**「有没有可以吸纳的关于 Entity 的相关内容？」**

**有，但不在它的实现里，在它的三个结构选择上**：

1. **bucket 与本体分层**——回答了我们「实体世界第一版建不建」的一半；
2. **显式索引、框架不做内建维度**——与铁律 1 独立同源。**但结构对不等于能力在**：它的索引在生产路径上**零查询消费**（§2.4），吸的时候必须连「谁查它」一起定；
3. **对称反查表**——直指 V1 `Find` 扫全部桶的真实成本；但**吸结构不吸 API**（`TryGetKeys` 零调用，见 §2.3 的更正）。

**关于「快速索引 Entity」这条职责本身**（用户本轮追问）：

> AbilityKit **设计文档里写得很完整**（三大承诺之首就是「查询高效 O(1)」），**落地只到示例层**——空间/范围查询零实现，索引自动同步靠调用方手工 `SetKey`，唯一真实生效的「索引 → 候选集 → 搜索」链路在 `CombatFirstWaveSamples.cs:112` 这个示例里。

⇒ 所以它是**方向正确、成品缺失**。可吸收的是「查询该被索引加速」这个立场，不是它的代码。而**具体要哪种加速**（§2.6 的 a/b/c 三支），得由我们的消费者倒推，不能由它倒推。

**它的实现（`NotifyUpdated` / `EntityUpdate` / `AddIndex` 回放）恰恰是我们最该躲的那一类**：机制建好了，内置消费者为零。这跟 V1 的五个空转机制是同一个病。

### 最后一条：本轮三处自我更正

| # | 我判成 | 实际 | 错在哪 |
|---|---|---|---|
| 1 | `TryGetKeys` 可吸纳 | 全仓零调用，是 `SetKey` 的自用件 | **「接口存在」⇒「该吸纳」** |
| 2 | 显式索引可吸纳 | 建了索引，生产路径零查询消费 | **只查数据结构，不查数据流** |
| 3 | 范围枚举该由实体契约提供 | 属于 TcsTargeting，且实体契约一个字都不用改 | **拿宿主的业务痛点论证框架的机制需求** |

三条都是同一种毛病的变体：**从局部证据直接跳到框架级结论**。这大概是本轮最该被记住的东西。

---

*本文件为调研记录，零提交。审核后如需采纳，逐条并入 [战斗实体.md](../02-核心基线/战斗实体.md) 并改其审核状态。*
