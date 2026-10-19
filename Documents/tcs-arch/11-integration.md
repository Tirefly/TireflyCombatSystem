# ARCH-11-integration · TcsIntegration（接线层：定义库 + 实体锚 + 查询实现）

- **文档 ID**：`ARCH-11-integration`
- **类型**：模块档案（现状审核）
- **状态**：已审 2026-10-17
- **审核对象**：`Source/TcsIntegration/`——16 文件 / **1 536 行**（`.h` + `.cpp`，不含 `TcsIntegration.Build.cs` 40 行；含则 1 576）
- **权威范围**：本文件回答「`TcsIntegration` 今天实际是什么、还欠什么」。需求真相源仍是 `openspec/specs/`；历史理由仍回 `Documents/combat-system-design/`
- **审核方法**：`INDEX` §5 的六步 + `ARCH-08` 立的第五类判据（时态词）+ `ARCH-09` 的第六类（设计已推翻）+ `ARCH-10` 的第七类（结构在、内容被删空）

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

> **把「资产里的定义」翻译成「世界里的可查物」，把「Actor 的生死」翻译成「句柄的有效性」——两件翻译都不做执行。**

三个类各翻译一件事：

| 类 | 翻译什么 | 生命周期 | 行数 |
|---|---|---|---|
| `UTcsDefinitionSubsystem` | 资产 → 进程级缓存 → 每世界登记 | **GameInstance**（跨世界） | 319 + 273 + 77 + 77 + 78 + 85 + 55 = 964 |
| `UTcsCombatEntityComponent` | Actor 生死 → 实体句柄 | Actor 组件 | 150 + 180 = 330 |
| `UTcsPieEntityQuery` | 实体句柄 ↔ Actor | **UObject（无世界归属）** | 122 + 85 = 207 |
| （壳） | `TcsIntegrationLogChannel` / `Module` | — | 23 + 20 = 43 |

**三者不在同一层**：定义库是**进程级缓存**，组件是**Actor 级身份**，查询实现是**世界内的映射表**。把它们放进一个模块的判据是「都只做翻译、都不做执行」——这是本模块与其余十个模块最根本的区别：**其余十个模块各持一个领域，本模块只持三条边**。

### 1.2 反射面：**全仓最薄，且薄得不对称**【实测】

```
TcsCore         9 UFUNCTION      TcsEffect       49
TcsNotation     0                 TcsSkill         7
TcsAssetButler  0                 TcsTargeting     3
TcsAttribute   14                 TcsDamage       15
TcsState        0                 TcsIntegration   1   ← 本模块
TcsBuff         2
```

那唯一的一个是 `UTcsCombatEntityComponent::ExecuteChainById`（`TcsCombatEntityComponent.h:146`），口径「`UFUNCTION()` 无 specifier」（`BlueprintCallable` 会被 UHT 的蓝图参数校验拒——返回 `FTcsChainRunHandle` 非 `BlueprintType`）。

**而剩下的 25 个公开方法一个都没有**：

| 类 | 公开方法 | `UFUNCTION` | 关键缺口 |
|---|---|---|---|
| `UTcsDefinitionSubsystem` | 11 | **0** | `IsRuntimeReady` / `GetFailureList` / 五个 `Resolve*` 全部脚本不可达 |
| `UTcsCombatEntityComponent` | 8 | 1 | `GetCurrent` / `ApplyModifier` / `RemoveBySource` 不可达 |
| `UTcsPieEntityQuery` | 7 | **0** | 契约本身就是 C++ 专用面（形参含 `TFunctionRef`），不是遗漏 |

**这不是本模块独有的形态**——`ARCH-07` 的 G-20 已经在 `TcsBuff` 见过一次（零 `UFUNCTION` 的整个门面）、`ARCH-08` 的 D-6 在 `TcsSkill` 见过第二次（34 个公开方法零 `UFUNCTION`）。本模块是**第三次**，但形态不同：前两次是「整个门面为零」，本模块是「**一个组件方法有，同模块的定义库整体没有**」。

### 1.3 结构面上最反常的一点：**三条 import 边的方向全是反的**【实测】

`TcsIntegration` 在依赖图上住最顶层（层 4），依赖**全部十个模块**。但**真正用到的**只有四个：

| 被依赖 | 用来干什么 | 有没有 include |
|---|---|---|
| `TcsEffect` | `UTcsEffectSubsystem` 登记口 + `ITcsEntityQuery` 契约 | ✅ |
| `TcsAttribute` | `RegisterEntity` / `EvaluateCurrent` / `ApplyModifier` | ✅ |
| `TcsBuff` | `RegisterBuffDef` | ✅ |
| `TcsSkill` | `RegisterSkillDef` | ✅ |
| `TcsCore` / `TcsNotation` | 句柄 / 池 / 来源发号器（经传递包含） | ⚠ 无直接 include |
| **`TcsState`** | **无**（Task 4 拆分后登记口已搬走） | ❌ |
| **`TcsTargeting`** | **无** | ❌ |
| **`TcsDamage`** | **无** | ❌ |
| **`TcsAssetButler`** | `FTcsPrimaryAssetQuery` | ✅ |

```
grep -rn "#include" Source/TcsIntegration/ | grep -i "targeting|damage"   →   零命中
```

`TcsState` 那条**已登记**（`ARCH-05` 的 D-8）；`TcsTargeting` / `TcsDamage` 两条**未登记**，见 §5 D-6。

### 1.4 被依赖面：**只有宿主装置一个消费者**【实测】

```
grep -rn "TcsIntegration" Source/*/*.Build.cs
  → Source/TcsDev/TcsDev.Build.cs:39            （宿主装置）
  → Source/TcsIntegration/TcsIntegration.Build.cs（自己）
```

插件内**零模块**依赖本模块。这与 `ARCH-09`（`TcsTargeting`）的处境相同——但原因相反：`TcsTargeting` 是**没人需要它**（选择器经 `FInstancedStruct` 走，不需要 include），本模块是**只有一个消费者**（`TcsDev`），且那个消费者是**开发用的、可整体摘除的**。

⇒ **本模块是十一篇里唯一「唯一消费者可整体删除」的模块**。`TcsDev.Build.cs` 的头注释明写「可整体摘除：从两个 Target 的 ExtraModuleNames 与 .uproject 的 Modules 各摘一行，再删本目录与 Content/TcsDev/，宿主项目无残留」——**照这句话执行，`UTcsDefinitionSubsystem` 就没有任何消费者了**。

这条不是缺陷，而是**本模块的真实边界**：它是给宿主接的，宿主还没接。

---

## §2 已落地

### 2.1 五条发现路径，其中三条走「主资产类型」、两条走「类路径」【实测】

```
DiscoverChainDefs    → FTcsPrimaryAssetQuery::GetAssetsForDefClass(UTcsEffectChainDef)
DiscoverTriggerDefs  → FTcsPrimaryAssetQuery::GetAssetsForDefClass(UTcsEffectTriggerDefAsset)
DiscoverStateDefs    → FTcsPrimaryAssetQuery::GetAssetsForDefClass(UTcsBuffDefAsset)
DiscoverAttrModDefs  → IAssetRegistry::GetAssetsByClass(UTcsAttrModDef)
DiscoverSkillDefs    → IAssetRegistry::GetAssetsByClass(UTcsSkillDef)
```

前三条的注释（`TcsDefinitionSubsystem.cpp:88-91` / `_Trigger.cpp:16-17` / `_State.cpp:16-18`）**逐条写明了为什么改**：

> 类换过模块后 `GetAssetsByClass` 按**当前类路径**查索引会**静默返回 0**（`UTcsEffectChainDef` 于 Task 8 自 `TcsIntegration` 搬入 `TcsEffect` ⇒ 实测链资产发现由 12 个退为 0 个）。

这是 `PLN-REFACTOR-1` Task 6 修的**两处真实回归之一**（另一处是 C# 夹具）。修复的机理写在 `Asset/TcsPrimaryAssetQuery.h` 的头注释里，值得原样记住：

> `CoreRedirects` 的 `ClassRedirects` 只在**加载期**把旧路径映射到新类，**不改写 AssetRegistry 索引里记录的类路径**；而 `GetAssetsByClass` 拿**当前**类路径去查索引 ⇒ 查不到。

⇒ 这是**重定向手段的第四层**：它救加载，不救按类路径的索引查询。

### 2.2 装配是四段有序的，且顺序判据写在同一处【实测】

`SeedWorld`（`TcsDefinitionSubsystem.cpp:151-260`）的装配顺序：

```
1. 链          RegisterChain(*Chain)              ← 必须最先：触发行的引用链预检要查得到链
2. 触发行      RegisterTriggerRow(Instance)       ← Source = TriggerSeedSource（进程唯一发号）
3. 状态定义    RegisterBuffDef(Pair.Key, Def)     ← 按值拷一份，不与本类共享生命周期
4. 技能定义    RegisterSkillDef(Pair.Key, Def)    ← 同款
```

**判据在代码里分三处写清，措辞一致**：

| 段 | 代码里的判据 |
|---|---|
| 1→2 | 「**在链之后**：引用链预检要查得到链」 |
| 2→3 | 「顺序只要求"不早于消费方存在"，状态定义与链/触发行之间无引用关系」 |
| 3→4 | 「与状态定义同档」（`:237-238`） |

而**唯一有硬引用关系的那条边**（链 → 触发行）被单独写明，并且**预检失败时仍登记**：

```cpp
// TcsDefinitionSubsystem.cpp:204-209
if (!EffectSubsystem->IsChainRegistered(TriggerDef->EffectChainId))
{
    UE_LOG(LogTcsIntegration, Warning,
        TEXT("UTcsDefinitionSubsystem: 触发定义 %s 引用的链 %s 在该世界未登记——仍登记该行（宿主可在运行期登记该链）"),
        *Pair.Key.ToString(), *TriggerDef->EffectChainId.ToString());
}
```

判据三句都在注释里：**不 ensure / 不 Error**（内容缺口不是契约违规，且 `ExecuteChain` 已有自己的拒绝面）、**不跳过登记**（"先起世界后登记链"是合法时序，跳过会让该规则永久静默失效）。这与规格 `integration-entity:64-65` **逐字对上**。

### 2.3 GC 保活：**索引容器与保活容器成对出现**——全仓最干净的一处【实测】

`AddReferencedObjects`（`TcsDefinitionSubsystem_GC.cpp:13-55`）为四份缓存逐条补引用，**第五份（修正器模板）显式排除**，理由写在头注释里（`:131-135`）：

> `AttrModDefs` 虽也是非 `UPROPERTY` 的 `TMap`，但它**只持有资产对象本身**（没有包在 `FInstancedStruct` 里的内层对象引用可漏），且每个入索引的资产都同时进 `AttrModDefAssets` 这个 `UPROPERTY` 数组 ⇒ GC 经 `RefLink` 已能看见。**这也划出本轮的边界：索引容器与保活容器成对出现时，保活责任归后者。**

**在同一段代码里出现了两种情况、并给出了统一判据**，这是 `ARCH-06`/`ARCH-07`/`ARCH-08` 三篇反复指出「同族入口判据不一致」之后的**反例**：这里的两份处理恰好相反，而理由是**同一条**（「容器是否 GC 可见」）。同一判据推出相反处置 = 判据真的在起作用。

### 2.4 失败清单：八类身份级校验，且「内容级规则」被明令排除【实测】

| 路径 | 校验条数 | 具体 |
|---|---|---|
| 链 | 3 | 加载失败或类型不符 / `ChainId` 空 / **双真相**（`Chain.ChainId != ChainId`）/ 重复 |
| 触发定义 | 4 | 加载失败 / `TriggerTag` 空 / `Def.EventTag` 空 / `Def.EffectChainId` 空 / 重复 |
| 状态定义 | 2（原 3） | 加载失败 / `DefTag` 空 / 重复 |
| 修正器模板 | 2 | 加载失败 / `TemplateTag` 空 / 重复 |
| 技能定义 | 2 | 加载失败 / `DefTag` 空 / 重复 |

**「状态定义少了一条」是对的**：`Def.StatusTag` 非空校验于 2026-10-15 删除，注释（`_State.cpp:53-56`）写明了为什么——`StatusTag` 并入 `CategoryTags` 容器后**非必填**，容器是**筛选依据而非身份**，拦它「属**误报**」。

**而这五条路径共享同一句边界声明**：

> 发现期只判"这条资产能不能被按 tag 寻址"；内容级规则（参数键重复、内联触发行缺项、时值来源为空、描述缺项）全归资产 `IsDataValid`。

这句话在头注释（`TcsDefinitionSubsystem.h:45-49`）里出现了三次（状态 / 修正器模板 / 技能定义各一次），措辞一致。**发现期与资产校验的分界线**是本模块最清晰的一条架构决定。

### 2.5 「不参与世界装配」是带理由的，且理由可判定【实测】

规格 `integration-entity:72-73` 对修正器模板写了：不参与世界装配、`ResolveAttrModDef` 运行期调用者为零、本路径交付的是**身份解析与索引**而不是运行期取用。

代码侧的落地是 `AttrModDefs` 只在 `DiscoverAttrModDefs` 里被写、在 `SeedWorld` 里**完全不出现**——且 `SeedWorld` 的「无内容即返回」判据（`:154`）也**不含** `AttrModDefs`：

```cpp
if (!World || (ChainDefs.Num() == 0 && TriggerDefs.Num() == 0 && StateDefs.Num() == 0 && SkillDefs.Num() == 0))
{
    return;
}
```

两句注释（`:153`）写「判据 = 四类定义**全空**」——**这是对的**（第五类不参与装配，不该算"有内容"），但措辞与"本类有五个缓存"读起来冲突，见 §5 D-9。

### 2.6 组件：三职责各有门禁，且门禁失败一律 Warning + 跳过【实测】

`BeginPlay`（`TcsCombatEntityComponent.cpp:37-89`）的六段：

```
1. Owner / World 可得性          → Warning + return
2. GameInstance 可得             → Warning + return
3. DefinitionSubsystem 可得      → Warning + return
4. IsRuntimeReady()              → Warning + return   ← 规格点名的门禁
5. AttributeSubsystem 可得       → Warning + return
6. RegisterEntity + 登记进查询映射
```

**四道门禁全部 Warning、全部不 ensure**，与规格 `integration-entity:12` 的「未就绪留 Warning 并跳过（时序是宿主责任，不 ensure 刷屏）」一致。`EndPlay` 是**严格逆序**：先摘映射（此后查询不再吐本句柄），再注销实体（句柄随之失效）——注释写明了为什么（`:95`）。

### 2.7 查询实现：两个容器分工，且失效项就地清理【实测】

```cpp
TMap<FTcsCombatEntityHandle, TWeakObjectPtr<AActor>> EntityToActor;  // 解析
TArray<FTcsCombatEntityHandle> RegistrationOrder;                    // 遍历序
```

注释（`TcsPieEntityQuery.h:115`）：「`TMap` 迭代序不稳定，故另存登记序数组保证遍历稳定序」。**双容器是为了序，不是为了性能**——这是对的判据。

`EnumerateEntities` 里还有一段「失效项就地清理」（Actor 已 GC/销毁但组件未走注销路径，如关卡切换的强制回收）：

```cpp
const TWeakObjectPtr<AActor>* Found = EntityToActor.Find(Handle);
if (!Found || !Found->IsValid())
{
    EntityToActor.Remove(Handle);
    RegistrationOrder.RemoveAtSwap(Index, EAllowShrinking::No);
    continue;
}
```

**这段的动机对、实现有偏差**——见 §5 D-4。

### 2.8 `IsEntityReady` 与 `IsAlive` 重合，但重合被写成了「待覆写」而不是「合并」【实测】

```cpp
bool UTcsPieEntityQuery::IsEntityReady(FTcsCombatEntityHandle Entity)
{
    // **PIE 简化**：此处"能操作"与"还活着"同解（句柄在映射里且 Actor 有效），故复用同一判据。
    // 但两者是正交轴——真项目 MUST 分开覆写：死亡触发的被动 / 复活技能应 IsAlive == false 而可放，
    // 待销毁尸体应 IsAlive == true 而不可放。MUST NOT 因为此处重合就把两个方法合并
    return IsAlive(Entity);
}
```

**这是全档案里「契约默认」判据（`ARCH-05` 的 G-13）的最佳形态之一**：实现存在（不是 `= 0`）、行为明确（PIE 语义下确实相同）、且**判据写在代码旁边而不是规格里**（规格 `entity-query-contract:40` 也有同一句，两边一致）。

而且这个重合**已有第三方引用**：`TcsSkillSubsystem.h:705` 与 `TcsSkillSubsystem_EntityReady.cpp:36` 都引用了「`TcsPieEntityQuery.cpp:71` 其类注释自认框架不认识死亡」作为**不用 `IsAlive` 当门禁**的理由。⇒ 这条注释**已经在承重**。

### 2.9 两处「归属声明」：全仓唯一的防误清注记【实测】

`TcsCombatEntityComponent.h:41-47` 与 `TcsPieEntityQuery.h:40-46` 各有一段**几乎逐字相同**的块：

> **归属声明（2026-10-15，`PLN-REFACTOR-1` Task 8 · 裁定⑤ 的落点澄清）**：本类**留在 `TcsIntegration`**……**MUST NOT 读成"待搬"**——本条只把归属写在类旁边，防止下一轮把它误当遗留物清走。

背景是 Task 8 把两个 Def 资产类搬去了 `TcsEffect`，而本模块剩下的三个类**看起来像遗留物**。⇒ 作者预判了「下一次重构会扫走它们」，于是把「不搬」的理由钉在类旁边。

**这是全仓唯一一处这类注记**，且它是 `ARCH-09` 的 G-27（「以后做」住在「已做完」目录里）的**反面招式**：把「不做什么」写在**会被读到的地方**（代码旁边），而不是写在没人去的地方。

### 2.10 触发行来源句柄：进程唯一发号器，本类只发一次【实测】

```cpp
// Initialize :37
TriggerSeedSource = TriggerSourceRegistry.Allocate();
// SeedWorld   :213
Instance.Source = TriggerSeedSource;
```

`FTcsSourceHandleRegistry` 的计数器**已出线到 TcsCore 的 `.cpp` 匿名 namespace**（`TcsSourceHandle.cpp:17-20`），全进程一份。注释（`TcsSourceHandle.h:44-53`）记了当初的缺陷：

> 原实现把计数器做成实例成员、`Allocate()` 全内联在头里 ⇒ **每个实例都从 1 开始发号、跨模块必然重号**，级联摘除会连别人的来源一起摘掉。

⇒ 本类的 `TriggerSourceRegistry` 是**五个持有者之一**（另四个：`TcsDamageSubsystem::FlowSourceRegistry` / `TcsBuffSubsystem::SourceRegistry` / `TcsSkillSubsystem::SourceRegistry` / `TcsEffect` 的两处临时对象）。

**注意 `TcsEffect` 那两处用的是临时对象** `FTcsSourceHandleRegistry().Allocate()`（`TcsEffectSubsystem_Run.cpp:83`、`_StepProtocol.cpp:49`）——因为发号器无状态，「每实例一份计数器」的旧缺陷修掉后，临时对象是安全的。注释（`TcsEffectSubsystem.cpp` 起链处）明确了这一点。

---
## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

| 符号 | 语义 | 消费者 | 稳定性判据 |
|---|---|---|---|
| `UTcsDefinitionSubsystem::IsRuntimeReady()` | 定义库就绪门禁 | `UTcsCombatEntityComponent.cpp:62`（**唯一**） | **稳定**——它是规格 `integration-entity:12` 点名的门禁 |
| `GetFailureList()` | 失败清单透传 | **零消费者**（含装置） | 稳定（诊断面，规格要求存在） |
| `ResolveChain` / `ResolveTriggerDef` / `ResolveStateDef` / `ResolveAttrModDef` / `ResolveSkillDef` | 五个按 tag 解析口 | **全部只有 `TcsDev` 装置在用** | 稳定（规格逐个定义），但**插件内零消费者** |
| `UTcsCombatEntityComponent::GetEntityHandle()` | 句柄读口 | **零消费者** | 稳定 |
| `ExecuteChainById` | 手动起链 | **零消费者** | 稳定（唯一 `UFUNCTION`） |
| `GetCurrent` / `ApplyModifier` / `RemoveBySource` | 组件转发属性门面 | **零消费者** | 稳定 |
| `UTcsPieEntityQuery`（四支能力） | `ITcsEntityQuery` 的 PIE 实现 | 经 `SetEntityQuery` 注入后由 `TcsTargeting` / `TcsSkill` / `TcsEffect` 消费 | **稳定**——但**注入动作在宿主，不在本模块** |

**一行值得记住的事实**：本模块全部七个对外符号，**插件内真实消费者只有「`IsRuntimeReady` 门禁」与「查询实现被注入」两条**。其余五个的消费者**全部是 `TcsDev` 装置**。

### 3.2 MUST NOT 依赖（内部实现细节）【实测】

| 内部物 | 为什么外部不该依赖 |
|---|---|
| `ChainDefs` / `TriggerDefs` / `StateDefs` / `SkillDefs` / `AttrModDefs` 五个缓存容器 | 私有；`TUniquePtr` 持有使解析指针地址稳定，但**该稳定性的唯一合法用法**是 `Resolve*` 的返回值，不是直接取容器 |
| `TriggerSeedSource` / `TriggerSourceRegistry` | 装配期的内部锚点；外部拿它做级联摘除会摘掉**整批定义触发行**（这才是它的设计用途，但入口应该是"世界重装配"，不是外部手工摘） |
| `SeedWorld` / `Discover*` 五个发现函数 | 私有；`Initialize` 是唯一入口。**外部手工调用会让"就绪"标记与缓存状态失配** |
| `AddReferencedObjects` 覆盖的边界（四份而非五份） | 见 §2.3——它依赖「索引容器与保活容器成对」这个结构事实，**新增第六份缓存时该判据要重新过一遍** |

### 3.3 一条与 `ARCH-09` 对照出来的**正面**差异：装配有分子分母，发现只有单数【实测】

`SeedWorld` 的收尾日志（`:256-259`）是**四段计数、每段带分母**：

```
已装配到世界 %s——链定义 %d/%d 条，触发行 %d/%d 条，状态定义 %d/%d 条，技能定义 %d/%d 条
```

分母是缓存条数、分子是**登记成功条数**（`RegisterChain` / `RegisterTriggerRow` / `RegisterBuffDef` / `RegisterSkillDef` 的返回值）。**这是一个"失败可见"的设计**：登记被拒（重复 / 身份无效）时分子小于分母，日志立刻显示差异。

而 `Discover*` 阶段的日志是**纯单数**（「发现 N 个」/「未发现任何 X（正常状态——尚无内容）」）。这不是遗漏：发现阶段的失败**逐条进 `FailureList`**，而装配阶段的失败**只能进计数**（`RegisterChain` 的 ensure 会打红字，但没有结构化去处）。

⇒ **两处的观测手段不同、但都不静默**。这与 `ARCH-09` 的 D-8 / D-9 / U-2（三档降级各自选了不同日志策略，其中两档零日志）形成**直接对照**——那是「判据从未被写下」，本模块是「判据写了、且两处各按自己的判据做」。

### 3.4 一条结构性缺口：**`SeededWorld` 是单槽，多世界下幂等判据失效**【实测】

```cpp
// TcsDefinitionSubsystem.h:264-265
// 已装配的世界（幂等判据——同一世界不重复装配）
TWeakObjectPtr<UWorld> SeededWorld;
```

**单槽**。含义：

| 场景 | 实际行为 |
|---|---|
| 单世界（今天的全部场景） | 正确 |
| **PIE 多客户端**（2 玩家 = 2 世界） | 世界 A 装配 → `SeededWorld = A`；世界 B 装配 → `SeededWorld = B`；**再回到 A 时 `SeededWorld != A` ⇒ 会重新装配 A** |
| 重新装配的后果 | `RegisterChain` 对已登记 id **`ensureMsgf` + 返回 false**（`TcsEffectSubsystem.cpp:103`）⇒ **每次切世界刷一批红字**，且那条 `ensure` 的文案是「拒绝重复登记（不得静默覆写）」——**看起来像内容冲突，实际是时序** |

⇒ 规格 `integration-entity:118-121` 的 Scenario「同世界只装配一次」**在单世界下恒真，多世界下恒假**。而它的 `WHEN` 是「同一世界发生多次装配触发路径（世界初始化回调与立即装配）」——**测的是同一世界的两条路径，不是多个世界**。规格从未写多世界。

> 注：`UTcsDefinitionSubsystem` 是 **GameInstance 级**，而 GameInstance 在 PIE 多客户端下**每客户端一份**（每个 PIE 窗口一个 GameInstance）。故严格的失效需要「一个 GameInstance 对应多个世界」——这在**无缝切换关卡**（`OpenLevel` 保留 GameInstance）时成立：旧世界销毁、新世界初始化时 `SeededWorld` 仍指旧世界（`TWeakObjectPtr` 已失效 ⇒ `Get()` 返回 nullptr ⇒ 不等于新世界 ⇒ **正常装配**）。⇒ **单槽在"世界串行更替"下恰好够用**。
> **真正失效的场景**是「同一 GameInstance 下**同时**存在两个世界」——今天的 PIE 配置是「每个 Play 一个世界」，故**未命中**。⇒ 本条是**【推断】级别的潜在缺陷**，不是今天可复现的缺陷。

### 3.5 `GetSkillDefCount` 是五份缓存里唯一的计数读口【实测】

```cpp
int32 GetSkillDefCount() const { return SkillDefs.Num(); }
```

**只有技能定义有**。链 / 触发 / 状态 / 修正器模板**各自没有** ⇒ 外部（含装置与宿主）**无法在不装配世界的前提下**核对「那四类发现到了几条」。

装置侧的绕法是**直接读资产再 `Resolve*` 验回指**（`TcsDevSliceRig.cpp:327` / `:677`），而不是读缓存计数。⇒ **「发现了几条」在四类上是不可观测的**，见 §5 D-9。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

```
  GameInstance 启动
    └─ UTcsDefinitionSubsystem::Initialize
         ├─ Discover* × 5                  （读 AssetRegistry / AssetManager）
         ├─ SeedWorld(当前世界)             → TcsEffect / TcsBuff / TcsSkill 三个登记口
         └─ 订阅 OnPostWorldInitialization  → 未来每个世界重复 SeedWorld

  世界初始化（含后续每个世界）
    └─ OnPostWorldInitialization → SeedWorld → 三个门面登记

  宿主 Actor 进入 Play
    └─ UTcsCombatEntityComponent::BeginPlay
         ├─ (GameInstance) UTcsDefinitionSubsystem::IsRuntimeReady   ← 门禁
         ├─ (World)        UTcsAttributeSubsystem::RegisterEntity    ← 发号
         └─ (World)        UTcsPieEntityQuery::RegisterEntity        ← 建映射

  运行期（技能 / 目标选择 / 效果链）
    └─ UTcsEffectSubsystem::GetEntityQuery() → ITcsEntityQuery::*
         （注入动作在宿主：SetEntityQuery——本模块不代劳）
```

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

| # | 硬约束 | 代码锚点 | 违反的后果 |
|---|---|---|---|
| 1 | 链**必须先于**触发行装配 | `TcsDefinitionSubsystem.cpp:190-219` | 引用链预检对全部触发行报 Warning（但**仍登记**——降级而非失败） |
| 2 | 状态 / 技能定义**必须在链与触发行之后** | `:221-252` | 无引用关系；判据是「不早于消费方存在」 |
| 3 | `Initialize` 里 **`SeedWorld` 必须在订阅之前** | `:43` 先、`:46` 后 | 若反了，`Initialize` 期间的 `OnPostWorldInitialization` 广播会漏（该广播**晚于** `InitializeSubsystems()`） |
| 4 | `OnPostWorldInitialization` **晚于** `InitializeSubsystems()` | 头注释 `:59-61` 引 `World.cpp:2447` / `:2601` | 若反了，装配时 EffectSubsystem 还不存在 ⇒ 静默跳过整个世界 |
| 5 | 组件的 `BeginPlay` **必须晚于**定义库 `Initialize` | 门禁在 `TcsCombatEntityComponent.cpp:62` | 未就绪 = Warning + **跳过注册**，该 Actor 永远不是战斗实体（**无重试**） |
| 6 | 查询实现**必须在实体生成之前注入** | 宿主责任（`TcsDevSliceRig.cpp:440-445` 注释） | 组件的 `BeginPlay` 会登记进"当前注入的查询"，顺序反了 ⇒ 映射空 |

**第 5 条是本模块最脆的一条**：`BeginPlay` 只跑一次，门禁失败即**永久**失去该实体。规格（`:26`）只写「留 Warning 并跳过注册（不 ensure 刷屏），组件句柄保持无效」——**没有重试机制、也没有"宿主要怎么补救"的说明**。见 §5 D-5。

### 4.3 对四个被消费模块的具体影响【实测】

| 被消费模块 | 本模块对它做什么 | 影响的性质 |
|---|---|---|
| **`TcsEffect`** | `RegisterChain` / `RegisterTriggerRow` / `IsChainRegistered` | **唯一有硬时序关系的边**。触发行的 `Source` = 本类的 `TriggerSeedSource` ⇒ 该来源的语义**只有本类能撤销**（但没有撤销路径，见 §5 D-2） |
| **`TcsBuff`** | `RegisterBuffDef(Pair.Key, *Pair.Value)` | 按值拷一份，**不与本类共享生命周期** ⇒ 本类缓存清空不影响门面 |
| **`TcsSkill`** | `RegisterSkillDef(Pair.Key, *Pair.Value)` | 同上 |
| **`TcsAttribute`** | `RegisterEntity(FName)` / `EvaluateCurrent` / `ApplyModifier` / `RemoveBySource` / `UnregisterEntity` | **组件是本模块唯一对属性门面说话的地方**；`RegisterEntity` 的**发号动作由本模块触发、发号权在属性门面** |

### 4.4 一条竖切级的缺口：**实体注销的三条级联，只有一条被调用**【实测】

`UTcsCombatEntityComponent::EndPlay`（`:91-114`）只做两件事：

```cpp
EntityQuery->UnregisterEntity(EntityHandle);           // 摘映射
AttributeSubsystem->UnregisterEntity(EntityHandle);    // 释放属性账本
```

**而 `TcsBuff` 与 `TcsSkill` 各有一个 `UnregisterEntity`**：

| 门面 | 方法 | 规格怎么描述它 | 谁在调 |
|---|---|---|---|
| `UTcsBuffSubsystem` | `UnregisterEntity(Entity)`（`TcsBuffSubsystem.h:288`） | 「对该实体**每条在册实例逐条广播** `Removed` 后删桶」（`state-instance-lifecycle:266`） | **组件不调**；插件内**零调用** |
| `UTcsSkillSubsystem` | `UnregisterEntity(Entity)`（`TcsSkillSubsystem.h:215`） | 「摘掉该实体全部条目并删除其桶」 | **组件不调**；插件内**零调用** |

全仓调用点（除定义自身）：**只有 `TcsDev` 装置**（`TcsDevRelationProbe.cpp` × 6、`TcsDevSliceRig.cpp` × 6、`TcsDevSkillRegistryProbe.cpp` × 1）。

⇒ **一个 Actor 死亡时，它的 buff 实例与技能账本不会被清理**。已经落地的后果链：

1. `UTcsAttributeSubsystem::UnregisterEntity` **先把属性账本 `Stores.Remove(Entity)`**（`TcsAttributeSubsystem.cpp:66`）；
2. buff 实例仍在桶里，它的修正器**指向一个已经不存在的属性容器**；
3. 到期堆里还有该实例的条目，**到期时才按句柄代际校验丢弃**——中间这段时间状态门面认为"它还挂着"。

**这不算本模块的实现缺陷**——`EndPlay` 的注释（`:95`）只说「先摘映射、再注销实体」，规格 `integration-entity:12` 也只写「`EndPlay` 反向注销」，**没有任何一处说它要级联到 buff/skill**。这是**规格从未写下这条级联**，与 `ARCH-09` 的 G-26（「消费方 MUST 核」而消费方不存在）同型。见 §6 U-2。

### 4.5 与 `TcsDev` 装置的关系：**唯一消费者，且是"全部真实读数"的唯一来源**【实测】

| 装置位置 | 用什么 |
|---|---|
| `TcsDevSliceRig.cpp` | `ResolveChain` / `ResolveTriggerDef` / `ResolveStateDef` / `SpawnSliceEntity`（造带组件的 Actor） |
| `TcsDevSliceRig_State.cpp` | `ResolveStateDef` / `ResolveAttrModDef` |
| `TcsDevSkillDefProbe.cpp` / `TcsDevSkillRegistryProbe.cpp` | `ResolveSkillDef` |
| 全部 11 处注入 | `NewObject<UTcsPieEntityQuery>()` + `SetEntityQuery` |

⇒ **本模块的全部观测手段都由装置驱动**，而装置是**可整体摘除的**。「本模块今天有没有在工作」这个问题的答案是：**只有装置在跑的时候才有读数**。

---

## §5 技术债（期望 vs 实际，逐条带证据）

### D-1 ★★★ 规格的「单出口 `OnDefinitionsReady`」**在代码里从未存在过**，是「结构被删空」的**第二个样本**——而且它是在**同一个提交**里与 `damage-step-library` 一起被删的【实测】

| 项 | 内容 |
|---|---|
| **期望** | 归档提案 `2026-09-21-add-tcsintegration-entity-layer/specs/integration-entity/spec.md:51` 写：「**单出口** `OnDefinitionsReady()`（M6 双层引导的最小版）：定义发现/加载/校验完成时调用一次（幂等——重复调用不重复登记）」，并在 `:62` 配了 Scenario「就绪后组件可注册」 |
| **实际** | `OnDefinitionsReady` **在 `Source/` 里不是符号，只是注释**（`TcsDefinitionSubsystem.h:53` 与 `:312` 各一次，都是描述性文字）。`git log -S "OnDefinitionsReady" -- Source/` 只有一个提交（`8bdacb8`），而那个提交的 `.h` 里**同样只有注释** |
| **偏差证据** | `8bdacb8:Source/TcsIntegration/Public/TcsDefinitionSubsystem.h` 的公开方法只有 `Initialize` / `Deinitialize` / `IsRuntimeReady` / `GetFailureList` / `ResolveChain` / 五个私有函数；`.cpp` 的 `Initialize` 里是 `bRuntimeReady = true;` 加一句注释「就绪单出口：发现/校验完成即派位并广播一次」——**没有任何广播、没有 `OnDefinitionsReady` 函数** |
| **影响面** | 规格点名的「M6 双层引导最小版」与「06 §4 硬规则① 单出口」在本模块**零落地**。今天的替代物是 `IsRuntimeReady()` **轮询**——而轮询的消费者只有组件 `BeginPlay` 一处（且只查一次）。**没有任何"就绪后通知"机制** |
| **是否已登记** | **否**（`ARCH-05` 的 D-8 记了本模块的 `TcsState` 依赖空转，没有记这条） |

**这条为什么是 ★★★**：它同时命中了 `ARCH-10` 的 G-30 判据的**第二个样本**，而且**判别点清晰**：

```
git log --oneline -S "OnDefinitionsReady" -- openspec/specs/integration-entity/spec.md
  → 8bdacb8   建库（归档提案，有 OnDefinitionsReady）
  → 539b14e   删（现行 spec 0 命中）
```

**`539b14e` 就是删掉 `damage-step-library` 十步契约的同一个提交**（`ARCH-10` D-2）。它对 `integration-entity` 的改动是 **−63/+... 净删**，Scenario **9 → 6**（`539b14e^` 时 9 个，改后 6 个），被删的三个正是：

| 被删的 Scenario | 被删的含义 |
|---|---|
| `就绪后组件可注册`（`WHEN OnDefinitionsReady 被调用`） | 就绪**通知**路径 |
| `链资产自动登记`（`WHEN DefLibrary 加载到 Chain_Fireball` → `THEN FindChain 可查到`） | 发现 → 可查的**端到端** |
| `新世界初始化后链仍可查（跨世界装配）` | **跨世界装配**（本模块最核心的设计） |

⇒ **同一个"标识体系 tag 化"提交，删掉了两个模块的正文契约**（`damage-step-library` 的十步、`integration-entity` 的单出口与跨世界装配 Scenario）。

**判别「写过被删」而非「从未写过」的铁证**：现行规格的**正文首句**仍写着「职责：按类发现定义资产 → 校验 → 缓存 → **就绪标记** → 装配到每个世界」（`integration-entity:49`）——**"就绪标记"还在，但"就绪单出口"没了**；而代码里也确实是"标记"（`bRuntimeReady` 一个 bool）没有"出口"。**规格与代码在这一点上恰好一致，一致的原因却是两边都被同一刀削掉了同一段**。

**归因订正**：`ARCH-05` 记本模块「代码搬走、引用滞留」（D-8），方向对但**漏了更严重的一条**——本模块不只是引用滞留，它还**被删空了一段**。`ARCH-10` 说「规格被删空」是第七类判据，`integration-entity` 是它的**第二次命中**。

### D-2 ★★★ 触发行来源句柄的**级联撤销口存在、而本类从不调用**——它是"定义库来源"这个语义的唯一锚点，也是它唯一的例外【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格 `integration-entity:62` 写：「`Source` = 定义库自持的来源句柄（`FTcsSourceHandleRegistry::Allocate` 在 `Initialize` 时发放一次）。这是 `effect-trigger` 规格"**定义加载期登记**（全局常驻规则）→ `Source` = 系统/DefLibrary 来源句柄"那条的实现落点」 |
| **实际** | `TriggerSeedSource` 的**全部四处分使用点**：分配（`:37`）、置空（`:77`）、赋值给行（`:213`）、声明（`.h:307`）。**没有任何一处用它做撤销** |
| **偏差证据** | `grep -rn UnregisterTriggerRowsBySource Source/` ⇒ 调用点 **1 个**，在 `TcsBuff/Private/State/TcsBuffOps_Trigger.cpp:72`（状态实例的 `CascadeAnchor`）。**本模块零调用** |
| **影响面** | ① 世界销毁时 `UTcsEffectSubsystem::Deinitialize` 会 `TriggerRegistry.Reset(...)` 全清 ⇒ **实际上不会泄漏**（世界级容器随世界走）；② 但 `Source` 的语义是「按来源级联摘除」，而本模块**永不撤销**，于是「定义库来源」与「永不过期」等价——**这个句柄白发了**；③ 更实际的问题：**本类反初始化时**（GameInstance 销毁），已经在世的世界的触发行**仍然带着一个失效的来源句柄**（句柄是纯编号、无存活性登记，`TcsSourceHandle.h:42` 明写「本类只发号，不做注销登记」） |
| **是否已登记** | **否** |

**为什么是 ★★★**：它与 `ARCH-10` 的 D-3 是**同一个缺陷的另一半**。`ARCH-10` 的 D-3 记的是「`TcsDamage` 的 `FlowSource` 注释说 `RemoveBySource` 级联摘除，而调用点为 0」；本模块的 `TriggerSeedSource` **同款但更深一层**：

| | `TcsDamage::FlowSource` | `TcsIntegration::TriggerSeedSource` |
|---|---|---|
| 分配 | ✅ | ✅ |
| 写入行/上下文 | ✅ | ✅ |
| 注释声称级联 | ✅ | ✅ |
| **级联口调用** | **0** | **0** |
| 世界销毁时是否兜底 | ✅（世界级容器随世界走） | ✅（同上） |

⇒ **同一个"来源句柄语义未兑现"的缺陷，在两个模块各出现一次，且都是 ★★★ 级规格债**。这是 `ARCH-10` 的 G-31（同款代码逐字复制）的**变体**：不是两份代码逐字相同，而是**两个模块各自独立地造出了同一个半成品**。

**但本模块这一条有一个 `TcsDamage` 没有的性质**：`TriggerSeedSource` 是 `SeedWorld` 里**唯一跨越"定义库（进程级）→ 世界级子系统"这条边界的东西**。它失效的语义后果是「触发行不知道自己属于谁」——而 `TcsBuff` 的 `CascadeAnchor` 用同一个口**是有效的**（有真实调用点）。⇒ **同一个发号器，两个消费者，一个用了一半、一个用完整了**。

### D-3 ★★ 规格标题承诺「逐世界装配」，而**验证它的那个 Scenario 被删了**——今天再也测不出跨世界是否成立【实测】

| 项 | 内容 |
|---|---|
| **期望** | `integration-entity` 的 `Purpose`（`:4`）写：「GameInstance 级的定义库（发现、装载、按 tag 解析与**逐世界装配**——把缓存定义登记进该世界的消费方子系统）」；正文 `:49` 写：「职责：……→ 就绪标记 → **装配到每个世界**」 |
| **实际** | 代码确实做了（订阅 `OnPostWorldInitialization`，`TcsDefinitionSubsystem.cpp:46-47` + `HandlePostWorldInitialization`）。**但规格里没有任何 Scenario 验证"第二个世界"** |
| **偏差证据** | `539b14e` 删掉的三个 Scenario 里，`新世界初始化后链仍可查（跨世界装配）` 正是验证它的那一个。现行 20 个 Scenario 全部只测**一个世界内**的行为（`WHEN ... 世界初始化后检查` / `WHEN 同一世界发生多次装配触发路径`） |
| **影响面** | 「逐世界装配」是本模块**唯一真正的跨级设计**（GameInstance 级缓存 → World 级消费），其理由在头注释 `:56-61` 里完整保留（含 `World.cpp:2447` / `:2601` 的时机核验、以及「R3 单地图 PIE 测不出，宿主首次切关卡必撞」的实测警告）。**规格侧只剩结论，理由与验证全没了** |
| **是否已登记** | **否** |

**这条和 D-1 是同一刀的两次伤口**，但性质不同：

| | D-1（`OnDefinitionsReady`） | D-3（跨世界 Scenario） |
|---|---|---|
| 被删的东西 | **需求的一条**（单出口机制） | **需求的一条验证场景**（不是需求本身） |
| 代码有没有 | ❌ 从来没有 | ✅ 有，且做对了 |
| 危害 | 规格承诺了、代码没有 ⇒ **规格更严** | 规格要求了、代码做了、**但没人能验** ⇒ **不可证伪** |

⇒ 本模块同时出现了 `ARCH-04` 的方向（规格更严）与 `ARCH-08` 的方向（规格单方面缺失）。**G-30 的判据在这里给出的结论是「同一个删除动作，在两个方向上都成立」**。

### D-4 ★★ `EnumerateEntities` 的「稳定序」在两个容器之间会**不一致**——而规格把稳定序列成了 MUST【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格 `integration-entity:32`：「遍历世界中带 `UTcsCombatEntityComponent` 的 Actor 并吐**句柄**（**稳定序**——按组件注册序或 Actor 名排序，同输入同输出）」；Scenario `:37-40`：「三个带组件的 Actor……**两次遍历顺序一致**」 |
| **实际** | `RegistrationOrder` 是**登记序**，`EnumerateEntities` 按它**反向**遍历（`:42` `for (int32 Index = RegistrationOrder.Num() - 1; Index >= 0; --Index)`），并在遇到失效项时用 **`RemoveAtSwap`** 清理（`:51`） |
| **偏差证据** | `RemoveAtSwap` **会打乱顺序**：把尾部元素换到当前位。⇒ 一次"含失效项的遍历"之后，`RegistrationOrder` 的顺序**已不是登记序**。后续再登记新实体时追加到末尾——于是**同一个世界，遍历顺序取决于历史上发生过多少次失效清理** |
| **影响面** | ① 规格的「同输入同输出」在**两次遍历之间没有失效项**时成立；② 只要发生过一次失效清理，**登记序被永久打乱**，而规格没有"打乱后怎么算"；③ 反向遍历 + `RemoveAtSwap` 组合下，**被 swap 过来的那个元素可能被跳过**（当前位处理完就 `--Index`，wap 来的元素落在已经处理过的位置） |
| **是否已登记** | **否** |

**具体推演**（3 个实体，下标 0/1/2，正向登记 A/B/C）：

```
RegistrationOrder = [A, B, C]     反向遍历：Index=2(C) → 1(B) → 0(A)  ⇒ 吐出 C, B, A

若 B 失效：
  Index=2 → C 有效，Visitor(C)
  Index=1 → B 失效 → Remove(EntityToActor[B]) + RemoveAtSwap(1)
            ⇒ RegistrationOrder = [A, C]（把尾部 C 换到位置 1）
  Index=0 → A 有效，Visitor(A)
  ⇒ 本轮吐出 C, A（B 被正确跳过）

但顺序已变成 [A, C]：下次再登记 D ⇒ [A, C, D] ⇒ 反向吐 D, C, A
而"登记序"应当是 A, B, C, D ⇒ 反向吐 D, C, B, A（B 已失效则是 D, C, A）

⇒ 本例恰好等价；但若失效的是 A（位置 0）：
  Index=2 → C，Index=1 → B，Index=0 → A 失效 → RemoveAtSwap(0) ⇒ [C, B]
  下次登记 D ⇒ [C, B, D] ⇒ 反向吐 D, B, C
  而登记序应当是 B, C, D ⇒ 反向吐 D, C, B
  ⇒ **顺序确实变了**（B/C 互换）
```

⇒ 「两次遍历顺序一致」在**无失效项**时恒真，在**有失效项**时会变。而**失效项是设计上预期的**（注释 `:46` 自己写了「如关卡切换的强制回收」）。

**修法有两条且都很短**：① 用 `RemoveAt` 保序（代价 O(n)，但 n 是实体数）；② 反向遍历改成正向遍历 + 收集后再删。判据选哪条，**规格没写**。

### D-5 ★★ `BeginPlay` 门禁失败**没有任何补救路径**，而规格把时序完全推给宿主【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格 `integration-entity:12`：「**门禁**：注册前检查定义就绪（`UTcsDefinitionSubsystem::IsRuntimeReady`）——未就绪留 Warning 并跳过（时序是宿主责任，不 ensure 刷屏）」；Scenario `:23-26`：「`WHEN` `BeginPlay` 时定义库未就绪 `THEN` 留 Warning 并跳过注册（不 ensure 刷屏），组件句柄保持无效」 |
| **实际** | `BeginPlay` 只跑一次。四道门禁**全部 `return`**，没有任何重试、没有任何"稍后再注册"的入口 |
| **偏差证据** | `TcsCombatEntityComponent.cpp:37-89` 六段全是顺序 `if (...) { Warning; return; }`；组件**没有** `TryRegister()` / `Initialize()` 之类的公开入口；`GetEntityHandle()` 返回无效句柄后，外部**唯一**能做的是自己再调 `UTcsAttributeSubsystem::RegisterEntity`（绕开组件）——而那会让组件与实体的身份脱钩 |
| **影响面** | 规格说「时序是宿主责任」，而**宿主没有任何工具可以承担这个责任**：组件没有公开的注册入口、也没有"未注册原因"的读口。⇒ 若定义库晚于第一个 Actor 的 `BeginPlay`（编辑器 PIE 首次、或宿主异步加载），**该 Actor 永久不是战斗实体**，且**只有一条 Warning**可查 |
| **是否已登记** | **否** |

**为什么是 ★★ 而不是 ★**：它与 `ARCH-09` 的 G-26（「消费方 MUST 核」里的"消费方"不存在）**完全同型**——规格把义务推给了"宿主"，而本模块**没有给宿主任何履行义务的手段**。区别只是对象从"消费方"换成了"宿主"。

**而它有一条极便宜的修法**：把门禁从 `BeginPlay` 里的一次性判断，改成「未就绪时记下待注册，`OnDefinitionsReady` 时补」（但那需要 D-1 里那个**不存在的** `OnDefinitionsReady`）。⇒ **D-1 是 D-5 的前置**，两条必须一起裁。

### D-6 ★★ 两条**空转的依赖边**（`TcsTargeting` / `TcsDamage`）——而 `TcsState` 那条**已登记、这两条没有**【实测】

| 项 | 内容 |
|---|---|
| **期望** | `TcsIntegration.Build.cs` 头注释（`:11`）：「最小编译集……依赖其下全部战斗模块」 |
| **实际** | 11 条依赖里 **3 条零直接使用**：`TcsState`（Task 4 拆分后登记口搬走）、`TcsTargeting`、`TcsDamage` |
| **偏差证据** | `grep -rn "#include" Source/TcsIntegration/ | grep -i "targeting\|damage"` ⇒ **零命中**。`TcsState` 的 include 也无，但在写 `TcsIntegration.Build.cs:28` 处有注释「共享层（两域共用的 Def 族与运行机制）」，**已被 `ARCH-05` 的 D-8 登记** |
| **影响面** | ① 编译期多算两条依赖边；② `TcsIntegration` 的「依赖全部十个模块」这句话**在依赖表上成立、在源码上不成立**；③ **真正的问题不是性能**——是它让「本模块依赖谁」的读数**失真**：`INDEX` §3.1 的依赖图画的是 `TcsIntegration（全部）`，而真实是 **4 个**（Effect / Attribute / Buff / Skill）+ 1 个（AssetButler） |
| **是否已登记** | `TcsState` **是**（`ARCH-05` D-8）；`TcsTargeting` / `TcsDamage` **否** |

**三者的成因不同**：

| 模块 | 为什么还留着 | 该不该删 |
|---|---|---|
| `TcsState` | Task 4 拆分时的残留（登记口搬去 `TcsBuff`） | 该删（`ARCH-05` 已裁） |
| `TcsTargeting` | **从未用过**（本模块不碰选择器） | 该删 |
| `TcsDamage` | **从未用过**（本模块不碰流程） | 该删 |

但**不该立刻删**——`TcsDev.Build.cs` 的注释给过一条理由：装置经本模块的**传递依赖**拿到某些类型。实测：`TcsDev` **自己已经显式声明了** `TcsTargeting` / `TcsDamage`（`TcsDev.Build.cs:33-34`），故这条理由不成立。

### D-7 ★★ 五条 `Discover*` 里**两条仍走旧口**，而它们的注释不说明为什么——「该改的改了、不该改的没写理由」【实测】

| 项 | 内容 |
|---|---|
| **期望** | `TcsIntegration.Build.cs:35-37` 的注释：「主资产发现（2026-10-15 Task 6）：**五条 `Discover*`** 由 `GetAssetsByClass` 改走加载层统一口」 |
| **实际** | 只改了 **3 条**（Chain / Trigger / State）。`DiscoverAttrModDefs` 与 `DiscoverSkillDefs` **仍是 `IAssetRegistry::GetAssetsByClass`**（`_AttrModDef.cpp:31`、`_Skill.cpp:32`） |
| **偏差证据** | 台账 `INTEG-3` 的 2026-10-15 结账里**写清了**：「未改的 2 条 = `DiscoverAttrModDefs` / `DiscoverSkillDefs`（其类未搬迁、旧口实测命中 1 / 5 正常）⇒ **不是缺陷修复面**」。**但代码侧的 `Build.cs` 注释漏了这半句**——它说"五条改走"，与代码不符 |
| **影响面** | ① 读 `Build.cs` 的人会以为五条都改了；② 两条旧口的注释里**也没有**「本类未搬迁、故不适用」的说明，未来搬迁时会**重蹈覆辙**（这是 Task 6 缺陷的完整成因：搬了类、没改发现路径）；③ 两条旧口各自**手动复制了**「AssetRegistry 不可得 → Error + 进失败清单」与「`IsLoadingAssets` → `WaitForCompletion`」两段（`_AttrModDef.cpp:15-27`、`_Skill.cpp:16-28`）——**与新口的 `HasInitialScanCompleted` 判据不同** |
| **是否已登记** | 台账 **是**（`INTEG-3`）；本档案 **否** |

**这两条旧口的重复代码值得单独提一句**：`_AttrModDef.cpp:15-27` 与 `_Skill.cpp:16-28` 是**同一段 13 行**（除类名外逐字相同）。这是 `ARCH-10` 的 **G-31（同款代码逐字复制）在本模块的第一次命中**——但**两边都没有缺陷**（两段实现相同且都对）。⇒ G-31 的判据应当补一句：**逐字复制的代码不必然含缺陷，它只是让缺陷有第二份载体**。

### D-8 ★ 引用链预检的 Warning **只在装配时发一次**，而「链在运行期才登记」是本模块明文承认的合法时序【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格 `integration-entity:64-65`：「装配一条触发定义前 MUST 检查其 `Def.EffectChainId` 在该世界是否已登记……不可解析时 MUST 留一条 **Warning**（含 `TriggerTag` 与链 id）并**仍登记该行**——MUST NOT 跳过登记（宿主可在世界运行期自行 `RegisterChain`，"先起世界后登记链"是合法时序……）」 |
| **实际** | 检查与 Warning 都做了（`TcsDefinitionSubsystem.cpp:204-209`），且「仍登记」也做了（`:215`）。**但补登记之后没有任何复核** |
| **偏差证据** | 规格承认「宿主可在世界运行期自行 `RegisterChain`」是合法路径，但**没有任何一处**：① 在链补登记时回头核对哪些触发行曾报过 Warning；② 提供一个"复查未解析引用"的读口；③ 在 `RegisterChain` 成功后清掉那条 Warning 状态 |
| **影响面** | ① 若宿主确实走"先起世界后登记链"，那条 Warning **会永久留在日志里**（看起来像持续故障，实际已自愈）；② 反过来，若宿主**没有**补登记，那条 Warning 之外**没有任何持续可观测的信号**——运行期起链会 `Error`（`ExecuteChain` 的拒绝面），但那要到真的触发时才出现；③ **没有任何"当前有几条触发行引用未解析链"的读口** |
| **是否已登记** | **否** |

**这条不是 ★★**：规格的要求（发一条 Warning、仍登记）**全部满足了**，缺的是规格没要求的东西。⇒ 它属于 `ARCH-09` 的 G-23（Scenario 网格缺角）的**规格侧**形态：Scenario 只写了 `WHEN 引用的链未登记` → `THEN Warning + 仍登记`，**没写 `WHEN 链后来被补登记`**。

### D-9 ★ 三个小口径问题：装配判据的措辞、四类缺计数读口、无用 include【实测】

三处彼此无关但都属"措辞/接口粒度"的小债，合并成一条：

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| a | `SeedWorld` 的早退判据注释写「判据 = 四类定义**全空**」，而本类有**五个**缓存 | `TcsDefinitionSubsystem.cpp:153-154` 注释 vs `ChainDefs` / `TriggerDefs` / `StateDefs` / `SkillDefs` / `AttrModDefs` | 判据本身**对**（第五类不参与装配），但措辞让读者以为漏了一类 |
| b | `GetSkillDefCount` 是五份缓存里**唯一**的计数读口 | `.h:169-172` | 另外四类「发现了几条」不可观测（§3.5） |
| c | `TcsDefinitionSubsystem.cpp` include 了 `AssetRegistry/ARFilter.h`，全文件**零使用** | `:5` include / `grep -n ARFilter` ⇒ 只有 include 行 | 无害，但它是 Task 6 改造后**残留**（原实现用 `FARFilter` 的痕迹） |

**关于 (b) 的一条判据**：`GetSkillDefCount` 自己的注释写「供宿主核对发现结果与就绪日志」（`:168`）。⇒ 这个需求的**合理性对四类定义都成立**，但只给了一类。**这是「接口粒度不一致」而非「零消费者不预建」**——`ARCH-04` 的 D-8 把零调用判为违反，而这里的判据是**已有的那一个**证明需求真实存在。

### 债务小结

| 编号 | 严重度 | 类型 | 一句话 | 是否已登记 |
|---|---|---|---|---|
| D-1 | ★★★ | **纯规格债 + 代码从未有** | `OnDefinitionsReady` 在代码里从未存在，且与 `damage-step-library` 在**同一提交**被删 | 否 |
| D-2 | ★★★ | **纯代码债 + 规格从未要求** | `TriggerSeedSource` 的级联撤销口存在、本类零调用（`TcsDamage::FlowSource` 同款） | 否 |
| D-3 | ★★ | **纯规格债** | 「逐世界装配」的验证 Scenario 被同一刀删掉 | 否 |
| D-4 | ★★ | **纯代码债** | `EnumerateEntities` 用 `RemoveAtSwap` 清理，破坏规格 MUST 的稳定序 | 否 |
| D-5 | ★★ | **双方**（规格推给宿主，宿主无手段） | `BeginPlay` 门禁失败无补救路径 | 否 |
| D-6 | ★★ | **代码债（依赖）** | `TcsTargeting` / `TcsDamage` 两条空转边（`TcsState` 那条已由 `ARCH-05` 记） | 部分 |
| D-7 | ★★ | **注释债 + 口径分裂** | 五条发现只改三条，`Build.cs` 注释说五条；两条旧口各自复制 13 行 | 台账已记 |
| D-8 | ★ | **规格网格缺角** | 补登记链后无复核、无读口 | 否 |
| D-9 | ★ | **措辞 / 接口粒度** | 三小项（装配判据措辞 / 缺计数读口 / 无用 include） | 否 |

**方向分布（按 `ARCH-04` 起的七类判据）**：

| 方向 | 条数 | 具体 |
|---|---|---|
| **结构在、内容被删空**（G-30） | **2** | D-1（单出口没了）/ D-3（跨世界 Scenario 没了） |
| **规格更严、代码更松**（G-10） | **2** | D-4（稳定序 MUST）/ D-1（单出口 MUST 的那一面） |
| **注释/口径与实现不符**（G-14/G-15 族） | **2** | D-7 / D-8 |
| **代码搬走、引用滞留**（G-12） | **1** | D-6（两条空转依赖） |
| **双方各错一半** | **1** | D-5 |
| **纯代码（规格从未要求）** | **1** | D-2 |

⇒ **G-30 第一次成为最高频方向**（2 条）。`ARCH-10` 是它的首现（1 条 + 自身 D-2），本模块让它在**单模块内占了 2/9**。

---

## §6 未实现（设计是否还成立 + 前置条件）

### U-1 ★★★ `UCombatWorldRegistrySubsystem`（中央注册表 / 实体状态机 / 泵接线）——**整层不存在，而它是旧设计的门禁主体**

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **部分成立，且已被削成了"两半分离"** |
| **依据今天是否仍有效** | 旧 `SPEC-05-integration` §2.1 写它是 `M3 中央注册表 + M0 泵接线的宿主`，含实体桶、实体状态机（`Unregistered→Registered→Loading→Ready→TornDown`）、到期堆接线，**门禁 = 查 `DefLibrary.IsRuntimeReady()`**。实测 `Source/` **零命中**（`grep -rn "UCombatWorldRegistrySubsystem"` ⇒ 只有 `TcsEntityQuery.h:79` 的注释提到「属 R7、今天不存在」）。⇒ **那一层今天由两部分替代**：实体桶 → `TcsBuff` / `TcsSkill` 各自的 per-entity 桶（`entity-runtime-mechanism` 能力）；泵 → `UTcsEventBusSubsystem` + `UTcsClockSubsystem`（`TcsCore`）。**唯一没被替代的是"实体状态机"** |
| **前置条件** | ① 需要先裁「实体状态到底是不是框架的事」——`ITcsEntityQuery::IsEntityReady` 的 R6 追加（`entity-query-contract:16`）已经**把这件事推回宿主**，并写明「R7 落地后 MUST 改成本方法的转发」。**⇒ 今天的落法（宿主实现）与旧设计（框架状态机）冲突，必须先裁**；② `entity-query-contract:16` 的「R7 转发纪律」写了两处落点（注释 + 台账 R7 区段），台账两处都在，**故这条义务是可追踪的** |
| **本模块的相关度** | **高**——`UTcsCombatEntityComponent::BeginPlay` 是目前**唯一的实体注册路径**，而它的门禁查的是 `IsRuntimeReady`（定义库），不是「实体状态 Ready」。旧设计指定的是**后者的前身**。⇒ 本模块的组件是那一层落地时**必须改的第一处** |

### U-2 ★★★ 实体注销的**跨门面级联**从未写下——Buff / Skill 的 `UnregisterEntity` 在插件内**零调用**【实测】

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **本身成立，但归属从没定过** |
| **依据今天是否仍有效** | Buff 与 Skill 两个门面都提供了 `UnregisterEntity`，规格也各自写了它的行为（`state-instance-lifecycle:266`：「逐条广播 `Removed` 后删桶」）。⇒ **接口存在、语义清楚、规格有字——只差"谁调用它"** |
| **前置条件** | 需要裁「实体注销的**编排点**在哪」。三个候选：① `UTcsCombatEntityComponent::EndPlay`（组件是身份锚，它最清楚"这个 Actor 没了"）；② `UTcsAttributeSubsystem::UnregisterEntity`（发号方，但它**不依赖** Buff/Skill——`TcsAttribute.Build.cs` 只依赖 Core/Notation，加依赖会成环）；③ 未来的中央注册表（U-1）。**① 是唯一不成环的落点**，但组件目前只调属性门面 + 查询映射 |
| **今天的实际后果** | 见 §4.4 的三步后果链：属性账本先删 → buff 修正器指向不存在的容器 → 到期堆里还留着条目。⇒ **这是本档案里唯一一条"有真实运行期错误状态"的未实现项** |

### U-3 ★★ 复制契约（`ICombatReplicationProxy`）——**旧设计明写"仅契约不实现"，而契约也没落**【实测】

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **成立，且设计本身就说了不实现** |
| **依据今天是否仍有效** | 旧 `SPEC-05` §2.3 原文：「`ICombatReplicationProxy`（草案）：`InjectOperation(操作流)` / `ExportSnapshot()`——代理 Actor 读写中央 store 的单一入口；**实现推迟到有联网项目**（届时按 ReplicationGraph/Iris 细化形态）」。实测 `grep -rn "ReplicationProxy\|CombatReplication" Source/` ⇒ **零命中**（连草案接口都没写）。⇒ 这是「**决定不做**」的典型，按 `ARCH-07` 的 G-20（「决定不做」无处可写）**`openspec` 侧零记录**——12 份相关规格无一提及 |
| **前置条件** | 有联网项目。**在那之前 MUST NOT 建**（旧设计自己说的）。⚠ 真正缺的是**这句"决定不做"没有位置可写** ⇒ 应进 `ARCH-00-overview` 的「已决定不做」清单 |

### U-4 ★★ `ICombatAttributeProvider` 在实体侧的实现——**AttributeSet 与"当前 Set"整块缺席（台账 `INTEG-1` / `INTEG-2`）**

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **被 `PLN-REFACTOR-1` 间接动摇** |
| **依据今天是否仍有效** | 旧 `SPEC-05` §2.2 的第 2 职责原文：「实现 `ICombatAttributeProvider`（缓存桶指针转发）；**并暴露该实体的 AttributeSet 查询与切换入口**」。实测：`ITcsAttributeProvider` **存在**（`TcsAttribute/Public/Attribute/TcsAttributeProvider.h`），但 `UTcsCombatEntityComponent` **不实现它**（`class UTcsCombatEntityComponent : public UActorComponent`，无第二基类）。`AttributeSet` 全库**零命中**（只有两处注释说"R3 无 AttributeSet——D2-15 属 R7"） |
| **⚠ 一个必须先裁的点** | `ITcsAttributeProvider` 今天**全仓零实现者**（`ARCH-04` / `ARCH-07` U-1 / `ARCH-08` U-4 三处各记一次，即 G-21）。⇒ 它已经从「组件要实现的接口」变成了「一个没有实现的契约」。组件实现它**是不是正确的落点**，需要重新裁——**组件持 `EntityHandle`，转发 `EvaluateCurrent` 是一行**，但它是 `UActorComponent`（`ITcsAttributeProvider` 是 `UINTERFACE`，能实现）。**技术上完全可行，判据上没裁过** |
| **前置条件** | 先裁 `ITcsAttributeProvider` 的落点（G-21 的跨模块待办），再谈组件要不要实现 |

### U-5 ★★ `LoadAll` / `LoadSelected` / `EnsureLoaded` 三个加载入口——**旧文档明写、规格从未写、代码零命中**

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **不成立——已被加载层改判替代** |
| **依据今天是否仍有效** | 旧 `SPEC-05` §3 入口服务写：「DefLibrary：`LoadAll/LoadSelected/EnsureLoaded(DefTag)`」。实测全库**零命中**。而它**不是遗漏**：`ETcsAssetLoadPolicy`（PreloadAll / PreloadSelected / OnDemand）**已落地**在 `TcsAssetButler`（`ARCH-03`），且台账 `INTEG-3` 的 ①③ 两项已结账。⇒ **设计的三档加载策略换了载体**（从 DefLibrary 的方法 → 各 Def 类自注册的策略枚举） |
| **⚠ 但有一处不对齐** | 新落法里 `ETcsAssetLoadPolicy` 的**读点为 0**（`ARCH-03` 的 D-4：「作 API 语义成立，作能力语义不成立」）。⇒ 旧设计要的"三档加载"今天既没有旧载体、新载体也没被读。**这是 U-5 真正的欠账** |
| **前置条件** | 出现第一个"要按策略预载"的调用点。届时 MUST 同时决定：策略从 `Def 类的注册项` 读，还是从 `Def 资产` 读（**两者今天都没有读点**） |

### U-6 ★ `ICombatEntityQuery` 的**宿主任意实现**路径——今天只有 `UTcsPieEntityQuery` 一个实现者

| 项 | 内容 |
|---|---|
| **设计是否还成立** | **成立，且已验证过一次** |
| **依据今天是否仍有效** | `entity-query-contract:9` 写「机制层定义、宿主实现」；实测 `ITcsEntityQuery` 的实现者：`UTcsPieEntityQuery`（本模块）+ `TcsDev` 的 `UTcsDevSkillGateQuery`（**装置侧的发散探针**，注释明写「`UTcsPieEntityQuery` 的 `IsAlive` 不是"活着"，故本组检查必须用发散探针」）。⇒ **契约真的被两个不同实现用过**，这是"注入契约可行"的实证 |
| **前置条件** | 无。**这是本档案里唯一"设计已验证、无需等待"的未实现项**——真项目只需写第二个实现 |

### U-7 ★ `GetFailureList` 的**透传面不存在**——规格要求"失败清单透传"，而消费方为零

| 项 | 内容 |
|---|---|
| **设计是否还成立** | 成立 |
| **依据今天是否仍有效** | 旧 `SPEC-05` §4 硬规则②：「就绪门禁 = `IsRuntimeReady()` 全量判定 + **失败清单透传**」。代码有 `GetFailureList()`，**但全仓（含 `TcsDev` 装置）零调用**。实测评测面绕开它的方式：直接 `Resolve*` 验回指（§3.5） |
| **前置条件** | 出现"要按失败清单决定是否放行"的宿主。规格 `integration-entity:149` 原话：「宿主可经 `GetFailureList` 决定是否放行」——**今天宿主是 `TcsDev`，它选择不看** |

### 补充：五条发现路径的**「已登记但零资产」与「未登记」被区分了**，而这是一条公认正确、却只在一条路径上生效的设计【实测】

`FTcsPrimaryAssetQuery::GetAssetsForDefClass` 的返回值语义（`TcsPrimaryAssetQuery.cpp:44-79`）：

| 情况 | 返回 | `OutDiagnostic` | 是否进失败清单 |
|---|---|---|---|
| 类型**未登记** | false | 非空 | **是**（退化，需补 `UE_DEFINE_PRIMARY_ASSET_TYPE`） |
| 已登记但**零资产** | false | **空** | 否（正常状态） |
| 已登记有资产 | true | 空 | 否 |

**头注释明写这是「本口相对 `GetAssetsByClass` 的新增可观测性」**，且这正是台账 `INTEG-3` 记的「未注册时 AssetManager 按类型查询会静默返回空，排障成本高」那个顾虑的消解点。

⇒ **而它只在 3 条路径上生效**（Chain / Trigger / State）。另 2 条（AttrModDef / Skill）走旧口，**没有这个区分**——它们的「AssetRegistry 不可得」与「零资产」都返回 false 且**都只留 Log**（`_Skill.cpp:35` 注释：「未发现资产是正常状态，不计入失败清单」）。⇒ **同一模块里两种诊断粒度**，见 D-7。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 载体 | 行数 | 与本模块的关系 |
|---|---|---|---|
| **可执行需求** | `openspec/specs/integration-entity/spec.md` | **175**（3 需求 / 20 Scenario） | 主规格。另涉 `entity-query-contract`（40 行）、`primary-asset-loading`（108 行）、`cpp-module-structure`（53 行）、`plugin-descriptor`（59 行）、`host-scripting-e2e-validation`（65 行） |
| **实际实现** | `Source/TcsIntegration/` | 1 536 | — |
| **历史理由** | 旧 `SPEC-05-integration`（`spec/06-module-integration.md`） | **81** | ⚠ **本档案迄今最短的模块规格**（前一篇最短的 `SPEC-06-targeting` 也有 91 行） |
| **设计意图** | `Documents/tcs-arch/DEC-*` / 台账 `INTEG-1/2/3` | — | `INTEG-3` 是**本模块唯一有实质内容**的台账行（2026-10-15 已 ✅ 部分消费） |

### 7.2 `SPEC-05-integration` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

旧文档 **81 行**（`§1~§9`），逐条对代码：

| # | 旧文档声称 | 代码实际 | 判定 |
|---|---|---|---|
| 1 | `§1` 依赖 `TcsSkill, TcsCue` | 依赖 11 条（`Build.cs`）；**`TcsSkill` 确实有**（`RegisterSkillDef`）；**`TcsCue` 模块不存在** | **半准确**——`TcsSkill` 那条对了，`TcsCue` 是虚构模块 |
| 2 | `§1` 「（R0 §9）此处只列**实际 include**」 | 实际 include 是 4 个 Tcs 模块（Effect / Attribute / Buff / Skill）+ AssetButler | **半准确**——把 `TcsCue` 换掉才对，但 `TcsBuff` / `TcsAssetButler` **两个模块整篇未提** |
| 3 | `§1` 「被依赖：TcsEditor（全部模块均被其消费）」 | `TcsEditor` **模块不存在**；`Source/` 零命中 | **过期**：被依赖面实际是 **`TcsDev`（1 个）** |
| 4 | `§1` 「~~StateTree 依赖~~ 已删」 | 正确删除，且**判据写明了两条**（裁定⑦ + as-built 双零实测） | ✅ **自纠准确**（2026-10-15 作者自己改的，是全篇最新的一段） |
| 5 | `§2.1` `UCombatDefLibrarySubsystem → UTcsDefLibrary → UTcsDefinitionSubsystem` 三轮改名史 | 类名 = `UTcsDefinitionSubsystem` ✅ | ✅ **准确**（且理由完整：避开"Registry"一词） |
| 6 | `§2.1` 「加载（六域三策略：PreloadAll/PreloadSelected/OnDemand）」 | 三策略**枚举已落**在 `TcsAssetButler`，**读点 0**；且只有 3 个 Def 类注册了 | **过期**：载体换了、范围从"六域"变"3 类"、语义未生效 |
| 7 | `§2.1` 「D3-2 就绪状态机住此：`Unloaded→Loading→Ready/Failed`，Ready 广播至多一次」 | **`bRuntimeReady` 一个 bool**，无状态机、**无广播**（D-1） | **完全未落地**——且这是本模块**唯一被规格与旧文档双双承诺、代码全无**的机制 |
| 8 | `§2.1` 「R3 发现机制用 `IAssetRegistry::GetAssetsByClass`（`PrimaryAssetTypesToScan` 属 M6 轮，见 `INTEG-3`）」 | 3 条已改 `FTcsPrimaryAssetQuery`、2 条仍旧口；`PrimaryAssetTypesToScan` **被明确否决**（改代码自注册） | **过期且方向反了**：旧文档说"等等到 M6 用 ini 注册"，实际是"**永不**用 ini"。⚠ **这条在 `openspec` 侧也已过期**（G-7 已记） |
| 9 | `§2.1` 「装配到世界（跨级方向纪律）」+ `World.cpp:2447` / `:2601` 时机核验 | 代码**逐字实现**，且头注释保留了全部理由 | ✅ **准确**（本模块最准确的一段） |
| 10 | `§2.2` `UCombatEntityComponent`（**类名无 `Tcs` 前缀**） | 实际 `UTcsCombatEntityComponent` | **过期（改名）**——`SPEC-05` 用的是旧命名族 |
| 11 | `§2.2` 第 2 职责「实现 `ICombatAttributeProvider`（缓存桶指针转发）」 | **不实现任何接口**（U-4） | **未落地**（台账 `INTEG-1` 部分覆盖） |
| 12 | `§2.2` 第 2 职责「并暴露该实体的 AttributeSet 查询与切换入口」 | 无 AttributeSet（U-4） | **未落地**（台账 `INTEG-1` 主体） |
| 13 | `§2.2` 第 3 职责「可选 StateTree 决策宿主 + `UCombatStateTreeSchema`」 | **零实现**，且裁定⑦ 已把 StateTree 判给未来的 `TcsStateTree` 模块 | **已推翻**（有理由、有裁定、`§1` 里也写了，但 `§2.2` 正文没删） |
| 14 | `§2.2` 「宿主注入接口：`ICombatEntityQuery{Enumerate/GetLocation/IsAlive}`（**三支**）」+ `IRelationResolver{IsHostile}` | `ITcsEntityQuery` **四支**（多了 `IsEntityReady`，R6 追加）；`IRelationResolver` **零命中** | **半准确**——三支过时（少一支）、`IRelationResolver` 未落地（台账 `WAIT-5`） |
| 15 | `§2.3` `ICombatReplicationProxy` 契约（草案） | **零命中**（U-3） | **未落地**——但旧文档自己标了"草案 + 推迟"，属**如实记录** |
| 16 | `§3` 「DefLibrary：`LoadAll/LoadSelected/EnsureLoaded(DefTag)`」 | **零命中**（U-5） | **未落地**，且**载体已改判** |
| 17 | `§3` 「DefLibrary 管辖 `UTcsAttributeSetAsset` 一族」 | 无此类（U-4） | **未落地**（`INTEG-1` / `INTEG-2`） |
| 18 | `§3` 「WorldRegistry：`RegisterEntity/UnregisterEntity/GetEntityState/ForEachEntity/ResolveProvider`」 | `UCombatWorldRegistrySubsystem` **零命中**（U-1）；`RegisterEntity`/`UnregisterEntity` 实际在 **`UTcsAttributeSubsystem`** | **未落地**（U-1），且**归属已迁移**（注册口在属性门面） |
| 19 | `§3` 「Entity 组件：`GetHandle()` / `GetAttributes()`（含"当前 Set"查询）/ 可选 `GetBrain()`」 | `GetEntityHandle()` ✅；`GetAttributes()` / `GetBrain()` **零命中** | **半准确**——`GetHandle` 改名了但语义对；后两者未落地 |
| 20 | `§4` 硬规则①「到达 Ready 的所有迁移路径**单出口**经过 OnReady 派发点」 | **无 Ready、无 OnReady、无状态机**（D-1） | **未落地** |
| 21 | `§4` 硬规则②「就绪门禁 = `IsRuntimeReady()` 全量判定 + 失败清单透传」 | `IsRuntimeReady` ✅（唯一消费者 = 组件）；**失败清单透传零消费者**（U-7） | **半落地**——前半落地、后半没有 |
| 22 | `§4` 「实体状态机门禁：`TryActivate` 第一道门 = 实体状态 Ready」 | **状态机不存在**（U-1）；门禁改成了 `IsEntityReady`（推给宿主） | **已推翻**——设计被反向实现，理由在 `entity-query-contract:16` |
| 23 | `§4` 「StateTree 决策语义：tick 三值映射 / ManualOnly 默认」 | **零实现**，且已裁定归未来模块 | **已推翻**（同 #13） |
| 24 | `§4` 「空间查询：`RadiusArea`（后置形态）= 遍历注册表已注册实体 + `ICombatEntityQuery` 位置过滤」 | `RadiusArea` **零命中**（`ARCH-09` U-3 已记） | **未落地**（后置，旧文档自己标了） |
| 25 | `§5` 网络姿态 NET-1/2 落点 | `ICombatReplicationProxy` 零命中 | **未落地**（同 #15） |
| 26 | `§6` 非目标（不做 Mass / 输入 / UI / 复制 / 关卡 / AI） | 全部**确实没做** | ✅ **准确**（六项全对） |
| 27 | `§8` 修订记录 v1~v4（四条）+ 「v2 定稿重写」 | 记的是 **2026-09-02 / 09-17 / 09-21**；而 `§1` 里有一段 **2026-10-15** 的改动**没进修订记录** | **过期（修订记录断链）**——最后一条实质改动（删 StateTree 依赖）**没有对应的修订条目** |
| 28 | `§9` 验收钩子：「构造"定义加载失败"与"快慢两种注册路径"，验证 OnReady 单出口与 Failed 清单」 | **OnReady 不存在** ⇒ 该钩子**无法执行** | **过期**——钩子验的东西不存在 |

### 7.3 差异的**类型分布**（与 `ARCH-05` 起的七类判据对照）

| 类型 | 条数 | 具体编号 | 说明 |
|---|---|---|---|
| ✅ **准确 / 如实记录未做** | **5** | #4 / #5 / #9 / #15 / #26 | 全篇质量最高的部分；#15 / #26 是"标注了后置/草案"的如实记录 |
| **半准确（一半对一半错）** | **4** | #1 / #2 / #14 / #19 | `TcsSkill` 对而 `TcsCue` 虚构 / 三支对而少一支 |
| **从未落地（旧文档当已做写）** | **7** | #7 / #11 / #12 / #16 / #17 / #18 / #20 | 状态机、AttributeSet、加载入口、WorldRegistry、OnReady |
| **已推翻仍留存** | **3** | #13 / #22 / #23 | StateTree 三项（`§1` 已删声称、`§2.2`/`§4` 没删） |
| **未落地（旧文档自己标了后置）** | **2** | #24（`RadiusArea`）/ #25（NET） | 属"如实记录"，不算错 |
| **范围 / 形态过时** | **2** | #3 / #6 | 被依赖面写 `TcsEditor` 实际是 `TcsDev` / "六域三策略"变"3 类且零读点" |
| **改名滞后** | **1** | #10（`UCombatEntityComponent` → `UTcsCombatEntityComponent`；#19 的 `GetHandle` 并入半准确行） | `Combat* → Tcs*` 族 |
| **方向已反** | **1** | #8（`PrimaryAssetTypesToScan`） | 旧文档说"等 M6 用 ini 注册"，实际**永不**用 ini |
| **修订记录断链** | **1** | #27 | 2026-10-15 的实质改动没进 `§8` |
| **验收钩子失效** | **1** | #28 | 钩子验的东西（`OnReady` 单出口）不存在 |
| **半落地（前半有后半无）** | **1** | #21 | `IsRuntimeReady` 有、**失败清单透传零消费者** |
| **合计** | **28** | — | 机械复核：5+4+7+3+2+2+1+1+1+1+1 = 28 ✓ |

**就地订正率 = 5 / 28 ≈ 18%**——这是本档案**已审十一篇里最低的**。

| 篇 | 旧文档行数 | 差异项 | 自纠准确 | 订正率 |
|---|---|---|---|---|
| `ARCH-05` `SPEC-02-states` | 643 | 14 | 约 7 | ~50% |
| `ARCH-06` `SPEC-03-effects` | 498 | 14 | 3 | ~21% |
| `ARCH-07` `SPEC-02-states` | 643 | 14 | 7 | ~50% |
| `ARCH-08` `SPEC-04-skill` | 390 | 14 | 6 | ~43% |
| `ARCH-09` `SPEC-06-targeting` | 91 | 8 | 3 | 37.5% |
| `ARCH-10` `SPEC-08-damage` | 135 | 14 | 5 | ~36% |
| **`ARCH-11` `SPEC-05-integration`** | **81** | **28** | **5** | **~18%** |

⇒ **两条规律同时成立**：

1. **`ARCH-09` §8.4 立的判据再次得到验证**：短文档（< 150 行）的「收束节」不可靠——本模块 `§8 修订记录` 只有 4 条，且**最后一条实质改动没进去**；
2. **但真正的原因不是"短"**：`SPEC-05` 的 28 项差异里，**7 项是"从未落地却当已做写"**，而它**没有 R5/R6 那样的收束回写节**。⇒ `ARCH-09` 的判据要**再补一句**：短文档的风险不在"收束节简陋"，而在**它压根没有收束节**——`SPEC-05` 的 `§8` 是**修订记录**（记"改了什么"），不是**收束回写**（记"现在是什么"）。

### 7.4 `openspec` 侧的缺口（三处，且已有两次被前篇登记）

| # | 缺口 | 状态 |
|---|---|---|
| 1 | `integration-entity:51` 的「五条按类路径发现」已假（3 条改走 `FTcsPrimaryAssetQuery`），且「不依赖 `PrimaryAssetTypesToScan` 注册」与 3 处自注册冲突 | **已由 `ARCH-03` 的 G-7 登记** |
| 2 | `integration-entity:74/136` 写 `UTcsStateSubsystem::RegisterStateDef` / `GetRegisteredStateDef`——两个符号**在全仓不存在**（实际是 `UTcsBuffSubsystem::RegisterBuffDef` / `GetRegisteredBuffDef`） | **已由 `ARCH-05` 的 D-4 与 `ARCH-07` 的 D-3 各记一次**（G-21 型：同一缺口多档案各记一次） |
| 3 | `integration-entity` 的**单出口 `OnDefinitionsReady` 与跨世界装配 Scenario 被删空** | **本篇首次登记**（D-1 / D-3） |

⇒ 第 1、2 两条是**跨模块的重复登记**——正是 `ARCH-08` 的 G-21（同一缺口在 ≥2 个模块档案里各记一次、无跨模块待办）的**第三、四次命中**。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

| 步 | 内容 |
|---|---|
| 1 | 读 `integration-entity`（175 行）+ `entity-query-contract`（40）+ `primary-asset-loading`（108）+ `cpp-module-structure`（53）+ `plugin-descriptor`（59）+ `host-scripting-e2e-validation`（65）+ `entity-runtime-mechanism`（155） |
| 2 | 读 `Source/TcsIntegration/` **全部 16 个文件**（1 536 行），提取 3 个 `UCLASS` / 0 个 `USTRUCT` / 0 个 `UENUM` / 26 个公开方法（其中 1 个 `UFUNCTION`） |
| 3 | 读旧 `SPEC-05-integration`（**81 行**）逐条对代码 → 产出 28 项差异清单（§7.2） |
| 4 | 追竖切：`Register*` 三个登记口 + `Resolve*` 五个解析口 + `IsRuntimeReady` + `SetEntityQuery` 在**全仓（插件 + LAC `Source/`）**的全部调用点 |
| 5 | 交叉验证 U-1..U-7：每条「未实现」都反查过**是否只是搬到了别处**（U-1 的中央注册表 → 拆成两半；U-5 的三档加载 → 改载体；U-4 的 `ITcsAttributeProvider` → 有接口无实现者） |
| 6 | **按 `ARCH-10` 的 G-30 判据跑了 `git log --follow` + `git show <commit>:<file>`**，抓到 D-1 与 D-3 两条「被删空」（详见 §8.4） |

### 8.2 本次审核用到的**七类判据**（`ARCH-05` 四类 + `ARCH-08` 第五 + `ARCH-09` 第六 + `ARCH-10` 第七）

| 类 | 判据 | 本模块命中 |
|---|---|---|
| 1 | **符号名**（拆分后引用滞留） | 1 处（`integration-entity:74/136` 的 `RegisterStateDef`，前篇已记） |
| 2 | **模块名** | 0 处（`TcsCue` 是虚构模块而非改名，归「范围过时」） |
| 3 | **类型名** | 1 处（`UCombatEntityComponent`，旧文档侧） |
| 4 | **字段位置** | 0 处 |
| 5 | **时态词**（`本 Task` / `本轮` / `尚未`） | **0 处**（`integration-entity` 里的 12 处日期标注**逐条核过，全部与代码一致**） |
| 6 | **设计已推翻** | 2 处（StateTree 三项 #13/#22/#23；`PrimaryAssetTypesToScan` #8） |
| 7 | **结构在、内容被删空** | **2 处**（D-1 单出口 / D-3 跨世界 Scenario） |

**本模块第一次让第 5 类（时态词）零命中**——这与 `ARCH-08` 的 D-2（`本 Task` 说三档未实现而代码全实现）**方向相反**。`integration-entity` 的日期标注风格是「**日期 + 改了什么 + 判据**」，而不是「**本 Task / 本轮**」这类相对时态 ⇒ 它**不会被时间追上**。这是一条可复制的写作判据。

⇒ 判据 7 是本模块的**主导方向**（2/9），而判据 1~4（拆分族）**合计只有 2 处**——远少于 `ARCH-05` 的 4 处、`ARCH-07` 的 10 处。

### 8.3 置信度分布

| 置信度 | 条数 | 具体 |
|---|---|---|
| **【实测】** | 15 | D-1..D-9、U-2..U-7 全部逐条读过代码或跑过命令；§7.2 的 28 项逐条 grep 验证 |
| **【推断】** | 2 | D-4（`RemoveAtSwap` 破坏稳定序——行为已推演，但**未跑装置实测**）；D-5（门禁无补救——代码确实没有，但「宿主因此无法补救」是推断） |
| **【待核】** | 1 | §3.4 的 `SeededWorld` 单槽在多世界下的失效——已推出「串行更替恰好够用、并行存在才失效」，但**未构造多世界场景** |

### 8.4 方法论收获：**G-30 的判据在第二个模块上再次命中，且「凶手」是同一个提交**

`ARCH-10` 立的第七类判据是：

> 标题 / Purpose 承诺 N 项而正文里少于 N 项时，MUST 跑 `git log --follow` + `git show <commit>:<file>` 区分「从未写过」与「写过被删」。

**本模块按此执行，抓到两条，且两条的「凶手」都是 `539b14e`**：

| 提交 | 对 `damage-step-library` | 对 `integration-entity` |
|---|---|---|
| `539b14e`（2026-09-22，标识体系 tag 化） | 十步契约 → 一段泛论（`ARCH-10` D-2） | 单出口 + 3 个 Scenario → 删除（本篇 D-1 / D-3） |

⇒ **「一次无关改造顺手删掉了别的东西」不是偶发，而是这个提交的第二个受害者**。`ARCH-10` 说「同批的 `effect-step-dispatch` 也做了 tag 化却没被掏空 ⇒ 不是 tag 化的必然代价，是单文件失手」——本模块的发现让这句话要**收紧**：

**同一个提交伤了两个文件，而这两个文件都属于同一个特征：每一行都提到键名。**

| 文件 | 有没有被掏空 | 为什么 |
|---|---|---|
| `damage-step-library` | ✅ 被掏空（`ARCH-10` D-2） | 十步清单每行都写键名 |
| `integration-entity` | ✅ 被掏空（本篇 D-1 / D-3） | 单出口那段每行都写 `ChainId` / `FName` 一类的键 |
| `effect-step-dispatch` | ❌ 没被掏空 | 步骤分派契约不逐行提键名 |
| `effect-chain` | ❌ 场景 **9 → 5**，但**需求正文完整** | 键名集中在少数几行，正文其余部分未被重写 |

⇒ **判据可以精确化一条**：tag 化这类「全局替换」改造，**风险与「每行都含被替换词」成正比，而不与文件大小成正比**——`damage-step-library`（121 行）与 `integration-entity`（175 行）**都不大**。

⇒ **推论**：将来再做全局替换类改造时，MUST 先跑一条统计——「**含被替换词的行的占比**」。占比高的文件是**回归高危区**，改完之后 MUST 逐条对回原文。

### 8.5 与 `ARCH-10` 结论相反的一条发现：**本模块的「同款代码」三对全部无缺陷**

`ARCH-10` 立了 **G-31**：

> 凡两个模块的某文件 `diff` 后只剩命名/文案差异，MUST 对其中一份审出的缺陷**直接在另一份上复查**。

**本模块实测了这条判据的边界**。`TcsIntegration` 能查到的「同款代码」有三对：

| 同款对象 | diff 结果 | 有没有缺陷 |
|---|---|---|
| `_AttrModDef.cpp:15-27` vs `_Skill.cpp:16-28`（**模块内** 13 行） | 除类名与文案外**逐字相同** | ❌ **无**（两段实现相同且都对） |
| 三份 `AddReferencedObjects`（vs `TcsEffect` / `TcsBuff` / `TcsSkill` 的同名函数） | 结构相同（逐条 `AddPropertyReferencesWithStructARO` + `Super::`） | ❌ **无**（都对；且本模块的注释最详细，还给出了「第五份为什么不需要」的判据） |
| `SeedWorld` 的四段装配循环 | 三段几乎逐字（链 / 状态 / 技能各一个 for 循环） | ❌ **无**（差异是各登记口的签名与返回值语义） |

⇒ **G-31 的判据应当补一条否定面**：**逐字复制的代码不必然含缺陷**，它只是让缺陷有第二份载体。**发现同构 ≠ 一定有缺陷，但必须复查**——本模块复查了三对，**零缺陷**。

这与 `ARCH-10` 的 `TcsEffect`↔`TcsDamage` 形成完整对照：**那一对是「同构且同缺陷」，这一对是「同构但都无缺陷」**。⇒ **同构是「必须查」的信号，不是「必然有错」的结论**。

### 8.6 与 `ARCH-09` 相反的一条对照：**本模块的日志判据是齐的**

`ARCH-09` 的 G-25 立了：「降级后结果与『正常配置但确实没选中』不可区分者，MUST 至少一条 `Warning`」，并按此在 `TcsTargeting` 里找出 **3 处需补**。

**本模块按同一判据逐条核过全部降级路径**：

| 降级点 | 日志 | 与正常路径可区分？ |
|---|---|---|
| `SeedWorld` 无内容即返回 | 无日志（但**也没做事**） | ✅ 可区分（装配行不出现） |
| `SeedWorld` 非游戏世界 | 静默返回 | ⚠ **不可区分**——但规格 `:56-58` 明写「静默跳过」是设计 |
| EffectSubsystem 不可得 | 静默返回 | ⚠ 同上 |
| StateSubsystem / SkillSubsystem 不可得 | **静默跳过该批**，但装配行**仍打印分母** | ✅ **可区分**（分母非零而分子为 0） |
| 触发定义引用链未解析 | **Warning**（含 `TriggerTag` + 链 id） | ✅ |
| 发现未登记类型 | **Warning + 进失败清单** | ✅ |
| 发现已登记但零资产 | Log（正常态） | ✅（可与「未登记」区分） |
| 组件四道门禁失败 | **各一条 Warning（含 Actor 名）** | ✅ |
| `Resolve*` 未命中 | **零日志、不 ensure** | ⚠ **不可区分**——但规格 `:60` 明写「正常查询路径，不 ensure」 |

⇒ **三处「不可区分」全部有规格明文授权**（非游戏世界静默 ×2、未命中不 ensure）。**这是本档案第一次出现「G-25 判据全过」的模块**。

⇒ 且其中一处的做法值得单独立为判据：**「分母照打」是零成本的可区分性**——`StateSubsystem` 不可得时跳过整批，但日志行仍写 `状态定义 0/12 条`。分母在那，读者立刻知道"不是没有定义，是没装上"。

---

## §9 结论

### 9.1 三句话

1. **本模块是全仓「最薄的一层」，薄到几乎只剩"翻译"两个字**——3 个 `UCLASS`、1 个 `UFUNCTION`、26 个公开方法，全部职责可以压成两条边：资产→世界、Actor→句柄。它**不做执行的判据它不是"没写完"，而是"本来就不该写"**。
2. **它的规格死因与 `TcsDamage` 完全一样，而且是同一个提交**——`539b14e`（标识体系 tag 化）删掉了 `damage-step-library` 的十步，也删掉了 `integration-entity` 的单出口与跨世界 Scenario。**`ARCH-10` 说那是"单文件失手"，本模块证明它是"一个提交伤了两个文件，且都伤在"每行都含键名"上**。
3. **它有一条与全档案相反的记录**：G-25（降级不可区分）**全过**、G-31（同构必同缺陷）**三对全部无缺陷**、第五类判据（时态词）**零命中**。⇒ **本模块的代码质量与注释质量在十一篇里并列最高**，而**旧文档质量垫底**（订正率 18%，全档案最低）。

### 9.2 一句值得记住的话

> **`TcsIntegration` 是全仓唯一一个「代码比规格干净」的模块——而它脏的那一面，恰好是被人顺手擦掉的那半页。**

### 9.3 与前十篇的对照

| 篇 | 模块 | 行数 | 主方向 | 纯代码债 | 纯规格债 | 备注 |
|---|---|---|---|---|---|---|
| `ARCH-04` | `TcsAttribute` | 3 012 | 规格更严、代码更松 | 少 | **4** | G-10 立 |
| `ARCH-05` | `TcsState` | 2 753 | 代码搬走、引用滞留 | 1 | 4 | G-12 立 |
| `ARCH-06` | `TcsEffect` | 7 115 | 各错一半 | **5** | 4 | G-15 立 |
| `ARCH-07` | `TcsBuff` | 5 839 | 纯规格为主 + 唯一崩溃级 | 1 | 多 | G-18 立 |
| `ARCH-08` | `TcsSkill` | 11 052 | 代码干净、规格掉队 | **1** | **8** | G-22 立 |
| `ARCH-09` | `TcsTargeting` | 1 144 | 判据从未被写下 | 1 | 4 | G-23..G-29 立 |
| `ARCH-10` | `TcsDamage` | 3 225 | 规格被掏空 ⇒ 代码失去对照物 | **4** | 3 | G-30 / G-31 立 |
| **`ARCH-11`** | **`TcsIntegration`** | **1 536** | **规格被掏空（同一提交）+ 代码干净** | **3** | **4** | **旧文档订正率 18%（最低）** |

**本模块的位置**：它同时具备 `ARCH-08` 的形态（代码干净）与 `ARCH-10` 的形态（规格被删空）——**第一次出现两者叠加**。

| 形态 | 判据 | 出现在 |
|---|---|---|
| 代码干净 + 规格掉队 | 纯代码 ≪ 纯规格 | `ARCH-08`（1 vs 8） |
| 规格被删空 + 代码随之偏 | 纯代码 ≈ 纯规格，且删空有明确凶手 | `ARCH-10`（4 vs 3） |
| **规格被删空 + 代码本来就干净** | 删空**没有传导**到代码 | **`ARCH-11`（3 vs 4）** |

⇒ **修正 `ARCH-10` 的推论**。`ARCH-10` 立的是：

> 规格的单方面缺失会**传染**成代码缺陷，反向不成立。

本模块给出了**反例**：`integration-entity` 的单出口被删空后，**代码里本来就没有 `OnDefinitionsReady`**——删与不删，代码都一样。⇒ 更准确的表述是：

> **规格缺失不"制造"缺陷，它只"让已有的缺陷变得不可发现"**。`TcsDamage` 的 D-4（`CandidateKey` 零读取）/ D-5（多目标语义）之所以在规格侧无从判起，不是因为缺了规格才写错代码，而是**写错了没人对照**。本模块的 D-4（`RemoveAtSwap`）是同一个道理：`integration-entity:32` 的稳定序 MUST **还在**，所以这条缺陷**能被发现**；如果那一行也被删了，它就会像 `TcsDamage` 的多目标语义一样沉底。

**⇒ 立一条新的、可执行的判据（G-32 候选）**：

> **「规格完整度」是缺陷的"可发现性"，而不是"发生率"**。因此审核 MUST 区分两件事：① 代码有没有缺陷（可直接测）；② 规格还在不在（决定这个缺陷会不会被后人发现）。**两者必须分开登记**——把"规格被删空"记成一条代码债是错的，它应当单独占一行，且严重度**不低于**它掩盖的那些缺陷。

### 9.4 给下一轮的四条建议（按代价从低到高）

| # | 动作 | 代价 | 收益 |
|---|---|---|---|
| 1 | **按 D-1 + D-3 的结论，用 `git show a1804ed:` / `git show 539b14e^:` 把 `integration-entity` 被删的单出口 + 3 个 Scenario 恢复** | ~30 行规格，零代码 | 恢复「M6 双层引导」这条设计线索；同时把 D-5 的前置补上 |
| 2 | **删 3 条空转依赖 + 改 `Build.cs` 注释「五条改走」→「三条改走」** | 4 行 | 依赖图读数变准；防止下一个人以为两条旧口不适用是"忘了" |
| 3 | **`EnumerateEntities` 的 `RemoveAtSwap` → `RemoveAt`（或正向遍历 + 收集后删）** | 2 行 | 兑现规格的稳定序 MUST |
| 4 | **裁 U-2（实体注销的编排点）**——它同时决定 `TcsBuff` / `TcsSkill` 两个 `UnregisterEntity` 有没有消费者，也决定本模块组件要不要扩职责 | 一次裁决 | 消除本档案唯一一条「有真实运行期错误状态」的未实现项 |

### 9.5 十一篇之后的档案级结论

| 事实 | 读数 |
|---|---|
| 11 个模块全部审完 | ✅ |
| 全局待办 | **G-1..G-32**（32 条，本篇交付时含新立的 G-32；`ARCH-00` 之后扩到 **G-1..G-35**） |
| 已修 | **0 条**（全部等用户裁决） |
| 纯代码债（十一篇合计） | 约 **22 条**，其中 ★★★ 级 **3 条**（`ARCH-07` D-1 崩溃级 / `ARCH-10` D-1 静默必中 / **`ARCH-11` D-2**） |
| 纯规格债（十一篇合计） | 约 **40 条** |
| **主导方向** | **规格侧问题（约 2:1 压倒代码侧）** |

⇒ **本档案十一篇的核心结论可以压成一句**：

> **这套代码写得比它的文档好。** 代码里的注释密度、判据质量、以及「为什么这么写而不是那么写」的痕迹，普遍高于 `openspec/specs/` 与旧 `SPEC`。⇒ 后续维护的正确姿势**不是"照规格补代码"**，而是**先把规格补回代码的水平**，再谈实现。

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。`ARCH-11-integration` 完成（**990 行**）。审出 **D-1..D-9 / U-1..U-7**。三条结构性发现：① **规格被删空（G-30）在第二个模块上再次命中，且凶手是同一个提交 `539b14e`**——它删掉了 `damage-step-library` 的十步，也删掉了 `integration-entity` 的单出口与 3 个 Scenario（含**跨世界装配**唯一的验证场景）；② **首次出现「规格被删空 + 代码本来就干净」的形态**，据此修正 `ARCH-10` 的推论：**规格缺失不"制造"缺陷，它只"让已有的缺陷变得不可发现"**（G-32 候选）；③ **三对「同款代码」全部无缺陷**（G-31 的否定面）、**G-25 的降级可区分性全过**、**第五类判据（时态词）零命中**——本模块的代码与注释质量在十一篇里并列最高，而**旧 `SPEC-05` 订正率 18% 为全档案最低**（81 行 / 28 项差异 / 5 项准确）。**§7.3 的订正**：`SPEC-05` 的 28 项差异按 11 类重新归类（首次给出「半准确」「半落地」两个对称档），自纠准确数由粗估的 4 修正为 **5**，订正率 **18%**。**§3.4 的订正**：`SeededWorld` 单槽判据由「多世界即失效」收紧为「**串行更替够用、并行存在才失效**」（【待核】——今天的 PIE 配置下不命中）。 |
