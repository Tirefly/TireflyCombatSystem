# ARCH-07-buff · TcsBuff（Buff 域的施加态运行本体）

- **文档 ID**：`ARCH-07-buff`
- **类型**：模块档案（现状审核）
- **状态**：生效中（2026-10-17 审）
- **权威范围**：回答「`TcsBuff` 今天实际长什么样、契约是什么、欠了什么」。**不承载决策理由**（旧 `decisions/` 与 `log/` 是理由的真相源），**不承载可执行需求**（`openspec/specs/state-*` 是契约的真相源）。三者分工见 [`ARCH-INDEX`](INDEX.md) §2。
- **模块规模**：**47 文件 / 5 839 行**（`.h` + `.cpp`，与 `ARCH-INDEX` §3 同基准；`TcsBuff.Build.cs` 另 33 行 ⇒ 含它 48 文件 / 5 872 行）
- **需求锚点**：8 份能力 / **1 361 行** / **42 Requirement** / **157 Scenario**（`state-instance-lifecycle` 332·7·41、`state-stacking-policies` 221·6·26、`state-param-snapshot` 201·6·25、`state-modifier-materialization` 160·7·18、`state-behavior-fragments` 108·4·12、`state-def-asset` 186·6·19、`state-relation-check` 117·5·12、`state-step-library` 36·1·4）
- **审核顺序**：本档案第 7 篇（`ARCH-01` ~ `ARCH-06` 已审）

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

> **`TcsBuff` 是「Buff 施加态」的运行本体——把一份定义变成一条可查、可到期、可叠层、可改属性、可起链的在册实例，并把它撤销干净。**

这句话里每一段都有独立的落点，且**互不重叠**：

| 段 | 落点 | 行数 |
|---|---|---|
| 「定义 → 实例」 | `FTcsBuffOps::Apply`（`TcsBuffOps.cpp:16-187`）+ 五轴决策（`TcsBuffOps_Stack.cpp`） | 317 + 330 |
| 「可到期」 | `PushExpiry` / `PushPeriod` / `ScheduleTime`（`TcsBuffOps_Lifetime.cpp`） | 210 |
| 「可改属性」 | `FTcsBuffModifierMaterializer` + `MountModifiers` / `StripModifiers` | 113 + 137 |
| 「可起链」 | `WireTriggerRows` / `UnwireTriggerRows`（`TcsBuffOps_Trigger.cpp`） | 79 |
| 「可响应事件」 | `FTcsBuffBehaviorRegistry` + `DispatchBehaviorEvent` | 166 + 131 |
| 「撤销干净」 | `Remove`（`TcsBuffOps.cpp:192-284`）的六段顺序 | 93 |

**它的「运行本体」性质有一条硬证据**：本模块**不持有任何跨实例的机制**。桶与注册表是共享层模板的别名（`TcsBuffRegistry.h:41-52`），代际发号是共享层的进程级发号器，重入守卫是共享层的 `TTcsEntityBroadcastGuard`，重申请决策接口是共享层的 `FTcsEntityAdmission`。本模块自己写的只有**领域语义**——层数、时值、周期、五轴、关系挂点。

### 1.2 文件组与职责【实测】

| 目录 | 行数 | 文件数 | 职责 | 关键符号 |
|---|---|---|---|---|
| `Public/`（根） | 470 | 2 | 门面（460 行）+ 日志通道 | `UTcsBuffSubsystem` / `LogTcsBuff` |
| `Public/State/` | 1 579 | 12 | 实例、句柄、枚举、事件、桶别名、行为/决策 Fragment、注册表、物化器、引擎函数头 | `FTcsBuffInstance` / `FTcsBuffOps` / `FStateStackPolicy` |
| `Public/Def/` | 262 | 3 | Def 数据形状、资产类、编辑期表行 | `FTcsBuffDef` / `UTcsBuffDefAsset` / `FTcsBuffDefTableRow` |
| `Public/Param/` | 336 | 2 | 四个等级类参数源 | `FTcsParamSource_{State,Instigator}Level{Array,Map}` |
| `Public/Host/` | 108 | 1 | 宿主等级读口 + 域派生求值上下文 | `ITcsEntityLevelProvider` / `FTcsStateEvaluateContext` |
| `Public/Chain/` | 57 | 1 | 链步骤形状 | `FTcsStepApplyState` |
| `Private/State/` | 2 137 | 18 | 引擎函数实现（8 个 `.cpp`）+ 物化器 + 属性白名单薄壳 + 行为注册表 | `FTcsBuffOps_*` / `FTcsBuffModifierMaterializer` |
| `Private/`（根） | 526 | 4 | 门面实现（3 个 `.cpp`）+ 日志通道 | `TcsBuffSubsystem{,_Behavior,_Definition}.cpp` |
| `Private/Def/` | 253 | 1 | 资产身份 + 作者期校验 | `UTcsBuffDefAsset::IsDataValid` |
| `Private/Chain/` | 91 | 1 | 步骤执行器 + 一行自注册 | `ExecuteStepApplyState` |
| 模块壳（根） | 20 | 2 | `IMPLEMENT_MODULE` 样板 | `FTcsBuffModule` |

**合计 5 839 行 / 47 文件**（不含 `TcsBuff.Build.cs` 的 33 行）。

**一条值得记的拆分纪律**：`FTcsBuffOps` 的 561 行头 + **8 个 `.cpp`**（`Apply/Remove` 一文件、`_Stack` / `_Relation` / `_Lifetime` / `_Modifier` / `_Query` / `_Snapshot` / `_Trigger` / `_Events` / `_Behavior` 各一）——**按「一个操作族一个文件」切**，而不是按「头/实现」切。头注释把这套切法写在 `TcsBuffOps.h:49-53`，且**每加一族就补一行说明**。这是全仓最彻底的一次按职责拆分（对照 `TcsEffect`：门面拆 7 个 `.cpp`，引擎函数只拆 3 个）。

### 1.3 本模块最反常的一点：**规格主语第一次全对，但符号名全错——且错的方向互相相反**【实测】

**先说好消息，因为它是全仓第一次**：`ARCH-05` 审 `TcsState` 时抓到的最严重缺陷是「**权威层主语错了**」——10 份 `state-*` 规格里 7 份整篇在讲 `TcsBuff`，`openspec` 里没有任何能力回答「`TcsState` 剩什么」。

**到了 `TcsBuff`，这个问题消失了**：8 份能力的 `TcsBuff` / `TcsState` 提及比是 **26:15 / 15:1 / 6:8 / 11:0 / 20:2 / 25:14 / 1:3 / 3:0**——主语全部指向 `TcsBuff`，`TcsState` 的出现几乎都是「共享层」语境的正当引用。

**但符号名是全仓最坏的一档**。我把规格里出现的 `TcsBuff` 类型名逐个拿去 `Source/` 里查，**6 个全命中 0**：

| 规格写的 | 代码里的真名 | 命中数 |
|---|---|---|
| `EApplyResult` / `EAR_Applied` / `EAR_Refreshed` / `EAR_Stacked` / `EAR_Rejected` | `EBuffApplyResult` / `EBAR_*` | **0** |
| `EDurationPolicy` / `EDP_Finite` / `EDP_Infinite` | `EBuffDurationPolicy` / `EBDP_*` | **0** |
| `ETcsPeriodRefresh` / `EPR_Keep` / `EPR_Reset` / `EPR_Immediate` | `ETcsBuffPeriodRefresh` / `EBPR_*` | **0** |
| `FTcsBuffStackPolicy` | `FStateStackPolicy` | **0** |
| `GetState(Handle) -> const FTcsStateInstance*` | `const FTcsBuffInstance*` | — |
| `UTcsStateSubsystem::RegisterStateDef` 族 | `UTcsBuffSubsystem::RegisterBuffDef` 族 | **0** |

**关键不是「错了 6 个」，而是「错的方向相反」**：

- 前三个枚举，代码**加了 `Buff` 前缀**（`EBuffApplyResult`），规格停在**无前缀的旧名**（`EApplyResult`）；
- 堆叠策略，代码**保持 `State` 无前缀**（`FStateStackPolicy`），规格却写成了**带 `Buff` 前缀**（`FTcsBuffStackPolicy`）。

⇒ 同一次「`TcsState` 拆出 `TcsBuff`」的重命名，**在两类类型上按相反方向执行**，而规格又是第三种组合。这不是「文档没跟上代码」，是**改名本身没有单一规则**（详见 §5 D-4）。

**最刺眼的一处证据在同一份文件里**：`state-stacking-policies/spec.md` 第 5 行（Purpose）写 `EBAR_Applied / Refreshed / Stacked / Rejected`（**新名**），第 82~86 行（正文决策树表）写 `EAR_Applied` / `EAR_Refreshed` / `EAR_Stacked` / `EAR_Rejected`（**旧名**）。**同一份规格、相隔 77 行、两种前缀并存。**

> **缺陷方向（新增，与 `ARCH-05` 的 G-12 / `ARCH-06` 的 G-15 都不同）**：G-12 是「代码搬走了、指向它的名字没搬」（**空间向**）；G-15 是「后写的把先写的坑修了、先写的没回头」（**时间向**，同族入口之间）。本条是 **「同一次改名的覆盖只做了一半，且新旧两种写法在同一份文件里并存」**——它既是搬运的结果（空间），又是局部性的体现（时间），但**判据是新的**：不是「某个入口停在旧判据」，而是 **「一次改名没有单一规则，于是每个类型各自随机落在三种组合之一」**。已记为全局待办 **G-18**。

---

## §2 已落地

### 2.1 施加流水：一条 172 行的 `Apply`，**四个阶段 + 三处重入重定位**【实测】

`FTcsBuffOps::Apply`（`TcsBuffOps.cpp:16-187`）是本模块唯一的主干。它的结构不是「步骤列表」，而是**「副作用 → 重定位」交替**：

```
① 校验：Target 有效 → Def 已登记 → CheckApplyGate（关系门·受动侧）
② 施动半：ApplyCancelAndBlock（TagsToCancel / TagsToBlock 两处时机）
   ⇒ ★ 重定位 1：重新 GetRegisteredBuffDef（施动半会移除实例并广播）
③ 决策：FindOrAddBucket → ResolveStackDecision（五轴，纯函数）
④ 分派：Reject（零副作用返回）/ Replace（走完整移除链）/ Refresh·Stack / NewInstance
   ⇒ ★ 重定位 2（Replace 支）：Remove 之后重新 FindBucket
⑤ 新建：AllocateSlot → 填字段 → BuildSnapshot → MountModifiers
   ⇒ ★ 重定位 3：MountModifiers 提交后重新 FindBucket + Find(Handle) + 重新 GetRegisteredBuffDef
⑥ 接线：ScheduleTime → WireTriggerRows → WireBehaviors → 值快照 → Broadcast(Applied)
```

**三处重定位各自有独立的理由，且注释逐处写明**（`:54-56` / `:93-94` / `:149` / `:160-161`）。这条纪律在 `ARCH-05` 的 `SPEC-02-states` §12.8 第 5 条被立为「本轮新增的一条通用纪律」：

> **任何「可能广播」的调用之后，调用方手里的实例指针与桶引用都可能失效。**

**本模块把它执行到了什么程度**——这是全仓最彻底的一处：

| 可能广播的调用 | 之后重定位了什么 | 位置 |
|---|---|---|
| `ApplyCancelAndBlock` | `Def` | `:56` |
| `Remove`（Replace 支） | `Bucket` | `:95` |
| `MountModifiers` | `Bucket` + `Instance` + `Def` | `:150-161` |
| `StripModifiers`（`Remove` 内） | `Bucket` + `Instance` | `TcsBuffOps.cpp:235-251` |
| `MountModifiers`（`RefreshStacked` 内） | `Bucket` + `Instance` + `Def` | `TcsBuffOps_Stack.cpp:271-288` |
| `ScheduleTime` 的 `Immediate` 周期事件 | `Instance` | `TcsBuffOps_Stack.cpp:297` |
| `Broadcast(StackChanged)` | `Instance` | `TcsBuffOps_Stack.cpp:310` |
| `Broadcast(Refreshed)` | `Instance` | `TcsBuffOps_Stack.cpp:318` |

**但 Replace 支漏了一个 `Def`**——见 §5 **D-1**。这是本模块最严重的一条债，且它的证据是**同函数内另外三处都做了**。

### 2.2 五轴共存决策：**决策只算不写**，且「档位由层数变没变反推」【实测】

`FTcsBuffOps::ResolveStackDecision`（`TcsBuffOps_Stack.cpp:154-236`）是一个**纯函数**：收 `(Request, Bucket, Policy)`，回 `FTcsBuffStackDecision{Kind, ExistingHandle, NewStacks}`，**不写任何实例字段**。写入在 `RefreshStacked` 里。头注释把这套纪律定为「算与写分开，读一处即全貌」（同物化器）。

决策树四步（代码顺序，不是逻辑顺序）：

| 步 | 判据 | 结果 | 位置 |
|---|---|---|---|
| ① | 组内无实例 | `NewInstance`，`NewStacks = 1` | `:182-187` |
| ② | Custom 的 `ShouldAccept` 拒绝 | `Reject` | `:194-204` |
| ③ | 满仓（`MaxStacks > 0 && Stacks >= MaxStacks`） | `Replace` 或 `Reject`（按 `Overflow`） | `:207-215` |
| ④ | 未满仓 | **层数由 Custom 或内置规则给出，档位由「层数变没变」反推** | `:219-233` |

**第④步的设计值得单独记**：它不是「先判同源/异源，再决定档位」，而是**先算目标层数、再用「层数变没变」反推是 `Refresh` 还是 `Stack`**（`:231-233`）。这样 `Fragment->ResolveStacks` 可以**直接决定这一次算不算叠层**（返回原值 = 刷新、返回 +1 = 叠层、返回 +N = 叠 N 层），而不需要策略再答一遍「你算哪档」。

**三处「解析一次」纪律**：`CustomDecision` 载荷只解析一次（`:163`），并**显式写明理由**——「若『找成员』与『出决策』各解析一次，同一处配置错误会刷两条重复 Warning」。这是一条**从症状反推出的设计约束**，不是风格。

**未声明来源 = 通配**（`:103-106`）：`!Request.Source.IsValid() || Request.Source == Existing.Source`。注释写明了不这么做的后果——「任何不关心来源的调用方在默认策略（层数无限）下**每次施加都叠一层**，一个只在长跑里才显形的错误」。

### 2.3 撤销顺序：**六段硬约束**，且每一段的位置都有单独理由【实测】

`FTcsBuffOps::Remove`（`TcsBuffOps.cpp:192-284`）的顺序是：

```
① StripModifiers(Entity, CascadeAnchor)     ← 批内提交，会重算 + 广播
② 重新定位 Bucket / Instance                ← ① 可能被重入移除
③ UnwireTriggerRows + UnwireBehaviors       ← 纯登记表操作，不广播
④ CancelTimeEntries                         ← 撤时值 + 周期两个条目
⑤ Transit(Expiring)                          ← 已 Expiring 则跳过（刷新路径）
⑥ 值快照 → 守卫 Enter → Broadcast → 守卫 Exit → 重新定位 → ReleaseSlot
```

**四段位置理由，逐条都有注释**：

- **① 排在「取实例指针」之前**：摘除的批提交会重算 + 广播（属性变更事件），订阅者可在其中重入移除本实例或注销实体 ⇒ 之后必须重新定位（`:227-232`）；
- **③ 排在广播之前**：该实例自己的触发行 MUST NOT 因自己的死亡事件起链（「状态走了，行为先走」，`:231-232`）；
- **④ 排在广播之前**：条目持句柄，槽位复用后回调仍会到达 ⇒ 先撤条目才不产生无谓回调（`:242-245`）；
- **⑥ 的释放排在广播之后**：订阅者要能在回调里用载荷句柄 `GetState` 读到实例（`:259-261`）。

**`RemovalBroadcastGuard` 的宽度是本模块的一处精细活**（`TcsBuffSubsystem.h:423-425` / `TcsBuffOps.cpp:211-215`）。它只覆盖**真正的移除广播窗口**，**MUST NOT 用 `Phase == Expiring` 当「正在移除」的判据**——因为 `RefreshStacked` 在刷新期间**也临时用 `Expiring`**（`TcsBuffOps_Stack.cpp:290-293` / `:319-322`）⇒ 按阶段判会**静默拒绝合法的 `Refreshed` 自移除**。代码里 `Remove` 的注释明确写了这句（`:211`）。

---

### 2.4 时值与周期：**两个锚点 + 三个刷新轴**，且 `Immediate` 是同步执行【实测】

`FTcsBuffInstance` 同时持 `ExpiryEntry` 与 `PeriodEntry`（`TcsBuffInstance.h:105-115`），分离理由被**逐字要求保留**：

> `Finite` 与 `Period > 0` 是**可同时成立的两个语义**（标准 DoT = 有限时值 + 周期跳伤），挤在一个锚点里会让「撤销旧条目」分不清撤的是哪一条（`ExtendDuration` 只该动时值条目，`PeriodRefresh` 只该动周期条目）。

**`ScheduleTime` 是施加与刷新共用的唯一时间调度点**（`TcsBuffOps_Lifetime.cpp:131-200`），它把三个刷新轴一次处理完：

| 轴 | 判据 | 行为 | 位置 |
|---|---|---|---|
| `StackDurationPolicy::ESD_None` | `bRefresh && 轴==None && DurationRemaining > 0` | **不覆写** `DurationRemaining`（保留剩余） | `:152-160` |
| `ESD_RefreshRemainingToTotal` | 否则 | 回满额 | `:156-160` |
| `PeriodRefresh::EBPR_Keep` | `bRefresh && Keep && PeriodRemaining > 0` | 保留周期剩余 | `:177-182` |
| `EBPR_Reset` / `EBPR_Immediate` | 否则 | 满额重建 | `:182` |

**`EBPR_Immediate` 是同步执行、不是排 0 延迟条目**（`:174-197`），注释写明理由：「『立即』的语义就是本次调用内发生，排 0 延迟会把它变成『下一泵点』」。执行后**重新定位实例**再决定是否重挂（`:191-195`）——因为周期广播可以自移除或嵌套刷新。

**周期回调的自续挂**（`PushPeriod`，`:61-119`）有一处细节值得记：回调一进来**先把 `PeriodEntry` 复位为空**（`:92`），注释写「条目已一次性失效，若下面中途 return 也不留悬空句柄」。这是对 `TcsStateInstance.h:126-129` 那条 `CancelExpiry` 陷阱的直接回应——`CancelExpiry` 对悬空句柄会 `ensureMsgf`，而到期回调跑的那一刻条目**仍然有效** ⇒ 撤销必须发生在复位锚点之后。

### 2.5 修正器物化：**值约定在物化边界转一次，层数也在物化边界乘一次**【实测】

`FTcsBuffModifierMaterializer::Materialize`（113 行）是纯函数式的——不写账本、不广播、无世界副作用。两条「边界转换」都落在它身上：

1. **值约定**（`:72-84`）：`OPK_Literal` 且来源的能力位允许 ⇒ `FTcsValueConvention::ConvertToCanonical`。判据走**虚分派读能力位**，MUST NOT 建「源类型 × 可配约定」的中心名单（`PV-10` 纪律）；
2. **层数乘值**（`:92-104`）：`ValueStack == EVS_AddValues && Stacks > 1` ⇒ `Literal *= Stacks`；`OPK_AttributeScaled` 形态则 `Coefficient *= Stacks`（账本条目仍是同一条）。

**「层数先落、数值后算」是一条无编译期保护的硬约束**：`RefreshStacked` 在调 `MountModifiers` **之前**写 `Instance.Stacks`（`TcsBuffOps_Stack.cpp:256` 先、`:268` 后），注释写「顺序有意：层数先落，数值后算」。反过来会让本次叠层的数值停在旧层数——**没有任何断言能抓**，只能靠注释与装置读数。

**快照经反射壳装进求值上下文**（`:35-42`）：`FTcsStateSnapshotScope`（RAII 栈式绑定）+ `MakeContext`（唯一装配点）。这两件都在共享层，本模块只**用**。

### 2.6 行为 Fragment：**每兴趣 Tag 一条订阅 + 计数配对**，且回调用副本【实测】

`FTcsBuffBehaviorRegistry`（166 行实现）的三张表：`TagSubscriptions`（Tag → 订阅句柄）/ `TagInstances`（Tag → 实例句柄数组）/ `InstanceInterests`（实例 → 去重兴趣数组）。

**为什么不能每实例各订一次**——理由写在 `state-behavior-fragments:57` 与本模块注释里，且是**引擎事实**：总线 `Subscribe` 只收 `UTcsEventHandler*`、派发只传 `(EventTag, Payload)` ⇒ **回调无法自辨身份**。收益为零、成本为正（订阅表随实例数膨胀、退订易漏）。

**派发侧（`TcsBuffSubsystem_Behavior.cpp:56-131`）的三层防御**，逐层有理由：

1. **`FTcsStateBehaviorCallbackScope`（`FGCObject`，`:14-41`）**：回调期间强持**片段副本**与**载荷副本**——注释写「注销 Def + 手动 GC 也不会破坏当前调用」。这不是防御性编程，是**真实可达的**：回调里可以注销定义并触发 GC；
2. **每次回调前重新解析实例与 Def**（`:99-104`）：前一回调可以移除本实例、其它实例或全清登记；
3. **`FragmentCount` 在遍历开始时固定**（`:81-86`）：回调中新添加的片段不加入当前事件。

**状态载荷按实例身份自筛**（`:69-78`）：`GetPtr<FTcsBuffEventPayload>()` 成功时只投递给 `Handle` 相等的候选；**非状态载荷（含空载荷、宿主自定义载荷）向全部存活候选扇出**——即使其读取器提供了 `Subject` 也不泛化过滤（`state-behavior-fragments:86` 的判据）。

### 2.7 关系门：**三道判定 + 施动半两处时机**，且「快速路径曾写错」被留在注释里【实测】

`CheckApplyGate`（`TcsBuffOps_Relation.cpp:62-145`）三道判定，任一不过即拒绝（零事件、实例数不变）：

| 道 | 判据 | 检查器 |
|---|---|---|
| ① | `RequiredTags` 未全满足 | `CheckRequired` |
| ② | `ForbiddenTags` 命中 | `CheckForbidden` |
| ③ | **反向查找**：存活实例的 `TagsToBlock` 覆盖我 | `CollectBlocks` |

**本模块最有价值的一段注释在 `:68-80`**：它记下了一条**曾经写错、由装置抓出、并明确禁止回到的错误形态**——

> 错误形态：只看「**我自己**四数组全空 ⇒ `return true`」。它把 ③ 反向查找一起跳过了——而反向查找问的是「**别人**是否阻断我」，与我自己的字段**无关**。装置实测：R5 时机①（目标后到）以 FAIL 报出，正是这条快速路径让 B 恒过。

这类注释（**错误形态 + 抓它的装置 + 禁止回到**）在全仓只有少数几处，是本档案认为最值得复制的文档形态——它把「为什么不能优化」写成了可验证的事实，而不是偏好。

**施动半 `ApplyCancelAndBlock`**（`:149-234`）承担两处时机：`TagsToCancel`（终止已在跑的）+ `TagsToBlock`（阻断「我已开始时」已在跑的）。两者落地动作相同（走同一条 `RemoveState` 链、原因 `EBRC_Cancelled`），注释写明**为什么合并**：「差别只在语义来源（事前拦截 vs 事后终止），而『目标后到时无法开始』那一半已由 `CheckApplyGate` 的 ③ 承担 ⇒ 两处时机在此合拢，无重复实现」。

### 2.8 Def 资产与作者期校验：**10 条错误 + 2 条警告**，其中 2 条是关系族新增【实测】

`UTcsBuffDefAsset::IsDataValid`（`TcsBuffDefAsset.cpp:100-252`）的规则全集：

| # | 类型 | 判据 | 位置 |
|---|---|---|---|
| 错 1 | 错误 | `Params` 键重复 | `:105-116` |
| 错 2 | 错误 | `ModifierRows` 空引用 | `:166-174` |
| 错 3/4 | 错误 | `Triggers` 的 `EventTag` / `EffectChainId` 无效 | `:177-194` |
| 错 5 | 错误 | 值约定白名单（能力位为假的源配了约定列） | `:121-132` |
| 错 6 | 错误 | `ForbiddenTags` 含自己的 `DefTag`（自指门禁永不可满足） | `:138-144` |
| 错 7 | 错误 | `TagsToCancel` 含自己的 `DefTag`（自取消） | `:152-158` |
| 错 8 | 错误 | `Finite` 而 `DurationTime` 来源为空 | `:218-223` |
| 错 9 | 错误 | `Fragments` 空载荷 / 类型不派生 | `:197-215` |
| 错 10 | 错误 | `Descriptions` 三规则（`DescriptionId` / `TextKey` / `SlotName`） | `:41-93` + `:226` |
| 警 1 | 警告 | `Period < 0` | `:232-236` |
| 警 2 | 警告 | `PeriodRefresh != Keep` 而 `Period == 0` | `:239-243` |
| 提升 | — | `NotValidated` → `Valid` | `:246-249` |

**两条关系族规则（错 6 / 错 7）各自带一条「MUST NOT 推广」的边界**（`:149-151`）：

> 本规则 MUST NOT 被推广成「`ForbiddenTags` 与 `TagsToCancel` 同命中即错」——那个组合是「在位者赢」的**合法**表达。

**并且它如实登记了自己判不了的那一半**（`:160-163`）：

> **跨资产的双向强夺**（A 取消 B 且 B 取消 A）在**单资产 `IsDataValid` 里判不了**……⇒ 它归**定义库的发现期**。**本变更如实登记为未落面**（MUST NOT 假装已在作者期拦下）。

⚠ 这句话是**准确的**，但**那半面今天确实没落**——见 §6 **U-3**。

### 2.9 门面反射面：**本模块 `UFUNCTION` 只有 1 个，且在宿主接口上**【实测】

`TcsBuff` 内 `UFUNCTION` 实测 **1 处**（`TcsEntityLevelProvider.h:57` 的 `BlueprintNativeEvent`），`UTcsBuffSubsystem` 本身**零 `UFUNCTION`**。对照 `ARCH-06` 的 `TcsEffect`（34 个公开方法 / 23 个 `UFUNCTION`），这是**全仓最保守的反射面**。

理由分散在三处注释里，且口径一致：`ApplyState` 的形参含 `TMap` 与未反射化句柄（`TcsBuffSubsystem.h:193`）；`GetState` 返回裸 struct 指针（`:214`）；`ForEachState` 含 `TFunctionRef`（`:236`）。⇒ **脚本面整体留 `SCRIPT` 系列台账**。

**这是一处「零反射面」的诚实登记，不是遗漏**——但它有一个直接后果：`openspec` 里没有任何能力回答「Buff 的脚本面长什么样」，因为**根本没有**。见 §6 **U-6**。

### 2.10 与共享层的接口：**九个件，全部是「用」或「向下注册」**【实测】

| 共享层件 | 本模块怎么用 | 位置 |
|---|---|---|
| `TTcsEntityRuntimeBucket` / `FTcsEntityRuntimeRegistry` | **别名绑定**（钉载荷与句柄类型），零实现 | `TcsBuffRegistry.h:41-52` |
| `FTcsStateGenerationIssuer` | 桶模板的第三参，进程级发号 | 同上（注释） |
| `TTcsEntityBroadcastGuard` | 移除广播窗口的有限重入守卫 | `TcsBuffSubsystem.h:425` |
| `FTcsEntityAdmission` | 决策 Fragment 的基类（三方法契约只一份） | `TcsBuffStackFragment.h:92` |
| `FTcsStateInstance` | 运行态基底（6 字段） | `TcsBuffInstance.h:112` |
| `FTcsStateDefBase` | Def 基底（类别词 + 关系四字段 + 等级 + 参数行 + 描述 + 修正器行） | `TcsBuffDef.h:35` |
| `FTcsStateRelation` | 关系检查器（四个静态函数） | `TcsBuffOps_Relation.cpp` |
| `FTcsStateSnapshotScope` / `UTcsStateParamTableReader` | 快照反射壳 + RAII 栈式绑定 | `TcsBuffModifierMaterializer.cpp:35` |
| `UTcsStateSubsystem` | 在飞查询契约的**注册目标**（向下注册） | `TcsBuffSubsystem.cpp:20-27` |

**注意最后一行的方向**：本模块在 `Initialize` 里**向共享层注册** `ITcsStateLiveQuery`（`:22`），在 `Deinitialize` 里**注销**（`:72`）。这是「域向下注册」而非「宿主注入」——判据写在 `TcsStateLiveQuery.h:30-33`：「这不是宿主能力，而是框架自身机制的缺失；若要求宿主接线，则**漏接的表现是关系门静默失效**」。

**一处值得记的自我约束**：本模块的 `TcsBuffAttributeAccess` 白名单薄壳**只有 5 个方法**（`IsLedgerReady` / `ApplyModifier` / `RemoveBySource` / `BeginBatch` / `Commit`），且头注释明确**收回了**一条曾经的假声明——

> **2026-10-05 订正（R5 Task 6）**：本注释原先声称允许面含 `EvaluateCurrent`，但本类**从未暴露**它、TcsState 侧也零消费者 ⇒ 措辞收回。

实测确认：`EvaluateCurrent` 在 `Source/TcsBuff` 内**零命中**（除这句注释本身）。这与 `ARCH-04` D-8（零调用即属违反）**不是同一档**——那条是「暴露了没人用」，本条是「**声明了但从没暴露**」，已自我订正。

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

**先说一个反直觉的事实**：`TcsBuff` 被**插件内其它模块**直接 include 的只有 3 个符号，且全部来自 `TcsIntegration`（唯一的上层消费者）：

| 符号 | 消费者 | 位置 |
|---|---|---|
| `FTcsBuffDef` | `TcsDefinitionSubsystem.cpp:12` / `_GC.cpp:6` | 定义库缓存的内容类型 |
| `UTcsBuffSubsystem` | `TcsDefinitionSubsystem.cpp:19` | 定义登记口 |
| `UTcsBuffDefAsset` | `TcsDefinitionSubsystem_State.cpp:8` | 按类发现 |

**这意味着一件重要的事**：`TcsSkill` / `TcsDamage` / `TcsTargeting` / `TcsEffect` **全部不 include `TcsBuff`**。`TcsBuff.Build.cs:12-23` 声明依赖 `TcsEffect` 与 `TcsState` 是**单向的**（Buff → Effect、Buff → State），反向一条都没有。这与 `PLN-REFACTOR-1` 的验收条件一致（`TcsSkill` MUST NOT 依赖 `TcsBuff`）。

**稳定契约清单**：

| 契约 | 形状 | 稳定性判据 |
|---|---|---|
| `RegisterBuffDef` / `UnregisterBuffDef` / `GetRegisteredBuffDef` / `GetRegisteredBuffDefCount` | 定义登记四口 | **有插件内真实消费者**（定义库）；且是唯一的「定义怎么进来」通道（依赖方向单向，门不反查定义库） |
| `ApplyState` | 六形参，返回 `EBuffApplyResult` | 唯一施加入口；四档全可达 |
| `GetState` / `IsStateActive` / `ForEachState` / `GetStateCount` / `GetTotalStateCount` | 查询五口 | `GetState` 按句柄做代际校验式定位；`ForEachState` 按槽位下标升序稳定序 |
| `RemoveState` / `ExpireState` / `UnregisterEntity` | 撤销三口 | 复用同一条移除链 |
| `ExtendDuration` / `SetRemaining` | 时长二口 | 同步到期堆；`Infinite` 上 Warning + 无操作 |
| `SetEntityLevelProvider` / `GetEntityLevelProvider` | 等级读口注入 | 未注入 = 配置状态不是错误 |
| `GetBehaviorSubscriptionCount` / `GetBehaviorInstanceCount` | 行为观测二口 | 订阅按 Tag 计数、不随实例数增长 |
| `GetEventBus` / `GetClockSubsystem` / `GetParamTableReader` | 内核三口 | 世界拆解期可能为 `nullptr`，调用方须容忍 |
| `ITcsEntityLevelProvider` | 宿主实现接口 | `BlueprintNativeEvent`，脚本/蓝图可实现 |
| `FTcsBuffEventPayload` + 六枚 `TcsEvent.State.*` | 事件契约 | 六枚共用一种载荷；`Source` 非反射 |
| `ITcsStateLiveQuery` 的实现（`ForEachLiveState` / `GetLiveCount`） | 向下注册 | `TcsSkill` 判关系表时读它 |

### 3.2 MUST NOT 依赖（内部实现细节）【实测+推断】

| 内部件 | 为什么不能依赖 |
|---|---|
| `FTcsBuffOps`（10 个静态函数族） | 门面实现体。头注释写「门面只做世界过滤与登记口，流程逻辑全在引擎函数里」——它是**门面的内部结构**，不是 API |
| `FTcsBuffModifierMaterializer` | 纯函数式物化器；输入是「Def + Instance」，外部拿不到 `FTcsBuffInstance` |
| `FTcsBuffAttributeAccess`（`Private/`） | 白名单薄壳，住 `Private/State/`，外部**物理不可达** |
| `FTcsBuffBehaviorRegistry` / `UTcsBuffBehaviorHandler` | 行为订阅的内部实现；Handler 由门面 `UPROPERTY` 强持有 |
| `FTcsBuffRegistry` / `FStateBucket`（别名） | 共享层模板的实例化；桶内容住 `TArray` 连续缓冲，**扩容即搬移** ⇒ 外部缓存指针即悬空 |
| `FTcsBuffInstance` 的字段 | **桶内池化纯数据**、非反射、按值住 `TArray`。外部只能经 `GetState` 短暂读，MUST NOT 跨「可能触发池扩容的调用」缓存 |
| `FTcsBuffStackRequest` / `FTcsBuffStackDecision` / `ETcsBuffStackDecisionKind` | 决策输入输出由**引擎流程装配**（头注释：「决策输入永远由引擎流程装配，不是脚本层要构造的东西」） |

### 3.3 一条**反直觉**的边界：`FTcsBuffOps` 带导出宏，但它是测试面不是契约面【实测】

`FTcsBuffOps` 带 `TCSBUFF_API`，理由写在 `TcsBuffOps.h:55-56`：

> **导出宏是必需项**（`WAIT-9` 判据："该符号是否被跨模块引用"）：宿主侧装置（`TcsDev`）直接调 `MakeContext` 等静态成员 ⇒ 不带 `TCSBUFF_API` 必 `LNK2019`（2026-10-05 实测）。

**这是一处诚实的取舍**：导出宏的判据是「有没有跨模块引用」，而引用者是**宿主装置**——判据成立、导出正确。但它造成一个**语义歧义**：一个带 `TCSBUFF_API` 的类，读者会默认它是稳定契约，而它实际是「为测试而导出的实现细节」。

**同一条歧义在四个观测口上更明显**。`GetRegisteredBuffDefCount` / `GetTotalStateCount` / `GetBehaviorSubscriptionCount` / `GetBehaviorInstanceCount` 的头注释都写「观测与装置断言用」，实测**消费者全部是 `TcsDev`**（宿主装置），插件内**零消费者**：

| 观测口 | `TcsDev` 调用点 | 插件内消费者 |
|---|---|---|
| `GetRegisteredBuffDefCount` | `TcsDevSliceRig.cpp:933` / `TcsDevSliceRig_State.cpp:204,208` | 0 |
| `GetTotalStateCount` | `TcsDevSliceRig_State.cpp:175` | 0 |
| `GetBehaviorSubscriptionCount` | `TcsDevSliceRig_Behavior.cpp:129,179,185,207,213,222,226,271` | 0 |
| `GetBehaviorInstanceCount` | `TcsDevSliceRig_Behavior.cpp:130,177,…` | 0 |

⇒ **它们不是「零消费者」，而是「只有测试消费者」**。这与 `ARCH-04` D-8（零调用即属违反）**不是同一档**：那类是「建了没人用」，本类是「建了只有测试用」——判据应当是**「测试是不是消费者」这条不算数**，还是**「测试也算消费者」**，本仓没有明文。见 §5 **D-12**。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

| 调用方 | 碰什么 | 时机 | 方向 |
|---|---|---|---|
| `UTcsDefinitionSubsystem`（`TcsIntegration`） | `RegisterBuffDef` | 世界装配期（`TcsDefinitionSubsystem.cpp:230`） | **下行**（接线层 → 域） |
| `TcsEffect` 的链解释器 | `FTcsStepApplyState` 执行器 | 链跑到该步骤时（`TcsStepApplyState.cpp:78`） | **经注册表**（Effect 不认识 Buff） |
| `TcsEffect` 的触发行求值器 | `FTcsBuffEventPayload` 读取器 | 事件命中触发行时（`TcsBuffEvents.cpp:36` 注册） | **经注册表**（同上） |
| `TcsSkill` | `ITcsStateLiveQuery`（经 `UTcsStateSubsystem` 中转） | 判关系表 `TagsToBlock` 时（`TcsSkillOps_Relation.cpp`） | **经共享层**（Skill 不认识 Buff） |
| `TcsAttribute` | 无直接调用 | 属性账本被 `ApplyModifier` / `RemoveBySource` 写 | **下行**（Buff → Attribute） |
| `TcsDev`（宿主装置） | 几乎全部公开面 | PIE 验收 | **宿主**（可依赖任何插件模块） |

**三条「经注册表/经共享层」是本模块竖切的核心**——它们让 `TcsBuff` 与 `TcsSkill` **互不认识**，同时又能互相影响。这三条边各自的机制不同：

| 边 | 机制 | 谁定义契约 | 失效表现 |
|---|---|---|---|
| Buff → Effect（步骤） | `UE_DEFINE_EFFECT_STEP_EXECUTOR` 静态自注册 | `TcsEffect` | 执行器未登记 ⇒ 门面断链（有日志） |
| Buff → Effect（载荷） | `UE_DEFINE_TRIGGER_PAYLOAD_READER` 静态自注册 | `TcsEffect` | 载荷无读取器 ⇒ 触发行不绑定主体（静默扇出） |
| Skill → Buff（在飞查询） | `UTcsStateSubsystem::SetLiveQuery` 域向下注册 | `TcsState` | 未注册 ⇒ 关系门恒过 + 具名日志（**保守侧**） |

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

这是本模块对其它模块影响**最需要记住**的一节——**六个接线点的位置都是硬约束**，且全部集中在 `Apply` 与 `Remove` 两条链上：

**施加侧（全部排在 `Applied` 广播之前）**：

| # | 接线点 | 为什么必须在这个位置 | 位置 |
|---|---|---|---|
| 1 | `BuildSnapshot` | 快照要先于修正器物化（物化读快照） | `TcsBuffOps.cpp:143` |
| 2 | `MountModifiers` | 订阅者在 `Applied` 回调里读到的属性必须已是挂载后的数值 | `:147` |
| 3 | `ScheduleTime` | 订阅者读到的是完整实例（含时值/周期） | `:170` |
| 4 | `WireTriggerRows` | **新登记的行要能看见自己的 `Applied`** | `:174` |
| 5 | `WireBehaviors` | 订阅者能在自己的 `Applied` 回调里跑行为 | `:175` |
| 6 | `Broadcast(Applied)` | 以上五步的**共同终点** | `:179` |

**撤销侧（全部排在 `Removed` / `Expired` 广播之前）**：

| # | 接线点 | 为什么必须在这个位置 | 位置 |
|---|---|---|---|
| 1 | `StripModifiers` | 订阅者读到的属性必须已归位 | `TcsBuffOps.cpp:233` |
| 2 | `UnwireTriggerRows` | **该实例自己的行 MUST NOT 因自己的死亡事件起链** | `:255` |
| 3 | `UnwireBehaviors` | 同上（行为面） | `:256` |
| 4 | `CancelTimeEntries` | 条目持句柄，先撤才不产生无谓回调 | `:258` |
| 5 | `Transit(Expiring)` | 广播时阶段 = 「移除中」 | `:262` |
| 6 | `Broadcast(Removed/Expired)` | 以上五步的**共同终点** | `:269` |

**两侧对称的一句话**：「**状态在，行为就在**」的对偶是「**状态走了，行为先走**」。这句话在 `TcsBuffOps.h:341` / `:361` 与 `TcsBuffOps.cpp:231-232` 三处重复出现，是本模块最强调的一条竖切纪律。

### 4.3 一条竖切级的教训：**「层数先落、数值后算」是无编译期保护的顺序约束**【实测】

`RefreshStacked` 里两行代码的顺序决定了叠层的数值对不对（`TcsBuffOps_Stack.cpp:256` 写层数、`:268` 调物化）：

```cpp
Instance.Stacks = Decision.NewStacks;          // ← 必须在前
// …
MountModifiers(Subsystem, Def, Instance, true); // ← 物化器读 Instance.Stacks 乘值
```

**反过来会怎样**：本次叠层的数值停留在旧层数——**属性读数错一层，且不崩溃、不报错、不进任何断言**。

**为什么没有保护**：`MountModifiers` 的形参是 `const FTcsBuffInstance&`，它**看不到**层数是新是旧；而 `Decision.NewStacks` 与 `Instance.Stacks` 是两个不同的值，编译器无法知道哪个才是「本次施加之后的层数」。

⇒ 这类「顺序即契约」的约束是本模块**唯一一类只能靠注释与装置读数守护的缺陷面**。它今天是对的（装置有对应检查），但**没有任何机制防止下一次改动打乱它**。见 §5 **D-6**。

### 4.4 一条跨模块的隐性契约：**`CascadeAnchor` 与 `Source` 的分工 MUST 守死**【实测】

这两个 `FTcsSourceHandle` 字段长得一样、类型一样、都在实例上，但**语义相反**：

| 字段 | 语义 | 谁用 | MUST NOT |
|---|---|---|---|
| `Source` | **谁施加的** | 共存决策的续杯/叠层判据、事件载荷的归属读数 | **MUST NOT 当撤销锚点** |
| `CascadeAnchor` | **级联撤销锚点** | 修正器条目、内联触发行、链挂账本条目 | **MUST NOT 与 Source 混用** |

**判据是实测出来的**：一个来源可以施加多个不同定义（一条技能给同一目标挂两个 buff）⇒ 按 `Source` 撤销会把别条实例的条目一起摘掉，而那条实例还活着 ⇒ 「**状态在，修正器没了**」的不一致态。

**代码里的执行情况**：

- `CascadeAnchor` 在建实例时恒发新号（`TcsBuffOps.cpp:132`），刷新/叠层不换（`TcsBuffOps_Stack.cpp:253-255` 明确写「两个句柄都刻意不动」）；
- 修正器物化用 `Instance.CascadeAnchor`（`TcsBuffModifierMaterializer.cpp:108`）；
- 内联触发行用 `Instance.CascadeAnchor`（`TcsBuffOps_Trigger.cpp:49`）；
- 事件载荷用 `Instance.Source`（`TcsBuffOps_Events.cpp:53`）；
- `StripModifiers` 的形参名就叫 `Source`，但注释**明确写**「= `FTcsBuffInstance::CascadeAnchor`；**不是**『施加方来源句柄』」（`TcsBuffOps.h:323`）。

⚠ **最后一处是一处命名陷阱**：`StripModifiers(Subsystem, Entity, Source)` 的形参名是 `Source`，实际收的是**锚点**。注释救回来了，但形参名与语义相反——这是全仓「`Source` 一词三义」的直接后果。见 §5 **D-7**。

### 4.5 跨模块：**本模块是「被依赖最少、但竖切影响最深」的一类**【实测】

`TcsBuff` 只有 1 个插件内消费者（`TcsIntegration`），是 11 个模块里最少的之一。但它的竖切影响面**不体现在 include 上**，而体现在：

1. **它决定了属性账本的最终数值**——`MountModifiers` 写账本、`StripModifiers` 摘账本，所有属性的运行时变化有相当一部分经它；
2. **它决定了触发行能不能起链**——`WireTriggerRows` 在 `Applied` 之前登记、`UnwireTriggerRows` 在 `Removed` 之前退订，这两处的时机错了，「buff 的行为」就整条消失；
3. **它是 `TcsSkill` 判关系表的唯一数据源**——经 `ITcsStateLiveQuery`，技能侧的 `TagsToBlock` 反向查找读的就是本模块的在册实例。

⇒ **判据**：本模块的「对外影响」不能用 include 数衡量，必须用「**它写进别处的状态**」衡量。这是 `ARCH-06` 记的那条「被依赖最广 ≠ 影响最深」的**反面**。

---

## §5 技术债（期望 vs 实际，逐条带证据）

> 判据（`ARCH-INDEX` §4）：**技术债 = 需求说了 / 设计定了，但代码做了却做偏了**。代码存在但不达标。

### D-1 ★★★ Replace 支**没有重新解析 `Def`**，而同一个函数里另外三处都做了【实测】

| 项 | 内容 |
|---|---|
| **期望** | `Apply` 在每次「可能广播的调用」之后重新取 `Def`。这条纪律在本函数内被明确执行了三处（`:56` 施动半之后、`:161` 挂载提交之后、`TcsBuffOps_Stack.cpp:284` 刷新挂载之后），注释也写了理由 |
| **实际** | **Replace 支没有**。`Remove(Subsystem, Decision.ExistingHandle, EBRC_Removed)`（`TcsBuffOps.cpp:91`）会走完整移除链——`StripModifiers`（批提交 + 广播）+ `Broadcast(Removed)`（`PublishImmediate`）**两次广播**——之后代码只重新取了 `Bucket`（`:95`），**`Def` 仍是 `:34` 取的旧指针**，并继续用它 `:135` `NewInstance->Level = Def->LevelBase`、`:143` `BuildSnapshot(..., *Def, ...)`、`:147` `MountModifiers(..., *Def, ...)` |
| **偏差证据** | ① `:34` 的 `Def` 是 `FTcsBuffDef const*`，指向 `RegisteredDefs` 里 `TUniquePtr` 持有的对象；② `UnregisterBuffDef` 是 `RegisteredDefs.Remove(DefTag)`（`TcsBuffSubsystem_Definition.cpp:36`）⇒ **`TUniquePtr` 析构、对象释放**；③ 订阅者在 `Removed` 回调里调 `UnregisterBuffDef(DefTag)` 是**公开 API、无需特权**；④ `TcsDev` 装置**已经在这么做**（`TcsDevSliceRig.cpp` 里 `UnregisterBuffDef` 有 20+ 处调用） |
| **影响面** | **Use-after-free**。读 `Def->LevelBase` / `Def->Params` / `Def->ModifierRows` 全是已释放内存——表现可能是错数值、也可能是崩溃，取决于分配器是否已复用。**这是本模块唯一的崩溃级缺陷** |
| **是否已登记** | ❌ 否（2026-10-17 审核发现；**同日已修复**，见下方「修复记录」） |

**为什么它比表面上更严重**：

1. **触发路径是常规的**：`MaxStacks = 1` + `EOP_ReplaceExisting` 是**五轴的标准组合之一**，规格里有专门 Scenario（`state-stacking-policies:109-112`「满仓替换换句柄」）；
2. **重入不是理论**：`Broadcast` 走 `PublishImmediate`（`TcsBuffOps_Events.cpp:64`），**同步派发**到订阅者，订阅者当场就能调 `UnregisterBuffDef`；
3. **同一函数里已经防了三次**：`:54-56` 的注释写「施动半会移除实例并广播 ⇒ 订阅者可重入状态操作 ⇒ `Def` 指针可能已被注销」——**这句话对 Replace 支逐字成立**，只是那一段没有执行；
4. **规格没有任何需求覆盖它**：`state-instance-lifecycle` 的「广播后身份重查」需求（`:270`）只写「广播者 MUST 用值快照，之后按句柄重查，不访问旧指针」——**它讲的是实例指针，没讲 Def 指针**。

**一处诚实说明**：本条与 `ARCH-06` 的 D-1（跨模块载荷生命周期契约缺位）**不同型**。那条是「两边都按自己的契约做对了、契约之间没对上」，本条是**纯粹的单模块内部遗漏**——同一函数内三处做了、一处没做，且理由是同一个。

**修复记录（2026-10-17，审核当天）**：

| 项 | 内容 |
|---|---|
| 改动 | `TcsBuffOps.cpp` 的 Replace 支在 `Remove(...)` 之后、取 `Bucket` 之前，插一段 `Def = Subsystem.GetRegisteredBuffDef(DefTag);` + 空判（Warning + `EBAR_Rejected`）——**+17 / −2 行** |
| 顺序理由 | 定义空判排在桶空判**之前**：`Def` 是后续新建路径的第一需求（`Def->LevelBase` 在 `:135` 就是第一条写入），若定义为空而桶还在，按桶先判会走进一条必然读悬垂指针的路 |
| 与既有三处的口径 | 逐字同款：`:56`（施动半之后）、`:161`（挂载提交之后）、`TcsBuffOps_Stack.cpp:284`（刷新挂载之后）——都是「可能广播的调用之后必须重取」 |
| 注释 | 写清了**为什么**（两次同步广播 / `UnregisterBuffDef` 是公开 API / `TUniquePtr` 析构语义 / 读的是哪三个字段），并标注了本条档案编号 |
| 验证 | `LegendAutoChessEditor Win64 Development` **Succeeded / 0 warning**；`LegendAutoChess Win64 Shipping` **Succeeded / 0 warning** |
| 未做 | **未跑 PIE 复现**。缺陷的触发需要「订阅者在 `Removed` 回调里注销该定义」，而 `TcsDev` 装置没有这样的订阅者（27 处 `UnregisterBuffDef` 全在命令体里、不在事件回调里）⇒ 今天的装置**打不到这条路径**。故本条是**静态确认 + 编译验证**，行为面置信度为【推断】 |
| 仍留的账 | 规格侧未补：`state-instance-lifecycle:270` 的「广播后身份重查」只讲**实例**指针、没讲 **Def** 指针。按 `G-32` 这条应单独记一行——**缺陷已修，但让它可被发现的判据仍不存在** |

### D-2 ★★ `state-param-snapshot:125` 与代码**对 `DurationTime` 的求值口径相反**，且代码里两处注释也互相矛盾【实测】

| 项 | 内容 |
|---|---|
| **期望** | `state-param-snapshot:125` 写：「**`Finite`**：`DurationRemaining` MUST **由快照的 `DurationTime` 求值取得**」 |
| **实际** | 代码**刻意实时求值、不回查快照**（`TcsBuffOps_Snapshot.cpp:91-96`），并 `(void)Snapshot;` 显式标记形参未用 |
| **偏差证据** | ① `TcsBuffOps.cpp:91` 的注释写「**刻意实时求值而不是回查快照**：`DurationTime` 是时值字段（不在 `Def.Params` 里）……把时值也塞进快照会引入一个『时值的参数键』——那是插件自造参数表键」；② 而**同一个函数的头注释**（`TcsBuffOps.h:488-492`）写的是相反的话：「**为什么经快照读而不是直接求值**……故先在快照里按键找，找不到才回落实时求值」；③ 规格站在头注释那边 |
| **影响面** | 行为上**今天等价**（`DurationTime` 确实不在 `Def.Params` 里，快照里找不到它 ⇒ 两条口径走同一条实时求值路径）。但**读者会得到相反的认识**：按规格或头注释去改，会以为「快照里有时值」而写出依赖它的代码 |
| **是否已登记** | ❌ 否 |

**这条的价值不在「谁对」，而在它是「同一件事三处三种说法」的完整样本**：

| 层 | 说法 | 位置 |
|---|---|---|
| 规格 | 「由快照的 `DurationTime` 求值取得」 | `state-param-snapshot:125` |
| 头注释 | 「经快照读……先在快照里按键找，找不到才回落实时求值」 | `TcsBuffOps.h:488-492` |
| 实现注释 | 「**刻意实时求值而不是回查快照**」 | `TcsBuffOps_Snapshot.cpp:91` |

⇒ 实现注释与头注释**在同一份代码里互相否定**，而规格站在错的那一边。`ARCH-05` 记过「同一件事在三处有不同说法且没有校验器能发现」（`ARCH-INDEX` §1.2）——这是该现象的又一次实证，且这次**代码自己也分裂了**。

### D-3 ★★ 六个别名符号在规格里**全部指向不存在的名字**，且错的方向相反【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格里的类型名 = 代码里的类型名 |
| **实际** | 6 组全错，`Source/` 命中 **0**（见 §1.3 的表） |
| **偏差证据** | 逐条：`EApplyResult` / `EAR_*` → `EBuffApplyResult` / `EBAR_*`（`TcsBuffEnums.h:64`）；`EDurationPolicy` / `EDP_*` → `EBuffDurationPolicy` / `EBDP_*`（`:18`）；`ETcsPeriodRefresh` / `EPR_*` → `ETcsBuffPeriodRefresh` / `EBPR_*`（`:28`）；`FTcsBuffStackPolicy` → `FStateStackPolicy`（`TcsBuffStackPolicy.h:83`）；`RegisterStateDef` 族 → `RegisterBuffDef` 族（`TcsBuffSubsystem_Definition.cpp:10/33/45`）；`GetState -> const FTcsStateInstance*` → `const FTcsBuffInstance*`（`TcsBuffSubsystem.h:219`） |
| **影响面** | 按规格写代码**编译不过**。且这 6 组分散在 5 份规格里（`state-instance-lifecycle` / `state-stacking-policies` / `state-param-snapshot` / `state-def-asset` / `state-layer-e2e-validation`），**没有任何一处能一次改完** |
| **是否已登记** | 部分：`ARCH-05` 的 D-4 记了 `RegisterStateDef` 一条，其余 5 组**未登记** |

**「方向相反」是关键**：前三个枚举**代码加了 `Buff` 前缀**，规格停在无前缀；`FTcsBuffStackPolicy` **代码保持 `State` 无前缀**，规格却加了 `Buff` 前缀。⇒ 同一次「`TcsState` 拆出 `TcsBuff`」，**两类类型按相反方向改名**，规格是第三种组合。

**最刺眼的一处**：`state-stacking-policies/spec.md` 第 5 行用新名 `EBAR_*`、第 82~86 行用旧名 `EAR_*`——**同一份文件、相隔 77 行、两种前缀并存**。

### D-4 ★★ 一次改名**没有单一规则**，于是每个类型各自落在三种组合之一【实测】

| 项 | 内容 |
|---|---|
| **期望** | 模块拆分后的改名有单一规则（要么全加 `Buff` 前缀、要么全不加），且规则写在某处 |
| **实际** | 实测三类并存：**代码加了 `Buff`、规格没加**（`EBuffApplyResult` vs `EApplyResult`）；**代码没加、规格加了**（`FStateStackPolicy` vs `FTcsBuffStackPolicy`）；**代码与规格都加**（`FTcsBuffInstanceHandle` / `FTcsBuffEventPayload` / `FTcsBuffBehaviorFragment` 等） |
| **偏差证据** | 第三类是对的（规格 7 / 4 / 3 处命中且与代码一致）；前两类各 3 组。规则本身**在 `PLN-REFACTOR-1` §12.13 的新旧名对照表里也不完整**——那张表只列了 11 个符号，不含这 6 个枚举/策略名 |
| **影响面** | 改名类缺陷**不可被工具发现**（`openspec validate` 只校验 Requirement/Scenario 结构，不校验符号名）。且它是**可重复的**：下一次模块拆分若仍无规则，会再造一批 |
| **是否已登记** | ❌ 否。已记为全局待办 **G-18** |

**判据（本档案的建议）**：类型名里的模块前缀应当按「**这个类型归哪个域**」定，而不是按「它现在住哪个模块」定。`EBuffApplyResult` 是对的（Buff 专属语义）；`FStateStackPolicy` 也是对的（它原属共享层、`TcsSkill` 也引用它的注释）。⇒ 规格侧要改的是 `FTcsBuffStackPolicy` 一个，代码侧要改的是三个枚举的规格描述。

### D-5 ★★ `state-instance-lifecycle:197` 的登记口**符号已不存在**（`ARCH-05` D-4 的承接）【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格写 `UTcsStateSubsystem` MUST 提供 `RegisterStateDef` / `UnregisterStateDef` / `GetRegisteredStateDef` |
| **实际** | 三个方法在 `TcsState` 里**全不存在**（`Source/` 命中 0）；实际实现是 `UTcsBuffSubsystem::RegisterBuffDef` / `UnregisterBuffDef` / `GetRegisteredBuffDef`（`TcsBuffSubsystem_Definition.cpp:10/33/45`），住 `TcsBuff` |
| **偏差证据** | `openspec/specs/integration-entity/spec.md:74` **也**写 `UTcsStateSubsystem::RegisterStateDef`，`:136` 又写 `GetRegisteredStateDef`——同一处错误在第二个能力里重复了一遍 |
| **影响面** | 按规格写代码**编译不过**。而 `integration-entity` 是 M6 接线层的权威规格 |
| **是否已登记** | ✅ 是：`ARCH-05` §5 **D-4**。本条**不重复登记**，只在 `TcsBuff` 侧确认「承接模块是它」 |

**为什么在本篇再列一次**：`ARCH-05` 是从 `TcsState` 侧发现「符号不在这里」，本篇是从 `TcsBuff` 侧确认「符号在这里」。**同一处债从两侧看结论一致**，这是两层审核互证的实证。

### D-6 ★★ 「层数先落、数值后算」是**无编译期保护的顺序契约**，且规格只写了结果没写顺序【实测】

| 项 | 内容 |
|---|---|
| **期望** | 叠层时数值按新层数成倍（`state-stacking-policies:127-130` 的 Scenario「累加档随层数成倍」） |
| **实际** | **结果对**。但实现依赖 `Instance.Stacks = ...`（`TcsBuffOps_Stack.cpp:256`）必须早于 `MountModifiers`（`:268`）——**这个顺序没有任何机制保护** |
| **偏差证据** | `MountModifiers` 的形参是 `const FTcsBuffInstance&`，它读 `Instance.Stacks` 但**无法知道这是新值还是旧值**；`Decision.NewStacks` 与 `Instance.Stacks` 是两个不同的值，编译器无从比较。倒过来写，属性读数错一层，**不崩溃、不报错、不进任何断言** |
| **影响面** | 中等。今天是对的（装置有对应检查），但**下一次改动随时可以打乱它而不被发现** |
| **是否已登记** | ❌ 否 |

**规格侧的缺口**：`state-stacking-policies:116-123` 的「叠层与刷新的数值与时长语义」需求**列出了六步顺序**（层数 → 数值 → 快照重建 → 修正器重挂 → 时长 → 广播），但第 2 步「数值」写的是「按 `ValueStack`……」——**它描述的是结果，不是「必须在第 1 步之后」这条约束**。⇒ 规格给了正确的序列，**没有把它写成 MUST 顺序**。

### D-7 ★ 「`Source`」一词在本模块有**三个语义**，其中一处形参名与语义相反【实测】

| 项 | 内容 |
|---|---|
| **期望** | 同一词汇在模块内单义 |
| **实际** | 三义并存：① `FTcsSourceHandle` 类型名里的「来源」；② `FTcsBuffInstance::Source` = **谁施加的**；③ `FTcsBuffOps::StripModifiers(Subsystem, Entity, Source)` 的形参 `Source` = **级联锚点 `CascadeAnchor`** |
| **偏差证据** | `TcsBuffOps.h:323` 的注释明确写「@param Source 级联锚点（= `FTcsBuffInstance::CascadeAnchor`；**不是**『施加方来源句柄』）」——**注释在救形参名** |
| **影响面** | 中等偏低。注释救回来了，但**形参名与它收到的语义相反**，且这正好落在全模块最强调的那条分工（§4.4）上——一处笔误就会把「按锚点摘」写成「按来源摘」，而那正是设计明文禁止的 |
| **是否已登记** | ❌ 否 |

### D-8 ★ `MountModifiers` 的日志打印 `Instance.Source.Id`，而它实际按 `CascadeAnchor` 摘【实测】

| 项 | 内容 |
|---|---|
| **期望** | 日志打印的是**本次操作实际使用的锚点** |
| **实际** | `TcsBuffOps_Modifier.cpp:112` 打印 `Instance.Source.Id`，而 `:101` 的摘除用的是 `Instance.CascadeAnchor` |
| **偏差证据** | 同函数内两行：`Access.RemoveBySource(Instance.Entity, Instance.CascadeAnchor);`（`:101`）与 `Instance.Source.Id`（`:112`）。**同一函数内两个字段混用** |
| **影响面** | 低（日志误导）。但在排查「摘错了别人的条目」这类问题时，日志给出的来源号与实际用的锚点号**不是同一个**——而这正是 §4.4 那条最需要读数的场景 |
| **是否已登记** | ❌ 否 |

**同类**：`TcsBuffModifierMaterializer.cpp:112` 的日志也打印 `Instance.Source.Id`，而 `:108` 装配用的是 `Instance.CascadeAnchor`。⇒ **两处日志、同一错误形态**。

---

### D-9 ★★ `state-instance-lifecycle:130` 的「与技能侧共用同一桶实现」**举错了类型**，且这一行是从 `ARCH-05` D-1 的同一根因长出来的【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格写：状态侧与技能侧的桶「是同一模板的不同实例化（`TPayload` 分别为 `FTcsStateInstance` 与施法运行态）」 |
| **实际** | 状态侧桶载荷是 **`FTcsBuffInstance`**（`TcsBuffRegistry.h:41` 的 `using FTcsBuffBucket = TTcsEntityRuntimeBucket<FTcsBuffInstance, FTcsBuffInstanceHandle>;`），**不是基底 `FTcsStateInstance`** |
| **偏差证据** | 同一份规格的**上一行**（`:56`）就写了「★ 桶载荷 MUST 绑派生结构 `FTcsBuffInstance`、MUST NOT 绑基底」——**两条需求在同一份文件里互相否定** |
| **影响面** | 中等。`:56` 是正确的（绑基底会值截断），`:130` 是旧的。读者若按 `:130` 写，会得到 §1.2 与 `TcsBuffInstance.h` 反复警告的那个值截断缺陷 |
| **是否已登记** | ❌ 否。**根因与 `ARCH-05` D-1 相同**：`ARCH-05` 记的是「7 份 state 规格整篇在讲 `TcsBuff`」——`:130` 正是那种「按旧形态写、忘了自己已改成派生」的残留 |

### D-10 ★★ `state-behavior-fragments:23` 的上下文门面**写错了模块**（`ARCH-05` D-5 的承接）【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格写：`FTcsBuffBehaviorContext` 的成员是 `UTcsStateSubsystem* Subsystem` |
| **实际** | 代码是 `UTcsBuffSubsystem* Subsystem = nullptr;`（`TcsBuffBehaviorFragment.h:29`） |
| **偏差证据** | 同文件 `:23` 的注释本身就在讲「上下文**不持实例指针**」，而它持有的**门面**类型错了。`state-behavior-fragments:39` 又正确写了 `"/Script/TcsBuff.TcsBuffBehaviorFragment"` ⇒ **同一份规格里两处模块归属不一致** |
| **影响面** | 中等——按规格实现会 include 错模块 |
| **是否已登记** | ✅ 是：`ARCH-05` §5 **D-5**。本条不重复登记 |

### D-11 ★★ `state-def-asset:17-19` 的三处形状描述**全部是旧形态**，且其中一处指向的 `BaseStruct` 是**不存在的类型**【实测】

| 项 | 内容 |
|---|---|
| **期望** | 规格里的 Def 形状 = 代码里的 `FTcsBuffDef` |
| **实际** | 三处偏差：① `EDurationPolicy DurationPolicy = EDP_Finite`（真名 `EBuffDurationPolicy` / `EBDP_Finite`）；② `FTcsBuffStackPolicy StackPolicy`（真名 `FStateStackPolicy`）；③ 行为 Fragment 的 `BaseStruct` 写 `"/Script/TcsState.TcsStateBehaviorFragment"`（**全仓不存在这个类型**，真值是 `"/Script/TcsBuff.TcsBuffBehaviorFragment"`，见 `TcsBuffDef.h:90`） |
| **偏差证据** | ③ 是最危险的一条：`TcsBuffDef.h:90` 的 `meta = (BaseStruct = "/Script/TcsBuff.TcsBuffBehaviorFragment")`，而 `TcsState` 模块**根本没有** `TcsStateBehaviorFragment` 这个类（`Source/TcsState/` 内零命中）。**`openspec validate` 不校验字符串元数据** ⇒ 工具永远查不出 |
| **影响面** | ③ 会让编辑器类型选择器**列不出任何类型**（`BaseStruct` 指向不存在的路径），且**编译不报错**。① ② 会让按规格写的代码编译不过 |
| **是否已登记** | 部分：`ARCH-05` §5 **D-6** 记了 ③；①②**未登记** |

### D-12 ★ 四个「只有测试消费者」的观测口，判据未定【实测】

| 项 | 内容 |
|---|---|
| **期望** | 本仓的「零消费者不预建」纪律（`ARCH-INDEX` §5.1 第 3 条）能给出确定结论 |
| **实际** | `GetRegisteredBuffDefCount` / `GetTotalStateCount` / `GetBehaviorSubscriptionCount` / `GetBehaviorInstanceCount` 四个口，**插件内零消费者、`TcsDev` 装置是唯一消费者**（调用点见 §3.3） |
| **偏差证据** | 四口的头注释都写「观测与装置断言用」——**作者明确知道消费者只有装置**。⇒ 这不是遗漏，是**有意的** |
| **影响面** | 低（它们各自一行 return，成本为零）。但它暴露了**判据缺口**：`ARCH-04` D-8 把「零调用」判为违反，而本模块这四个口「有调用、只是调用者全是测试」——**这一档没有判据** |
| **是否已登记** | ❌ 否。已记为全局待办 **G-19** |

**为什么值得单列**：它是「零消费者不预建」与「可观测性需要读数」两条纪律的**真实冲突点**。若不裁清，下一轮会出现两种相反的处理：一类人删掉它们（按零消费者纪律），另一类人继续加（按可观测性需求）。而**装置断言失去读数 = 验收失去证据**。

### 债务小结

| 级别 | 条数 | 编号 |
|---|---|---|
| ★★★（崩溃级） | 1 | D-1 |
| ★★（契约级） | 7 | D-2 / D-3 / D-4 / D-5 / D-6 / D-9 / D-10 / D-11（其中 D-5 / D-10 / D-11③ 是承接） |
| ★（读数级） | 4 | D-7 / D-8 / D-11①② / D-12 |

**方向分布**：

| 方向 | 条数 | 编号 |
|---|---|---|
| **纯代码缺陷**（规格对、代码错） | 3 | D-1 / D-6 / D-7 / D-8（D-8 是日志，算半条） |
| **纯规格缺陷**（代码对、规格错） | 5 | D-2（部分）/ D-3 / D-4 / D-9 / D-11 |
| **规格内部自相矛盾** | 2 | D-2（头注释与实现注释对立）/ D-9（`:56` 与 `:130` 对立） |
| **承接他篇** | 3 | D-5 / D-10 / D-11③ |

⇒ **与前三篇的对照**：

| 篇 | 主方向 | 条数 |
|---|---|---|
| `ARCH-04` 属性 | 规格更严、代码更松 | 4 |
| `ARCH-05` 状态 | 代码搬走、引用滞留（空间） | 4 |
| `ARCH-06` 效果 | 规格与代码各错一半 | 5 + 4 |
| **`ARCH-07` Buff** | **纯代码 1 条 + 纯规格 5 条，比例最悬殊的一篇** | **1 + 5** |

**本模块只有一条真·代码缺陷（D-1），而它是崩溃级的**；其余全部是「代码对、文档错」或「文档自相矛盾」。这是全仓第一次出现这种极端分布——原因是 `TcsBuff` 是 `PLN-REFACTOR-1` 拆分出的新模块，**代码是重写的、规格是抄的**。

---

## §6 未实现（设计是否还成立 + 前置条件）

> 判据（`ARCH-INDEX` §4）：**未实现 = 代码根本不存在**。每条 MUST 写：设计是否还成立、当初依据今天是否仍有效、前置条件。

### U-1 ★★ `SetLevel` / `GetLevel`——规格明文列出，**代码从未存在**【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `SPEC-02-states` §4「入口服务」列 `SetLevel / GetLevel`（Custom 增长模式的项目入口）；`state-instance-lifecycle` **未收录** |
| **实测** | `TcsBuff` 内 `SetLevel` **零命中**；`GetLevel` 只作为 `TcsSkill` 的方法存在（`TcsSkillSubsystem.h:466` 等），与 Buff 无关。工作树 0 + 历史 0 |
| **设计是否还成立** | **部分成立但定位已变**。§3.5 明文写「**增长/修改策略完全交给项目业务侧**（引擎零内置——`FollowStacks` 枚举撤回）」⇒ 本就不该有引擎侧的 `SetLevel`。而 `Instance.Level` 今天的唯一写入点是 `Def->LevelBase`（`TcsBuffOps.cpp:135` / `TcsBuffOps_Stack.cpp:257`），**没有任何运行时改等级的入口** |
| **当初依据今天是否仍有效** | 「引擎零内置增长策略」仍有效且被 `state-def-asset` 的「基类 MUST NOT 预建字段」精神支持。⇒ **这一条应当记为「设计已撤回」，不是「欠实现」** |
| **前置条件** | 若将来需要宿主侧改等级：需要一条**明确的入口契约**（是 `SetLevel(Handle, NewLevel)` 还是「重新施加带 Override」？后者会重建快照，语义完全不同）。**今天零消费者**，按「零消费者不预建」不动 |

**⚠ 一条需要澄清的语义冲突**：`state-param-snapshot` 的「等级来源与实体等级读口」需求说等级源读 `Context.EffectiveLevel`，而 `TcsBuffOps::MakeContext` 把它设为 `Def->LevelBase`（`:33` 注释「本轮 = `Def.LevelBase`」）。⇒ **`MaxLevel` 字段全仓零消费者**（实测：`Source/` 内只有声明处 `TcsStateDefBase.h:155` 与注释引用）。这是 U-1 的连带面。

### U-2 ★★ `EVS_PerStackValue`（按层取值）——**枚举有值、零行为**，且规格明文承认【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `state-stacking-policies:20` / `:26`：`EVS_PerStackValue = 2`「**本轮只落形状**，零行为」；Scenario `:43-46`「按层取值档零行为」 |
| **实测** | 完全一致。`TcsBuffModifierMaterializer.cpp:92` 只判 `EVS_AddValues`，`PerStackValue` 落 else 分支 ⇒ 与 `KeepMax` 同行为 |
| **设计是否还成立** | **成立但前提未到**。`TcsBuffStackPolicy.h:45` 的注释写清了合规实现的两条路：「需『层数读口 + 按层参数源』，否则会变成设计明文撤回的『等级跟随层数』」 |
| **当初依据今天是否仍有效** | 有效。这是**「落形状不落行为」的诚实登记**，不是缺陷 |
| **前置条件** | 需要两个新机制：① 参数源能读「当前层数」（`FTcsParamSource_StackCount` 一类）；② 参数行能按层取不同值。**台账 `STAT-6` 已登记，裁决 = 不补** |

⇒ **本条不是缺陷**，列在此处是为了**确认规格的自我登记是准确的**。这是本档案少见的「规格说没做、代码确实没做、且理由仍在」的干净样本。

### U-3 ★★ 关系表的**跨资产双向强夺**校验——代码注释承认未落，规格也没有【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `state-relation-check:98-102` 明确要求：「① `A.TagsToCancel` 与 `B.TagsToCancel` **互相包含** ⇒ 双向强夺、无确定赢家 ⇒ **Error**」，并有 Scenario `:104-107`「双向强夺被报错」 |
| **实测** | **代码没有**。`TcsBuffDefAsset.cpp:160-163` 的注释明确写：「**跨资产的双向强夺**……在**单资产 `IsDataValid` 里判不了**……⇒ 它归**定义库的发现期**（`UTcsDefinitionSubsystem`，能看到全部已发现定义）。**本变更如实登记为未落面**」 |
| **偏差证据** | `Source/TcsIntegration/` 内 `TagsToCancel` **零命中**（实测）⇒ 定义库侧也没有这段校验。⇒ **规格要求的这条错误，全仓没有任何实现** |
| **设计是否还成立** | **成立**。判据（`:100`）「两条声明各自合法，但同时成立时结果依赖施加顺序、作者无法预测」今天仍准确 |
| **当初依据今天是否仍有效** | 有效，且**这条比单资产的两条（自指 / 自取消）更有价值**——它防的是「两个资产各自合法、组合起来无解」这类作者无法自查的错误 |
| **前置条件** | 需要 `UTcsDefinitionSubsystem` 在发现期做一次 O(n²) 的两两比对（`n` = 已发现状态定义数）。**前置条件已满足**：定义库已有第四发现路径与缓存（`TcsDefinitionSubsystem_State.cpp`），只差这段校验 |

**这是本篇唯一一条「规格要求、代码承认没做、且前置条件已满足」的未实现**——它与 `ARCH-06` 的 U-2（步骤协议没有失败档）同型：规格写了，实现绕开了，且**绕开的理由写在自己文件的注释里**。

### U-4 ★★ 行为 Fragment 的**刷新路径重订阅**未实现——但规格明确要求不重订阅【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `state-behavior-fragments:60`：「刷新 / 叠层路径 **MUST NOT 重订阅**（同一实例、订阅已在册）」 |
| **实测** | 一致。`RefreshStacked`（`TcsBuffOps_Stack.cpp:240-330`）**没有调 `WireBehaviors` / `UnwireBehaviors`** |
| **设计是否还成立** | **成立，且有一条未写的推论**：刷新路径**改了 `Def` 的内容**（如果定义在两次施加之间被注销重登，`Fragments` 会不同）——此时订阅与 Def **不一致**，而代码不修 |
| **当初依据今天是否仍有效** | 判据「同一实例、订阅已在册」成立。但**它默认「Def 不变」**，而 `UnregisterBuffDef` + `RegisterBuffDef` 是公开 API，重登后 `Fragments` 可以完全不同 |
| **前置条件** | 若要修：刷新路径需要「比较新旧 Def 的 `Fragments` 是否相同」，不同则 `UnwireBehaviors` + `WireBehaviors`。**但 `RefreshStacked` 拿不到旧 Def**（它在 `Apply` 里被重新解析，旧的那份没有留存）⇒ 需要额外保存一份 |

**⚠ 这条比看起来严重**：`UnregisterBuffDef` 的注释写「已建实例持有定义副本的身份（`DefTag`）而非指针，注销只影响『新施加能否解析到定义』」（`TcsBuffSubsystem.h:146-147`）——**这句话对行为订阅不成立**：行为订阅在 `AddInstance` 时**已经读走了 `Fragments` 的兴趣列表**并写进 `TagInstances`，注销后它**继续按旧兴趣派发**，而派发时 `GetRegisteredBuffDef` 返回 `nullptr` ⇒ `FragmentCount = 0` ⇒ **回调静默不发生**。

⇒ **实际行为**：注销 + 重登后，旧实例的行为订阅**还在（订阅数不减）**，但**永不触发**（Def 解析不到）。这是一个「订阅泄漏 + 静默失效」的组合。见 §6 **U-5**。

### U-5 ★★ 定义注销时**不清理该定义的在册实例的行为订阅**——订阅泄漏【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `state-instance-lifecycle:146-147`：「**有该定义的存活实例时不代为撤销**：已建实例持有定义副本的身份（`DefTag`）而非指针，注销只影响『新施加能否解析到定义』」 |
| **实测** | 前半句成立（实例不动）；**后半句对行为订阅不成立**。`UnregisterBuffDef`（`TcsBuffSubsystem_Definition.cpp:33-43`）只做 `RegisteredDefs.Remove(DefTag)`，**不碰 `BehaviorRegistry`** |
| **偏差证据** | ① `DispatchBehaviorEvent` 的 `FragmentCount` 取自 `Def ? Def->Fragments.Num() : 0`（`TcsBuffSubsystem_Behavior.cpp:84-86`）⇒ 定义没了 ⇒ 0 ⇒ **回调不发生**；② 但 `TagInstances` / `InstanceInterests` / `TagSubscriptions` 里该实例的登记**仍在**（只有 `RemoveInstance` 才摘）；③ ⇒ `GetBehaviorSubscriptionCount()` 读数**偏高**，且该 Tag 的订阅**永不会被退订**（直到最后一个实例被移除） |
| **影响面** | 中等。订阅表泄漏一条（每 Tag 一条，不随实例数增长），且**行为静默失效**。`TcsDev` 装置里 `UnregisterBuffDef` 有 20+ 处调用，其中部分会留下这种状态 |
| **设计是否还成立** | 「注销不代为撤销实例」成立（实例是值语义、按 `DefTag` 记身份）；但**「注销不影响行为」这句推论是错的**——行为订阅的兴趣列表是**注销前读走的快照** |
| **当初依据今天是否仍有效** | 该句写于 2026-10-04（R5 Task 2），当时**行为 Fragment 尚未落地**（Task 6b，2026-10-05）⇒ 这句话是在行为面还不存在时写的，**从未被行为面复核过** |
| **前置条件** | 修法二选一：① `UnregisterBuffDef` 时遍历该实体桶、对每个用该 `DefTag` 的实例调 `RemoveInstance`（但那样「重登后自动恢复」就没了）；② 在 `DispatchBehaviorEvent` 里把「Def 解析不到」当**正常跳过**并同步摘除该实例的登记（自愈）。**② 更符合现有架构**（派发侧已有重解析纪律） |

**这是本篇最有价值的一条未实现**——它的判据是**时序性的**：一句话写在行为面还不存在的时候，之后没有任何一轮回头复核它。这与 `ARCH-06` 的 G-15（后写的把先写的坑修了）**方向相同但载体不同**：G-15 是「同一族入口之间」，本条是「**同一句规格在它描述的对象出现之后没有被重新读一遍**」。

### U-6 ★★ Buff 域的**脚本面整体为零**——规格没有任何能力回答它【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | 无。8 份 `state-*` 能力**没有任何一处**描述 Buff 的脚本/蓝图可达面 |
| **实测** | `UTcsBuffSubsystem` **零 `UFUNCTION`**（§2.9）；唯一的 `UFUNCTION` 是 `ITcsEntityLevelProvider::GetEntityLevel`（宿主实现，不是给脚本调 Buff 用） |
| **设计是否还成立** | **成立且是有意的**。三处注释逐条给了理由（`TMap` 形参 / 裸 struct 指针 / `TFunctionRef`），并统一说「脚本面整体留 `SCRIPT` 系列台账」 |
| **当初依据今天是否仍有效** | 三条技术理由**全部仍然成立**（UHT 的限制没变）。⇒ **本条的缺口不是「没做」，而是「没人写下来」** |
| **前置条件** | 若要做脚本面：需要先解决「裸 struct 指针不可反射」——即要么让 `GetState` 返回 `USTRUCT` 副本（值语义，但会脱离实例实时态），要么加一层 `UFUNCTION` 包装返回逐个字段。**两者都是设计决策，不是实现细节** |

**这与 `ARCH-06` 的 G-17（有消费者却零收录的门面方法）方向相反**：那边是「有真实消费者的口没写进规格」，这边是「**整片不存在的面也没写进规格**」。⇒ 规格倾向于记录「做了的」，对「决定不做的」没有位置。已记为全局待办 **G-20**。

### U-7 ☆ `FTcsBuffDefTableRow`（编辑期表行）**全仓零消费者**——双轨制的「表轨」从未接通【实测】

| 项 | 内容 |
|---|---|
| **规格声称** | `state-def-asset:121-137`：`FTcsBuffDefTableRow : FTableRowBase`（双轨制的表行轨），「表 = 编辑期载体、资产 = 运行期载体」，两轨一致性由编辑器侧同步器维护（R8 `TOOLS-3`） |
| **实测** | `Source/` 内 `FTcsBuffDefTableRow` **只有声明处一个命中**（`TcsBuffDefTableRow.h:32`），**零使用、零实例、零 DataTable 引用** |
| **设计是否还成立** | **形状成立，机制未建**。规格自己写明同步器归 R8 `TOOLS-3` ⇒ 表轨今天只有「形状」，没有「编辑器同步」 |
| **当初依据今天是否仍有效** | 「双轨制」的理由（策划批量编辑 vs 运行期解析）仍成立。但**零消费者已经持续了自 Task 1（2026-10-04）至今** |
| **前置条件** | 需要 R8 的编辑器同步器（资产 ↔ 表行双向同步）。在那之前，表行是一个**只能手填、不会生效**的字段集 |

**⚠ 与 `ARCH-04` D-8（零调用即属违反）的判据冲突**：表行是**规格明文承诺的载体**（`state-def-asset` 有专门一条需求 + 3 个 Scenario），按「契约默认」判据（`ARCH-INDEX` G-13）应当保留；但它的消费者（同步器）**整体未建** ⇒ 保留多久、是否需要标注「未接通」，本仓无判据。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 载体 | 本模块的份量 | 状态 |
|---|---|---|---|
| **可执行需求** | 8 份 `state-*` 能力（1 361 行 / 42 需求 / 157 场景） | 全仓第二大（仅次于 `effect-*` 的 1 108 行 / 7 份） | **主语正确**（本模块史上第一次），**符号名 6 组全错** |
| **实际实现** | `Source/TcsBuff/`（47 文件 / 5 839 行） | 11 个模块第 4 大 | 唯一真·代码缺陷是 D-1（崩溃级） |
| **历史理由** | `Documents/combat-system-design/spec/03-module-states.md`（390 行，`SPEC-02-states`）+ `decisions/` + `log/` | 本模块**没有独立规格**——全部混在 `SPEC-02-states` 里 | 已冻结；**`SPEC-02-states` §12.13 的收束回写反而比 `openspec` 更准**（见 §7.2） |

### 7.2 `SPEC-02-states` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

`SPEC-02-states` 是本模块唯一的旧文档来源，且它有 **§12.13 的 `PLN-REFACTOR-1` 收束回写**——这是旧文档族里**质量最高的一节**。逐项对照：

| # | `SPEC-02-states` 的位置 | 它说 | 代码实际 | 判定 |
|---|---|---|---|---|
| 1 | §12.13 一、模块归属表（11 行） | 逐项列出 `TcsState` → `TcsBuff` 的搬迁 | **逐项准确**（`FTcsBuffDef` / `UTcsBuffDefAsset` / `FTcsBuffDefTableRow` / `FTcsBuffInstance` / `FTcsBuffRegistry` / `FTcsBuffOps` / `FTcsBuffStackPolicy` / `FTcsBuffOps_Lifetime` / `FTcsBuffBehavior*` / `FTcsBuffModifierMaterializer` / `FTcsBuffAttributeAccess` / `TcsStepApplyState` / 等级契约 / 六枚事件） | ✅ **自纠准确**（唯一一处：`FTcsStateStackDecisionFragment` → `FTcsBuffStackFragment` 的**类名**在代码里是 `FTcsBuffStackDecisionFragment`，文件名才是 `TcsBuffStackFragment.h`） |
| 2 | §12.13 二、依赖方向表 | `TcsBuff` = Core, Notation, Attribute, **Effect**, **State** | `TcsBuff.Build.cs` 实测 = Core, CoreUObject, Engine, GameplayTags, TcsCore, TcsNotation, TcsAttribute, TcsEffect, TcsState, **TcsAssetButler** | ⚠ **漏了 `TcsAssetButler`**——它在 2026-10-15 Task 6 才补（`TcsBuff.Build.cs:28-30` 的注释记了原因：`GetAssetsByClass` 静默返 0，`State.Run` 由 26/0 退为 6/9） |
| 3 | §12.13 三、主线读数 | `State.Run` **15/0**、`State.Reject` **7/0**、`Slice.Run` **65/0** | 无法在本轮复核（未跑 PIE） | **【待核】** |
| 4 | §3.1 实例字段表（就地注记） | 基底 6 + Buff 8，且「桶载荷 MUST 绑派生」 | ✅ 完全准确（`TcsBuffInstance.h` 的字段与注释逐条对上） | ✅ 准确 |
| 5 | §3.2 五轴堆叠（就地注记） | 五轴 + 层模型 + 两处裁档 | ✅ 准确（`TcsBuffStackPolicy.h` 的五个枚举与注释逐条对上） | ✅ 准确 |
| 6 | §3.4 关系表（两笔就地注记） | 四字段改名 + `TagsToBlock` 新增 + 「填法即策略」 | ✅ 准确，且**比 `openspec` 更完整**（`SPEC-02-states` 记了「Lyra 外部对照佐证」，`openspec` 没记） | ✅ 准确 |
| 7 | §4 入口服务 | `SetLevel / GetLevel`（Custom 增长入口） | **不存在** | ⚠ 见 §6 **U-1**（设计已撤回，非欠实现） |
| 8 | §5 Apply 流程 | 「校验 → 关系表检查 → 五轴共存决策 → 池分配 → 快照 → 行为订阅 → 触发行 → 挂 Duration/Period → 修正器物化 → `OnStateApplied`」 | 代码顺序**更细**：关系门分两段（受动 `CheckApplyGate` + 施动 `ApplyCancelAndBlock`），且**修正器物化在快照之后、时间条目之前** | ⚠ 旧文档把「修正器物化」排在最后，代码排在 `ScheduleTime` 之前 |
| 9 | §5 Remove 流程 | 「原因标记 → M2 `RemoveBySource` → 行为退订 → 周期惰性摘除 → 关系表级联重评 → `OnStateRemoved`」 | 代码：`StripModifiers` → **触发行退订** → 行为退订 → 撤时值+周期 → `Expiring` → 广播 → 归还槽位 | ⚠ 两处差异：① 旧文档**没提触发行退订**（它归 Task 6 后补）；② 旧文档有「关系表级联重评」，代码**没有**（见 `ARCH-05` U-3） |
| 10 | §5 Phase 转移矩阵 | 「阶段×允许迁移 = 数据表声明（可查不可改）」 | 代码是 `if` 链（`TcsBuffSubsystem.cpp:288-292`），四条合法边 | ⚠ **「数据表声明」未落**——代码注释自己写了「本轮矩阵以代码表达（相位仍只有三态，落表属于关系表族那一批的形态统一）」 |
| 11 | §5 Fragment 机制 | 「策略 = USTRUCT 反射基类 + C++ 虚函数分派……**两种深度**：决策 Fragment / 行为 Fragment」 | ✅ 两者都落了；但**决策 Fragment 已上提共享层**（`FTcsEntityAdmission`），旧文档 §12.13 记了 | ✅ 准确（含 §12.13 的改述） |
| 12 | §6 依赖方向声明 | 「`TcsEffect` 不依赖 `TcsState`」+ §12.13 的三模块依赖表 | `TcsEffect.Build.cs` 实测无 `TcsState`；`TcsBuff` 依赖两者 | ✅ 准确 |
| 13 | §7 网络姿态 | 「操作复制：状态操作流（`Apply{DefTag,Source,ParamSnapshot}` 等）复制到客户端镜像桶重放」 | **零实现**（`WAIT-3` / R7） | ⚠ 未实现，旧文档已标「未开工」 |
| 14 | §12.10 第 15 条 `STAT-5` 有限重入守卫 | 「**MUST NOT** 用 `Phase == ESP_Expiring` 当『正在移除』的判据」 | ✅ 完全兑现（`TcsBuffOps.cpp:211` 的注释逐字重述了这条） | ✅ 准确，且**代码里也有同样的注释** |

### 7.3 差异的**类型分布**

| 类型 | 条数 | 编号 | 判据 |
|---|---|---|---|
| **自纠准确**（旧文档改了、与代码逐条对上） | **6** | 1 / 4 / 5 / 6 / 11 / 14 | `SPEC-02-states` §12.13 的收束回写质量极高 |
| **自纠遗漏**（旧文档改了但漏一项） | 1 | 2（漏 `TcsAssetButler`） | 与 `ARCH-06` G-16 同族：**收束回写也会漏** |
| **未落但如实标注** | 2 | 13（网络）/ 10（阶段矩阵表） | 旧文档自己写了「未落」，**不是缺陷** |
| **设计已撤回** | 1 | 7（`SetLevel`/`GetLevel`） | 旧文档 §3.5 明文撤回，§4 却仍列 ⇒ **同一份文档内不一致** |
| **流程描述偏差** | 2 | 8（修正器物化位置）/ 9（触发行退订缺失 + 级联重评不存在） | 实施顺序在落地后变了，旧文档停在设计轮 |

**关键判据**：`SPEC-02-states` **不是「过期文档」，而是「一份自纠得很好的文档 + 几处没有回头的设计轮描述」**。这与 `ARCH-06` 对 `SPEC-03-effects` 的判定（「局部自纠 + 局部从不回写」）**完全同型**。

⇒ **`ARCH-06` 与 `ARCH-07` 的对照给出了一个可推广的结论**：旧文档族的 §12 类「收束回写」段落**质量普遍很高**（因为它们是实施后写的），而**正文段落停在设计轮**。读者的正确读法 = **先读 §12，再读正文，两者冲突时以 §12 为准**。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

| 步 | 动作 | 覆盖 |
|---|---|---|
| 1 | 读可执行需求 | 8 份 `state-*` 能力全文（1 361 行） |
| 2 | 读代码 | `Source/TcsBuff/` **全部 47 个 `.h`/`.cpp`**（5 839 行）+ `TcsBuff.Build.cs`（33 行）逐个通读；另读 `TcsState` 的 9 个共享层件、`TcsCore` 的 `TcsExpiryHeap` / `TcsSourceHandle` |
| 3 | 读旧规格 | `SPEC-02-states` 全文 390 行（含 §12.13 收束回写） |
| 4 | 追竖切 | 全仓 `grep` 本模块符号在其它模块的使用点；逐条核对 `TcsDev` 的调用面 |
| 5 | 交叉验证 | 每条结论标置信度；「未实现」项逐条反查是否搬到别处（`U-1` 的 `GetLevel` 反查到 `TcsSkill`） |

### 8.2 本次审核用到的**四类引用名判据**（`ARCH-05` G-12 的实证应用）

`ARCH-05` 立的 G-12 说：「凡经历过拆分的模块，审核 MUST 逐项核对『规格里的符号名是否还指向它』」。本篇把这句话展开成**四类逐个查**：

| 类 | 查法 | 本篇命中 |
|---|---|---|
| **符号名** | 把规格里的每个类型名拿去 `Source/` grep | **6 组全错**（D-3） |
| **模块名** | 把规格里的 `TcsState` / `TcsBuff` 提及逐个看语境 | **2 处错**（D-9 的桶载荷类型 / D-10 的上下文门面） |
| **类型名** | `BaseStruct` 之类的字符串元数据逐个解析 | **1 处错**（D-11③，指向不存在的 `TcsStateBehaviorFragment`） |
| **字段位置** | 字段住在哪个 struct | **1 处错**（`ARCH-05` D-7：关系四字段已上移基类，规格仍写在 `FTcsBuffDef` 下） |

⇒ **四类全部命中，共 10 处**。这证实 G-12 的判据是**必要且充分**的——一个经历过拆分的模块，四类引用**每一类都会滞留**。

### 8.3 置信度分布

| 置信度 | 条数 | 典型 |
|---|---|---|
| **【实测】** | **21 / 21**（D-1..D-12 / U-1..U-7 全部） | 每条都有 `路径:行号` 或 `grep` 零命中证据 |
| 【推断】 | 0 | — |
| 【待核】 | 1（§7.2 第 3 行的 PIE 读数） | 未跑 PIE |

**本篇是六篇里实测率最高的一篇**（100%），原因是：本模块的缺陷**几乎全部是「名字对不对」**，而这类可以 grep 直接证伪，不需要推断。

### 8.4 一次**自我纠错**（记录在此，因为它是方法论的实证）

初稿的 §2.10 与 §3.1 里，我写了「`TcsBuff` 被外部模块直接 include 的只有 4 个符号」，并把 `FTcsStepApplyState` 算进去。复核时发现：

- `FTcsStepApplyState` **没有被任何模块 include**——它经 `UScriptStruct` 路径由资产引用（`effect-step-dispatch` 的注册制分派）；
- 因此 `TcsBuff` 的**插件内 include 面只有 3 个符号**，且全部来自 `TcsIntegration`。

⇒ 已改正。**这条纠错本身值得记**：`ARCH-INDEX` §5 第 5 步要求「凡『未实现』项必须反查是否只是在别的模块里实现」——本次是**反向应用**：凡「有依赖」项必须反查**是不是真的 include**。注册制分派让「类型被使用」与「头被 include」解耦，这是 `ARCH-06` 已记过的同一现象的第二次踩坑。

---

## §9 结论

### 9.1 三句话

1. **`TcsBuff` 是全仓「代码质量最高、文档债最多」的模块**——12 条债里只有 **1 条真·代码缺陷**（D-1，崩溃级），其余全是规格符号名、规格自相矛盾、规格承接；
2. **它是 `ARCH-05` 那个「权威层主语错了」的完全解药**——8 份能力的 `TcsBuff` / `TcsState` 提及比全部指向本模块，`openspec` 第一次有了回答「`TcsBuff` 是什么」的能力；
3. **但它把那个问题的另一半暴露得更彻底**——主语对了、名字全错了，而且**错的方向相反**，说明「模块拆分后的改名」在本仓**没有单一规则**（D-4 / G-18）。

### 9.2 一句值得记住的话

> **`TcsBuff` 的规格是「用正确的模块名讲错误类型名的故事」；而它唯一一条真缺陷，是同函数里另外三处都防了、只有一处没防的 use-after-free。**

### 9.3 与前四篇的对照

| 篇 | 模块 | 债数 | 主方向 | 一句话 |
|---|---|---|---|---|
| `ARCH-04` | `TcsAttribute` | 11 | 规格更严、代码更松 | 规格是 MUST，代码全没做 |
| `ARCH-05` | `TcsState` | 12 | 代码搬走、引用滞留 | 权威层主语错了 |
| `ARCH-06` | `TcsEffect` | 12 | 规格与代码各错一半 | 后写的把先写的坑修了 |
| **`ARCH-07`** | **`TcsBuff`** | **12** | **纯规格 5 + 纯代码 1** | **主语对了、名字全错** |

**四篇的债数都是 11~12 条**——这个巧合值得记：它说明**每个模块的债务密度大致恒定**，而不是「某个模块特别烂」。⇒ 后续四篇（`TcsSkill` 11 052 行、`TcsDamage` 3 225、`TcsTargeting` 1 144、`TcsIntegration` 1 536）应当预期**同样是 10~15 条**，其中 `TcsSkill` 因为体量最大，可能更多。

### 9.4 给下一轮的三条建议（按代价从低到高）

| # | 建议 | 代价 | 收益 |
|---|---|---|---|
| 1 | **先修 D-1**（Replace 支重新解析 `Def`） | **3 行代码**（照抄 `:161` 那三行） | 消掉本模块唯一的崩溃级缺陷 |
| 2 | **给「模块拆分改名」立一条规则**（G-18） | 一次裁定 + 一轮全库回改 | 消掉 D-3 / D-4 这一整类，且防止下一次拆分再造一批 |
| 3 | **把「测试算不算消费者」裁清**（G-19） | 一次裁定 | 消掉 D-12，并让「零消费者不预建」这条纪律有确定边界 |

**关于 D-1 的修法**（本档案的建议，供裁决）：

```cpp
// 在 TcsBuffOps.cpp:102 之后（Replace 支重新取 Bucket 之后）补：
Def = Subsystem.GetRegisteredBuffDef(DefTag);
if (!Def) { /* 同 :161 的处置：Log + 返回 EBAR_Applied/Rejected */ }
```

注意 `Def` 在 `:34` 声明为 `FTcsBuffDef const*`，**已经预留了重新赋值的能力**（注释写「非 const 指针……需重取」，虽然类型是 `const*`，但**指针本身可重赋值**）。⇒ 这是一处**注释与声明都准备好了、只差一行赋值**的遗漏。

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立。`ARCH-07-buff` 首版（**871 行**，修复记录后 **884 行**）：**47 文件 / 5 839 行**审核完成。审出 **D-1..D-12 / U-1..U-7**。三条结构性发现：① **规格主语第一次全对、符号名全错**（6 组命中 0，且错的方向相反）——与 `ARCH-05` 的「权威层主语错了」正好互补；② **唯一真·代码缺陷是 D-1**（Replace 支未重新解析 `Def` ⇒ use-after-free），而同函数内另外三处都防了；③ 新增缺陷方向 **G-18**（一次改名没有单一规则，每个类型各自落在三种组合之一）。另新增 **G-19**（「测试算不算消费者」判据缺口）/ **G-20**（规格倾向于记录「做了的」，对「决定不做的」没有位置）。**§7.2 首次给出 `SPEC-02-states` 的 14 项差异清单**，判定旧文档族「**§12 收束回写质量高、正文停在设计轮**」——与 `ARCH-06` 对 `SPEC-03-effects` 的判定同型 |
| 2026-10-17 | **D-1 已修复**（本条为全档案唯一崩溃级缺陷）。`TcsBuffOps.cpp` 的 Replace 支在 `Remove(...)` 之后补 `Def` 重取 + 空判（+17 / −2）。触发路径 = `MaxStacks = 1` + `EOP_ReplaceExisting`（五轴标准组合），`Remove` 的两次同步广播中订阅者可调 `UnregisterBuffDef`（公开 API）⇒ `TUniquePtr` 析构 ⇒ 后续读 `Def->LevelBase` / `Def->Params` / `Def->ModifierRows` 是 use-after-free。**双配置编译 0 error 0 warning**（Editor Development + Shipping）。⚠ 未跑 PIE——装置没有「在 `Removed` 回调里注销定义」的订阅者，今天打不到这条路径，故为静态确认；规格侧「广播后身份重查」仍只讲实例指针、没讲 Def 指针，该缺口按 `G-32` 单列 |

