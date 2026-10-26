# ARCH-05-state · TcsState（共享层）

- **文档 ID**：`ARCH-05-state`
- **类型**：模块档案（现状 + 债务 + 未实现）
- **状态**：生效中
- **权威范围**：`TcsState` **今天实际落地了什么**。本模块是**层 2 的共享层**——它不服务某一个业务域，而是 `TcsBuff` 与 `TcsSkill` **两个互不依赖的兄弟域**的唯一共用下层。它的契约一旦漂移，两个域同时受影响，且**没有任何一域的测试能发现**。
- **最后更新**：2026-10-17（首次审核）

---

## §1 模块定位与边界

**一句话**：给两个兄弟域提供**它们必须共用、又谁都不该独占**的四样东西——Def 数据形状族、运行态基底与快照、per-entity 桶与注册表模板、跨域互通契约（参数表读口 + 关系表检查器 + 决策接口）。**它自己不做业务**：不施加、不广播生命周期事件、不物化修正器、不认识时间。

| 项 | 内容 |
|---|---|
| 代码规模 | **2 781 行**（28 个文件：15 `.h` + 11 `.cpp` + `TcsStateModule.h/.cpp` + `TcsState.Build.cs` 28 行）；扣掉 `Build.cs` ⇒ 代码 **2 753 行** |
| 编译期依赖 | `Core` / `CoreUObject` / `Engine` / `GameplayTags` + **`TcsCore`** + **`TcsNotation`** + **`TcsAttribute`**（`TcsState.Build.cs:17-26`，恰好七项） |
| 被谁依赖（`Build.cs` 实测） | `TcsBuff` / `TcsSkill` / `TcsIntegration` —— **3 个** |
| **不依赖谁** | **`TcsEffect`**（裁定⑫已兑现，`Build.cs:15-16` 明写「共享层对 `TcsEffect` 的 include 为零」）——但**依赖 `TcsAttribute`**，因为 `FTcsStateDefBase::ModifierRows` 是 `TArray<TSoftObjectPtr<UTcsAttrModDef>>` |
| 对应可执行需求 | **10 个能力 / 51 需求 / 187 场景 / 1 628 行**（但**只有 3 个能力真的在描述本模块的代码**，见 §1.2） |
| 旧文档族 | `SPEC-02-states`（`spec/03-module-states.md`，**390 行**，含 §12.13 的 `PLN-REFACTOR-1` 收束回写）+ 决策 `dec-2026-10-13-state-module-positioning` / `dec-2026-10-17-tag-relationship-ownership` |
| 落地日期 | 2026-10-04 起（R5 全轮）；2026-10-14 `PLN-REFACTOR-1` Task 4 **拆出 `TcsBuff`**，本模块保留原名、定位改述为共享层；2026-10-16 `R5.5-e1` 加关系表检查器；2026-10-17 加在飞查询契约 |

### 1.1 文件组与职责【实测】

| 组 | 文件 | 行 | 职责 |
|---|---|---|---|
| 模块壳 | `TcsStateModule.h/.cpp` | 20 | **空体模块类，零注册**（`IMPLEMENT_MODULE`，`class FTcsStateModule : public IModuleInterface {}`） |
| 日志 | `TcsStateLogChannel.h/.cpp` | 15 | `LogTcsState`（D0-6 v2：不 include `Module.h`） |
| **Def 形状** | `Def/TcsStateDefBase.h` | **191** | `FTcsStateDefBase`——两域 Def 的公共基类（`CategoryTags` / 关系四数组 / 等级 / 参数行 / 描述 / 修正器行） |
| Def 形状 | `Def/TcsParamRow.h` + `Def/TcsDescriptionEntry.h` | 114 + 84 | `ETcsParamMode` / `FTcsNumericParamRow` / `FTcsDescriptionEntry` / `FTcsDescriptionViewSlot` |
| Def 资产 | `Def/TcsStateDef.h/.cpp` | 85 + 43 | `UTcsStateDef : UPrimaryDataAsset`（族基类，只持 `DefTag` + `IsDataValid` 挂点） |
| **运行态基底** | `State/TcsStateInstance.h` | **134** | `FTcsStateInstance`——**6 个公共字段**，两域运行态的公共父结构 |
| **参数快照** | `State/TcsStateSnapshot.h` | 113 | `FTcsParamSnapshotEntry` / `FTcsParamSnapshot`（**非反射**） |
| 参数表读口 | `State/TcsStateParamTableReader.h` | **221** | `FTcsStateParamTableReader`（纯 C++ 本体）+ `UTcsStateParamTableReader`（反射壳）+ `FTcsStateSnapshotScope`（RAII 栈式绑定） |
| **关系表族** | `State/TcsStateRelation.h/.cpp` | 184 + 125 | `FTcsRelationRow`（五字段）+ `FTcsStateRelation`（**静态函数族，唯一判定入口**） |
| 关系表视图 | `State/TcsStateLiveView.h` + `State/TcsStateLiveQuery.h` | 58 + 85 | `FTcsLiveStateView`（事实 + 声明两半）+ `ITcsStateLiveQuery`（**单向只读契约，单槽位**） |
| **桶与注册表** | `Runtime/TcsEntityRuntimeBucket.h` | **462** | `TTcsEntityRuntimeBucket<TPayload, THandleType, TGenerationIssuer>` + `FTcsEntityRuntimeRegistry<TPayload, THandleType>` |
| 代际发号 | `Runtime/TcsEntityRuntimeGenerationIssuer.h/.cpp` | 39 + 29 | `FTcsStateGenerationIssuer`（**进程级**，出线到 `.cpp` 的匿名 namespace） |
| 重入守卫 | `Runtime/TcsEntityBroadcastGuard.h` | 105 | `TTcsEntityBroadcastGuard<THandleType>`（两域各持一份实例、实现只有一份） |
| **决策接口** | `Runtime/TcsEntityAdmission.h/.cpp` | 207 + 70 | `FTcsEntityAdmission`（三方法中性默认）+ 请求/既有视图 + **三个技能侧默认策略类** + 决策档位枚举 |
| 句柄语义单点 | `Runtime/TcsInstanceHandleOps.h` | 142 | `FTcsInstanceHandleOps`（判据 / 身份比较 / 哈希 / 池位模式转换） |
| **门面** | `TcsStateSubsystem.h/.cpp` | 120 + 55 | `UTcsStateSubsystem : UWorldSubsystem`——**只有 3 个方法**，见 §2.1 |
| 契约键 | `Param/TcsStateParamKeys.h/.cpp` | 42 + 10 | `Tag_TcsStateParam_Level` = `TcsStateParam.Level`（**插件原生声明**） |

### 1.2 规格的模块归属：**审核时 10 份里只有 3 份在讲本模块，2026-10-24 已重切**

> **★ 本节已按 2026-10-24 的「按模块重切」就地改述（提交 `c4511e9`）**。原文记录的是**审核当日（2026-10-17）的现场**，保留在下表「审核时」一列以便追溯；「今天」一列是重切后的实测。

审核时 `openspec/specs/` 有 10 个名字带 `state` 的能力，逐份核对主语后发现：**7 份整篇在讲 `TcsBuff`、2 份半讲共用件、1 份讲宿主装置**——**没有一个能力回答「`TcsState` 这个模块今天还剩什么」**。根因是 `PLN-REFACTOR-1` Task 4 拆模块时**只改了代码与注释、没有重切规格**：规格的**文件名**沿用 `state-*`，而主语已经搬走。

重切按**需求级主语**逐条核（不是按目录名），把 10 份分成四类，分别处理：

| 类 | 审核时的能力 | 处理 | 今天的能力 |
|---|---|---|---|
| **① 纯 Buff 篇**（25/25 需求全是 `TcsBuff`） | `state-instance-lifecycle` / `state-stacking-policies` / `state-modifier-materialization` / `state-behavior-fragments` / `state-step-library` | **改名**（正文一字未动） | `buff-*` 同名五份 |
| **② 宿主装置篇**（4/4 宿主装置） | `state-layer-e2e-validation` | **改名** | `buff-layer-e2e-validation` |
| **③ 混合篇**（各 4 Buff + 2 共享层） | `state-def-asset` / `state-param-snapshot` | **拆分** | 各拆成 `state-*`（共享层）+ `buff-*`（Buff 域） |
| **④ 真共享层篇** | `state-relation-check` / `entity-runtime-mechanism` | **不动**，补门面与基底契约 | 同名，各增需求 |

**⇒ 一个关键发现（③ 只拆不改名）**：拆完 Buff 部分后，`state-def-asset` 只剩 `FTcsStateDefBase` / `FTcsDescriptionEntry` / `UTcsStateDef`，`state-param-snapshot` 只剩 `FTcsParamSnapshot` / `UTcsStateParamTableReader`——**全是 `TcsState` 的东西**。⇒ 这两份是「**名字对、内容混了一半**」，不是「名字谎报主语」。**① 只能改名（名字错、内容也对），③ 只能拆（名字对、内容混）**，两者对症不同。

重切后的实测（2026-10-24）：

| 能力 | 行 / 需求 / 场景 | 主语 | 是否描述本模块代码 |
|---|---|---|---|
| `entity-runtime-mechanism` | 261 / **8** / 28 | 桶 / 注册表 / 守卫 / 决策接口 **+ 共享层门面与其存在判据 + 运行态基底** | ✅ **是**（基底与门面契约已并入） |
| `state-relation-check` | 118 / 5 / 12 | `FTcsRelationRow` / `FTcsStateRelation` | ✅ **是** |
| `state-def-asset` | 89 / **3** / 8 | `FTcsStateDefBase` / `FTcsDescriptionEntry` / `UTcsStateDef` | ✅ **是**（Buff 半已拆出） |
| `state-param-snapshot` | 58 / **2** / 6 | `FTcsParamSnapshot` / `UTcsStateParamTableReader` | ✅ **是**（Buff 半已拆出） |
| `buff-def-asset` | 154 / 5 / 16 | `FTcsBuffDef` / `UTcsBuffDefAsset` / 表行 / 作者期校验 | ❌ 否（`TcsBuff`） |
| `buff-param-snapshot` | 172 / 5 / 21 | 快照构建规则 / 等级源 / 时值周期 / 时长同步 | ❌ 否（`TcsBuff`） |
| `buff-instance-lifecycle` | 350 / 7 / 42 | `FTcsBuffInstance` / `UTcsBuffSubsystem` | ❌ 否（`TcsBuff`） |
| `buff-stacking-policies` | 225 / 6 / 26 | `FTcsBuffStackPolicy` | ❌ 否（`TcsBuff`） |
| `buff-modifier-materialization` | 161 / 7 / 18 | `FTcsBuffModifierMaterializer` | ❌ 否（`TcsBuff`） |
| `buff-behavior-fragments` | 111 / 4 / 12 | `FTcsBuffBehaviorFragment` | ❌ 否（`TcsBuff`） |
| `buff-step-library` | 37 / 1 / 4 | `FTcsStepApplyState` | ❌ 否（`TcsBuff`） |
| `buff-layer-e2e-validation` | 117 / 4 / 13 | 宿主装置 `Tcs.Test.State.Run`（住 `TcsDev`） | ❌ 否（装置住宿主） |

**⇒ 结论（重切后）**：回答「`TcsState` 今天剩什么」的能力有 **4 份 / 18 需求 / 54 场景**——`entity-runtime-mechanism`(8) + `state-relation-check`(5) + `state-def-asset`(3) + `state-param-snapshot`(2)。而挂 `state-*` 名字的**全部指向本模块**，不再有一份谎报主语。

> **原文的判词保留（仅作历史）**：「这条不是文档过期，而是权威层没有覆盖对象……只能读 `TcsStateSubsystem.h` 的头注释」。⇒ **该判断在 2026-10-24 之后已不成立**：门面三方法与其存在判据、运行态基底 6 字段判据都已进 `entity-runtime-mechanism`。**§5 D-1 已闭合**。

---
## §2 已落地

### 2.1 门面被削到只剩 3 个方法——这是本模块最健康的一处设计【实测】

`UTcsStateSubsystem` 原体 431 行，Task 4 拆走后**只剩 120 行头 + 55 行实现**，对外**只有三个方法**：

```
Source/TcsState/Public/TcsStateSubsystem.h:38   class TCSSTATE_API UTcsStateSubsystem : public UWorldSubsystem
  :84   UTcsStateParamTableReader* GetParamTableReader();   // 两域共用的快照参数表壳
  :98   ITcsStateLiveQuery*         GetLiveQuery() const;    // 在飞运行态查询契约（R5.5-e1）
  :109  void                        SetLiveQuery(ITcsStateLiveQuery* InQuery);
```

**判据写进了头注释，且它是可执行的**（`TcsStateSubsystem.h:28`）：

> 「**本类的存在判据 = 『这个设施是否被两个域在代码级使用』**，本类只剩这一条成立。」

这条判据的可执行性来自一个**实测事实**：`TcsSkill` 对本模块的依赖**只有 `GetParamTableReader()`**（`TcsCastOps.cpp:547` / `TcsCastOps_Phase.cpp:44` 两处），而等级读口 / 事件与时钟读口的代码级消费者**全部在 `TcsBuff`**。头注释还留了一条自我纠正的记录：「该判据曾被写错，见 `PLN-REFACTOR-1` Task 4《开工前置》的《一处判据自我纠正》」。

**这比多数共享层健康**：绝大多数「共享层」的腐烂方式是**什么都往里塞**，而这里的判据是「**两个域都在代码级用它**」——不满足就不进。今天**只有 3 个方法**是这条判据还在生效的证据。

### 2.2 Def 形状族：191 行基类 + 两个薄派生位【实测】

`FTcsStateDefBase`（`Def/TcsStateDefBase.h:39`）持**五组字段**，且每组都有明确的「为什么在这一层」：

| 组 | 字段 | 上提/留守的判据 |
|---|---|---|
| 类别标识 | `FGameplayTagContainer CategoryTags` | 2026-10-15 由 `StatusTag`（单 tag）**合并而来**——容器是超集，合并 = **删掉冗余的那个**，不是新增 |
| **关系** | `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel` | **2026-10-17 由两个派生类上移到基类**（见 §2.3） |
| 等级 | `LevelBase = 1` / `MaxLevel = 0` | 两域同角色；`0` = 无上界 |
| 参数行 | `TArray<FTcsNumericParamRow> Params` | 两域同型（技能 Def 继承白拿） |
| 描述 | `TArray<FTcsDescriptionEntry> Descriptions` | 同上（渲染归 R8，**零真实消费者**） |
| 修正器行 | `TArray<TSoftObjectPtr<UTcsAttrModDef>> ModifierRows` | 同上；**这也是本模块唯一依赖 `TcsAttribute` 的原因** |

**基类的划分判据被写死并可机械核验**（`TcsStateDefBase.h:23-25`）：

> 「**无时值 / 无堆叠 = 基类；有时值 / 有堆叠 = 派生**」

这条判据不是装饰——`FTcsBuffDef` 的四个时值字段（`DurationPolicy` / `DurationTime` / `Period` / `PeriodRefresh`）与堆叠字段（`StackPolicy`）确实全在派生，`FTcsSkillDefData` 也确实一个都没有。**它是全仓少数「划分判据能被 grep 验证」的基类。**

### 2.3 关系四字段上移：一次「付过代价才修的」重构【实测】

四个关系字段原先**住两个派生类各一份、逐字同义**（`FTcsBuffDef` / `FTcsSkillDefData`）。2026-10-17 上移到基类，理由在 `TcsStateDefBase.h:81-99` 写得比规格更清楚：

| 论据 | 内容 |
|---|---|
| **按本基类自己的判据本就在基类** | 这四个字段既无时值也无堆叠 ⇒ 判据直通 |
| **从未有过「为何住派生」的论证** | 「全库文档与注释均无」——即它是 R5 先落 Buff、R6 建技能 Def 时**照抄**的结果 |
| **代价是真实的** | 「每次关系族演进都要改两处，且**漏一处不报错**」 |
| **当时可能的理由已不成立** | `SPEC-03` 原记「两侧语义映射不同」（状态约束**施加**、技能约束**激活**），但 `R5.5-e1` 实现下来两侧走**同一个检查器**、且**都拿「在跑状态」当客体词面** ⇒ 「施加 vs 激活」只是域内动词差异，**不是两种语义** |

**「`R5.5-e1` 一次性付过两次」**这句是本仓少见的**量化了重复成本**的注释：新增 `TagsToBlock` 改两个文件、`Blocks` 改名改两个文件。

**同时留了一条反例**（`TcsStateDefBase.h:97-99`）：`Priority` **MUST NOT 跟进**——它不属关系字段族（归槽位竞争表 `R5.5-e4`），故仍留两个派生类各一份。**这是「按判据」而不是「按顺手」的处置。**

### 2.4 运行态基底：6 字段，且有三处「看起来像、实际不是」的邻词被显式挡住【实测】

`FTcsStateInstance`（`State/TcsStateInstance.h:61`，**非反射**）只有 6 个字段：

| 字段 | 为什么在基底 |
|---|---|
| `DefTag` | 两域皆有定义身份 |
| `Entity` | 两侧现状逐字相同；自持后 `GetState(Handle)` 才是 O(1)（句柄无实体段） |
| `CascadeAnchor` | 两域同角色——「按它一次摘净」的级联撤销锚点 |
| `ParamSnapshot` | 两侧逐字相同 |
| `Level` | 两侧逐字相同（生效瞬间冻结、运行中不追溯） |
| `ExpiryEntry` | 两侧各有一枚「当前计时项的取消锚」 |

**三处邻词被逐条挡在门外**（`TcsStateInstance.h:46-53`，标注「逐条实测，2026-10-14」）：

1. **`CascadeAnchor` ≠ 域内的 `Source`**——后者是「谁施加的」，明文 **MUST NOT 当撤销锚点**（同一来源可施加多个不同定义 ⇒ 按它摘会误摘别条实例的条目）。`Source` **不住基底**。
2. **`ExpiryEntry` ≠ 技能侧的「时段游标」**——`FTcsSkillInstance::PhaseIndex` 是时段推进下标，**同词根不同义**。
3. **基底 MUST NOT 持生命周期态**——`Phase` 守的是「移除广播窗口内重入移除同一句柄」（台账 `STAT-5`），而技能侧同类问题**已由 `TTcsEntityBroadcastGuard` 解决** ⇒ 该字段在技能侧**零消费者**，按「零消费者不预建」留各域自己声明。

**命名判据也写死了**（`TcsStateInstance.h:18-24`）：`State` 在本仓是**共享层词根**、不是「Buff 专属」，它与 Def 层**完全同构**（`FTcsStateDefBase` ← `FTcsBuffDef` / `FTcsSkillDefData` ⇒ `FTcsStateInstance` ← `FTcsBuffInstance` / `FTcsSkillInstance`）；并明写 **MUST NOT 取 `FTcsEntityRuntime` 一类含 `Entity` 的名字**（`Entity` 专指承载运行态的战斗实体，用 `Entity` 命名运行态属词汇二义性，见 `DEC-2026-10-14-entity-vocabulary`）。

### 2.5 per-entity 桶：**进程级代际发号器**是它存在的理由【实测】

`TTcsEntityRuntimeBucket`（`Runtime/TcsEntityRuntimeBucket.h:63`）是**一个模板的两个实例化**（`FTcsBuffInstance` / `FTcsLearnedSkillEntry`），载体是 `TcsCore` 的 `TTcsInstancePool`。合并掉的重复是真实的：状态手写桶与技能手写桶**逐行同构**（槽位 + 代际 + 空闲栈 + 代际校验 + 稳定序遍历 + 重置）。

**核心判据在第三模板参上**（`TcsEntityRuntimeBucket.h:42-46`）：

> 「桶是 **per-entity** 的，而句柄**不带实体段**；若各桶各从代际 1 起发，两个实体各自的首实例会得到**相同的 `(Index, Generation)`** ⇒ 『跨桶扫描式解析』（`FTcsBuffOps::Find` 即此类）**命中错实体**。这是本仓 **2026-10-05 实测过的缺陷**。」

发号器本体**出线到 `.cpp` 的匿名 namespace**（`TcsEntityRuntimeGenerationIssuer.cpp:13`），并明写 **MUST NOT 用头内联 `static` / inline 变量**——「模块化构建下每个引用它的 DLL 各留一份，于是各模块各自从 1 起发号（症状随构建配置漂移）」。**这是全仓对 `static` 变量陷阱写得最清楚的一处。**

**唯一合法的非默认用法也被写死**（`TcsEntityRuntimeBucket.h:58-60`）：技能侧那张**扁平**的施法运行态池（`FTcsSkillInstance` 池）传 `FTcsPoolLocalGenerationIssuer`——「它不由实体分桶、也不做跨桶扫描 ⇒ 无身份撞车风险」。

### 2.6 重入广播守卫：两域各持实例，实现只有一份【实测】

`TTcsEntityBroadcastGuard<THandleType>`（`Runtime/TcsEntityBroadcastGuard.h:32`）取代了状态侧 `InRemovalBroadcastHandles` 与技能侧 `InFinishBroadcastHandles` 两份同形成员。实际使用：

| 域 | 成员 | 调用点 |
|---|---|---|
| `TcsBuff` | `RemovalBroadcastGuard` | `TcsBuffOps.cpp:212`（判）/ `:268`（Enter）/ `:271`（Exit） |
| `TcsSkill` | `FinishBroadcastGuard` | `TcsCastOps.cpp:706`（判）/ `:712`（Enter）/ `:915`（Exit） |

**语义三条被逐条保住**（`TcsEntityBroadcastGuard.h:18-22`）：重入时**返回 false 且静默**（MUST NOT ensure、MUST NOT Warning——它是合法订阅者行为）；只拦**同一句柄**；撤锚顺序 = **Add → 广播 → Remove**。

**为什么是模板而不是「统一句柄」**：三个句柄类型**刻意保持独立**（编译期护栏，见 §2.7）⇒ 守卫按句柄类型模板化。

### 2.7 句柄语义单点：拒绝合并、也拒绝用宏绕过【实测】

`FTcsInstanceHandleOps`（`Runtime/TcsInstanceHandleOps.h:45`）是三个展平句柄（`FTcsBuffInstanceHandle` / `FTcsSkillInstanceHandle` / `FTcsSkillEntryHandle`）的**判据 / 身份比较 / 哈希 / 池位模式转换的唯一实现处**。三个薄型各自委托，实测委托点各 5 处（`IsValid` / `ToInner` / `FromInner` / `IsSameIdentity` / `Hash`）。

**这条设计最有价值的部分是它记录的「被否决的绕过方案」**（`TcsInstanceHandleOps.h:31-38`）：一个宏生成三个 `USTRUCT`。否决依据是**引擎源码实测**：

- 引擎 `Runtime/` 内**宏展开出 `USTRUCT(` 的用例零命中**；四个自带 wrapper-struct 宏**全部只生成普通 C++ struct**；
- `USTRUCT(...)` 在 `ObjectMacros.h` 里被定义成**空**（「给编译器看的占位」），**不是** UHT 的展开入口；
- `GENERATED_BODY` = `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)` ⇒ **按 `__LINE__` 拼符号名**，而 UHT **按扫描到的行**生成 ⇒ 外层宏包裹后**行号语义是否仍对齐，本机无法确认**。

⇒ 依本仓纪律（「同类『本机不可核实』的依赖一律去掉」），**MUST NOT** 把该能力建在未核实的 UHT 行为上。**这条记录的价值在于：它把「为什么不用更聪明的写法」写成了可复核的证据，而不是偏好。**

### 2.8 决策接口：抽象手法与「框架零具体策略」的边界【实测】

`FTcsEntityAdmission`（`Runtime/TcsEntityAdmission.h:121`）用 `USTRUCT(meta = (Hidden))` + **中性默认实现** + `virtual ~() = default`，并明写 **MUST NOT 用 `= 0`**（UHT 为每个 `USTRUCT` 生成 `TCppStructOps<T>`，构造路径需可默认构造）与 **MUST NOT 用 `PURE_VIRTUAL`**（`CHECK_PUREVIRTUALS` 开启时展开为 `= 0`、关闭时才是致命错误体 ⇒ **Development 编不过、Shipping 能过**，最坏的一类失败）。

三个默认策略类住本模块（`TcsEntityAdmission.h:170/186/202`）：`FTcsEntityAdmissionAlwaysNew` / `FTcsEntityAdmissionPerEntityReject` / `FTcsEntityAdmissionPerEntityReplace`。**它们住共享层的判据是「零业务类型」**——若住 `TcsSkill`，共享层无法引用，而它们本身不含任何域词汇。

消费侧实测：`TcsCastOps.cpp:248-257` 取址后走虚分派（三个无状态值类型），`TcsBuffOps_Stack.cpp:224` 走 `Fragment->ResolveStacks`。**两边都真的走接口，没有内联分流。**

### 2.9 契约键 `TcsStateParam.Level`：一处「为什么不能下放给宿主」的完整论证【实测】

`Param/TcsStateParamKeys.h:42` 声明 `extern TCSSTATE_API FNativeGameplayTag Tag_TcsStateParam_Level;`，`.cpp:8` 定义 `TcsStateParam.Level`。**这是本模块唯一原生声明的 tag**，而规格 `state-def-asset:14` 与 `param-value:49` 都写着「插件 MUST NOT 声明任何参数表键」——**表面冲突，实际不冲突**，头注释给了三条判据：

| 判据 | 内容 |
|---|---|
| 语义归属 | `Level` 的本体论在本模块（`LevelBase` / `MaxLevel` 声明在 `FTcsStateDefBase`，技能 Def **继承**它 ⇒ M5 没有自己的等级定义层） |
| **依赖方向（决定性）** | `TcsSkill.Build.cs` 已含 `TcsState` ⇒ 声明在本模块时 M5 消费零成本；反过来若声明在 `TcsSkill`，状态侧将来要消费就**反向依赖 = 成环** |
| 声明方判据 | 「谁解释那个词」——`EffectiveLevel = clamp(0, LevelBase + Σ)` 由等级域解释 ⇒ 按 `gameplay-tag-governance` 的「框架契约词 MUST 由插件模块原生声明」，MUST 由拥有该语义的模块声明 |

**并且它明确区分了契约词与内容词**：「一般键是**内容词**（含义由宿主规定）；`Level` 是**框架自己解释的契约词**。下放给宿主配置，宿主漏配即 `Level` 修正**静默失效**（数值照旧进账本、等级永不动、零报错）。」

**导出宏也给了实证**（`TcsStateParamKeys.h:38-40`）：「`UE_DECLARE_GAMEPLAY_TAG_EXTERN` 展开为**裸 `extern`**（`NativeGameplayTags.h:31`，无 `__declspec(dllexport)`）⇒ 其它模块引用本变量会**链接失败**（`LNK2001`）」——**这条是 `ARCH-04` 也踩过的同一个坑**（见 `ARCH-04` §2.7）。

## §3 对外契约

### 3.1 稳定契约（其它模块可以依赖）【实测】

| 契约 | 符号 | 真实消费者（实测计数） | 稳定性 |
|---|---|---|---|
| **Def 基类** | `FTcsStateDefBase` | `TcsBuff`（`FTcsBuffDef : FTcsStateDefBase`）+ `TcsSkill`（`FTcsSkillDefData : FTcsStateDefBase`） | **稳定**——字段划分判据可 grep 验证 |
| Def 族基类 | `UTcsStateDef` | `UTcsBuffDefAsset : UTcsStateDef`（`TcsBuff`）+ `UTcsSkillDef`（`TcsSkill`） | 稳定 |
| **运行态基底** | `FTcsStateInstance` | 两域派生各 1 处 | **稳定**——但见 §3.3 的载荷警告 |
| **参数快照** | `FTcsParamSnapshot` / `FTcsParamSnapshotEntry` | 全仓 **19 处**引用（两域各半） | **稳定**（非反射，纯 C++ 流转） |
| **参数表读口** | `UTcsStateParamTableReader` / `FTcsStateSnapshotScope` | 9 + 11 处：`TcsBuff` 物化 1 处、`TcsSkill` 3 处（`TcsCastOps` ×2 + `TcsCastOps_Phase` ×1） | **稳定**——这是 `TcsSkill` 对本模块的**唯一**代码级依赖 |
| **关系行与检查器** | `FTcsRelationRow` / `FTcsStateRelation` | 13 + 7 处：`TcsBuff`（`TcsBuffOps_Relation.cpp` 5 处）+ `TcsSkill`（`TcsSkillOps_Relation.cpp` 5 处） | **稳定**——判定逻辑是两域唯一一份 |
| 在飞查询契约 | `ITcsStateLiveQuery` / `FTcsLiveStateView` | 9 + 4 处：实现方 `UTcsBuffSubsystem`，消费方 `TcsSkill` | **⚠ 即将被推翻**（见 §5 D-3） |
| **桶与注册表模板** | `TTcsEntityRuntimeBucket` / `FTcsEntityRuntimeRegistry` | 各 4 处：`FTcsBuffRegistry` / `FTcsSkillRegistry` 两个别名 | **稳定** |
| 重入守卫 | `TTcsEntityBroadcastGuard` | 7 处：两域各 1 个成员 + 各 3 个调用点 | 稳定 |
| **决策接口** | `FTcsEntityAdmission` + 3 个默认策略类 | **34 处**（全仓第三多）：`TcsBuff` 决策树 + `TcsSkill` 门禁④ | **稳定** |
| 句柄语义单点 | `FTcsInstanceHandleOps` | 22 处：三个句柄各 5 个委托方法 | 稳定 |
| 进程级发号器 | `FTcsStateGenerationIssuer` | 3 处（桶模板默认档） | 稳定 |
| 契约键 | `Tag_TcsStateParam_Level` | `TcsSkill` 参数链 + 宿主 `TcsDev` | 稳定 |
| 门面 | `UTcsStateSubsystem` | 15 处（`TcsBuff` 4 + `TcsSkill` 4 + 自身 7） | **稳定但极窄**（3 个方法） |

### 3.2 MUST NOT 依赖（内部实现细节）

- **`FTcsStateParamTableReader`（纯 C++ 本体）**：全仓**零外部构造**——只有反射壳 `UTcsStateParamTableReader::TryGetNumericParam_Implementation` 就地构造它（`TcsStateParamTableReader.h:133`）。外部只该经壳 + 作用域。
- **`FTcsStateGenerationIssuer` 的计数器本体**：住 `.cpp` 匿名 namespace，**任何模块 MUST NOT 读取或复位它**。
- **桶的 `FPool` 成员**：私有；外部只能经 `AllocateSlot` / `ReleaseSlot` / `Find` / `ForEach` 一族。
- **`FTcsEntityAdmission` 的三个方法实现**：中性默认实现是**契约的一部分**（`IsSameGroup` 默认 = 同实体 + 同 `DefTag`），但**调用方 MUST NOT 假设某个具体策略类的存在**。

### 3.3 一条**载荷类型**的硬约束：桶 MUST 绑派生、MUST NOT 绑基底【实测】

这是本模块最容易被误用的一处。规格写得很直白（`state-instance-lifecycle:56`）：

> 「**桶载荷 MUST 绑派生结构 `FTcsBuffInstance`、MUST NOT 绑基底**——池按值存 `TArray<TPayload>`，绑基底会把层数 / 时值 / 周期**静默截断**（值截断，多数读数仍会过）。」

**代码侧实测成立**：`FTcsBuffRegistry = FTcsEntityRuntimeRegistry<FTcsBuffInstance, FTcsBuffInstanceHandle>`（`TcsBuffRegistry.h:52`）、`FTcsSkillRegistry = FTcsEntityRuntimeRegistry<FTcsLearnedSkillEntry, FTcsSkillEntryHandle>`（`TcsSkillRegistry.h:183`）——**都绑派生，无一处绑基底**。

**为什么这条要写进契约**：`FTcsStateInstance` 是**基底**，看起来「桶里装公共字段就够」，而实际业务全部要读派生字段。绑基底**不报错**、只截断，且**多数读数仍然通过**（只有读层数/时值的检查才失败）。⇒ 共享层把「基底存在」和「基底可作载荷」分开是必要的。

---

## §4 竖切影响

### 4.1 调用方向（谁在什么时候碰它）【实测】

```
World 初始化
  ├─ UTcsStateSubsystem::Initialize        → 建快照参数表壳（单例，零 per-apply 分配）
  └─ UTcsBuffSubsystem::Initialize        → SetLiveQuery(this)  ★ 域向下注册

GameInstance 定义库装配（TcsIntegration）
  └─ UTcsBuffSubsystem::RegisterBuffDef   → 按值拷入（TUniquePtr 持有，地址稳定）

施加一次状态（TcsBuff）
  ├─ CheckApplyGate      → FTcsStateRelation::CheckRequired / CheckForbidden / CollectBlocks
  ├─ ApplyCancelAndBlock → FTcsStateRelation::CollectCancels / CollectBlocks
  ├─ BuildSnapshot       → FTcsParamSnapshot
  ├─ MountModifiers      → FTcsStateSnapshotScope（绑壳）→ UTcsStateParamTableReader
  └─ Broadcast           → 事件总线

技能激活（TcsSkill）
  ├─ 关系门   → GetLiveQuery() → ForEachLiveState → FTcsStateRelation::CollectBlocks
  └─ 参数求值 → GetParamTableReader() → FTcsStateSnapshotScope
```

### 4.2 时序位置（硬约束，逐条有代码锚点）

| # | 时序 | 锚点 | 违约后果 |
|---|---|---|---|
| 1 | **共享层门面 MUST 先于域门面存在** | `TcsBuffSubsystem.cpp:20` 在 `Initialize` 里 `GetWorld()->GetSubsystem<UTcsStateSubsystem>()` | 拿不到 ⇒ 契约未注册 ⇒ 技能侧关系门**降级放行**（`TcsBuffSubsystem.cpp:24` 留 Warning） |
| 2 | **`SetLiveQuery` MUST 在域 `Initialize`、`SetLiveQuery(nullptr)` MUST 在域 `Deinitialize`** | `TcsBuffSubsystem.cpp:22` / `:72` | 悬空指针（共享层可能比域活得久）⇒ 注释明写「防悬空」 |
| 3 | **关系门 MUST 排在共存决策之前** | `TcsBuffOps.cpp:47` | 门禁答「能不能存在」，叠层答「以什么形态存在」——前者不成立时后者无意义 |
| 4 | **施动半（Cancel/Block）MUST 在门禁之后、共存决策之前** | `TcsBuffOps.cpp:55` | 「刷新/叠层/新建三条路径都覆盖」；只做门禁会漏掉刷新路径 |
| 5 | **施动半之后 MUST 重取 `Def`** | `TcsBuffOps.cpp:59-63` | 施动半会移除实例并广播 ⇒ 订阅者可重入注销定义 ⇒ 旧指针悬空 |
| 6 | **快照绑定 MUST 用 RAII 作用域，MUST NOT 手工 `Bind` 跨广播** | `TcsStateParamTableReader.h:160` | 「当前绑定」会变成一处隐式的跨帧状态 |
| 7 | **嵌套绑定 MUST 栈式恢复，MUST NOT 退出即清空** | `TcsStateParamTableReader.h:174/187` | 外层作用域**静默失去绑定**（表现为外层后续查询全部落兜底，最难查的一类） |

### 4.3 一条竖切级的教训：**`SetLiveQuery` 是「单槽位」，而它注定要被两域同时用**

这是本模块**目前最严重的活缺陷**，且它已经在规格层面被判死。

**现状**（`TcsStateSubsystem.h:109-113`）：

```
void SetLiveQuery(ITcsStateLiveQuery* InQuery);   // 单个裸指针成员，无校验、无报错
ITcsStateLiveQuery* LiveQuery = nullptr;
```

**注册方只有一个**：`UTcsBuffSubsystem`（`TcsBuffSubsystem.cpp:22`）。**消费方也只有一个**：`TcsSkill`（`TcsCastOps.cpp:203`）。

**但 2026-10-17 的裁定要求两域都注册**（`DEC-2026-10-17-tag-relationship-ownership` §3.2 原文）：

> | 问题 | 后果 |
> |---|---|
> | `SetLiveQuery` 静默覆写 | 后注册者**顶掉**先注册者，**无报错** |
> | `Deinitialize` 无条件清 | 某域反初始化会**误清另一个域的注册** ⇒ 调用方拿空指针 ⇒ **静默失效** |

**代码侧两条都已坐实**：
- `SetLiveQuery` 的实现就是 `LiveQuery = InQuery;`（`TcsStateSubsystem.cpp:52-55`）——**没有「已被占」的检查**；
- `UTcsBuffSubsystem::Deinitialize`（`TcsBuffSubsystem.cpp:70-73`）**无条件** `SetLiveQuery(nullptr)`。

**⇒ 今天只有一个域注册，所以两个 bug 都不显形。** 一旦 `TcsSkill` 按裁定注册自己的那半，两域初始化顺序决定谁被顶掉，而**症状是关系门静默失效**（`TcsCastOps.cpp:196` 的降级路径会留一条 Warning，但那是「未注册」的措辞，会把「被顶掉」误报成「没注册」）。

**这条不是「未来才需要关心」**——裁定已拍板、缺口清单已列出 5 条（§5 的 D-3 引用）。它今天就在册，只是**恰好没被触发**。

### 4.4 一条被两域**用对**的边界：`TcsSkill` 不依赖 `TcsBuff`

`TcsSkill.Build.cs:13-23` 的依赖数组实测：`Core / CoreUObject / Engine / GameplayTags / TcsCore / TcsNotation / TcsAttribute / TcsEffect / TcsState` —— **无 `TcsBuff`**。

而技能侧要读状态域的在飞实例，路径是：

```
TcsSkill ──► UTcsStateSubsystem::GetLiveQuery() ──► ITcsStateLiveQuery（共享层定义）
                                                        ▲
TcsBuff  ──► SetLiveQuery(this) ───────────────────────┘（域向下注册）
```

**这条边是全仓最值得记的一处设计**：它不靠「宿主接线」解决跨域互通，而是**域自己向下注册**。头注释给了判据（`TcsStateLiveQuery.h:30-33`）：

> 「这不是宿主能力——宿主没有理由知道『插件内部两个域要互通』，那是**框架自身机制的缺失**。若要求宿主接线，则每个宿主都要接一次，而**漏接的表现是关系门静默失效**。」

**⇒ 判据正确、方向正确，缺的只是「多注册」这一半**（§5 D-3）。

### 4.5 跨模块：本模块是**被依赖最多**的模块之一，但依赖它的模块大多只碰 1~2 个符号

| 模块 | 依赖 `TcsState` 的 `Build.cs` | 实际使用的符号（实测） |
|---|---|---|
| `TcsBuff` | ✅ | 全部（基底 / 桶 / 守卫 / 决策 / 快照 / 关系 / 读口） |
| `TcsSkill` | ✅ | 基底 / 桶 / 守卫 / 决策 / 快照 / 关系 / 读口 / 句柄单点 |
| `TcsIntegration` | ✅ | **零直接符号**——只在 `Build.cs` 声明，源码里对 `TcsState` 的提及全是注释（`TcsDefinitionSubsystem.h:72` 的「→ `TcsState` 的登记口」已过期，实际登记口在 `TcsBuff`） |
| `TcsEffect` / `TcsDamage` / `TcsTargeting` | ❌ | 0 |

**⇒ 一条真实的依赖债**：`TcsIntegration.Build.cs:28` 声明了 `TcsState`，但 `TcsIntegration` 的源码**一次都没直接用它**。这是 `PLN-REFACTOR-1` Task 4 拆分时的残留（当时登记口还在 `TcsState`）。**不是错误，但是一处可以删的边**——登记为 §5 D-8。

## §5 技术债（期望 vs 实际，逐条带证据）

> **本模块的债有极强的同质性**：几乎全部来自 **`PLN-REFACTOR-1` Task 4 把 `TcsBuff` 拆出去时，代码搬了、规格与注释没搬**。所以下面 9 条里 5 条（D-1 / D-4 / D-5 / D-6 / D-8）本质是**同一个根因的 5 个面**——**其中 D-1 已于 2026-10-24 闭合（提交 `c4511e9`），其余 4 条仍待裁**。这与 `ARCH-04` 的「规格更严、代码更松」方向相反——**这里是「代码搬走了，指向它的东西全留在原地」**。

### D-1 ★★★ ✅ **已闭合（2026-10-24，提交 `c4511e9`）** `openspec` 里**没有**描述 `TcsState` 本体的能力——7 份「state」规格整篇在讲 `TcsBuff`

| 项 | 内容 |
|---|---|
| **期望** | 按 `ARCH-INDEX` §2 的三层真相源，`openspec/specs/` 是**最高权威层**——读者问「共享层的契约是什么」应当能在那里查到 |
| **实际** | 10 个 `state-*` 能力中，**7 个整篇描述 `TcsBuff`**、2 个描述共用件、1 个描述宿主装置（§1.2 的逐份核对表）。**没有一个能力的主语是 `TcsState` 这个模块** |
| **证据** | `state-instance-lifecycle` 332 行全部围绕 `UTcsBuffSubsystem` / `FTcsBuffInstance`；`state-stacking-policies` / `state-modifier-materialization` / `state-behavior-fragments` / `state-step-library` / `state-layer-e2e-validation` 同理。**实测 grep：`openspec/specs/` 内「统一读口」零命中**，说明连 2026-10-17 的新裁定都还没进规格 |
| **根因** | `PLN-REFACTOR-1` Task 4 只改了**代码与注释**，**没有重切规格**——规格的**文件名**沿用 `state-*`，而主语已经搬进 `TcsBuff` |
| **影响面** | 这是**结构性的**：它决定了本模块后面所有债的读法。今天想知道「共享层的契约」只能读 `TcsStateSubsystem.h` 的头注释（而头注释**不是**权威层） |
| **是否已登记** | ❌ 否（`ARCH-INDEX` 的 G-11 是「台账与档案不同步」，本条是「权威层缺对象」，不同类） |
| **建议处置** | 原建议：新建一个 `tcs-shared-layer` 能力（或把 7 份里的共用部分抽出来），主语明确是 `TcsState`。**这不是本档案能做的决定**，需要用户裁 |
| **实际处置** | **用户 2026-10-24 裁定：三刀都做，且不新建能力**。① 改名 6 份 `state-*` → `buff-*`（正文一字未动，逐字节往返比对验证）；② 拆 2 份混合篇（`state-def-asset` → 3+5 需求、`state-param-snapshot` → 2+5 需求，原始 44 个场景全保留）；③ 门面与基底契约**并入 `entity-runtime-mechanism`**（新增「共享层门面与其存在判据」「共享层运行态基底 `FTcsStateInstance`」两条需求）。**未新建能力**——核实后两条缺口都是「内容已经在、位置错了」的搬家问题 |
| **闭合证据** | 能力数 45 → **47**；`openspec validate --all --strict` **48 passed / 0 failed**；回答「`TcsState` 今天剩什么」的能力 = **4 份 / 18 需求 / 54 场景**（`entity-runtime-mechanism`(8) + `state-relation-check`(5) + `state-def-asset`(3) + `state-param-snapshot`(2)）；挂 `state-*` 名字的**全部指向本模块**，不再有一份谎报主语。详见 §1.2 的改述与 `openspec/changes/refactor-split-state-specs-by-module/` |

### D-2 ★★ `SetLiveQuery` 是**单槽位且无冲突检测**，而 2026-10-17 的裁定要求两域都注册

| 项 | 内容 |
|---|---|
| **期望** | 按 `DEC-2026-10-17-tag-relationship-ownership` §3.2：「形状 MUST 为**按域一格**；注册口 MUST 能在『该格已被占』时**报错**（而非覆写）」 |
| **实际** | `TcsStateSubsystem.cpp:52-55` 的实现是 `LiveQuery = InQuery;`——**一个裸指针、无检查、无报错**。`UTcsBuffSubsystem::Deinitialize`（`TcsBuffSubsystem.cpp:70-73`）**无条件** `SetLiveQuery(nullptr)` |
| **偏差证据** | 代码 3 行；裁定原文把两个后果**逐条列出**（静默覆写 / 误清别人注册）。今天只有一个域注册 ⇒ **两个 bug 都不显形** |
| **影响面** | 一旦 `TcsSkill` 注册自己的那半，初始化顺序决定谁被顶掉，**症状 = 关系门静默失效**；且降级路径的 Warning 措辞是「未注册」，会把「被顶掉」**误报**成「没注册」 |
| **是否已登记** | 🟡 部分——裁定文档已记录（§3.2 / §5），但**台账 `deferred-inputs-ledger.md` 零命中 2026-10-17**（实测 grep 计数 = **0**） |

**⇒ 这是本模块唯一的「活缺陷」**：其余各条都是文档/注释面，只有这条会在下一批实施时真的出错。

### D-3 ★★ 关系表的**客体面被窄化**，5 条跨域方向表达不了（裁定已判死，代码未动）

| 项 | 内容 |
|---|---|
| **期望** | 按裁定：「关系表眼里**没有域**，只有『在跑的运行态』」——客体 = **该实体上在跑的全部运行态**（不分 Buff / Skill） |
| **实际** | `ITcsStateLiveQuery` 的语义是**单向只读状态域**（`TcsStateLiveQuery.h:25-28`：「技能侧判关系表时要读**状态域**的在跑实例」）；实现方只有 `UTcsBuffSubsystem` |
| **偏差证据** | 裁定 §5 列了 **5 条缺口**（状态 ⊣ 技能 4 条 + 技能 ↔ 技能 1 条），并给了根因：「技能侧的『在跑』判据散在两处——账本 `FTcsLearnedSkillEntry::RunHandles` 与池 `SkillInstances`（**扁平池，不分实体**）；全库唯一的『按实体找在跑 run』是 `FTcsCastOps::FindActiveInstance`——**它要先有 `Entry`**」 |
| **影响面** | 用户的两个业务例证里，**例 1 能表达、例 2 不能**（「角色处于某 Buff 期间无法释放某被动技能」跑不通）。裁定明文「彼此打断是**硬性要求**」 |
| **是否已登记** | 🟡 裁定 §5 已列；`EVID-2026-10-17-relation-checker` §7 已如实登记；**台账零命中** |

**⚠ 一条必须写明的边界**：**检查器本身没错**——裁定 §7 明写「检查器与四字段：判定逻辑一字不改」。错的只是**客体词面的来源**（`TcsBuff` 单域 → 应两域合并）。⇒ 修这条**不需要动 `TcsStateRelation` 的任何一个函数**。

### D-4 ★★ 规格 `state-instance-lifecycle:197` 的定义登记口**符号已不存在**（`RegisterStateDef` → `RegisterBuffDef`）

| 项 | 内容 |
|---|---|
| **期望** | 规格写：`UTcsStateSubsystem` MUST 提供 `RegisterStateDef` / `UnregisterStateDef` / `GetRegisteredStateDef` |
| **实际** | 三个方法在 `TcsState` 里**全不存在**（实测 grep 零命中）；实际实现是 `UTcsBuffSubsystem::RegisterBuffDef` / `UnregisterBuffDef` / `GetRegisteredBuffDef`（`TcsBuffSubsystem_Definition.cpp:10/33/45`），住 `TcsBuff` |
| **偏差证据** | `openspec/specs/integration-entity/spec.md:74` **也**写 `UTcsStateSubsystem::RegisterStateDef`，`:136` 又写 `GetRegisteredStateDef`——**同一处错误在第二个能力里重复了一遍** |
| **影响面** | 按规格写代码会**编译不过**。而 `integration-entity` 是 M6 接线层的权威规格 |
| **是否已登记** | ❌ 否。**这是 `ARCH-INDEX` G-7 的同类但不同条**（G-7 记的是「五条按类发现路径已假」，本条记的是「登记口的符号名已假」） |

### D-5 ★★ 规格 `state-behavior-fragments:23` 的上下文类型**写错了模块**

| 项 | 内容 |
|---|---|
| **期望** | 规格写：`FTcsBuffBehaviorContext` 的成员是 `UTcsStateSubsystem* Subsystem` |
| **实际** | 代码是 `UTcsBuffSubsystem* Subsystem = nullptr;`（`TcsBuffBehaviorFragment.h:29`） |
| **偏差证据** | 同文件 `:23` 的注释本身就在讲「上下文**不持实例指针**」，而它持有的**门面**类型错了。`state-behavior-fragments:39` 又正确写了 `"/Script/TcsBuff.TcsBuffBehaviorFragment"` ⇒ **同一份规格里两处模块归属不一致** |
| **影响面** | 中等——按规格实现会 include 错模块 |
| **是否已登记** | ❌ 否 |

### D-6 ★ 规格 `state-def-asset:18` 的 `BaseStruct` 元数据**指向不存在的类型**

| 项 | 内容 |
|---|---|
| **期望** | 规格写：`Fragments` 的选择器经 `meta = (BaseStruct = "/Script/TcsState.TcsStateBehaviorFragment")` 收窄 |
| **实际** | 代码是 `meta = (BaseStruct = "/Script/TcsBuff.TcsBuffBehaviorFragment")`（`TcsBuffDef.h:90`）。**`TcsStateBehaviorFragment` 这个类型在全仓不存在** |
| **偏差证据** | 与 D-5 是**同一类错误的两处**：规格把「拆分前的模块名」留在了两处字面量里。**`state-behavior-fragments:39` 是对的**，`state-def-asset:18` 是错的 ⇒ 两份规格互相矛盾 |
| **影响面** | 高——`BaseStruct` 是**编辑器选择器的收窄元数据**，写错则作者在编辑器里看不到任何可选类型（或看到全部）。**且它不会被 `openspec validate` 发现**（元数据是字符串） |
| **是否已登记** | ❌ 否 |

### D-7 ★ 关系四字段**已上移基类**，而 `state-def-asset:19` 仍把它们写在 `FTcsBuffDef` 下

| 项 | 内容 |
|---|---|
| **期望** | 规格的 `FTcsBuffDef : FTcsStateDefBase` 一节逐字段列了 `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel` / `Priority` |
| **实际** | 前四个**已在 2026-10-17 上移到 `FTcsStateDefBase`**（`TcsStateDefBase.h:108-139`）；`FTcsBuffDef.h:102` 明文「本类 MUST NOT 重复声明 `RequiredTags` / `ForbiddenTags` / `TagsToBlock` / `TagsToCancel`」 |
| **偏差证据** | 代码注释比规格更准确——`TcsStateDefBase.h:81-99` 有完整的上移论证（代价、当时理由为何不成立），**而规格完全没记这件事** |
| **影响面** | 中——读者按规格会以为字段在派生类。**但 `Priority` 确实仍在派生类**（`FTcsBuffDef.h:112` / `TcsSkillDefData.h:205`），规格这一半是对的 |
| **是否已登记** | ❌ 否 |

### D-8 ★ `TcsIntegration.Build.cs` 声明了 `TcsState` 依赖，但源码**零直接使用**

| 项 | 内容 |
|---|---|
| **期望** | `Build.cs` 的依赖项 = 源码真实使用的模块 |
| **实际** | `TcsIntegration.Build.cs:28` 声明 `"TcsState"`；而 `Source/TcsIntegration/` 内对 `TcsState` 的**全部 5 处提及都是注释**（`TcsDefinitionSubsystem.h:72` / `TcsDefinitionSubsystem.cpp:224` / `TcsDefinitionSubsystem_State.cpp:16` / `Build.cs:24` / `Build.cs:28` 自身） |
| **偏差证据** | `TcsDefinitionSubsystem.h:72` 的注释写「本类 → `TcsState` 的登记口」——**该描述已过期**：登记口是 `UTcsBuffSubsystem::RegisterBuffDef`（`TcsDefinitionSubsystem.cpp:230`），住 `TcsBuff` |
| **影响面** | 低（多一条编译依赖），但**注释是误导性的**——它会让读者以为登记口在共享层 |
| **是否已登记** | ❌ 否 |

### D-9 ☆ 基类注释里有两处**过时符号名**（`FSkillDef` / `SPEC-05-skill`）

| 项 | 内容 |
|---|---|
| **实际** | `TcsStateDefBase.h:21` 与 `:24` 写「`FSkillDef`（TcsSkill，R6）」，而实际类型名是 **`FTcsSkillDefData`**（全仓 `FSkillDef` 零命中）。`TcsBuffDef.h:23` 同样写 `FSkillDef` |
| **性质** | 与 `ARCH-04` 的 D-11（注释里的过时引用）**同类**，属低危但**会在 grep 时误导**——搜 `FSkillDef` 会得到「有注释、无定义」的假象 |
| **是否已登记** | ❌ 否 |

### D-10 ★★ 「访问者返回 false 即提前终止」——**契约写了、注释写了、规格有 Scenario，代码没实现**

这是本模块**唯一一处纯代码缺陷**（其余各条都在文档/注释面），也是 `ARCH-04` 的 **G-10（规格更严、代码更松）在共享层的第二例**。

| 项 | 内容 |
|---|---|
| **期望** | `TTcsEntityRuntimeBucket::ForEach` 的访问者签名是 `TFunctionRef<bool(TPayload&)>`，其注释明写「**访问者返回 false 即提前终止**」（`TcsEntityRuntimeBucket.h:186`）；规格 `state-instance-lifecycle:167` 有**专门的 Scenario**：「某实体在册三个实例，调 `ForEachState` 并在访问两个后返回 false ⇒ 访问序按槽位下标升序、**第三次访问不发生**」 |
| **实际** | `TcsEntityRuntimeBucket.h:188-194` 的实现是：`Pool.ForEach([&Visitor](TPayload& Item) { Visitor(Item); });`——**`Visitor` 的 bool 返回值被丢弃**。而池的 `ForEach` 签名是 `TFunctionRef<void(TInstanceType&)>`（`TcsInstancePool.h:247`），**池根本不支持提前终止** ⇒ **遍历恒走完全部在册槽位** |
| **偏差证据** | 三处公开头的注释都承诺了它：`TcsBuffSubsystem.h:231`「访问者返回 false 即提前终止」、`TcsSkillSubsystem.h:230` 同句、`TcsSkillOps.h:145` 同句、`TcsEntityRuntimeBucket.h:186` 同句。**四处注释 + 一个 Scenario 全部落空** |
| **影响面** | ① **性能**：`TcsBuffOps_Stack.cpp:168`（找同组成员）、`TcsBuffOps_Relation.cpp:110`（阻断扫描）、`TcsSkillOps.cpp:58`（找既有条目）三处都以为命中即停，实际全扫；② **读数**：`TcsBuffOps_Relation.cpp:110` 的 `BlockerTag` 在 `bBlocked = true` 之后**仍会被后续命中覆写** ⇒ 日志里的「阻断者」可能是**最后一个**而非第一个；③ **契约**：规格的 Scenario **今天必然失败**（除非在册实例恰好 ≤ 2 个） |
| **是否已登记** | ❌ 否 |
| **修法** | 池侧加 `ForEachUntil(TFunctionRef<bool(TInstanceType&)>)`（或让现有 `ForEach` 支持 bool 返回），桶侧转发返回值。**注意 `ForEachForGC` 必须保持 `void`**（GC 期 MUST 纯读、MUST 走完全部） |

**为什么这条一直没被发现**：三个调用点**功能上都仍然正确**——`TcsBuffOps_Stack` 的组模型保证「组内至多一条实例」（所以覆写不会发生）、`TcsSkillOps` 的同 `DefTag` 条目也至多一条。**⇒ 唯一的可观测症状是性能与日志读数，而两者都不进任何断言。** 这正是「契约未兑现但不报错」的典型形态。

**⚠ 一条必须同时记录的边界**：`ForEachState` 的**遍历序**是对的（池按 `SlotIndex` 升序、凭代际奇偶跳过空闲槽，`TcsInstancePool.h:249-258`），**只是不会提前停**。⇒ 规格 Scenario 的前半句（「访问序按槽位下标升序」）通过，后半句（「第三次访问不发生」）失败。

### D-11 ☆ 决策结果类型有**两份逐字同构的定义**，其中共享层那份**零消费者**

| 项 | 内容 |
|---|---|
| **期望** | `FTcsEntityAdmission` 是「统一重申请决策接口」，规格 `entity-runtime-mechanism:152` 有 Scenario「**决策只算不写**：接口只算不写（返回目标层数与档位），由调用方写入实例」 |
| **实际** | 共享层定义了 `ETcsEntityAdmissionKind` + `FTcsEntityAdmissionDecision`（`TcsEntityAdmission.h:73/91`）；而**实际消费的是域侧的两份**：`TcsBuff` 用 `ETcsBuffStackDecisionKind` + `FTcsBuffStackDecision`（`TcsBuffStackFragment.h:45/62`），`TcsSkill` 用 `ETcsEntityAdmissionKind` 但**自己拼装、不构造 `FTcsEntityAdmissionDecision`**（`TcsCastOps.cpp:273-278` 是一个局部变量 `AdmissionKind`） |
| **偏差证据** | 实测 grep：`FTcsEntityAdmissionDecision` **全仓只有 1 处命中 = 它自己的定义**（零消费者）。而 `ETcsBuffStackDecisionKind` 与 `ETcsEntityAdmissionKind` 是**五个枚举值逐字相同**的两份（`NewInstance/Refresh/Stack/Replace/Reject`，注释也几乎逐字相同） |
| **影响面** | 规格的 Scenario「决策只算不写」**没有可指的代码对象**——真正「只算不写」的是 `ResolveStackDecision`（返回 `FTcsBuffStackDecision`，域侧类型）。⇒ 要么把决策结果类型也上提（但那会逼 `TcsBuff` 丢掉 `ExistingHandle` 字段，它今天在用），要么承认**决策结果类型本就该住域侧**、规格改述 |
| **是否已登记** | ❌ 否 |

**这条与 D-1 同源**：`PLN-REFACTOR-1` 上提了**决策接口**（三方法）却没上提**决策结果**，于是共享层留了一个「形状对、无人用」的类型。**它属 `ARCH-03`/`ARCH-04` 反复出现的「零消费者不预建」违反**。

### D-12 ☆ 三个「零消费者」的桶 API，其中 `FindByPredicate` 的注释与实现**不一致**

| 项 | 内容 |
|---|---|
| **实测** | `TTcsEntityRuntimeBucket` 的公开口里，`ContainsEntity`（0 处）、`NumBuckets`（0 处）、`IsValidHandle`（0 处）、`FindByPredicate`（**只有 2 处，且都在 `TcsSkill`**：`TcsCastOps.cpp:150/331`） |
| **性质** | `ContainsEntity` / `NumBuckets` / `IsValidHandle` 是**观测/便利口**，按「零消费者不预建」本不该建。`FindByPredicate` 有真实消费者，**但它与 `ForEach` 同样不提前终止**（内部用 `ForEach` 实现，见 `TcsEntityRuntimeBucket.h:167-182`）⇒ **D-10 的同款问题**：注释写「先命中先返回」，实现上虽然只保留第一个命中（`if (Predicate) { Found = &Item; return false; }` 但返回值被丢弃）**结果仍然正确**，只是继续扫完 |
| **影响面** | 低。**但它是 D-10 的第三个受害者**，一并修更省事 |
| **是否已登记** | ❌ 否 |

### 债务小结

| # | 债务 | 严重度 | 性质 | 修法 |
|---|---|---|---|---|
| D-1 | 规格无对象（7 份讲 `TcsBuff`） | ★★★ | **权威层缺覆盖** | ✅ **已闭合 2026-10-24**（改名 6 份 + 拆 2 份 + 契约并入 `entity-runtime-mechanism`，提交 `c4511e9`） |
| D-2 | `SetLiveQuery` 单槽位无检测 | ★★ | **活缺陷** | 改多槽位 + 冲突报错（裁定已给形状） |
| D-3 | 客体面窄化为单域 | ★★ | **活缺陷**（功能缺失） | 补技能侧「按实体枚举 run」口 + 两域注册 |
| D-4 | `RegisterStateDef` 符号已不存在 | ★★ | 规格说错 | 改规格 2 处（含 `integration-entity` 2 处） |
| D-5 | 上下文门面类型写错模块 | ★★ | 规格说错 | 改规格 1 处 |
| D-6 | `BaseStruct` 指向不存在的类型 | ★ | 规格说错（**工具查不出**） | 改规格 1 处 |
| D-7 | 四字段已上移基类未回写 | ★ | 规格滞后 | 改规格 1 处 |
| D-8 | `TcsIntegration` 的 `TcsState` 依赖空转 | ★ | 依赖残留 + 注释误导 | 删依赖 + 改注释 |
| D-9 | 基类注释用旧类型名 | ☆ | 注释过时 | 改 3 处注释 |
| **D-10** | **`ForEach` 提前终止契约未实现** | **★★** | **纯代码缺陷**（规格有 Scenario） | 池加 `ForEachUntil`，桶转发返回值 |
| D-11 | 决策结果类型两份同构、共享层那份零消费者 | ☆ | 零消费者预建 | 或上提、或承认住域侧并改述规格 |
| D-12 | 三个零消费者的桶 API | ☆ | 零消费者预建 | 删，或随 D-10 一并处理 |

**分布**：★★★ 1 条 / ★★ 5 条 / ★ 4 条 / ☆ 2 条。**已闭合 1 条（D-1，2026-10-24）**，其余 11 条仍待裁。**性质分布**：

| 性质 | 条数 | 编号 |
|---|---|---|
| **规格说错（代码搬走、引用滞留）** | **4** | D-4 / D-5 / D-6 / D-8 |
| **规格滞后（代码做了、规格没写）** | 1 | D-7 |
| **纯代码缺陷** | 1 | **D-10** |
| **权威层缺覆盖** | 1 | D-1 ✅ **已闭合** |
| **功能缺失（活缺陷）** | 2 | D-2 / D-3 |
| 零消费者预建 | 2 | D-11 / D-12 |
| 注释过时 | 1 | D-9 |

**⇒ 4 条「规格说错」全部由 `PLN-REFACTOR-1` Task 4 拆模块造成**：符号名、模块名、类型名、字段位置四类引用全部滞留。这 4 条**合起来构成一个缺陷类**——*拆分模块时，代码搬了，而指向它的四类名字一个都没搬*。登记为 `ARCH-INDEX` **G-12**。

**⇒ 唯一的纯代码缺陷是 D-10**，而它恰恰是**四条注释 + 一个 Scenario 全都承诺了**的东西。**这是「契约写得很足、实现漏了一行」的典型**：`Visitor(Item);` 少了返回值判断。

---

## §6 未实现（设计是否还成立 + 前置条件）

> **★ 本节的读法（2026-10-17 用户裁定）**：「未实现」MUST 先分「**框架欠的**」与「**宿主该做的**」两类。业务向入口在插件内零调用是**设计常态**，MUST NOT 记为框架债务——判据与分档见 `ARCH-00` §3.3 的 **E 档**。本节各项**均已按该判据复核过**，无一项属 E 档（`U-1` 统一读口是框架机制、`U-2` 中心表 / `U-3` 级联重评 / `U-4` 排队档 / `U-5` 槽位竞争表是框架机制、`U-6` 契约默认、`U-7` 如实记录）。

### U-1 ★★★ **统一读口（TagRelationship 的多注册合并读口）整体未建**

| 项 | 内容 |
|---|---|
| **设计** | `DEC-2026-10-17-tag-relationship-ownership` §3：共享层定义契约（按实体枚举运行态视图，**不含域**）、注册口**按域一格**、两域各自注册，判定侧读到**一份合并结果** |
| **今天** | 只有**单域单向**的 `ITcsStateLiveQuery`（`TcsStateLiveQuery.h:45`），实现方只有 `UTcsBuffSubsystem` |
| **设计是否还成立** | ✅ **完全成立**，且用户已把「彼此打断」定为**硬性要求**。裁定的判据链（两域 Def 同源 `FTcsStateDefBase`、运行态同源 `FTcsStateInstance`）经实测复核无误 |
| **当初的依据今天是否仍有效** | ✅ 有效且**更强**了——`R5.5-e1` 归档时的追注已明文标出「本提案中将被推翻的部分」（`archive/2026-10-17-add-relation-table-checker/proposal.md` §「归档时的追注」），说明该推翻是**已知且已排期**的 |
| **前置条件** | ① 技能侧补「**按实体枚举在跑 run**」的口（裁定 §5 缺口 5 的根因：今天唯一的 `FTcsCastOps::FindActiveInstance` **要先有 `Entry`**）；② `SetLiveQuery` 改多槽位（§5 D-2）；~~③ **§4.3 的匹配面并集读法（甲/乙）MUST 显式拍板**~~ ⇒ **✅ 已于 2026-10-24 拍板为「甲」**（并集 + **一次**层级匹配；乙 = 拆两个字段，被否） |

**⇒ ③ 已裁，U-1 从此是纯工程量。** 三条前置的真实分布变了：**③ 已解**（甲，且其**匹配面并集本身——主体面与客体面同口径——已由 `2297357` 落地并 PIE 25/25 验证**），**①② 是编码工作量**。实现形状由裁定 §3.1 给定（契约住 `TcsState`、注册口按域一格、判定侧读一份合并结果），不需要再做设计判断。

### U-2 ★★ **中心关系表 `UDataAsset`（`e2`）未建**——关系四字段今天只能平铺在 Def 上

| 项 | 内容 |
|---|---|
| **设计** | `DEC-2026-10-13-relation-row-shape`：一行 = 一个主体词 + 四个容器，载体 = **中心 `UDataAsset`**（吸收 Lyra 的 `ULyraAbilityTagRelationshipMapping`）。兑现旧 `SPEC-02-states` §3.4 的 `D3-16`「统一 DataTable/DataAsset 行」 |
| **今天** | `FTcsRelationRow` **类型已落地**（`TcsStateRelation.h:41`），但**没有任何中心资产**——实际走的是「Def 上的四个平铺数组 → 域侧合成行视图」（`TcsBuffOps_Relation.cpp:27` 的 `MakeSelfRelationRow`）。实测 grep：`Source/` 内 `RelationRow` 类资产**零命中** |
| **设计是否还成立** | 🟡 **部分动摇**——2026-10-17 裁定把关系表判定对象扩为「全部运行态」后，**中心资产的主体词可以是父词覆盖一族**这件事仍是它唯一不可替代的价值（Def 平铺数组的主体恒为本定义自己）。⇒ 成立，但**优先级下降**（跨域缺口比「一族覆盖」更急） |
| **前置条件** | 先完成 U-1（统一读口），否则中心表也只是「一个域读它」 |

### U-3 ★★ **移除级联重评（`e3`）未建**

| 项 | 内容 |
|---|---|
| **设计** | 旧 `SPEC-02-states` §3.4：「**任何状态移除后对依赖它的存活状态做事件驱动重评**（脏标记，批量移除合并一次）」 |
| **今天** | 实测 grep：`级联重评` / `重评` / `Reevaluate` 全仓**零命中**。关系门只在 **apply 时**判一次（`TcsBuffOps.cpp:47`）⇒ 「A 依赖 B 存在，B 被移除后 A 应失效」这条**没有实现** |
| **设计是否还成立** | ✅ 成立。且它今天**比 U-2 更急**——`RequiredTags` 已落地且有消费者（`CheckRequired`），语义是「必须存在」，而 B 消失后 A 仍然在跑，**这是可观测的语义漏洞** |
| **前置条件** | 需要「移除后重扫该实体全部实例」的挂点。注意 `UnregisterEntity` 逐条移除时**不能**每条都重扫（O(n²)）⇒ 必须按「批量合并一次」的脏标记实现 |
| **⚠ 一处与 D-10 的关联** | 重评需要「遍历该实体实例并判定」，而 `ForEach` 不提前终止（D-10）⇒ **建议先修 D-10 再落 U-3**，否则重评扫描会白跑 |

### U-4 ★ `ApplyState` 的**排队档**（`Requires` 未满足）未建

| 项 | 内容 |
|---|---|
| **设计** | 旧 `SPEC-02-states` §3.4：「执行时机：apply 时校验（`Blocks` 拒绝 / `Requires` 未满足**拒绝或排队**）」；裁决③（`DEC-2026-10-13-relation-semantics`）明文「**`Requires` 未满足的『排队』档本轮不落**（零消费者不预建）」 |
| **今天** | 只有**拒绝**一档（`CheckRequired` 返回 false ⇒ `EBAR_Rejected`）。排队**不存在** |
| **设计是否还成立** | 🟡 **需要重新核对**——「排队」在 `RequiredTags` 已落地的今天语义更清晰了（等前置出现后自动施加），但它的**生命周期语义有未定处**：排队条目是否占槽位？前置永远不出现时何时丢弃？超时？⇒ **不是纯实现缺口，是设计未收口** |
| **前置条件** | 先裁「排队条目的生命周期与容量语义」 |

### U-5 ★ **槽位竞争表（`e4`）与 `Priority` 未建**

| 项 | 内容 |
|---|---|
| **设计** | `DEC-2026-10-13-relation-semantics` 裁决①：`Priority` **移出关系字段族**，归**槽位竞争表**（主体 = 施法运行 / 动作状态）；`DEC-2026-08-31-state-relation` §4 的「槽位竞争（`PriorityOnly` + `PreemptionPolicy`）」 |
| **今天** | `Priority` 字段**两域各一份、零消费者**（`FTcsBuffDef.h:112` / `TcsSkillDefData.h:205`）。槽位竞争表**完全不存在** |
| **设计是否还成立** | 🟡 **部分存疑**——旧 `SPEC-02-states` §3.4 自述该节的依据「`TCS 报告 06` **库外不可复核**」⇒ 「`PriorityOnly` + `PreemptionPolicy` 已验证」**这三字不成立**（`DEC-2026-10-13-relation-semantics` 裁决①已就地更正） |
| **前置条件** | 按裁决：「语义须由 `PLN-R5.5` Task 4 **从判据重新推导**，`PreemptionPolicy` 不建字段」⇒ **先推导，不是先实现** |

### U-6 ★ **`FTcsParamSnapshotEntry::SourceRef` 零消费者**（有意留白，但已 3 年未变）

| 项 | 内容 |
|---|---|
| **设计** | 旧 `SPEC-02-states` §3.6（PV-3）：「已解析规范值 + **源引用位**双字段——Debug 可见取值来源，**Live 化（已承诺基建）零结构迁移**」 |
| **今天** | **零消费者**（实测：`SourceRef` 的写入点 2 处 `TcsBuffOps_Snapshot.cpp:56` / `TcsCastOps_Snapshot.cpp:49`，读取点 **0 处**，只有 GC 补引用 2 处 `AddStructReferencedObjects`） |
| **设计是否还成立** | ✅ 成立，且**保留成本极低**（一个可复制的 `FInstancedStruct`）。类型注释自己就写明了判据：「删掉它会与设计冲突，而保留的成本只是一个可复制的 `FInstancedStruct`。判据见 `docs-convention` §6.4」 |
| **⇒ 处置建议** | **不是债，是有意留白**。但按「零消费者不预建」它确实违反——**这是两条判据的冲突点**，值得在 `ARCH-INDEX` 记一笔（§6 G-13） |

### U-7 ☆ `TcsState` 的**模块类 `FTcsStateModule` 是空体**，零注册

| 项 | 内容 |
|---|---|
| **实际** | `TcsStateModule.h:11` 是 `class FTcsStateModule : public IModuleInterface {};`——**完全空的类体**；`TcsStateModule.cpp` 只有 `IMPLEMENT_MODULE` |
| **性质** | **不是债**。本模块确实没有需要在模块加载期做的事（门面是 `UWorldSubsystem`，契约键是 `FNativeGameplayTag` 静态初始化）。**如实记录以便与「模块类有内容」的其它模块区分** |

---

## §7 文档资产对照

| 层 | 载体 | 行数 | 与本模块的关系 |
|---|---|---|---|
| **可执行需求** | 重切前：`openspec/specs/` 10 个 `state-*` 能力<br>**重切后（2026-10-24）**：本模块 **4 份**（`entity-runtime-mechanism` 261 行 / `state-relation-check` 118 / `state-def-asset` 89 / `state-param-snapshot` 58）+ `TcsBuff` **7 份** `buff-*` | 1 628<br>→ 本模块 **526** / Buff **1 225** | 重切前 **7 份主语是 `TcsBuff`**（§1.2）；**重切后挂 `state-*` 名字的全部指向本模块**，无一份谎报主语。详见 §1.2 与 `openspec/changes/refactor-split-state-specs-by-module/` |
| **历史理由** | `Documents/combat-system-design/spec/03-module-states.md`（`SPEC-02-states`） | **390** | **质量意外地好**：§3.1 与 §6 都有 2026-10-15 的 `PLN-REFACTOR-1` **就地改述**（不是重写，是「原文保留 + 追注现行读法」），§12.13 有完整的收束回写。**这份旧规格比 `openspec` 更准确地描述了今天的模块归属** |
| 历史理由 | `decisions/dec-2026-10-13-state-module-positioning.md` | — | 拆分 `TcsBuff` 的裁决 |
| 历史理由 | `decisions/dec-2026-10-17-tag-relationship-ownership.md` | 203 | **当前 TagRelationship 的权威**，明写「实现状态 = 部分落地」 |
| 证据 | `evidence/2026-10-17-relation-checker.md` | — | §7「如实登记的偏差与未落面」、§8「契约部分将被推翻」 |
| 计划 | `plans/plan-r5.5-relation-table.md` | — | `e1`~`e4` 批次表；**状态行仍写 `ACTIVE` 且「实施未开工」**（`e1` 已完成，该行过期——见 `ARCH-INDEX` 已登记的同类问题） |

**⇒ 一条反常的结论（审计当日的状态，2026-10-24 已部分反转）**：**当时旧文档族对 `TcsState` 的描述比 `openspec` 更准确**。

`SPEC-02-states` §12.13（「`PLN-REFACTOR-1` 收束回写」）逐项列了「模块归属 / 依赖方向 / 主线读数 / 本节不覆盖的」，且 §3.1 / §6 的追注用的是**「原文保留 + 就地改述」**——即它既保住了历史现场、又给出了现行读法。而 `openspec` 侧**没有任何一次等价的重切**。

**⇒ 这推翻了 `ARCH-INDEX` §1.2 的一条隐含假设**：那里说「旧文档族含大量已过期内容、新档案描述现状、`openspec` 是最高权威」。但对本模块而言，**审计当日的权威顺序是反的**：`openspec` 最不准（7/10 份主语错了）、旧 `SPEC-02` 最准（有收束回写）、代码注释最准（`TcsStateDefBase.h` 的上移论证规格里根本没有）。

> **★ 2026-10-24 反转（MUST 读）**：`openspec` 已在 `c4511e9` 按模块重切，**上述反转不再成立**——最高权威层现在如实描述本模块（4 份 / 18 需求 / 54 场景）。**但这次重切暴露了「反转」真正的原因**：不是 `openspec` 天生不可靠，而是**它缺少「模块拆分时同步重切」这一步**。旧 `SPEC-02` 之所以更准，恰恰因为它做了「原文保留 + 就地改述」的收束回写——**那正是 `openspec` 缺的那件事**。
> ⇒ **可复用判据**：判断一份规格是否可靠，看的不是它属于哪一层，而是**它的最后一次结构性代码变更之后，是否有过等价的规格重切**。

---

## §8 审核记录

| 项 | 内容 |
|---|---|
| 审核日期 | 2026-10-17 |
| 读取范围 | **全部 28 个源文件 / 2 781 行**（15 `.h` + 11 `.cpp` + `TcsStateModule.h/.cpp` + `Build.cs`） |
| 读取的规格 | 10 个 `state-*` 能力 **1 628 行 / 51 需求 / 187 场景** + `integration-entity`（相关段）+ `gameplay-tag-governance`（根表）+ `param-value` |
| 读取的旧文档 | `SPEC-02-states` **390 行**（含 §12.13 收束回写）+ `DEC-2026-10-13-state-module-positioning` + `DEC-2026-10-17-tag-relationship-ownership` **203 行** + `EVID-2026-10-17-relation-checker` 目录 |
| 跨模块实测 | `Build.cs` 依赖图（11 个模块逐个读数组）；`TcsState` 符号在 `TcsBuff` / `TcsSkill` / `TcsIntegration` / `TcsEffect` / `TcsDamage` / `TcsTargeting` 的命中计数 |
| 审核方法 | 按 `ARCH-INDEX` §5 的五步；**双向查**（G-10）：既查「代码做了规格没写」，也查「规格写了代码没做」 |
| 双向查的收获 | **「规格写了代码没做」2 条**（D-10 提前终止 / D-11 决策结果类型）；**「代码做了规格没写」1 条**（D-7 四字段上移基类）。⇒ 与 `ARCH-04`（4 条单向）不同，**本模块的债主要在第三个方向：代码搬走了、规格没搬**（D-4 / D-5 / D-6 三条） |

### 8.1 本次审核发现的**第三个缺陷方向**（重要）

`ARCH-INDEX` 的 G-10 记的是两个方向（规格更严 / 规格漏写）。本模块审出了**第三个**：

| 方向 | 含义 | 本模块实例 | 已登记为 |
|---|---|---|---|
| ① 规格更严、代码更松 | 规格写 MUST，代码没做 | D-10（提前终止）/ D-11（决策结果） | G-10 |
| ② 规格漏写、代码在做 | 代码有、规格无 | D-7（四字段上移基类） | G-10 |
| **③ 代码搬走了、指向它的东西没搬** | 模块拆分后，**符号名 / 模块名 / 类型名 / 字段位置** 四类引用全部滞留 | **D-4 / D-5 / D-6 / D-8**（4 条） | **G-12（新）** |

**⇒ 方向③是本仓的特有缺陷类**，因为本仓做过一次大规模模块拆分（`PLN-REFACTOR-1` Task 4）且**没有等价的重切规格**。**后续模块审核 MUST 加查这个方向**：凡经历过拆分的模块，逐个核对「规格里的符号名是否还指向它」。

---

## §9 结论

### 9.1 三句话

1. **代码质量高，且有一处是全仓最好的设计**：门面被削到只剩 3 个方法（判据「两域都在代码级用它」可执行、且有自我纠正记录），桶模板的进程级发号器判据有实测缺陷支撑，句柄单点连「被否决的绕过方案」都留了引擎源码证据。
2. **规格侧曾是本仓最差的一处，已于 2026-10-24 重切**：审核当时 10 份 `state-*` 能力里 **7 份整篇在讲 `TcsBuff`**，`openspec` 里**没有一个能力回答「`TcsState` 今天剩什么」**。重切后挂 `state-*` 名字的**全部指向本模块**，回答本模块契约的能力共 **4 份 / 18 需求 / 54 场景**（D-1 已闭合，提交 `c4511e9`）。
3. **唯一的活缺陷是 `SetLiveQuery` 单槽位**（D-2）——它今天不显形只因为只有一个域注册，而 2026-10-17 的裁定要求两域都注册。**其余 11 条债全部在文档/注释/契约面。**

### 9.2 一句值得记住的话

> **本模块的债几乎全部来自「模块被拆小了，而指向它的名字没有跟着变小」**——门面从 431 行削到 120 行是**做对了**，但 `openspec` 的 7 份规格、`RegisterStateDef` 的符号名、`BaseStruct` 的模块路径、`TcsIntegration` 的依赖声明，**全部还停在拆分前**。
>
> **代码是新的，地图是旧的，而地图比代码更难改——因为它有四份副本（openspec / 旧 SPEC / 注释 / Build.cs）。**
>
> **★ 2026-10-24 追记（四份副本里最大的一份已换新）**：`openspec` 这一份已按模块重切——**但换地图的代价远高于预期**。改名 6 份需处理 18 处交叉引用与 1 处「不可随改名的引用」（`EVID-<date>-state-*` 证据文档路径指向冻结层）；拆 2 份混合篇要逐条判主语、切原文、补场景、再回改 6 处引用指向；**而归档器本身有解析限制**（需求正文含表格的能力无法被 `retire_capabilities` 退休，6 份里 2 份失败）⇒ 最终**手工改名、不走 `archive`**。
> **⇒ 可执行的教训**：**模块拆分与规格重切 MUST 同一批次完成**。当时省下的那一步，今天要花三刀、47 份能力全量核对、外加一次工具限制的绕行来还。

### 9.3 与 `ARCH-04` 的对照

| 维度 | `ARCH-04`（`TcsAttribute`） | `ARCH-05`（`TcsState`） |
|---|---|---|
| 规格质量 | **全仓最好**（752 行 / 27 需求 / 86 场景） | 审核时**全仓最差**（主语错了 7/10）→ **2026-10-24 重切后本模块契约已完整**（4 份 / 18 需求 / 54 场景） |
| 债的主要方向 | ① 规格更严、代码更松（4 条） | ③ 代码搬走、引用滞留（4 条） |
| 代码质量 | 高（折叠核单份、`CachedCurrent` 单写者） | 高（门面 3 方法、桶判据有实测支撑） |
| 唯一活缺陷 | 无（4 条都是规格未兑现，调用方都不依赖） | **`SetLiveQuery` 单槽位**（会在下一批实施时真的出错） |

**⇒ 两个模块的「代码好、文档差」是同一件事的两面**：`TcsAttribute` 的规格写得好但没落地（实现欠账），`TcsState` 的代码做得好但规格没跟上（文档欠账）。**本仓在「规格与代码两张皮」这条上，两个方向都走到了极端。**
