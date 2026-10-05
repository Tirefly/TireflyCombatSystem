# state-stacking-policies Specification

## Purpose

定义 M3 状态层的**重复施加语义**：同一 buff 再次施加时会发生什么——按什么分组、层数上限多少、满仓怎么处置、数值随不随层数走、刷新对剩余时长做什么。承载三个形状：`FStateStackPolicy`（五轴策略，`FTcsBuffDef` 的字段）、组键解析与共存决策树（`EAR_Applied / Refreshed / Stacked / Rejected` 四档回执全可达）、以及 `FTcsStateStackDecisionFragment`（宿主自定义分组/决策的逃逸位）。

**边界**：本能力只覆盖"重复施加之下的确定性决策与更新流水"。实例在册与生命周期广播（`state-instance-lifecycle`）、参数快照（`state-param-snapshot`）、修正器物化与级联摘除（`state-modifier-materialization`）各自另有能力——本能力只负责**决定**该新建/刷新/叠层/替换/拒绝，并把结果交给那些既有流水；它不新增事件词（`StackChanged` 是既有声明，本能力让它**首次有产生者**）。

**层模型（本能力的核心口径）**：**层数记在一条实例内**（`FTcsStateInstance.Stacks`），组内至多一条实例；"续杯 / 叠层"的判据 = **施加方换没换**（同来源 ⇒ 刷新、异来源 ⇒ 叠一层，未声明来源按同来源处理）。三条如实边界：legacy `NoMerge`（"每次施加独立成实例"）只能**近似**或改用 Custom 决策 Fragment 表达；"按分组词分组"档因基座含 `DefTag` 而不可表达（已裁撤）；"跨定义共享层数"与"逐层来源归属"登记台账。

## Requirements

### Requirement: 五轴堆叠政策的形状与默认

TcsState MUST 提供 `FStateStackPolicy`（`USTRUCT(BlueprintType)`）——五轴 + 一个附带字段：

- `EGroupByPolicy GroupBy`：`EGB_None = 0`（默认；同单位 + 同 `DefTag` 共处一组）/ `EGB_PerSource = 1`（组键附来源句柄）/ `EGB_PerInstigator = 2`（附发起者实体）/ `EGB_Custom = 1 << 4`（逃逸位：分组判定交给决策 Fragment）。**Custom MUST 走高位置位**，MUST NOT 占用低位的可枚举取值；**值 3 曾预留给"按分组词"档，已裁撤**（理由见「组键解析」需求末段）；
- `int32 MaxStacks`：组内最大层数，**`<= 0` = 无限**（默认 `0`）；
- `EOverflowPolicy Overflow`：`EOP_RejectNew = 0`（默认）/ `EOP_ReplaceExisting = 1`；
- `EValueStackPolicy ValueStack`：`EVS_KeepMax = 0`（默认；层数不参与取值）/ `EVS_AddValues = 1`（值随层数成倍）/ `EVS_PerStackValue = 2`（**本轮只落形状**，零行为，见本需求末段）；
- `EStackDurationPolicy StackDurationPolicy`：`ESD_None = 0`（默认；不动剩余时长）/ `ESD_RefreshRemainingToTotal = 1`（回满额）；
- `FInstancedStruct CustomDecision`：Custom 决策载荷，**裸 `FInstancedStruct` 载体**（MUST NOT 用 `TInstancedStruct<T>`——该载体在宿主脚本层导出为空壳，脚本层配不了值），类型收窄由手写 `meta = (BaseStruct = ...)` 提供。

五轴 MUST 各自以**值 0 为默认**（"最不介入"语义）：默认组合 = 同 `DefTag` 一组、层数无限、层数不参与取值、刷新不动时长；`MaxStacks <= 0` 使 `Overflow` 不可达（无限层永不判满）。

`EVS_PerStackValue` 本轮 MUST 只落枚举值与注释（零行为，如实入台账）——它的合规实现要么引入"层数读口 + 按层参数源"这套新机制，要么退化成设计明文撤回的"等级跟随层数"增长策略；`KeepMax` / `AddValues` 两档 MUST 落真实行为。

#### Scenario: 全默认策略的语义可枚举

- **WHEN** 检查默认构造的 `FStateStackPolicy`
- **THEN** `GroupBy == None` / `MaxStacks == 0`（无限）/ `Overflow == RejectNew` / `ValueStack == KeepMax` / `StackDurationPolicy == None`，且 `CustomDecision` 为空

#### Scenario: Custom 位不占用低位

- **WHEN** 检查 `EGroupByPolicy` 的全部枚举值
- **THEN** 只有 `None` / `PerSource` / `PerInstigator` / `Custom` 四档，且 `EGB_Custom` 的值不是 1 / 2 中任何一个（逃逸位与组键附加项的可枚举取值分开）

#### Scenario: 溢出轴只有两个取值

- **WHEN** 检查 `EOverflowPolicy`
- **THEN** 只有 `EOP_RejectNew` 与 `EOP_ReplaceExisting` 两档（"替换最旧 / 替换最新"在"组内至多一条实例"的模型下同义，已裁撤）

#### Scenario: 按层取值档零行为

- **WHEN** 以 `ValueStack = PerStackValue` 施加并重复施加
- **THEN** 行为与 `KeepMax` 一致（值不随层数变化），且该档的未落地面在台账中登记

### Requirement: 组键解析

组键 MUST = `(单位, DefTag)` 再按 `GroupBy` 附一项：`PerSource` 附**来源句柄**、`PerInstigator` 附**发起者实体**、`None` 不附、`Custom` 交给决策 Fragment 判定。组键解析 MUST 确定性（同输入同分组，可复现验收的前提）。

**基座含 `DefTag` 是硬约束**：一条实例 = 一个定义的数据（快照、修正器、事件载荷全按该定义来），故**不同 `DefTag` 的实例永不共组**。这正是"按分组词分组"档被裁撤的原因——分组词取自定义自身，同一 `DefTag` 内它恒定 ⇒ 该档与 `None` 完全同义；而要让不同定义共享一个组（层数上限跨定义累计），必须允许**组内多条实例**，那是另一套层模型（"层 = 独立实例"），本轮已明确不采用。"跨定义共享层数组"作为未落地面登记台账。

**"组内至多一条实例"是本模型的推论**：每次施加要么命中组内唯一实例（刷新或叠层），要么在建出条目前先移除它（替换）——MUST NOT 出现同组两条并存。

#### Scenario: PerSource 让不同来源各自独立

- **WHEN** 以来源 A 与来源 B 各施加一次（`GroupBy = PerSource`）
- **THEN** 建出两条实例（两笔 `Applied`），互不刷新

#### Scenario: PerInstigator 按发起者分组

- **WHEN** 同一发起者施加两次、另一发起者施加一次（`GroupBy = PerInstigator`）
- **THEN** 前两次落在同一组（第二条按未满仓规则处理），第三次另建一条实例

#### Scenario: 不同定义永不共组

- **WHEN** 两枚不同的 `DefTag` 施在同一单位上（任意 `GroupBy`）
- **THEN** 各自建出实例（两笔 `Applied`），互不刷新、互不替换——基座含定义身份

#### Scenario: 按分组词档已裁撤

- **WHEN** 检查 `EGroupByPolicy` 与 `FStateStackPolicy`
- **THEN** 既无"按分组词"档、也无分组词字段（该档与 `None` 同义；其唯一有意义的读法需"组内多条实例"，已登记台账）

### Requirement: 共存决策与四档回执

施加 MUST 走确定性决策树，结果即 `EApplyResult`：

| 组内 | 来源 | 层数 | 回执 | 事件 |
|---|---|---|---|---|
| 无实例 | — | — | `EAR_Applied`（新建，`Stacks = 1`） | `Applied` |
| 有实例 | **同来源** | 未满仓 | `EAR_Refreshed`（**层数不变**，按刷新语义更新） | `Refreshed` |
| 有实例 | **异来源** | 未满仓 | `EAR_Stacked`（层数 +1） | `StackChanged` → `Refreshed` |
| 有实例 | 任意 | 已满仓 + `RejectNew` | `EAR_Rejected`（实例不动，**无广播**） | — |
| 有实例 | 任意 | 已满仓 + `ReplaceExisting` | `EAR_Applied`（移除旧 + 建新，层数回到 1） | `Removed`（旧）→ `Applied`（新） |

**"层"的判据 = 施加方换没换**：同一来源重复施加是"续杯"（层数不变），**声明了另一个来源**的施加才叠一层——这是唯一让"未满仓则层数 +1"与"同组同来源的重新施加 = 刷新"两条口径同时成立的读法，也使四档回执全部可达（否则 `Refreshed` 或 `Stacked` 必有一档成为死档）。

**调用方未声明来源（无效句柄）时 MUST 视为同来源**（续杯）：否则"不关心来源"的调用方每次都被门面发一个新号、每次都判为异来源 ⇒ 默认策略（层数无限）下会无限叠层。**生命周期长于一次施加的调用方 MUST 传稳定的 `Source`**——它既是级联锚点，也是"续杯 / 叠层"的判据。

满仓拒绝是**业务结果不是错误**：MUST 用 `Log` 记录（常规验收命令 MUST 保持零非预期红字）。

#### Scenario: 同来源重复施加不叠层

- **WHEN** 以同一来源句柄连续施加两次（`MaxStacks <= 0`）
- **THEN** 第二次返回 `EAR_Refreshed`、实例层数仍为 1、`Applied` 只到达一笔

#### Scenario: 异来源加入叠一层

- **WHEN** 以来源 A 施加后再以来源 B 施加（`MaxStacks <= 0`）
- **THEN** 第二次返回 `EAR_Stacked`、实例层数为 2，且 `StackChanged` 与 `Refreshed` 各到达一笔

#### Scenario: 满仓拒绝不改动实例

- **WHEN** `MaxStacks = 1` + `RejectNew` 时第二次施加
- **THEN** 返回 `EAR_Rejected`、实例的层数与剩余时长均不变、无任何状态事件广播、无红字

#### Scenario: 满仓替换换句柄

- **WHEN** `MaxStacks = 1` + `ReplaceExisting` 时第二次施加
- **THEN** 旧句柄被移除（`Removed`）、新句柄在册（`Applied`）、旧句柄查询被拒

### Requirement: 叠层与刷新的数值与时长语义

叠层与刷新 MUST 走同一条更新流水，顺序即下表：

1. **层数**：按决策结果写入（叠层 +1 / 同来源刷新不变 / 替换回到 1）；
2. **数值**：按 `ValueStack`——`KeepMax` 层数不参与取值（账本条目数不随层变）/ `AddValues` 值随层数成倍（框架在物化值上乘层数）；
3. **快照重建**：按本次施加的覆盖值重建（D3-12："修改 = 重新施加，新 payload 覆盖旧的"）；
4. **修正器重挂**：按来源摘旧后按新值挂新（**同一批内**完成，条数不累加——复用 D3-19 的刷新挂点）；
5. **时长**：按 `StackDurationPolicy`——`ESD_None` 保留剩余 / `ESD_RefreshRemainingToTotal` 回到满额；`Period > 0` 时按既有 `PeriodRefresh` 处理周期条目；
6. **广播**：见「层数与刷新的事件契约」。

层数变化 MUST 使数值按第 2 步重新计算（`AddValues` 下"层数 +1 ⇒ 值成倍"是它的可观测判据）。

#### Scenario: 累加档随层数成倍

- **WHEN** `ValueStack = AddValues` 时以另一来源叠到 2 层
- **THEN** 修正器物化出的值按层数成倍（属性账本读数 = 单层值的两倍），且账本条目数不累加

#### Scenario: 取最大档不随层数变

- **WHEN** `ValueStack = KeepMax` 时叠到 2 层
- **THEN** 属性账本读数与单层时相同（层数只作计数）

#### Scenario: 刷新把剩余时长拉回满额

- **WHEN** `StackDurationPolicy = RefreshRemainingToTotal` 的实例走掉一段剩余时长后再施加（同来源）
- **THEN** 该实例的剩余时长回到满额

#### Scenario: 不动时长档保留剩余

- **WHEN** `StackDurationPolicy = None` 的实例走掉一段剩余时长后再施加（同来源）
- **THEN** 该实例的剩余时长小于满额（刷新不改计时）

#### Scenario: 快照按新覆盖值重建

- **WHEN** 携新的参数覆盖值再次施加（同来源）
- **THEN** 实例快照里该键的值等于新覆盖值（旧值被整体替换，不累加）

### Requirement: Custom 决策 Fragment

`EGB_Custom` 的分组与决策 MUST 由 `FTcsStateStackDecisionFragment` 接管：`USTRUCT(meta = (Hidden))`，三个决策方法**全部中性默认实现**（MUST NOT `= 0`、MUST NOT `PURE_VIRTUAL`——同一份代码在 Development 与 Shipping 下行为不同）：

- `virtual bool IsSameGroup(...) const`：默认 = 内置 `None` 语义（同单位 + 同 `DefTag`）；
- `virtual bool ShouldAccept(...) const`：默认 = 接受；
- `virtual int32 ResolveStacks(...) const`：默认 = 沿用内置规则（同来源不变 / 异来源 +1）。

策略实例归 **Def 资产**持有（`CustomDecision` 字段）；`FTcsStateInstance` MUST NOT 持有任何策略载体（值语义记录是池化与操作复制的地基）。

`GroupBy = EGB_Custom` 而载荷缺失或类型不符 MUST 退化为内置 `None` 语义并留 `Warning`（**配置错误**语义——与满仓拒绝的 `Log` 分开，后者是业务结果）。

框架 MUST NOT 提供任何具体分组或决策策略实现（分组词与业务判据是宿主词汇；框架只给中性默认与逃逸位）。

#### Scenario: 宿主策略被真实调用

- **WHEN** 宿主提供一个 `IsSameGroup` 恒假的样本策略并以 `EGB_Custom` 施加两次
- **THEN** 两次都建出新实例（两笔 `Applied`），且样本被真实调用（日志可证）

#### Scenario: 载荷缺失时退化

- **WHEN** `GroupBy = EGB_Custom` 但 `CustomDecision` 为空
- **THEN** 按内置 `None` 语义处理并留 `Warning`（不崩溃、不 `ensure`）

#### Scenario: 实例不持有策略

- **WHEN** 检查 `FTcsStateInstance` 的字段集合
- **THEN** 无策略载体（策略住 Def 资产，实例只留数据）

### Requirement: 层数与刷新的事件契约

- `TcsEvent.State.StackChanged` MUST **只在层数真的变化时**广播（即叠层路径）；载荷 `Stacks` = 新层数；
- 叠层路径 MUST 广播 `StackChanged` **先于** `Refreshed`（层数在刷新前已就位；两笔载荷的 `Stacks` 均等于新层数）；
- 同来源刷新（层数不变）MUST NOT 广播 `StackChanged`；
- 替换路径 MUST 广播 `Removed`（旧实例）与 `Applied`（新实例）；层数未变化 ⇒ MUST NOT 广播 `StackChanged`；
- 满仓拒绝 MUST NOT 广播任何状态事件。

#### Scenario: 叠层两笔事件的先后

- **WHEN** 同一订阅者同时订阅 `StackChanged` 与 `Refreshed` 并在叠层路径上处理两笔
- **THEN** 它的处理序号显示 `StackChanged` 早于 `Refreshed`，且两笔载荷的 `Stacks` 都等于新层数

#### Scenario: 刷新不广播层数变化

- **WHEN** 同来源重复施加（层数不变）
- **THEN** `StackChanged` 计数不增，只有 `Refreshed` 计数增加

#### Scenario: 替换不广播层数变化

- **WHEN** `MaxStacks = 1` + `ReplaceExisting` 的替换路径
- **THEN** `Removed` 与 `Applied` 各一笔，`StackChanged` 计数不增

#### Scenario: 拒绝不广播任何状态事件

- **WHEN** 满仓 + `RejectNew` 时施加
- **THEN** 六枚状态事件计数全部不变
