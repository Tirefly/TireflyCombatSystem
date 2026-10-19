# ARCH-09-targeting · TcsTargeting（目标选择策略层）

- **档案 ID**：`ARCH-09-targeting`
- **模块**：`TcsTargeting`（1 144 行 / 23 文件 ~~1 166 / 24~~ —— 仅 `.h` + `.cpp`；含 `TcsTargeting.Build.cs` 22 行为 1 166 / 24）
- **上游依赖**：`TcsCore` / `TcsEffect`（实测自 `TcsTargeting.Build.cs:12-20`）
- **下游消费者**：`TcsIntegration`（声明）/ **零代码消费者**（见 §1.4）
- **审核日期**：2026-10-17
- **审核方法**：`Documents/tcs-arch/INDEX.md` §5（六步；本篇首次按第 6 步「先读收束节」执行，见 §7.2）
- **本篇结论**：**全仓唯一一个"零 `UCLASS` 的领域模块"——它没有自己的门面与生命周期，运行期靠别人的门面活着。规格覆盖率是全仓第二高，但覆盖率最高的那半边（排序相位）在代码里无法自证。**

---

## §1 模块定位与边界

### 1.1 一句话职责【实测】

`TcsTargeting` 交付三件东西，且**只交付这三件**：

1. 三个**策略基类**（选择器 / 过滤器 / 评分器）——全是 `USTRUCT(meta = (Hidden))` 的 C++ 虚分派基类；
2. **两个内置实现**（`FTcsSelSelf` 选自己、`FTcsScorerDistance` 距离评分）+ **三个宿主脚本转发器**（选择 / 过滤 / 评分各一个）；
3. **一个链步骤** `FTcsStepSelectTargets` 与它的执行器——**选择 → 过滤 → 去重 → 排序 → 取前 K → 写回 `Context.Targets`**。

它**不选谁、不认识存活与阵营、不理解目标集的下游用途**。头注释把这条纪律写在三处（`TcsTargetSelectorStrategy.h:18-19` / `TcsTargetFilterStrategy.h:18-21` / `TcsTargetScorerStrategy.h:24-27`），措辞是「选谁是宿主/内容语义，框架只立契约」与「框架零宿主词汇」。

### 1.2 文件组与职责【实测】

| 组 | 文件 | 行 | 职责 |
|---|---|---|---|
| 模块壳 | `TcsTargetingModule.h/.cpp`、`TcsTargetingLogChannel.h/.cpp` | 35 | 标准壳 + 独立日志通道 |
| 选择器 | `Targeting/TcsTargetSelectorStrategy.h` | 56 | 基类（中性默认 = 不产出） |
| | `Targeting/TcsSelSelf.h/.cpp` | 74 | 内置：写 `Context.Caster` |
| | `Targeting/TcsSelHostDelegate.h/.cpp` | 84 | 脚本转发器 |
| 过滤器 | `Targeting/TcsTargetFilterStrategy.h` | 47 | 基类（中性默认 = **通过**） |
| | `Targeting/TcsFilterHostDelegate.h/.cpp` | 74 | 脚本转发器 |
| 评分器 | `Targeting/TcsTargetScorerStrategy.h` | 60 | 基类（中性默认 = `0.0`） |
| | `Targeting/TcsScorerDistance.h/.cpp` | 94 | 内置：到施法者的**真实欧氏距离** |
| | `Targeting/TcsScorerHostDelegate.h/.cpp` | 91 | 脚本转发器 |
| 排序项 | `Targeting/TcsTargetSortItem.h` | 52 | `{ FInstancedStruct Scorer; ETcsTargetSortDirection Direction; }` |
| 宿主接口 | `Host/TcsTargetSelectorHost.h` / `TcsTargetFilterHost.h` / `TcsTargetScorerHost.h` | 175 | 三个 `UINTERFACE(MinimalAPI, Blueprintable)` |
| 步骤 | `Chain/TcsStepSelectTargets.h/.cpp` | 302 | 步骤形状（4 个 `UPROPERTY`）+ 执行器（225 行） |

**结构上的第一件事**：`Public/` 下 9 个 `USTRUCT` + 3 个 `UINTERFACE`，**`UCLASS` 一个都没有**（`grep '^UCLASS' Source/TcsTargeting/` 零命中）。这不是遗漏——见 §1.3。

### 1.3 本模块最反常的一点：**它是全仓唯一"零 `UCLASS` 的领域模块"，于是它的运行期入口是别人的门面**【实测】

十一个运行期模块里，每一个领域模块都有**自己的 `UCLASS` 门面**（子系）：`UTcsAttributeSubsystem` / `UTcsStateSubsystem` / `UTcsEffectSubsystem` / `UTcsBuffSubsystem` / `UTcsDamageSubsystem` / `UTcsSkillSubsystem` / `UTcsDefinitionSubsystem`。**唯独 `TcsTargeting` 没有**。

| 模块 | `UCLASS` 数 | 自己的门面 |
|---|---|---|
| `TcsCore` | 5 | 无（基础设施，本来就该没有） |
| `TcsNotation` | 0 | 无（header-only 静态助手，见 `ARCH-02`） |
| `TcsAssetButler` | 2 | 无（加载层，见 `ARCH-03`） |
| `TcsAttribute` … `TcsSkill` | 2~6 | **有** |
| **`TcsTargeting`** | **0** | **没有** |

那么它的运行期入口在哪？**在 `UTcsEffectSubsystem` 身上**：

```
// Source/TcsTargeting/Private/Chain/TcsStepSelectTargets.cpp:82-87
// 宿主能力注入点（未注入为 nullptr——契约允许，策略自行降级）
ITcsEntityQuery* EntityQuery = nullptr;
if (const UTcsEffectSubsystem* Subsystem = Run.Owner.Get())
{
    EntityQuery = Subsystem->GetEntityQuery();
}
```

它靠 **`FTcsChainRun::Owner`**（`TcsChainRun.h:139`，`TWeakObjectPtr<UTcsEffectSubsystem>`）拿到效果门面，再取注入的实体查询能力。**除此之外它没有任何状态、没有任何持有、没有任何生命周期**。

⇒ **三条推论，本篇全部结论都从这里生长**：

1. **它的"落地"只能通过别人跑**：模块级证据必然来自"某条链在跑"（不是"调它的 API"）；
2. **它的配置面完全在数据里**：三个策略靠 `FInstancedStruct` 内嵌、排序靠资产上的数组 ⇒ **验收天然是"资产对不对"而非"代码对不对"**；
3. **它没有任何"自己失败"的表达能力**：执行器恒返回 `TSR_Completed`（`:65-71` 类型不符也返回完成）——它**不能断链、不能挂起、不能报错升级**。

第 3 点不是缺陷，是设计（步骤只做数据变换）；但它决定了本模块的**全部失败都是软失败 + 日志**，验收只能读日志与结果集。

### 1.4 依赖面：唯一的消费者声明了一个用不上的依赖【实测】

`TcsTargeting.Build.cs:12-20` 声明 `Core / CoreUObject / Engine / GameplayTags / TcsCore / TcsEffect`（六引擎项 + 两个兄弟模块）。反查**谁依赖它**：

```
$ grep -rn 'TcsTargeting' Source/*/*.Build.cs
Source/TcsIntegration/TcsIntegration.Build.cs:21:      "TcsTargeting",
Source/TcsTargeting/TcsTargeting.Build.cs:5/7:  （自身）
```

**只有一个消费者，而那个消费者一行代码都没用过它**：`TcsIntegration` 全模块对 `TcsTargeting` 的命中数 = **1**，而那一处是**注释**（`TcsPieEntityQuery.h:34` 的「机制层（TcsEffect/TcsTargeting/TcsDamage）MUST NOT 依赖本映射」）。同批另有 `TcsDamage` 一模一样（也是 1 处注释、0 引用）。

⇒ **`TcsTargeting` 是插件里"编译期耦合最窄、运行期位置最中间"的模块**：它是叶子（零下游代码依赖），却坐在每一条链的第二个步骤上（见 §4.5）。这条组合在本档案十篇里是唯一的。见 **D-6**。

---

## §2 已落地

### 2.1 执行器：五段流水，**三段各有独立降级，两段没有**【实测】

`Private/Chain/TcsStepSelectTargets.cpp` 的 `ExecuteStepSelectTargets`（`:63-221`）是这个模块**全部业务逻辑**所在。它的骨架是六步：

| # | 段 | 代码 | 降级行为 |
|---|---|---|---|
| 0 | 载荷类型校验 | `:65-71` | 类型不符 → `Error` + **返回完成** |
| 1 | 取选择器 | `:73-80` | **未配 → 目标集原样 + `Warning` + 返回完成** |
| 2 | 解析候选 | `:90-91` | 选择器自己降级（基类中性 = 不产出） |
| 3 | 过滤（AND + 短路） | `:96-121` | **空槽位 → `continue`（不淘汰）** |
| 4 | 去重（键 = 句柄身份） | `:126-140` | 无降级（幂等） |
| 5 | 排序（可选） | `:144-203` | **空 `SortItems` / 候选 ≤ 1 → 整段跳过** |
| 6 | 取前 K | `:207-211` | `MaxCount <= 0` → 不限 |
| 7 | 写回 | `:213` | 整体替换 `Context.Targets` |

**三段降级各有一条独立的理由注释，且理由不同类**（这是本模块注释质量最高的一处）：

- 段 1「未配 Selector」的理由是**"配置缺失与合法空集必须可区分"**——所以它**保留原目标集**而不是清空（`:76`）；
- 段 3「空槽位」的理由是**"执行器不做隐式过滤"**——所以它 `continue` 而不是淘汰（`:106`）；
- 段 5「空 `SortItems`」的理由是**"既有保序行为逐位不变"**——所以它**整段跳过**（`:144`）。

**但段 5 的"候选 ≤ 1"与「空 `SortItems`」被合进了同一个 `if`**（`:145`）：

```
if (Step->SortItems.Num() > 0 && ResultTargets.Num() > 1)
{
```

「配了排序项」与「有东西可排」是两个不同条件，前者的语义是"**要评分并淘汰 NaN**"，后者只是"排序本身是空操作"。合并的后果见 **D-1**——这是本模块唯一的纯代码缺陷。

**另有两条"零降级"的段值得记住**：

- **段 4 去重没有任何开关**：规格 `:68` 明确「去重是执行器的责任，MUST NOT 转嫁给选择器」，代码如实照办（`TSet` + 保序 `Add` 两段式，`:129-139`）；
- **段 7 是替换不是累加**：`Context.Targets = MoveTemp(ResultTargets)`。这条语义的下游后果见 **§4.4** 与 **U-6**。

### 2.2 去重：**位置本身就是规格的一部分**【实测】

`:123-142`：

```
// 去重（键 = 实体句柄身份）：宿主来源重叠时下游会对同一目标动手两次——本步把该缺口堵住。
// 位置 MUST 在取前 K **之前**（放到之后会让结果数少于 K）。重复项彼此不可区分，
// 故"保留先出现的还是后出现的"无观测差异。
```

规格 `:68` 与旧设计 §3 都写了这条位置约束。**这是本仓少见的"位置约束被规格、代码注释、实现三处同时钉住"的地方**。

去重键 = `FTcsCombatEntityHandle` 的 `operator==`（`TcsCombatEntityHandle.h:47-50`，只比 `Id`）+ `GetTypeHash`（`:52-56`）——所以"键 = 完整身份"这个说法在无代际段的句柄上是**精确的**（不存在"同 Id 不同代际"的可区分情形）。

### 2.3 排序：预计算矩阵 + 严格字典序 + 稳定键，**但入口有一个 `> 1` 的门**【实测】

三段结构：

1. **预计算**（`:149-175`）：每个候选 × 每个排序项求值一次，装进 `FSelectTargetsScoredCandidate{ Handle, Scores }`；任一排序项 NaN ⇒ **该行不进数组**（= 排除）；
2. **字典序比较**（`:180-196`）：逐项比对，第 N 项只在前面全等时才参与；每项方向独立；
3. **稳定键决胜**（`:195`）：全等后 `A.Handle.Id < B.Handle.Id`。

预计算的理由写在匿名命名空间的类型注释里（`:14-22`），并且**点名了 UBT 的 unity 合并会撞名**——这是本仓少见的"文件级结构决策 + 它的引擎依据"写在一起。

**"±Inf 保留"的实现手法值得单独记一笔**：注释说「比较用 `==` 判等，±Inf 与自身相等，故不会误进稳定键分支」（`:148`）。核对：`+Inf == +Inf` 为真 ⇒ `continue`，不落稳定键。**把浮点判等当成结构性的依赖写进注释**，而非只写"注意浮点"。这类"原因写在结果旁边"的注释在本模块密度最高。

### 2.4 三个降级面：**空载体三档不同，且三档都被论证过**【实测】

三个"槽位里没东西"的情形，处理**互不相同**，且**每一档都有独立的理由**：

| 情形 | 处理 | 理由（代码锚点） |
|---|---|---|
| `Selector` 载体为空 | **保留原目标集 + `Warning`** | 配置缺失 vs 合法空集必须可区分（`:76-79`） |
| `Filters` 某元素载体为空 | **`continue`（视为通过）** | "执行器不做隐式过滤"，配置残缺由作者期校验兜底（`:106-108`） |
| `SortItems` 的 `Scorer` 载体为空 | **中性分 `0.0`** | 与过滤器同口径："配置残缺由作者侧校验兜底，执行器不做隐式淘汰"（`:35-48`） |

三档的**共同判据**是「执行器不替配置作者做决定」，但**具体落点三个方向**：保留 / 通过 / 中性分。三者都对，因为**失败的下游后果不同**：

- 选择器为空 ⇒ 没有候选来源，只能"什么都不做"（保留原集）；
- 过滤器为空 ⇒ **淘汰全部**会让"打不到人"，比"放过"更难排查（`TcsFilterHostDelegate.h:48-50` 有同款论证）；
- 评分器为空 ⇒ 返回 `0` 会让"取不到"表现成"都并列"，但返回 NaN 会更糟（**淘汰全部**）⇒ 取中性分。

⇒ **这是本档案首次出现"族内三种不同处理全部被正确论证"的模块**。前三篇里同族差异几乎都是债（`ARCH-06` D-2/D-3/D-11 的 G-15，`ARCH-07` D-4 的 G-18）；这里方向相反——**差异存在且正确，理由写在各自旁边**。

**唯一没被论证的一处**：三个转发器的"`Host` 未配置"是**三档**（选择器 `Warning` + 空集 / 过滤器 **静默** 通过 / 评分器 `Warning` + 中性分）。过滤器那档"静默"其实符合规格（`targeting-strategy:184` 只要求"通过"，`:286` 才要求评分器"**MUST NOT 静默**"），但**规格没有解释为什么过滤器豁免了"必须可见"**——见 **D-9**。

### 2.5 三个宿主插槽：**同一套"两层转发"论证被抄了三遍，抄得一字不差**【实测】

三个转发器（`FTcsSelHostDelegate` / `FTcsFilterHostDelegate` / `FTcsScorerHostDelegate`）的**存在理由完全相同**，且三个头文件都**各写了一遍**。

**这不是重复冗余，而是必要的**：三个文件是三个独立编译单元、三个独立阅读入口。同一理由写三遍的代价是"改一处漏两处"，收益是"任取一个文件都能读懂"。**本模块选了后者**，并在 `TcsTargetSelectorHost.h:28-30` 明确写了「故 MUST 经两层」，在另两处写「同款理由，见 `TcsTargetSelectorHost.h`」——**分工正确**：一处完整论证、两处指路。

三档降级对比（这是三个转发器**唯一不同**的地方，见 §2.4 末段）：

| 转发器 | `Host` 为空时 | 日志级别 |
|---|---|---|
| `FTcsSelHostDelegate` | 产出**空集** | `Warning`（`TcsSelHostDelegate.cpp:17`） |
| `FTcsFilterHostDelegate` | 判**通过** | **无日志**（`TcsFilterHostDelegate.cpp:12-14`，只 `return true`） |
| `FTcsScorerHostDelegate` | 返回**中性分 0.0** | `Warning`（`TcsScorerHostDelegate.cpp:16-17`） |

**评分器那个"必须报 Warning"的规格要求（`targeting-strategy:286`）是有依据的**，理由写在代码注释 `TcsScorerHostDelegate.h:54-56`：配置残缺 SHOULD 表现成"这一项不区分候选"而不是"淘汰全部候选"。**而过滤器不报的理由同样成立**（落后于"通过"），只是规格没写——见 D-9。

### 2.6 距离评分器：**四条失败路径全部收敛到同一个 NaN，且其中一条是 `Verbose`**【实测】

`TcsScorerDistance.cpp` 仅 39 行，四个出口：

| 出口 | 条件 | 日志级别 |
|---|---|---|
| `:14-19` | `EntityQuery == nullptr` | `Warning` |
| `:22-28` | 施法者句柄无效 **或** 施法者无定位 | `Warning` |
| `:30-35` | 候选无定位 | **`Verbose`** |
| `:38` | 成功 | — |

**三档日志级别的选择是有依据的**：前两条是"**整链都无法评分**"（全体候选被排除、目标集必空）⇒ `Warning`；第三条是"**这一个候选不行**"（其余照常，属正常业务）⇒ `Verbose`。后者在 `:33` 的注释是「候选无定位——该候选被排除」，没解释为什么降级到 Verbose，但**对照前两条就能读出来**。

「分数 = 真实欧氏距离」这条纪律的实现手法值得记：`:37` 的注释写「分数 = 真实欧氏距离（非平方值）——它可观测，未声明的变换会误导诊断面」，用 `FVector::Dist` 而非 `DistSquared`。**规格 `:253` 同款措辞**（「它是可观测值（后续的诊断/预览面会展示"首个排序项分数"）」）⇒ **代码与规格在这一条上逐字对齐**，且**理由一致**。这是本模块规格-代码一致性最高的一处。

### 2.7 `SelSelf`：20 行，**且"不需要实体查询"是被显式声明的**【实测】

`Private/Targeting/TcsSelSelf.cpp` 全部 20 行，逻辑只有三行：判 `Caster` 有效，有效则 `Add`，无效则 `Warning` + `return`。

形参写法是 `ITcsEntityQuery* /*EntityQuery*/` —— **注释掉形参名**，即"我不用它，但契约要求我收下它"。这条手法在 `TcsScorerHostDelegate.cpp:9` 也有（同款），是"契约形参与实现需要解耦"的一个小型惯例。

**"不进存活过滤"这条纪律有 4 处独立表述**（`TcsSelSelf.h:29` / `TcsStepSelectTargets.cpp:94-95` / `targeting-strategy:120` / 旧设计 §2.2「框架零默认 Filter」）。这是本模块**被重复强调最多**的一条纪律，理由是"框架不认识存活"——而它正是 `ITcsEntityQuery::IsAlive` 存在的理由（见 `ARCH-08` 对 `IsEntityReady` 的分析：那个接口自认"框架不认识死亡"）。

⇒ **一条值得记住的对照**：模块**有**读存活的能力（`IsAlive` 在注入接口上，**零调用点**），却**必须**不读。见 §4.2 与 U-1。

---

## §3 对外契约

### 3.1 稳定契约（其它模块可以放心依赖）【实测】

**先说一个事实**：`TcsTargeting` 的对外契约**几乎没有跨模块消费者**。它的真实契约面是**按"数据引用"而不是"代码 include"数的**：

| 契约 | 形状 | 稳定性判据 |
|---|---|---|
| `FTcsTargetSelectorStrategy` | `virtual void Resolve(Context, ITcsEntityQuery*, TArray<...>&) const` | C++ 子类扩展点；`meta=(Hidden)` 不可被 picker 选 |
| `FTcsTargetFilterStrategy` | `virtual bool Pass(Handle, const Context&) const` | 同上 |
| `FTcsTargetScorerStrategy` | `virtual double Score(Handle, const Context&, ITcsEntityQuery*) const` | 同上 |
| `FTcsStepSelectTargets` | 4 个 `UPROPERTY`（`Selector` / `Filters` / `SortItems` / `MaxCount`） | **内容是数据**——链资产直接持有它，是这个模块唯一"被内容引用"的类型 |
| 三个 `ITcsTarget*Host` | 各 1 个 `BlueprintNativeEvent` | **脚本层唯一可达面**（见 §3.3） |
| 三个 `FTcs*HostDelegate` | `TScriptInterface<...> Host` + 转发 | 配置面（picker 可选） |
| `FTcsSelSelf` / `FTcsScorerDistance` | 两个内置实现 | 内容资产直接引用它们（类型身份） |

**"谁是真正的契约"这条判据在本模块要换一个问法**：不是"谁会 include 我"（答案是零），而是"**谁会在数据里引用我**"。按后者数：链资产引 `FTcsStepSelectTargets`；该步骤的 `Selector` / `Filters` / `SortItems` 字段引三个策略族；三个宿主插槽由脚本实现引用。**全部是数据引用，没有一处是代码 include**。

### 3.2 MUST NOT 依赖（内部实现细节）【实测+推断】

| 内部件 | 为什么不能依赖 |
|---|---|
| `ExecuteStepSelectTargets`（匿名命名空间） | 执行器本体；外部**物理不可达**（`:12-222` 全在 `namespace {}` 内） |
| `FSelectTargetsScoredCandidate` | 排序中间态；同匿名命名空间 |
| `EvaluateSelectTargetsSortItem` | 同上 |
| `TSet<FTcsCombatEntityHandle> SeenHandles` | 去重实现；块作用域（`:128-140`） |
| 策略基类的**中性默认实现** | 它们是"因为 UHT 禁纯虚而必须存在"的占位（见 §3.3），MUST NOT 被当作"可用的默认策略" |

### 3.3 一条反直觉的边界：**三个基类的"中性默认实现"不是默认策略，是编译期事故的补丁**【实测】

三个基类的虚函数都有实现体，且**实现体各不相同**（不产出 / 通过 / 0.0）。头注释把原因写得很清楚（`TcsTargetSelectorStrategy.h:21-25`）：

```
// **"抽象"由约定达成（禁纯虚）**：UHT 为每个 USTRUCT 无条件生成 `TCppStructOps<T>`（构造路径需可默认
// 构造），抽象类报 C2259；且 `PURE_VIRTUAL` 在 `CHECK_PUREVIRTUALS` 开启时展开为 `= 0`
// （`CoreMiscDefines.h:100-102`）——同一份代码会在 Development 编不过、Shipping 能过。
```

**引擎引用逐条核过**（2026-10-17）：

```
// CoreMiscDefines.h:99-103
#if CHECK_PUREVIRTUALS
#define PURE_VIRTUAL(func,...) =0;
#else
#define PURE_VIRTUAL(func,...) { LowLevelFatalError(TEXT("Pure virtual not implemented (%s)"), TEXT(#func)); __VA_ARGS__ }
#endif
```

⇒ **行号 `:100-102` 精确**（`=0;` 在 100、`{ ... }` 在 102）。这是本档案**引擎行号引用最准**的一处；对照 `ARCH-06` 审出的几处行号漂移，说明"引用引擎源码"这种写法本身是可靠的，问题在**写的时候有没有去核**。

**"抽象由约定达成"的代价**：基类可被 `NewObject` / `InitializeAs` 构造，运行期取到它就是代码缺陷。代码用 `meta=(Hidden)` 把它从 picker 里摘掉（引擎 `SInstancedStructPicker.cpp:99-103` 按 `Hidden` 元数据过滤——**行号已核**），但**运行期没有守卫**：如果有人手写 `FInstancedStruct::InitializeAs<FTcsTargetSelectorStrategy>()`，执行器只会看到"选择器没产出任何目标"，**与"选择器选中空集"完全不可区分**。

⇒ 这是 U-2 的一半。

### 3.4 一条被刻意保留的"重复字段名"：**三个转发器的 `Host` 字段名完全相同**【实测】

三个转发器的字段全部叫 `Host`（`TcsSelHostDelegate.h:41` / `TcsFilterHostDelegate.h:36` / `TcsScorerHostDelegate.h:42`），类型分别是三个不同的 `TScriptInterface<ITcsTarget*Host>`。

**这不是疏忽而是正确**：字段名一致让"配插槽"这件事在三个族里**读起来是同一件事**（既然语义都是"宿主的实现对象"，名字就该一样）。

**代价**：三个 `Host` 在 `grep` 里不可区分（都要靠文件名断上下文）。本仓另有一处相反的选择——`ARCH-07` 记的 `CascadeAnchor` vs `Source` 分名——两处判据不同：**这里分名没有信息量**（三个都是"宿主对象"），**那里分名有**（两个不同的撤销语义）。

---

## §4 竖切影响（对其它模块的实际影响面）

### 4.1 调用方向（谁在什么时候碰它）【实测】

| 调用方 | 碰什么 | 时机 | 方向 |
|---|---|---|---|
| **链解释器**（`TcsEffect`） | 经执行器注册表分派到 `ExecuteStepSelectTargets` | 链跑到该步骤 | **运行期反射**（无编译边） |
| `UTcsEffectSubsystem::GetEntityQuery` | 取注入的实体查询 | 执行器内部（`:82-87`） | **上行**（目标层取效果门面） |
| `FTcsChainRun::Owner` | `TWeakObjectPtr<UTcsEffectSubsystem>` | 同上 | 上行（弱引用） |
| **宿主脚本**（C#/蓝图） | 实现三个 `ITcsTarget*Host` | 配置期 / 执行期 | **反射分派**（`UFunction::Invoke`） |
| `TcsDev`（LAC） | `FTcsSelDevMulti` 继承策略基类；`FTcsSelSelf` 装配 | 装置期 | C++ 继承 |
| `TcsIntegration` | **什么都不碰**（声明了依赖但零引用，见 §1.4） | — | — |

**注意第 2 行的方向**：这是本篇唯一一处"**下层模块向上取上层门面**"的运行期调用（`TcsTargeting` 编译期只依赖 `TcsEffect`，所以 `UTcsEffectSubsystem` 对它是可见的——**方向是合法的**）。但它带来一个微妙后果：**步骤的执行上下文不是一个纯数据黑板，而是"黑板 + 一个指向门面的弱指针"**。见 **D-6**。

### 4.2 时序位置（硬约束，逐条有代码锚点）【实测】

| # | 约束 | 依据 |
|---|---|---|
| ① | 实体 MUST 先注册进查询映射，再执行含排序项的选择步骤 | 组件在 `BeginPlay` 登记（`TcsCombatEntityComponent.cpp:82-85`）；顺序反了 ⇒ 距离评分全 NaN ⇒ **目标集恒空**（`TcsDevSliceRig.cpp:440-443` 记了这次实测踩坑） |
| ② | 注入 MUST 早于实体生成 | 同上（同一条） |
| ③ | 选择步骤 MUST 在被消费步骤之前 | `Context.Targets` 是唯一载体（`:213` 整体替换） |
| ④ | 去重 MUST 在取前 K 之前 | 规格 `:68` + 代码注释 `:124` |
| ⑤ | 排序 MUST 在去重之后 | 代码 `:126-203` 的物理顺序（先去重成 `ResultTargets`，再排序） |

**①和②是同一条被记了两次的坑**，且**两次都是夹具缺陷而非框架缺陷**。这条值得记住：**它的症状（"目标集恒空"）指向评分算法，而根因在装配顺序**——这正是本仓反复记的"误导性症状"一类。

**⑤ 的顺序有一处规格-代码的微妙差异**：规格 `:68` 只说"去重 MUST 发生在**过滤之后、取前 K 之前**"，**没说排序与去重的相对顺序**。代码是"先去重后排序"。这个顺序**唯一可观测的差异**是：若先去重后排序，则评分只需算 N 次（N = 去重后数）；若先排序后去重，则算 M 次（M ≥ N）。**性能上后者更差，语义上无差别**（重复项不可区分）。代码选了优的那个，但**这条没写进规格**——见 D-6 的次条。

### 4.3 一条竖切级的教训：**"目标集是替换不是累加"是本模块最重要却最容易被误读的语义**【实测】

`:213` 一行：

```
Context.Targets = MoveTemp(ResultTargets);
```

**这一行把"链上的目标数据流"变成了单向覆盖**。三个实际后果：

1. **多次选择各自截断**，不累加（旧设计 §3 原文：「多次选择各自截断是**显式数据流语义**（不额外约束"累积截断"）」）；
2. **零 `SelectTargets` 的链要能跑通，`Context.Targets` MUST 已被调用方预填**——这是 `TcsIntegration` 层（`TcsCombatEntityComponent.cpp:174-177`）与 `ExecuteChainForCaster`（`TcsEffectSubsystem_Run.cpp:110-115`）在做的；
3. **触发器起链时 `Targets` 是空的**（`TcsTriggerEvaluator.cpp:117` 注释「Targets 留空」）⇒ **走触发器的链必须先配 `SelectTargets`，否则下游步骤会"目标无效"跳过**。

第 3 点是**本模块最重要的竖切影响，且它埋在另一个模块的一行注释里**。核对代码：`TcsStepModifyAttribute.cpp:35-42` 与 `TcsStepApplyState.cpp:40-45` 都是"步骤字段优先，无效则取 `Context.Targets[0]`"，两处都在空集时 `Warning` + 跳过。

⇒ **后果**：**一条由触发器起、未配 `SelectTargets` 的链，会静默地什么都不做**（只有一条 `Warning`，不报错、不断链）。`targeting-strategy` **零记载**这件事——它不属于本模块（本模块只做替换），但**本模块是唯一能传达它的地方**（因为"谁来填 `Targets` 默认值"是本模块的上游问题）。见 **U-6**。

### 4.4 一条跨模块的隐性契约：**`Context.Targets` 的"默认值归谁"从未被裁定**【实测】

四条起链路径，三种不同的 `Targets` 初始值：

| 起链路径 | `Targets` 初值 | 代码锚点 |
|---|---|---|
| `TcsCombatEntityComponent::ExecuteChainById` | **= 自身**（`Context.Targets.Add(EntityHandle)`） | `TcsCombatEntityComponent.cpp:174-177` |
| `UTcsEffectSubsystem::ExecuteChainForCaster` | **= Caster** | `TcsEffectSubsystem_Run.cpp:110-115` |
| **触发器求值器**（`TcsTriggerEvaluator`） | **= 空** | `TcsTriggerEvaluator.cpp:114-118` |
| 宿主直调 `ExecuteChain(ChainId, Context)` | **= 由宿主给**（契约要求） | `TcsEffectSubsystem.h` 的形参 |

⇒ **同一个"链"在四条路径上有三种初值语义**。而规格层面：

- `damage-primitive:216` 说「目标由链上 `SelectTargets` 或**上下文默认目标**表达」；
- `effect-trigger:14` 说「目标归属由 `SelectTargets` 步骤或**上下文默认目标**表达」；
- `effect-trigger:219` 说「`Targets` 本轮留空（完整"事件载荷 → 目标"通路需真实带目标的载荷类型）」。

**"上下文默认目标"被两处规格当成一个已经存在的机制引用，而它在今天不是一个机制，是三个调用点各自的硬编码**。台账 `DAMAGE-1` 记的正是这件事（"载荷 → `Context.Targets` 仍未落地"），状态仍是"待触发"。

⇒ 这是 **D-4** 的实体。

### 4.5 跨模块：**本模块是"被依赖最少、但竖切影响最深"的一类**【实测】

量化对照（本档案十篇的口径统一）：

| 模块 | 行 | 被插件内模块 include 的头 | 位置 |
|---|---|---|---|
| `TcsTargeting` | 1 144 | **0**（唯一声明消费者零引用，§1.4） | 每条链的第 2 步（若配了） |
| `TcsNotation` | 92 | 1 个（`FTcsValueConvention`） | 物化边界 |
| `TcsAssetButler` | 1 058 | 2 个 | 定义发现 |
| `TcsBuff` | 5 839 | 3 个（全来自 `TcsIntegration`） | 状态施加 |

**"零代码消费者"与"坐在链中间"同时成立的只有本模块**。原因是它的接口面全部是**数据面**（`FInstancedStruct` 内嵌、`UPROPERTY` 数组）而非**代码面**——内容资产直接构造它，编译期谁都不必认识它。

⇒ 这解释了一个反直觉现象：**它是最容易"整个模块零改动地换掉"的模块**（只要换掉执行器实现，链资产零改动），也是**最容易在重构中被遗忘**的模块（没有任何编译边提醒你它还活着）。**D-6 从这个结构性事实出发**。

---

## §5 技术债（期望 vs 实际，逐条带证据）

### D-1 ★★★ 排序相位的入口把"配了排序项"和"有多个候选"合并成一个门，于是**单候选时 NaN 不会被排除**【实测】

**这是本模块唯一的纯代码缺陷，且它与规格直接冲突。**

代码（`TcsStepSelectTargets.cpp:144-145`）：

```
// 排序（**空 SortItems 时不做任何重排**——既有"保序"行为逐位不变）
if (Step->SortItems.Num() > 0 && ResultTargets.Num() > 1)
{
```

规格（`targeting-strategy:210`）：

> **浮点边界**：任一排序项对某候选返回 **NaN** ⇒ 该候选 MUST 被**排除**（MUST NOT 排到末尾、MUST NOT 取任意位置）

以及配套 Scenario（`:227-230`）：

> **WHEN** 三个候选中一个的排序项返回 NaN，另两个返回有限值
> **THEN** 该候选不出现在结果集里

**规格说"任一候选 NaN ⇒ 排除"，没有说"仅当候选数 > 1 时"**。而代码的 `> 1` 使**单候选 + NaN 时该候选被保留**——直接把一个"无法评分"的目标交给下游。

**触发条件的真实面比看上去宽**：

| 场景 | 命中缺陷？ |
|---|---|
| 未注入 `ITcsEntityQuery` + 配了距离评分器 + **恰有 1 个候选** | **是**（NaN 候选被保留） |
| 同上但 ≥ 2 个候选 | 否（正常全排除） |
| 只有一个候选且它无定位 | **是** |
| 只有一个候选且有定位 | 否（分数有限，本就不排除） |

⇒ **`MaxCount = 1` 的常见配置下，"1 个候选 + 取不到位置"是高频路径**，而它今天的行为是"把取不到位置的候选当成合法目标选中"。

**它有多隐蔽**：不崩溃、不报错，日志那一行仍然是 `取前 1 → 目标集 1`（`:215-218` 的日志**不打印分数**）。唯一痕迹是 `FTcsScorerDistance::Score` 那条日志——**而候选无定位那一档是 `Verbose`**（`TcsScorerDistance.cpp:33`），**默认日志级别下不可见**。

⇒ **静默默认失效**：配错了查询、或实体未注册，单候选链会**照常选中那个候选**，且默认日志看不出任何异常。

**修法（去掉一个条件）**：

```
- if (Step->SortItems.Num() > 0 && ResultTargets.Num() > 1)
+ if (Step->SortItems.Num() > 0)
```

去掉 `> 1` 后，单候选也会走评分路径；`Sort` 对长度 1 的数组是空操作，唯一变化是**评分被求值一次**——而那正是规格要求的"排除 NaN"。**性能代价 = 每个候选一次评分器调用**（本来就该算）。

**为什么它不是"故意的优化"**：`:144` 的注释只解释了 `SortItems.Num() > 0`（空数组保序），**对 `> 1` 一字未提**。同函数的另外三处降级（§2.1）全部有注释。⇒ **这是一处没有理由的守卫**，而它恰好改变了规格语义。

### D-2 ★★ 规格把「排序与收束」写进一条需求，于是**它的 13 个 Scenario 漏了最关键的组合**（D-1 就住在那个洞里）【实测】

`targeting-strategy` 有 9 条需求 / 37 个 Scenario，**其中 13 个 Scenario 全压在「SelectTargets 步骤与执行器」这一条需求上**（`:52-111`）。密度是全档案最高的一处。

高密度带来的问题不是"写得少"，而是**组合覆盖反而稀**：

| 组合 | 有 Scenario？ |
|---|---|
| 配排序项 + 多候选 | ✅（`:88-96`） |
| 配排序项 + `MaxCount` | ✅（`:103-111`） |
| **不配排序项 + `MaxCount`** | ❌ |
| **配排序项 + 单候选** | **❌**（**D-1 就住在这里**） |
| **`MaxCount` 大于候选数** | ❌ |
| **去重后少于 `MaxCount`** | ❌（规格只在正文 `:68` 提了"会让结果数少于 K"） |
| **NaN + 单候选** | **❌** |

⇒ **D-1 之所以能长期存在，是因为规格的 Scenario 网格少了一格**。这不是"规格落后于代码"（那是 `ARCH-08` D-2 的方向），而是**规格的验收网格本身有洞**——见 **G-23**。

**顺带一条正面观察**：37 个 Scenario 里**没有一个是空泛的**（每个都有具体的输入与可判定的结果），包括"空宿主不淘汰候选""内置评分器与插槽可混用"这类容易写成废话的条目。**质量高、网格缺角**——这是本模块规格的准确画像。

### D-3 ★★ `FTcsSelEventTarget` 被规格点名两次、说"随触发行轮落地"，而**触发行轮已经落地**【实测】

`targeting-strategy:115`：

> 它是设计 10 §2.2 承诺的框架默认选择器之一（Self / EventTarget；**EventTarget 依赖"事件载荷 → 目标"通路，随触发行轮落地**）

**核对"触发行轮是否已落地"**：**已落地**——`Source/TcsEffect/Public/Trigger/` 七个头文件、`FTcsTriggerPayloadReaderRegistry` 已运行、`effect-trigger` 规格 8 条需求、证据三篇（`2026-10-04-trigger-def-asset-pie` / `2026-10-06-trigger-host-slots` 等）。

**而 `FTcsSelEventTarget` 在 `Source/` 零命中**，且 `git log --all -S` 检索 `FTcsSelEventTarget` 在 `Source/` **也为空**（**双零** ⇒ 从未存在过，连数据形状都没落）。

⇒ 这是 **G-22（时态词）的第二次命中**，但形态与 `ARCH-08` D-2 **不同**：

| | `ARCH-08` D-2 | 本篇 D-3 |
|---|---|---|
| 时态词 | 「本 Task 只实现…」 | 「随触发行轮落地」 |
| 所指的那一轮 | 已完成（Task 5） | 已完成（R4 Task 3.5） |
| 后果 | Scenario **会假失败** | **无 Scenario**，只是读者会被误导为"等一等就有" |

**关键差别**：D-3 不会假失败（没有配套 Scenario），所以**它不会在任何验收里暴露**——`openspec validate` 只查结构，PIE 只跑实现了的东西。这比 `ARCH-08` D-2 更隐蔽。

**"事件载荷 → 目标"通道今天的真实状态**（这是 D-3 与 D-4 的交点）：

- 载荷读取器注册表**已落地且已有属主登记**（`TcsBuffEvents.cpp:36` 的 `UE_DEFINE_TRIGGER_PAYLOAD_READER`、`TcsDamage` 侧同款）⇒ **"载荷 → Caster"这条路已通**；
- 缺的只是"载荷 → `Targets`"这一步（台账 `DAMAGE-1` 已记，见 §4.4）。

⇒ **`FTcsSelEventTarget` 今天技术上可写**（读 `Context.EventPayload` + 一个属主读取器），但规格把它挂在"等触发行轮"上，而那一轮**已经过去了**。

### D-4 ★★ 「上下文默认目标」被两份规格当成机制引用，而它是**三处各自的硬编码**【实测】

§4.4 已给证据。补充**规格层面的三条**：

| 规格位置 | 原文 | 今天的真实面 |
|---|---|---|
| `damage-primitive:216` | 「目标由链上 `SelectTargets` 或**上下文默认目标**表达」 | "上下文默认目标"没有单一实现点 |
| `effect-trigger:14` | 「目标归属由 `SelectTargets` 步骤或**上下文默认目标**表达」 | 同上 |
| `effect-trigger:219` | 「`Targets` **本轮留空**（完整通路需真实带目标的载荷类型）」 | 这条是**诚实的**——它明说了留空 |

⇒ **前两条把它当成既有机制，第三条诚实地说它没有。同一份规格文件内相隔 205 行两种口径**。

**这属于哪一类缺陷**：不是"代码搬走引用滞留"（G-12，没有搬走），也不是"改名没有规则"（G-18）；它是 **G-20 的变体**——「**决定不做的东西没有位置**」，于是它被**当作做了的东西**引用。`ARCH-07` 的 G-20 记的是"有意不实现的决定无处可写"，这里是"**未实现的东西被当成前提引用**"。

**它与 G-21 的关系**：`ARCH-04` / `ARCH-07` / `ARCH-08` 三篇各记过一次"`AttributeScaled` 零实现者"。本条是**同一模式在另一个符号上的第一次命中**——"一个不存在的机制被两处规格引用为前提"。见 **G-21 的推广**。

### D-5 ★★ 三个宿主插槽里，**只有选择器家族有端到端行为证据，评分器家族零证据**【实测】

按 `ARCH-04` 立的"三层真相源"口径，逐个插槽数证据：

| 家族 | 接口 | 转发器 | C# 实现 | PIE 行为证据 |
|---|---|---|---|---|
| 选择器 | `ITcsTargetSelectorHost` | `FTcsSelHostDelegate` | ✅ `UTcsDevGcTargetSelectorHost` | ✅ `EVID-2026-09-28-selector-ref-pie`（`ref` 回写 + GC 后重入） |
| 过滤器 | `ITcsTargetFilterHost` | `FTcsFilterHostDelegate` | ✅ `UTcsDevGcTargetFilterHost` | ✅ `EVID-2026-10-03-scripting-gc-survival-probe-pie` `:127`（`PassTarget candidate=1/2 pass=True call=1/2`） |
| **评分器** | `ITcsTargetScorerHost` | `FTcsScorerHostDelegate` | **❌ 零 C# 实现** | **❌ 零证据** |

核对：在 `Source/` 与 `Script/` 里检索评分器插槽的三个符号——除了 `TcsTargeting` 自己，只有**三处注释**（`TcsParamSourceHost.h:28` / `TcsTriggerCondition.h:86` / `TcsCostPolicyHost.h:32`，都把它当"同族先例"引用），**零真实实现、零真实调用**。

⇒ **评分器插槽是"已交付但从未被任何宿主用过"的一族**。而 `targeting-strategy:280` 明确说它"补齐第三个插槽家族"，`:292-310` 给了 4 个 Scenario——**没有一个有证据**。

**为什么这条比"未实现"更难发现**：它是**已实现、已编译、已生成 glue、有 4 个 Scenario** 的东西，缺的只是"有人试过一次"。任何静态检查都查不出来，`openspec validate` 也查不出来。

⇒ 这与 `ARCH-08` D-11（观测口零规格收录）**方向相反**：那是"有消费者、规格没写"，这是"**规格写了、没有消费者**"。

### D-6 ★★ 模块没有任何"自己的生命周期"，于是**"它有没有被加载/注册"不可观测**【实测】

§1.3 已给结构证据。补充**可观测性**这一面：

`TcsTargeting` 是**唯一一个没有门面、没有子系统、没有注册表的领域模块**。它的执行器靠 `UE_DEFINE_EFFECT_STEP_EXECUTOR`（`:225`）静态自注册进 **`TcsEffect` 的注册表**：

```
// TcsEffect/Public/Chain/TcsEffectStepExecutor.h:197-198
#define UE_DEFINE_EFFECT_STEP_EXECUTOR(StepType, ExecutorFn)
	FTcsEffectStepExecutorRegistrar ExecutorFn##StepExecutorRegistrar(&StepType::StaticStruct, &ExecutorFn);
```

⇒ **三个后果**：

1. **"`TcsTargeting` 有没有编进来"无法直接问**——只能问"`FTcsStepSelectTargets` 的执行器在不在"（而 `TcsEffect` 的注册表**没有公开枚举面**，`ARCH-06` 已把这条记为 `TOOLS-6` 的一部分）；
2. **模块初始化期是唯一时机**——`IMPLEMENT_MODULE` 之后没有任何 `StartupModule` 逻辑（`TcsTargetingModule.h` 空类，13 行）；
3. **"注册失败"无表达**——静态注册器的构造函数不返回状态；注册表只在**查询期**才做幂等与寿命校验。

**这条债的形状值得单列**：它不是"少了某个 API"，而是「**这个模块的存活状态是其宿主注册表的一个不可见投影**」。对照 `ARCH-01` 记的 `TcsCore`：那个模块也零门面，但它的所有类型都是**值类型**（不需要注册）；本模块的类型需要注册，却**没有任何自有出口**。

**同批的次条**：§4.2 的⑤（排序与去重的相对顺序）**规格未写**。代码选了优的那个（先去重，评分次数少），但"先排后去"在语义上也能过全部 Scenario ⇒ **这条约束是隐性的**。

### D-7 ★★ 规格说「`BaseStruct` metadata 的存在性与正确性 MUST 进 M8 校验矩阵」，而**该矩阵的台账行里没有它**【实测】

`targeting-strategy:16`：

> 且 `BaseStruct` metadata **写错则 picker 静默不限**（`TryFindTypeSlow` 找不到返回 nullptr 不报错）——故该 metadata 的存在性与正确性 **MUST 进 M8 校验矩阵**（台账 `TOOLS-1` 范围）

**核对 `TOOLS-1` 的台账行**（`deferred-inputs-ledger.md:225`）：它记的是「**PV-8 四联校验矩阵**：源类型 / 上下文类型 / 可配约定（D5-18 v3）/ 视图兼容（D5-17 v3 `IsCompatible(Probe)`）」，四条里**没有一条是 `BaseStruct` metadata 校验**。

**唯一提到它的地方是台账的变更记录**（`:413`）：

> - **新增待办（建议入 M8 校验矩阵，归 `TOOLS-1`）**：`BaseStruct` metadata **写错则 picker 静默不限**……——需"metadata 存在性与正确性"校验。

⇒ **"建议入"写在变更记录里，从没进过 `TOOLS-1` 那一行**。这是 **G-11（台账与档案是两套账）的一个具体受害面**，形态与 `ARCH-08` 的 G-21 不同：

| | G-21（`ARCH-08`） | 本篇 D-7 |
|---|---|---|
| 缺口在哪 | 三个档案各记一次，**台账零记录** | 规格明确说"归台账 X 范围"，**台账 X 行里没有** |
| 根因 | 没人做跨模块汇总 | **"归 X 范围"是一句没有校验的声明** |

⇒ **"归 X 范围"这类措辞 MUST 有对应的行断言**，否则它等价于"已经不打算做了"。见 **G-24**。

**技术面核实**：`BaseStruct` 写错的后果，我逐行核了引擎（2026-10-17）：

```
// SInstancedStructPicker.cpp:218-230
const FString& BaseStructName = StructProperty->GetMetaData(UE::StructUtils::Metadata::BaseStructName);
if (!BaseStructName.IsEmpty())
{
    if (UScriptStruct* Struct = UClass::TryFindTypeSlow<UScriptStruct>(BaseStructName))
    {
        return Struct;
    }
    // TODO: We can't load an object at that point, this would break autoRTFM in Editor.
    return nullptr;   // <- 写错就到这里：静默返回 nullptr
}
```

⇒ **规格的描述精确**：`TryFindTypeSlow` 找不到即返回 `nullptr`、不报错；而 `nullptr` 会让 `FInstancedStructFilter::BaseStruct` 为空（`:298-300` 只在非空时才限定），**picker 变成全量列表**。

**并且这不是本模块独有的**：全仓有 **7 处** `BaseStruct` metadata（`TcsBuffDef.h:90` / `TcsBuffStackPolicy.h:149` / `TcsParamValue.h:52` / `TcsCostTypes.h:416` / `TcsStepSelectTargets.h:53` / `:58` / `TcsTargetSortItem.h:44`），**全部没有校验**。本模块占了 **3 处**。

### D-8 ★ 「`SortItems` 的 `Scorer` 载体为空时按中性分降级」**没有任何日志**【实测】

`:38-51` 的 `EvaluateSelectTargetsSortItem`：

```
const FTcsTargetScorerStrategy* Scorer = SortItem.Scorer.GetPtr<FTcsTargetScorerStrategy>();
if (!Scorer)
{
    return 0.0;
}
```

注释在 `:35-37` 给了理由（「与过滤器"空槽位不淘汰候选"同款口径」），**但没有日志**。对照四档同族降级：

| 降级 | 日志 |
|---|---|
| `Selector` 载体为空 | `Warning` + 目标集数量 |
| `Filters` 元素为空 | **无**（`continue`） |
| **`SortItems` 的 `Scorer` 为空** | **无**（返回 `0.0`） |
| 转发器 `Host` 为空（评分器） | `Warning`（规格 `:286` 明文 MUST NOT 静默） |

⇒ **同族四档，两档有日志、两档没有，而规格只对"转发器 `Host` 为空"要求了 MUST NOT 静默**。

**为什么这条是债而不是"规格允许"**：规格 `:16` 明确写了换型代价是「丢编译期类型限定 ⇒ 改由运行期校验兜底（`GetPtr<T>()` 的 `IsChildOf` 检查 + **调用点判空**）」——**"判空"做了，但判空之后"怎么降级"规格没写**。而三处判空的降级方式各不相同（保留 / 通过 / 中性分），**只有这一处是静默的**。

⇒ 与 `ARCH-06` 的 G-15 同型（同族入口判据不一致），但**方向相反**：G-15 是"后写的修好了先写的"，这里是"**三处判空都没有统一判据，于是各自选了不同的日志策略**"。

### D-9 ★ 过滤器转发器"空宿主 = 静默通过"与评分器"空宿主 = 必须 Warning"，**规格只论证了后者**【实测】

三档对比见 §2.5。**规格的措辞差异是精确的**：

- 过滤器（`:184`）：「**空 Host 的语义 = 通过**（与基类中性默认实现一致）」——**没说日志**；
- 评分器（`:286`）：「**MUST NOT 静默**：Warning 让配置残缺可见」。

**代码严格照办了**（过滤器零日志、评分器 `Warning`）。⇒ **这不是代码缺陷，是规格没给判据**。

**为什么它值得记**：两条降级的**技术后果完全对称**（都是"配置残缺 ⇒ 该项不起作用"），规格却给出相反的日志要求，**且没有解释差异**。真实理由可以从代码注释推出来（`TcsScorerHostDelegate.h:54-56`：「配置遗漏 SHOULD 表现成"这一项不区分候选"，而不是"淘汰全部候选"」——两者都"不淘汰"，都该可见），但**规格没写**。

⇒ **判据缺位**：什么时候"配置残缺"必须可见？本仓有一条隐含规则——**降级后结果与"正常配置"不可区分时必须可见**。按这条规则，**过滤器空宿主也该 Warning**（"通过全部"与"没配过滤器"不可区分）。见 **G-25**。

### D-10 旧设计 §2.3 点名两个注入接口，**一个从不存在（`IRelationResolver`）、一个改了名（`ICombatEntityQuery`）**【实测】

`Documents/combat-system-design/spec/10-module-targeting.md:47-50`（§2.3 注入接口）写：

> - `ICombatEntityQuery`（UINTerface，TcsEffect 定义——机制层对宿主能力的契约）：实体遍历（稳定序全量句柄）/ `GetLocation` / `IsAlive`。
> - `IRelationResolver`：阵营关系判定——`IsHostile` / `IsFriendly(来源, 目标)`。
> - 传递方式：经 `FEffectContext` 携带的注入引用访问（链上下文注入——运行时协作不产生编译边）。

**三项逐一核对**：

| 声称 | 实测 | 性质 |
|---|---|---|
| `ICombatEntityQuery` | 代码名是 `ITcsEntityQuery`（`TcsEntityQuery.h:34`），且旧设计 §1 自己也标注了「实现名 `ITcsEntityQuery` 见 plan2」 | **改名，且文件内已就地订正** |
| `IRelationResolver`（`IsHostile` / `IsFriendly`） | `Source/` 零命中；`git log --all -S` **也零命中**（双零） | **从未存在过** |
| 「经 `FEffectContext` 携带的注入引用」 | **明确否决**：`TcsEffectContext.h:19-20` 原文「**注入能力引用（实体查询等）不住这里**——它归门面注入点」 | **设计已被推翻，且代码里写明了理由** |
⇒ **第 2 项已入台账（`WAIT-5`，状态"等第一个需要阵营判定的宿主实现出现"）；第 3 项完全没入任何地方**。

**第 3 项为什么重要**：旧设计 §2.3 的"上下文携带注入引用"如果实现，会**改变模块的依赖形状**（上下文变成持接口指针的可变结构）。而实现选了相反的路（门面注入 + 执行期取），理由是 `TcsEffectContext.h:18-21` 的「黑板只持本次执行的数据」。**这是一次正确的设计推翻，但它只写在代码注释里，规格与台账都没有记录**。

⇒ 属 **G-20 的第三次命中**（"决定不做的东西没有位置"）。前两次：`ARCH-07` U-6（Buff 脚本面整体为零）、`ARCH-08` D-6（门面零 `UFUNCTION`）。

### D-11 ★ 旧设计 §5 的"非目标"清单里，**一条已经在今天实现了（且已自纠）**【实测】

`10-module-targeting.md:74`（§5 非目标）列了 8 条"不做"，逐条核对：

| 非目标 | 今天 |
|---|---|
| 不做指示器/瞄准渲染 | ✅ 仍不做（宿主/LAC） |
| 不做锁定/软锁辅助 | ✅ 仍不做 |
| 不做空间加速结构 | ✅ 仍不做 |
| 不认识具体效果与伤害 | ✅ 仍不做 |
| 不做投射物/区域实体 | ✅ 仍不做 |
| **不做枚举式内置模式集** | ✅ 仍不做（v2 策略化，理由仍成立） |
| **不做 Top-K 的流式实现** | ✅ 仍不做（理由"仅凭流式之名不能推断更快"仍成立） |
| **不做目标评分/权重排序（未见需求）** | ❌ **已实现**（`:87` 的修订记录自己写了「**§5 删去"不做目标评分/权重排序（未见需求）"**」） |

⇒ **7 条仍准确、1 条已自纠**。这是本档案**旧文档非目标清单准确率最高的一篇**（对照 `ARCH-06` 的 3 项自纠 / `ARCH-08` 的 6 项自纠）。

**但有一处没自纠**：§2.3 的 `IRelationResolver`（D-10）**既不在 §5 非目标里，也没在修订记录里**。⇒ **"从未做过"的东西既不属于"已实现"也不属于"非目标"**——G-20 的又一形态。

### 债务小结

| 方向 | 条目 | 数 |
|---|---|---|
| **纯代码缺陷** | **D-1** | **1** |
| **纯规格债** | D-3 / D-4 / D-7 / D-9 | 4 |
| **双方都有问题** | D-2（规格网格有洞）/ D-5（规格写了无消费者）/ D-6（结构 + 规格未写顺序）/ D-8（代码静默 + 规格未写降级口径）/ D-10 / D-11 | 6 |

**总数 11 条，是全档案第二少的一篇**（`ARCH-01` 最少）。但**分布与前面五篇全部不同**：

| 篇 | 纯代码 | 纯规格 | 双方 | 特征 |
|---|---|---|---|---|
| `ARCH-04` | 0 | 1 | 3 | 规格更严、代码更松 |
| `ARCH-05` | 0 | 4 | 0 | 代码搬走、引用滞留 |
| `ARCH-06` | 5 | 4 | 3 | 各错一半 |
| `ARCH-07` | 1 | 5 | 3 | 代码好、规格差 |
| `ARCH-08` | 1 | 8 | 3 | 完全反向 |
| **`ARCH-09`** | **1** | **4** | **6** | **"双方"占比最高（55%），且多数是"代码做对了但规格没写判据"** |

⇒ **"双方"在 `ARCH-06/07/08` 里指"两边各错一处"，在本篇里有一半是"同一件事，代码对了、规格缺判据"**（D-2 / D-6 次条 / D-8 / D-9）。这是新的形状——**不是"谁落后"，而是"判据从未被写下"**。

---

## §6 未实现（设计是否还成立 + 前置条件）

### U-1 ★★ `ITcsEntityQuery` 的四支能力里**只有一支被本模块消费**，而"该不该用 `IsAlive`"的责任链**两端都断了**【实测】

接口四支（`TcsEntityQuery.h`）：

| 能力 | 本模块消费者 | 插件内全部消费者 |
|---|---|---|
| `EnumerateEntities` | **零** | 零（只有 LAC `FTcsSelDevMulti` 用） |
| `GetLocation` | **✅ `TcsScorerDistance.cpp:23/31`** | **仅此一处** |
| `IsAlive` | 零（**且规格明令禁止**，§2.7） | **零** |
| `IsEntityReady` | 零 | `TcsSkillSubsystem` 门禁第一道 |

⇒ **`GetLocation` 的插件内唯一消费者就是本模块的距离评分器**。这也给"`RadiusArea` 后置"一个硬约束：**范围选择一旦落地，它会成为 `EnumerateEntities` 的第一个插件内消费者**（今天零消费）。

**"该不该用 `IsAlive`"的四条表述互相指认**：

| 位置 | 原文 |
|---|---|
| `TcsSelSelf.h:29` | 「**不做存活过滤**（框架不认识"存活"——候选有效性由宿主的 Filter 表达）」 |
| `TcsStepSelectTargets.cpp:94-95` | 「**不做存活过滤**……无 Filter 时句柄原样传递（**消费方自行用 `IsAlive` 核对**）」 |
| `targeting-strategy:120` | 「**MUST NOT 做存活过滤**（框架不认识"存活"，与 `SelectTargets` 执行器同款纪律）」 |
| `entity-query-contract:20` | 「**消费者可得的句柄可能已失效**：消费方 MUST 用 `IsAlive` 自行核对」 |

⇒ **第 2 条把责任推给"消费方"，第 4 条要求"消费方必须核"，而"消费方"是谁从未点名**。实际消费 `Context.Targets` 的步骤（`StepDamage` / `StepApplyState` / `StepModifyAttribute` / `TcsDamage` 的流程步骤）**无一调用 `IsAlive`**：

```
$ grep -rn 'IsAlive' Source/ | grep -v 'virtual bool IsAlive'
Source/TcsEffect/Public/Host/TcsEntityQuery.h       （全是注释）
Source/TcsIntegration/Private/Entity/TcsPieEntityQuery.cpp:71   （实现）
Source/TcsIntegration/Private/Entity/TcsPieEntityQuery.cpp:84   （IsEntityReady 转发给它）
Source/TcsSkill/Public/TcsSkillSubsystem.h:702-704  （注释）
```

⇒ **外部调用点 = 0**。**这条责任链两端都断开了**：发出方（本模块）说"消费方核"，接收方（四个步骤）不核，规格说"必须核"。**"消费方"这个词指一个不存在的主体**。见 **G-26**。

### U-2 ★★ 「策略基类不该被运行期取到」**没有守卫**，而 `meta=(Hidden)` 只挡编辑器【实测】

§3.3 已给。补充**影响面**：三个基类**都是可构造的 `USTRUCT`**（UHT 为每个 `USTRUCT` 生成 `TCppStructOps<T>`），所以：

- 资产里写一个 `FTcsTargetSelectorStrategy`（手改二进制 / 脚本层 `FInstancedStruct.Make`）⇒ 运行期取到基类 ⇒ `Resolve` 跑中性默认 ⇒ **不产出任何目标**；
- 症状 = `SelectTargets` 日志的 `候选 0`，**与"选择器选中空集"完全一致**；而"未配 Selector"分支根本进不去（载体**非空**）。

⇒ **三档失效在日志上不可区分**：未配选择器（`Warning` + 保留原集）/ 配了基类（无日志 + 空集）/ 配了选中空集的选择器（无日志 + 空集）。

**这一条与 D-1 是同一族**：都是"降级后的结果与正常路径不可区分"。⇒ **本篇的核心主题就是这一条**（§9.2）。

### U-3 ★★ `RadiusArea` / `FTargetingShape` 的**后置从未进入任何规格或台账**，只在归档提案里【实测】

| 符号 | `Source/` | `openspec/specs/` | `openspec/changes/`（全部是 archive） | 台账 |
|---|---|---|---|---|
| `RadiusArea` | 0 | **0** | 7 | **0** |
| `FTargetingShape` | 0 | **0** | 2 | **0** |
| `FTcsSelEventTarget` | 0 | **0**（`targeting-strategy:115` 只提了 `EventTarget` 这个词） | 6 | 1（间接触及） |
| `IRelationResolver` | 0 | **0** | 4 | 2（`WAIT-5`） |

⇒ **四个"后置项"里只有 `IRelationResolver` 真正入了台账**（`WAIT-5`）。其余三项**只在归档提案里出现过**，而归档提案按 `openspec` 的语义是"**已完成的变更**"——读者不会去那里找待办。

**这是本仓最危险的一类文档状态**：一条"我们决定以后做"的记录，住在"这件事已经做完"的目录里。对照 `ARCH-08` 的 G-22（时态词）——那条是**规格正文里的时态过期**，本条是**"决定不做"被存在了"已做完"的位置**。

⇒ **G-20 的第四次命中**，且这一次是**结构性**的：`openspec/changes/archive/` 与 `openspec/specs/` 之间**没有"待办"这个第三种位置**。见 **G-27**。

**顺带一条实证**：`plans/plan-r3-2-damage-chain.md:143` 明写「`GetLocation` / `IsAlive`（**随 `RadiusArea` 轮**）……见 Task 5/6 与 Task 2 注记」——而 `RadiusArea` 那一轮**从未被排进任何路线图**（`Documents/combat-system-design/plans/` 全系列检索 = 0 命中，除这一行自己）。⇒ **"随 X 轮"里的 X 轮不存在**。

### U-4 ★★ "**同一个实现对象注入多个槽位**"从未被验证【实测】

三个 `Host` 字段都是独立 `TScriptInterface`，所以宿主**可以**把同一个 C# 对象同时配进选择器与过滤器（合法，且很可能发生——"我的战斗管理器"），也可以配三个不同对象。

**今天只有"不同对象"的证据**：`TcsDevGcProbeDriver.cs:116-119` 造了两个**不同**对象（`selectorHost` / `filterHost`），配进**同一条链的两个槽位**。⇒ **"同一对象进多个槽位"零证据**。

**为什么这值得记**：`ARCH-07` U-1 / `ARCH-08` U-1 都是"跨模块的生命周期残留"；本条是**同模块内的对象复用**。若宿主真这么做，三个槽位的调用会**交错发生在同一个对象上**（选择 → 过滤 → 评分）——而 `ARCH-07` 记的 `FTcsBuffBehaviorRegistry` 的"每兴趣 Tag 一条订阅 + 计数配对"正是同一类问题的解法。

⇒ **无证据 = 无结论**。属"待触发"。

### U-5 ☆ `MaxCount` 的**负值处理**规格要求"按 0 处理"，代码靠 `> 0` 隐式实现，而 `ClampMin` 让负值在编辑器不可达【实测】

`TcsStepSelectTargets.h:72-74`：

```
// 取前 K（按**完整字典序**取，MUST NOT 理解成"按某一个排序项截断"；0 = 不限，负值按 0 处理）
UPROPERTY(EditAnywhere, Category = "Tcs|Targeting|Sort", meta = (ClampMin = "0"))
int32 MaxCount = 0;
```

规格 `:66` 要求「`0` = 不限（**负值 MUST 按 0 处理**）」。代码 `:208` 的 `Step->MaxCount > 0` 确实把负值当 0（不截断）。

⇒ **行为正确**，但这条要求在编辑器侧不可触发（`ClampMin` 只允许 ≥ 0）；而**脚本层 `FInstancedStruct` 的字段写入没有 clamp**（C# glue 是裸 `int32` 写，已核 `TcsStepSelectTargets.generated.cs` 的 `MaxCount` 字段）。⇒ **这条规格是为"脚本层可写负值"准备的**，属正面案例（对照 `ARCH-08` D-10 记的"实现超文档"）。

**未验证**：脚本层真写负值时的行为。待触发。

### U-6 ★★ `Context.Targets` 的默认值归属**从未被裁定**，而四条起链路径已各有自己的答案【实测】

§4.4 已给完整证据。这里补**为什么它是"未实现"而不是纯债**：

- 台账 `DAMAGE-1` 明确记它为「**待触发**（②"载荷 → `Context.Targets`"仍未落地）」⇒ **它是一件已知的、在册的未实现事项**；
- 但**"三条路径三种初值"这件事本身没有登记**——`DAMAGE-1` 只记"载荷 → `Targets` 未落地"，没记"另外三条路径互相不一致"。

⇒ **台账记了一半**：记了"缺的那条路"，没记"已有的三条路互相不一致"。而后者才是让读者误判的原因——**三条路一致时缺一条只是"功能少一块"；三条路不一致时，同一条链换个入口行为就变了**。

**触发条件**：出现"同一条链同时被触发器与手动 API 驱动"的真实场景（那会让不一致立刻可见）。

### U-7 ☆ 三个宿主接口的 `Blueprintable` 与"蓝图其实配不进链"的关系**从未被澄清**【实测】

三个 `UINTERFACE(MinimalAPI, Blueprintable)` 都有 `Blueprintable`，且都是 `BlueprintNativeEvent`。规格 `:154` 说「接口 MUST 加 `Blueprintable`（**脚本层要能实现它**）」。

**但"蓝图能实现它"与"蓝图能用它"是两件事**：

- **能实现**：✅ 蓝图类可实现接口（`Blueprintable` 保证）；
- **能用**：❌ 蓝图**无法把实现配进** `Host` 字段——因为该字段住在 `USTRUCT` 上，而 `USTRUCT` 走 `FInstancedStruct` 内嵌编辑（StructUtilsEditor 的 picker 只列 **struct 类型**，不列 `UObject` 实现）⇒ **蓝图只能"实现接口"，无法"把它挂到链上"**。

⇒ 规格 `:154` 的措辞（「脚本层要能实现它」）**精确地只说了"实现"**，没承诺"蓝图能配"。但 `:147` 的**需求标题**写「让宿主用任意 UE 脚本语言（**C#/AS/Luau/TS/蓝图**）实现目标选择，**零 C++ 改动**」——**"零 C++ 改动"对蓝图不成立**（缺"配进链"这一步）。

**同批核实**：`R0 §9` 的「蓝图不承诺」在 `TcsTargetSelectorStrategy.h:27` 被引用了一次（「BP 通道放弃，R0 §9"蓝图不承诺"承责」）⇒ **策略本身的蓝图通道已明确放弃**，但**插槽的蓝图承诺仍留在规格正文里**。

⇒ 这是 `ARCH-08` D-4 那类"注记加了、正文没改"的**变体**：注释里加了"BP 通道放弃"，规格正文里"蓝图"仍在脚本语言清单里。见 **G-28**。

---

## §7 文档资产对照

### 7.1 三层真相源在本模块的分布【实测】

| 层 | 内容 | 本模块的准确度 |
|---|---|---|
| **`openspec/specs/targeting-strategy/spec.md`** | 310 行 / 9 需求 / 37 Scenario | **高**——除 D-3 / D-4 / D-7 / D-9 外，代码与规格逐条对得上 |
| **代码** | 1 144 行 / 23 文件 | **高**——只有 D-1 一条纯代码缺陷 |
| **旧 `SPEC-06-targeting`** | 91 行 / ACTIVE | **8 项非目标里 7 项仍准确、1 项已自纠**（D-11） |

**规格 / 代码行数比**：

| 模块 | 代码行 | 对应规格行 | 比 |
|---|---|---|---|
| **`TcsTargeting`** | **1 144** | **310**（1 能力） | **1 : 0.27** |
| `TcsAttribute` | 3 012 | 752（4 能力） | 1 : 0.25 |
| `TcsBuff` | 5 839 | 1 473（9 能力） | 1 : 0.25 |
| `TcsDamage` | 3 225 | 635（4 能力） | 1 : 0.20 |
| `TcsSkill` | 11 052 | 1 769（8 能力） | 1 : 0.16 |
| `TcsEffect` | 7 115 | 1 068（6 能力） | 1 : 0.15 |

⇒ **本模块的规格密度是全仓第二高**（仅次于 `TcsAttribute`），且它是**唯一"一个模块对一份能力"**的模块（其余都是家族）。这与 D-2（网格有洞）**不矛盾**：**密度高 ≠ 网格全**。

### 7.2 `SPEC-06-targeting` 与代码的差异清单（**只登记，不修补**——旧文档已冻结）

按 `INDEX` §5 第 6 步（**先读收束节**）执行。本文件的"收束节"是 **§7 修订记录**（4 条 v1~v4）+ **§8 验收钩子**。

| # | 项 | 旧文档说法 | 今天的真实面 | 判定 |
|---|---|---|---|---|
| 1 | 选择器载体（§2.1） | 「Def/步骤以 **`TInstancedStruct`** 成员持有」 | **裸 `FInstancedStruct`**（`TcsStepSelectTargets.h:54`） | **自纠**（同句写了"换型 2026-09-24"，形态词仍是旧的） |
| 2 | 策略基类虚函数（§2.1） | 「**纯虚** `Resolve(Context, 注入查询, OutTargets)`」 | **禁纯虚**（`TcsTargetSelectorStrategy.h:21-25`） | **过期，零注记** |
| 3 | 过滤器基类（§2.2） | 「**纯虚** `Pass(Candidate, Context)`」 | 同 #2；中性默认 = **通过** | **过期，零注记** |
| 4 | `FTcsSelEventTarget`（§2.1） | 「**默认实现（框架提供）**：`FTcsSelSelf` / `FTcsSelEventTarget`」 | Self ✅ / EventTarget **从未存在**（双零） | **过期**（D-3 同源） |
| 5 | `RadiusArea` / `FTargetingShape`（§2.1） | 「整体后置……待真实需求出现时再定」 | 仍后置，**未入台账** | **准确**（但位置在归档提案里，U-3） |
| 6 | `ICombatEntityQuery`（§2.3） | 旧名 + **三支**能力 | 实名 `ITcsEntityQuery` / **四支**（R6 加 `IsEntityReady`） | **过期**（§1 已自纠命名，能力数未改） |
| 7 | `IRelationResolver`（§2.3） | 「`IsHostile` / `IsFriendly`」 | **双零** | **过期**（台账 `WAIT-5` 已记） |
| 8 | 注入传递（§2.3） | 「经 `FEffectContext` 携带的注入引用访问」 | **明确否决**（`TcsEffectContext.h:19-20`） | **过期，零注记** |
| 9 | 评分器默认（§2.4） | 「中性默认 = `0.0`；`EntityQuery` 可空，缺设施时降级 MUST 为 `NaN`」 | **完全一致** | **准确** |
| 10 | 内置评分器（§2.4） | 「只有一个 `FTcsScorerDistance`，真实欧氏距离」 | **完全一致** | **准确** |
| 11 | 空 `Host` = `0.0` + Warning（§2.4） | 「同 §2.2 空 Filter 的裁定理由」 | **完全一致** | **准确** |
| 12 | 执行管线（§3） | 「五段 = Resolve → 过滤 → 去重 → 排序 → 取前 K」 | **完全一致**（§2.1 的七段含两段防御） | **准确** |
| 13 | 确定性口径（§3） | 「同进程内同输入同输出（原"客户端重算一致"已收窄）」 | **完全一致**（`targeting-strategy:214` 同款） | **准确** |
| 14 | 挂起语义（§3） | 「策略/过滤产出的句柄**经代际校验**，悬空跳过」 | **不存在**：`FTcsCombatEntityHandle` **无代际段**（`TcsCombatEntityHandle.h:14-16` 明文"Id 单调递增且永不复用"） | **过期，零注记** |

⇒ **6 项准确 / 8 项过期**。**比例与 `ARCH-08` 对 `SPEC-04-skill` 的判定完全相同（6 / 8）**，但**分布不同**：

| | `SPEC-04-skill`（`ARCH-08`） | `SPEC-06-targeting`（本篇） |
|---|---|---|
| 准确项 | 6 | **6** |
| 过期但**有就地注记** | **7 / 8** | **3 / 8**（#1 / #6 / #7） |
| 过期且**零注记** | 1 | **5**（#2 / #3 / #4 / #8 / #14） |

⇒ **本模块旧文档的"就地订正率"远低于 `SPEC-04-skill`**（3/8 vs 7/8）。原因可从规模看出来：**本文件只有 91 行**（旧 `spec/` 目录第二短），而 `SPEC-04-skill` 是 498 行。**短文件的订正密度反而低**——因为它"看起来不用维护"。

### 7.3 差异的**类型分布**

按 `ARCH-05` 立的四类引用判据 + `ARCH-08` 的第五类（时态词），本文件的 8 项过期归属：

| 类型 | 项 | 数 |
|---|---|---|
| **符号名**（G-12） | #6（`ICombatEntityQuery` → `ITcsEntityQuery`）/ #7（`IRelationResolver` 不存在） | 2 |
| **类型形状**（G-18） | #1（`TInstancedStruct` → `FInstancedStruct`）/ #2 #3（纯虚 → 中性默认） | 3 |
| **字段/句柄语义**（G-12 第五面） | #14（代际校验） | 1 |
| **时态词**（G-22） | #4（EventTarget"随触发行轮落地"） | 1 |
| **设计已推翻**（**本篇新增**） | #8（上下文携带注入引用） | 1 |

⇒ **新增第六类引用判据**：「**设计已推翻（有理由、有代码、无记录）**」。它既不是时态（那一轮过去了），也不是改名（名字没问题），而是**"这个设计被否决了"这件事本身没有位置可写**——G-20 的又一形态。见 **G-29**。

---

## §8 审核记录

### 8.1 本次审核做了什么【实测】

1. 读完 `TcsTargeting` 全部 **23 个 `.h` / `.cpp`**（1 144 行），逐文件记录职责、形参、降级面；
2. 读完 `openspec/specs/targeting-strategy/spec.md` 全部 **310 行**（9 需求 / 37 Scenario），逐条与代码对照；
3. 读完旧 `SPEC-06-targeting` 全部 **91 行**，**按 `INDEX` §5 第 6 步先读 §7 修订记录与 §8 验收钩子**，再读正文——本篇是该方法立下后的**首次执行**（8.4）；
4. 追竖切：`SelectTargets` 在另 4 份规格里的出现、`GetLocation` / `IsAlive` / `EnumerateEntities` 的全部调用点、`ExecuteChain` 的 4 条起链路径、`SetEntityQuery` 的全部注入点（LAC `TcsDev` 10 处）；
5. **核了 4 处引擎源码**（`CoreMiscDefines.h:99-103` / `SInstancedStructPicker.cpp:99-103, 218-230` / `InstancedStruct.cpp:244` / `UhtStructProperty.cs:634`），逐条确认行号；
6. 核了 `git log --all -S` 判双零（`FTcsSelEventTarget` / `IRelationResolver` / `RadiusArea`）；
7. 核了 C# glue 产物（`Intermediate/UnrealSharp/UHT/` 下 `TcsTargeting` 的 14 个 `.generated.cs`），确认 `SortItems` / `MaxCount` 可写、无空壳。

### 8.2 本次审核用到的**六类判据**（`ARCH-05` 四类 + `ARCH-08` 第五类 + 本篇第六类）

| # | 判据 | 本篇命中 | 出处 |
|---|---|---|---|
| 1 | **符号名** | `IRelationResolver` / `FTcsSelEventTarget` | `ARCH-05` G-12 |
| 2 | **模块名** | **无**（本模块无拆分史） | 同上 |
| 3 | **类型名** | `ICombatEntityQuery` → `ITcsEntityQuery` / `TInstancedStruct` → `FInstancedStruct` | 同上 |
| 4 | **字段位置** | `Context.Targets` 的默认值归属 | 同上 |
| 5 | **时态词** | 「随触发行轮落地」（D-3） | `ARCH-08` G-22 |
| 6 | **设计已推翻** | 「经 `FEffectContext` 携带注入引用」（#8） | **本篇新增 G-29** |

**第二类（模块名）零命中的意义**：本模块没有经历过拆分（`PLN-REFACTOR-1` 未触及它），所以 `ARCH-05` / `ARCH-07` 那类"符号搬走了引用没搬"的缺陷**结构上不可能发生**。这是**六篇里第一次某一类判据零命中**——它反过来验证了 G-12 / G-18 的因果：**那些缺陷的成因确实是拆分，不是疏忽**。

### 8.3 置信度分布

| 置信度 | 条目 |
|---|---|
| **实证（读过代码 + 跑过 grep / git log / 引擎源码）** | D-1 ~ D-11 全部；U-1 / U-3 / U-6 / U-7 |
| **推断（有代码依据但没有运行证据）** | U-2（基类可构造，但没人这么写过）/ U-4（同一对象多槽位未验证）/ U-5（脚本写负值未验证） |
| **不确定** | **无** |

⇒ **本篇没有"不确定"条目**——十篇里第一次。原因在模块规模（1 144 行全部可读完）与规格密度（310 行覆盖全部行为）。

### 8.4 一次**方法论层面的收获**：`INDEX` 第 6 步（先读收束节）首战即兑现，**但需要一条修正**

`ARCH-08` §7.2 立的规律是「**旧文档正文一律不可当现状读，但收束节质量普遍高于 `openspec` 对应能力**」，当时是三篇同型（`SPEC-02-states` / `SPEC-03-effects` / `SPEC-04-skill`）。

**本篇是第四个样本，结论需要修正**：

| 篇 | 旧文档行数 | 收束节 | 正文就地订正率 |
|---|---|---|---|
| `ARCH-06`（`SPEC-03-effects`） | 223 | 高 | 3 / 14 |
| `ARCH-07`（`SPEC-02-states`） | 390 | **极高**（§12.13 收束回写） | 高 |
| `ARCH-08`（`SPEC-04-skill`） | 498 | **极高**（§11 实施范围与切分） | 7 / 14 |
| **`ARCH-09`（`SPEC-06-targeting`）** | **91** | **中**（§7 四条修订记录，只记"改了什么"、不记"为什么"） | **3 / 14** |

⇒ **规律本身成立（收束节比正文可靠），但它的强度与文件规模正相关**：91 行的文件，收束节也短（4 条修订记录），**它记下了 v1~v4 的变更标题却没记任何"为什么改"**；498 行的文件，收束节是**完整的实施范围表**。

**对审核方法的修正（已回写 `INDEX` §5）**：第 6 步**仍然成立**，但**不能假设收束节一定够用**——**短文档的收束节只能当"变更索引"，不能当"当前状态"**。

---

## §9 结论

### 9.1 三句话

1. **`TcsTargeting` 是全仓唯一"零 `UCLASS` 的领域模块"**——没有自己的门面、注册表、生命周期，运行期靠 `FTcsChainRun::Owner` 取别人的门面活着。这既是它"最小编译集"承诺的实现，也是它**可观测性为零**的根源（D-6）。
2. **代码质量是全档案第二高（1 条纯代码缺陷）、规格覆盖密度是全仓第二高（1:0.27），但两者都不是问题所在**——问题是**判据从未被写下**：三处判空三档降级只有一处有日志、"消费方 MUST 核 `IsAlive`"的消费方不存在、"上下文默认目标"被当作机制引用、`BaseStruct` 校验"归 X 范围"而 X 里没有它。
3. **它的核心缺陷（D-1）的形状值得单独记住**：一个**没有理由的守卫**（`&& ResultTargets.Num() > 1`）恰好改变了规格语义，且**症状与正常路径完全不可区分**（日志不打印分数、无定位那条日志是 `Verbose`）。这不是"代码没写完"，是"**代码多写了一个条件**"。

### 9.2 一句值得记住的话

> **`TcsTargeting` 的每一处降级都做对了，但「降级之后和正常路径看起来一样」这件事，它一处都没有解决。**

具体到六档：未配选择器 ✅ 有 `Warning`（可区分）；配了基类 ❌（与选中空集不可区分）；空过滤器 ✅ 不淘汰（规格允许）；空评分器 ❌ 静默降中性分（不可区分）；空 `Host` 的过滤器 ❌ 静默通过（不可区分）；**单候选 NaN ❌ 静默保留（与正常选中不可区分）**。

⇒ **应立一条判据**：「**凡降级后结果与"正常配置但确实没选中"不可区分者，MUST 至少一条 `Warning`**」。按这条，本模块有 **3 处需补**（D-1 / D-8 / U-2）。

### 9.3 与前五篇的对照

| 篇 | 模块 | 行 | 纯代码 | 纯规格 | 双方 | 一句话 |
|---|---|---|---|---|---|---|
| `ARCH-04` | `TcsAttribute` | 3 012 | 0 | 1 | 3 | 规格更严、代码更松 |
| `ARCH-05` | `TcsState` | 2 753 | 0 | 4 | 0 | 代码搬走、引用滞留 |
| `ARCH-06` | `TcsEffect` | 7 115 | 5 | 4 | 3 | 各错一半 |
| `ARCH-07` | `TcsBuff` | 5 839 | 1 | 5 | 3 | 代码好、规格主语对符号错 |
| `ARCH-08` | `TcsSkill` | 11 052 | 1 | 8 | 3 | 代码干净、规格单方面掉队 |
| **`ARCH-09`** | **`TcsTargeting`** | **1 144** | **1** | **4** | **6** | **判据从未被写下** |

**六篇债数 = 11 / 4 / 12 / 9 / 12 / 11** ⇒ **仍落在 9~12 这个窄带**。这已是第六篇，**"债务密度与模块规模无关"可以定论**：1 144 行与 11 052 行的模块，债的数量几乎相同。

**但"双方"的定义在本篇变了**：前五篇的"双方"是"两边各错一处"；本篇有一半是"**代码做对了、规格没写判据**"（D-2 / D-6 次条 / D-8 / D-9）。⇒ **"规格缺判据"第一次成为主导形态**，而不是"规格落后于代码"。

### 9.4 给下一轮的四条建议（按代价从低到高）

1. **D-1 去掉 `&& ResultTargets.Num() > 1`**（1 行，零设计判断）——本篇唯一纯代码缺陷，改完立刻与规格一致；
2. **D-1 / D-8 / U-2 补三条 `Warning`**（各 1-2 行）——按 §9.2 立的判据，这三处是"不可区分"的最小集合；
3. **D-3 / D-4 改规格**（各 1 段）——「随触发行轮落地」改为「未实现（触发行轮已落地，缺的是载荷 → `Targets` 通路，见 `DAMAGE-1`）」；「上下文默认目标」改为「三条起链路径各自的初值（见 `DAMAGE-1`）」；
4. **D-6 / D-7 / U-3 属系统性**（需用户裁决）——D-6 要决定"无门面模块的可观测性怎么做"，D-7 要把"归 X 范围"变成台账行，U-3 要给"后置项"一个合法位置（规格 / 台账 / 第三个目录，三选一）。

---

## §10 修订记录

| 日期 | 变更 |
|---|---|
| 2026-10-17 | 建立 `ARCH-09-targeting`（**本篇**）。审核 `TcsTargeting` 全部 1 144 行 + `targeting-strategy` 310 行 + 旧 `SPEC-06-targeting` 91 行。审出 **D-1~D-11 / U-1~U-7**。三条结构性发现：① **全仓唯一零 `UCLASS` 的领域模块**（§1.3），于是**可观测性为零**（D-6）；② **"判据从未被写下"第一次成为主导形态**——三处判空三档降级只有一处有日志、`IsAlive` 的责任链两端都断开、"上下文默认目标"被两份规格当作机制引用而它不存在（D-4 / D-8 / U-1）；③ **D-1 是全档案第一个"没有理由的守卫"**（`&& ResultTargets.Num() > 1`）——它恰好改变规格语义，且症状与正常选中**完全不可区分**。新增 **G-23**（规格的 Scenario 网格缺角）/ **G-24**（"归 X 范围"是无校验声明）/ **G-25**（"降级后不可区分"这条判据本仓没有）/ **G-26**（"消费方 MUST 核"而消费方不存在）/ **G-27**（"决定以后做"住在"已经做完"的目录里）/ **G-28**（注释里放弃了 BP 通道、规格正文仍列蓝图）/ **G-29**（第六类引用判据：设计已推翻）。**同时修正 `INDEX` §5 第 6 步**：收束节的可靠性与文件规模正相关，**短文档的收束节只能当变更索引**（8.4）。**旧文档判定**：`SPEC-06-targeting` **6 项准确 / 8 项过期**，与 `ARCH-08` 的 6/8 同比例但**就地订正率低得多**（3/8 vs 7/8），原因 = 文件只有 91 行、"看起来不用维护"。 |

