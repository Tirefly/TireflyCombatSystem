# entity-runtime-mechanism Specification

## Purpose

定义 `TcsState`（共享层）、`TcsBuff` 与 `TcsSkill` **共用的运行态机制**——把此前"各写一份"的句柄判据、per-entity 桶与注册表、GC 引用收集口、门面清理口与重入广播守卫、以及"重申请"决策空间，收敛为**一份模板化实现 + 一处语义单点**，供两域各持自己的载荷类型当场复用。

**本能力的边界（MUST 读）**：

- **它只定机制，定字段归基底化那一轮**——本能力定的是**机制**（句柄语义 / 桶 / GC 口 / 清理口 / 守卫 / 决策接口）；而**记录字段的基底化与派生形状**由 `PLN-REFACTOR-1` Task 2/3 落地（**已完成**）：基底 `FTcsStateInstance`（6 公共字段）+ 两个派生 `FTcsBuffInstance` / `FTcsSkillInstance`；`FTcsLearnedSkillEntry` 仍是**独立记录**（账本条目，不参与基底化）。**三者仍各自完整**。**2026-10-24 订正**：原文写"本能力**不对其字段作规定**"——该边界在基底被搬入本能力后**已不再成立**：基底的 6 字段契约由本能力的「共享层运行态基底 `FTcsStateInstance`」需求规定，派生结构的**域内**字段仍归各域能力（`buff-instance-lifecycle` / `skill-cast-runtime`）。
- **它是"共享层"定位的落地**：`TcsState` 不再只承担状态域运行态，同时承担两域共用的定义族与运行机制（该定位裁决见 `DEC-2026-10-13-state-module-positioning`）。**自 Task 4 起 Buff 专属语义已拆入 `TcsBuff`**，故本能力所称的"状态侧"在机制面上指**共享层 + Buff 域**的整体（句柄 / 桶 / 守卫的实现住共享层，载荷类型住 `TcsBuff`）。
- **三个句柄类型 MUST 保持独立**（本能力含一条**否定式要求**）——合并会移除"跨桶误传编译期失败"这道现存护栏；收敛的是**逻辑**，不是**声明**。
- **载体是 `TTcsInstancePool`**，且 per-entity 桶 MUST 注入**进程级**代际发号器（各桶各自从代际 1 起发，会让跨桶扫描**命中错实体**——本仓 2026-10-05 实测过的缺陷）。

## Requirements

### Requirement: 统一运行态句柄判据与转换（MUST NOT 合并类型）

三个运行态句柄类型（`FTcsBuffInstanceHandle` / `FTcsSkillInstanceHandle` / `FTcsSkillEntryHandle`）MUST **保持为三个各自反射的 `USTRUCT(BlueprintType)`**，但它们的**判据、相等比较、哈希与池位模式转换的语义 MUST 收敛到一处**（非反射公共件，如 `FTcsInstanceHandleOps` 静态函数族），三个薄型各自委托它。

- **MUST NOT 合并为一个类型**（本条是**否定式要求**，理由见下）；
- **MUST NOT 让各处自行拼装句柄**——池位模式转换（`static_cast` 保证 `-1` ↔ `0xFFFFFFFF`）的语义 MUST 单点定义；
- **`IsValid()` 判据 MUST 统一为最严**：`Index >= 0 && Generation > 0`。**被统一的真不一致**：`FTcsSkillInstanceHandle` 现判 `Index != -1`（**不看 `Generation`**），而另两者判 `Index >= 0 && Generation > 0`；
- **行为中性（MUST 记录，防误读为行为变更）**：对**良构句柄**（`Allocate` 产物 `Generation >= 1`；默认构造产物 `Index = -1`）**两套判据结论逐例相同**；仅"手工构造 `Index >= 0` 且 `Generation == 0`"这一**从不出现的形态**由"有效"变为"无效"；
- **受影响的既有注释 MUST 改述**：`FTcsSkillInstanceHandle::IsValid` 原注释自述"**因其代际由池在解析/释放时校验，本结构只判『是否曾被赋值』**"——统一后该注释 MUST 说明为何现在连 `Generation` 一起判（**已落地**：现注释为"句柄有效性（**判据委托 `FTcsInstanceHandleOps`**——三句柄语义单点，本型不自行实现）"）。

**为什么 MUST NOT 合并为一个类型（判据三条，MUST 写进头注释）**：

1. **会移除一道现存的编译期护栏**：各型持有**幻影标签**（`FTcsBuffInstanceTag` / `FTcsSkillInstanceTag` / …），使"把状态句柄传给施法运行 API"在**编译期失败**。合并后该误用降为**运行期查不到**——正是本仓反复拦的"静默错配 + 误导性症状"形态。`instance-handle-pool` 能力为此**立了两条**判据（「不同池的句柄类型不可互换」+ Scenario「句柄类型隔离」→ **编译失败**）。
2. **单点转换必然退化**：一个类型的 `GetInner()` **只能返回一个** `TTcsInstanceHandle<Tag>`，无法服务三个标签 ⇒ 要么拆成三个方法（等于没合并）、要么调用点各自 `static_cast`（违反"唯一转换点"）。
3. **三个反射身份本就必须各自声明**：它们是三份不同的 `USTRUCT` 反射身份，**不是可消除的重复**；实测三型各自的**代码**约 15 行（其余皆为注释）⇒ 真正的重复是**逻辑**，收敛逻辑即达成 DRY。

**★ 同时否决的绕过方案（"一个宏生成三个类型"）MUST 记录以免后人重试**：【引擎源码】实测 —— 引擎 `Runtime/` 内**宏展开出 `USTRUCT(` 的用例零命中**；引擎自带的四个 wrapper-struct 宏（`DEFINE_INTERPCURVE_WRAPPER_STRUCT` 等）**全部只生成普通 C++ struct**（无 `USTRUCT`、无 `GENERATED_BODY`）；`USTRUCT(...)` 在 `ObjectMacros.h` 里被定义成**空**（"给编译器看的占位"），**不是** UHT 的展开入口。**且存在结构性障碍**：`ObjectMacros.h:GENERATED_BODY` = `BODY_MACRO_COMBINE(CURRENT_FILE_ID,_,__LINE__,_GENERATED_BODY)` ⇒ **按 `__LINE__` 拼符号名**，而 UHT **按扫描到的行**生成对应符号 ⇒ 外层宏包裹后**行号语义是否仍对齐，本机无法确认**。⇒ 依本仓既有纪律（同类"本机不可核实"的依赖一律去掉），**MUST NOT** 把本能力建在未核实的 UHT 行为上。

#### Scenario: 三型仍是三个类型（编译期隔离不丢）

- **WHEN** 把 `FTcsBuffInstanceHandle` 传给期望 `FTcsSkillInstanceHandle` 的 API
- **THEN** **编译失败**（幻影标签与类型隔离保留；MUST NOT 因"统一判据"而变成运行期查不到）

#### Scenario: 判据统一后良构句柄结论不变（行为中性对照）

- **WHEN** 对 `Allocate` 产物与默认构造产物分别按旧判据与新判据（`Index >= 0 && Generation > 0`）求值
- **THEN** 两个产物的结论逐一相同（⇒ 本变更对良构句柄**无行为差异**）

#### Scenario: 判据与转换语义单点

- **WHEN** 检查三个句柄类型的 `IsValid` / `operator==` / `GetTypeHash` / 池位模式转换
- **THEN** 三者的**语义取自同一处**（公共件），MUST NOT 存在三份各自实现的判据；且池位模式转换 MUST NOT 在调用点自行 `static_cast` 拼装

#### Scenario: 未分配句柄无效

- **WHEN** 取一个默认构造的任一句柄并调 `IsValid`
- **THEN** 返回 false（`Index` 为负或 `Generation` 为 0）

#### Scenario: 分配出的句柄可直接判有效

- **WHEN** 取任一门面分配出的句柄（`Index` 可能为 0）
- **THEN** `IsValid` 返回 true（合法性判据是"`Generation` 非 0"，不是"下标非 0"——下标 0 是合法槽位）

### Requirement: 统一 per-entity 桶与注册表

`TcsState`（共享层）MUST 提供**模板化**的 per-entity 桶与注册表，供状态与技能两域共用（**自 Task 4 起"状态域"的载荷类型住 `TcsBuff`，模板与注册表本身仍住共享层**）：

- `TTcsEntityRuntimeBucket<TPayload>`——**载体 MUST 为 `TTcsInstancePool`**（`TcsCore` 既有代际池，复用其分配 / 释放 / 解析 / 代际 / 稳定序遍历 / GC 只读遍历 / 重置）；MUST NOT 各域各写一份槽位 + 代际 + 空闲栈（此前实测**三种变体**：状态手写桶、技能池、技能账本手写桶）；
  - **★ 代际发号 MUST 可注入，且 per-entity 桶 MUST 注入进程级发号器**（本要求是 2026-10-14 实施期发现的一处**提案内部矛盾**的闭合，MUST 保留）：池原先**只能**"每池自增"（新槽从 1 起、复用 +1）⇒ 若桶直接用池，**各桶会各从代际 1 起发**；而桶是 **per-entity** 的、句柄**不带实体段** ⇒ 两个实体各自的首实例会得到**相同的 `(Index, Generation)`**，于是"跨桶扫描式解析"（`FTcsBuffOps::Find` 即此类，`GetState(Handle)` 无实体入参）**命中错实体**——正是本仓 **2026-10-05 实测并修复过的缺陷**。⇒ 池 MUST 提供**代际发号策略模板参**（默认档 = 池内自增 ⇒ 其余四个既有池使用者**零行为差异**），per-entity 桶注入**进程级**发号器（不复用正奇数 / 释放置偶 / 跨世界不复位 / `int32` 耗尽 `Fatal`）；
  - **状态侧手写桶的真因 MUST 记为准（订正一处曾写错的表述）**：其自述理由**不是**"一个池 = 一个句柄空间"，而是 **`Generation` 由本模块出线发号器统一分配不复用的奇数号**（`TcsStateRegistry.h` 原文两句并列）⇒ 该理由**在机制合并后依然成立**，故合并的落法必须是"池 + 可注入发号"，**MUST NOT** 读成"理由不再成立、直接用池即可"；
- `FTcsEntityRuntimeRegistry<TPayload>`——`TMap<FTcsCombatEntityHandle, TUniquePtr<Bucket>>`；**`TUniquePtr` 间接层 MUST 保留**（`TMap` 扩容会搬移元素，按值存放会使桶地址失效 ⇒ 遍历中为别的实体建桶不安全）；
- **脏句柄一律拒绝 + `Warning`**（不 `ensure`——这是**时序竞态**语义，与"配置错误"分开）：代际失配 / 下标越界 / 实体未注册。拒绝 MUST NOT 影响同桶其它实例；
- `UnregisterEntity` 删桶时 MUST 回收该实体全部槽位（否则池占用统计残留）。

#### Scenario: 桶地址不随登记表增长失效

- **WHEN** 持续为不同实体建桶与实例（`TMap` 发生多次扩容）后，用早先实体的句柄查询
- **THEN** 查询照常命中（桶经 `TUniquePtr` 间接持有，登记表扩容不搬移桶地址）

#### Scenario: 脏句柄被拒绝且不影响他人

- **WHEN** 对同一实体建两个实例后，用代际失配的旧句柄调移除
- **THEN** 拒绝该调用并留 Warning，两个实例仍在册（无实例被误移除），不出现 ensure

#### Scenario: 实体销毁后桶被回收

- **WHEN** 对某实体建若干实例后调 `UnregisterEntity`
- **THEN** 该实体全部实例被移除、桶被删除，且用其旧句柄查询一律被拒绝

#### Scenario: 两域共用同一桶实现

- **WHEN** 检查状态侧与技能侧的桶类型
- **THEN** 两者是**同一模板的不同实例化**（`TPayload` 分别为状态记录与施法运行态），MUST NOT 存在第二份手写的槽位 / 代际 / 空闲栈逻辑

### Requirement: 统一 GC 引用收集口

实体运行态的 GC 引用收集 MUST 经**唯一入口**，且 MUST NOT 在 GC 期触发线程断言：

- 池 MUST 提供 `ForEachForGC(TFunctionRef<void(const TInstanceType&)>)`：**纯读**、**MUST NOT 断言游戏线程**（这正是它存在的理由）、判据与常规遍历**逐字相同**（按槽位 Index 升序、按代际奇偶跳过空闲槽）、MUST NOT 引入第二套"哪些槽在册"的判定；
- **它 MUST NOT 被任何常规逻辑消费**——唯一合法调用者是宿主门面的 `AddReferencedObjects`；拿它做业务遍历等于静默放弃单线程纪律；
- **为什么必需**：GC 的引用收集**不在游戏线程上**，而池的每个正常进入点都断言游戏线程 ⇒ 在 ARO 里调 `ForEach` 会**每次 GC 触发一次 `ensure(IsInGameThread())`**。该症状具隐蔽性：`ensure` 每站点每进程只报一次 ⇒ 表现为"偶尔一条红字"，且**与池空不空无关**。

#### Scenario: GC 遍历不触发线程断言

- **WHEN** 在**非游戏线程**上下文（GC 引用收集）对池调用 `ForEachForGC`，池内既有在册槽也有空闲槽
- **THEN** **不产生任何 ensure**，且回调恰好收到全部在册槽各一次（与 `ForEach` 的可见集逐项一致）；作为对照，同上下文调用 `ForEach` **MUST** 触发 `ensure(IsInGameThread())`

#### Scenario: 阴性对照（关掉补引用则存活读数失败）

- **WHEN** 临时关闭两域门面的 `AddReferencedObjects` 补引用，重跑既有 GC 存活读数
- **THEN** 存活断言**出现失败**（先例：`EVID-2026-10-08-skill-cast-runtime` 的 ARO 阴性对照 = 24 PASS / 2 FAIL、`存活=否`）。⇒ **读数不失败说明该遍历没被真正走到**，MUST 查明而非结案

### Requirement: 统一门面清理口与重入广播守卫

两域门面 MUST 共用同一套**世界反初始化清理**与**重入广播守卫**形状：

- **清理口**：`Deinitialize` MUST 全量清理且**覆盖该域全部持有者**——技能侧现状为 **7 个 owner**（运行态池 / 登记表 / 账本 / 注入的实体查询接口 / 链重定向栈（含宿主契约）/ 在飞终结标记集 / 实体级组冷却槽），状态侧为 3 个。**MUST NOT 只照其中一侧的清单**（漏清 ⇒ 跨世界残影：旧句柄在新世界被误判有效）；
- **重入守卫**：MUST 为**一份**实现（取代 `InRemovalBroadcastHandles` / `InFinishBroadcastHandles` 两份），句柄类型随统一句柄收敛；语义 = **进出广播窗口成对标记 / 解除**，使"订阅者在回调内重入终结同一句柄"返回 false 且静默（MUST NOT 重复广播、MUST NOT 重复归还槽位）。

#### Scenario: 世界反初始化确定性清空（覆盖全部 owner）

- **WHEN** 某世界建过实例后世界被反初始化
- **THEN** 该域**全部**持有者被清空（技能侧 7 项 / 状态侧 3 项逐项核对），旧句柄在新世界一律被拒绝

#### Scenario: 广播内递归移除被静默拒绝

- **WHEN** 订阅者在移除广播回调内重入终结同一句柄
- **THEN** 该重入返回 false 且静默：无重复广播、无重复归还槽位（读数 = 该事件恰好一笔）

### Requirement: 统一重申请决策接口

"同组则刷新 / 叠层，异组则新建 / 拒绝 / 替换"这一**决策空间** MUST 由**唯一接口**表达，供两域各挂自己的默认策略（取代"状态决策 Fragment + 技能内联四路分流"两份实现）：

- `FTcsEntityAdmission`（`USTRUCT(meta = (Hidden))` + 中性默认实现 + `virtual ~() = default`，**MUST NOT** 用 `= 0` 或 `PURE_VIRTUAL`——UHT 为每个 `USTRUCT` 生成 `TCppStructOps<T>`，构造路径需可默认构造）：
  - `virtual bool IsSameGroup(const Request&, const Existing&) const`——默认 = 内置"不分组"语义（同实体 + 同 `DefTag`）；
  - `virtual bool ShouldAccept(const Request&, const Existing&) const`——默认 = 恒接受；
  - `virtual int32 ResolveStacks(const Request&, const Existing&) const`——默认 = 同来源不变、异来源 +1；
- **载体纪律**：策略实例 MUST 归 **Def 资产**持有；运行态实例 MUST NOT 持有任何策略载体（值语义记录是池化与操作复制的地基）；
- **框架零具体策略**：分组词与业务判据是宿主词汇，本接口只给中性默认 + 逃逸位；
- **技能侧三档 MUST 由默认策略类表达，MUST NOT 内联在算子实现里**：`InstancePerExecution` → 恒不同组（`IsSameGroup` 恒 false ⇒ 每次新建）；`InstancePerEntity` + `bRetriggerOnActive = false` → `ShouldAccept` 拒；`+ true` → 替换。**三个默认策略类住共享层**（`FTcsEntityAdmissionAlwaysNew` / `FTcsEntityAdmissionPerEntityReject` / `FTcsEntityAdmissionPerEntityReplace`，见 `Runtime/TcsEntityAdmission.h`）——它们由**技能侧（`TcsSkill`）在门禁④消费**：若它们住 `TcsSkill`，则共享层无法引用，而它们本身不含任何域词汇（判据 = 零业务类型），故住共享层是唯一不成环的落法。

#### Scenario: 恒不同组即每次独立成实例

- **WHEN** 挂一个 `IsSameGroup` 恒假的策略并施加两次
- **THEN** 两次都建出新实例（两笔 `Applied`，在册读数 = 2）——即"每次施加独立成实例"这一语义**由字段表达而非特例代码**

#### Scenario: 不接受即拒绝且不改动实例

- **WHEN** 挂一个 `ShouldAccept` 恒假的策略并施加
- **THEN** 返回拒绝档、实例不动、无任何事件广播

#### Scenario: 技能侧不再内联分流

- **WHEN** 检查技能侧激活算子中对 `ECastInstancing` 的处理
- **THEN** MUST 经本接口的策略类完成，MUST NOT 存在内联的 if/else 分流；且 `Instancing` 的**行为读数与重构前逐字一致**（阴性对照：四态各取一次读数）

#### Scenario: 决策只算不写

- **WHEN** 检查决策接口的返回值如何生效
- **THEN** 接口**只算不写**（返回目标层数与档位），由调用方写入实例——同物化器纪律"算与写分开，读一处即全貌"

### Requirement: 共享层运行态基底 `FTcsStateInstance`

`TcsState`（共享层）MUST 提供运行态**基底** `FTcsStateInstance`，持**两域公共的 6 个字段**：

| 字段 | 为什么在基底（**唯一判据 = 两域是否同角色，MUST NOT 按名字相似判定**） |
|---|---|
| `DefTag` | 两域皆有定义身份 |
| `Entity` | 两侧现状逐字相同，注释都自述"必须自持，O(1) 定位" |
| `CascadeAnchor` | 两域同角色——"按它一次摘净"的**级联撤销锚点**（状态侧修正器与内联触发行按它挂；技能侧链挂的账本条目按它回收） |
| `ParamSnapshot` | 两侧现状逐字相同 |
| `Level` | 两侧现状逐字相同（生效瞬间冻结、运行中不追溯） |
| `ExpiryEntry` | 两侧各有一枚"当前计时项的取消锚"（状态侧 = 时值到期；技能侧 = 当前时段到期） |

**两域公共字段 MUST 住基底、MUST NOT 在派生结构重复声明**。派生结构只声明**自己的**域内字段：`FTcsBuffInstance` 的 `Handle` / `Source` / `Instigator` / `Phase` / `Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry`，施法侧 `FTcsSkillInstance` 的 `EntryHandle` / `PhaseIndex` / `ChainRunHandle` 等——**MUST NOT 进本基底**（进了就重犯本基底要修的病）。

**命名判据 MUST 写进头注释**（`TcsStateInstance.h:17`）：`State` 在本仓是**共享层**的词根，不是"Buff 专属"——它与 Def 层**完全同构**：`FTcsStateDefBase` ← `FTcsBuffDef` / `FTcsSkillDefData`，对应运行态就是 `FTcsStateInstance` ← `FTcsBuffInstance` / `FTcsSkillInstance`。**MUST NOT 取 `FTcsEntityRuntime` 一类含 `Entity` 的名字**：`Entity` 在本仓**专指**承载 State/Buff/Skill/Attribute 的**战斗实体**，而本结构装的是**实体身上的运行态**，不是实体本身——用 `Entity` 命名它属**词汇二义性**（`DEC-2026-10-14-entity-vocabulary` 所禁）。

**★ 三处"看起来像、实际不是"的邻词 MUST NOT 按名字合并**（逐条实测，2026-10-14）：
- **`CascadeAnchor` ≠ 域内的 `Source`**：后者是"**谁施加的**"，明文 **MUST NOT 当撤销锚点**（同一来源可施加多个不同定义 ⇒ 按它摘会误摘别条实例的条目）。`Source` **不住本基底**；
- **`ExpiryEntry` ≠ 技能侧的"时段游标"**：`FTcsSkillInstance::PhaseIndex` 是**时段推进的下标**，与本字段（计时项的取消锚）**同词根不同义**，MUST NOT 混为一谈。

#### Scenario: 基底只有两域公共字段

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 恰为 `DefTag` / `Entity` / `CascadeAnchor` / `ParamSnapshot` / `Level` / `ExpiryEntry` 六个；**不含** `Handle` / `Source` / `Instigator` / `Phase` / `Stacks` / `DurationRemaining` / `PeriodRemaining` / `PeriodEntry`（Buff 域内）与 `EntryHandle` / `PhaseIndex` / `ChainRunHandle`（施法侧）

#### Scenario: 派生结构不重复声明公共字段

- **WHEN** 检查 `FTcsBuffInstance` 与 `FTcsSkillInstance` 的字段声明
- **THEN** 两者都**不**重复声明基底 6 字段（各自只声明域内字段），且都能直接读到基底字段（继承而非复制）

#### Scenario: 来源不作撤销锚点

- **WHEN** 检查撤销路径按什么摘除条目
- **THEN** 按 `CascadeAnchor` 摘（MUST NOT 按 `Source`）——同一来源可施加多个不同定义，按 `Source` 摘会误摘别条实例的条目

### Requirement: 共享层门面与其存在判据

`TcsState` MUST 提供**极薄的共享门面** `UTcsStateSubsystem : UWorldSubsystem`，它**只持两件被两域在代码级共用的设施**（MUST NOT 成为"什么都往里塞"的共享层）：

- **快照参数表反射壳** `GetParamTableReader()`——技能侧求值引用类时长与参数链行时从它取壳（类型与绑定纪律归 `state-param-snapshot`）；
- **跨域在飞运行态查询契约的注册与取用点** `GetLiveQuery()` / `SetLiveQuery()`——语义归本能力的「跨域在飞运行态查询契约」需求。

**存在判据 MUST 写进头注释且可执行**：`TcsStateSubsystem.h:28` 的「**本类的存在判据 = "这个设施是否被两个域在代码级使用"**」。**不满足判据的设施 MUST NOT 进本门面**——门面被削到只剩两件，正是该判据仍在生效的证据。

**该判据曾两度失效，两份真相 MUST 留痕**：
1. **判据本身曾写错**（`PLN-REFACTOR-1` Task 4《开工前置》的《一处判据自我纠正》）——当时把"三组读口"写成本门面职责，而按代码级消费者实测，等级读口 / 事件与时钟读口的消费者**全部在 Buff 域**，故它们随 `UTcsBuffSubsystem` 走。
2. **"唯一共用设施"的排他性断言曾失效**（2026-10-24 订正）——原文写「**只**持快照参数表反射壳……两域代码级**唯一**共用设施」，该断言在 `f626582`（`R5.5-e1` 落地 `ITcsStateLiveQuery`）之后就**已假**：本门面同时是跨域查询契约的注册与取用点。⇒ **判据 MUST 写成"这个设施是否被两个域在代码级使用"这类增量安全的形式，MUST NOT 写成"唯一的共用设施是 X"**（排他性断言在每次新增共用设施时都会变成假话，而结论往往仍成立）。

**MUST NOT 把两份门面读成读口重复**：`UTcsStateSubsystem` 与 `UTcsBuffSubsystem` 的分工判据就是上面那条——施加 / 查询 / 移除 / 等级读口 / 事件与时钟读口全部住 `TcsBuff`，只有"两域都用"的才留在本门面。

#### Scenario: 门面只持两域共用设施

- **WHEN** 检查 `UTcsStateSubsystem` 的公开方法集
- **THEN** 恰为 `GetParamTableReader()` / `GetLiveQuery()` / `SetLiveQuery()` 三项；**不含**任何施加 / 移除 / 等级读口 / 事件与时钟读口（那些住 `UTcsBuffSubsystem`）

#### Scenario: 单域设施的消费者都在同一域

- **WHEN** 取证某设施是否该进共享门面
- **THEN** 按**代码级消费者**实测：若消费者全部落在同一域（如等级读口只在 Buff 侧物化路径与四个等级源被读），则该设施 MUST NOT 进本门面——"看起来通用"不构成判据

#### Scenario: 新增共用设施不使判据失效

- **WHEN** 未来新增第三件被两域共用的设施
- **THEN** 本条判据（"是否被两个域在代码级使用"）**仍然成立**，只需把该设施列入清单；MUST NOT 因"原文写了唯一"而认为需要推翻结论

### Requirement: 跨域在飞运行态查询契约（`ITcsStateLiveQuery`）

「A 域要读 B 域的在跑运行态」MUST 经**共享层定义的单一契约**中转，**MUST NOT** 让任一域认识另一域：

- `ITcsStateLiveQuery`（`UINTERFACE`，住 `Public/State/TcsStateLiveQuery.h`）MUST 由 `TcsState` 定义、由**域自己向下注册**（`UTcsStateSubsystem::SetLiveQuery`），**MUST NOT** 由宿主注入——判据：这**不是**宿主能力（宿主没有理由知道「插件内部两个域要互通」），而是框架自身机制的缺失；若要求宿主接线，则每个宿主都要接一次，而**漏接的表现是关系门静默失效**（本仓最忌的失效形态）；
- **形状 MUST 是一对，不是单半**：`FTcsLiveStateView` 同时交出「**事实半**」`CategoryTags`（这个在跑实例**是什么**）与「**声明半**」`Row`（这个在跑实例**约束谁**）。两半 **MUST NOT** 合并进一个类型——行答「我约束谁」（声明）、词面答「我是什么」（事实），把 `CategoryTags` 塞进 `FTcsRelationRow` 会让同一格承载两种语义（本仓反复拦的混装形态）。**该形状经三次修正，前两版各少一半**，MUST NOT 回到只交类别词或只交行的形态；
- **两个方法**：`ForEachLiveState(Entity, Visitor)` MUST 按**稳定序遍历**该实体当前在册的运行态并逐个交出视图（`Visitor` 返回是否继续）；`GetLiveCount(Entity) const` MUST 返回在册实例数（未注册实体返回 0）。**遍历期间 MUST NOT 增删实例**（会改槽位与空闲栈）——调用方若需增删，先收集再动（两段式）；
- **未注册时 MUST 降级而非失败**：某域未注册 ⇒ 该域**零贡献**（不是「整条查询失败」）；调用方 MUST 按「该域无在跑实例」处理（关系门恒过的保守侧）并**留一条具名日志**，**MUST NOT** 静默当真、**MUST NOT** 因此拒绝——缺注册不该变成随机的业务失败；
- **契约 MUST 为 C++ 专用面**（形参含 `TFunctionRef`，蓝图不可表达），与 `ITcsEntityQuery` / `ForEachState` 的先例一致；
- **它与 `ITcsEntityQuery` 的分工**：那个读**世界**（遍历 / 定位 / 存活——宿主本体论，框架不认识「世界有哪些实体」）；本契约读的是**框架自己的运行态**（哪些运行态在跑），故实现方是域而非宿主。

> **收录订正（2026-10-24）**：本契约于 `f626582`（`R5.5-e1` 关系表族检查）已落地并被**真实消费者**使用（`FTcsSkillOps_Relation::CheckRelationGate` 的第 ③ 向判定），但**本能力自 `R5.5-e1` 起从未收录它**——`ITcsStateLiveQuery` / `FTcsLiveStateView` / `GetLiveQuery` / `SetLiveQuery` / `ForEachLiveState` / `GetLiveCount` 六个符号在 `openspec/specs/` 全库**零命中**。性质同 `G-6`（`FTcsPrimaryAssetQuery` 有唯一真实消费者却零收录）/ `G-17`（`IsChainRegistered` 同款）：**有实现者、有消费者，而最高权威层查不到**。
> **连带订正两处错误的排他性断言**：`plugin-descriptor:29` 与 `buff-instance-lifecycle:140` 都写「技能侧对状态侧的**唯一**代码级依赖是 `GetParamTableReader()`」——该断言 **已假**（真实依赖共 7 类：`FTcsStateInstance` 基底 / `FTcsParamSnapshot` / `UTcsStateParamTableReader` / `FTcsStateRelation` / `FTcsRelationRow` / `ITcsStateLiveQuery` / `FTcsLiveStateView`）。两句都成文于 `PLN-REFACTOR-1` Task 4/5（`948e98d` / `c0bbeae`），而本契约晚至 `f626582` 才进代码 ⇒ **不是写错，是「写下排他性断言之后世界变了、没人回头改」**。⚠ **结论仍成立**（`TcsSkill.Build.cs` 确无 `TcsBuff`），但**论证依据已失效**：照原文判断「新增一个跨域读口会不会破坏这条边界」会得到错误答案。

#### Scenario: 跨域接口不依赖任一域

- **WHEN** 检查 `ITcsStateLiveQuery` 与 `FTcsLiveStateView` 的声明所在模块与其形参类型
- **THEN** 二者都住 `TcsState`（共享层），且形参只含共享层自己定义的值类型（`FTcsCombatEntityHandle` / `FTcsRelationRow` / `FGameplayTagContainer`）——**MUST NOT** 出现任一域的 Def 类型或运行态记录类型（出现即制造包含环）

#### Scenario: 视图两半缺一即判据缺失

- **WHEN** 消费方要同时判「在跑实例是否落进我的禁区」（需事实半）与「在跑实例是否覆盖我」（需声明半）
- **THEN** 一次遍历即可拿到两半；**只交类别词**会使后者无法表达、**只交行**会使前者无法表达——两种残缺形态都被本 Scenario 判为不合规

#### Scenario: 未注册时降级放行且可诊断

- **WHEN** 状态域尚未注册实现时，技能侧调用关系门
- **THEN** 关系门按「无在跑实例」放行并**留一条具名日志**（含实体与定义）；**MUST NOT** 静默当真、**MUST NOT** 因此拒绝该次激活、**MUST NOT** ensure

#### Scenario: 遍历期间不改动在册集合

- **WHEN** 消费方在 `ForEachLiveState` 的 `Visitor` 内尝试增删该实体的运行态
- **THEN** 该行为被禁止（调用方 MUST 先收集再动）；判据 = 遍历依赖桶的槽位与空闲栈，中途增删会让遍历命中已归还的槽位

#### Scenario: 在册计数是可读的观测面

- **WHEN** 对某实体调用 `GetLiveCount`
- **THEN** 返回该实体当前在册的运行态数（未注册实体返回 0）；该口是**观测面**（装置断言用），不参与任何判定——**消费者 MUST 在头注释登记「哪个装置 + 该装置可整体摘除」两件事**
